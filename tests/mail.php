<?php
// SC\Mail: an .eml built here and the Outlook .msg sc-mime's tests write (tests/resource/outlook.msg,
// from sc-mime's tests/resource/make_msg.py): headers, bodies, parts and their labels, attached messages.
require __DIR__ . '/check.inc';

$pdf = "%PDF-1.4\n" . str_repeat("\x00\xff", 100);
$eml = "From: =?UTF-8?Q?Ann_S=C3=A9n?= <ann@example.com>\r\n"
    . "To: Bob <bob@example.com>, carol@example.com\r\n"
    . "Cc: Dave <dave@example.com>\r\n"
    . "Subject: =?UTF-8?B?" . base64_encode('Résumé') . "?=\r\n"
    . "Date: Mon, 01 Jan 2024 00:00:00 +0000\r\n"
    . "Message-ID: <m1@example.com>\r\n"
    . "X-Thing: one\r\n"
    . "MIME-Version: 1.0\r\n"
    . "Content-Type: multipart/mixed; boundary=\"b1\"\r\n\r\n"
    . "--b1\r\nContent-Type: multipart/alternative; boundary=\"b2\"\r\n\r\n"
    . "--b2\r\nContent-Type: text/plain; charset=iso-8859-1\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\nCaf=E9\r\n"
    . "--b2\r\nContent-Type: text/html; charset=utf-8\r\n\r\n<p>Café</p>\r\n"
    . "--b2--\r\n"
    . "--b1\r\nContent-Type: application/pdf\r\nContent-Disposition: attachment; filename=\"a.pdf\"\r\nContent-Transfer-Encoding: base64\r\n\r\n"
    . chunk_split(base64_encode($pdf), 76, "\r\n")
    . "--b1\r\nContent-Type: message/rfc822\r\n\r\n"
    . "From: eve@example.com\r\nSubject: Inner\r\n\r\nInner text\r\n"
    . "--b1--\r\n";

$mail = SC\Mail::fromString($eml);
check($mail->subject() === 'Résumé', 'the subject, decoded');
check($mail->from() === [['name' => 'Ann Sén', 'address' => 'ann@example.com']], 'from');
check(array_column($mail->to(), 'address') === ['bob@example.com', 'carol@example.com'] && $mail->cc()[0]['name'] === 'Dave', 'to and cc');
check($mail->bcc() === [] && $mail->replyTo() === [], 'no bcc or reply-to');
check($mail->date() === 1704067200, 'the date');
check($mail->messageId() === 'm1@example.com', 'the message id without <>');
check($mail->header('x-thing') === 'one' && $mail->header('X-None') === null, 'a header, any case');
check(in_array(['name' => 'X-Thing', 'value' => 'one'], $mail->headers(), true), 'the headers');
check($mail->text() === "Café\n" || $mail->text() === 'Café', 'the text, in UTF-8');
check(str_contains($mail->html(), '<p>Café</p>'), 'the HTML');

$parts = $mail->parts();
check(array_column($parts, 'label') === ['1', '1.1', '1.1.1', '1.1.2', '1.2', '1.3', '1.3.1'], 'labels, as mailparse numbers them');
check($parts[0]['type'] === 'multipart/mixed' && $parts[0]['multipart'] && $parts[0]['size'] === 0, 'the top part');
check($parts[2]['charset'] === 'iso-8859-1' && $parts[2]['disposition'] === null && !$parts[2]['attachment'], 'a text part');
check($parts[4]['filename'] === 'a.pdf' && $parts[4]['disposition'] === 'attachment' && $parts[4]['size'] === strlen($pdf), 'the PDF');
check($parts[5]['message'] && $parts[5]['type'] === 'message/rfc822', 'the attached message');
check($mail->part('1.2') === $pdf, "a file's bytes, decoded");
check($mail->part('1.1.1') === "Caf\xe9\r\n" || $mail->part('1.1.1') === "Caf\xe9", 'text in its own charset');
check(str_starts_with($mail->partText('1.1.1'), 'Café'), 'text in UTF-8');
check(array_column($mail->attachments(), 'label') === ['1.2', '1.3'], 'the attachments: the PDF and the message, not what is in it');
$inner = $mail->message('1.3');
check($inner instanceof SC\Mail && $inner->subject() === 'Inner' && str_contains($inner->text(), 'Inner text'), 'the attached message as a Mail');
check(throws(fn() => $mail->part('9')), 'a label that is not a part');
check(throws(fn() => $mail->part('1')), 'a multipart has no content of its own');
check(throws(fn() => $mail->partText('1.2')), 'partText() of a PDF');
check(throws(fn() => $mail->message('1.2')), 'message() of a part that is not one');
check(throws(fn() => SC\Mail::fromString('')), 'no message');
check(throws(fn() => SC\Mail::open(__DIR__ . '/resource/none.eml')), 'no file');

$msg = SC\Mail::open(__DIR__ . '/resource/outlook.msg');
check($msg->subject() === 'Résumé review', 'a .msg: the subject');
check($msg->from()[0] === ['name' => 'Ann Smith', 'address' => 'ann@example.com'], 'its SMTP sender');
check($msg->to()[0]['address'] === 'carol@example.com' && $msg->cc()[0]['address'] === 'dave@example.com', 'its recipients');
check($msg->date() === 1704067200, 'when it was sent');
check(str_contains($msg->text(), 'Hello é world, as text.'), 'its text');
check(str_contains($msg->html(), '<p>Hello é world'), 'its HTML, out of the RTF');
$files = array_column($msg->attachments(), null, 'filename');
check(isset($files['notes.txt'], $files['logo.png'], $files['big.bin']), 'its attachments');
check($files['logo.png']['content_id'] === 'logo@example.com', 'an inline picture with its content id');
check($files['big.bin']['size'] === 5000 && $msg->part($files['big.bin']['label']) === implode(array_map(fn($i) => chr($i % 251), range(0, 4999))), 'a big attachment, byte for byte');
$attached = array_values(array_filter($msg->attachments(), fn($p) => $p['message']));
check(count($attached) === 1 && $msg->message($attached[0]['label'])->subject() === 'Inner message', 'its attached message');
check(SC\Mail::fromString(file_get_contents(__DIR__ . '/resource/outlook.msg'))->subject() === 'Résumé review', 'a .msg from a string');

$rtf = <<<'RTF'
{\rtf1\ansi\ansicpg1252 Hello \'e9\par World}
RTF;
check(str_contains(SC\Mail::rtfToHtml($rtf), 'Hello é') && str_contains(SC\Mail::rtfToHtml($rtf), 'World'), 'RTF as HTML');
