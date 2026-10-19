<?php
// SC\Html2Pdf: options by name, pages, save() and output(), and mistakes.
require __DIR__ . '/check.inc';

$html = '<html><head><title>Report</title></head><body>';
foreach (['One', 'Two'] as $part) {
    $html .= "<section>$part</section><h2>$part</h2>" . str_repeat('<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit.</p>', 40);
}
$html .= '</body></html>';

$pdf = new SC\Html2Pdf($html, [
    'size' => 'a4', 'landscape' => false, 'toc_page' => 2,
    'margins' => ['top' => 10, 'left' => 20, 'first_top' => null],
    'header' => '<p>#section# - page #Page# of #ToPage#</p>', 'footer' => '<p>footer 👍</p>',
    'metadata' => ['Author' => 'PHP test'],
]);
check($pdf->pageCount() >= 3, 'two sections and a table of contents: at least 3 pages');

$bytes = $pdf->output();
check(str_starts_with($bytes, '%PDF-'), 'output() is a PDF');
check(str_contains($bytes, '/Author (PHP test)'), 'with its metadata');
check(str_contains($bytes, '/Title (Report)'), 'its title from the HTML');
check(str_contains($bytes, '/Type /Outlines'), 'and an outline');

$file = sys_get_temp_dir() . '/sc-phplib-test.pdf';
$pdf->save($file);
check(filesize($file) === strlen($bytes), 'save() writes the same PDF');
unlink($file);

$locked = (new SC\Html2Pdf('<p>secret</p>', ['user_password' => 'open']))->output();
check(str_contains($locked, '/Encrypt'), 'a password encrypts it');

check(throws(fn() => new SC\Html2Pdf('<p>x</p>', ['sise' => 'A4'])), 'an unknown option throws');
check(throws(fn() => new SC\Html2Pdf('<p>x</p>', ['size' => 'B7'])), 'an unknown size throws');
check(throws(fn() => new SC\Html2Pdf('<p>x</p>', ['margins' => 5])), 'margins must be an array');
check(throws(fn() => (new SC\Html2Pdf('<p>x</p>'))->save('/no/such/directory/x.pdf')), 'an unwritable file throws');
check(throws(fn() => clone $pdf, Error::class), 'not cloned');
