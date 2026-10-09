<?php
declare(strict_types=1);

header_remove('X-Powered-By');
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
header('Pragma: no-cache');
header('Expires: 0');
header('X-Content-Type-Options: nosniff');
header('Access-Control-Allow-Origin: *');

define('GL_SECURITY_HMAC_SECRET', 'GL_SECRET_HANDSHAKE_KEY_2026_NCL_GAMELAND');

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    header('Allow: POST');
    echo json_encode(['success' => false, 'error' => 'method_not_allowed']);
    exit;
}

// Authentication handshake check
$secret = (string)($_POST['auth_secret'] ?? '');
if (!hash_equals(GL_SECURITY_HMAC_SECRET, $secret)) {
    http_response_code(403);
    echo json_encode(['success' => false, 'error' => 'forbidden']);
    exit;
}

function gregorianToJalaliDate(int $gy, int $gm, int $gd): array
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

function formatIranJalaliNow(): string
{
    $now = new DateTimeImmutable('now', new DateTimeZone('Asia/Tehran'));
    [$jy, $jm, $jd] = gregorianToJalaliDate((int)$now->format('Y'), (int)$now->format('n'), (int)$now->format('j'));
    return sprintf('%04d/%02d/%02d %s', $jy, $jm, $jd, $now->format('H:i:s'));
}

$clientIp = (string)($_SERVER['REMOTE_ADDR'] ?? 'unknown');
$deviceHash = strtoupper(trim((string)($_POST['device_hash'] ?? '')));
$deviceHash = (string)preg_replace('/[^A-F0-9]/', '', $deviceHash);
if (strlen($deviceHash) > 32) {
    $deviceHash = substr($deviceHash, 0, 32);
}

$clientType = strtolower(trim((string)($_POST['client_type'] ?? 'unknown')));
if (!in_array($clientType, ['home', 'gamenet'], true)) {
    $clientType = (strlen($deviceHash) === 24) ? 'home' : 'gamenet';
}

$tag = trim((string)($_POST['tag'] ?? ''));
if (strlen($tag) > 64) {
    $tag = substr($tag, 0, 64);
}

$playerTag = trim((string)($_POST['player_tag'] ?? ''));
if (strlen($playerTag) > 32) {
    $playerTag = substr($playerTag, 0, 32);
}

$phone = trim((string)($_POST['phone'] ?? ''));
if (strlen($phone) > 20) {
    $phone = substr($phone, 0, 20);
}

$computerName = trim((string)($_POST['computer_name'] ?? ''));
if (strlen($computerName) > 64) {
    $computerName = substr($computerName, 0, 64);
}

$userName = trim((string)($_POST['user_name'] ?? ''));
if (strlen($userName) > 64) {
    $userName = substr($userName, 0, 64);
}

$violationType = trim((string)($_POST['violation_type'] ?? 'نامشخص'));
if (strlen($violationType) > 128) {
    $violationType = substr($violationType, 0, 128);
}

$violationDetails = trim((string)($_POST['violation_details'] ?? ''));
if (strlen($violationDetails) > 2048) {
    $violationDetails = substr($violationDetails, 0, 2048);
}

$detectedAt = trim((string)($_POST['detected_at'] ?? ''));
if (strlen($detectedAt) > 64) {
    $detectedAt = substr($detectedAt, 0, 64);
}

$reportId = 'rep_' . bin2hex(random_bytes(6));
$jalaliTime = formatIranJalaliNow();
$timestamp = time();

$record = [
    'id' => $reportId,
    'timestamp' => $timestamp,
    'date_jalali' => $jalaliTime,
    'date_iso' => gmdate('c'),
    'ip' => $clientIp,
    'device_hash' => $deviceHash,
    'client_type' => $clientType,
    'tag' => $tag,
    'player_tag' => $playerTag,
    'phone' => $phone,
    'computer_name' => $computerName,
    'user_name' => $userName,
    'violation_type' => $violationType,
    'violation_details' => $violationDetails,
    'detected_at' => $detectedAt !== '' ? $detectedAt : $jalaliTime,
];

$dataFile = __DIR__ . '/security_reports.json';
$handle = @fopen($dataFile, 'c+');
if ($handle === false || !flock($handle, LOCK_EX)) {
    if (is_resource($handle)) {
        fclose($handle);
    }
    http_response_code(500);
    echo json_encode(['success' => false, 'error' => 'storage_unavailable']);
    exit;
}

try {
    $raw = stream_get_contents($handle);
    $reports = [];
    if (is_string($raw) && trim($raw) !== '') {
        $decoded = json_decode($raw, true);
        if (is_array($decoded)) {
            $reports = $decoded;
        }
    }

    // Deduplication check: ignore exact duplicate violation reported from same device within 30 seconds
    foreach (array_slice($reports, 0, 20) as $recent) {
        if (!is_array($recent)) continue;
        if (($recent['device_hash'] ?? '') === $deviceHash &&
            ($recent['violation_type'] ?? '') === $violationType &&
            ($recent['violation_details'] ?? '') === $violationDetails &&
            ($timestamp - (int)($recent['timestamp'] ?? 0)) < 30) {
            // Already received recently, acknowledge success without duplicating
            echo json_encode(['success' => true, 'id' => (string)($recent['id'] ?? $reportId), 'deduplicated' => true]);
            exit;
        }
    }

    // Prepend new report
    array_unshift($reports, $record);

    // Limit maximum retained reports to 1000
    if (count($reports) > 1000) {
        $reports = array_slice($reports, 0, 1000);
    }

    rewind($handle);
    ftruncate($handle, 0);
    fwrite($handle, json_encode($reports, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE) . "\n");
    fflush($handle);

    echo json_encode(['success' => true, 'id' => $reportId]);
} finally {
    flock($handle, LOCK_UN);
    fclose($handle);
}
