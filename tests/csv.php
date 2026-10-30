<?php
// SC\Csv: CSV read into an SC\Excel\Sheet and any sheet written as CSV, quoting, the delimiter found
// or given, typed values, options, and the same code on a CSV sheet and an Excel one.
require __DIR__ . '/check.inc';

$sheet = SC\Csv::fromString("Item,Price\r\nBread,12.5\r\n\"Milk, full cream\",20\r\n\"He said \"\"hi\"\"\",TRUE\r\n");
check($sheet instanceof SC\Excel\Sheet, 'a CSV is an SC\Excel\Sheet');
check($sheet['A3']->value === 'Milk, full cream', 'a quoted delimiter');
check($sheet['A4']->value === 'He said "hi"', 'a doubled quote');
check($sheet['B2']->value === 12.5 && $sheet['B4']->value === true, 'typed: numbers and TRUE');
check(count($sheet) === 4, 'four rows');

// The same code on a CSV sheet and an Excel one.
$total = function (SC\Excel\Sheet $sheet): float {
    $sum = 0.0;
    foreach ($sheet as $row) foreach ($row as $cell) if ($cell->column === 1 && is_float($cell->value)) $sum += $cell->value;
    return $sum;
};
$book = new SC\Excel\Workbook();
$book['Prices']['A1'] = 'Item';
$book['Prices']['B2'] = 12.5;
$book['Prices']['B3'] = 20;
$book['Prices']['A1']->style->font->bold = true;
check($total($sheet) === 32.5 && $total($book['Prices']) === 32.5, 'the same code on both');

// The delimiter: found, or given; typed off.
check(SC\Csv::delimiter("Name;Price\nBread;12,50\n") === ';', 'semicolons found');
check(SC\Csv::delimiter("a,b\n1,2\n") === ',', 'commas found');
$european = SC\Csv::fromString("Name;Price\nBread;12,50\n");
check($european['B2']->value === '12,50', 'a decimal comma stays text');
$text = SC\Csv::fromString("007,12.5\n", ['typed' => false]);
check($text['A1']->value === '007' && $text['B1']->value === '12.5', 'typed false: all text');
check(throws(fn() => SC\Csv::fromString('a', ['delim' => ';'])), 'an unknown option throws');
check(throws(fn() => SC\Csv::fromString('a', ['delimiter' => ';;'])), 'a delimiter is one character');
check(throws(fn() => SC\Csv::fromString("\"open,b\n")), 'an unclosed quote throws');

// Writing: an Excel sheet's values, a created sheet, a file read back by name.
check(SC\Csv::output($book['Prices']) === "Item,\r\n,12.5\r\n,20\r\n", 'an Excel sheet written as CSV');
check(SC\Csv::output($book['Prices'], ['delimiter' => ';', 'bom' => true]) === "\xEF\xBB\xBFItem;\r\n;12.5\r\n;20\r\n", 'with ; and a BOM');
$out = SC\Csv::create();
$out['A1'] = 'code';
$out['A2'] = '007';
$out['B2'] = '12';                     // text that looks like a number stays text
$file = sys_get_temp_dir() . '/sc-phplib-prices.csv';
SC\Csv::save($out, $file);
check(file_get_contents($file) === "code,\r\n007,\"12\"\r\n", 'saved, text kept text');
$back = SC\Csv::open($file);
check($back['A2']->value === '007' && $back['B2']->value === '12', 'read back the same');
unlink($file);
check(throws(fn() => SC\Csv::open('/no/such/file.csv')), 'a missing file throws');
