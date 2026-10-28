// SC\BootstrapEmail: HTML written with Bootstrap 5's classes made into HTML email clients show as intended, by
// sc-bootstrap-email (libsc-bootstrap-email's shared library, from its package), a C++ port of bootstrap-email:
//
//     $html = SC\BootstrapEmail::format($email); // tables, CSS in style attributes, the head's stylesheet
//
// A static method: there's nothing to keep between calls. A failure is an SC\Exception.

#include "php_glue.h"

#include <sc_bootstrap_email.h>

namespace {
    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_format, 0, 1, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, html, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(bootstrap_email_format) {
        zend_string *html = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(html)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::return_guarded_string(return_value, [&] { return sc::bootstrap_email::compile(sc::php::view(html)); });
    }

    const zend_function_entry methods[] = {
        ZEND_NAMED_ME(format, bootstrap_email_format, arginfo_format, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_FE_END
    };

    void startup() {
        zend_class_entry type;
        INIT_NS_CLASS_ENTRY(type, "SC", "BootstrapEmail", methods);
        zend_class_entry *registered = zend_register_internal_class(&type);
        registered->ce_flags |= ZEND_ACC_FINAL;
    }
}

sc::php::area sc::php::bootstrap_email_area() { return {nullptr, startup}; }
