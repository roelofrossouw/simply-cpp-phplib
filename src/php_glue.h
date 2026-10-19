#pragma once

// What every part of the extension shares: PHP's headers, the SC\Exception class, how a C++
// exception becomes a PHP one, reading arguments, and how a part (an "area") plugs in.
//
// An area is one file: its functions (zend_function_entry, ending with ZEND_FE_END) and, when it
// has classes, a startup() that registers them. sc_php.cpp lists the areas; adding one is a file
// and a line there.

#include <php.h>
#include <zend_exceptions.h>

#include <exception>
#include <optional>
#include <string>
#include <string_view>

namespace sc::php {
    // SC\Exception, an \Exception: what every C++ failure becomes.
    extern zend_class_entry *exception_class;

    // Runs body; a C++ exception becomes an SC\Exception. PHP's own errors don't longjmp out of
    // this (since PHP 8 argument errors are exceptions), so C++ objects in body are destroyed
    // as usual. After it, a function returns when EG(exception) is set (RETURN_THROWS()).
    template<typename Body>
    void guarded(Body body) {
        try {
            body();
        } catch (const std::exception &error) {
            zend_throw_exception(exception_class, error.what(), 0);
        } catch (...) {
            zend_throw_exception(exception_class, "unknown error", 0);
        }
    }

    inline std::string_view view(const zend_string *text) { return {ZSTR_VAL(text), ZSTR_LEN(text)}; }

    // An array's value by key, or nullptr.
    inline zval *find(HashTable *table, const std::string_view key) { return zend_hash_str_find(table, key.data(), key.size()); }

    // A value as text (numbers and booleans converted, as PHP would).
    inline std::string text_of(zval *value) {
        zend_string *text = zval_get_string(value);
        std::string out{view(text)};
        zend_string_release(text);
        return out;
    }

    struct area {
        const zend_function_entry *functions; // ending with ZEND_FE_END; nullptr for none
        void (*startup)();                    // registers its classes; nullptr for none
    };

    area base64_area();
    area svg_area();
    area html2pdf_area();
}
