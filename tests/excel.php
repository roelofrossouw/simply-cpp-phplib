<?php
// SC\Excel: workbooks, sheets, rows and cells as containers; values and formulas written and read
// back; views into a workbook; mistakes.
require __DIR__ . '/check.inc';

use SC\Excel\Workbook;

$dir = sys_get_temp_dir() . '/sc-phplib-excel-' . getmypid();
@mkdir($dir);

// Writing: the sheet added by $book[$name], cells by name or [row, column].
$book = new Workbook();
check(count($book) === 0, 'a new workbook has no sheets');
$prices = $book['Prices'];
check(count($book) === 1 && $prices->name === 'Prices', '$book[$name] adds the sheet');
check($book['prices']->name === 'Prices', 'sheets are found ignoring case, as Excel does');
$prices['A1'] = 'Item';
$prices['B1'] = 'Price';
$prices['A2'] = 'Bread';
$prices['B2'] = 18.5;
$prices[[2, 0]] = 'Milk';               // A3
$prices[[2, 1]] = 21;                   // an int: a number
$prices['C2'] = true;
$prices['A4'] = 'Total & <sum>';
$prices['B4']->formula = '=SUM(B2:B3)'; // a cell that isn't there yet, added by setting it
$prices['D9'] = null;                   // an empty cell
$book->add('Notes')['A1'] = "line\nbreak, ünïcödé";
check(throws(fn() => $book->add('NOTES')), 'add() of a name in use throws');
check(throws(fn() => $book['a/b']), "a name Excel doesn't take throws");

check(isset($prices['B2']) && !isset($prices['C9']), 'isset() on cells');
check(count($prices) === 5, 'five rows have cells (D9 holds nothing, but is there)');
check($prices['B2']->value === 18.5 && $prices['B3']->value === 21.0, 'numbers are floats');
check($prices['C9']->value === null && $prices['C9']->formula === '', "a cell that isn't there is empty");
check(!isset($prices['C9']), '... and reading it adds nothing');

$file = "$dir/prices.xlsx";
$book->save($file);
$bytes = $book->output();
check(str_starts_with($bytes, 'PK'), 'output() gives the .xlsx');

// Reading back: from the file and from the string.
foreach ([Workbook::open($file), Workbook::fromString($bytes)] as $back) {
    check(count($back) === 2, 'two sheets');
    $names = [];
    foreach ($back as $name => $sheet) $names[] = $name . '=' . $sheet->name;
    check($names === ['Prices=Prices', 'Notes=Notes'], 'sheets in order, keyed by name');
    $cells = [];
    foreach ($back['Prices'] as $r => $row) {
        check($row->index === $r, 'a row knows its index');
        foreach ($row as $c => $cell) {
            check($cell->row === $r && $cell->column === $c, 'a cell knows its place');
            $cells[$cell->name] = [$cell->value, $cell->formula];
        }
    }
    check($cells === ['A1' => ['Item', ''], 'B1' => ['Price', ''], 'A2' => ['Bread', ''], 'B2' => [18.5, ''], 'C2' => [true, ''],
                      'A3' => ['Milk', ''], 'B3' => [21.0, ''], 'A4' => ['Total & <sum>', ''], 'B4' => [null, '=SUM(B2:B3)']],
          'only the cells that are set, in order, with values and formulas (' . json_encode($cells) . ')');
    check($back['Notes']['A1']->value === "line\nbreak, ünïcödé", 'text as it was');
}

// A row: cells by column number or letters.
$back = Workbook::open($file);
foreach ($back['Prices'] as $row) {
    if ($row->index !== 1) continue;
    check(count($row) === 3, 'row 2 has three cells');
    check($row['B']->value === 18.5 && $row[1]->value === 18.5, 'a row: by letters or number');
    $row['D'] = 'new';
    check($back['Prices']['D2']->value === 'new', "setting a row's cell sets the sheet's");
    unset($row[3]);
    check(!isset($back['Prices']['D2']), "unset() removes a row's cell");
}

// Views into the workbook: changing a cell changes the workbook, and they keep it alive.
$cell = (function () use ($file) {
    $book = Workbook::open($file);
    return $book['Prices']['A2'];
})();
check($cell->value === 'Bread', 'a cell keeps its workbook alive');
$sheet = $back['Prices'];
$sheet['A2']->value = 'Rye';
check($back['Prices']['A2']->value === 'Rye', 'setting through a view changes the workbook');
$copy = clone $back;
$copy['Prices']['A2'] = 'Copied';
check($back['Prices']['A2']->value === 'Rye', 'a clone is a workbook of its own');
$sheet['E1'] = $sheet['B4'];
check($back['Prices']['E1']->formula === '=SUM(B2:B3)', 'a cell assigned copies the value and formula');
unset($sheet['E1']);
check(!isset($sheet['E1']), 'unset() removes a cell');
check(json_decode(json_encode($sheet['B2']), true) === ['row' => 1, 'column' => 1, 'name' => 'B2', 'value' => 18.5, 'formula' => '',
      'style' => ['font' => ['size' => null, 'color' => null, 'bold' => false], 'background' => null]],
      'json_encode() shows a cell');

// Styles: a part at a time, as an array, or another cell's; column widths.
$styled = new Workbook();
$s = $styled['Styled'];
$s['A1'] = 'Header';
$s['A1']->style->font->bold = true;
$s['A1']->style->font->size = 14;
$s['A1']->style->font->color = '#1F4E79';
$s['A1']->style->background = 'yellow';
check($s['A1']->style->font->bold && $s['A1']->style->font->size === 14.0, 'a font set a part at a time');
check($s['A1']->style->font->color === '#1f4e79' && $s['A1']->style->background === '#ffff00', 'colours read back as #rrggbb');
$s['B1']->style = ['font' => ['bold' => true, 'color' => 'red'], 'background' => '#ddebf7'];
check(isset($s['B1']) && $s['B1']->value === null, 'a style alone adds the cell');
check($s['B1']->style->font->size === null && $s['B1']->style->font->color === '#ff0000', 'what an array leaves out is the default');
$s['C1']->style = $s['A1']->style;
check($s['C1']->style->font->size === 14.0, "another cell's style");
$s['C1']->style->font = ['size' => 9];
check($s['C1']->style->font->size === 9.0 && !$s['C1']->style->font->bold && $s['C1']->style->background === '#ffff00', 'the font replaced, the background kept');
$s['A2'] = 5;
$s['A2']->style = $s['A1']->style;
$s['A2'] = 6;
check($s['A2']->style->font->bold, 'setting the value keeps the style');
$s['D1'] = $s['A1'];
check($s['D1']->value === 'Header' && $s['D1']->style->font->bold, "a cell assigned copies its style");
$s['D1']->style = null;
check(!$s['D1']->style->font->bold && $s['D1']->style->background === null, 'null: the plain style');
$font = $s['A1']->style->font;
$font->bold = false;
check(!$s['A1']->style->font->bold, 'a Font kept is still a view into its cell');
$font->bold = true;
check(throws(function () use ($s) { $s['A1']->style->font->size = 500; }), 'a font size beyond 409 throws');
check(throws(function () use ($s) { $s['A1']->style->font->color = 'reddish'; }), "text that isn't a colour throws");
check(throws(function () use ($s) { $s['A1']->style = ['colour' => 'red']; }), 'a key a style lacks throws');
check(throws(function () use ($s) { $s['A1']->style = 'bold'; }), 'a style that is text throws');
check(throws(function () use ($s) { $s['A1']->style->font->italic = true; }), 'a property fonts lack throws');
check($s->width('A') === null, 'a column without a width: null');
check($s->width('A', 30) === 30.0 && $s->width(0) === 30.0, 'width() sets and reads, by letters or number');
$s->width('B', 12.5);
$s->width('C', 12.5);
$s->width('E', 4);
$s->width('E', null);
check($s->widths === [0 => 30.0, 1 => 12.5, 2 => 12.5], 'widths by column number');
check(throws(fn() => $s->width('A', 256)), 'a width beyond 255 throws');
check(throws(fn() => $s->width('A1', 2)), "a column that isn't one throws");

$again = Workbook::fromString($styled->output());
$t = $again['Styled'];
check($t['A1']->style->font->bold && $t['A1']->style->font->size === 14.0 && $t['A1']->style->font->color === '#1f4e79', 'the font read back');
check($t['A1']->style->background === '#ffff00' && $t['B1']->style->background === '#ddebf7', 'backgrounds read back');
check(isset($t['B1']), 'a cell with a style and no value read back');
check($t['A2']->style->font->bold && $t['A2']->value === 6.0, 'value and style read back');
check($t->widths === [0 => 30.0, 1 => 12.5, 2 => 12.5], 'widths read back');

// Mistakes.
check(throws(fn() => Workbook::open("$dir/missing.xlsx")), 'a missing file throws');
check(throws(fn() => Workbook::open(__FILE__)), "a file that isn't an .xlsx throws");
check(throws(fn() => Workbook::fromString('not a workbook')), "bytes that aren't an .xlsx throw");
check(throws(fn() => (new Workbook())->save("$dir/never.xlsx")), 'a workbook without sheets throws');
check(!file_exists("$dir/never.xlsx"), 'and writes nothing');
check(throws(fn() => $sheet['3B']), "a cell name that isn't one throws");
check(throws(fn() => $sheet[[1]]), 'an address of one number throws');
check(throws(function () use ($sheet) { $sheet['A1'] = [1, 2]; }), 'an array as a value throws');
check(throws(function () use ($sheet) { $sheet[] = 1; }), '$sheet[] = ... throws: which cell?');
check(throws(function () use ($sheet) { $sheet['A1']->row = 5; }), "a cell's row can't be set");
check(throws(function () use ($sheet) { $sheet->name = 'Other'; }), "a sheet's name can't be set");
check(throws(fn() => $sheet['A1']->colour), 'a property cells lack throws');
check(throws(function () use ($back) { unset($back['Prices']); }), "sheets can't be removed");
check(throws(function () use ($back, $sheet) { $back['Other'] = $sheet; }), "a sheet isn't set, it's added");
check(throws(fn() => new SC\Excel\Cell(), Error::class), 'cells come from a workbook');

array_map('unlink', glob("$dir/*"));
rmdir($dir);
