<?php
// A test's checks: each failure printed (ctest looks for "FAILED") and counted in the exit code.
$failures = 0;
function check(bool $ok, string $what): void {
    global $failures;
    if (!$ok) {
        $failures++;
        echo "FAILED: $what\n";
    }
}
function throws(callable $call, string $class = SC\Exception::class): bool {
    try {
        $call();
    } catch (Throwable $e) {
        return $e instanceof $class;
    }
    return false;
}
register_shutdown_function(function () {
    global $failures;
    echo $failures ? "$failures failed\n" : "passed\n";
    if ($failures) exit(1);
});
