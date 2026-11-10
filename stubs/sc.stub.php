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
 * per page (a section whose text is an image's path, <section alt='Brokerage'>/logos/logo1.png</section>,
 * shows that image, class section-image, while its alt is its table of contents entry); a
 * header's #P1Head shows on the first page only and #POHead on the others. In the content
 * <section> starts a new page and an entry in the table of contents.
 *
 * An image whose src starts with qr: is a QR code of what follows, drawn as vector shapes (sharp
 * at any size), written like a data: URI: qr:[correction=L|M|Q|H][;margin=N][;base64],<data>, as
 * in <img src="qr:,https://example.com/pay/123" style="width: 30mm">. It works in the header and
 * footer too; data too long for a QR code throws.
 *
 * Laying out (the constructor) and writing (save(), output()) pause PHP's max_execution_time and
 * set it going again with the time it had left: a long render isn't cut off by it, which used to
 * end the whole php-fpm worker. They have a limit of their own instead, the option time_limit
 * (by default the ini setting sc.time_limit, 300 seconds), counted from the start of each call:
 * past it, or once the client has gone, the render stops and throws an Exception.
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
     *     image_resolution?: float,
     *     time_limit?: float
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
     *     time_limit: seconds (wall clock) the constructor, save() and output() may each take
     *     (0: none; default the ini setting sc.time_limit, 300); PHP's max_execution_time is
     *     paused while they run.
     * @throws Exception For an unknown option or a value it can't use, or a render stopped by its
     *     time limit or because the client has gone.
     */
    public function __construct(string $html, array $options = [])
    {
    }

    /**
     * Writes the PDF to a file.
     *
     * @throws Exception When the file can't be written, or it was stopped by its time limit (no file is left).
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
     * @throws Exception When it can't be written, or it was stopped by its time limit.
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

/**
 * What a passkey check that fails throws: the response isn't what the relying party asked for, or
 * isn't a valid one. getReason() names the check for code; getMessage() says it in a sentence.
 *
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/PHP
 */
class PasskeyException extends Exception
{
    /**
     * Which check failed: "format" (not a response, or malformed), "type", "credential" (ids that
     * differ or aren't 1 to 1023 bytes), "challenge", "origin", "cross_origin", "rp_id",
     * "user_presence", "user_verification", "flags" (backed up without being eligible),
     * "algorithm", "key", "signature" or "counter" (the sign count went back: maybe a clone).
     */
    public function getReason(): string
    {
    }
}

/**
 * Passkeys (WebAuthn Level 3) for a relying party: the options navigator.credentials.create() and
 * get() take, and every check of what the browser answers. The caller keeps the challenge (say, in
 * Redis) and the credentials (id, public key, sign count); this does the CBOR, COSE keys and
 * signatures (ES256, EdDSA with Ed25519, RS256). Attestation is "none": any format is taken, its
 * statement not checked. Ids, keys and challenges are base64url text.
 *
 * In the browser, /usr/share/simply-cpp-passkey/sc-passkey.js (scPasskey.create(options),
 * scPasskey.get(options)) turns the options into a call and the answer into the JSON to send back,
 * also where the browser lacks PublicKeyCredential.toJSON().
 *
 *     $pk = new \SC\Passkey('example.com', 'Example', ['https://*.example.com']);
 *     $options = $pk->registrationOptions($userId, $login, $name, $idsOfTheirPasskeys);
 *     // keep $options['challenge']; json_encode($options) to scPasskey.create()
 *     $made = $pk->verifyRegistration($answerJson, $challenge);      // store $made
 *
 *     $options = $pk->authenticationOptions($idsOfTheirPasskeys);    // to scPasskey.get()
 *     $stored = ...;                                                  // by \SC\Passkey::credentialId($answerJson)
 *     $used = $pk->verifyAuthentication($answerJson, $challenge, $stored['publicKey'], $stored['signCount']);
 *     // store $used['signCount']
 *
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/PHP
 */
final class Passkey
{
    /**
     * @param string $rpId The relying party's ID: a domain, "example.com".
     * @param string $rpName Its name, as authenticators may show it.
     * @param string[] $origins Where the pages are: exact ("https://www.example.com") or a wildcard
     *     ("https://*.example.com": https, any subdomain depth, not the bare domain); each the RP ID
     *     or under it, no ports or paths.
     * @throws Exception For an RP ID or origin that isn't one.
     */
    public function __construct(string $rpId, string $rpName, array $origins)
    {
    }

    /**
     * Options for navigator.credentials.create() (PublicKeyCredentialCreationOptionsJSON), with a
     * fresh challenge to keep for verifyRegistration(). Algorithms ES256, EdDSA, RS256 in that
     * order; resident key "preferred"; attestation "none".
     *
     * @param string $userId Opaque bytes, 1 to 64 (not an e-mail address: authenticators show the names, not this).
     * @param string $userName The account's name, as authenticators show it.
     * @param string $displayName A friendlier name; the user name when empty.
     * @param array<string|array{id: string, transports?: string[]}> $exclude The user's passkeys
     *     already: base64url ids, or with their transports.
     * @param int $timeout In milliseconds, 1000 to 3600000.
     * @param 'required'|'preferred'|'discouraged' $userVerification
     * @return array{rp: array{id: string, name: string}, user: array{id: string, name: string, displayName: string},
     *     challenge: string, pubKeyCredParams: list<array{type: string, alg: int}>, timeout: int,
     *     excludeCredentials: list<array{type: string, id: string, transports?: string[]}>,
     *     authenticatorSelection: array{residentKey: string, requireResidentKey: bool, userVerification: string}, attestation: string}
     * @throws Exception For a user id that isn't 1 to 64 bytes, an empty user name, or a timeout or user verification that isn't one.
     */
    public function registrationOptions(string $userId, string $userName, string $displayName = '', array $exclude = [], int $timeout = 300000, string $userVerification = 'preferred'): array
    {
    }

    /**
     * Options for navigator.credentials.get() (PublicKeyCredentialRequestOptionsJSON), with a fresh
     * challenge to keep for verifyAuthentication().
     *
     * @param array<string|array{id: string, transports?: string[]}> $allow The passkeys it may use
     *     (none: any discoverable one).
     * @param int $timeout In milliseconds, 1000 to 3600000.
     * @param 'required'|'preferred'|'discouraged' $userVerification
     * @return array{challenge: string, timeout: int, rpId: string, allowCredentials: list<array{type: string, id: string, transports?: string[]}>, userVerification: string}
     * @throws Exception For a timeout or user verification that isn't one.
     */
    public function authenticationOptions(array $allow = [], int $timeout = 300000, string $userVerification = 'preferred'): array
    {
    }

    /**
     * The browser's answer to registrationOptions() (RegistrationResponseJSON, what scPasskey.create()
     * gives), checked against the kept challenge: what to keep for the passkey.
     *
     * @param string $response The JSON.
     * @param string $challenge The kept options' challenge.
     * @param bool $requireUserVerification Refuse it when the user wasn't verified (PIN, biometrics).
     * @return array{id: string, publicKey: string, algorithm: int, signCount: int, aaguid: string,
     *     transports: string[], attachment: string, attestationFormat: string, userVerified: bool,
     *     backupEligible: bool, backedUp: bool} publicKey is the COSE key, base64url, to pass back
     *     unchanged; aaguid a UUID (all zeros when the authenticator gives none).
     * @throws PasskeyException For a check that fails.
     * @throws Exception For a challenge that isn't base64url.
     */
    public function verifyRegistration(string $response, string $challenge, bool $requireUserVerification = false): array
    {
    }

    /**
     * The browser's answer to authenticationOptions() (AuthenticationResponseJSON, what
     * scPasskey.get() gives), checked against the kept challenge and the passkey's kept public key
     * and sign count: keep its new signCount. A sign count that went back (both non-zero, the new
     * one not above the kept one) is refused ("counter"); 0 and 0 is what synced passkeys send.
     *
     * @param string $response The JSON.
     * @param string $challenge The kept options' challenge.
     * @param string $publicKey The passkey's publicKey, as verifyRegistration() gave it.
     * @param int $signCount Its sign count as kept.
     * @param bool $requireUserVerification Refuse it when the user wasn't verified.
     * @return array{id: string, userHandle: ?string, signCount: int, userVerified: bool, backupEligible: bool, backedUp: bool}
     * @throws PasskeyException For a check that fails.
     * @throws Exception For a challenge or public key that isn't what was kept (not base64url, not a COSE key).
     */
    public function verifyAuthentication(string $response, string $challenge, string $publicKey, int $signCount, bool $requireUserVerification = false): array
    {
    }

    /**
     * The passkey an answer is for (its base64url id), to look its public key up before verifying.
     *
     * @throws PasskeyException ("format") For JSON that isn't an answer.
     */
    public static function credentialId(string $response): string
    {
    }
}

/**
 * Bootstrap 5 HTML made into HTML email clients show as intended: the layout as tables, the CSS in each
 * element's style attribute (and bgcolor, align and valign), a stylesheet in the head for what can't be
 * inlined. sc-bootstrap-email, a C++ port of bootstrap-email (https://bootstrapemail.com) 1.8.1, which
 * gives what its Ruby gem's command gives, in the PHP process.
 *
 *     $html = \SC\BootstrapEmail::format('<body class="bg-light"><div class="container">...</div></body>');
 *
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/PHP
 */
final class BootstrapEmail
{
    /**
     * The email's HTML: a whole page keeps its head; a <body> alone gets bootstrap-email's layout.
     * The page's own <style> tags are inlined and taken out (unless data-premailer="ignore").
     *
     * @throws Exception When the HTML can't be read at all.
     */
    public static function format(string $html): string
    {
    }
}

/**
 * HTTP for REST and SOAP calls (sc-http's sc::rest over libcurl): TLS certificates and host names
 * checked by default, timeouts always, and nothing written to the logs; error() says what went
 * wrong. Only http:// and https:// URLs, redirects included.
 *
 *     $api = new \SC\Rest('https://example.com/api/items');
 *     $body = $api->bearer($token)->timeout(10)->post(['name' => 'x']);   // JSON
 *     if ($api->status() !== 200) error_log($api->status() . ' ' . $api->error());
 *
 *     $soap = new \SC\Rest($location);
 *     $xml = $soap->header('Content-Type', 'text/xml; charset=utf-8')->header('SOAPAction', '"' . $action . '"')
 *         ->clientCertificate('/etc/app/client.pem', null, $passphrase)->post($envelope);
 *
 * The object keeps its settings; each request is a new connection with them, and status(), headers(),
 * error(), effectiveUrl() and totalTime() describe the last one. A request that gets no response
 * (refused, timed out, a certificate not trusted) returns '' with status() 0; it doesn't throw.
 *
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/PHP
 */
final class Rest
{
    public function __construct(string $url)
    {
    }

    /**
     * A request header, replacing one of the same name (ignoring case). An empty value sends none,
     * not even the default Content-Type (application/json) or curl's own.
     *
     * @throws Exception For a name with ':' or a line break, or a value with a line break.
     */
    public function header(string $name, string $value): static
    {
    }

    /** Authorization: Bearer <token>, replacing one set before. */
    public function bearer(string $token): static
    {
    }

    /**
     * The whole request's limit (default 30; 0: none, e.g. with idleTimeout()).
     *
     * @throws Exception For less than 0 seconds or more than a day.
     */
    public function timeout(int $seconds): static
    {
    }

    /**
     * Gives up when no data arrives for this long (less than a byte a second over that time), however
     * long the whole request may take: a slow but steady answer goes on, a silent server doesn't. 0
     * (the default): no such limit. With timeout(0), as file_get_contents' read timeout.
     *
     * @throws Exception For less than 0 seconds or more than a day.
     */
    public function idleTimeout(int $seconds): static
    {
    }

    /**
     * Connecting's limit (default 10).
     *
     * @throws Exception For less than 1 second or more than a day.
     */
    public function connectTimeout(int $seconds): static
    {
    }

    /**
     * A TLS client certificate (PEM), for a server that asks for one.
     *
     * @param string|null $keyFile Its key; null when it's in certificateFile too (SoapClient's local_cert).
     * @param string|null $passphrase The key's, when it's encrypted.
     */
    public function clientCertificate(string $certificateFile, ?string $keyFile = null, ?string $passphrase = null): static
    {
    }

    /** Trusts only certificates signed by (or being) one in this PEM file, instead of the system's authorities. */
    public function trust(string $caFile): static
    {
    }

    /** Whether the certificate must name the host connected to (default true); it's still checked against trust(). */
    public function verifyHost(bool $on): static
    {
    }

    /** Accepts only a server with this public key, curl's "sha256//<base64>" (several joined by ';'). */
    public function pinnedPublicKey(string $pins): static
    {
    }

    /**
     * Follows redirects (off by default), at most max; effectiveUrl() is where they led. A POST
     * answered with 301, 302 or 303 is followed with a GET.
     *
     * @throws Exception For a max outside 0 to 50.
     */
    public function followRedirects(bool $on = true, int $max = 5): static
    {
    }

    /**
     * A GET; the body byte for byte, whatever the status ('' when no response came).
     *
     * @param array $query Query parameters, added to the URL's own as http_build_query() writes them.
     */
    public function get(array $query = []): string
    {
    }

    /** A HEAD: status() and headers(), no body. */
    public function head(array $query = []): string
    {
    }

    /** A DELETE; the body, whatever the status. */
    public function delete(array $query = []): string
    {
    }

    /**
     * A POST; the response body, whatever the status.
     *
     * @param string|array|null $body A string as it is; an array as JSON (json_encode()), or, when the
     *     Content-Type set is application/x-www-form-urlencoded, as http_build_query() writes it.
     * @param array $query Query parameters.
     * @throws Exception For an array body with a multipart Content-Type (that's postMultipart()'s),
     *     or one that can't be written as JSON.
     */
    public function post(string|array|null $body = '', array $query = []): string
    {
    }

    /** A PUT, with a body as post() takes it. */
    public function put(string|array|null $body = '', array $query = []): string
    {
    }

    /** A PATCH, with a body as post() takes it. */
    public function patch(string|array|null $body = '', array $query = []): string
    {
    }

    /**
     * A POST of multipart/form-data, for upload APIs; the Content-Type, with its boundary, is curl's.
     *
     * @param array $fields name => value.
     * @param array $files name => path, or name => ['path' => ..., 'filename' => ..., 'type' => ...]
     *     (the filename defaults to the file's own, the type to application/octet-stream).
     * @throws Exception For a field that's an array, or a file without a path. A file that can't be
     *     read is no response: status() 0, error() says which.
     */
    public function postMultipart(array $fields, array $files = [], array $query = []): string
    {
    }

    /** The last response's HTTP status (200, 404, ...); 0 when none came. */
    public function status(): int
    {
    }

    /** The last response's headers by lower-cased name ('content-type' => ...). */
    public function headers(): array
    {
    }

    /** Why the last request got no response; '' when one came. */
    public function error(): string
    {
    }

    /**
     * curl's error number for the last request (7 couldn't connect, 28 timed out, 56 the connection
     * broke while receiving ...); 0 when a response came. Curl's numbers, whatever does the transfer.
     */
    public function errorCode(): int
    {
    }

    /** The URL the last response came from: where redirects led, with the query. */
    public function effectiveUrl(): string
    {
    }

    /** How long the last request took, in seconds, redirects included. */
    public function totalTime(): float
    {
    }
}

/**
 * A QR code made from text or bytes, written as SVG or PNG with no image library (sc-qrencode, on
 * Project Nayuki's QR Code generator). The smallest version that holds the data is chosen, then the
 * highest correction level that still fits, at least the one asked for.
 *
 *     $qr = new \SC\QrCode('https://example.com/invoice/123');
 *     $svg = $qr->svg(30);   // 30 mm square
 *     $png = $qr->png(8);    // 8 pixels a module
 *
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/PHP
 */
final class QrCode
{
    /**
     * @param string $text UTF-8; digits and upper-case letters are packed tighter.
     * @param 'low'|'medium'|'quartile'|'high' $correction How much damage it survives: about 7, 15, 25
     *     or 30% of the code.
     * @throws Exception When it doesn't fit (2953 bytes at 'low'), the text holds a NUL, or the
     *     correction is none of those.
     */
    public function __construct(string $text, string $correction = 'medium')
    {
    }

    /**
     * Bytes as they are, for data that isn't text.
     *
     * @param 'low'|'medium'|'quartile'|'high' $correction
     * @throws Exception As the constructor.
     */
    public static function binary(string $bytes, string $correction = 'medium'): QrCode
    {
    }

    /**
     * The dark modules as one path on a white square, with a quiet zone of light modules around.
     *
     * @param float|null $size The picture's width and height in mm, quiet zone included; null for a
     *     viewBox only, so it takes the size it's shown at.
     * @param int $quietZone 4 modules, as the standard asks.
     * @throws Exception For a size that isn't positive, or a quiet zone outside 0 to 100.
     */
    public function svg(?float $size = null, int $quietZone = 4): string
    {
    }

    /**
     * Black and white, 1 bit a pixel, each module an exact block of pixels.
     *
     * @param int $modulePixels 1 to 100.
     * @throws Exception For other sizes, or a quiet zone outside 0 to 100.
     */
    public function png(int $modulePixels = 8, int $quietZone = 4): string
    {
    }

    /** 1 to 40. */
    public function version(): int
    {
    }

    /** Modules a side, without the quiet zone: 21 to 177. */
    public function size(): int
    {
    }

    /** @return 'low'|'medium'|'quartile'|'high' The level used, at least the one asked for. */
    public function correction(): string
    {
    }

    /** Whether a module is dark, counted from the top left; outside the code, false. */
    public function dark(int $x, int $y): bool
    {
    }
}

/**
 * CSV files (RFC 4180), read into and written from the same \SC\Excel\Sheet as an Excel workbook's,
 * so code that only uses data works the same on both: $sheet['B2']->value, foreach over rows and
 * cells, $sheet['A1'] = 'Item'. A CSV file has no styles, widths or formulas: they're left out
 * when one is written.
 *
 * $options: 'delimiter' (one character; '' or none finds it: a comma, unless the file reads more
 * evenly with semicolons, as Excel writes CSV where the decimal separator is a comma), 'typed'
 * (true by default: plain numbers read as numbers and TRUE/FALSE as booleans, a quoted field stays
 * text; false reads every field as text), 'bom' (writing: a UTF-8 byte order mark first, which
 * Excel needs to read UTF-8 text as such). An unknown option throws.
 *
 *     $prices = \SC\Csv::open('prices.csv');
 *     foreach ($prices as $row) foreach ($row as $cell) echo $cell->name, ' ', $cell->value;
 *     \SC\Csv::save($book['Prices'], 'prices.csv');   // an Excel sheet's values
 *
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/Office
 */
final class Csv
{
    /**
     * A CSV file as a sheet, named after the file ("prices" for prices.csv; "Sheet1" when Excel
     * wouldn't take that name). Empty fields are cells that aren't set.
     *
     * @throws Exception When it can't be read, or a quoted field isn't closed.
     */
    public static function open(string $filename, array $options = []): \SC\Excel\Sheet
    {
    }

    /** The same, for CSV text held in a string; the sheet is "Sheet1". */
    public static function fromString(string $text, array $options = []): \SC\Excel\Sheet
    {
    }

    /** An empty sheet, to fill and save as CSV. */
    public static function create(string $name = 'Sheet1'): \SC\Excel\Sheet
    {
    }

    /**
     * Writes a sheet's values as CSV, from A1 to its last row and column, with CRLF line ends:
     * a CSV sheet's, or an Excel workbook's (a formula gives the value it last calculated). Text
     * that would read back as a number or TRUE/FALSE is quoted, so it stays text.
     *
     * @throws Exception When the file can't be written, or for a number that isn't finite.
     */
    public static function save(\SC\Excel\Sheet $sheet, string $filename, array $options = []): void
    {
    }

    /** What save() writes, as a string: to send with header('Content-Type: text/csv'). */
    public static function output(\SC\Excel\Sheet $sheet, array $options = []): string
    {
    }

    /** The delimiter open() would find in the text: "," or ";". */
    public static function delimiter(string $text): string
    {
    }
}

/**
 * A Word document's (.docx) text, as a reader (a person, or a language model that can't open a
 * .docx) would take it: a line per paragraph with its list number in front as Word shows it
 * ("4.1.2 the Purchaser ..."), the numbering set on the paragraph or through its style; bullets get
 * none. Tabs and line breaks kept; a table a row per line, its cells separated by " | "; content
 * controls and text boxes included; deleted text (tracked changes), field codes and the copies kept
 * for older readers left out; at most one blank line between paragraphs.
 *
 *     $text = SC\Word::text('/tmp/agreement.docx');
 *
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/Office
 */
final class Word
{
    /**
     * The text of the .docx at path.
     *
     * @param array{headers_footers?: bool, notes?: bool} $options headers_footers: the headers' and
     *     footers' text after the body; notes: the footnotes' and endnotes' after that.
     * @throws Exception For a file that isn't a .docx (an old .doc too), or an unknown option.
     */
    public static function text(string $path, array $options = []): string
    {
    }

    /**
     * The same for a .docx's bytes (an upload, a mail's attachment).
     *
     * @param array{headers_footers?: bool, notes?: bool} $options As for text().
     * @throws Exception For bytes that aren't a .docx, or an unknown option.
     */
    public static function textFromString(string $bytes, array $options = []): string
    {
    }
}

/**
 * An email read in memory: an .eml file (MIME) or an Outlook .msg, with nothing written next to
 * it. A .msg reads like an .eml: From (the SMTP address when it has one), To, Cc, Subject, Date,
 * the text body, HTML from its HTML or its RTF, attachments and attached messages.
 *
 *     $mail = \SC\Mail::open('/var/www/data/in/x.msg');      // or \SC\Mail::fromString($bytes)
 *     $mail->subject(); $mail->from()[0]['address'];
 *     $html = $mail->html() ?? nl2br(htmlspecialchars($mail->text() ?? ''));
 *     foreach ($mail->attachments() as $file)
 *         file_put_contents($folder . '/' . basename($file['filename'] ?? $file['label']), $mail->part($file['label']));
 *
 * Parts have labels as mailparse's do: the top part is "1", a multipart's parts "1.1", "1.2", ...,
 * and an attached message's top part one level below it ("1.2.1"). A part is an array:
 * 'label', 'type' ("application/pdf"), 'charset', 'filename', 'disposition' ("inline",
 * "attachment" or null), 'content_id' (what the HTML's "cid:" links name), 'size' (decoded bytes;
 * 0 for a multipart), 'multipart', 'message' (an attached message), 'attachment' (a file rather
 * than body text). Addresses are arrays of ['name' => ..., 'address' => ...].
 *
 * @link https://github.com/roelofrossouw/simply-cpp/wiki/PHP
 */
final class Mail
{
    /**
     * An .eml or .msg file.
     *
     * @throws Exception When it can't be read, or holds no message.
     */
    public static function open(string $path): Mail
    {
    }

    /**
     * The same, for a message's bytes.
     *
     * @throws Exception When they hold no message.
     */
    public static function fromString(string $bytes): Mail
    {
    }

    /** RTF as HTML: the HTML Outlook wraps in RTF as it was, other RTF as HTML of its text. */
    public static function rtfToHtml(string $rtf): string
    {
    }

    /** Decoded to UTF-8; '' when it has none. */
    public function subject(): string
    {
    }

    public function from(): array
    {
    }

    public function to(): array
    {
    }

    public function cc(): array
    {
    }

    public function bcc(): array
    {
    }

    public function replyTo(): array
    {
    }

    /** When it was sent, in seconds since 1970; null when it has no date. */
    public function date(): ?int
    {
    }

    /** Without the <>; '' when it has none. */
    public function messageId(): string
    {
    }

    /** A header's value, unfolded and decoded, any case; null when it has none. */
    public function header(string $name): ?string
    {
    }

    /** Every header in order, as ['name' => ..., 'value' => ...]. */
    public function headers(): array
    {
    }

    /** The text body in UTF-8 with "\n" line ends; null when it has none. */
    public function text(): ?string
    {
    }

    /** The HTML body in UTF-8 (a .msg's from its RTF when that's all it has); null when it has none. */
    public function html(): ?string
    {
    }

    /** Every part, in order (see above). */
    public function parts(): array
    {
    }

    /** The parts that are files (inline pictures and attached messages too), not those inside an attached message. */
    public function attachments(): array
    {
    }

    /**
     * A part's content, its transfer encoding undone: a file's bytes, text in its own charset, an
     * attached message's whole text.
     *
     * @throws Exception For a label that isn't a part, or a multipart.
     */
    public function part(string $label): string
    {
    }

    /**
     * A text part's content in UTF-8 with "\n" line ends; RTF as HTML.
     *
     * @throws Exception For a label that isn't a part, or a part that isn't text.
     */
    public function partText(string $label): string
    {
    }

    /**
     * An attached message, as a Mail of its own.
     *
     * @throws Exception For a label that isn't an attached message.
     */
    public function message(string $label): Mail
    {
    }
}

namespace SC\Excel;

use SC\Exception;

/**
 * An Excel workbook (.xlsx): its sheets in order, each holding the rows that have cells, each row
 * the cells that are set, with their values and formulas. Works like an array of sheets, found by
 * name ignoring case, as Excel does. Sheets, rows, cells and their styles are views into it (they
 * keep it alive): changing one changes the workbook. Cells have a style (font size, colour, bold,
 * italic, strikethrough and underline; a background; borders; alignment; a number format), columns
 * a width and rows a height (either can be hidden), cells can be merged and panes frozen; no
 * pictures.
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
 *     $out['Prices']->hiddenColumn('E', true);
 *     $out['Prices']->hiddenRow(9, true);     // row 10
 *     $out['Prices']->freeze('A2');           // row 1 stays in view
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
    /** @var list<int> The hidden columns, by number (from 0). Read only: see hiddenColumn(). */
    public array $hiddenColumns;
    /** @var list<int> The hidden rows, by number (from 0). Read only: see hiddenRow(). */
    public array $hiddenRows;
    /** The cell the panes are frozen at ("B2"), null when they aren't. Read only: see freeze(). */
    public ?string $frozen;

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

    /**
     * Whether a column is hidden. With $hide, hides it first (true) or shows it again (false,
     * keeping its width).
     *
     * @param int|string $column A number from 0, or letters ("E").
     * @throws Exception For a column beyond XFD.
     */
    public function hiddenColumn(int|string $column, ?bool $hide = null): bool
    {
    }

    /**
     * Whether a row is hidden. With $hide, hides it first (true) or shows it again (false, keeping
     * its height and cells). A hidden row with no cells is kept.
     *
     * @param int $row From 0.
     * @throws Exception For a row below 0 or beyond 1048576.
     */
    public function hiddenRow(int $row, ?bool $hide = null): bool
    {
    }

    /**
     * Freezes the rows above the cell and the columns left of it, as Excel's Freeze Panes does
     * there: 'B2' keeps row 1 and column A in view, 'A2' row 1 only, 'B1' column A only; 'A1'
     * freezes nothing.
     *
     * @param string|array{int, int} $cell "B2", or [row, column] from 0.
     * @throws Exception For a cell that isn't one.
     */
    public function freeze(string|array $cell): void
    {
    }

    /** Unfreezes the panes. */
    public function unfreeze(): void
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
     * ..., 'italic' => ..., 'strike' => ..., 'underline' => ...] or null.
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
     * 'bottom' => ..., 'diagonalUp' => ..., 'diagonalDown' => ...] (each as an Edge takes; those left
     * out have none), one edge for all four sides ('thin', ['line' => 'thin', 'color' => '#1f4e79'],
     * an Edge; no diagonals), or null for none.
     */
    public Borders $borders;
    /**
     * Where its text sits. Set it to another cell's, ['horizontal' => 'center', 'vertical' =>
     * 'top', 'wrap' => true, 'indent' => 1, 'rotation' => 45, 'stacked' => false] (what's left out
     * is the default) or null.
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
    /**
     * Degrees counter-clockwise, from -90 (reading downwards) to 90 (upwards), as Excel's
     * Orientation shows them.
     *
     * @throws Exception For anything else.
     */
    public int $rotation;
    /** Letters stacked one under another (Excel's Vertical Text); rotation then counts for nothing. */
    public bool $stacked;

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
    /**
     * The diagonals: up from bottom left to top right, down from top left to bottom right. Excel
     * draws both alike, so when both are set they must be the same edge (saving throws otherwise).
     */
    public Edge $diagonalUp;
    public Edge $diagonalDown;

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
    /** Struck through. */
    public bool $strike;
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
