<?php
// SC\svg_to_png(): a PNG at the SVG's own size, or one given.
require __DIR__ . '/check.inc';

$svg = '<svg xmlns="http://www.w3.org/2000/svg" width="40" height="20"><rect width="40" height="20" fill="red"/></svg>';
$png = SC\svg_to_png($svg);
check(str_starts_with($png, "\x89PNG"), 'a PNG');
check(unpack('N2', substr($png, 16, 8)) === [1 => 40, 2 => 20], 'its own size');
$big = SC\svg_to_png($svg, 80);
check(unpack('N2', substr($big, 16, 8)) === [1 => 80, 2 => 40], 'a width, the aspect ratio kept');
check(throws(fn() => SC\svg_to_png('not svg')), 'text that isn\'t SVG throws');
check(throws(fn() => SC\svg_to_png($svg, -1)), 'a negative width throws');
