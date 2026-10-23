<?php
// SC\Pdf: merge(), decorate(), protect() and pageCount() on PDFs SC\Html2Pdf writes, and mistakes.
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
