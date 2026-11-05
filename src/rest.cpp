// SC\Rest: sc-http's sc::rest (libsc-http's shared library, from its package), for REST and SOAP
// calls: TLS checked by default, timeouts always, and nothing written to the logs (error() says
// what went wrong).
//
//     $api = new SC\Rest('https://example.com/api/items');
//     $api->bearer($token)->timeout(10);
//     $body = $api->post(['name' => 'x']);          // an array goes as JSON (or form: see post())
//     if ($api->status() !== 200) error_log($api->status() . ' ' . $api->error());
//
//     $soap = new SC\Rest($location);
//     $soap->header('Content-Type', 'text/xml; charset=utf-8')->header('SOAPAction', '"' . $action . '"')
//          ->clientCertificate('/etc/app/client.pem', null, $passphrase);
//     $xml = $soap->post($envelope);
//
// The object keeps its settings; each request is a fresh connection with them, and status(),
// headers(), error(), effectiveUrl() and totalTime() describe the last one. A bad argument (an
// array nested in a multipart field, a file part without a path) is an SC\Exception; a request
// that got no response isn't: status() is 0 and error() says why.

#include "php_glue.h"

// php_json.h has no extern "C" of its own: without it C++ would look for a mangled php_json_encode.
extern "C" {
#include <ext/json/php_json.h>
}
#include <zend_smart_str.h>

#include <rest.h>

#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    zend_class_entry *rest_class = nullptr;
    zend_object_handlers handlers;

    // What an SC\Rest holds: its settings, and what its last request got.
    struct settings {
        std::string url;
        std::vector<std::pair<std::string, std::string>> headers; // in order; header() replaces one of the same name
        long timeout = 30, connect_timeout = 10, idle = 0; // idle: idleTimeout(), 0 none
        std::string certificate, key, passphrase, ca, pins;
        bool verify_host = true, follow = false;
        long max_redirects = 5;

        long status = 0;
        std::map<std::string, std::string> response_headers;
        std::string error, effective_url;
        long error_code = 0;
        double total_time = 0;
    };

    struct rest_object {
        settings *self;
        zend_object std;
    };

    rest_object *object_of(zend_object *object) {
        return reinterpret_cast<rest_object *>(reinterpret_cast<char *>(object) - XtOffsetOf(rest_object, std));
    }

    zend_object *create(zend_class_entry *type) {
        auto *made = static_cast<rest_object *>(zend_object_alloc(sizeof(rest_object), type));
        made->self = nullptr;
        zend_object_std_init(&made->std, type);
        object_properties_init(&made->std, type);
        made->std.handlers = &handlers;
        return &made->std;
    }

    void free_object(zend_object *object) {
        delete object_of(object)->self;
        zend_object_std_dtor(object);
    }

    settings &of(zval *self) {
        auto *held = object_of(Z_OBJ_P(self))->self;
        if (!held) throw std::logic_error{"SC\\Rest: not constructed"};
        return *held;
    }

    std::string lower(std::string text) {
        for (auto &c: text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    }

    // The value of a header that was set, lower-cased; "" when none was.
    std::string header_value(const settings &s, const std::string &name) {
        for (const auto &[each, value]: s.headers)
            if (lower(each) == name) return lower(value);
        return "";
    }

    // A scalar as http_build_query() writes it: true "1", false "0", null left out (nullopt).
    std::optional<std::string> scalar_text(zval *value) {
        switch (Z_TYPE_P(value)) {
            case IS_NULL: return std::nullopt;
            case IS_TRUE: return "1";
            case IS_FALSE: return "0";
            default: return sc::php::text_of(value);
        }
    }

    // An array's pairs as http_build_query() makes them: nested keys as prefix[key].
    void pairs_of(HashTable *table, const std::string &prefix, sc::rest::fields &out) {
        zend_string *key = nullptr;
        zend_ulong index = 0;
        zval *value = nullptr;
        ZEND_HASH_FOREACH_KEY_VAL(table, index, key, value) {
            ZVAL_DEREF(value);
            const std::string name = key ? std::string{sc::php::view(key)} : std::to_string(index);
            const std::string full = prefix.empty() ? name : prefix + "[" + name + "]";
            if (Z_TYPE_P(value) == IS_ARRAY) {
                pairs_of(Z_ARRVAL_P(value), full, out);
            } else if (Z_TYPE_P(value) == IS_OBJECT) {
                throw std::invalid_argument{"SC\\Rest: " + full + " is an object; give its fields as an array"};
            } else if (const auto text = scalar_text(value)) {
                out.emplace_back(full, *text);
            }
        }
        ZEND_HASH_FOREACH_END();
    }

    sc::rest::fields pairs_of(zval *array) {
        sc::rest::fields out;
        if (array) pairs_of(Z_ARRVAL_P(array), "", out);
        return out;
    }

    // An array as json_encode() would write it.
    std::string json_of(zval *value) {
        smart_str text = {};
        if (php_json_encode(&text, value, 0) == FAILURE) {
            smart_str_free(&text);
            throw std::invalid_argument{"SC\\Rest: the body can't be written as JSON"};
        }
        smart_str_0(&text);
        std::string out = text.s ? std::string{ZSTR_VAL(text.s), ZSTR_LEN(text.s)} : "";
        smart_str_free(&text);
        return out;
    }

    // One request with the object's settings; its answer, and what it got kept in s.
    std::string perform(settings &s, const std::string &method, const sc::rest::fields &query, const std::string *body,
                        const std::vector<sc::rest::part> *parts) {
        sc::rest request{s.url};
        request.quiet();
        for (const auto &[name, value]: s.headers) request.header(name, value);
        request.timeout(s.timeout).connect_timeout(s.connect_timeout).idle_timeout(s.idle);
        if (!s.certificate.empty()) request.client_certificate(s.certificate, s.key, s.passphrase);
        if (!s.ca.empty()) request.trust(s.ca);
        request.verify_host(s.verify_host);
        if (!s.pins.empty()) request.pinned_public_key(s.pins);
        request.follow_redirects(s.follow, s.max_redirects);
        request.query(query);
        std::string answer;
        if (parts) answer = request.post_multipart(*parts);
        else if (method == "GET") answer = request.get();
        else if (method == "HEAD") answer = request.head();
        else if (method == "DELETE") answer = request.del();
        else if (method == "POST") answer = request.post(*body);
        else if (method == "PUT") answer = request.put(*body);
        else answer = request.patch(*body);
        s.status = request.status();
        s.response_headers = request.response_headers();
        s.error = request.error();
        s.error_code = request.error_code();
        s.effective_url = request.effective_url();
        s.total_time = request.total_time();
        // No response: an empty string, not sc::rest's {"error": ...} (error() has it).
        if (s.status == 0) answer.clear();
        return answer;
    }

    // A body argument: a string as it is; an array JSON, or form-encoded when the Content-Type set is
    // application/x-www-form-urlencoded (then also the default Content-Type for it).
    std::string body_of(settings &s, zval *body) {
        if (!body || Z_TYPE_P(body) == IS_NULL) return "";
        if (Z_TYPE_P(body) == IS_STRING) return std::string{Z_STRVAL_P(body), Z_STRLEN_P(body)};
        const auto type = header_value(s, "content-type");
        if (type.starts_with("multipart/form-data")) throw std::invalid_argument{"SC\\Rest: a multipart body is postMultipart()'s"};
        if (type.starts_with("application/x-www-form-urlencoded")) return sc::rest::form_encode(pairs_of(body));
        return json_of(body);
    }

    void return_this(zval *return_value, zval *self) { RETURN_OBJ_COPY(Z_OBJ_P(self)); }

    // --- settings ---

    ZEND_BEGIN_ARG_INFO_EX(arginfo_construct, 0, 0, 1)
        ZEND_ARG_TYPE_INFO(0, url, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(rest_construct) {
        zend_string *url = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(url)
        ZEND_PARSE_PARAMETERS_END();
        auto *held = object_of(Z_OBJ_P(ZEND_THIS));
        sc::php::guarded([&] {
            auto made = std::make_unique<settings>();
            made->url = sc::php::view(url);
            made->headers.emplace_back("Content-Type", "application/json");
            delete held->self;
            held->self = made.release();
        });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_header, 0, 2, static, 0)
        ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    // Replaces one of the same name (ignoring case); an empty value sends none.
    ZEND_NAMED_FUNCTION(rest_header) {
        zend_string *name = nullptr, *value = nullptr;
        ZEND_PARSE_PARAMETERS_START(2, 2)
            Z_PARAM_STR(name)
            Z_PARAM_STR(value)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] {
            auto &s = of(ZEND_THIS);
            const std::string wanted{sc::php::view(name)};
            if (wanted.empty() || wanted.find_first_of(":\r\n") != std::string::npos) throw std::invalid_argument{"SC\\Rest: \"" + wanted + "\" isn't a header name"};
            const std::string text{sc::php::view(value)};
            if (text.find_first_of("\r\n") != std::string::npos) throw std::invalid_argument{"SC\\Rest: header " + wanted + "'s value has a line break"};
            for (auto &[each, set]: s.headers)
                if (lower(each) == lower(wanted)) {
                    each = wanted;
                    set = text;
                    return;
                }
            s.headers.emplace_back(wanted, text);
        });
        if (EG(exception)) RETURN_THROWS();
        return_this(return_value, ZEND_THIS);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_bearer, 0, 1, static, 0)
        ZEND_ARG_TYPE_INFO(0, token, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_trust, 0, 1, static, 0)
        ZEND_ARG_TYPE_INFO(0, caFile, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_pins, 0, 1, static, 0)
        ZEND_ARG_TYPE_INFO(0, pins, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    template<typename Set>
    void set_string(INTERNAL_FUNCTION_PARAMETERS, Set set) {
        zend_string *value = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(value)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] { set(of(ZEND_THIS), std::string{sc::php::view(value)}); });
        if (EG(exception)) RETURN_THROWS();
        return_this(return_value, ZEND_THIS);
    }

    ZEND_NAMED_FUNCTION(rest_bearer) {
        set_string(INTERNAL_FUNCTION_PARAM_PASSTHRU, [](settings &s, const std::string &token) {
            for (auto &[name, value]: s.headers)
                if (lower(name) == "authorization") {
                    value = "Bearer " + token;
                    return;
                }
            s.headers.emplace_back("Authorization", "Bearer " + token);
        });
    }

    ZEND_NAMED_FUNCTION(rest_trust) {
        set_string(INTERNAL_FUNCTION_PARAM_PASSTHRU, [](settings &s, const std::string &file) { s.ca = file; });
    }

    ZEND_NAMED_FUNCTION(rest_pinned_public_key) {
        set_string(INTERNAL_FUNCTION_PARAM_PASSTHRU, [](settings &s, const std::string &pins) { s.pins = pins; });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_seconds, 0, 1, static, 0)
        ZEND_ARG_TYPE_INFO(0, seconds, IS_LONG, 0)
    ZEND_END_ARG_INFO()

    // A limit in seconds, least (0: none, or 1) to a day.
    template<typename Set>
    void set_seconds(INTERNAL_FUNCTION_PARAMETERS, const long least, Set set) {
        zend_long seconds = 0;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_LONG(seconds)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] {
            if (seconds < least || seconds > 86400)
                throw std::invalid_argument{least == 0 ? "SC\\Rest: a limit is 0 (none) to 86400 seconds" : "SC\\Rest: a limit is 1 to 86400 seconds"};
            set(of(ZEND_THIS), static_cast<long>(seconds));
        });
        if (EG(exception)) RETURN_THROWS();
        return_this(return_value, ZEND_THIS);
    }

    ZEND_NAMED_FUNCTION(rest_timeout) {
        set_seconds(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, [](settings &s, const long seconds) { s.timeout = seconds; });
    }

    ZEND_NAMED_FUNCTION(rest_connect_timeout) {
        set_seconds(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1, [](settings &s, const long seconds) { s.connect_timeout = seconds; });
    }

    // Gives up when no data arrives for that long (0: never), however long the whole may take.
    ZEND_NAMED_FUNCTION(rest_idle_timeout) {
        set_seconds(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, [](settings &s, const long seconds) { s.idle = seconds; });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_certificate, 0, 1, static, 0)
        ZEND_ARG_TYPE_INFO(0, certificateFile, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, keyFile, IS_STRING, 1, "null")
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, passphrase, IS_STRING, 1, "null")
    ZEND_END_ARG_INFO()

    // keyFile null: the key is in certificateFile's PEM too (as SoapClient's local_cert).
    ZEND_NAMED_FUNCTION(rest_client_certificate) {
        zend_string *certificate = nullptr, *key = nullptr, *passphrase = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 3)
            Z_PARAM_STR(certificate)
            Z_PARAM_OPTIONAL
            Z_PARAM_STR_OR_NULL(key)
            Z_PARAM_STR_OR_NULL(passphrase)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] {
            auto &s = of(ZEND_THIS);
            s.certificate = sc::php::view(certificate);
            s.key = key ? std::string{sc::php::view(key)} : "";
            s.passphrase = passphrase ? std::string{sc::php::view(passphrase)} : "";
        });
        if (EG(exception)) RETURN_THROWS();
        return_this(return_value, ZEND_THIS);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_verify_host, 0, 1, static, 0)
        ZEND_ARG_TYPE_INFO(0, on, _IS_BOOL, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(rest_verify_host) {
        bool on = true;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_BOOL(on)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] { of(ZEND_THIS).verify_host = on; });
        if (EG(exception)) RETURN_THROWS();
        return_this(return_value, ZEND_THIS);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_follow, 0, 0, static, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, on, _IS_BOOL, 0, "true")
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, max, IS_LONG, 0, "5")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(rest_follow_redirects) {
        bool on = true;
        zend_long max = 5;
        ZEND_PARSE_PARAMETERS_START(0, 2)
            Z_PARAM_OPTIONAL
            Z_PARAM_BOOL(on)
            Z_PARAM_LONG(max)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] {
            if (max < 0 || max > 50) throw std::invalid_argument{"SC\\Rest: at most 0 to 50 redirects"};
            auto &s = of(ZEND_THIS);
            s.follow = on;
            s.max_redirects = static_cast<long>(max);
        });
        if (EG(exception)) RETURN_THROWS();
        return_this(return_value, ZEND_THIS);
    }

    // --- requests ---

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_get, 0, 0, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, query, IS_ARRAY, 0, "[]")
    ZEND_END_ARG_INFO()

    template<typename Run>
    void request(zval *return_value, Run run) {
        std::string answer;
        sc::php::guarded([&] { answer = run(); });
        if (EG(exception)) RETURN_THROWS();
        RETURN_STRINGL(answer.data(), answer.size());
    }

    // GET, HEAD and DELETE: query parameters, no body.
    template<const char *Method>
    void without_body(INTERNAL_FUNCTION_PARAMETERS) {
        zval *query = nullptr;
        ZEND_PARSE_PARAMETERS_START(0, 1)
            Z_PARAM_OPTIONAL
            Z_PARAM_ARRAY(query)
        ZEND_PARSE_PARAMETERS_END();
        request(return_value, [&] { return perform(of(ZEND_THIS), Method, pairs_of(query), nullptr, nullptr); });
    }

    constexpr char get_method[] = "GET", head_method[] = "HEAD", delete_method[] = "DELETE";
    constexpr char post_method[] = "POST", put_method[] = "PUT", patch_method[] = "PATCH";

    ZEND_NAMED_FUNCTION(rest_get) { without_body<get_method>(INTERNAL_FUNCTION_PARAM_PASSTHRU); }
    ZEND_NAMED_FUNCTION(rest_head) { without_body<head_method>(INTERNAL_FUNCTION_PARAM_PASSTHRU); }
    ZEND_NAMED_FUNCTION(rest_delete) { without_body<delete_method>(INTERNAL_FUNCTION_PARAM_PASSTHRU); }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_post, 0, 0, IS_STRING, 0)
        ZEND_ARG_TYPE_MASK(0, body, MAY_BE_STRING | MAY_BE_ARRAY | MAY_BE_NULL, "''")
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, query, IS_ARRAY, 0, "[]")
    ZEND_END_ARG_INFO()

    // POST, PUT and PATCH: a body (a string as it is, an array JSON or a form), query parameters.
    template<const char *Method>
    void with_body(INTERNAL_FUNCTION_PARAMETERS) {
        zval *body = nullptr, *query = nullptr;
        ZEND_PARSE_PARAMETERS_START(0, 2)
            Z_PARAM_OPTIONAL
            Z_PARAM_ZVAL(body)
            Z_PARAM_ARRAY(query)
        ZEND_PARSE_PARAMETERS_END();
        if (body && Z_TYPE_P(body) != IS_STRING && Z_TYPE_P(body) != IS_ARRAY && Z_TYPE_P(body) != IS_NULL) {
            zend_argument_type_error(1, "must be of type string|array|null, %s given", zend_zval_type_name(body));
            RETURN_THROWS();
        }
        request(return_value, [&] {
            auto &s = of(ZEND_THIS);
            const auto text = body_of(s, body);
            return perform(s, Method, pairs_of(query), &text, nullptr);
        });
    }

    ZEND_NAMED_FUNCTION(rest_post) { with_body<post_method>(INTERNAL_FUNCTION_PARAM_PASSTHRU); }
    ZEND_NAMED_FUNCTION(rest_put) { with_body<put_method>(INTERNAL_FUNCTION_PARAM_PASSTHRU); }
    ZEND_NAMED_FUNCTION(rest_patch) { with_body<patch_method>(INTERNAL_FUNCTION_PARAM_PASSTHRU); }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_multipart, 0, 1, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, fields, IS_ARRAY, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, files, IS_ARRAY, 0, "[]")
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, query, IS_ARRAY, 0, "[]")
    ZEND_END_ARG_INFO()

    // fields: name => value; files: name => path, or name => ['path' => ..., 'filename' => ..., 'type' => ...].
    ZEND_NAMED_FUNCTION(rest_post_multipart) {
        zval *fields = nullptr, *files = nullptr, *query = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 3)
            Z_PARAM_ARRAY(fields)
            Z_PARAM_OPTIONAL
            Z_PARAM_ARRAY(files)
            Z_PARAM_ARRAY(query)
        ZEND_PARSE_PARAMETERS_END();
        request(return_value, [&] {
            std::vector<sc::rest::part> parts;
            zend_string *key = nullptr;
            zend_ulong index = 0;
            zval *value = nullptr;
            ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(fields), index, key, value) {
                ZVAL_DEREF(value);
                const std::string name = key ? std::string{sc::php::view(key)} : std::to_string(index);
                if (Z_TYPE_P(value) == IS_ARRAY || Z_TYPE_P(value) == IS_OBJECT) throw std::invalid_argument{"SC\\Rest: field " + name + " isn't a value"};
                if (const auto text = scalar_text(value)) parts.push_back({.name = name, .value = *text});
            }
            ZEND_HASH_FOREACH_END();
            if (files) {
                ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(files), index, key, value) {
                    ZVAL_DEREF(value);
                    const std::string name = key ? std::string{sc::php::view(key)} : std::to_string(index);
                    sc::rest::part part{.name = name};
                    if (Z_TYPE_P(value) == IS_STRING) {
                        part.file = sc::php::text_of(value);
                    } else if (Z_TYPE_P(value) == IS_ARRAY) {
                        const auto text = [&](const char *field) {
                            zval *found = sc::php::find(Z_ARRVAL_P(value), field);
                            return found && Z_TYPE_P(found) != IS_NULL ? sc::php::text_of(found) : std::string{};
                        };
                        part.file = text("path");
                        part.filename = text("filename");
                        part.content_type = text("type");
                    }
                    if (part.file.empty()) throw std::invalid_argument{"SC\\Rest: file " + name + " has no path"};
                    parts.push_back(std::move(part));
                }
                ZEND_HASH_FOREACH_END();
            }
            return perform(of(ZEND_THIS), "POST", pairs_of(query), nullptr, &parts);
        });
    }

    // --- the last response ---

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_long, 0, 0, IS_LONG, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(rest_status) {
        ZEND_PARSE_PARAMETERS_NONE();
        zend_long status = 0;
        sc::php::guarded([&] { status = of(ZEND_THIS).status; });
        if (EG(exception)) RETURN_THROWS();
        RETURN_LONG(status);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_array, 0, 0, IS_ARRAY, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(rest_headers) {
        ZEND_PARSE_PARAMETERS_NONE();
        std::map<std::string, std::string> headers;
        sc::php::guarded([&] { headers = of(ZEND_THIS).response_headers; });
        if (EG(exception)) RETURN_THROWS();
        array_init_size(return_value, static_cast<uint32_t>(headers.size()));
        for (const auto &[name, value]: headers) add_assoc_stringl_ex(return_value, name.data(), name.size(), value.data(), value.size());
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_text, 0, 0, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(rest_error) {
        ZEND_PARSE_PARAMETERS_NONE();
        sc::php::return_guarded_string(return_value, [&] { return of(ZEND_THIS).error; });
    }

    // curl's error number for the last request (CURLcode), 0 when a response came.
    ZEND_NAMED_FUNCTION(rest_error_code) {
        ZEND_PARSE_PARAMETERS_NONE();
        long code = 0;
        sc::php::guarded([&] { code = of(ZEND_THIS).error_code; });
        if (EG(exception)) RETURN_THROWS();
        RETURN_LONG(code);
    }

    ZEND_NAMED_FUNCTION(rest_effective_url) {
        ZEND_PARSE_PARAMETERS_NONE();
        sc::php::return_guarded_string(return_value, [&] { return of(ZEND_THIS).effective_url; });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_double, 0, 0, IS_DOUBLE, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(rest_total_time) {
        ZEND_PARSE_PARAMETERS_NONE();
        double seconds = 0;
        sc::php::guarded([&] { seconds = of(ZEND_THIS).total_time; });
        if (EG(exception)) RETURN_THROWS();
        RETURN_DOUBLE(seconds);
    }

    const zend_function_entry methods[] = {
        ZEND_NAMED_ME(__construct, rest_construct, arginfo_construct, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(header, rest_header, arginfo_header, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(bearer, rest_bearer, arginfo_bearer, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(timeout, rest_timeout, arginfo_seconds, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(connectTimeout, rest_connect_timeout, arginfo_seconds, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(idleTimeout, rest_idle_timeout, arginfo_seconds, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(clientCertificate, rest_client_certificate, arginfo_certificate, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(trust, rest_trust, arginfo_trust, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(verifyHost, rest_verify_host, arginfo_verify_host, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(pinnedPublicKey, rest_pinned_public_key, arginfo_pins, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(followRedirects, rest_follow_redirects, arginfo_follow, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(get, rest_get, arginfo_get, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(head, rest_head, arginfo_get, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(delete, rest_delete, arginfo_get, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(post, rest_post, arginfo_post, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(put, rest_put, arginfo_post, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(patch, rest_patch, arginfo_post, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(postMultipart, rest_post_multipart, arginfo_multipart, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(status, rest_status, arginfo_long, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(headers, rest_headers, arginfo_array, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(error, rest_error, arginfo_text, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(errorCode, rest_error_code, arginfo_long, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(effectiveUrl, rest_effective_url, arginfo_text, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(totalTime, rest_total_time, arginfo_double, ZEND_ACC_PUBLIC)
        ZEND_FE_END
    };

    void startup() {
        zend_class_entry type;
        INIT_NS_CLASS_ENTRY(type, "SC", "Rest", methods);
        rest_class = zend_register_internal_class(&type);
        rest_class->ce_flags |= ZEND_ACC_FINAL;
        rest_class->create_object = create;
        std::memcpy(&handlers, zend_get_std_object_handlers(), sizeof(handlers));
        handlers.offset = XtOffsetOf(rest_object, std);
        handlers.free_obj = free_object;
        handlers.clone_obj = nullptr;
    }
}

sc::php::area sc::php::rest_area() { return {nullptr, startup}; }
