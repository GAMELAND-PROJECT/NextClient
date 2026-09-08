<?php
declare(strict_types=1);
header('Content-Type: text/plain; charset=utf-8');
header('Cache-Control: no-store, max-age=0');
header('X-Content-Type-Options: nosniff');
function jalaliToGregorian(int $jy, int $jm, int $jd): array
{
    $jy += 1595;
    $days = -355668 + 365 * $jy + intdiv($jy, 33) * 8 + intdiv(($jy % 33) + 3, 4) + $jd;
    $days += $jm < 7 ? ($jm - 1) * 31 : ($jm - 7) * 30 + 186;
    $gy = 400 * intdiv($days, 146097);
    $days %= 146097;
    if ($days > 36524) {
        $gy += 100 * intdiv(--$days, 36524);
        $days %= 36524;
        if ($days >= 365) { ++$days; }
    }
    $gy += 4 * intdiv($days, 1461);
    $days %= 1461;
    if ($days > 365) {
        $gy += intdiv($days - 1, 365);
        $days = ($days - 1) % 365;
    }
    $leap = ($gy % 4 === 0 && $gy % 100 !== 0) || $gy % 400 === 0;
    $lengths = [31, $leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
    $gm = 1;
    while ($gm <= 12 && $days >= $lengths[$gm - 1]) {
        $days -= $lengths[$gm - 1];
        ++$gm;
    }
    return [$gy, $gm, $days + 1];
}

function gregorianToJalali(int $gy, int $gm, int $gd): array
{
    $offsets = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334];
    $gy2 = $gm > 2 ? $gy + 1 : $gy;
    $days = 355666 + 365 * $gy + intdiv($gy2 + 3, 4) - intdiv($gy2 + 99, 100) +
        intdiv($gy2 + 399, 400) + $gd + $offsets[$gm - 1];
    $jy = -1595 + 33 * intdiv($days, 12053);
    $days %= 12053;
    $jy += 4 * intdiv($days, 1461);
    $days %= 1461;
    if ($days > 365) {
        $jy += intdiv($days - 1, 365);
        $days = ($days - 1) % 365;
    }
    if ($days < 186) {
        return [$jy, 1 + intdiv($days, 31), 1 + ($days % 31)];
    }
    return [$jy, 7 + intdiv($days - 186, 30), 1 + (($days - 186) % 30)];
}

function validJalaliDate(string $value): bool
{
    if (!preg_match('/^(\d{4})\/(\d{2})\/(\d{2})$/', $value, $parts)) {
        return false;
    }
    [$year, $month, $day] = [(int)$parts[1], (int)$parts[2], (int)$parts[3]];
    if ($year < 1200 || $year > 1700 || $month < 1 || $month > 12 || $day < 1 ||
        ($month <= 6 && $day > 31) || ($month >= 7 && $month <= 11 && $day > 30) ||
        ($month === 12 && $day > 30)) {
        return false;
    }
    [$gy, $gm, $gd] = jalaliToGregorian($year, $month, $day);
    return gregorianToJalali($gy, $gm, $gd) === [$year, $month, $day];
}

$tag = (string)($_GET['tag'] ?? '');
if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'GET' ||
    !preg_match('/\\A[A-Za-z0-9_-]{1,64}\\z/', $tag)) {
    http_response_code(400);
    exit('DENIED');
}
$config = is_file(__DIR__ . '/admin/config.php') ? require __DIR__ . '/admin/config.php' : [];
$directory = is_array($config) ? (string)($config['data_dir'] ?? __DIR__) : __DIR__;
$rows = @file(rtrim($directory, '/\\\\') . '/client_tags.txt', FILE_IGNORE_NEW_LINES);
if ($rows === false) { http_response_code(503); exit('UNAVAILABLE'); }
$now = new DateTimeImmutable('now', new DateTimeZone('Asia/Tehran'));
$today = gregorianToJalali((int)$now->format('Y'), (int)$now->format('m'), (int)$now->format('d'));
$todayKey = $today[0] * 10000 + $today[1] * 100 + $today[2];
$matches = 0;
$allowed = false;
foreach ($rows as $line) {
    $parts = array_map('trim', explode('|', trim($line)));
    if (strcasecmp($parts[0], $tag) !== 0) { continue; }
    ++$matches;
    if (count($parts) !== 2 && count($parts) !== 3) { continue; }
    $expiry = $parts[count($parts) - 1];
    if (!validJalaliDate($expiry)) { continue; }
    $allowed = (int)str_replace('/', '', $expiry) >= $todayKey;
}
echo $matches === 1 && $allowed ? 'ACTIVE|' . $tag : 'DENIED';
