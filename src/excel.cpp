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
//     $out->save('out.xlsx');                 // or $out->output(), the file as a string
//
// A Sheet, Row or Cell is a view into its workbook (which it keeps alive), not a copy: changing it
// changes the workbook. Every failure is an SC\Exception.

#include "php_glue.h"

#include <excel.h>

#include <ext/spl/spl_array.h>
#include <zend_interfaces.h>

#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

namespace {
    namespace excel = sc::office::excel;
    using sc::php::view;

    zend_class_entry *workbook_class = nullptr, *sheet_class = nullptr, *row_class = nullptr, *cell_class = nullptr;
    zend_object_handlers workbook_handlers, part_handlers;

    // ---- the objects ----

    struct workbook_object {
        excel::workbook *book;
        zend_object std;
    };

    // A Sheet, Row or Cell: its workbook's object (kept alive while this one lives), the sheet, and
    // the row and column it stands for (as far as it has them).
    struct part_object {
        zend_object *owner;
        excel::sheet *sheet;
        std::size_t row;
        std::size_t column;
        zend_object std;
    };

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
        self->row = self->column = 0;
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
                   const std::size_t column = 0) {
        object_init_ex(out, type);
        auto *self = part_of(Z_OBJ_P(out));
        GC_ADDREF(owner);
        self->owner = owner;
        self->sheet = &sheet;
        self->row = row;
        self->column = column;
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

    // A cell given to a sheet or row: its value and formula, from another cell or a value.
    void assign(excel::cell &cell, zval *given) {
        ZVAL_DEREF(given);
        if (Z_TYPE_P(given) == IS_OBJECT && Z_OBJCE_P(given) == cell_class) {
            const auto &from = *part_of(Z_OBJ_P(given));
            if (from.sheet && from.sheet->contains({from.row, from.column})) {
                const auto &source = from.sheet->at({from.row, from.column});
                cell.value = source.value;
                cell.formula = source.formula;
            } else {
                cell.value = {};
                cell.formula.clear();
            }
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

    const zend_function_entry sheet_methods[] = {
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

    const zend_function_entry cell_methods[] = {
        ZEND_NAMED_ME(__construct, part_construct, arginfo_none, ZEND_ACC_PRIVATE)
        ZEND_FE_END
    };

    // ---- properties: Sheet's name, Row's index, Cell's row, column, name, value and formula ----

    // The property's value into out; false when the object has no such property.
    bool property(zend_object *object, const std::string_view name, zval *out) {
        auto &self = *part_of(object);
        if (!self.sheet) return false;
        if (object->ce == sheet_class) {
            if (name != "name") return false;
            ZVAL_STRINGL(out, self.sheet->name().data(), self.sheet->name().size());
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
        } else return false;
        return true;
    }

    std::span<const std::string_view> property_names(const zend_object *object) {
        static constexpr std::string_view sheet[] = {"name"}, row[] = {"index"}, cell[] = {"row", "column", "name", "value", "formula"};
        if (object->ce == sheet_class) return sheet;
        if (object->ce == row_class) return row;
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
            if (object->ce != cell_class || !self.sheet || (name != "value" && name != "formula"))
                throw std::logic_error{"SC\\Excel: " + std::string{ZSTR_VAL(object->ce->name)} + "'s '" + std::string{name} +
                                       "' can't be set" + (object->ce == cell_class ? " (a cell's value and formula can)" : "")};
            auto &cell = (*self.sheet)[{self.row, self.column}];
            if (name == "value") cell = value_of(value);
            else cell.formula = sc::php::text_of(value);
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
        for (auto *each: {sheet_class, row_class, cell_class}) each->create_object = create_part;
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
