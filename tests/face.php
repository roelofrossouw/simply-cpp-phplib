<?php
// SC\Face: features and faces read through ai-api (a stand-in here: resource/ai_api.py), the largest
// face chosen, similarity on stored features, and what it refuses. sc.ai_url and sc.ai_token_file are
// system settings, so the parts that need them run in another PHP with -d (as php.ini would set them).
require __DIR__ . '/check.inc';

$e0 = array_fill(0, 512, 0.0);
$e0[0] = 1.0;
$e1 = array_fill(0, 512, 0.0);
$e1[1] = 1.0;

if (getenv('SC_FACE_PART') === 'service') {
    $dir = getenv('SC_FACE_DIR');
    file_put_contents("$dir/photo.jpg", 'a photo');
    file_put_contents("$dir/empty.jpg", 'no faces');
    file_put_contents("$dir/text.jpg", 'not a picture');

    $features = SC\Face::features("$dir/photo.jpg");
    check(is_array($features) && count($features) === 512, 'features(): 512 numbers');
    check($features === $e0, 'features(): the largest face, not the first');
    check(SC\Face::features("$dir/empty.jpg") === null, 'features(): null without a face');

    $faces = SC\Face::faces("$dir/photo.jpg");
    check(count($faces) === 2, 'faces(): every face');
    check(array_keys($faces[0]) === ['features', 'box'], 'faces(): features and box each');
    check($faces[1]['box'] == ['left' => 50, 'top' => 40, 'width' => 100, 'height' => 120], 'faces(): the box in pixels');
    check(count(SC\Face::faces("$dir/photo.jpg", 1)) === 1, 'faces(): maxFaces goes to ai-api');
    check(SC\Face::faces("$dir/empty.jpg") === [], 'faces(): an empty list without faces');
    check(SC\Face::similarity($features, $faces[1]['features']) == 100, 'its features compare as the same face');

    check(throws(fn() => SC\Face::features("$dir/text.jpg")), 'not a picture (422)');
    check(throws(fn() => SC\Face::features("$dir/missing.jpg")), 'a file that is not there');
    check(throws(fn() => SC\Face::faces("$dir/photo.jpg", 0)) && throws(fn() => SC\Face::faces("$dir/photo.jpg", 101)), 'maxFaces outside 1 to 100');
    check(ini_set('sc.ai_url', 'http://elsewhere.example.com') === false && ini_get('sc.ai_url') !== 'http://elsewhere.example.com',
        'a script cannot move sc.ai_url');
    exit;
}
if (getenv('SC_FACE_PART') === 'refused') {
    check(throws(fn() => SC\Face::features(getenv('SC_FACE_DIR') . '/photo.jpg')), getenv('SC_FACE_WHY'));
    exit;
}

// Here, without the settings
check(ini_get('sc.ai_url') === '' && ini_get('sc.ai_token_file') === '', 'no ai-api unless set');
check(throws(fn() => SC\Face::features(__FILE__)), 'features() without sc.ai_url');

// similarity(): on features alone, no ai-api
check(abs(SC\Face::similarity($e0, $e0) - 100) < 1e-9, 'the same features: 100');
check(abs(SC\Face::similarity($e0, $e1)) < 1e-9, 'unrelated features: 0');
$half = $e0;
$half[1] = 1.0; // normalised again: 1/sqrt(2) along e0
check(abs(SC\Face::similarity($e0, $half) - 100 / sqrt(2)) < 1e-6, 'features normalised again');
check(abs(SC\Face::similarity($e0, array_map(fn($x) => -$x, $e0)) + 100) < 1e-9, 'opposite features: -100');
$int = $e0;
$int[0] = 3;
check(abs(SC\Face::similarity($e0, $int) - 100) < 1e-9, 'integers count as numbers');
check(throws(fn() => SC\Face::similarity([1.0, 2.0], $e0)), 'not 512 numbers');
check(throws(fn() => SC\Face::similarity(array_fill(0, 512, 0.0), $e0)), 'all zero');
check(throws(fn() => SC\Face::similarity(array_fill(0, 512, 'x'), $e0)), 'not numbers');
check(throws(fn() => SC\Face::similarity(array_fill(0, 512, NAN), $e0)), 'not finite');

// Through the stand-in ai-api
$extension = getenv('SC_PHPLIB_EXTENSION');
check(is_string($extension) && is_file($extension), 'the test knows the extension (SC_PHPLIB_EXTENSION)');
$dir = sys_get_temp_dir() . '/sc-face-' . getmypid();
@mkdir($dir);
file_put_contents("$dir/token", "test-token\n");
file_put_contents("$dir/wrong-token", "other\n");
file_put_contents("$dir/photo.jpg", 'a photo');
$server = proc_open(['python3', __DIR__ . '/resource/ai_api.py', 'test-token'], [0 => ['pipe', 'r'], 1 => ['pipe', 'w']], $pipes);
[$word, $port] = explode(' ', trim(fgets($pipes[1])));
check($word === 'listening', 'the stand-in ai-api runs');

$run = function (string $part, string $url, string $token_file, string $why = '') use ($extension, $dir): string {
    $env = getenv() + ['SC_FACE_PART' => $part, 'SC_FACE_DIR' => $dir, 'SC_FACE_WHY' => $why];
    $env['SC_FACE_PART'] = $part;
    $process = proc_open([PHP_BINARY, '-n', '-d', "extension=$extension", '-d', "sc.ai_url=$url", '-d', "sc.ai_token_file=$token_file", __FILE__],
        [1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $out, null, $env);
    $text = stream_get_contents($out[1]) . stream_get_contents($out[2]);
    proc_close($process);
    return $text;
};
$said = $run('service', "http://127.0.0.1:$port", "$dir/token");
check(str_contains($said, 'passed') && !str_contains($said, 'FAILED'), "through ai-api:\n$said");
foreach ([
    ['a wrong token', "http://127.0.0.1:$port", "$dir/wrong-token"],
    ['a token file that is not there', "http://127.0.0.1:$port", "$dir/none"],
    ['no sc.ai_token_file', "http://127.0.0.1:$port", ''],
    ['an ai-api that cannot be reached', 'http://127.0.0.1:1', "$dir/token"],
] as [$why, $url, $token_file]) {
    $said = $run('refused', $url, $token_file, $why);
    check(str_contains($said, 'passed') && !str_contains($said, 'FAILED'), "refused, $why:\n$said");
}

fclose($pipes[0]);
proc_close($server);
array_map('unlink', glob("$dir/*"));
rmdir($dir);
