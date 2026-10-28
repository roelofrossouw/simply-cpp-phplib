<?php
// SC\BootstrapEmail::format(): Bootstrap HTML as email HTML, as the bootstrap-email gem makes it.
require __DIR__ . '/check.inc';

$html = SC\BootstrapEmail::format('<body class="bg-light"><div class="container"><div class="card"><div class="card-body">'
    . '<h1>Hello</h1><p>Café “quotes”</p><a class="btn btn-primary" href="https://example.com/?a=1&b=2">Open</a></div></div></div></body>');
check(str_starts_with($html, '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"'), 'a whole email page');
check(str_contains($html, '<meta name="viewport" content="width=device-width, initial-scale=1">'), 'with the meta tags emails need');
check(str_contains($html, 'class="btn btn-primary" role="presentation" border="0" cellpadding="0" cellspacing="0"'), 'the button as a table');
check(str_contains($html, 'bgcolor="#0D6EFD"'), "the button's colour also as bgcolor");
check(str_contains($html, 'Caf&#233; &#8220;quotes&#8221;'), 'text beyond ASCII as character references');
check(str_contains($html, 'href="https://example.com/?a=1&amp;b=2"'), 'links kept');
check(SC\BootstrapEmail::Format('<p>x</p>') === SC\BootstrapEmail::format('<p>x</p>'), 'Format() is format()');
