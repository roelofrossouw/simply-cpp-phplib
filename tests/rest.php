<?php
// SC\Rest against a stand-in server (resource/http_server.py: HTTP, HTTPS, and HTTPS that wants a
// client certificate): methods, query parameters, JSON, form, XML and multipart bodies, headers,
// status, binary bodies, redirects, timeouts, TLS and client certificates.
require __DIR__ . '/check.inc';

$r = new SC\Rest('http://127.0.0.1:1/');
check($r->connectTimeout(2)->get() === '' && $r->status() === 0 && $r->error() !== '', 'no response: empty, status 0, error() says why');
check(throws(fn() => (new SC\Rest('http://x/'))->timeout(0)), 'a limit of 0 seconds refused');
check(throws(fn() => (new SC\Rest('http://x/'))->header("X-A\r\nX-B", '1')), 'a header name with a line break refused');
foreach (['file:///etc/hosts', 'file:///etc/hostname', 'ftp://127.0.0.1/x', 'gopher://127.0.0.1/x', 'dict://127.0.0.1/x'] as $url) {
    $other = new SC\Rest($url);
    check($other->get() === '' && $other->status() === 0 && str_contains($other->error(), 'rotocol'), "only http and https: $url refused");
}

if (!shell_exec('command -v openssl') || !shell_exec('command -v python3')) {
    echo "(no openssl or python3: the server checks didn't run)\n";
    return;
}
$dir = sys_get_temp_dir() . '/sc-phplib-rest-' . getmypid();
@mkdir($dir);
$certificate = function (string $name, string $key) use ($dir) {
    exec("openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -pkeyopt ec_param_enc:named_curve $key -days 1 -subj /CN=sc-phplib-$name"
        . " -addext basicConstraints=critical,CA:TRUE -addext subjectKeyIdentifier=hash -addext authorityKeyIdentifier=keyid:always"
        . " -addext keyUsage=critical,digitalSignature,keyCertSign -addext subjectAltName=IP:127.0.0.1"
        . " -keyout $dir/$name.key -out $dir/$name.crt > /dev/null 2>&1", $out, $status);
    return $status === 0;
};
check($certificate('server', '-nodes') && $certificate('client', '-passout pass:secret'), 'test certificates made');
file_put_contents("$dir/client.pem", file_get_contents("$dir/client.crt") . file_get_contents("$dir/client.key"));

$server = proc_open(['python3', __DIR__ . '/resource/http_server.py', "$dir/server.crt", "$dir/server.key", "$dir/client.crt"],
    [1 => ['pipe', 'w'], 2 => ['file', '/dev/null', 'w']], $pipes);
[$word, $http_port, $https_port, $mtls_port] = explode(' ', trim(fgets($pipes[1])));
register_shutdown_function(function () use ($server, $dir) {
    proc_terminate($server);
    array_map('unlink', glob("$dir/*"));
    @rmdir($dir);
});
check($word === 'listening', 'the stand-in server started');
$http = "http://127.0.0.1:$http_port";
$echo = fn(string $answer) => json_decode($answer, true);

// Methods and bodies
foreach (['get', 'head', 'delete'] as $method) {
    $r = new SC\Rest("$http/echo");
    $answer = $r->$method(['q' => 'a b']);
    check($r->status() === 200, "$method answered");
    if ($method !== 'head') check($echo($answer)['method'] === strtoupper($method) && $echo($answer)['query'] === 'q=a%20b', "$method with its query");
}
$r = new SC\Rest("$http/echo?fixed=1");
$sent = $echo($r->post(['name' => 'x', 'n' => 1], ['page' => 2]));
check($sent['method'] === 'POST' && $sent['query'] === 'fixed=1&page=2', 'a query added to the URL\'s own');
check(base64_decode($sent['body']) === '{"name":"x","n":1}' && $sent['content_type'] === 'application/json', 'an array posted as JSON');
foreach (['put', 'patch'] as $method) {
    $sent = $echo((new SC\Rest("$http/echo"))->$method('raw text'));
    check($sent['method'] === strtoupper($method) && base64_decode($sent['body']) === 'raw text', "$method a string as it is");
}
$form = new SC\Rest("$http/echo");
$sent = $echo($form->header('content-type', 'application/x-www-form-urlencoded')->post(['name' => 'Jo Soap', 'tags' => ['a', 'b'], 'on' => true, 'none' => null]));
check($sent['content_type'] === 'application/x-www-form-urlencoded', 'a form');
check(base64_decode($sent['body']) === http_build_query(['name' => 'Jo Soap', 'tags' => ['a', 'b'], 'on' => true, 'none' => null], '', '&', PHP_QUERY_RFC3986), 'encoded as http_build_query() does');
$soap = new SC\Rest("$http/echo");
$sent = $echo($soap->header('Content-Type', 'text/xml; charset=utf-8')->header('SOAPAction', '"urn:Ping"')->post('<Envelope/>'));
check($sent['content_type'] === 'text/xml; charset=utf-8' && $sent['headers']['soapaction'] === '"urn:Ping"', 'a SOAP 1.1 request\'s headers');
check(base64_decode($sent['body']) === '<Envelope/>', 'its envelope');
$bare = new SC\Rest("$http/echo");
check($echo($bare->header('Content-Type', '')->post('x'))['content_type'] === null, 'an empty header sends none');
$auth = new SC\Rest("$http/echo");
check($echo($auth->bearer('token-1')->bearer('token-2')->get())['headers']['authorization'] === 'Bearer token-2', 'bearer() replaces');

file_put_contents("$dir/upload.txt", 'file contents');
$upload = new SC\Rest("$http/echo");
$sent = $echo($upload->postMultipart(['note' => 'hi there'], ['doc' => ['path' => "$dir/upload.txt", 'filename' => 'report.txt', 'type' => 'text/plain'], 'raw' => "$dir/upload.txt"]));
$body = base64_decode($sent['body']);
check(str_starts_with($sent['content_type'], 'multipart/form-data; boundary='), 'multipart');
check(str_contains($body, 'name="note"') && str_contains($body, 'hi there') && str_contains($body, 'filename="report.txt"') && str_contains($body, 'file contents'), 'its fields and files');
check(throws(fn() => (new SC\Rest("$http/echo"))->postMultipart(['a' => ['nested']])), 'a nested field refused');
check(throws(fn() => (new SC\Rest("$http/echo"))->header('Content-Type', 'multipart/form-data')->post(['a' => 1])), 'an array body with a multipart Content-Type is postMultipart()\'s');

// The response
$gone = new SC\Rest("$http/status/404");
check($gone->get() === 'status 404' && $gone->status() === 404 && $gone->error() === '', 'a 404 is a response, with its body');
$binary = new SC\Rest("$http/binary");
$bytes = $binary->get();
check(strlen($bytes) === 256 && ord($bytes[255]) === 255, 'a binary body byte for byte');
check($binary->headers()['x-test'] === 'yes' && $binary->headers()['content-type'] === 'application/octet-stream', 'headers by lower-cased name');
check($binary->totalTime() > 0 && $binary->effectiveUrl() === "$http/binary", 'the time and the URL');

// Redirects and timeouts
$plain = new SC\Rest("$http/redirect/2");
$plain->get();
check($plain->status() === 302, 'redirects not followed unless asked');
$followed = new SC\Rest("$http/redirect/2");
check($echo($followed->followRedirects()->get())['path'] === '/echo' && $followed->effectiveUrl() === "$http/echo", 'followed, to where they led');
$to_file = new SC\Rest("$http/to-file");
$read = $to_file->followRedirects()->get();
check($to_file->status() === 0 && !str_contains($read, 'localhost') && str_contains($to_file->error(), 'rotocol'), 'a redirect to file:// refused, nothing read');
$limited = new SC\Rest("$http/redirect/4");
$limited->followRedirects(true, 2)->get();
check($limited->status() === 0 && $limited->error() !== '', 'more than max: no response');
$slow = new SC\Rest("$http/slow");
$started = microtime(true);
$slow->timeout(1)->get();
check($slow->status() === 0 && microtime(true) - $started < 3, 'a timeout');

// TLS
$https = "https://127.0.0.1:$https_port/echo";
$unknown = new SC\Rest($https);
$unknown->get();
check($unknown->status() === 0 && $unknown->error() !== '', 'a self-signed server refused');
check((new SC\Rest($https))->trust("$dir/server.crt")->get() !== '', 'trusted by its own certificate');
$by_name = new SC\Rest("https://localhost:$https_port/echo");
$by_name->trust("$dir/server.crt")->get();
check($by_name->status() === 0, 'the host name is checked');
$unchecked = new SC\Rest("https://localhost:$https_port/echo");
$unchecked->trust("$dir/server.crt")->verifyHost(false)->get();
check($unchecked->status() === 200, 'unless told not to');
$mtls = "https://127.0.0.1:$mtls_port/echo";
$anonymous = new SC\Rest($mtls);
$anonymous->trust("$dir/server.crt")->get();
check($anonymous->status() === 0, 'a server that wants a client certificate');
$client = new SC\Rest($mtls);
$client->trust("$dir/server.crt")->clientCertificate("$dir/client.crt", "$dir/client.key", 'secret')->get();
check($client->status() === 200, 'a client certificate with its encrypted key');
$one = new SC\Rest($mtls);
$one->trust("$dir/server.crt")->clientCertificate("$dir/client.pem", null, 'secret')->get();
check($one->status() === 200, 'certificate and key in one PEM');
$wrong = new SC\Rest($mtls);
$wrong->trust("$dir/server.crt")->clientCertificate("$dir/client.pem", null, 'wrong')->get();
check($wrong->status() === 0, 'a wrong passphrase');
