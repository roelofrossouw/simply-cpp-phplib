<?php
// SC\QrCode: QR codes as SVG or PNG, the version and correction chosen, and what it refuses.
require __DIR__ . '/check.inc';

$qr = new SC\QrCode('HELLO WORLD', 'quartile');
check($qr->version() === 1 && $qr->size() === 21, 'the smallest version');
check($qr->correction() === 'quartile', 'the correction asked for, when no better fits');
check((new SC\QrCode('1', 'low'))->correction() === 'high', 'a better correction when it fits');
check($qr->dark(0, 0) && $qr->dark(6, 0) && !$qr->dark(7, 0) && !$qr->dark(-1, 0) && !$qr->dark(0, 21), 'modules: a finder pattern, nothing outside');

$svg = $qr->svg();
check(str_starts_with($svg, '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 29 29"'), 'SVG with a viewBox of modules');
check(str_contains($qr->svg(30, 2), 'viewBox="0 0 25 25" width="30mm" height="30mm"'), 'SVG of a size, with a smaller quiet zone');
check(str_contains($svg, 'd="M4,4h7v1h-7z'), 'the dark modules as a path');

$png = $qr->png(4);
check(str_starts_with($png, "\x89PNG\r\n\x1a\n"), 'a PNG');
$size = unpack('Nwidth/Nheight', substr($png, 16, 8));
check($size['width'] === (21 + 8) * 4 && $size['height'] === $size['width'], 'its size: modules and quiet zone, 4 pixels each');
check(ord($png[24]) === 1 && ord($png[25]) === 0, '1 bit a pixel, greyscale');
if (function_exists('imagecreatefromstring')) check(imagecreatefromstring($png) !== false, 'GD reads it');

$bytes = SC\QrCode::binary("a\0b", 'high');
check($bytes instanceof SC\QrCode && $bytes->correction() === 'high', 'bytes, NUL included');
check($qr->svg() === (new SC\QrCode('HELLO WORLD', 'quartile'))->svg(), 'the same data, the same code');

check(throws(fn() => new SC\QrCode(str_repeat('x', 3000))), 'too much data');
check(throws(fn() => new SC\QrCode("a\0b")), 'text holding a NUL');
check(throws(fn() => new SC\QrCode('x', 'best')), 'an unknown correction');
check(throws(fn() => $qr->png(0)) && throws(fn() => $qr->png(101)), 'module pixels outside 1 to 100');
check(throws(fn() => $qr->svg(0)) && throws(fn() => $qr->svg(null, -1)), 'a size or quiet zone that is wrong');
check(throws(fn() => clone $qr, Error::class), "a code isn't cloned");
