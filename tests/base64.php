<?php
// SC\base64_encode() and SC\base64_decode(): standard base64, and a mistake throws.
require __DIR__ . '/check.inc';

check(extension_loaded('sc'), 'the extension is loaded');
check(SC\base64_encode('Hello, PHP') === 'SGVsbG8sIFBIUA==', 'encode');
check(SC\base64_decode('SGVsbG8sIFBIUA==') === 'Hello, PHP', 'decode');
check(SC\base64_encode("\x00\xff") === base64_encode("\x00\xff"), 'binary, as PHP\'s own');
check(throws(fn() => SC\base64_decode('not base64!')), 'text that isn\'t base64 throws SC\Exception');
check(is_subclass_of(SC\Exception::class, Exception::class), 'SC\Exception is an Exception');
