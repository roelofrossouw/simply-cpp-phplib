<?php
// Long native calls and PHP's max_execution_time: a render longer than it completes (PHP can't stop
// C++, and used to end the whole process when its time ran out inside one), the PHP code after it
// is still limited by the time it had left, and the extension's own limit (the option 'time_limit',
// sc.time_limit) stops a render with an SC\Exception.
require __DIR__ . '/check.inc';

$extension = getenv('SC_PHPLIB_EXTENSION');
check(is_string($extension) && is_file($extension), 'the test knows the extension (SC_PHPLIB_EXTENSION)');

// $script run by a PHP of its own, with only this extension: its output and exit code.
function child(string $script): array {
    global $extension;
    $file = tempnam(sys_get_temp_dir(), 'sc-phplib-time-');
    file_put_contents($file, "<?php\n" . $script);
    $process = proc_open([PHP_BINARY, '-n', '-d', "extension=$extension", $file], [1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
    $out = stream_get_contents($pipes[1]) . stream_get_contents($pipes[2]);
    fclose($pipes[1]);
    fclose($pipes[2]);
    $code = proc_close($process);
    unlink($file);
    return [$out, $code];
}

// A render that takes over 1.5 s here (doubled until it does), then the same under a 1 s limit.
$render = <<<'PHP'
function html(int $n): string {
    return str_repeat('<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor.</p>', $n);
}
function render(string $html): int {
    $pdf = new SC\Html2Pdf($html, ['header' => '<p>#Page# of #ToPage#</p>', 'footer' => '<p>footer</p>']);
    return strlen($pdf->output()) > 0 ? $pdf->pageCount() : 0;
}
$n = 4000;
for (;;) {
    $started = microtime(true);
    render(html($n));
    if (microtime(true) - $started > 1.5 || $n >= 64000) break;
    $n *= 2;
}
set_time_limit(1);
$started = microtime(true);
$pages = render(html($n));
printf("rendered %d pages in %.1f s under a 1 s limit\n", $pages, microtime(true) - $started);
PHP;

[$out, $code] = child($render . 'echo "done\n";');
check($code === 0 && str_contains($out, "done\n"), "a render longer than max_execution_time completes (exit $code: " . trim($out) . ')');

// The PHP after it still has a limit: what was left of the 1 s.
[$out, $code] = child($render . 'for (;;) {}');
check(str_contains($out, 'rendered') && str_contains($out, 'Maximum execution time of 1 second'),
      "PHP code after the render is still limited (exit $code: " . trim($out) . ')');
check($code !== 124, 'ended by PHP\'s error, not killed (exit 124)');

// The extension's own limit.
$long = str_repeat('<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor.</p>', 8000);
$stopped = null;
try {
    new SC\Html2Pdf($long, ['header' => '<p>#Page#</p>', 'time_limit' => 0.01]);
} catch (SC\Exception $e) {
    $stopped = $e->getMessage();
}
check($stopped !== null && str_contains($stopped, 'time limit of 0.01 s'), "its own time limit stops it: $stopped");

ini_set('sc.time_limit', '0.01');
check(throws(fn() => new SC\Html2Pdf($long, ['header' => '<p>#Page#</p>'])), 'sc.time_limit too');
$pdf = new SC\Html2Pdf($long, ['header' => '<p>#Page#</p>', 'time_limit' => 0]); // 0: none
check($pdf->pageCount() > 10, "'time_limit' => 0 has none");
ini_set('sc.time_limit', '300');

// Counted again for save() and output(): a limit the layout kept, the writing passes.
$pdf = new SC\Html2Pdf($long, ['header' => '<p>#Page#</p>', 'time_limit' => 60]);
$file = sys_get_temp_dir() . '/sc-phplib-time-limit.pdf';
$pdf->save($file);
check(filesize($file) > 0, 'saved within its limit');
unlink($file);

check(throws(fn() => new SC\Html2Pdf('<p>x</p>', ['time_limit' => -1])), 'a negative time_limit throws');
check(throws(fn() => new SC\Html2Pdf('<p>x</p>', ['time_limit' => 'soon'])), 'time_limit is a number');
