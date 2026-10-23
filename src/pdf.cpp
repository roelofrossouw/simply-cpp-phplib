// SC\Pdf: existing PDFs written again, by sc-pdflite (libsc-pdflite's shared library, from its
// package): pages merged, passwords added, a letterhead or stamp drawn on every page.
//
//     SC\Pdf::merge([$report, $appendix], $report, ['owner_password' => $secret,
//                                                   'permissions' => ['annotate' => true]]);
//     SC\Pdf::decorate($report, $letterhead, $report);           // 'over' for a stamp
//     SC\Pdf::protect($report, $report, ['user_password' => 'open']);
//     SC\Pdf::pageCount($report);
//
// Static methods: there's nothing to keep between calls. Every failure is an SC\Exception.

#include "php_glue.h"

#include <pdflite/merge.h>
#include <pdflite/reader.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    using sc::php::find;
    using sc::php::text_of;

    void check_keys(HashTable *table, const std::string &what, std::initializer_list<std::string_view> known) {
        zend_string *key = nullptr;
        ZEND_HASH_FOREACH_STR_KEY(table, key) {
            const std::string_view name = key ? sc::php::view(key) : std::string_view{};
            if (std::ranges::find(known, name) == known.end())
                throw std::invalid_argument{"SC\\Pdf: unknown " + what + " '" + std::string{key ? name : "(a number)"} + "'"};
        } ZEND_HASH_FOREACH_END();
    }

    // The protection array: passwords and permissions; nullopt when it sets no password.
    std::optional<sc::pdflite::protection> protection_of(HashTable *given) {
        if (!given) return std::nullopt;
        check_keys(given, "protection", {"user_password", "owner_password", "permissions"});
        sc::pdflite::protection settings;
        if (zval *v = find(given, "user_password")) settings.user_password = text_of(v);
        if (zval *v = find(given, "owner_password")) settings.owner_password = text_of(v);
        if (zval *v = find(given, "permissions")) {
            if (Z_TYPE_P(v) != IS_ARRAY) throw std::invalid_argument{"SC\\Pdf: 'permissions' must be an array"};
            HashTable *allowed = Z_ARRVAL_P(v);
            check_keys(allowed, "permission", {"print", "print_high_quality", "copy", "modify", "annotate", "fill_forms", "assemble"});
            auto &p = settings.permissions;
            const std::pair<std::string_view, bool *> flags[] = {{"print", &p.print}, {"print_high_quality", &p.print_high_quality}, {"copy", &p.copy},
                                                                 {"modify", &p.modify}, {"annotate", &p.annotate}, {"fill_forms", &p.fill_forms},
                                                                 {"assemble", &p.assemble}};
            for (const auto &[key, flag]: flags)
                if (zval *f = find(allowed, key)) *flag = zend_is_true(f);
        }
        if (settings.user_password.empty() && settings.owner_password.empty()) return std::nullopt;
        return settings;
    }

    sc::pdflite::layer layer_of(const std::string_view text) {
        if (text == "behind") return sc::pdflite::layer::behind;
        if (text == "over") return sc::pdflite::layer::over;
        throw std::invalid_argument{"SC\\Pdf: the layer is 'behind' or 'over', not '" + std::string{text} + "'"};
    }

    std::filesystem::path path_of(const zend_string *file) { return std::string{sc::php::view(file)}; }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_merge, 0, 2, IS_VOID, 0)
        ZEND_ARG_TYPE_INFO(0, files, IS_ARRAY, 0)
        ZEND_ARG_TYPE_INFO(0, output, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, protection, IS_ARRAY, 0, "[]")
    ZEND_END_ARG_INFO()

    // The files' pages, in order, into output (which may be one of them).
    ZEND_NAMED_FUNCTION(pdf_merge) {
        HashTable *files = nullptr, *protection = nullptr;
        zend_string *output = nullptr;
        ZEND_PARSE_PARAMETERS_START(2, 3)
            Z_PARAM_ARRAY_HT(files)
            Z_PARAM_STR(output)
            Z_PARAM_OPTIONAL
            Z_PARAM_ARRAY_HT(protection)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] {
            std::vector<std::filesystem::path> inputs;
            zval *file = nullptr;
            ZEND_HASH_FOREACH_VAL(files, file) { inputs.emplace_back(text_of(file)); } ZEND_HASH_FOREACH_END();
            sc::pdflite::merge(inputs, path_of(output), protection_of(protection));
        });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_decorate, 0, 3, IS_VOID, 0)
        ZEND_ARG_TYPE_INFO(0, file, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, decoration, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, output, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, layer, IS_STRING, 0, "\"behind\"")
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, protection, IS_ARRAY, 0, "[]")
    ZEND_END_ARG_INFO()

    // decoration's page n drawn behind (or over) file's page n, its last on the rest.
    ZEND_NAMED_FUNCTION(pdf_decorate) {
        zend_string *file = nullptr, *decoration = nullptr, *output = nullptr, *layer = nullptr;
        HashTable *protection = nullptr;
        ZEND_PARSE_PARAMETERS_START(3, 5)
            Z_PARAM_STR(file)
            Z_PARAM_STR(decoration)
            Z_PARAM_STR(output)
            Z_PARAM_OPTIONAL
            Z_PARAM_STR(layer)
            Z_PARAM_ARRAY_HT(protection)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] {
            sc::pdflite::decorate(path_of(file), path_of(decoration), path_of(output),
                                  layer ? layer_of(sc::php::view(layer)) : sc::pdflite::layer::behind, protection_of(protection));
        });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_protect, 0, 3, IS_VOID, 0)
        ZEND_ARG_TYPE_INFO(0, file, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, output, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, protection, IS_ARRAY, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, password, IS_STRING, 0, "\"\"")
    ZEND_END_ARG_INFO()

    // file written again with passwords; password opens it when it's encrypted already.
    ZEND_NAMED_FUNCTION(pdf_protect) {
        zend_string *file = nullptr, *output = nullptr, *password = nullptr;
        HashTable *protection = nullptr;
        ZEND_PARSE_PARAMETERS_START(3, 4)
            Z_PARAM_STR(file)
            Z_PARAM_STR(output)
            Z_PARAM_ARRAY_HT(protection)
            Z_PARAM_OPTIONAL
            Z_PARAM_STR(password)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] {
            const auto settings = protection_of(protection);
            if (!settings) throw std::invalid_argument{"SC\\Pdf::protect(): no password given"};
            sc::pdflite::protect(path_of(file), path_of(output), *settings, password ? sc::php::view(password) : std::string_view{});
        });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_page_count, 0, 1, IS_LONG, 0)
        ZEND_ARG_TYPE_INFO(0, file, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, password, IS_STRING, 0, "\"\"")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(pdf_page_count) {
        zend_string *file = nullptr, *password = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_STR(file)
            Z_PARAM_OPTIONAL
            Z_PARAM_STR(password)
        ZEND_PARSE_PARAMETERS_END();
        zend_long pages = 0;
        sc::php::guarded([&] {
            pages = static_cast<zend_long>(sc::pdflite::reader{path_of(file), password ? sc::php::view(password) : std::string_view{}}.page_count());
        });
        if (EG(exception)) RETURN_THROWS();
        RETURN_LONG(pages);
    }

    const zend_function_entry methods[] = {
        ZEND_NAMED_ME(merge, pdf_merge, arginfo_merge, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(decorate, pdf_decorate, arginfo_decorate, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(protect, pdf_protect, arginfo_protect, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(pageCount, pdf_page_count, arginfo_page_count, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_FE_END
    };

    void startup() {
        zend_class_entry type;
        INIT_NS_CLASS_ENTRY(type, "SC", "Pdf", methods);
        zend_class_entry *registered = zend_register_internal_class(&type);
        registered->ce_flags |= ZEND_ACC_FINAL;
    }
}

sc::php::area sc::php::pdf_area() { return {nullptr, startup}; }
