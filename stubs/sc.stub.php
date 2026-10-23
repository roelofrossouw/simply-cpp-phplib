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
     *     builtin_fonts?: bool
     * } $options size: the paper, in any case (default A4). margins: in mm (top 10, right 10,
     *     bottom 10, left 30; first_top 60, the first page's top, null for the same as the others).
     *     header, footer: HTML on every page. css: for the content, header and footer. base_url: what
     *     relative links and images are found against. toc_page: the page a table of contents starts
     *     on (0, the default: none). metadata: "Title", "Author", "Subject", "Keywords", "Creator",
     *     "Producer" or your own. user_password: to open it (readers may print); owner_password: for
     *     every right. split_boxes: cut boxes across pages as browsers print, rather than moving them
     *     whole. draw_margins: outline the margins, header and footer. builtin_fonts: text in PDF's
     *     built-in Helvetica, Times and Courier (CSS families map to them), embedding none.
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
     * password only reads.
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
