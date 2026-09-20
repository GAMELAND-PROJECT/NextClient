<?php
declare(strict_types=1);

header_remove('X-Powered-By');
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
header('Pragma: no-cache');
header('Expires: 0');
header('X-Content-Type-Options: nosniff');
header('Access-Control-Allow-Origin: *');

function verificationAttemptAllowed(string $username): bool
{
    $clientAddress = (string)($_SERVER['REMOTE_ADDR'] ?? 'unknown');
    // Key limit by IP and username to prevent brute force
    $rateKey = hash('sha256', $clientAddress . '|' . strtolower($username));
    $rateFile = rtrim(sys_get_temp_dir(), DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR .
        'allclient-install-' . $rateKey . '.rate';
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
        if ($attempts >= 20) { // Limit attempts to 20 per 5 min per user/IP
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

function getSubscription(string $username): ?array
{
    $tagsFile = __DIR__ . '/../client_tags.txt';
    if (!is_file($tagsFile)) {
        return null;
    }
    $lines = file($tagsFile, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    if ($lines === false) {
        return null;
    }
    
    $usernameUpper = strtoupper(trim($username));
    foreach ($lines as $line) {
        $parts = array_map('trim', explode('|', $line));
        if (count($parts) >= 3 && strtoupper($parts[0]) === $usernameUpper) {
            return [
                'build' => $parts[0],
                'player' => $parts[1],
                'expiry' => $parts[2],
                'upload_password' => $parts[3] ?? '',
                'install_password' => $parts[4] ?? ''
            ];
        }
    }
    return null;
}

if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    http_response_code(405);
    header('Allow: GET');
    echo json_encode(['success' => false, 'error' => 'method_not_allowed']);
    exit;
}

$action = strtolower(trim((string)($_GET['action'] ?? 'status')));

if ($action === 'status') {
    echo json_encode([
        'service' => 'allclient-access',
        'online' => true,
        'mode' => 'managed-password',
        'active' => true,
    ], JSON_UNESCAPED_SLASHES);
    exit;
}

if ($action === 'verify') {
    $username = trim((string)($_GET['username'] ?? ''));
    $password = trim((string)($_GET['password'] ?? ''));
    
    if ($username === '') {
        echo json_encode(['valid' => false]);
        exit;
    }
    
    if (!verificationAttemptAllowed($username)) {
        http_response_code(429);
        header('Retry-After: 300');
        echo json_encode(['valid' => false, 'error' => 'rate_limited']);
        exit;
    }
    
    $sub = getSubscription($username);
    $valid = false;
    $downloadUrl = '';
    
    if ($sub !== null && $password !== '' && $sub['install_password'] === $password) {
        $valid = true;
        // Build the download URL pointing to license_api.php
        $protocol = (isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] === 'on') ? "https" : "http";
        $host = $_SERVER['HTTP_HOST'] ?? 'gameland.cam';
        $downloadUrl = $protocol . '://' . $host . '/license_api.php?tag=' . urlencode($sub['build']);
    }
    
    echo json_encode(['valid' => $valid, 'download_url' => $downloadUrl], JSON_UNESCAPED_SLASHES);
    exit;
}

http_response_code(400);
echo json_encode(['success' => false, 'error' => 'unsupported_action']);
