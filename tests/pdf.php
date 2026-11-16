<?php
// SC\Pdf: merge(), decorate(), protect(), decrypt() and pageCount() on PDFs SC\Html2Pdf writes, and mistakes.
require __DIR__ . '/check.inc';

$dir = sys_get_temp_dir() . '/sc-phplib-pdf-' . getmypid();
@mkdir($dir);
$report = "$dir/report.pdf";
$appendix = "$dir/appendix.pdf";
$letterhead = "$dir/letterhead.pdf";
$page = fn(string $text) => '<p>' . $text . '</p>';
(new SC\Html2Pdf($page('Report') . '<div style="page-break-before: always"></div>' . $page('Page two'),
                 ['metadata' => ['Title' => 'The report']]))->save($report);
(new SC\Html2Pdf($page('Appendix'), ['owner_password' => 'theirs']))->save($appendix); // encrypted, opens without a password
(new SC\Html2Pdf('<div style="border: 2px solid #1e88e5; height: 260mm">Letterhead</div>'))->save($letterhead);
check(SC\Pdf::pageCount($report) === 2, 'the report has 2 pages');

// merge(): into the report itself, as MergePDF() does.
SC\Pdf::merge([$report, $appendix], $report);
check(SC\Pdf::pageCount($report) === 3, 'merged: 3 pages');
check(str_contains(file_get_contents($report), 'The report'), "the first file's title is kept");

// Pictures as pages: a PNG (written here) the size of the page before it.
$png = "$dir/picture.png"; // a 1x1 PNG, from its bytes (the tests run with only sc loaded: no GD)
file_put_contents($png, base64_decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg=='));
SC\Pdf::merge([$report, $png], "$dir/with-picture.pdf");
check(SC\Pdf::pageCount("$dir/with-picture.pdf") === 4, 'a PNG appended as a page');
check(throws(fn() => SC\Pdf::merge([$report, __FILE__], "$dir/x.pdf")), "a file that's neither PDF nor picture throws");

// decorate(): behind, and over.
SC\Pdf::decorate($report, $letterhead, "$dir/behind.pdf");
check(SC\Pdf::pageCount("$dir/behind.pdf") === 3, 'decorated: still 3 pages');
SC\Pdf::decorate($report, $letterhead, "$dir/over.pdf", 'over');
check(filesize("$dir/over.pdf") > 0, 'stamped');

// Passwords: merged and protected in one call, and protect() alone.
SC\Pdf::merge([$report, $appendix], "$dir/locked.pdf",
              ['user_password' => 'open', 'owner_password' => 'admin', 'permissions' => ['annotate' => true]]);
check(throws(fn() => SC\Pdf::pageCount("$dir/locked.pdf")), 'locked: no password, no pages');
check(SC\Pdf::pageCount("$dir/locked.pdf", 'open') === 4, 'locked: opened with its password');
check(SC\Pdf::pageCount("$dir/locked.pdf", 'admin') === 4, 'and with the owner password');
SC\Pdf::protect("$dir/locked.pdf", "$dir/relocked.pdf", ['owner_password' => 'mine'], 'open');
check(SC\Pdf::pageCount("$dir/relocked.pdf") === 4, 'protected again with an owner password only: opens without one');
SC\Pdf::merge([$report], "$dir/none.pdf", ['user_password' => '']);
check(SC\Pdf::pageCount("$dir/none.pdf") === 3, 'an empty password: none added');

// decrypt(): the password taken off, by the user's or the owner's; in place; one with an owner password only.
$special = 'a&b"c';
SC\Pdf::protect($report, "$dir/special.pdf", ['user_password' => $special, 'owner_password' => 'xyz']);
check(throws(fn() => SC\Pdf::pageCount("$dir/special.pdf")), 'special: needs its password');
SC\Pdf::decrypt("$dir/special.pdf", "$dir/open.pdf", $special);
check(SC\Pdf::pageCount("$dir/open.pdf") === 3, 'decrypted by the user password: opens without one');
$opened = file_get_contents("$dir/open.pdf");
check(str_contains($opened, 'The report') || stripos($opened, bin2hex('The report')) !== false, 'its title readable (literal or hex)');
check(!str_contains(file_get_contents("$dir/open.pdf"), '/Encrypt'), 'no /Encrypt left');
SC\Pdf::decrypt("$dir/special.pdf", "$dir/open-owner.pdf", 'xyz');
check(SC\Pdf::pageCount("$dir/open-owner.pdf") === 3, 'and by the owner password');
copy("$dir/special.pdf", "$dir/in-place.pdf");
SC\Pdf::decrypt("$dir/in-place.pdf", "$dir/in-place.pdf", $special);
check(SC\Pdf::pageCount("$dir/in-place.pdf") === 3, 'in place');
SC\Pdf::decrypt("$dir/relocked.pdf", "$dir/unrestricted.pdf");
check(!str_contains(file_get_contents("$dir/unrestricted.pdf"), '/Encrypt'), 'an owner password only: taken off without a password');
$message = '';
try {
    SC\Pdf::decrypt("$dir/special.pdf", "$dir/never.pdf", 'wrong');
} catch (SC\Exception $e) {
    $message = $e->getMessage();
}
check(str_contains($message, "the password doesn't open this file"), 'a wrong password: the message pageCount() gives');
check(throws(fn() => SC\Pdf::decrypt("$dir/special.pdf", "$dir/never.pdf")), 'no password throws');
check(!file_exists("$dir/never.pdf"), 'and writes nothing');
SC\Pdf::decrypt($report, "$dir/copy.pdf");
check(file_get_contents("$dir/copy.pdf") === file_get_contents($report), 'not encrypted: copied as it is');
check(throws(fn() => SC\Pdf::decrypt(__FILE__, "$dir/never.pdf")), "a file that isn't a PDF throws");

// Revisions 3 and 5 with no user password (printing and changes refused), in the same process as SC\Html2Pdf above:
// libsc-html2pdf 1.6.6 exported its own sc-pdflite 4.6.3, which took over these calls and refused revision 5.
$resource = __DIR__ . '/resource';
$pages = function (string $file, string $password = ''): int|string { // the message instead, so a failure is a check, not the end
    try {
        return SC\Pdf::pageCount($file, $password);
    } catch (SC\Exception $e) {
        return $e->getMessage();
    }
};
foreach (['aes-256-r5-empty.pdf', 'rc4-128-empty.pdf'] as $file) {
    $opened = $pages("$resource/$file");
    check($opened === 7, "$file: opens without a password (got " . var_export($opened, true) . ')');
    check($pages("$resource/$file", 'anything') === 7, "$file: and with any password");
    check(!throws(fn() => SC\Pdf::decrypt("$resource/$file", "$dir/$file")), "$file: decrypted without a password");
    check(is_file("$dir/$file") && !str_contains(file_get_contents("$dir/$file"), '/Encrypt') && $pages("$dir/$file") === 7, "$file: and opens unencrypted");
    check(!throws(fn() => SC\Pdf::merge(["$resource/$file", $report], "$dir/merged-$file")) && $pages("$dir/merged-$file") === 10, "$file: merged");
}
// Three kinds of failure: a password needed, encryption it can't read (no password helps), not a PDF.
$failure = function (callable $call): string {
    try {
        $call();
    } catch (SC\Exception $e) {
        return $e->getMessage();
    }
    return '(opened)';
};
check($failure(fn() => SC\Pdf::pageCount("$dir/special.pdf")) === "sc::pdflite: the password doesn't open this file", 'needs a password: told so');
$certificates = "$dir/certificates.pdf"; // the standard handler's name swapped for another of the same length
file_put_contents($certificates, str_replace('/Filter /Standard', '/Filter /PubSec01', file_get_contents("$resource/rc4-40-plain.pdf")));
check($failure(fn() => SC\Pdf::pageCount($certificates)) === "sc::pdflite: an encryption sc::pdflite can't read: the security handler PubSec01",
      "another security handler: can't read, not a password");
check(!str_contains($failure(fn() => SC\Pdf::pageCount(__FILE__)), 'password'), "not a PDF: no password asked");

// Mistakes.
check(throws(fn() => SC\Pdf::merge(["$dir/missing.pdf"], "$dir/x.pdf")), 'a missing file throws');
check(!file_exists("$dir/x.pdf"), 'and writes nothing');
check(throws(fn() => SC\Pdf::merge([], "$dir/x.pdf")), 'nothing to merge throws');
check(throws(fn() => SC\Pdf::merge([$report], "$dir/x.pdf", ['password' => 'x'])), 'an unknown protection key throws');
check(throws(fn() => SC\Pdf::merge([$report], "$dir/x.pdf", ['owner_password' => 'x', 'permissions' => ['printing' => true]])),
      'an unknown permission throws');
check(throws(fn() => SC\Pdf::decorate($report, $letterhead, "$dir/x.pdf", 'under')), "a layer that isn't behind or over throws");
check(throws(fn() => SC\Pdf::protect($report, "$dir/x.pdf", [])), 'protect() without a password throws');
check(throws(fn() => SC\Pdf::pageCount(__FILE__)), "a file that isn't a PDF throws");

array_map('unlink', glob("$dir/*"));
rmdir($dir);
