// SC\Passkey: sc-passkey's sc::passkey::verifier, passkeys (WebAuthn) for a relying party.
//
//     $pk = new SC\Passkey('example.com', 'Example', ['https://*.example.com']);
//     $options = $pk->registrationOptions($userId, 'roelof', 'Roelof', $existingIds);
//     // json_encode($options) to the browser (sc-passkey.js), $options['challenge'] kept
//     $made = $pk->verifyRegistration($answerJson, $challenge);   // ['id' => ..., 'publicKey' => ...]
//     $used = $pk->verifyAuthentication($answerJson, $challenge, $made['publicKey'], $made['signCount']);
//
// Options are arrays ready for json_encode(); results are arrays. A check that fails throws
// SC\PasskeyException, whose getReason() names it ("origin", "challenge", "signature", ...); a
// mistake in the arguments throws SC\Exception.

#include "php_glue.h"

#include <sc_passkey.h>

#include <cstring>
#include <memory>
#include <stdexcept>

namespace {
    zend_class_entry *passkey_class = nullptr;
    zend_class_entry *passkey_exception_class = nullptr;
    zend_object_handlers handlers;

    struct passkey_object {
        sc::passkey::verifier *verifier;
        zend_object std;
    };

    passkey_object *object_of(zend_object *object) {
        return reinterpret_cast<passkey_object *>(reinterpret_cast<char *>(object) - XtOffsetOf(passkey_object, std));
    }

    zend_object *create(zend_class_entry *type) {
        auto *self = static_cast<passkey_object *>(zend_object_alloc(sizeof(passkey_object), type));
        self->verifier = nullptr;
        zend_object_std_init(&self->std, type);
        object_properties_init(&self->std, type);
        self->std.handlers = &handlers;
        return &self->std;
    }

    void free_object(zend_object *object) {
        delete object_of(object)->verifier;
        zend_object_std_dtor(object);
    }

    const sc::passkey::verifier &verifier_of(zval *self) {
        const auto *verifier = object_of(Z_OBJ_P(self))->verifier;
        if (!verifier) throw std::logic_error{"SC\\Passkey: not constructed"};
        return *verifier;
    }

    // JSON as PHP values: objects as associative arrays, as json_decode($json, true) gives them.
    void to_zval(zval *out, const nlohmann::ordered_json &json) {
        switch (json.type()) {
            case nlohmann::ordered_json::value_t::null:
            case nlohmann::ordered_json::value_t::discarded: ZVAL_NULL(out); break;
            case nlohmann::ordered_json::value_t::boolean: ZVAL_BOOL(out, json.get<bool>()); break;
            case nlohmann::ordered_json::value_t::number_integer: ZVAL_LONG(out, static_cast<zend_long>(json.get<std::int64_t>())); break;
            case nlohmann::ordered_json::value_t::number_unsigned: ZVAL_LONG(out, static_cast<zend_long>(json.get<std::uint64_t>())); break;
            case nlohmann::ordered_json::value_t::number_float: ZVAL_DOUBLE(out, json.get<double>()); break;
            case nlohmann::ordered_json::value_t::string: {
                const auto &text = json.get_ref<const std::string &>();
                ZVAL_STRINGL(out, text.data(), text.size());
                break;
            }
            case nlohmann::ordered_json::value_t::binary: ZVAL_NULL(out); break;
            case nlohmann::ordered_json::value_t::array: {
                array_init_size(out, static_cast<uint32_t>(json.size()));
                for (const auto &each: json) {
                    zval item;
                    to_zval(&item, each);
                    add_next_index_zval(out, &item);
                }
                break;
            }
            case nlohmann::ordered_json::value_t::object: {
                array_init_size(out, static_cast<uint32_t>(json.size()));
                for (const auto &[key, each]: json.items()) {
                    zval item;
                    to_zval(&item, each);
                    add_assoc_zval_ex(out, key.data(), key.size(), &item);
                }
                break;
            }
        }
    }

    // A list of credentials: each its id (base64url), or ['id' => ..., 'transports' => [...]].
    std::vector<sc::passkey::descriptor> descriptors_of(HashTable *given, const char *what) {
        std::vector<sc::passkey::descriptor> out;
        if (!given) return out;
        zval *each = nullptr;
        ZEND_HASH_FOREACH_VAL(given, each) {
            sc::passkey::descriptor item;
            if (Z_TYPE_P(each) == IS_STRING) {
                item.id = std::string{sc::php::view(Z_STR_P(each))};
            } else if (Z_TYPE_P(each) == IS_ARRAY) {
                zval *id = sc::php::find(Z_ARRVAL_P(each), "id");
                if (!id || Z_TYPE_P(id) != IS_STRING) throw std::invalid_argument{std::string{"SC\\Passkey: an entry of "} + what + " has no 'id'"};
                item.id = std::string{sc::php::view(Z_STR_P(id))};
                if (zval *transports = sc::php::find(Z_ARRVAL_P(each), "transports"); transports && Z_TYPE_P(transports) == IS_ARRAY) {
                    zval *transport = nullptr;
                    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(transports), transport) {
                        item.transports.push_back(sc::php::text_of(transport));
                    } ZEND_HASH_FOREACH_END();
                }
            } else {
                throw std::invalid_argument{std::string{"SC\\Passkey: "} + what + " holds ids (strings) or ['id' => ..., 'transports' => [...]]"};
            }
            out.push_back(std::move(item));
        } ZEND_HASH_FOREACH_END();
        return out;
    }

    std::chrono::milliseconds timeout_of(const zend_long given) {
        if (given < 1000 || given > 3600000) throw std::invalid_argument{"SC\\Passkey: a timeout is 1000 to 3600000 ms"};
        return std::chrono::milliseconds{given};
    }

    std::string user_verification_of(zend_string *given) {
        std::string out = given ? std::string{sc::php::view(given)} : "preferred";
        if (out != "required" && out != "preferred" && out != "discouraged")
            throw std::invalid_argument{"SC\\Passkey: userVerification is 'required', 'preferred' or 'discouraged'"};
        return out;
    }

    // Runs body as guarded() does, but a failed check becomes an SC\PasskeyException with its reason.
    template<typename Body>
    void checked(Body body) {
        try {
            body();
        } catch (const sc::passkey::error &problem) {
            zend_object *thrown = zend_throw_exception(passkey_exception_class, problem.what(), 0);
            zend_update_property_stringl(passkey_exception_class, thrown, "reason", sizeof("reason") - 1, problem.reason().data(), problem.reason().size());
        } catch (const std::exception &problem) {
            zend_throw_exception(sc::php::exception_class, problem.what(), 0);
        } catch (...) {
            zend_throw_exception(sc::php::exception_class, "unknown error", 0);
        }
    }

    ZEND_BEGIN_ARG_INFO_EX(arginfo_construct, 0, 0, 3)
        ZEND_ARG_TYPE_INFO(0, rpId, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, rpName, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, origins, IS_ARRAY, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(passkey_construct) {
        zend_string *rp_id = nullptr, *rp_name = nullptr;
        HashTable *origins = nullptr;
        ZEND_PARSE_PARAMETERS_START(3, 3)
            Z_PARAM_STR(rp_id)
            Z_PARAM_STR(rp_name)
            Z_PARAM_ARRAY_HT(origins)
        ZEND_PARSE_PARAMETERS_END();
        auto *self = object_of(Z_OBJ_P(ZEND_THIS));
        sc::php::guarded([&] {
            std::vector<std::string> list;
            zval *each = nullptr;
            ZEND_HASH_FOREACH_VAL(origins, each) {
                list.push_back(sc::php::text_of(each));
            } ZEND_HASH_FOREACH_END();
            auto made = std::make_unique<sc::passkey::verifier>(std::string{sc::php::view(rp_id)}, std::string{sc::php::view(rp_name)}, std::move(list));
            delete self->verifier;
            self->verifier = made.release();
        });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_registration_options, 0, 2, IS_ARRAY, 0)
        ZEND_ARG_TYPE_INFO(0, userId, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, userName, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, displayName, IS_STRING, 0, "''")
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, exclude, IS_ARRAY, 0, "[]")
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "300000")
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, userVerification, IS_STRING, 0, "'preferred'")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(passkey_registration_options) {
        zend_string *user_id = nullptr, *user_name = nullptr, *display_name = nullptr, *verification = nullptr;
        HashTable *exclude = nullptr;
        zend_long timeout = 300000;
        ZEND_PARSE_PARAMETERS_START(2, 6)
            Z_PARAM_STR(user_id)
            Z_PARAM_STR(user_name)
            Z_PARAM_OPTIONAL
            Z_PARAM_STR(display_name)
            Z_PARAM_ARRAY_HT(exclude)
            Z_PARAM_LONG(timeout)
            Z_PARAM_STR(verification)
        ZEND_PARSE_PARAMETERS_END();
        nlohmann::ordered_json out;
        checked([&] {
            out = verifier_of(ZEND_THIS).registration_options(
                {std::string{sc::php::view(user_id)}, std::string{sc::php::view(user_name)}, display_name ? std::string{sc::php::view(display_name)} : ""},
                descriptors_of(exclude, "exclude"), timeout_of(timeout), user_verification_of(verification));
        });
        if (EG(exception)) RETURN_THROWS();
        to_zval(return_value, out);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_authentication_options, 0, 0, IS_ARRAY, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, allow, IS_ARRAY, 0, "[]")
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout, IS_LONG, 0, "300000")
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, userVerification, IS_STRING, 0, "'preferred'")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(passkey_authentication_options) {
        HashTable *allow = nullptr;
        zend_long timeout = 300000;
        zend_string *verification = nullptr;
        ZEND_PARSE_PARAMETERS_START(0, 3)
            Z_PARAM_OPTIONAL
            Z_PARAM_ARRAY_HT(allow)
            Z_PARAM_LONG(timeout)
            Z_PARAM_STR(verification)
        ZEND_PARSE_PARAMETERS_END();
        nlohmann::ordered_json out;
        checked([&] {
            out = verifier_of(ZEND_THIS).authentication_options(descriptors_of(allow, "allow"), timeout_of(timeout), user_verification_of(verification));
        });
        if (EG(exception)) RETURN_THROWS();
        to_zval(return_value, out);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_verify_registration, 0, 2, IS_ARRAY, 0)
        ZEND_ARG_TYPE_INFO(0, response, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, challenge, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, requireUserVerification, _IS_BOOL, 0, "false")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(passkey_verify_registration) {
        zend_string *response = nullptr, *challenge = nullptr;
        bool require = false;
        ZEND_PARSE_PARAMETERS_START(2, 3)
            Z_PARAM_STR(response)
            Z_PARAM_STR(challenge)
            Z_PARAM_OPTIONAL
            Z_PARAM_BOOL(require)
        ZEND_PARSE_PARAMETERS_END();
        nlohmann::ordered_json out;
        checked([&] { out = verifier_of(ZEND_THIS).verify_registration(sc::php::view(response), sc::php::view(challenge), require).to_json(); });
        if (EG(exception)) RETURN_THROWS();
        to_zval(return_value, out);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_verify_authentication, 0, 4, IS_ARRAY, 0)
        ZEND_ARG_TYPE_INFO(0, response, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, challenge, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, publicKey, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, signCount, IS_LONG, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, requireUserVerification, _IS_BOOL, 0, "false")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(passkey_verify_authentication) {
        zend_string *response = nullptr, *challenge = nullptr, *public_key = nullptr;
        zend_long sign_count = 0;
        bool require = false;
        ZEND_PARSE_PARAMETERS_START(4, 5)
            Z_PARAM_STR(response)
            Z_PARAM_STR(challenge)
            Z_PARAM_STR(public_key)
            Z_PARAM_LONG(sign_count)
            Z_PARAM_OPTIONAL
            Z_PARAM_BOOL(require)
        ZEND_PARSE_PARAMETERS_END();
        nlohmann::ordered_json out;
        checked([&] {
            if (sign_count < 0 || sign_count > 0xffffffffLL) throw std::invalid_argument{"SC\\Passkey: signCount is 0 to 4294967295"};
            out = verifier_of(ZEND_THIS)
                      .verify_authentication(sc::php::view(response), sc::php::view(challenge), sc::php::view(public_key), static_cast<std::uint32_t>(sign_count),
                                             require)
                      .to_json();
        });
        if (EG(exception)) RETURN_THROWS();
        to_zval(return_value, out);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_credential_id, 0, 1, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, response, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(passkey_credential_id) {
        zend_string *response = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(response)
        ZEND_PARSE_PARAMETERS_END();
        std::string out;
        checked([&] { out = sc::passkey::credential_id(sc::php::view(response)); });
        if (EG(exception)) RETURN_THROWS();
        RETURN_STRINGL(out.data(), out.size());
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_get_reason, 0, 0, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(passkey_exception_get_reason) {
        ZEND_PARSE_PARAMETERS_NONE();
        zval unused;
        zval *reason = zend_read_property(passkey_exception_class, Z_OBJ_P(ZEND_THIS), "reason", sizeof("reason") - 1, 1, &unused);
        RETURN_STR(zval_get_string(reason));
    }

    const zend_function_entry methods[] = {
        ZEND_NAMED_ME(__construct, passkey_construct, arginfo_construct, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(registrationOptions, passkey_registration_options, arginfo_registration_options, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(authenticationOptions, passkey_authentication_options, arginfo_authentication_options, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(verifyRegistration, passkey_verify_registration, arginfo_verify_registration, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(verifyAuthentication, passkey_verify_authentication, arginfo_verify_authentication, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(credentialId, passkey_credential_id, arginfo_credential_id, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_FE_END
    };

    const zend_function_entry exception_methods[] = {
        ZEND_NAMED_ME(getReason, passkey_exception_get_reason, arginfo_get_reason, ZEND_ACC_PUBLIC)
        ZEND_FE_END
    };

    void startup() {
        zend_class_entry type;
        INIT_NS_CLASS_ENTRY(type, "SC", "Passkey", methods);
        passkey_class = zend_register_internal_class(&type);
        passkey_class->ce_flags |= ZEND_ACC_FINAL;
        passkey_class->create_object = create;
        std::memcpy(&handlers, zend_get_std_object_handlers(), sizeof(handlers));
        handlers.offset = XtOffsetOf(passkey_object, std);
        handlers.free_obj = free_object;
        handlers.clone_obj = nullptr;

        zend_class_entry exception;
        INIT_NS_CLASS_ENTRY(exception, "SC", "PasskeyException", exception_methods);
        passkey_exception_class = zend_register_internal_class_ex(&exception, sc::php::exception_class);
        zend_declare_property_string(passkey_exception_class, "reason", sizeof("reason") - 1, "", ZEND_ACC_PROTECTED);
    }
}

sc::php::area sc::php::passkey_area() { return {nullptr, startup}; }
