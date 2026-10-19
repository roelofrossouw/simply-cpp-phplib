// SC\svg_to_png(string $svg, int $width = 0, int $height = 0): string, by sc-image's sc::svg2png
// (libsc-image's shared library, from its package). A width or height of 0 keeps the aspect
// ratio; both 0 is the SVG's own size.

#include "php_glue.h"

#include <color.h>
#include <svg2png.h>

namespace {
    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_svg_to_png, 0, 1, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, svg, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, width, IS_LONG, 0, "0")
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, height, IS_LONG, 0, "0")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(sc_svg_to_png) {
        zend_string *svg = nullptr;
        zend_long width = 0, height = 0;
        ZEND_PARSE_PARAMETERS_START(1, 3)
            Z_PARAM_STR(svg)
            Z_PARAM_OPTIONAL
            Z_PARAM_LONG(width)
            Z_PARAM_LONG(height)
        ZEND_PARSE_PARAMETERS_END();
        std::string png;
        sc::php::guarded([&] {
            if (width < 0 || height < 0) throw std::invalid_argument{"SC\\svg_to_png: the width and height can't be negative"};
            png = width == 0 && height == 0 ? sc::svg2png::FromString(sc::php::view(svg))
                                            : sc::svg2png::FromString(sc::php::view(svg), static_cast<int>(width), static_cast<int>(height),
                                                                      sc::color{1, 1, 1});
        });
        if (EG(exception)) RETURN_THROWS();
        RETURN_STRINGL(png.data(), png.size());
    }

    const zend_function_entry functions[] = {
        ZEND_NS_NAMED_FE("SC", svg_to_png, sc_svg_to_png, arginfo_svg_to_png)
        ZEND_FE_END
    };
}

sc::php::area sc::php::svg_area() { return {functions, nullptr}; }
