<?php
declare(strict_types=1);

header_remove('X-Powered-By');
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
header('Pragma: no-cache');
header('Expires: 0');
header('X-Content-Type-Options: nosniff');
header('Access-Control-Allow-Origin: *');

function verificationAttemptAllowed(): bool
{
    $clientAddress = (string)($_SERVER['REMOTE_ADDR'] ?? 'unknown');
    $rateFile = rtrim(sys_get_temp_dir(), DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR .
        'allclient-install-' . hash('sha256', $clientAddress) . '.rate';
    $handle = @fopen($rateFile, 'c+');
    if ($handle === false || !flock($handle, LOCK_EX)) {
        if (is_resource($handle)) {
            fclose($handle);
        }
        return true;
    }

    try {
        $raw = stream_get_contents($handle);
        $state = is_string($raw) ? json_decode($raw, true) : null;
        $windowStarted = is_array($state) ? (int)($state['window'] ?? 0) : 0;
        $attempts = is_array($state) ? (int)($state['attempts'] ?? 0) : 0;
        $now = time();
        if ($windowStarted <= 0 || $now - $windowStarted >= 300) {
            $windowStarted = $now;
            $attempts = 0;
        }
        if ($attempts >= 120) {
            return false;
        }

        ++$attempts;
        rewind($handle);
        ftruncate($handle, 0);
        fwrite($handle, json_encode(['window' => $windowStarted, 'attempts' => $attempts]));
        fflush($handle);
        return true;
    } finally {
        flock($handle, LOCK_UN);
        fclose($handle);
    }
}

if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    http_response_code(405);
    header('Allow: GET');
    echo json_encode(['success' => false, 'error' => 'method_not_allowed']);
    exit;
}

const ALLCLIENT_INSTALLER_ACCESS_INTERNAL = true;
$stateFile = __DIR__ . '/.installer_access.php';
$state = is_file($stateFile) ? require $stateFile : null;
$active = is_array($state) && !empty($state['active']) &&
    preg_match('/\A[a-f0-9]{64}\z/', (string)($state['code_hash'] ?? '')) === 1;
$action = strtolower(trim((string)($_GET['action'] ?? 'status')));

if ($action === 'status') {
    echo json_encode([
        'service' => 'allclient-access',
        'online' => true,
        'mode' => 'managed-one-time-code',
        'active' => $active,
    ], JSON_UNESCAPED_SLASHES);
    exit;
}

if ($action === 'verify') {
    if (!verificationAttemptAllowed()) {
        http_response_code(429);
        header('Retry-After: 300');
        echo json_encode(['valid' => false, 'error' => 'rate_limited']);
        exit;
    }
    $code = trim((string)($_GET['code'] ?? ''));
    $valid = $active && preg_match('/\A\d{8}\z/', $code) === 1 &&
        hash_equals((string)$state['code_hash'], hash('sha256', $code));
    echo json_encode(['valid' => $valid]);
    exit;
}

http_response_code(400);
echo json_encode(['success' => false, 'error' => 'unsupported_action']);
