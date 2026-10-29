// SC\QrCode: sc-qrencode's sc::qr::code (libsc-qrencode's shared library, from its package).
//
//     $qr = new SC\QrCode('https://example.com/invoice/123');
//     $svg = $qr->svg(30);        // 30 mm square; svg() alone has a viewBox only and scales
//     $png = $qr->png(8);         // 8 pixels a module
//     $qr = SC\QrCode::binary($bytes, 'high');
//
// The correction level is 'low', 'medium' (the default), 'quartile' or 'high'. A failure is an
// SC\Exception.

#include "php_glue.h"

#include <sc_qrencode.h>

#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
    zend_class_entry *qr_code_class = nullptr;
    zend_object_handlers handlers;

    struct qr_code_object {
        sc::qr::code *code;
        zend_object std;
    };

    qr_code_object *object_of(zend_object *object) {
        return reinterpret_cast<qr_code_object *>(reinterpret_cast<char *>(object) - XtOffsetOf(qr_code_object, std));
    }

    zend_object *create(zend_class_entry *type) {
        auto *self = static_cast<qr_code_object *>(zend_object_alloc(sizeof(qr_code_object), type));
        self->code = nullptr;
        zend_object_std_init(&self->std, type);
        object_properties_init(&self->std, type);
        self->std.handlers = &handlers;
        return &self->std;
    }

    void free_object(zend_object *object) {
        delete object_of(object)->code;
        zend_object_std_dtor(object);
    }

    sc::qr::correction correction_of(const zend_string *name) {
        if (!name) return sc::qr::correction::medium;
        const auto text = sc::php::view(name);
        if (text == "low") return sc::qr::correction::low;
        if (text == "medium") return sc::qr::correction::medium;
        if (text == "quartile") return sc::qr::correction::quartile;
        if (text == "high") return sc::qr::correction::high;
        throw std::invalid_argument{"SC\\QrCode: the correction is 'low', 'medium', 'quartile' or 'high'"};
    }

    const char *name_of(const sc::qr::correction level) {
        switch (level) {
            case sc::qr::correction::low: return "low";
            case sc::qr::correction::medium: return "medium";
            case sc::qr::correction::quartile: return "quartile";
            case sc::qr::correction::high: return "high";
        }
        return "medium";
    }

    const sc::qr::code &made(zval *self) {
        const auto *code = object_of(Z_OBJ_P(self))->code;
        if (!code) throw std::logic_error{"SC\\QrCode: not constructed"};
        return *code;
    }

    int quiet_zone_of(const zend_long zone) {
        if (zone < 0 || zone > 100) throw std::invalid_argument{"SC\\QrCode: the quiet zone is 0 to 100 modules"};
        return static_cast<int>(zone);
    }

    ZEND_BEGIN_ARG_INFO_EX(arginfo_construct, 0, 0, 1)
        ZEND_ARG_TYPE_INFO(0, text, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, correction, IS_STRING, 0, "'medium'")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(qr_code_construct) {
        zend_string *text = nullptr, *correction = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_STR(text)
            Z_PARAM_OPTIONAL
            Z_PARAM_STR(correction)
        ZEND_PARSE_PARAMETERS_END();
        auto *self = object_of(Z_OBJ_P(ZEND_THIS));
        sc::php::guarded([&] {
            auto code = std::make_unique<sc::qr::code>(sc::php::view(text), correction_of(correction));
            delete self->code;
            self->code = code.release();
        });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_binary, 0, 1, SC\\QrCode, 0)
        ZEND_ARG_TYPE_INFO(0, bytes, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, correction, IS_STRING, 0, "'medium'")
    ZEND_END_ARG_INFO()

    // Bytes as they are, for data that isn't text.
    ZEND_NAMED_FUNCTION(qr_code_binary) {
        zend_string *bytes = nullptr, *correction = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_STR(bytes)
            Z_PARAM_OPTIONAL
            Z_PARAM_STR(correction)
        ZEND_PARSE_PARAMETERS_END();
        std::unique_ptr<sc::qr::code> code;
        sc::php::guarded([&] { code = std::make_unique<sc::qr::code>(sc::qr::code::binary(sc::php::view(bytes), correction_of(correction))); });
        if (EG(exception)) RETURN_THROWS();
        object_init_ex(return_value, qr_code_class);
        delete object_of(Z_OBJ_P(return_value))->code;
        object_of(Z_OBJ_P(return_value))->code = code.release();
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_svg, 0, 0, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, size, IS_DOUBLE, 1, "null")
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, quietZone, IS_LONG, 0, "4")
    ZEND_END_ARG_INFO()

    // size: the picture's width and height in mm, quiet zone included; null: a viewBox only.
    ZEND_NAMED_FUNCTION(qr_code_svg) {
        double size = 0;
        bool none = true;
        zend_long zone = 4;
        ZEND_PARSE_PARAMETERS_START(0, 2)
            Z_PARAM_OPTIONAL
            Z_PARAM_DOUBLE_OR_NULL(size, none)
            Z_PARAM_LONG(zone)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::return_guarded_string(return_value, [&] {
            const auto millimetres = none ? std::nullopt : std::optional<sc::distance>{sc::dist::mm(size)};
            return made(ZEND_THIS).svg(millimetres, quiet_zone_of(zone));
        });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_png, 0, 0, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, modulePixels, IS_LONG, 0, "8")
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, quietZone, IS_LONG, 0, "4")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(qr_code_png) {
        zend_long pixels = 8, zone = 4;
        ZEND_PARSE_PARAMETERS_START(0, 2)
            Z_PARAM_OPTIONAL
            Z_PARAM_LONG(pixels)
            Z_PARAM_LONG(zone)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::return_guarded_string(return_value, [&] {
            if (pixels < 1 || pixels > 100) throw std::invalid_argument{"SC\\QrCode: a module is 1 to 100 pixels"};
            return made(ZEND_THIS).png(static_cast<int>(pixels), quiet_zone_of(zone));
        });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_long, 0, 0, IS_LONG, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(qr_code_version) {
        ZEND_PARSE_PARAMETERS_NONE();
        zend_long version = 0;
        sc::php::guarded([&] { version = made(ZEND_THIS).version(); });
        if (EG(exception)) RETURN_THROWS();
        RETURN_LONG(version);
    }

    ZEND_NAMED_FUNCTION(qr_code_size) {
        ZEND_PARSE_PARAMETERS_NONE();
        zend_long size = 0;
        sc::php::guarded([&] { size = made(ZEND_THIS).size(); });
        if (EG(exception)) RETURN_THROWS();
        RETURN_LONG(size);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_correction, 0, 0, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(qr_code_correction) {
        ZEND_PARSE_PARAMETERS_NONE();
        sc::php::return_guarded_string(return_value, [&] { return std::string{name_of(made(ZEND_THIS).level())}; });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dark, 0, 2, _IS_BOOL, 0)
        ZEND_ARG_TYPE_INFO(0, x, IS_LONG, 0)
        ZEND_ARG_TYPE_INFO(0, y, IS_LONG, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(qr_code_dark) {
        zend_long x = 0, y = 0;
        ZEND_PARSE_PARAMETERS_START(2, 2)
            Z_PARAM_LONG(x)
            Z_PARAM_LONG(y)
        ZEND_PARSE_PARAMETERS_END();
        bool dark = false;
        sc::php::guarded([&] {
            const auto &code = made(ZEND_THIS);
            dark = x >= 0 && y >= 0 && x < code.size() && y < code.size() && code.dark(static_cast<int>(x), static_cast<int>(y));
        });
        if (EG(exception)) RETURN_THROWS();
        RETURN_BOOL(dark);
    }

    const zend_function_entry methods[] = {
        ZEND_NAMED_ME(__construct, qr_code_construct, arginfo_construct, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(binary, qr_code_binary, arginfo_binary, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(svg, qr_code_svg, arginfo_svg, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(png, qr_code_png, arginfo_png, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(version, qr_code_version, arginfo_long, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(size, qr_code_size, arginfo_long, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(correction, qr_code_correction, arginfo_correction, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(dark, qr_code_dark, arginfo_dark, ZEND_ACC_PUBLIC)
        ZEND_FE_END
    };

    void startup() {
        zend_class_entry type;
        INIT_NS_CLASS_ENTRY(type, "SC", "QrCode", methods);
        qr_code_class = zend_register_internal_class(&type);
        qr_code_class->ce_flags |= ZEND_ACC_FINAL;
        qr_code_class->create_object = create;
        std::memcpy(&handlers, zend_get_std_object_handlers(), sizeof(handlers));
        handlers.offset = XtOffsetOf(qr_code_object, std);
        handlers.free_obj = free_object;
        handlers.clone_obj = nullptr;
    }
}

sc::php::area sc::php::qr_code_area() { return {nullptr, startup}; }
