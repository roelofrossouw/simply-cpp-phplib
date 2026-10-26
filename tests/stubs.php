<?php
// The stubs (stubs/sc.stub.php) describe what the extension declares: every function and class
// method it has, with the same parameters, is in them, and they declare nothing it lacks.
require __DIR__ . '/check.inc';

$stub = file_get_contents(__DIR__ . '/../stubs/sc.stub.php');
check($stub !== false, 'the stubs are there');

$extension = new ReflectionExtension('sc');
foreach ($extension->getFunctions() as $function) {
    $name = substr($function->getName(), strlen('SC\\'));
    check(preg_match('/function ' . preg_quote($name, '/') . '\(([^)]*)\)/', $stub, $m) === 1, "$name is in the stubs");
    $stub_parameters = $m[1] === '' ? 0 : substr_count($m[1], '$');
    check($stub_parameters === $function->getNumberOfParameters(), "$name has its parameters in the stubs");
}
foreach ($extension->getClasses() as $class) {
    $short = $class->getShortName(); // Html2Pdf, or Workbook in SC\Excel
    check(preg_match('/class ' . preg_quote($short, '/') . '\b/', $stub) === 1, "class $short is in the stubs");
    foreach ($class->getMethods() as $method) {
        if ($method->getDeclaringClass()->getName() !== $class->getName()) continue; // \Exception's own
        check(str_contains($stub, 'function ' . $method->getName() . '('), "{$class->getName()}::{$method->getName()} is in the stubs");
    }
}
preg_match_all('/^function (\w+)/m', $stub, $declared);
foreach ($declared[1] as $name) check(function_exists("SC\\$name"), "SC\\$name, in the stubs, exists");
