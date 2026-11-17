<?php
// SC\Bankserv: made-up load reports in the bank's layout (tests/resource/bankserv, the same as
// sc-bankserv's): a debit order file, a rejected one in the other carriage-control style, real-time
// files accepted and rejected, two statuses in one report; nothing throws on content.
require __DIR__ . '/check.inc';

$dir = __DIR__ . '/resource/bankserv';
$report = SC\Bankserv::loadReportFile("$dir/debit-orders.txt");
check($report === SC\Bankserv::loadReport(file_get_contents("$dir/debit-orders.txt")), 'from a file and from its contents: the same');
check($report['unparsed'] === [] && count($report['reports']) === 3, 'three reports, nothing unparsed');
check($report['outcome'] === ['realtime' => false, 'rejected' => false, 'user_code' => 'K999', 'generation' => '0123', 'received' => 3, 'accepted' => 2,
                              'rejected_count' => 1, 'adjusted' => 1], 'the outcome: ' . json_encode($report['outcome']));
[$header, $rejected, $accepted] = $report['reports'];
check($header['kind'] === 'header_trailer' && $header['run_date'] === '2026-03-02' && $header['number'] === '00101', 'the header and trailer report');
check($header['received'] === ['count' => 3, 'cents' => 250000] && $header['rejected'] === ['count' => 1, 'cents' => 100000], 'totals in cents');
check($header['data_set'] === 'DATA SET ACCEPTED' && $header['next_generation'] === '0124', 'the data set and the next generation');
$user = array_values(array_filter($header['fields'], fn($f) => $f['name'] === 'USER CODE'))[0];
check($user === ['name' => 'USER CODE', 'header' => 'K999', 'header_reason' => null, 'trailer' => 'K999', 'trailer_reason' => null], 'a field: ' . json_encode($user));
check($rejected['kind'] === 'transactions' && $rejected['status'] === 'REJECTED VALIDATION' && $rejected['batch'] === '0123', 'a transaction status');
$record = $rejected['records'][0];
check($record['sequence'] === 2 && $record['side'] === 'D' && $record['cents'] === 100000 && $record['action_date'] === '2026-03-02', 'a record');
check($record['own_kind'] === 'OWD' && $record['own_id'] === 1001 && $record['reject_code'] === '152' && $record['reject_description'] === 'HOM ACCT TYPE INVAL',
      "1web's id and the reject code");
check($record['contra_batch'] === null && $accepted['records'][1]['contra_batch'] === 123, 'the contra record');
check($rejected['summaries'][2] === ['level' => 'STATUS', 'key' => 'REJECTED VALIDATION', 'counts' => null, 'debit' => ['count' => 1, 'cents' => 100000],
                                     'credit' => ['count' => 0, 'cents' => 0]], 'a summary');

$other = SC\Bankserv::loadReportFile("$dir/rejected-file.txt");
check($other['outcome']['rejected'] === true, 'a rejected data set');
check($other['reports'][1]['records'][0]['name'] === "CAF\u{C9} OWNER", 'Windows-1252 as UTF-8');
check(count($other['unparsed']) === 1 && $other['unparsed'][0]['report'] === 1 && $other['unparsed'][0]['text'] === 'SOMETHING THE READER HAS NOT SEEN', 'an unknown line kept');

$realtime = SC\Bankserv::loadReportFile("$dir/realtime-rejected.txt");
check($realtime['outcome']['realtime'] && $realtime['outcome']['rejected'] && $realtime['outcome']['rejected_count'] === 1, 'a real-time file rejected');
check($realtime['reports'][0]['messages'][1] === ['label' => 'OVERALL FILE STATUS', 'text' => 'REJECTED'], 'its overall status');
check($realtime['reports'][1]['records'][0]['notes'] === ['ERROR:    12    34 INVALID REFERENCE VALIDATION'], 'an error under a record');

$two = SC\Bankserv::loadReportFile("$dir/two-statuses.txt");
check(array_column($two['reports'], 'status') === ['REJECTED ON LOAD', 'ACCEPTED ON LOAD'], 'two statuses in one report: two sections');

check(SC\Bankserv::loadReport('') === ['outcome' => ['realtime' => false, 'rejected' => false, 'user_code' => null, 'generation' => null, 'received' => 0, 'accepted' => 0,
                                                     'rejected_count' => 0, 'adjusted' => 0], 'reports' => [], 'unparsed' => [], 'lines' => 1], 'nothing');
$junk = SC\Bankserv::loadReport("not a report\n");
check(count($junk['unparsed']) === 1 && $junk['unparsed'][0]['report'] === null, 'not a report: kept as unparsed, nothing thrown');
check(throws(fn() => SC\Bankserv::loadReportFile("$dir/missing.txt")), "a file that isn't there throws");
