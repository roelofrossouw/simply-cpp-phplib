// The sc extension: SC\Exception, the areas' functions and classes, and phpinfo()'s line.

#include "php_glue.h"

#include <ext/standard/info.h>

#include <vector>

namespace sc::php {
    zend_class_entry *exception_class = nullptr;

    namespace {
        // The extension's parts: a new one is a file and a line here.
        const std::vector<area> &areas() {
            static const std::vector<area> all{base64_area(), svg_area(), html2pdf_area()};
            return all;
        }

        // Every area's functions in one table, as PHP takes them.
        const zend_function_entry *functions() {
            static const std::vector<zend_function_entry> table = [] {
                std::vector<zend_function_entry> all;
                for (const auto &each: areas()) {
                    for (const auto *entry = each.functions; entry && entry->fname; ++entry) all.push_back(*entry);
                }
                all.push_back(ZEND_FE_END);
                return all;
            }();
            return table.data();
        }
    }
}

static PHP_MINIT_FUNCTION(sc) {
    zend_class_entry exception;
    INIT_NS_CLASS_ENTRY(exception, "SC", "Exception", nullptr);
    sc::php::exception_class = zend_register_internal_class_ex(&exception, zend_ce_exception);
    for (const auto &each: sc::php::areas())
        if (each.startup) each.startup();
    return SUCCESS;
}

static PHP_MINFO_FUNCTION(sc) {
    php_info_print_table_start();
    php_info_print_table_row(2, "simply-cpp (sc)", "enabled");
    php_info_print_table_row(2, "Version", SC_PHPLIB_VERSION);
    php_info_print_table_row(2, "Provides", "SC\\Html2Pdf, SC\\svg_to_png(), SC\\base64_encode(), SC\\base64_decode()");
    php_info_print_table_end();
}

static zend_module_entry sc_module_entry = {
    STANDARD_MODULE_HEADER,
    "sc",
    nullptr, // the functions, filled in by get_module()
    PHP_MINIT(sc),
    nullptr,
    nullptr,
    nullptr,
    PHP_MINFO(sc),
    SC_PHPLIB_VERSION,
    STANDARD_MODULE_PROPERTIES
};

extern "C" ZEND_DLEXPORT zend_module_entry *get_module() {
    sc_module_entry.functions = sc::php::functions();
    return &sc_module_entry;
}
