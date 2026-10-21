// SC\base64_encode() and SC\base64_decode(), by sc-core's sc::base64 (decoding throws for text
// that isn't base64, where PHP's own returns false).

#include "php_glue.h"

#include <base64.h>

namespace {
    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_base64, 0, 1, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(sc_base64_encode) {
        zend_string *data = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(data)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::return_guarded_string(return_value, [&] { return sc::base64::encode(sc::php::view(data)); });
    }

    ZEND_NAMED_FUNCTION(sc_base64_decode) {
        zend_string *data = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(data)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::return_guarded_string(return_value, [&] { return sc::base64::decode(sc::php::view(data)); });
    }

    const zend_function_entry functions[] = {
        ZEND_NS_NAMED_FE("SC", base64_encode, sc_base64_encode, arginfo_base64)
        ZEND_NS_NAMED_FE("SC", base64_decode, sc_base64_decode, arginfo_base64)
        ZEND_FE_END
    };
}

sc::php::area sc::php::base64_area() { return {functions, nullptr}; }
