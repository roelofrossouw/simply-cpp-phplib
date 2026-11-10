// SC\Word: a Word document's (.docx) text, sc-office's sc::office::word::text(): a line per
// paragraph with its list number as Word shows it ("4.1.2 the Purchaser ..."), tables a row per
// line with their cells separated by " | ".
//
//     $text = SC\Word::text('/tmp/agreement.docx');
//     $text = SC\Word::textFromString($bytes, ['headers_footers' => true, 'notes' => true]);

#include "php_glue.h"

#include <office.h>

#include <stdexcept>

namespace {
    using sc::php::find;

    // The options: headers_footers, notes (booleans); one it doesn't know throws.
    sc::office::word::text_options options_of(HashTable *given) {
        sc::office::word::text_options out;
        if (!given) return out;
        zend_string *key = nullptr;
        ZEND_HASH_FOREACH_STR_KEY(given, key) {
            const std::string_view name = key ? sc::php::view(key) : std::string_view{};
            if (name != "headers_footers" && name != "notes")
                throw std::invalid_argument{"SC\\Word: unknown option '" + std::string{key ? name : "(a number)"} + "'"};
        } ZEND_HASH_FOREACH_END();
        if (zval *v = find(given, "headers_footers")) out.headers_footers = zend_is_true(v);
        if (zval *v = find(given, "notes")) out.notes = zend_is_true(v);
        return out;
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_text, 0, 1, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(word_text) {
        zend_string *path = nullptr;
        HashTable *given = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_PATH_STR(path)
            Z_PARAM_OPTIONAL
            Z_PARAM_ARRAY_HT(given)
        ZEND_PARSE_PARAMETERS_END();
        std::string out;
        sc::php::guarded_native([&] { out = sc::office::word::text(std::string{sc::php::view(path)}, options_of(given)); });
        if (EG(exception)) RETURN_THROWS();
        RETURN_STRINGL(out.data(), out.size());
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_text_from_string, 0, 1, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, bytes, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(word_text_from_string) {
        zend_string *bytes = nullptr;
        HashTable *given = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_STR(bytes)
            Z_PARAM_OPTIONAL
            Z_PARAM_ARRAY_HT(given)
        ZEND_PARSE_PARAMETERS_END();
        std::string out;
        sc::php::guarded_native([&] { out = sc::office::word::text_in(std::string{sc::php::view(bytes)}, options_of(given)); });
        if (EG(exception)) RETURN_THROWS();
        RETURN_STRINGL(out.data(), out.size());
    }

    const zend_function_entry methods[] = {
        ZEND_NAMED_ME(text, word_text, arginfo_text, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(textFromString, word_text_from_string, arginfo_text_from_string, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_FE_END
    };

    void startup() {
        zend_class_entry type;
        INIT_NS_CLASS_ENTRY(type, "SC", "Word", methods);
        auto *word_class = zend_register_internal_class(&type);
        word_class->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NOT_SERIALIZABLE | ZEND_ACC_NO_DYNAMIC_PROPERTIES;
    }
}

sc::php::area sc::php::word_area() { return {nullptr, startup}; }
