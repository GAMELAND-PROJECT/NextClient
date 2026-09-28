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
    $tagsFile = __DIR__ . '/client_tags.txt';
    if (!is_file($tagsFile)) {
        $tagsFile = __DIR__ . '/../client_tags.txt';
    }
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

function normalizeDeviceHash(string $hash): string
{
    $hash = strtoupper(trim($hash));
    $hash = (string)preg_replace('/[^A-F0-9]/', '', $hash);
    return substr($hash, 0, 24);
}

function normalizePhoneNumber(string $phone): string
{
    $phone = strtr($phone, [
        '۰' => '0', '۱' => '1', '۲' => '2', '۳' => '3', '۴' => '4',
        '۵' => '5', '۶' => '6', '۷' => '7', '۸' => '8', '۹' => '9',
        '٠' => '0', '١' => '1', '٢' => '2', '٣' => '3', '٤' => '4',
        '٥' => '5', '٦' => '6', '٧' => '7', '٨' => '8', '٩' => '9'
    ]);
    return (string)preg_replace('/[^0-9]/', '', $phone);
}

function jalaliToGregorianDate(int $jy, int $jm, int $jd): array
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

function jalaliRemainingDays(string $jalaliDate): int
{
    if (!preg_match('/^(\d{4})\/(\d{2})\/(\d{2})$/', $jalaliDate, $m)) {
        return -9999;
    }
    [$gy, $gm, $gd] = jalaliToGregorianDate((int)$m[1], (int)$m[2], (int)$m[3]);
    $expiryDt = new DateTimeImmutable(sprintf('%04d-%02d-%02d', $gy, $gm, $gd), new DateTimeZone('Asia/Tehran'));
    $todayDt = new DateTimeImmutable('today', new DateTimeZone('Asia/Tehran'));
    return (int)$todayDt->diff($expiryDt)->format('%r%a');
}

function loadHomeClientsData(): array
{
    $homeClientsFile = __DIR__ . '/home_clients.json';
    if (!is_file($homeClientsFile)) {
        $homeClientsFile = __DIR__ . '/../home_clients.json';
    }
    if (!is_file($homeClientsFile)) {
        return [];
    }
    $raw = file_get_contents($homeClientsFile);
    if (!is_string($raw) || trim($raw) === '') {
        return [];
    }
    $data = json_decode($raw, true);
    return is_array($data) ? $data : [];
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

if ($action === 'verify_home' || $action === 'check_home_subscription') {
    $rawHash = (string)($_GET['hash'] ?? '');
    $rawPhone = (string)($_GET['phone'] ?? '');

    $hash = normalizeDeviceHash($rawHash);
    $phone = normalizePhoneNumber($rawPhone);

    if (strlen($hash) !== 24) {
        echo json_encode([
            'valid' => false,
            'status' => 'invalid_hash',
            'error' => 'Device hash must be exactly 24 characters.'
        ], JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
        exit;
    }

    $clients = loadHomeClientsData();
    if (!isset($clients[$hash])) {
        echo json_encode([
            'valid' => false,
            'status' => 'not_registered',
            'hash' => $hash,
            'error' => 'این کد دستگاه در پنل مدیریت ثبت نشده است.'
        ], JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
        exit;
    }

    $record = $clients[$hash];
    $registeredPhone = normalizePhoneNumber((string)($record['phone'] ?? ''));

    // If client supplied a phone number, verify it matches
    if ($phone !== '' && $registeredPhone !== '' && $phone !== $registeredPhone) {
        echo json_encode([
            'valid' => false,
            'status' => 'phone_mismatch',
            'hash' => $hash,
            'phone' => $phone,
            'error' => 'شماره تلفن وارد شده با کد دستگاه همخوانی ندارد.'
        ], JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
        exit;
    }

    if (!empty($record['suspended'])) {
        echo json_encode([
            'valid' => false,
            'status' => 'suspended',
            'hash' => $hash,
            'phone' => $registeredPhone,
            'error' => 'اشتراک این دستگاه توسط مدیریت به حالت تعلیق درآمده است.'
        ], JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
        exit;
    }

    $expiry = (string)($record['expiry'] ?? '');
    $daysRemaining = jalaliRemainingDays($expiry);

    if ($daysRemaining < 0) {
        echo json_encode([
            'valid' => false,
            'status' => 'expired',
            'hash' => $hash,
            'phone' => $registeredPhone,
            'expiry' => $expiry,
            'days_remaining' => $daysRemaining,
            'error' => 'مدت زمان اشتراک این دستگاه به پایان رسیده است.'
        ], JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
        exit;
    }

    echo json_encode([
        'valid' => true,
        'status' => 'active',
        'hash' => $hash,
        'phone' => $registeredPhone,
        'expiry' => $expiry,
        'days_remaining' => $daysRemaining,
        'notes' => (string)($record['notes'] ?? ''),
    ], JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
    exit;
}

http_response_code(400);
echo json_encode(['success' => false, 'error' => 'unsupported_action']);
