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
 * name ignoring case, as Excel does. Sheets, rows, cells and their styles are views into it (they
 * keep it alive): changing one changes the workbook. Cells have a style (font size, colour, bold,
 * italic and underline; a background; borders; alignment; a number format), columns a width, rows
 * a height, and cells can be merged; no pictures.
 *
 *     $book = Workbook::open('prices.xlsx');
 *     foreach ($book as $sheet)
 *         foreach ($sheet as $row)
 *             foreach ($row as $cell) echo $cell->name, ' ', $cell->value, ' ', $cell->formula;
 *
 *     $out = new Workbook();
 *     $out['Prices']['A1'] = 'Item';          // the sheet added, as with an array
 *     $out['Prices']['B3']->formula = '=B2*2';
 *     $out['Prices']['A1']->style->font->bold = true;
 *     $out['Prices']['A1']->style->background = '#ddebf7';
 *     $out['Prices']['A1']->style->borders->bottom = 'double';
 *     $out['Prices']['A1']->style->alignment->horizontal = 'center';
 *     $out['Prices']['B2']->style->numberFormat = '#,##0.00';
 *     $out['Prices']->width('A', 20);         // characters
 *     $out['Prices']->height(0, 24);          // row 1, points
 *     $out['Prices']->merge('A1:D1');
 *     $out->save('out.xlsx');
 *
 * @implements \ArrayAccess<string, Sheet>
 * @implements \IteratorAggregate<string, Sheet>
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/PHP
 */
final class Workbook implements \ArrayAccess, \IteratorAggregate, \Countable
{
    /**
     * Every sheet's cells: values, formulas with "=" (a formula Excel keeps once for a range is
     * given to each cell, its relative references moved) and styles; the column widths set by hand.
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
     * Writes it as an .xlsx: each cell's value, formula and style, text inline, and the columns'
     * widths. Excel calculates the formulas when it opens the file.
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
    /** @var array<int, float> The columns given a width, by number (from 0). Read only: see width(). */
    public array $widths;
    /** @var array<int, float> The rows given a height, by number (from 0), in points. Read only: see height(). */
    public array $heights;
    /** @var list<string> The merged ranges ("A1:D1"), in the order they were merged. Read only: see merge(). */
    public array $merged;

    private function __construct()
    {
    }

    /**
     * A column's width in characters (so many "0"s in the default font, as Excel shows it); null:
     * Excel's default (8.43). With $characters, sets it first (from 0 to 255; null for the
     * default): $sheet->width('B', 20).
     *
     * @param int|string $column A number from 0, or letters ("B").
     * @throws Exception For a column beyond XFD or a width outside 0 to 255.
     */
    public function width(int|string $column, ?float $characters = null): ?float
    {
    }

    /**
     * A row's height in points; null: Excel's default (15 for Calibri 11, more when the text needs
     * it). With $points, sets it first (from 0 to 409, 0 hiding it; null for the default):
     * $sheet->height(0, 24) for row 1. A row with a height and no cells is kept.
     *
     * @param int $row From 0.
     * @throws Exception For a row below 0 or beyond 1048576, or a height outside 0 to 409.
     */
    public function height(int $row, ?float $points = null): ?float
    {
    }

    /**
     * Merges a range of cells into one, shown as its top left cell (the others keep what they
     * hold, hidden).
     *
     * @param string|array{string|array{int, int}, string|array{int, int}} $range "A1:D1", or
     *     [first, last], each a cell as $sheet[...] takes it.
     * @throws Exception For one cell, a range that isn't one, or one overlapping a merge already
     *     there.
     */
    public function merge(string|array $range): void
    {
    }

    /**
     * Undoes the merge of exactly that range.
     *
     * @param string|array{string|array{int, int}, string|array{int, int}} $range As merge() takes it.
     * @return bool Whether there was one.
     * @throws Exception For a range that isn't one.
     */
    public function unmerge(string|array $range): bool
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
     * Sets the cell's value (null, a number, text or a bool), keeping its formula and style, or
     * copies another cell's value, formula and style.
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
 * A cell of a sheet: its place, value, formula and style. Setting the value, formula or style of
 * one that isn't there yet adds it.
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
    /**
     * How it looks. Set it to another cell's style, an array ['font' => ['size' => 14, 'color' =>
     * '#1f4e79', 'bold' => true], 'background' => '#ddebf7', 'borders' => 'thin', 'alignment' =>
     * ['horizontal' => 'center'], 'numberFormat' => '0.00'] (what's left out is the default), or
     * null for the plain style; or change it a part at a time:
     * $cell->style->font->bold = true.
     */
    public Style $style;

    private function __construct()
    {
    }
}

/**
 * A cell's style, a view into it: setting a part of it sets the cell's.
 */
final class Style
{
    /**
     * The font. Set it to another cell's font, an array ['size' => ..., 'color' => ..., 'bold' =>
     * ..., 'italic' => ..., 'underline' => ...] or null.
     */
    public Font $font;
    /**
     * A solid background: "#rrggbb" (any CSS colour when set: "#ddebf7", "#ff0", "yellow",
     * "rgb(255, 255, 0)"), null for none. Theme colours read from a file come as what they are.
     *
     * @throws Exception For text that isn't a colour.
     */
    public ?string $background;
    /**
     * Its borders. Set them to another cell's, ['left' => ..., 'right' => ..., 'top' => ...,
     * 'bottom' => ...] (each as an Edge takes; sides left out have none), one edge for all four
     * sides ('thin', ['line' => 'thin', 'color' => '#1f4e79'], an Edge), or null for none.
     */
    public Borders $borders;
    /**
     * Where its text sits. Set it to another cell's, ['horizontal' => 'center', 'vertical' =>
     * 'top', 'wrap' => true, 'indent' => 1] (what's left out is the default) or null.
     */
    public Alignment $alignment;
    /**
     * Excel's format code: "0.00", "#,##0", "0%", "yyyy-mm-dd", "h:mm", "@" (text),
     * '"R" #,##0.00'; "" for General (null sets General too). A date is a number, days since
     * 1899-12-30, shown as one by a date format. Excel's built-in format 14 (the reader's own short
     * date) reads as "mm-dd-yy".
     *
     * @throws Exception For a code longer than 255 characters.
     */
    public string $numberFormat;

    private function __construct()
    {
    }
}

/**
 * Where a cell's text sits, a view into its cell.
 */
final class Alignment
{
    /**
     * Across the cell, by Excel's name: "general" (text left, numbers right; the default), "left",
     * "center", "right", "fill", "justify", "centerContinuous" (across the empty cells to the
     * right), "distributed". Null sets "general".
     *
     * @throws Exception For a name Excel doesn't have.
     */
    public string $horizontal;
    /**
     * Up and down: "bottom" (the default), "top", "center", "justify", "distributed". Null sets
     * "bottom".
     *
     * @throws Exception For a name Excel doesn't have.
     */
    public string $vertical;
    /** Whether long text wraps onto more lines. */
    public bool $wrap;
    /**
     * Indented by so many steps (about three characters each), from 0 to 250.
     *
     * @throws Exception For anything else.
     */
    public int $indent;

    private function __construct()
    {
    }
}

/**
 * A cell's borders, a view into it: each side an Edge.
 */
final class Borders
{
    /** Set a side to another cell's Edge, ['line' => 'thin', 'color' => '#1f4e79'], a line or null. */
    public Edge $left;
    public Edge $right;
    public Edge $top;
    public Edge $bottom;

    private function __construct()
    {
    }
}

/**
 * One side's border, a view into its cell.
 */
final class Edge
{
    /**
     * Excel's name for its line: "thin", "medium", "thick", "hair", "dotted", "dashed", "double",
     * "dashDot", "dashDotDot", "slantDashDot", "mediumDashed", "mediumDashDot",
     * "mediumDashDotDot"; null for none.
     *
     * @throws Exception For a name Excel doesn't have.
     */
    public ?string $line;
    /**
     * "#rrggbb" (any CSS colour when set), null for automatic (black).
     *
     * @throws Exception For text that isn't a colour.
     */
    public ?string $color;

    private function __construct()
    {
    }
}

/**
 * A cell's font, a view into it. What isn't set is the workbook's default (Calibri, 11 points,
 * automatic colour).
 */
final class Font
{
    /**
     * In points, from 1 to 409; null for the default.
     *
     * @throws Exception For a size outside 1 to 409.
     */
    public ?float $size;
    /**
     * "#rrggbb" (any CSS colour when set), null for automatic.
     *
     * @throws Exception For text that isn't a colour.
     */
    public ?string $color;
    public bool $bold;
    public bool $italic;
    /**
     * "single", "double", "singleAccounting" or "doubleAccounting" (the accounting ones run the
     * cell's width); null for none. Set true for "single", false for none.
     *
     * @throws Exception For a name Excel doesn't have.
     */
    public ?string $underline;

    private function __construct()
    {
    }
}
