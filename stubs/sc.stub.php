<?php
/**
 * Stubs for the sc PHP extension (apt: simply-cpp-phplib), for IDEs and static analysers: what it
 * declares, with types and documentation. Not for including: the extension provides all of it.
 *
 * PhpStorm: Settings > PHP > Include Path, add /usr/share/doc/simply-cpp-phplib (or a copy of this
 * file in your project). VS Code (Intelephense): add the folder to "intelephense.environment.includePaths".
 *
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/PHP
 */

namespace SC;

/**
 * What every failure in the extension throws: a mistake in the arguments or options, HTML or SVG
 * that can't be read, a file that can't be written.
 */
class Exception extends \Exception
{
}

/**
 * HTML as a PDF of pages: the content flows from page to page under a header and over a footer,
 * cut between lines of text, images and boxes, never through them.
 *
 * In the header and footer #Page#, #ToPage#, #section#, #subsection# and #subtitle# are filled in
 * per page; a header's #P1Head shows on the first page only and #POHead on the others. In the
 * content <section> starts a new page and an entry in the table of contents.
 *
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/HTML-to-PDF
 */
class Html2Pdf
{
    /**
     * Lays out the HTML and cuts it into pages. An option it doesn't know throws, so a typo shows.
     *
     * @param string $html The page, or a fragment of one.
     * @param array{
     *     size?: 'A3'|'A4'|'A5'|'letter'|'legal',
     *     landscape?: bool,
     *     margins?: array{top?: float, right?: float, bottom?: float, left?: float, first_top?: float|null},
     *     header?: string,
     *     footer?: string,
     *     css?: string,
     *     base_url?: string,
     *     media?: 'print'|'screen',
     *     toc_page?: int,
     *     toc_title?: string,
     *     outline?: bool,
     *     metadata?: array<string, string>,
     *     user_password?: string,
     *     owner_password?: string,
     *     split_boxes?: bool,
     *     draw_margins?: bool,
     *     builtin_fonts?: bool,
     *     image_resolution?: float
     * } $options size: the paper, in any case (default A4). margins: in mm (top 10, right 10,
     *     bottom 10, left 30; first_top 60, the first page's top, null for the same as the others).
     *     header, footer: HTML on every page. css: for the content, header and footer. base_url: what
     *     relative links and images are found against. toc_page: the page a table of contents starts
     *     on (0, the default: none). metadata: "Title", "Author", "Subject", "Keywords", "Creator",
     *     "Producer" or your own. user_password: to open it (readers may print); owner_password: for
     *     every right. split_boxes: cut boxes across pages as browsers print, rather than moving them
     *     whole. draw_margins: outline the margins, header and footer. builtin_fonts: text in PDF's
     *     built-in Helvetica, Times and Courier (CSS families map to them), embedding none.
     *     image_resolution: PNGs written at most about this many pixels per inch where they're
     *     shown (150, the default; 0 keeps them as they are). Each image is written once.
     * @throws Exception For an unknown option or a value it can't use.
     */
    public function __construct(string $html, array $options = [])
    {
    }

    /**
     * Writes the PDF to a file.
     *
     * @throws Exception When the file can't be written.
     */
    public function save(string $filename): void
    {
    }

    /**
     * The PDF as a string, to send straight from a web page:
     *
     *     header('Content-Type: application/pdf');
     *     echo $pdf->output();
     *
     * @throws Exception When it can't be written.
     */
    public function output(): string
    {
    }

    /** Pages in all, the table of contents' included. */
    public function pageCount(): int
    {
    }
}

/**
 * Existing PDFs written again: pages merged, a letterhead or stamp drawn on every page, passwords
 * added. Any PDF reads, encrypted ones (RC4, AES-128, AES-256) and damaged ones (repaired as
 * they're read) too. The output may be one of the inputs: it's written beside it and renamed, so a
 * failure leaves it as it was.
 *
 * $protection: 'user_password' (to open the file), 'owner_password' (every right), 'permissions'
 * (what readers without the owner's password may do: 'print' and 'print_high_quality', true by
 * default; 'copy', 'modify', 'annotate', 'fill_forms', 'assemble', false). No password: none is
 * added. The encryption is AES-256.
 *
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/PDF-Lite
 */
final class Pdf
{
    /**
     * The files' pages, in order, into $output. The first file's title, bookmarks and named
     * destinations are kept. Files are opened without a password: one encrypted with an owner
     * password only reads. A JPEG or PNG among them (known by its first bytes) becomes a page the
     * size of the page before it (the next file's first page when it comes first; A4 without any
     * PDF), upright by its EXIF orientation, scaled to fit and centred; other picture formats need
     * converting first.
     *
     * @param string[] $files
     * @param array{user_password?: string, owner_password?: string, permissions?: array<string, bool>} $protection
     * @throws Exception For a file that can't be read, no pages at all, or an option it doesn't know.
     */
    public static function merge(array $files, string $output, array $protection = []): void
    {
    }

    /**
     * $decoration's page n drawn on $file's page n, and its last page on the pages after (a one-page
     * letterhead goes on every page), as pdftk's multibackground and multistamp do: upright as each
     * page is shown, scaled to fit and centred.
     *
     * @param 'behind'|'over' $layer Behind the page's content (a background) or over it (a stamp).
     * @param array{user_password?: string, owner_password?: string, permissions?: array<string, bool>} $protection
     * @throws Exception For a file that can't be read, or a decoration without pages.
     */
    public static function decorate(string $file, string $decoration, string $output, string $layer = 'behind', array $protection = []): void
    {
    }

    /**
     * $file written again with passwords; $password opens it when it's encrypted already.
     *
     * @param array{user_password?: string, owner_password?: string, permissions?: array<string, bool>} $protection
     * @throws Exception For no password given, or a file that can't be read.
     */
    public static function protect(string $file, string $output, array $protection, string $password = ''): void
    {
    }

    /**
     * The file's pages; $password opens it when it needs one.
     *
     * @throws Exception For a file that can't be read.
     */
    public static function pageCount(string $file, string $password = ''): int
    {
    }
}

/**
 * An SVG as a PNG, at its own size, or $width and/or $height pixels (one of them 0 keeps the aspect
 * ratio), on white.
 *
 * @throws Exception For text that isn't SVG, or a negative size.
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/Images
 */
function svg_to_png(string $svg, int $width = 0, int $height = 0): string
{
}

/** Standard base64 (RFC 4648, padded with "="), as PHP's own base64_encode(). */
function base64_encode(string $data): string
{
}

/**
 * Standard base64 decoded.
 *
 * @throws Exception For text that isn't base64 (where PHP's own base64_decode() returns false).
 */
function base64_decode(string $data): string
{
}

namespace SC\Excel;

use SC\Exception;

/**
 * An Excel workbook (.xlsx): its sheets in order, each holding the rows that have cells, each row
 * the cells that are set, with their values and formulas. Works like an array of sheets, found by
 * name ignoring case, as Excel does. Sheets, rows and cells are views into it (they keep it
 * alive): changing one changes the workbook. No formatting, styles, merged cells or pictures.
 *
 *     $book = Workbook::open('prices.xlsx');
 *     foreach ($book as $sheet)
 *         foreach ($sheet as $row)
 *             foreach ($row as $cell) echo $cell->name, ' ', $cell->value, ' ', $cell->formula;
 *
 *     $out = new Workbook();
 *     $out['Prices']['A1'] = 'Item';          // the sheet added, as with an array
 *     $out['Prices']['B3']->formula = '=B2*2';
 *     $out->save('out.xlsx');
 *
 * @implements \ArrayAccess<string, Sheet>
 * @implements \IteratorAggregate<string, Sheet>
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/PHP
 */
final class Workbook implements \ArrayAccess, \IteratorAggregate, \Countable
{
    /**
     * Every sheet's cells: values, and formulas with "=" (a formula Excel keeps once for a range
     * is given to each cell, its relative references moved).
     *
     * @throws Exception For a file that can't be read or isn't an .xlsx.
     */
    public static function open(string $filename): Workbook
    {
    }

    /**
     * The same, from an .xlsx's bytes.
     *
     * @throws Exception For bytes that aren't an .xlsx.
     */
    public static function fromString(string $bytes): Workbook
    {
    }

    /**
     * Writes it as an .xlsx: each cell's value and formula, text inline, no styles. Excel
     * calculates the formulas when it opens the file.
     *
     * @throws Exception For a workbook without sheets, a number that isn't finite, or a file that
     *     can't be written; no file is made then.
     */
    public function save(string $filename): void
    {
    }

    /**
     * The .xlsx as a string: what save() writes.
     *
     * @throws Exception As save().
     */
    public function output(): string
    {
    }

    /**
     * Adds an empty sheet.
     *
     * @throws Exception For a name Excel doesn't take: empty, longer than 31 characters, with any
     *     of : \ / ? * [ ], starting or ending with ', or already used (ignoring case).
     */
    public function add(string $name): Sheet
    {
    }

    /** Whether it has a sheet of that name (ignoring case). */
    public function offsetExists(mixed $offset): bool
    {
    }

    /**
     * The sheet of that name, added when there's none.
     *
     * @throws Exception For a name Excel doesn't take.
     */
    public function offsetGet(mixed $offset): Sheet
    {
    }

    /** @throws Exception Always: a sheet is added by $book[$name] or add(), not set. */
    public function offsetSet(mixed $offset, mixed $value): void
    {
    }

    /** @throws Exception Always: sheets can't be removed. */
    public function offsetUnset(mixed $offset): void
    {
    }

    /** @return \Iterator<string, Sheet> The sheets, by name, in the workbook's order. */
    public function getIterator(): \Iterator
    {
    }

    /** The number of sheets. */
    public function count(): int
    {
    }
}

/**
 * A sheet of a workbook: the rows that have cells, in order. A cell is "B3", or [row, column]
 * counted from 0. $sheet['B3'] = 18.5 sets one; $sheet['B3'] is the cell, there or not yet
 * (setting its value or formula adds it).
 *
 * @implements \ArrayAccess<string|array{int, int}, Cell>
 * @implements \IteratorAggregate<int, Row>
 */
final class Sheet implements \ArrayAccess, \IteratorAggregate, \Countable
{
    /** Its name. Read only. */
    public string $name;

    private function __construct()
    {
    }

    /** Whether the cell is there (set, even to null). */
    public function offsetExists(mixed $offset): bool
    {
    }

    /**
     * The cell, there or not yet: reading one that isn't there gives a null value and no formula,
     * and adds nothing.
     *
     * @throws Exception For a cell name that isn't one ("3B"), or beyond XFD1048576.
     */
    public function offsetGet(mixed $offset): Cell
    {
    }

    /**
     * Sets the cell's value (null, a number, text or a bool), or copies another cell's value and
     * formula.
     *
     * @throws Exception For any other value, or no cell named ($sheet[] = ...).
     */
    public function offsetSet(mixed $offset, mixed $value): void
    {
    }

    /** Removes the cell, and its row when that was its last. */
    public function offsetUnset(mixed $offset): void
    {
    }

    /** @return \Iterator<int, Row> The rows that have cells, by their index (from 0). */
    public function getIterator(): \Iterator
    {
    }

    /** The number of rows that have cells. */
    public function count(): int
    {
    }
}

/**
 * A row of a sheet: the cells in it that are set, in column order. A column is a number from 0,
 * or its letters ("B").
 *
 * @implements \ArrayAccess<int|string, Cell>
 * @implements \IteratorAggregate<int, Cell>
 */
final class Row implements \ArrayAccess, \IteratorAggregate, \Countable
{
    /** Its place in the sheet, from 0. Read only. */
    public int $index;

    private function __construct()
    {
    }

    public function offsetExists(mixed $offset): bool
    {
    }

    /** The cell in that column, there or not yet, as Sheet's. */
    public function offsetGet(mixed $offset): Cell
    {
    }

    /** Sets the cell in that column, as Sheet's. */
    public function offsetSet(mixed $offset, mixed $value): void
    {
    }

    public function offsetUnset(mixed $offset): void
    {
    }

    /** @return \Iterator<int, Cell> The cells that are set, by their column (from 0). */
    public function getIterator(): \Iterator
    {
    }

    /** The number of cells that are set. */
    public function count(): int
    {
    }
}

/**
 * A cell of a sheet: its place, value and formula. Setting the value or formula of one that
 * isn't there yet adds it.
 */
final class Cell
{
    /** Its row, from 0. Read only. */
    public int $row;
    /** Its column, from 0. Read only. */
    public int $column;
    /** Its name, "B3". Read only. */
    public string $name;
    /**
     * Null (empty), a number (a float: Excel's numbers are), text or a bool. Dates are numbers,
     * days since 1899-12-30. An error ("#DIV/0!") reads as its text. With a formula, the value it
     * last calculated (null when it was never calculated).
     */
    public float|string|bool|null $value;
    /** "=C6", with the "=", as Excel shows it; "" when there's none. */
    public string $formula;

    private function __construct()
    {
    }
}
