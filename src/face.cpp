// SC\Face: the faces in a picture as sc-ai's 512-number features, read by the ai-api service (its /faces
// route, on the ai server's GPU) over sc-http's sc::http::client. The models don't run in PHP: the
// extension stays small and the web servers need no GPU.
//
//     $features = SC\Face::features('/tmp/photo.jpg');   // the largest face's 512 numbers, null without a face
//     $faces = SC\Face::faces('/tmp/photo.jpg', 5);      // every face: ['features' => [...], 'box' => [...]]
//     $percent = SC\Face::similarity($features, $other); // 100 the same face; one person from about 35
//
// Where ai-api is: the ini settings sc.ai_url (http://ai.example.com:8092) and sc.ai_token_file (a file
// holding its bearer token, readable by PHP's user). Both are system settings: a script can't point
// them elsewhere. A failure (not set, not reachable, not a picture) is an SC\Exception.

#include "php_glue.h"

// php_json.h has no extern "C" of its own: without it C++ would look for a mangled php_json_decode_ex.
extern "C" {
#include <ext/json/php_json.h>
}

#include <sc/http/client.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    constexpr std::size_t feature_size = 512;

    std::string ini_text(const char *name) {
        const char *value = zend_ini_string_ex(name, std::strlen(name), 0, nullptr);
        return value ? value : "";
    }

    std::string trimmed(std::string text) {
        const auto first = text.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        text.erase(text.find_last_not_of(" \t\r\n") + 1);
        return text.substr(first);
    }

    std::string read_file(const std::string &path, const std::string &what) {
        std::ifstream in{path, std::ios::binary};
        if (!in) throw std::runtime_error{"SC\\Face: can't read " + what + " " + path};
        std::ostringstream bytes;
        bytes << in.rdbuf();
        return bytes.str();
    }

    // The picture POSTed to ai-api's /faces; its JSON answer.
    std::string ask_for_faces(const std::string &picture, const zend_long max_faces) {
        const auto url = trimmed(ini_text("sc.ai_url"));
        if (url.empty()) throw std::runtime_error{"SC\\Face: sc.ai_url isn't set (where ai-api is, http://host:8092)"};
        const auto token_file = trimmed(ini_text("sc.ai_token_file"));
        if (token_file.empty()) throw std::runtime_error{"SC\\Face: sc.ai_token_file isn't set (the file holding ai-api's token)"};
        const auto token = trimmed(read_file(token_file, "sc.ai_token_file"));
        if (token.empty()) throw std::runtime_error{"SC\\Face: sc.ai_token_file " + token_file + " is empty"};

        const auto bytes = read_file(picture, "the picture");
        sc::http::client call{(url.ends_with('/') ? url.substr(0, url.size() - 1) : url) + "/faces"};
        call.quiet();
        call.query({{"max_faces", std::to_string(max_faces)}, {"face_images", "false"}});
        call.header("Authorization", "Bearer " + token);
        call.header("Content-Type", "application/octet-stream");
        const double limit = sc::php::default_time_limit();
        call.timeout(limit > 0 ? static_cast<long>(std::ceil(limit)) : 0).connect_timeout(10);
        auto answer = call.post(bytes);
        const auto status = call.status();
        if (status == 200) return answer;
        if (status == 0) throw std::runtime_error{"SC\\Face: ai-api (" + url + ") can't be reached: " + call.error()};
        if (status == 422) throw std::runtime_error{"SC\\Face: " + picture + " isn't a picture ai-api can read"};
        if (status == 413) throw std::runtime_error{"SC\\Face: " + picture + " is too large for ai-api"};
        if (status == 401 || status == 403) throw std::runtime_error{"SC\\Face: ai-api refused the token in " + token_file};
        throw std::runtime_error{"SC\\Face: ai-api answered " + std::to_string(status) + ": " + answer.substr(0, 200)};
    }

    // 512 numbers from a PHP array (a list, as features() gives it).
    std::vector<double> features_of(HashTable *table, const char *which) {
        if (zend_hash_num_elements(table) != feature_size)
            throw std::invalid_argument{std::string{"SC\\Face::similarity(): "} + which + " must hold 512 numbers"};
        std::vector<double> out;
        out.reserve(feature_size);
        zval *value;
        ZEND_HASH_FOREACH_VAL(table, value) {
            if (Z_TYPE_P(value) == IS_DOUBLE) out.push_back(Z_DVAL_P(value));
            else if (Z_TYPE_P(value) == IS_LONG) out.push_back(static_cast<double>(Z_LVAL_P(value)));
            else throw std::invalid_argument{std::string{"SC\\Face::similarity(): "} + which + " must hold numbers only"};
            if (!std::isfinite(out.back())) throw std::invalid_argument{std::string{"SC\\Face::similarity(): "} + which + " must be finite"};
        } ZEND_HASH_FOREACH_END();
        double norm = 0;
        for (const double each: out) norm += each * each;
        if (norm <= 0) throw std::invalid_argument{std::string{"SC\\Face::similarity(): "} + which + " is all zero"};
        norm = std::sqrt(norm);
        for (double &each: out) each /= norm;
        return out;
    }

    // The faces of ai-api's answer as PHP arrays: ['features' => [512 floats], 'box' => ['left', 'top',
    // 'width', 'height']] each, in ai-api's order. Throws when the answer isn't that.
    void faces_array(zval *return_value, const std::string &answer) {
        zval decoded;
        ZVAL_UNDEF(&decoded);
        if (php_json_decode_ex(&decoded, answer.data(), answer.size(), PHP_JSON_OBJECT_AS_ARRAY, 64) == FAILURE
            || Z_TYPE(decoded) != IS_ARRAY) {
            zval_ptr_dtor(&decoded);
            if (EG(exception)) zend_clear_exception();
            throw std::runtime_error{"SC\\Face: ai-api's answer isn't a list of faces"};
        }
        array_init(return_value);
        zval *each;
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL(decoded), each) {
            if (Z_TYPE_P(each) != IS_ARRAY) continue;
            zval *features = sc::php::find(Z_ARRVAL_P(each), "features");
            zval *box = sc::php::find(Z_ARRVAL_P(each), "box");
            if (!features || Z_TYPE_P(features) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(features)) != feature_size) continue;
            zval face;
            array_init(&face);
            Z_TRY_ADDREF_P(features);
            add_assoc_zval(&face, "features", features);
            if (box && Z_TYPE_P(box) == IS_ARRAY) {
                Z_TRY_ADDREF_P(box);
                add_assoc_zval(&face, "box", box);
            }
            add_next_index_zval(return_value, &face);
        } ZEND_HASH_FOREACH_END();
        zval_ptr_dtor(&decoded);
    }

    double box_area(zval *face) {
        zval *box = sc::php::find(Z_ARRVAL_P(face), "box");
        if (!box || Z_TYPE_P(box) != IS_ARRAY) return 0;
        zval *width = sc::php::find(Z_ARRVAL_P(box), "width"), *height = sc::php::find(Z_ARRVAL_P(box), "height");
        return width && height ? zval_get_double(width) * zval_get_double(height) : 0;
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_faces, 0, 1, IS_ARRAY, 0)
        ZEND_ARG_TYPE_INFO(0, file, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, maxFaces, IS_LONG, 0, "5")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(face_faces) {
        zend_string *file = nullptr;
        zend_long max_faces = 5;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_PATH_STR(file)
            Z_PARAM_OPTIONAL
            Z_PARAM_LONG(max_faces)
        ZEND_PARSE_PARAMETERS_END();
        if (max_faces < 1 || max_faces > 100) {
            zend_throw_exception(sc::php::exception_class, "SC\\Face::faces(): maxFaces is 1 to 100", 0);
            RETURN_THROWS();
        }
        std::string answer;
        sc::php::guarded_native([&] { answer = ask_for_faces(std::string{sc::php::view(file)}, max_faces); });
        if (EG(exception)) RETURN_THROWS();
        sc::php::guarded([&] { faces_array(return_value, answer); });
        if (EG(exception)) RETURN_THROWS();
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_features, 0, 1, IS_ARRAY, 1)
        ZEND_ARG_TYPE_INFO(0, file, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(face_features) {
        zend_string *file = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_PATH_STR(file)
        ZEND_PARSE_PARAMETERS_END();
        std::string answer;
        // Up to 10, so the largest is among them however many small faces are in the background
        sc::php::guarded_native([&] { answer = ask_for_faces(std::string{sc::php::view(file)}, 10); });
        if (EG(exception)) RETURN_THROWS();
        zval faces;
        ZVAL_UNDEF(&faces);
        sc::php::guarded([&] { faces_array(&faces, answer); });
        if (EG(exception)) {
            zval_ptr_dtor(&faces);
            RETURN_THROWS();
        }
        zval *largest = nullptr, *each;
        double largest_area = -1;
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL(faces), each) {
            const double area = box_area(each);
            if (area > largest_area) {
                largest_area = area;
                largest = each;
            }
        } ZEND_HASH_FOREACH_END();
        if (!largest) {
            zval_ptr_dtor(&faces);
            RETURN_NULL();
        }
        ZVAL_COPY(return_value, sc::php::find(Z_ARRVAL_P(largest), "features"));
        zval_ptr_dtor(&faces);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_similarity, 0, 2, IS_DOUBLE, 0)
        ZEND_ARG_TYPE_INFO(0, a, IS_ARRAY, 0)
        ZEND_ARG_TYPE_INFO(0, b, IS_ARRAY, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(face_similarity) {
        HashTable *a = nullptr, *b = nullptr;
        ZEND_PARSE_PARAMETERS_START(2, 2)
            Z_PARAM_ARRAY_HT(a)
            Z_PARAM_ARRAY_HT(b)
        ZEND_PARSE_PARAMETERS_END();
        double percent = 0;
        sc::php::guarded([&] {
            const auto first = features_of(a, "the first"), second = features_of(b, "the second");
            double dot = 0;
            for (std::size_t i = 0; i < feature_size; ++i) dot += first[i] * second[i];
            percent = dot * 100;
        });
        if (EG(exception)) RETURN_THROWS();
        RETURN_DOUBLE(percent);
    }

    const zend_function_entry methods[] = {
        ZEND_NAMED_ME(features, face_features, arginfo_features, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(faces, face_faces, arginfo_faces, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(similarity, face_similarity, arginfo_similarity, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_FE_END
    };

    void startup() {
        zend_class_entry type;
        INIT_NS_CLASS_ENTRY(type, "SC", "Face", methods);
        auto *face_class = zend_register_internal_class(&type);
        face_class->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NOT_SERIALIZABLE | ZEND_ACC_NO_DYNAMIC_PROPERTIES;
    }
}

sc::php::area sc::php::face_area() { return {nullptr, startup}; }
