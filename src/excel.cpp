// SC\Excel: Excel workbooks (.xlsx), by sc-office's sc::office::excel (libsc-office's shared
// library, from its package). They work as the C++ ones do, with PHP's ArrayAccess,
// IteratorAggregate and Countable: a workbook holds sheets, a sheet the rows that have cells, a row
// the cells that are set.
//
//     $book = SC\Excel\Workbook::open('prices.xlsx');
//     foreach ($book as $sheet)
//         foreach ($sheet as $row)
//             foreach ($row as $cell) echo $cell->name, ' ', $cell->value, ' ', $cell->formula;
//     if (isset($book['Prices']['B2'])) $price = $book['Prices']['B2']->value;
//
//     $out = new SC\Excel\Workbook();
//     $out['Prices']['A1'] = 'Item';          // the sheet added, as std::map would
//     $out['Prices']['B3']->formula = '=B2*2';
//     $out['Prices']['A1']->style->font->bold = true;
//     $out['Prices']['A1']->style->background = '#ddebf7';
//     $out['Prices']['A1']->style->borders->bottom = 'double';
//     $out['Prices']['A1']->style->alignment->horizontal = 'center';
//     $out['Prices']['B2']->style->numberFormat = '#,##0.00';
//     $out['Prices']->width('A', 20);         // characters
//     $out['Prices']->height(0, 24);          // row 1, points
//     $out['Prices']->merge('A1:D1');
//     $out['Prices']->hiddenColumn('E', true);
//     $out['Prices']->freeze('A2');           // row 1 stays in view
//     $out->save('out.xlsx');                 // or $out->output(), the file as a string
//
// A Sheet, Row, Cell, Style, Font, Borders, Edge or Alignment is a view into its workbook (which it keeps alive), not a
// copy: changing it changes the workbook. Every failure is an SC\Exception.
//
// SC\Csv reads CSV files into the same SC\Excel\Sheet and writes any sheet's values as CSV (sc::office::csv), so
// code that only uses data works the same on both:
//
//     $prices = SC\Csv::open('prices.csv');          // , or ; found by itself
//     $total = $prices['B2']->value;
//     SC\Csv::save($book['Prices'], 'prices.csv');    // an Excel sheet's values

#include "php_glue.h"

#include <excel.h>
#include <sc_csv.h>

#include <ext/spl/spl_array.h>
#include <zend_interfaces.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

namespace {
    namespace excel = sc::office::excel;
    using sc::php::view;

    zend_class_entry *workbook_class = nullptr, *sheet_class = nullptr, *row_class = nullptr, *cell_class = nullptr, *style_class = nullptr,
                     *font_class = nullptr, *borders_class = nullptr, *edge_class = nullptr, *alignment_class = nullptr;
    zend_object_handlers workbook_handlers, part_handlers;

    // ---- the objects ----

    struct workbook_object {
        excel::workbook *book;
        zend_object std;
    };

    // A Sheet, Row, Cell, or a cell's Style, Font, Borders, Edge or Alignment: its workbook's object (kept alive
    // while this one lives), the sheet, the row and column it stands for (as far as it has them), and
    // an Edge's side.
    struct part_object {
        zend_object *owner;
        excel::sheet *sheet;
        std::size_t row;
        std::size_t column;
        std::size_t side;
        zend_object std;
    };

    // The sides of a cell's borders, and its diagonals, in the order an Edge's side counts them.
    constexpr std::string_view sides[] = {"left", "right", "top", "bottom", "diagonalUp", "diagonalDown"};

    excel::edge excel::borders::*const side_members[] = {&excel::borders::left,   &excel::borders::right,       &excel::borders::top,
                                                         &excel::borders::bottom, &excel::borders::diagonal_up, &excel::borders::diagonal_down};

    workbook_object *workbook_of(zend_object *object) {
        return reinterpret_cast<workbook_object *>(reinterpret_cast<char *>(object) - XtOffsetOf(workbook_object, std));
    }

    part_object *part_of(zend_object *object) {
        return reinterpret_cast<part_object *>(reinterpret_cast<char *>(object) - XtOffsetOf(part_object, std));
    }

    zend_object *create_workbook(zend_class_entry *type) {
        auto *self = static_cast<workbook_object *>(zend_object_alloc(sizeof(workbook_object), type));
        self->book = new excel::workbook;
        zend_object_std_init(&self->std, type);
        object_properties_init(&self->std, type);
        self->std.handlers = &workbook_handlers;
        return &self->std;
    }

    void free_workbook(zend_object *object) {
        delete workbook_of(object)->book;
        zend_object_std_dtor(object);
    }

    // clone $book: a workbook of its own.
    zend_object *clone_workbook(zend_object *object) {
        zend_object *copy = create_workbook(object->ce);
        *workbook_of(copy)->book = *workbook_of(object)->book;
        zend_objects_clone_members(copy, object);
        return copy;
    }

    zend_object *create_part(zend_class_entry *type) {
        auto *self = static_cast<part_object *>(zend_object_alloc(sizeof(part_object), type));
        self->owner = nullptr;
        self->sheet = nullptr;
        self->row = self->column = self->side = 0;
        zend_object_std_init(&self->std, type);
        object_properties_init(&self->std, type);
        self->std.handlers = &part_handlers;
        return &self->std;
    }

    void free_part(zend_object *object) {
        if (auto *owner = part_of(object)->owner) OBJ_RELEASE(owner);
        zend_object_std_dtor(object);
    }

    // A Sheet, Row or Cell of the workbook in owner, into out.
    void make_part(zval *out, zend_class_entry *type, zend_object *owner, excel::sheet &sheet, const std::size_t row = 0,
                   const std::size_t column = 0, const std::size_t side = 0) {
        object_init_ex(out, type);
        auto *self = part_of(Z_OBJ_P(out));
        GC_ADDREF(owner);
        self->owner = owner;
        self->sheet = &sheet;
        self->row = row;
        self->column = column;
        self->side = side;
    }

    part_object &part(zval *self) {
        auto *made = part_of(Z_OBJ_P(self));
        if (!made->sheet) throw std::logic_error{"SC\\Excel: sheets, rows and cells come from a workbook"};
        return *made;
    }

    excel::workbook &book(zval *self) { return *workbook_of(Z_OBJ_P(self))->book; }

    // ---- values ----

    void zval_of(const excel::value &value, zval *out) {
        if (const auto *text = std::get_if<std::string>(&value)) ZVAL_STRINGL(out, text->data(), text->size());
        else if (const auto *number = std::get_if<double>(&value)) ZVAL_DOUBLE(out, *number);
        else if (const auto *truth = std::get_if<bool>(&value)) ZVAL_BOOL(out, *truth);
        else ZVAL_NULL(out);
    }

    excel::value value_of(zval *given) {
        ZVAL_DEREF(given);
        switch (Z_TYPE_P(given)) {
            case IS_NULL: return {};
            case IS_TRUE: return true;
            case IS_FALSE: return false;
            case IS_LONG: return static_cast<double>(Z_LVAL_P(given));
            case IS_DOUBLE: return Z_DVAL_P(given);
            case IS_STRING: return std::string{view(Z_STR_P(given))};
            default: throw std::invalid_argument{std::string{"SC\\Excel: a cell holds null, a number, text or a bool, not "} + zend_zval_type_name(given)};
        }
    }

    // The cell a Cell, Style or Font stands for as it is now: a plain empty one when it isn't there.
    const excel::cell &cell_now(const part_object &self) {
        static const excel::cell none;
        const excel::address where{self.row, self.column};
        return self.sheet && self.sheet->contains(where) ? self.sheet->at(where) : none;
    }

    // The cell a Cell, Style or Font stands for, added when it isn't there, to be changed.
    excel::cell &cell_to_change(part_object &self) { return (*self.sheet)[{self.row, self.column}]; }

    // The part object given is of that class (and from a workbook): what it stands for.
    const part_object *given_part(zval *given, const zend_class_entry *type) {
        if (Z_TYPE_P(given) != IS_OBJECT || Z_OBJCE_P(given) != type) return nullptr;
        const auto *from = part_of(Z_OBJ_P(given));
        return from->sheet ? from : nullptr;
    }

    // A colour as "#rrggbb"; null: none.
    void zval_of(const std::optional<sc::color> &color, zval *out) {
        if (!color) {
            ZVAL_NULL(out);
            return;
        }
        const auto hex = color->to_hex();
        ZVAL_STRINGL(out, hex.data(), hex.size());
    }

    // A colour given as CSS gives one ("#c00000", "#c00", "red", "rgb(192, 0, 0)"); null: none.
    // Text that isn't a colour throws (sc::color would make it black).
    std::optional<sc::color> color_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) == IS_NULL) return std::nullopt;
        if (Z_TYPE_P(given) != IS_STRING) throw std::invalid_argument{"SC\\Excel: a colour is text, \"#c00000\" or \"red\", or null"};
        std::string text{view(Z_STR_P(given))};
        const sc::color color{text};
        std::ranges::transform(text, text.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (color == sc::color::Black && text != "black" && text != "#000" && text != "#000000" && !text.starts_with("rgb") && !text.starts_with("hsl"))
            throw std::invalid_argument{"SC\\Excel: \"" + std::string{view(Z_STR_P(given))} + "\" isn't a colour"};
        return color;
    }

    // A font's size in points, from 1 to 409; null: the workbook's.
    std::optional<sc::distance> size_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) == IS_NULL) return std::nullopt;
        if (Z_TYPE_P(given) != IS_LONG && Z_TYPE_P(given) != IS_DOUBLE) throw std::invalid_argument{"SC\\Excel: a font's size is a number of points, or null"};
        const double points = zval_get_double(given);
        if (!(points >= 1 && points <= 409)) throw std::out_of_range{"SC\\Excel: a font's size is from 1 to 409 points"};
        return points;
    }

    // Each of an array's string keys, with its value; a key not in known throws.
    template<typename Each>
    void each_key(zval *given, const std::initializer_list<std::string_view> known, const char *what, Each each) {
        zend_string *key = nullptr;
        zval *value = nullptr;
        ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(given), key, value) {
            if (!key || std::ranges::find(known, view(key)) == known.end())
                throw std::invalid_argument{std::string{"SC\\Excel: "} + what + " has no '" + (key ? std::string{view(key)} : std::string{"(a number)"}) + "'"};
            each(view(key), value);
        } ZEND_HASH_FOREACH_END();
    }

    // Excel's name for an underline or a border's line; null for none.
    template<typename Kind>
    void zval_of_kind(const Kind kind, zval *out) {
        if (kind == Kind::none) {
            ZVAL_NULL(out);
            return;
        }
        const auto name = excel::name(kind);
        ZVAL_STRINGL(out, name.data(), name.size());
    }

    // An underline given: Excel's name ("single", "double", "singleAccounting", "doubleAccounting"),
    // true for single, false or null for none.
    excel::underline underline_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) == IS_NULL || Z_TYPE_P(given) == IS_FALSE) return excel::underline::none;
        if (Z_TYPE_P(given) == IS_TRUE) return excel::underline::single;
        if (Z_TYPE_P(given) != IS_STRING) throw std::invalid_argument{"SC\\Excel: an underline is \"single\", \"double\" ..., true, false or null"};
        return excel::underline_named(view(Z_STR_P(given)));
    }

    // How text sits across a cell, by Excel's name ("general", "left", "center", "right", "fill",
    // "justify", "centerContinuous", "distributed"); null for general.
    excel::horizontal horizontal_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) == IS_NULL) return excel::horizontal::general;
        if (Z_TYPE_P(given) != IS_STRING) throw std::invalid_argument{"SC\\Excel: a horizontal alignment is \"left\", \"center\" ... or null"};
        return excel::horizontal_named(view(Z_STR_P(given)));
    }

    // How text sits up and down a cell ("bottom", "top", "center", "justify", "distributed"); null
    // for bottom.
    excel::vertical vertical_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) == IS_NULL) return excel::vertical::bottom;
        if (Z_TYPE_P(given) != IS_STRING) throw std::invalid_argument{"SC\\Excel: a vertical alignment is \"top\", \"center\" ... or null"};
        return excel::vertical_named(view(Z_STR_P(given)));
    }

    // An indent, 0 to 250 steps.
    unsigned indent_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) != IS_LONG || Z_LVAL_P(given) < 0 || Z_LVAL_P(given) > 250)
            throw std::out_of_range{"SC\\Excel: an indent is a whole number from 0 to 250"};
        return static_cast<unsigned>(Z_LVAL_P(given));
    }

    // A text rotation, in whole degrees from -90 to 90.
    int rotation_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) != IS_LONG || Z_LVAL_P(given) < -90 || Z_LVAL_P(given) > 90)
            throw std::out_of_range{"SC\\Excel: a rotation is a whole number of degrees from -90 to 90"};
        return static_cast<int>(Z_LVAL_P(given));
    }

    // A border's line given: Excel's name ("thin", "double", "mediumDashDot" ...), or null for none.
    excel::line line_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) == IS_NULL) return excel::line::none;
        if (Z_TYPE_P(given) != IS_STRING) throw std::invalid_argument{"SC\\Excel: a border's line is \"thin\", \"double\" ... or null"};
        return excel::line_named(view(Z_STR_P(given)));
    }

    // A border's side given: another cell's Edge, ['line' => 'thin', 'color' => '#1f4e79'], a line
    // ("thin": automatic colour), or null for none.
    excel::edge edge_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) == IS_NULL || Z_TYPE_P(given) == IS_STRING) return {line_of(given)};
        if (const auto *from = given_part(given, edge_class)) return cell_now(*from).style.borders.*side_members[from->side];
        if (Z_TYPE_P(given) != IS_ARRAY) throw std::invalid_argument{"SC\\Excel: a border's side is an Edge, ['line' => ..., 'color' => ...], a line or null"};
        excel::edge edge;
        each_key(given, {"line", "color"}, "a border's side", [&](const std::string_view key, zval *value) {
            if (key == "line") edge.line = line_of(value);
            else edge.color = color_of(value);
        });
        return edge;
    }

    // Borders given: another cell's Borders, ['left' => ..., 'right' => ..., 'top' => ..., 'bottom'
    // => ..., 'diagonalUp' => ..., 'diagonalDown' => ...] (those left out have none), one side for
    // all four (an Edge, a line, ['line' => ..., 'color' => ...]; no diagonals), or null for none.
    excel::borders borders_of(zval *given) {
        ZVAL_DEREF(given);
        if (const auto *from = given_part(given, borders_class)) return cell_now(*from).style.borders;
        if (Z_TYPE_P(given) == IS_ARRAY) {
            bool by_side = false;
            zend_string *key = nullptr;
            ZEND_HASH_FOREACH_STR_KEY(Z_ARRVAL_P(given), key) {
                by_side = by_side || (key && std::ranges::find(sides, view(key)) != std::end(sides));
            } ZEND_HASH_FOREACH_END();
            if (by_side) {
                excel::borders borders;
                each_key(given, {"left", "right", "top", "bottom", "diagonalUp", "diagonalDown"}, "borders", [&](const std::string_view key, zval *value) {
                    borders.*side_members[std::ranges::find(sides, key) - std::begin(sides)] = edge_of(value);
                });
                return borders;
            }
        }
        return excel::borders::all(edge_of(given));
    }

    // An alignment given: another cell's Alignment, ['horizontal' => 'center', 'vertical' => 'top',
    // 'wrap' => true, 'indent' => 1, 'rotation' => 45, 'stacked' => false] (what's left out is the
    // default), or null for the default.
    excel::alignment alignment_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) == IS_NULL) return {};
        if (const auto *from = given_part(given, alignment_class)) return cell_now(*from).style.alignment;
        if (Z_TYPE_P(given) != IS_ARRAY)
            throw std::invalid_argument{"SC\\Excel: an alignment is an Alignment, ['horizontal' => ..., 'vertical' => ..., 'wrap' => ..., 'indent' => ..., "
                                        "'rotation' => ..., 'stacked' => ...] or null"};
        excel::alignment alignment;
        each_key(given, {"horizontal", "vertical", "wrap", "indent", "rotation", "stacked"}, "an alignment", [&](const std::string_view key, zval *value) {
            if (key == "horizontal") alignment.horizontal = horizontal_of(value);
            else if (key == "vertical") alignment.vertical = vertical_of(value);
            else if (key == "wrap") alignment.wrap = zend_is_true(value);
            else if (key == "indent") alignment.indent = indent_of(value);
            else if (key == "rotation") alignment.rotation = rotation_of(value);
            else alignment.stacked = zend_is_true(value);
        });
        return alignment;
    }

    // A font given: another cell's Font, ['size' => 14, 'color' => '#c00000', 'bold' => true,
    // 'italic' => true, 'strike' => true, 'underline' => 'single'] (what's left out is the default),
    // or null for the default font.
    excel::font font_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) == IS_NULL) return {};
        if (const auto *from = given_part(given, font_class)) return cell_now(*from).style.font;
        if (Z_TYPE_P(given) != IS_ARRAY) throw std::invalid_argument{"SC\\Excel: a font is a Font, an array ['size' => ..., 'color' => ..., 'bold' => ..., 'italic' => ..., 'strike' => ..., 'underline' => ...] or null"};
        excel::font font;
        each_key(given, {"size", "color", "bold", "italic", "strike", "underline"}, "a font", [&](const std::string_view key, zval *value) {
            if (key == "size") font.size = size_of(value);
            else if (key == "color") font.color = color_of(value);
            else if (key == "bold") font.bold = zend_is_true(value);
            else if (key == "italic") font.italic = zend_is_true(value);
            else if (key == "strike") font.strike = zend_is_true(value);
            else font.underline = underline_of(value);
        });
        return font;
    }

    // A number format: Excel's code ("#,##0.00", "0%", "yyyy-mm-dd"); "" or null for General.
    std::string number_format_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) == IS_NULL) return {};
        if (Z_TYPE_P(given) != IS_STRING) throw std::invalid_argument{"SC\\Excel: a number format is Excel's code, \"#,##0.00\", or null"};
        if (ZSTR_LEN(Z_STR_P(given)) > 255) throw std::invalid_argument{"SC\\Excel: a number format is at most 255 characters"};
        return std::string{view(Z_STR_P(given))};
    }

    // A style given: another cell's Style, ['font' => [...], 'background' => '#ddebf7', 'borders'
    // => ..., 'alignment' => [...], 'numberFormat' => '0.00'] (what's left out is the default), or
    // null for the plain style.
    excel::style style_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) == IS_NULL) return {};
        if (const auto *from = given_part(given, style_class)) return cell_now(*from).style;
        if (Z_TYPE_P(given) != IS_ARRAY) throw std::invalid_argument{"SC\\Excel: a style is a Style, an array ['font' => [...], 'background' => ..., 'borders' => ..., 'alignment' => [...], "
                                                                                          "'numberFormat' => ...] or null"};
        excel::style style;
        each_key(given, {"font", "background", "borders", "alignment", "numberFormat"}, "a style", [&](const std::string_view key, zval *value) {
            if (key == "font") style.font = font_of(value);
            else if (key == "background") style.background = color_of(value);
            else if (key == "borders") style.borders = borders_of(value);
            else if (key == "alignment") style.alignment = alignment_of(value);
            else style.number_format = number_format_of(value);
        });
        return style;
    }

    // A cell given to a sheet or row: another cell (its value, formula and style), or a value.
    void assign(excel::cell &cell, zval *given) {
        ZVAL_DEREF(given);
        if (const auto *from = given_part(given, cell_class)) {
            const auto source = cell_now(*from); // a copy: it may be this cell
            cell.value = source.value;
            cell.formula = source.formula;
            cell.style = source.style;
            return;
        }
        cell = value_of(given);
    }

    // A cell's address: "B3", or [row, column] from 0.
    excel::address address_of(zval *offset) {
        ZVAL_DEREF(offset);
        if (Z_TYPE_P(offset) == IS_STRING) return excel::address{view(Z_STR_P(offset))};
        if (Z_TYPE_P(offset) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(offset)) == 2) {
            zval *row = zend_hash_index_find(Z_ARRVAL_P(offset), 0), *column = zend_hash_index_find(Z_ARRVAL_P(offset), 1);
            if (row && column && Z_TYPE_P(row) == IS_LONG && Z_TYPE_P(column) == IS_LONG && Z_LVAL_P(row) >= 0 && Z_LVAL_P(column) >= 0)
                return {static_cast<std::size_t>(Z_LVAL_P(row)), static_cast<std::size_t>(Z_LVAL_P(column))};
        }
        throw std::invalid_argument{"SC\\Excel: a cell is \"B3\" or [row, column] (from 0)"};
    }

    // A range: "A1:D3", or [first, last], each a cell as address_of() takes it.
    excel::range range_of(zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) == IS_STRING) return excel::range{view(Z_STR_P(given))};
        if (Z_TYPE_P(given) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(given)) == 2) {
            zval *first = zend_hash_index_find(Z_ARRVAL_P(given), 0), *last = zend_hash_index_find(Z_ARRVAL_P(given), 1);
            if (first && last) return {address_of(first), address_of(last)};
        }
        throw std::invalid_argument{"SC\\Excel: a range is \"A1:D3\" or [first, last] (each \"B3\" or [row, column])"};
    }

    // A column in a row: 1 (from 0), or its letters, "B".
    std::size_t column_of(zval *offset) {
        ZVAL_DEREF(offset);
        if (Z_TYPE_P(offset) == IS_LONG && Z_LVAL_P(offset) >= 0) return static_cast<std::size_t>(Z_LVAL_P(offset));
        if (Z_TYPE_P(offset) == IS_STRING) {
            const auto letters = view(Z_STR_P(offset));
            if (!letters.empty() && letters.find_first_of("0123456789") == std::string_view::npos)
                return excel::address{std::string{letters} + "1"}.column;
        }
        throw std::invalid_argument{"SC\\Excel: a row's column is a number (from 0) or letters (\"B\")"};
    }

    std::string name_of(zval *offset) {
        ZVAL_DEREF(offset);
        if (Z_TYPE_P(offset) != IS_STRING) throw std::invalid_argument{"SC\\Excel: a sheet is found by its name"};
        return std::string{view(Z_STR_P(offset))};
    }

    // An ArrayIterator over items, for getIterator(); items is released.
    void iterator_of(zval *items, zval *return_value) {
        object_init_ex(return_value, spl_ce_ArrayIterator);
        zend_call_known_instance_method_with_1_params(spl_ce_ArrayIterator->constructor, Z_OBJ_P(return_value), nullptr, items);
        zval_ptr_dtor(items);
    }

    // ---- arginfo shared by the containers (ArrayAccess, IteratorAggregate, Countable) ----

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offset_exists, 0, 1, _IS_BOOL, 0)
        ZEND_ARG_TYPE_INFO(0, offset, IS_MIXED, 0)
    ZEND_END_ARG_INFO()

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offset_get, 0, 1, IS_MIXED, 0)
        ZEND_ARG_TYPE_INFO(0, offset, IS_MIXED, 0)
    ZEND_END_ARG_INFO()

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offset_set, 0, 2, IS_VOID, 0)
        ZEND_ARG_TYPE_INFO(0, offset, IS_MIXED, 0)
        ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
    ZEND_END_ARG_INFO()

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offset_unset, 0, 1, IS_VOID, 0)
        ZEND_ARG_TYPE_INFO(0, offset, IS_MIXED, 0)
    ZEND_END_ARG_INFO()

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_get_iterator, 0, 0, Iterator, 0)
    ZEND_END_ARG_INFO()

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_count, 0, 0, IS_LONG, 0)
    ZEND_END_ARG_INFO()

    ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, 0, 0)
    ZEND_END_ARG_INFO()

    // Sheets, rows and cells come from a workbook.
    ZEND_NAMED_FUNCTION(part_construct) { ZEND_PARSE_PARAMETERS_NONE(); }

    // ---- SC\Excel\Workbook ----

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_open, 0, 1, SC\\Excel\\Workbook, 0)
        ZEND_ARG_TYPE_INFO(0, filename, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(workbook_open) {
        zend_string *filename = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(filename)
        ZEND_PARSE_PARAMETERS_END();
        std::unique_ptr<excel::workbook> read;
        sc::php::guarded([&] { read = std::make_unique<excel::workbook>(excel::read(std::string{view(filename)})); });
        if (EG(exception)) RETURN_THROWS();
        object_init_ex(return_value, workbook_class);
        delete workbook_of(Z_OBJ_P(return_value))->book;
        workbook_of(Z_OBJ_P(return_value))->book = read.release();
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_from_string, 0, 1, SC\\Excel\\Workbook, 0)
        ZEND_ARG_TYPE_INFO(0, bytes, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(workbook_from_string) {
        zend_string *bytes = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(bytes)
        ZEND_PARSE_PARAMETERS_END();
        std::unique_ptr<excel::workbook> read;
        sc::php::guarded([&] { read = std::make_unique<excel::workbook>(excel::read_from(std::string{view(bytes)})); });
        if (EG(exception)) RETURN_THROWS();
        object_init_ex(return_value, workbook_class);
        delete workbook_of(Z_OBJ_P(return_value))->book;
        workbook_of(Z_OBJ_P(return_value))->book = read.release();
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_save, 0, 1, IS_VOID, 0)
        ZEND_ARG_TYPE_INFO(0, filename, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(workbook_save) {
        zend_string *filename = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(filename)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] { excel::write(book(ZEND_THIS), std::string{view(filename)}); });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_output, 0, 0, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    // The .xlsx as a string: what save() writes.
    ZEND_NAMED_FUNCTION(workbook_output) {
        ZEND_PARSE_PARAMETERS_NONE();
        sc::php::return_guarded_string(return_value, [&] { return excel::bytes(book(ZEND_THIS)); });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_add, 0, 1, SC\\Excel\\Sheet, 0)
        ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(workbook_add) {
        zend_string *name = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(name)
        ZEND_PARSE_PARAMETERS_END();
        excel::sheet *added = nullptr;
        sc::php::guarded([&] { added = &book(ZEND_THIS).add(std::string{view(name)}); });
        if (EG(exception)) RETURN_THROWS();
        make_part(return_value, sheet_class, Z_OBJ_P(ZEND_THIS), *added);
    }

    ZEND_NAMED_FUNCTION(workbook_offset_exists) {
        zval *offset = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_ZVAL(offset)
        ZEND_PARSE_PARAMETERS_END();
        bool found = false;
        sc::php::guarded([&] { found = book(ZEND_THIS).contains(name_of(offset)); });
        if (EG(exception)) RETURN_THROWS();
        RETURN_BOOL(found);
    }

    // $book['Prices']: the sheet, added when there's none (as std::map would).
    ZEND_NAMED_FUNCTION(workbook_offset_get) {
        zval *offset = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_ZVAL(offset)
        ZEND_PARSE_PARAMETERS_END();
        excel::sheet *found = nullptr;
        sc::php::guarded([&] { found = &book(ZEND_THIS)[name_of(offset)]; });
        if (EG(exception)) RETURN_THROWS();
        make_part(return_value, sheet_class, Z_OBJ_P(ZEND_THIS), *found);
    }

    ZEND_NAMED_FUNCTION(workbook_offset_set) {
        zval *offset = nullptr, *value = nullptr;
        ZEND_PARSE_PARAMETERS_START(2, 2)
            Z_PARAM_ZVAL(offset)
            Z_PARAM_ZVAL(value)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([] { throw std::logic_error{"SC\\Excel\\Workbook: a sheet is added by $book[$name] or add($name), not set"}; });
    }

    ZEND_NAMED_FUNCTION(workbook_offset_unset) {
        zval *offset = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_ZVAL(offset)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([] { throw std::logic_error{"SC\\Excel\\Workbook: sheets can't be removed"}; });
    }

    // The sheets, by name, in the workbook's order.
    ZEND_NAMED_FUNCTION(workbook_get_iterator) {
        ZEND_PARSE_PARAMETERS_NONE();
        zval items;
        array_init(&items);
        for (auto &sheet: book(ZEND_THIS)) {
            zval each;
            make_part(&each, sheet_class, Z_OBJ_P(ZEND_THIS), sheet);
            zend_symtable_str_update(Z_ARRVAL(items), sheet.name().data(), sheet.name().size(), &each);
        }
        iterator_of(&items, return_value);
    }

    ZEND_NAMED_FUNCTION(workbook_count) {
        ZEND_PARSE_PARAMETERS_NONE();
        RETURN_LONG(static_cast<zend_long>(book(ZEND_THIS).size()));
    }

    const zend_function_entry workbook_methods[] = {
        ZEND_NAMED_ME(open, workbook_open, arginfo_open, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(fromString, workbook_from_string, arginfo_from_string, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(save, workbook_save, arginfo_save, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(output, workbook_output, arginfo_output, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(add, workbook_add, arginfo_add, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(offsetExists, workbook_offset_exists, arginfo_offset_exists, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(offsetGet, workbook_offset_get, arginfo_offset_get, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(offsetSet, workbook_offset_set, arginfo_offset_set, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(offsetUnset, workbook_offset_unset, arginfo_offset_unset, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(getIterator, workbook_get_iterator, arginfo_get_iterator, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(count, workbook_count, arginfo_count, ZEND_ACC_PUBLIC)
        ZEND_FE_END
    };

    // ---- SC\Excel\Sheet ----

    ZEND_NAMED_FUNCTION(sheet_offset_exists) {
        zval *offset = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_ZVAL(offset)
        ZEND_PARSE_PARAMETERS_END();
        bool found = false;
        sc::php::guarded([&] { found = part(ZEND_THIS).sheet->contains(address_of(offset)); });
        if (EG(exception)) RETURN_THROWS();
        RETURN_BOOL(found);
    }

    // $sheet['B2']: the cell, there or not yet (setting its value or formula adds it).
    ZEND_NAMED_FUNCTION(sheet_offset_get) {
        zval *offset = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_ZVAL(offset)
        ZEND_PARSE_PARAMETERS_END();
        excel::address where;
        sc::php::guarded([&] {
            where = address_of(offset);
            (void) part(ZEND_THIS);
            if (where.row >= excel::max_rows || where.column >= excel::max_columns) throw std::out_of_range{"SC\\Excel: cell beyond XFD1048576"};
        });
        if (EG(exception)) RETURN_THROWS();
        const auto &self = part(ZEND_THIS);
        make_part(return_value, cell_class, self.owner, *self.sheet, where.row, where.column);
    }

    ZEND_NAMED_FUNCTION(sheet_offset_set) {
        zval *offset = nullptr, *value = nullptr;
        ZEND_PARSE_PARAMETERS_START(2, 2)
            Z_PARAM_ZVAL(offset)
            Z_PARAM_ZVAL(value)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] {
            if (Z_TYPE_P(offset) == IS_NULL) throw std::invalid_argument{"SC\\Excel\\Sheet: $sheet[] = ...: which cell?"};
            assign((*part(ZEND_THIS).sheet)[address_of(offset)], value);
        });
    }

    ZEND_NAMED_FUNCTION(sheet_offset_unset) {
        zval *offset = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_ZVAL(offset)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] { part(ZEND_THIS).sheet->erase(address_of(offset)); });
    }

    // The rows that have cells, by their index (from 0).
    ZEND_NAMED_FUNCTION(sheet_get_iterator) {
        ZEND_PARSE_PARAMETERS_NONE();
        zval items;
        array_init(&items);
        sc::php::guarded([&] {
            auto &self = part(ZEND_THIS);
            for (const auto &row: *self.sheet) {
                zval each;
                make_part(&each, row_class, self.owner, *self.sheet, row.index());
                zend_hash_index_update(Z_ARRVAL(items), static_cast<zend_ulong>(row.index()), &each);
            }
        });
        if (EG(exception)) {
            zval_ptr_dtor(&items);
            RETURN_THROWS();
        }
        iterator_of(&items, return_value);
    }

    ZEND_NAMED_FUNCTION(sheet_count) {
        ZEND_PARSE_PARAMETERS_NONE();
        zend_long rows = 0;
        sc::php::guarded([&] { rows = static_cast<zend_long>(part(ZEND_THIS).sheet->size()); });
        if (EG(exception)) RETURN_THROWS();
        RETURN_LONG(rows);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_width, 0, 1, IS_DOUBLE, 1)
        ZEND_ARG_TYPE_MASK(0, column, MAY_BE_LONG | MAY_BE_STRING, NULL)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, characters, IS_DOUBLE, 1, "null")
    ZEND_END_ARG_INFO()

    // $sheet->width('B'): the column's width in characters, null for Excel's default;
    // $sheet->width('B', 20) sets it (from 0 to 255), $sheet->width('B', null) gives it the default.
    // Either way it returns the width.
    ZEND_NAMED_FUNCTION(sheet_width) {
        zval *column = nullptr;
        double characters = 0;
        bool none = true;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_ZVAL(column)
            Z_PARAM_OPTIONAL
            Z_PARAM_DOUBLE_OR_NULL(characters, none)
        ZEND_PARSE_PARAMETERS_END();
        std::optional<double> width;
        sc::php::guarded([&] {
            auto &self = part(ZEND_THIS);
            const auto index = column_of(column);
            if (ZEND_NUM_ARGS() == 2) self.sheet->width(index, none ? std::nullopt : std::optional{characters});
            width = self.sheet->width(index);
        });
        if (EG(exception)) RETURN_THROWS();
        if (!width) RETURN_NULL();
        RETURN_DOUBLE(*width);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_height, 0, 1, IS_DOUBLE, 1)
        ZEND_ARG_TYPE_INFO(0, row, IS_LONG, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, points, IS_DOUBLE, 1, "null")
    ZEND_END_ARG_INFO()

    // $sheet->height(0): row 1's height in points, null for Excel's default; $sheet->height(0, 24)
    // sets it (from 0 to 409), $sheet->height(0, null) gives it the default. Either way it returns
    // the height.
    ZEND_NAMED_FUNCTION(sheet_height) {
        zend_long row = 0;
        double points = 0;
        bool none = true;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_LONG(row)
            Z_PARAM_OPTIONAL
            Z_PARAM_DOUBLE_OR_NULL(points, none)
        ZEND_PARSE_PARAMETERS_END();
        std::optional<sc::distance> height;
        sc::php::guarded([&] {
            auto &self = part(ZEND_THIS);
            if (row < 0) throw std::out_of_range{"SC\\Excel: a row is a number from 0"};
            const auto index = static_cast<std::size_t>(row);
            if (ZEND_NUM_ARGS() == 2) self.sheet->height(index, none ? std::nullopt : std::optional<sc::distance>{points});
            height = self.sheet->height(index);
        });
        if (EG(exception)) RETURN_THROWS();
        if (!height) RETURN_NULL();
        RETURN_DOUBLE(height->points());
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_merge, 0, 1, IS_VOID, 0)
        ZEND_ARG_TYPE_MASK(0, range, MAY_BE_STRING | MAY_BE_ARRAY, NULL)
    ZEND_END_ARG_INFO()

    // $sheet->merge('A1:D1'): the range shown as one cell, its top left.
    ZEND_NAMED_FUNCTION(sheet_merge) {
        zval *cells = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_ZVAL(cells)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] { part(ZEND_THIS).sheet->merge(range_of(cells)); });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_unmerge, 0, 1, _IS_BOOL, 0)
        ZEND_ARG_TYPE_MASK(0, range, MAY_BE_STRING | MAY_BE_ARRAY, NULL)
    ZEND_END_ARG_INFO()

    // $sheet->unmerge('A1:D1'): undoes exactly that merge; whether there was one.
    ZEND_NAMED_FUNCTION(sheet_unmerge) {
        zval *cells = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_ZVAL(cells)
        ZEND_PARSE_PARAMETERS_END();
        bool removed = false;
        sc::php::guarded([&] { removed = part(ZEND_THIS).sheet->unmerge(range_of(cells)) > 0; });
        if (EG(exception)) RETURN_THROWS();
        RETURN_BOOL(removed);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_hidden_column, 0, 1, _IS_BOOL, 0)
        ZEND_ARG_TYPE_MASK(0, column, MAY_BE_LONG | MAY_BE_STRING, NULL)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, hide, _IS_BOOL, 1, "null")
    ZEND_END_ARG_INFO()

    // $sheet->hiddenColumn('E'): whether the column is hidden; $sheet->hiddenColumn('E', true) hides
    // it, false shows it again (keeping its width). Either way it returns whether it's hidden.
    ZEND_NAMED_FUNCTION(sheet_hidden_column) {
        zval *column = nullptr;
        bool hide = false, none = true;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_ZVAL(column)
            Z_PARAM_OPTIONAL
            Z_PARAM_BOOL_OR_NULL(hide, none)
        ZEND_PARSE_PARAMETERS_END();
        bool hidden = false;
        sc::php::guarded([&] {
            auto &self = part(ZEND_THIS);
            const auto index = column_of(column);
            if (!none) self.sheet->hidden_column(index, hide);
            hidden = self.sheet->hidden_column(index);
        });
        if (EG(exception)) RETURN_THROWS();
        RETURN_BOOL(hidden);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_hidden_row, 0, 1, _IS_BOOL, 0)
        ZEND_ARG_TYPE_INFO(0, row, IS_LONG, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, hide, _IS_BOOL, 1, "null")
    ZEND_END_ARG_INFO()

    // $sheet->hiddenRow(4): whether row 5 is hidden; $sheet->hiddenRow(4, true) hides it, false shows
    // it again (keeping its height and cells). Either way it returns whether it's hidden.
    ZEND_NAMED_FUNCTION(sheet_hidden_row) {
        zend_long row = 0;
        bool hide = false, none = true;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_LONG(row)
            Z_PARAM_OPTIONAL
            Z_PARAM_BOOL_OR_NULL(hide, none)
        ZEND_PARSE_PARAMETERS_END();
        bool hidden = false;
        sc::php::guarded([&] {
            auto &self = part(ZEND_THIS);
            if (row < 0) throw std::out_of_range{"SC\\Excel: a row is a number from 0"};
            const auto index = static_cast<std::size_t>(row);
            if (!none) self.sheet->hidden_row(index, hide);
            hidden = self.sheet->hidden_row(index);
        });
        if (EG(exception)) RETURN_THROWS();
        RETURN_BOOL(hidden);
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_freeze, 0, 1, IS_VOID, 0)
        ZEND_ARG_TYPE_MASK(0, cell, MAY_BE_STRING | MAY_BE_ARRAY, NULL)
    ZEND_END_ARG_INFO()

    // $sheet->freeze('B2'): the rows above and the columns left of the cell stay in view, as Excel's
    // Freeze Panes does there; 'A1' freezes nothing.
    ZEND_NAMED_FUNCTION(sheet_freeze) {
        zval *cell = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_ZVAL(cell)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] { part(ZEND_THIS).sheet->freeze(address_of(cell)); });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_void, 0, 0, IS_VOID, 0)
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(sheet_unfreeze) {
        ZEND_PARSE_PARAMETERS_NONE();
        sc::php::guarded([&] { part(ZEND_THIS).sheet->unfreeze(); });
    }

    const zend_function_entry sheet_methods[] = {
        ZEND_NAMED_ME(hiddenColumn, sheet_hidden_column, arginfo_hidden_column, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(hiddenRow, sheet_hidden_row, arginfo_hidden_row, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(freeze, sheet_freeze, arginfo_freeze, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(unfreeze, sheet_unfreeze, arginfo_void, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(width, sheet_width, arginfo_width, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(height, sheet_height, arginfo_height, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(merge, sheet_merge, arginfo_merge, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(unmerge, sheet_unmerge, arginfo_unmerge, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(__construct, part_construct, arginfo_none, ZEND_ACC_PRIVATE)
        ZEND_NAMED_ME(offsetExists, sheet_offset_exists, arginfo_offset_exists, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(offsetGet, sheet_offset_get, arginfo_offset_get, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(offsetSet, sheet_offset_set, arginfo_offset_set, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(offsetUnset, sheet_offset_unset, arginfo_offset_unset, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(getIterator, sheet_get_iterator, arginfo_get_iterator, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(count, sheet_count, arginfo_count, ZEND_ACC_PUBLIC)
        ZEND_FE_END
    };

    // ---- SC\Excel\Row ----

    // The row as the sheet has it now; nullptr when it has no cells.
    excel::row *row_now(part_object &self) {
        for (auto &row: *self.sheet)
            if (row.index() == self.row) return &row;
        return nullptr;
    }

    ZEND_NAMED_FUNCTION(row_offset_exists) {
        zval *offset = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_ZVAL(offset)
        ZEND_PARSE_PARAMETERS_END();
        bool found = false;
        sc::php::guarded([&] {
            auto &self = part(ZEND_THIS);
            found = self.sheet->contains({self.row, column_of(offset)});
        });
        if (EG(exception)) RETURN_THROWS();
        RETURN_BOOL(found);
    }

    ZEND_NAMED_FUNCTION(row_offset_get) {
        zval *offset = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_ZVAL(offset)
        ZEND_PARSE_PARAMETERS_END();
        std::size_t column = 0;
        sc::php::guarded([&] {
            (void) part(ZEND_THIS);
            column = column_of(offset);
            if (column >= excel::max_columns) throw std::out_of_range{"SC\\Excel: no column beyond XFD"};
        });
        if (EG(exception)) RETURN_THROWS();
        const auto &self = part(ZEND_THIS);
        make_part(return_value, cell_class, self.owner, *self.sheet, self.row, column);
    }

    ZEND_NAMED_FUNCTION(row_offset_set) {
        zval *offset = nullptr, *value = nullptr;
        ZEND_PARSE_PARAMETERS_START(2, 2)
            Z_PARAM_ZVAL(offset)
            Z_PARAM_ZVAL(value)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] {
            if (Z_TYPE_P(offset) == IS_NULL) throw std::invalid_argument{"SC\\Excel\\Row: $row[] = ...: which column?"};
            auto &self = part(ZEND_THIS);
            assign((*self.sheet)[{self.row, column_of(offset)}], value);
        });
    }

    ZEND_NAMED_FUNCTION(row_offset_unset) {
        zval *offset = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_ZVAL(offset)
        ZEND_PARSE_PARAMETERS_END();
        sc::php::guarded([&] {
            auto &self = part(ZEND_THIS);
            self.sheet->erase({self.row, column_of(offset)});
        });
    }

    // The cells that are set, by their column (from 0).
    ZEND_NAMED_FUNCTION(row_get_iterator) {
        ZEND_PARSE_PARAMETERS_NONE();
        zval items;
        array_init(&items);
        sc::php::guarded([&] {
            auto &self = part(ZEND_THIS);
            if (const auto *row = row_now(self))
                for (const auto &cell: *row) {
                    zval each;
                    make_part(&each, cell_class, self.owner, *self.sheet, cell.row, cell.column);
                    zend_hash_index_update(Z_ARRVAL(items), static_cast<zend_ulong>(cell.column), &each);
                }
        });
        if (EG(exception)) {
            zval_ptr_dtor(&items);
            RETURN_THROWS();
        }
        iterator_of(&items, return_value);
    }

    ZEND_NAMED_FUNCTION(row_count) {
        ZEND_PARSE_PARAMETERS_NONE();
        zend_long cells = 0;
        sc::php::guarded([&] {
            const auto *row = row_now(part(ZEND_THIS));
            cells = row ? static_cast<zend_long>(row->size()) : 0;
        });
        if (EG(exception)) RETURN_THROWS();
        RETURN_LONG(cells);
    }

    const zend_function_entry row_methods[] = {
        ZEND_NAMED_ME(__construct, part_construct, arginfo_none, ZEND_ACC_PRIVATE)
        ZEND_NAMED_ME(offsetExists, row_offset_exists, arginfo_offset_exists, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(offsetGet, row_offset_get, arginfo_offset_get, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(offsetSet, row_offset_set, arginfo_offset_set, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(offsetUnset, row_offset_unset, arginfo_offset_unset, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(getIterator, row_get_iterator, arginfo_get_iterator, ZEND_ACC_PUBLIC)
        ZEND_NAMED_ME(count, row_count, arginfo_count, ZEND_ACC_PUBLIC)
        ZEND_FE_END
    };

    // Cell, Style and Font: properties only.
    const zend_function_entry cell_methods[] = {
        ZEND_NAMED_ME(__construct, part_construct, arginfo_none, ZEND_ACC_PRIVATE)
        ZEND_FE_END
    };

    // ---- properties: Sheet's name, widths, heights, merged, hiddenColumns, hiddenRows and frozen,
    // Row's index, Cell's row, column, name, value, formula
    // and style, Style's font, background, borders, alignment and numberFormat, Font's size, color,
    // bold, italic, strike and underline, Borders' left, right, top, bottom, diagonalUp and
    // diagonalDown, Edge's line and color, Alignment's horizontal, vertical, wrap, indent, rotation
    // and stacked ----

    // The property's value into out; false when the object has no such property.
    bool property(zend_object *object, const std::string_view name, zval *out) {
        auto &self = *part_of(object);
        if (!self.sheet) return false;
        if (object->ce == sheet_class) {
            if (name == "name") {
                ZVAL_STRINGL(out, self.sheet->name().data(), self.sheet->name().size());
            } else if (name == "widths") { // by column number, as rows and cells are
                array_init(out);
                for (const auto &[column, width]: self.sheet->widths()) add_index_double(out, static_cast<zend_ulong>(column), width);
            } else if (name == "heights") { // by row number, in points
                array_init(out);
                for (const auto &[row, height]: self.sheet->heights()) add_index_double(out, static_cast<zend_ulong>(row), height.points());
            } else if (name == "hiddenColumns" || name == "hiddenRows") { // their numbers
                array_init(out);
                for (const auto each: name == "hiddenColumns" ? self.sheet->hidden_columns() : self.sheet->hidden_rows())
                    add_next_index_long(out, static_cast<zend_long>(each));
            } else if (name == "frozen") { // "B2", or null
                if (const auto &frozen = self.sheet->frozen()) {
                    const auto text = frozen->name();
                    ZVAL_STRINGL(out, text.data(), text.size());
                } else ZVAL_NULL(out);
            } else if (name == "merged") { // "A1:D1" ...
                array_init(out);
                for (const auto &cells: self.sheet->merged()) {
                    const auto text = cells.name();
                    add_next_index_stringl(out, text.data(), text.size());
                }
            } else return false;
            return true;
        }
        if (object->ce == style_class) {
            if (name == "font") make_part(out, font_class, self.owner, *self.sheet, self.row, self.column);
            else if (name == "background") zval_of(cell_now(self).style.background, out);
            else if (name == "borders") make_part(out, borders_class, self.owner, *self.sheet, self.row, self.column);
            else if (name == "alignment") make_part(out, alignment_class, self.owner, *self.sheet, self.row, self.column);
            else if (name == "numberFormat") {
                const auto &code = cell_now(self).style.number_format;
                ZVAL_STRINGL(out, code.data(), code.size());
            } else return false;
            return true;
        }
        if (object->ce == alignment_class) {
            const auto &alignment = cell_now(self).style.alignment;
            if (name == "horizontal" || name == "vertical") {
                const auto text = name == "horizontal" ? excel::name(alignment.horizontal) : excel::name(alignment.vertical);
                ZVAL_STRINGL(out, text.data(), text.size());
            } else if (name == "wrap") ZVAL_BOOL(out, alignment.wrap);
            else if (name == "indent") ZVAL_LONG(out, static_cast<zend_long>(alignment.indent));
            else if (name == "rotation") ZVAL_LONG(out, alignment.rotation);
            else if (name == "stacked") ZVAL_BOOL(out, alignment.stacked);
            else return false;
            return true;
        }
        if (object->ce == borders_class) {
            const auto side = std::ranges::find(sides, name);
            if (side == std::end(sides)) return false;
            make_part(out, edge_class, self.owner, *self.sheet, self.row, self.column, static_cast<std::size_t>(side - std::begin(sides)));
            return true;
        }
        if (object->ce == edge_class) {
            const auto &edge = cell_now(self).style.borders.*side_members[self.side];
            if (name == "line") zval_of_kind(edge.line, out);
            else if (name == "color") zval_of(edge.color, out);
            else return false;
            return true;
        }
        if (object->ce == font_class) {
            const auto &font = cell_now(self).style.font;
            if (name == "size") {
                if (font.size) ZVAL_DOUBLE(out, font.size->points());
                else ZVAL_NULL(out);
            } else if (name == "color") zval_of(font.color, out);
            else if (name == "bold") ZVAL_BOOL(out, font.bold);
            else if (name == "italic") ZVAL_BOOL(out, font.italic);
            else if (name == "strike") ZVAL_BOOL(out, font.strike);
            else if (name == "underline") zval_of_kind(font.underline, out);
            else return false;
            return true;
        }
        if (object->ce == row_class) {
            if (name != "index") return false;
            ZVAL_LONG(out, static_cast<zend_long>(self.row));
            return true;
        }
        const excel::address where{self.row, self.column};
        if (name == "row") ZVAL_LONG(out, static_cast<zend_long>(self.row));
        else if (name == "column") ZVAL_LONG(out, static_cast<zend_long>(self.column));
        else if (name == "name") {
            const auto text = where.name();
            ZVAL_STRINGL(out, text.data(), text.size());
        } else if (name == "value") {
            if (self.sheet->contains(where)) zval_of(self.sheet->at(where).value, out);
            else ZVAL_NULL(out);
        } else if (name == "formula") {
            const auto text = self.sheet->contains(where) ? self.sheet->at(where).formula : std::string{};
            ZVAL_STRINGL(out, text.data(), text.size());
        } else if (name == "style") {
            make_part(out, style_class, self.owner, *self.sheet, self.row, self.column);
        } else return false;
        return true;
    }

    std::span<const std::string_view> property_names(const zend_object *object) {
        static constexpr std::string_view sheet[] = {"name", "widths", "heights", "merged", "hiddenColumns", "hiddenRows", "frozen"}, row[] = {"index"},
                                          cell[] = {"row", "column", "name", "value", "formula", "style"},
                                          style[] = {"font", "background", "borders", "alignment", "numberFormat"},
                                          font[] = {"size", "color", "bold", "italic", "strike", "underline"}, edge[] = {"line", "color"},
                                          alignment[] = {"horizontal", "vertical", "wrap", "indent", "rotation", "stacked"};
        if (object->ce == sheet_class) return sheet;
        if (object->ce == row_class) return row;
        if (object->ce == style_class) return style;
        if (object->ce == font_class) return font;
        if (object->ce == borders_class) return sides;
        if (object->ce == edge_class) return edge;
        if (object->ce == alignment_class) return alignment;
        return cell;
    }

    zval *read_property(zend_object *object, zend_string *member, int, void **, zval *rv) {
        if (property(object, view(member), rv)) return rv;
        zend_throw_exception(sc::php::exception_class,
                             ("SC\\Excel: " + std::string{ZSTR_VAL(object->ce->name)} + " has no property '" + std::string{view(member)} + "'").c_str(), 0);
        return &EG(uninitialized_zval);
    }

    zval *write_property(zend_object *object, zend_string *member, zval *value, void **) {
        const auto name = view(member);
        sc::php::guarded([&] {
            auto &self = *part_of(object);
            const auto refuse = [&](const char *settable) {
                return std::logic_error{"SC\\Excel: " + std::string{ZSTR_VAL(object->ce->name)} + "'s '" + std::string{name} + "' can't be set" + settable};
            };
            if (!self.sheet) throw refuse("");
            if (object->ce == cell_class) {
                if (name == "value") cell_to_change(self) = value_of(value);
                else if (name == "formula") cell_to_change(self).formula = sc::php::text_of(value);
                else if (name == "style") cell_to_change(self).style = style_of(value);
                else throw refuse(" (a cell's value, formula and style can)");
            } else if (object->ce == style_class) {
                if (name == "font") cell_to_change(self).style.font = font_of(value);
                else if (name == "background") cell_to_change(self).style.background = color_of(value);
                else if (name == "borders") cell_to_change(self).style.borders = borders_of(value);
                else if (name == "alignment") cell_to_change(self).style.alignment = alignment_of(value);
                else if (name == "numberFormat") cell_to_change(self).style.number_format = number_format_of(value);
                else throw refuse(" (a style's font, background, borders, alignment and numberFormat can)");
            } else if (object->ce == font_class) {
                if (name == "size") cell_to_change(self).style.font.size = size_of(value);
                else if (name == "color") cell_to_change(self).style.font.color = color_of(value);
                else if (name == "bold") cell_to_change(self).style.font.bold = zend_is_true(value);
                else if (name == "italic") cell_to_change(self).style.font.italic = zend_is_true(value);
                else if (name == "strike") cell_to_change(self).style.font.strike = zend_is_true(value);
                else if (name == "underline") cell_to_change(self).style.font.underline = underline_of(value);
                else throw refuse(" (a font's size, color, bold, italic, strike and underline can)");
            } else if (object->ce == borders_class) {
                const auto side = std::ranges::find(sides, name);
                if (side == std::end(sides)) throw refuse(" (borders' left, right, top, bottom, diagonalUp and diagonalDown can)");
                cell_to_change(self).style.borders.*side_members[side - std::begin(sides)] = edge_of(value);
            } else if (object->ce == edge_class) {
                auto &edge = cell_to_change(self).style.borders.*side_members[self.side];
                if (name == "line") edge.line = line_of(value);
                else if (name == "color") edge.color = color_of(value);
                else throw refuse(" (an edge's line and color can)");
            } else if (object->ce == alignment_class) {
                auto &alignment = cell_to_change(self).style.alignment;
                if (name == "horizontal") alignment.horizontal = horizontal_of(value);
                else if (name == "vertical") alignment.vertical = vertical_of(value);
                else if (name == "wrap") alignment.wrap = zend_is_true(value);
                else if (name == "indent") alignment.indent = indent_of(value);
                else if (name == "rotation") alignment.rotation = rotation_of(value);
                else if (name == "stacked") alignment.stacked = zend_is_true(value);
                else throw refuse(" (an alignment's horizontal, vertical, wrap, indent, rotation and stacked can)");
            } else {
                throw refuse("");
            }
        });
        return value;
    }

    // isset() is false for a null value and an empty formula, as for an array's.
    int has_property(zend_object *object, zend_string *member, int has_set_exists, void **) {
        zval found;
        if (!property(object, view(member), &found)) return 0;
        int answer = 1;
        if (has_set_exists == ZEND_PROPERTY_ISSET) answer = Z_TYPE(found) != IS_NULL;
        else if (has_set_exists == ZEND_PROPERTY_NOT_EMPTY) answer = zend_is_true(&found);
        zval_ptr_dtor(&found);
        return answer;
    }

    void unset_property(zend_object *object, zend_string *member, void **) {
        zend_throw_exception(sc::php::exception_class,
                             ("SC\\Excel: " + std::string{ZSTR_VAL(object->ce->name)} + "'s '" + std::string{view(member)} + "' can't be unset").c_str(), 0);
    }

    // No direct pointers: reads and writes go through the handlers above.
    zval *property_pointer(zend_object *, zend_string *, int, void **) { return nullptr; }

    // var_dump(), (array), json_encode(), var_export(): the properties as they are now.
    zend_array *properties_for(zend_object *object, zend_prop_purpose) {
        zend_array *out = zend_new_array(5);
        for (const auto name: property_names(object)) {
            zval value;
            if (property(object, name, &value)) zend_hash_str_update(out, name.data(), name.size(), &value);
        }
        return out;
    }

    // ---- SC\Csv ----

    // A Csv method's options: delimiter (one character; "" finds it), typed, bom.
    sc::office::csv::options csv_options(HashTable *given) {
        sc::office::csv::options how;
        if (!given) return how;
        zend_string *key = nullptr;
        zval *value = nullptr;
        ZEND_HASH_FOREACH_STR_KEY_VAL(given, key, value) {
            const std::string_view name = key ? view(key) : std::string_view{};
            if (name == "delimiter") {
                const auto text = sc::php::text_of(value);
                if (text.size() > 1) throw std::invalid_argument{"SC\\Csv: the delimiter is one character, or \"\" to find it"};
                how.delimiter = text.empty() ? '\0' : text[0];
            } else if (name == "typed") {
                how.typed = zend_is_true(value);
            } else if (name == "bom") {
                how.byte_order_mark = zend_is_true(value);
            } else {
                throw std::invalid_argument{"SC\\Csv: unknown option '" + std::string{key ? name : "(a number)"} + "' (delimiter, typed, bom)"};
            }
        } ZEND_HASH_FOREACH_END();
        return how;
    }

    // values as a new workbook's one sheet (named name, or "Sheet1" when Excel won't take it), into out.
    void sheet_of(zval *out, const sc::office::csv::sheet &values, const std::string &name) {
        zval book;
        object_init_ex(&book, workbook_class);
        excel::sheet *sheet = nullptr;
        sc::php::guarded([&] {
            auto &into = *workbook_of(Z_OBJ(book))->book;
            try {
                sheet = &into.add(name.empty() ? std::string{"Sheet1"} : name);
            } catch (const std::invalid_argument &) {
                sheet = &into.add("Sheet1");
            }
            for (const auto &row: values)
                for (const auto &cell: row) (*sheet)[{cell.row, cell.column}] = cell.value;
        });
        if (!EG(exception)) make_part(out, sheet_class, Z_OBJ(book), *sheet);
        zval_ptr_dtor(&book);
    }

    // The Sheet given, or an SC\Exception.
    const excel::sheet *sheet_given(zval *given) {
        const auto *from = given_part(given, sheet_class);
        if (!from) zend_throw_exception(sc::php::exception_class, "SC\\Csv: give a sheet from a workbook, or from SC\\Csv::open()", 0);
        return from ? from->sheet : nullptr;
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_csv_open, 0, 1, SC\\Excel\\Sheet, 0)
        ZEND_ARG_TYPE_INFO(0, filename, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(csv_open) {
        zend_string *filename = nullptr;
        HashTable *given = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_STR(filename)
            Z_PARAM_OPTIONAL
            Z_PARAM_ARRAY_HT(given)
        ZEND_PARSE_PARAMETERS_END();
        sc::office::csv::sheet values;
        sc::php::guarded([&] { values = sc::office::csv::read(std::string{view(filename)}, csv_options(given)); });
        if (EG(exception)) RETURN_THROWS();
        sheet_of(return_value, values, values.name());
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_csv_from_string, 0, 1, SC\\Excel\\Sheet, 0)
        ZEND_ARG_TYPE_INFO(0, text, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(csv_from_string) {
        zend_string *text = nullptr;
        HashTable *given = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_STR(text)
            Z_PARAM_OPTIONAL
            Z_PARAM_ARRAY_HT(given)
        ZEND_PARSE_PARAMETERS_END();
        sc::office::csv::sheet values;
        sc::php::guarded([&] { values = sc::office::csv::read_from(view(text), csv_options(given)); });
        if (EG(exception)) RETURN_THROWS();
        sheet_of(return_value, values, "Sheet1");
    }

    ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_csv_create, 0, 0, SC\\Excel\\Sheet, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, name, IS_STRING, 0, "\"Sheet1\"")
    ZEND_END_ARG_INFO()

    // An empty sheet, to fill and save as CSV.
    ZEND_NAMED_FUNCTION(csv_create) {
        zend_string *name = nullptr;
        ZEND_PARSE_PARAMETERS_START(0, 1)
            Z_PARAM_OPTIONAL
            Z_PARAM_STR(name)
        ZEND_PARSE_PARAMETERS_END();
        sheet_of(return_value, sc::office::csv::sheet{}, name ? std::string{view(name)} : std::string{"Sheet1"});
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_csv_save, 0, 2, IS_VOID, 0)
        ZEND_ARG_OBJ_INFO(0, sheet, SC\\Excel\\Sheet, 0)
        ZEND_ARG_TYPE_INFO(0, filename, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
    ZEND_END_ARG_INFO()

    ZEND_NAMED_FUNCTION(csv_save) {
        zval *sheet = nullptr;
        zend_string *filename = nullptr;
        HashTable *given = nullptr;
        ZEND_PARSE_PARAMETERS_START(2, 3)
            Z_PARAM_OBJECT_OF_CLASS(sheet, sheet_class)
            Z_PARAM_STR(filename)
            Z_PARAM_OPTIONAL
            Z_PARAM_ARRAY_HT(given)
        ZEND_PARSE_PARAMETERS_END();
        const auto *values = sheet_given(sheet);
        if (!values) RETURN_THROWS();
        sc::php::guarded([&] { sc::office::csv::write(*values, std::string{view(filename)}, csv_options(given)); });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_csv_output, 0, 1, IS_STRING, 0)
        ZEND_ARG_OBJ_INFO(0, sheet, SC\\Excel\\Sheet, 0)
        ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
    ZEND_END_ARG_INFO()

    // The CSV as a string: what save() writes.
    ZEND_NAMED_FUNCTION(csv_output) {
        zval *sheet = nullptr;
        HashTable *given = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_OBJECT_OF_CLASS(sheet, sheet_class)
            Z_PARAM_OPTIONAL
            Z_PARAM_ARRAY_HT(given)
        ZEND_PARSE_PARAMETERS_END();
        const auto *values = sheet_given(sheet);
        if (!values) RETURN_THROWS();
        sc::php::return_guarded_string(return_value, [&] { return sc::office::csv::bytes(*values, csv_options(given)); });
    }

    ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_csv_delimiter, 0, 1, IS_STRING, 0)
        ZEND_ARG_TYPE_INFO(0, text, IS_STRING, 0)
    ZEND_END_ARG_INFO()

    // The delimiter open() would find in text: "," or ";".
    ZEND_NAMED_FUNCTION(csv_delimiter) {
        zend_string *text = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_STR(text)
        ZEND_PARSE_PARAMETERS_END();
        const char found = sc::office::csv::find_delimiter(view(text));
        RETURN_STRINGL(&found, 1);
    }

    const zend_function_entry csv_methods[] = {
        ZEND_NAMED_ME(open, csv_open, arginfo_csv_open, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(fromString, csv_from_string, arginfo_csv_from_string, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(create, csv_create, arginfo_csv_create, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(save, csv_save, arginfo_csv_save, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(output, csv_output, arginfo_csv_output, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_NAMED_ME(delimiter, csv_delimiter, arginfo_csv_delimiter, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
        ZEND_FE_END
    };

    zend_class_entry *register_class(const char *name, const zend_function_entry *methods, const bool container) {
        zend_class_entry type;
        INIT_CLASS_ENTRY_EX(type, name, std::strlen(name), methods);
        zend_class_entry *registered = zend_register_internal_class(&type);
        registered->ce_flags |= ZEND_ACC_FINAL;
        if (container) zend_class_implements(registered, 3, zend_ce_arrayaccess, zend_ce_aggregate, zend_ce_countable);
        return registered;
    }

    void startup() {
        workbook_class = register_class("SC\\Excel\\Workbook", workbook_methods, true);
        workbook_class->create_object = create_workbook;
        std::memcpy(&workbook_handlers, zend_get_std_object_handlers(), sizeof(workbook_handlers));
        workbook_handlers.offset = XtOffsetOf(workbook_object, std);
        workbook_handlers.free_obj = free_workbook;
        workbook_handlers.clone_obj = clone_workbook;

        sheet_class = register_class("SC\\Excel\\Sheet", sheet_methods, true);
        row_class = register_class("SC\\Excel\\Row", row_methods, true);
        cell_class = register_class("SC\\Excel\\Cell", cell_methods, false);
        style_class = register_class("SC\\Excel\\Style", cell_methods, false);
        font_class = register_class("SC\\Excel\\Font", cell_methods, false);
        borders_class = register_class("SC\\Excel\\Borders", cell_methods, false);
        edge_class = register_class("SC\\Excel\\Edge", cell_methods, false);
        alignment_class = register_class("SC\\Excel\\Alignment", cell_methods, false);
        register_class("SC\\Csv", csv_methods, false);
        for (auto *each: {sheet_class, row_class, cell_class, style_class, font_class, borders_class, edge_class, alignment_class})
            each->create_object = create_part;
        std::memcpy(&part_handlers, zend_get_std_object_handlers(), sizeof(part_handlers));
        part_handlers.offset = XtOffsetOf(part_object, std);
        part_handlers.free_obj = free_part;
        part_handlers.clone_obj = nullptr; // a view into a workbook: clone the workbook instead
        part_handlers.read_property = read_property;
        part_handlers.write_property = write_property;
        part_handlers.has_property = has_property;
        part_handlers.unset_property = unset_property;
        part_handlers.get_property_ptr_ptr = property_pointer;
        part_handlers.get_properties_for = properties_for;
    }
}

sc::php::area sc::php::excel_area() { return {nullptr, startup}; }
