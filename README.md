# simply-cpp-phplib

The `sc` PHP extension: simply-cpp's libraries for PHP, in the `SC` namespace. It's a thin
wrapper: the work is done by the modules' shared libraries (sc-html2pdf, sc-pdflite, sc-office, sc-image, sc-core),
which its package depends on. Apache-2.0, as everything it links is (no AGPL modules).

```php
$pdf = new SC\Html2Pdf($html, [
    'header'   => '<p>#section# - page #Page# of #ToPage#</p>',
    'footer'   => '<p>Confidential</p>',
    'size'     => 'A4',                 // A3, A4, A5, letter, legal, in any case
    'landscape'=> false,
    'margins'  => ['top' => 10, 'right' => 10, 'bottom' => 10, 'left' => 30, 'first_top' => 60], // mm; first_top null: as top
    'css'      => 'body { font-family: Roboto, sans-serif }',
    'base_url' => 'https://www.example.com/',
    'toc_page' => 2,
    'metadata' => ['Title' => 'Invoice', 'Author' => 'Accounts'],
    'user_password' => 'open',          // and 'owner_password'
    'builtin_fonts' => true,            // PDF's built-in Helvetica, Times, Courier: nothing embedded
    'image_resolution' => 150,          // PNGs scaled down to this many pixels an inch (the default; 0 keeps them)
]);
$pdf->pageCount();
$pdf->save('/tmp/invoice.pdf');
header('Content-Type: application/pdf');
echo $pdf->output();                    // the PDF as a string

// Existing PDFs written again (sc-pdflite): merged, decorated, protected; any PDF, encrypted or damaged
SC\Pdf::merge(['/tmp/invoice.pdf', $appendix, 'photo.jpg'], '/tmp/invoice.pdf');  // into the first one; a JPEG or PNG a page
SC\Pdf::decorate('/tmp/invoice.pdf', $letterhead, '/tmp/invoice.pdf'); // its pages behind; 'over' for a stamp
SC\Pdf::protect('/tmp/invoice.pdf', '/tmp/invoice.pdf', ['owner_password' => $secret,
                 'permissions' => ['annotate' => true]]);          // print and annotate, nothing else
SC\Pdf::pageCount('/tmp/invoice.pdf');

// Excel workbooks (sc-office): sheets of cells with their values and formulas, read and written
$book = SC\Excel\Workbook::open('prices.xlsx');        // or ::fromString($bytes)
foreach ($book as $name => $sheet)                       // the sheets, by name
    foreach ($sheet as $r => $row)                       // the rows that have cells (from 0)
        foreach ($row as $c => $cell)                    // the cells that are set
            echo $cell->name, ' ', $cell->value, ' ', $cell->formula;   // B3 18.5 =B2*2
if (isset($book['Prices']['B2'])) $price = $book['Prices']['B2']->value; // or [[1, 1]]
$out = new SC\Excel\Workbook();
$out['Prices']['A1'] = 'Item';          // the sheet added, as with an array
$out['Prices']['B2'] = 18.5;            // null, a number, text or a bool
$out['Prices']['B3']->formula = '=B2*2';
unset($out['Prices']['A1']);
$out->save('/tmp/prices.xlsx');         // or $out->output(), the .xlsx as a string

$png = SC\svg_to_png($svg, 800);        // 800 wide; width and height 0 (the default): its own size
SC\base64_encode($bytes);               // SC\base64_decode() throws for text that isn't base64
```

The options are [sc::html2pdf's](https://github.com/roelofrossouw/simply-cpp-html2pdf) by name
(`media`, `toc_title`, `outline`, `split_boxes` and `draw_margins` too); one it doesn't know
throws, so a typo shows. Every failure is an `SC\Exception` (an `Exception`).

`SC\Excel\Workbook`, `Sheet` and `Row` are `ArrayAccess`, `IteratorAggregate` and `Countable`, like
sc-office's C++ classes: sheets are found by name ignoring case, cells by name (`"B3"`) or
`[row, column]` from 0, a row's cells by column number or letters (`$row['B']`). A `Cell` has
`row`, `column`, `name`, `value` (numbers are floats; dates are numbers, days since 1899-12-30) and
`formula` (`"=C6"`). Reading a cell that isn't there gives a null value and adds nothing; setting
its value or formula adds it. Sheets, rows and cells are views into their workbook, which they
keep alive: changing one changes the workbook (`clone $book` makes a separate one). Sheets can't
be removed; cells can (`unset()`).

**Documentation:** the [simply-cpp wiki](https://github.com/roelofrossouw/simply-cpp/wiki)'s
[HTML to PDF guide](https://github.com/roelofrossouw/simply-cpp/wiki/HTML-to-PDF) covers what the
HTML can do.

**IDEs:** `stubs/sc.stub.php`, installed as `/usr/share/doc/simply-cpp-phplib/sc.stub.php`,
describes all of it with types and documentation. Add that folder to PhpStorm's include path
(Settings > PHP > Include Path) or to Intelephense's `intelephense.environment.includePaths`, and
`SC\Html2Pdf`, `SC\Excel\Workbook`, their options and the functions autocomplete. A test checks the stubs against the
extension.

## Install

```bash
curl -fsSL https://apt.roelof.co.za/setup.sh | sudo bash
sudo apt install simply-cpp-phplib
```

The package is built for the PHP its Ubuntu release ships (jammy 8.1, noble 8.3, resolute 8.5)
and depends on that PHP's module API. Installing puts `sc.so` in PHP's extension directory and
`sc.ini` in `/etc/php/<version>/mods-available`, enables it for every SAPI (`phpenmod -s ALL sc`)
and restarts php-fpm when it runs; upgrades restart it again, and removal disables the extension
and restarts it.

## Adding to it

Each part is one file in `src/` (an "area"): its functions as a `zend_function_entry` table and,
for classes, a `startup()` that registers them, written against PHP's own C API with the helpers
in `src/php_glue.h` (`guarded()` turns C++ exceptions into `SC\Exception`). `src/sc_php.cpp`
lists the areas, so a new one is a file and a line there, plus its module's shared library in
`CMakeLists.txt`. Tests are `tests/*.php`, each run by ctest with only this extension loaded.

## Building

`bash scripts/install.sh` builds, tests and packages it (on Ubuntu it installs `php-dev` and
`php-cli` when missing, and installs the extension; on macOS it builds against Homebrew's PHP
and doesn't install). `-DSC_PHPLIB_PHP_CONFIG=/path/to/php-config` builds for another PHP.
