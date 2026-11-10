<?php
// SC\Word: a .docx's text (tests/resource/agreement.docx, made up), list numbers as Word shows them,
// tables as rows of cells; headers, footers and notes on request; a file that isn't a .docx throws.
require __DIR__ . '/check.inc';

$file = __DIR__ . '/resource/agreement.docx';
$text = SC\Word::text($file);
check(str_starts_with($text, "DEED OF SALE\n1. DEFINITIONS\n1.1 Purchaser means\tAnn\n(a) first\n(b) second\n2. CONDITIONS PRECEDENT\n"),
      'clause numbers from the styles and the list levels: ' . substr($text, 0, 120));
check(str_contains($text, "2.1 within 30 days\n"), 'deleted text left out');
check(str_contains($text, "\na bullet\n"), 'bullets get no number');
check(str_contains($text, "\nV) roman from five\n5.1 legal\n"), 'startOverride, roman numerals, legal numbering');
check(str_contains($text, "Price | R1 000 000\nErf | 123\nJohannesburg\n"), 'a table: a row a line, its cells separated by |');
check(str_contains($text, "non-refundable7"), 'a non-breaking hyphen; the field code left out');
check(!str_contains($text, '(old)'), 'the copy kept for older readers left out');
check(str_ends_with($text, 'Bob Seller'), 'a content control read');
check(!str_contains($text, 'Initials'), 'no headers unless asked for');

check(SC\Word::textFromString(file_get_contents($file)) === $text, 'from a string: the same');

$all = SC\Word::text($file, ['headers_footers' => true, 'notes' => true]);
check(str_ends_with($all, "Bob Seller\n\nInitials: ____\n\nPage footer\n\nSee the title deed."), 'headers, footers and notes when asked for');

check(throws(fn() => SC\Word::text(__FILE__)), "a file that isn't a .docx throws");
check(throws(fn() => SC\Word::textFromString('not a docx')), "bytes that aren't a .docx throw");
check(throws(fn() => SC\Word::text($file, ['footers' => true])), 'an unknown option throws');
