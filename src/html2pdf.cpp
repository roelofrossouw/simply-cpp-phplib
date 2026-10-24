// SC\Html2Pdf: sc-html2pdf's sc::html2pdf (libsc-html2pdf's shared library, from its package).
//
//     $pdf = new SC\Html2Pdf($html, ['header' => $header, 'size' => 'A4', 'toc_page' => 2,
//                                    'margins' => ['top' => 10, 'first_top' => 60]]);
//     $pdf->save('/tmp/invoice.pdf');      // or $pdf->output(), the PDF as a string
//     $pdf->pageCount();
//
// The options are sc::html2pdf::options by name; one it doesn't know throws, so a typo shows.

#include "php_glue.h"

#include <html2pdf.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace {
    zend_class_entry *html2pdf_class = nullptr;
    zend_object_handlers handlers;

    struct html2pdf_object {
        sc::html2pdf *pdf;
        zend_object std;
    };

    html2pdf_object *object_of(zend_object *object) {
        return reinterpret_cast<html2pdf_object *>(reinterpret_cast<char *>(object) - XtOffsetOf(html2pdf_object, std));
    }

    zend_object *create(zend_class_entry *type) {
        auto *self = static_cast<html2pdf_object *>(zend_object_alloc(sizeof(html2pdf_object), type));
        self->pdf = nullptr;
        zend_object_std_init(&self->std, type);
        object_properties_init(&self->std, type);
        self->std.handlers = &handlers;
        return &self->std;
    }

    void free_object(zend_object *object) {
        delete object_of(object)->pdf;
        zend_object_std_dtor(object);
    }

    using sc::php::find;
    using sc::php::text_of;

    void check_keys(HashTable *table, const std::string &what, std::initializer_list<std::string_view> known) {
        zend_string *key = nullptr;
        ZEND_HASH_FOREACH_STR_KEY(table, key) {
            const std::string_view name = key ? sc::php::view(key) : std::string_view{};
            if (std::ranges::find(known, name) == known.end())
                throw std::invalid_argument{"SC\\Html2Pdf: unknown " + what + " '" + std::string{key ? name : "(a number)"} + "'"};
        } ZEND_HASH_FOREACH_END();
    }

    HashTable *array_of(zval *value, const std::string &name) {
        if (Z_TYPE_P(value) != IS_ARRAY) throw std::invalid_argument{"SC\\Html2Pdf: '" + name + "' must be an array"};
        return Z_ARRVAL_P(value);
    }

    sc::html2pdf::options options_of(HashTable *given) {
        check_keys(given, "option",
                   {"size", "landscape", "margins", "header", "footer", "css", "base_url", "media", "toc_page", "toc_title", "outline",
                    "metadata", "user_password", "owner_password", "split_boxes", "draw_margins", "builtin_fonts",
                    "image_resolution"});
        sc::html2pdf::options settings;
        if (zval *v = find(given, "size")) settings.size = sc::page_size_from(text_of(v));
        if (zval *v = find(given, "landscape")) settings.turned = zend_is_true(v) ? sc::orientation::landscape : sc::orientation::portrait;
        if (zval *v = find(given, "margins")) {
            HashTable *margins = array_of(v, "margins");
            check_keys(margins, "margin", {"top", "right", "bottom", "left", "first_top"});
            if (zval *m = find(margins, "top")) settings.margins.top = zval_get_double(m);
            if (zval *m = find(margins, "right")) settings.margins.right = zval_get_double(m);
            if (zval *m = find(margins, "bottom")) settings.margins.bottom = zval_get_double(m);
            if (zval *m = find(margins, "left")) settings.margins.left = zval_get_double(m);
            if (zval *m = find(margins, "first_top")) {
                if (Z_TYPE_P(m) == IS_NULL) settings.margins.first_top.reset(); // the same as the others
                else settings.margins.first_top = zval_get_double(m);
            }
        }
        if (zval *v = find(given, "header")) settings.header = text_of(v);
        if (zval *v = find(given, "footer")) settings.footer = text_of(v);
        if (zval *v = find(given, "css")) settings.css = text_of(v);
        if (zval *v = find(given, "base_url")) settings.html.base_url = text_of(v);
        if (zval *v = find(given, "media")) settings.html.media = text_of(v);
        if (zval *v = find(given, "toc_page")) {
            const zend_long page = zval_get_long(v);
            if (page < 0) throw std::invalid_argument{"SC\\Html2Pdf: 'toc_page' can't be negative"};
            settings.toc_page = static_cast<unsigned>(page);
        }
        if (zval *v = find(given, "toc_title")) settings.toc_title = text_of(v);
        if (zval *v = find(given, "outline")) settings.outline = zend_is_true(v);
        if (zval *v = find(given, "metadata")) {
            zend_string *key = nullptr;
            zval *value = nullptr;
            ZEND_HASH_FOREACH_STR_KEY_VAL(array_of(v, "metadata"), key, value) {
                if (key) settings.metadata[std::string{sc::php::view(key)}] = text_of(value);
            } ZEND_HASH_FOREACH_END();
        }
        zval *user = find(given, "user_password"), *owner = find(given, "owner_password");
        if (user || owner) {
            sc::pdf::protection locked;
            if (user) locked.user_password = text_of(user);
            if (owner) locked.owner_password = text_of(owner);
            if (!locked.user_password.empty() || !locked.owner_password.empty()) settings.protection = locked;
        }
        if (zval *v = find(given, "split_boxes")) settings.split_boxes = zend_is_true(v);
        if (zval *v = find(given, "draw_margins")) settings.draw_margins = zend_is_true(v);
        if (zval *v = find(given, "builtin_fonts")) settings.builtin_fonts = zend_is_true(v);
        if (zval *v = find(given, "image_resolution")) {
            const double resolution = zval_get_double(v);
            if (!(resolution >= 0) || !std::isfinite(resolution)) throw std::invalid_argument{"SC\\Html2Pdf: 'image_resolution' must be 0 or more"};
            settings.image_resolution = resolution;
        }
        return settings;
    }

    sc::html2pdf &laid_out(zval *self) {
        auto *pdf = object_of(Z_OBJ_P(self))->pdf;
        if (!pdf) throw std::logic_error{"SC\\Html2Pdf: not constructed"};
        return *pdf;
    }

    ZEND_BEGIN_ARG_INFO_EX(arginfo_construct, 0, 0, 1)
        ZEND_ARG_TYPE_INFO(0, html, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(html2pdf_construct) {
        zend_string *html = nullptr;
        HashTable *given = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_STR(html)
            Z_PARAM_OPTIONAL
            Z_PARAM_ARRAY_HT(given)
        ZEND_PARSE_PARAMETERS_END();
        auto *self = object_of(Z_OBJ_P(ZEND_THIS));
        sc::php::guarded([&] {
            auto made = std::make_unique<sc::html2pdf>(sc::php::view(html), given ? options_of(given) : sc::html2pdf::options{});
            delete self->pdf;
            self->pdf = made.release();
        });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_save, 0, 1, IS_VOID, 0)
        ZEND_ARG_TYPE_INFO(0, filename, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(html2pdf_save) {
        zend_string *filename = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(filename)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] { laid_out(ZEND_THIS).save(std::string{sc::php::view(filename)}); });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_output, 0, 0, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    // The PDF as a string: what save() writes (bytes()).
    ZEND_NAMED_FUNCTION(html2pdf_output) {
        ZEND_PARSE_PARAMETERS_NONE();
        sc::php::return_guarded_string(return_value, [&] { return laid_out(ZEND_THIS).bytes(); });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_page_count, 0, 0, IS_LONG, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(html2pdf_page_count) {
        ZEND_PARSE_PARAMETERS_NONE();
        zend_long pages = 0;
        sc::php::guarded([&] { pages = laid_out(ZEND_THIS).page_count(); });
        if (EG(exception)) RETURN_THROWS();
        RETURN_LONG(pages);
    }

    const zend_function_entry methods[] = {
        ZEND_NAMED_ME(__construct, html2pdf_construct, arginfo_construct, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(save, html2pdf_save, arginfo_save, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(output, html2pdf_output, arginfo_output, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(pageCount, html2pdf_page_count, arginfo_page_count, ZEND_ACC_PUBLIC)
        ZEND_FE_END
    };

    void startup() {
        zend_class_entry type;
        INIT_NS_CLASS_ENTRY(type, "SC", "Html2Pdf", methods);
        html2pdf_class = zend_register_internal_class(&type);
        html2pdf_class->create_object = create;
        std::memcpy(&handlers, zend_get_std_object_handlers(), sizeof(handlers));
        handlers.offset = XtOffsetOf(html2pdf_object, std);
        handlers.free_obj = free_object;
        handlers.clone_obj = nullptr; // a laid out document isn't copied
    }
}

sc::php::area sc::php::html2pdf_area() { return {nullptr, startup}; }
