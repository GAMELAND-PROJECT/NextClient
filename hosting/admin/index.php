<?php
declare(strict_types=1);

const FILE_SERVERS = 'pinned_servers.txt';
const FILE_MIX_SERVERS = 'mix_servers.txt';
const FILE_HOME_CLIENTS = 'home_clients.json';
const FILE_SECURITY_REPORTS = 'security_reports.json';
const FILE_TAGS = 'client_tags.txt';
const FILE_PASSWORD = 'server_password.txt';
const FILE_FTP_CONFIG = 'ftp_config.txt';
const FILE_SUSPENDED_SUBSCRIPTIONS = '.suspended_subscriptions.php';
const FILE_UPDATES = 'updates.json';
const DIR_DOWNLOADS = 'downloads';
const MAX_SERVERS = 64;
const MAX_TAGS = 256;

header('X-Content-Type-Options: nosniff');
header('X-Frame-Options: DENY');
header('Referrer-Policy: no-referrer');
header("Content-Security-Policy: default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; worker-src 'self'; manifest-src 'self'; img-src 'self' data:; form-action 'self'; frame-ancestors 'none'; base-uri 'none'");
header('Cache-Control: no-store, max-age=0');

$configFile = __DIR__ . '/config.php';
$configExists = is_file($configFile);
$loadedConfig = $configExists ? require $configFile : null;
if ($configExists && !is_array($loadedConfig)) {
    http_response_code(503);
    exit('Panel configuration is invalid.');
}
$configured = is_array($loadedConfig) && !empty($loadedConfig['password_hash']) &&
    $loadedConfig['password_hash'] !== 'REPLACE_WITH_PASSWORD_HASH';
$config = $configured ? $loadedConfig : [
    'password_hash' => '',
    'data_dir' => dirname(__DIR__),
    'session_name' => 'allclient_admin',
    'session_idle_seconds' => 1800,
    'backup_limit' => 30,
];

$dataDir = realpath((string)($config['data_dir'] ?? ''));
if ($dataDir === false || !is_dir($dataDir) || !is_writable($dataDir)) {
    http_response_code(503);
    exit('Panel data directory is unavailable or not writable.');
}

session_name((string)($config['session_name'] ?? 'allclient_admin'));
session_set_cookie_params([
    'lifetime' => 0,
    'path' => '/',
    'secure' => !empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off',
    'httponly' => true,
    'samesite' => 'Strict',
]);
session_start();

$idleLimit = max(300, (int)($config['session_idle_seconds'] ?? 1800));
if (!empty($_SESSION['authenticated']) &&
    time() - (int)($_SESSION['last_activity'] ?? 0) > $idleLimit) {
    $_SESSION = [];
    session_regenerate_id(true);
}
if (!empty($_SESSION['authenticated'])) {
    $_SESSION['last_activity'] = time();
}

function escape(string $value): string
{
    return htmlspecialchars($value, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
}

function redirectHome(): never
{
    header('Location: ./', true, 303);
    exit;
}

function savePanelConfig(string $passwordHash): void
{
    global $configFile;
    $content = "<?php\ndeclare(strict_types=1);\n\nreturn [\n" .
        "    'password_hash' => " . var_export($passwordHash, true) . ",\n" .
        "    'data_dir' => dirname(__DIR__),\n" .
        "    'session_name' => 'allclient_admin',\n" .
        "    'session_idle_seconds' => 1800,\n" .
        "    'backup_limit' => 30,\n" .
        "];\n";
    $temporary = tempnam(__DIR__, '.config-');
    if ($temporary === false || file_put_contents($temporary, $content, LOCK_EX) === false) {
        throw new RuntimeException('نوشتن تنظیمات پنل ممکن نیست.');
    }
    @chmod($temporary, 0600);
    if (!rename($temporary, $configFile)) {
        @unlink($temporary);
        throw new RuntimeException('فعال‌سازی تنظیمات پنل ممکن نیست.');
    }
}

function normalizeAdminPassword(string $password): string
{
    return strtr($password, [
        '۰' => '0', '۱' => '1', '۲' => '2', '۳' => '3', '۴' => '4',
        '۵' => '5', '۶' => '6', '۷' => '7', '۸' => '8', '۹' => '9',
        '٠' => '0', '١' => '1', '٢' => '2', '٣' => '3', '٤' => '4',
        '٥' => '5', '٦' => '6', '٧' => '7', '٨' => '8', '٩' => '9',
    ]);
}

function validateNewAdminPassword(string $password, string $confirmation): string
{
    $password = normalizeAdminPassword($password);
    $confirmation = normalizeAdminPassword($confirmation);
    if ($password !== $confirmation) {
        throw new RuntimeException('تکرار رمز با رمز جدید یکسان نیست.');
    }
    if (!preg_match('/\A.{8}\z/us', $password)) {
        throw new RuntimeException('رمز مدیریت باید دقیقاً ۸ کاراکتر باشد؛ حروف و اعداد مجاز هستند.');
    }
    return $password;
}

function csrfToken(): string
{
    if (empty($_SESSION['csrf'])) {
        $_SESSION['csrf'] = bin2hex(random_bytes(32));
    }
    return (string)$_SESSION['csrf'];
}

function requireValidCsrf(): void
{
    $provided = (string)($_POST['csrf'] ?? '');
    if ($provided === '' || !hash_equals(csrfToken(), $provided)) {
        throw new RuntimeException('درخواست منقضی یا نامعتبر است. صفحه را تازه‌سازی کنید.');
    }
}

function flash(string $type, string $message): void
{
    $_SESSION['flash'] = ['type' => $type, 'message' => $message];
}

function takeFlash(): ?array
{
    $message = $_SESSION['flash'] ?? null;
    unset($_SESSION['flash']);
    return is_array($message) ? $message : null;
}

function dataPath(string $name): string
{
    global $dataDir;
    return $dataDir . DIRECTORY_SEPARATOR . $name;
}

function readTextFile(string $name, int $maxBytes = 131072): string
{
    $path = dataPath($name);
    if (!is_file($path)) {
        return '';
    }
    $size = filesize($path);
    if ($size === false || $size > $maxBytes) {
        throw new RuntimeException("فایل {$name} بیش از اندازه بزرگ یا غیرقابل خواندن است.");
    }
    $content = file_get_contents($path);
    if ($content === false) {
        throw new RuntimeException("خواندن فایل {$name} ممکن نیست.");
    }
    return str_replace(["\r\n", "\r"], "\n", $content);
}

function backupAndAtomicWrite(string $name, string $content): void
{
    global $config;
    $backupDir = __DIR__ . '/backups';
    if (!is_dir($backupDir) && !mkdir($backupDir, 0700, true) && !is_dir($backupDir)) {
        throw new RuntimeException('ساخت پوشه پشتیبان ممکن نیست.');
    }

    $denyFile = $backupDir . '/.htaccess';
    if (!is_file($denyFile)) {
        file_put_contents($denyFile, "Require all denied\n", LOCK_EX);
    }

    $lock = fopen($backupDir . '/admin.lock', 'c');
    if ($lock === false || !flock($lock, LOCK_EX)) {
        throw new RuntimeException('قفل‌کردن فایل برای ذخیره امن ممکن نیست.');
    }

    try {
        $target = dataPath($name);
        if (is_file($target)) {
            $stamp = gmdate('Ymd-His') . '-' . bin2hex(random_bytes(3));
            if (!copy($target, $backupDir . '/' . $name . '.' . $stamp . '.bak')) {
                throw new RuntimeException('ساخت نسخه پشتیبان ممکن نیست.');
            }
        }

        $temporary = tempnam(dirname($target), '.allclient-');
        if ($temporary === false || file_put_contents($temporary, $content, LOCK_EX) === false) {
            throw new RuntimeException('نوشتن فایل موقت ممکن نیست.');
        }
        @chmod($temporary, 0644);
        if (!rename($temporary, $target)) {
            @unlink($temporary);
            throw new RuntimeException('جایگزینی اتمیک فایل ممکن نیست.');
        }

        $limit = max(5, (int)($config['backup_limit'] ?? 30));
        $backups = glob($backupDir . '/' . $name . '.*.bak') ?: [];
        usort($backups, static fn(string $a, string $b): int => filemtime($b) <=> filemtime($a));
        foreach (array_slice($backups, $limit) as $oldBackup) {
            @unlink($oldBackup);
        }
    } finally {
        flock($lock, LOCK_UN);
        fclose($lock);
    }
}

function normalizedLines(string $value): array
{
    $value = str_replace(["\r\n", "\r"], "\n", $value);
    return array_values(array_filter(array_map('trim', explode("\n", $value)),
        static fn(string $line): bool => $line !== '' && !str_starts_with($line, '#')));
}

function validateServers(string $input): array
{
    $lines = normalizedLines($input);
    if (count($lines) > MAX_SERVERS) {
        throw new RuntimeException('حداکثر ۶۴ سرور قابل ثبت است.');
    }

    $result = [];
    foreach ($lines as $index => $line) {
        if (!preg_match('/^([^:\s]+):(\d{1,5})$/', $line, $matches)) {
            throw new RuntimeException('فرمت سرور در خط ' . ($index + 1) . ' باید IP:PORT باشد.');
        }
        $ip = $matches[1];
        $port = (int)$matches[2];
        if (filter_var($ip, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4) === false || $port < 1 || $port > 65535) {
            throw new RuntimeException('IP یا پورت سرور در خط ' . ($index + 1) . ' نامعتبر است.');
        }
        $endpoint = $ip . ':' . $port;
        $result[$endpoint] = $endpoint;
    }
    return array_values($result);
}

function readMixServersText(): string
{
    return implode("\n", validateServers(readTextFile(FILE_MIX_SERVERS)));
}

function sendPanelBackup(): never
{
    $backup = [
        'format' => 'allclient-admin-backup',
        'version' => 3,
        'created_at' => gmdate('c'),
        'public_servers' => normalizedLines(readTextFile(FILE_SERVERS)),
        'mix_servers' => normalizedLines(readMixServersText()),
        'client_tags' => normalizedLines(readTextFile(FILE_TAGS)),
        'server_password' => trim(readTextFile(FILE_PASSWORD)),
        'suspended_subscriptions' => readSuspendedSubscriptionRows(),
        'home_clients' => readHomeClients(),
        'security_reports' => readSecurityReports(),
    ];

    header('Content-Type: application/json; charset=UTF-8');
    header('Content-Disposition: attachment; filename="allclient-panel-backup-' . gmdate('Ymd-His') . '.json"');
    echo json_encode($backup, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
    exit;
}

function restorePanelBackup(array $upload): void
{
    if (($upload['error'] ?? UPLOAD_ERR_NO_FILE) !== UPLOAD_ERR_OK || !is_uploaded_file((string)$upload['tmp_name'])) {
        throw new RuntimeException('Backup file was not uploaded correctly.');
    }
    if ((int)($upload['size'] ?? 0) > 1024 * 1024) {
        throw new RuntimeException('Backup file is too large.');
    }

    $payload = json_decode((string)file_get_contents((string)$upload['tmp_name']), true);
    if (!is_array($payload) || ($payload['format'] ?? '') !== 'allclient-admin-backup') {
        throw new RuntimeException('Backup file format is invalid.');
    }

    $publicServers = validateServers(implode("\n", (array)($payload['public_servers'] ?? [])));
    $mixServers = validateServers(implode("\n", (array)($payload['mix_servers'] ?? [])));
    $tagRows = validateTagsFromLines((array)($payload['client_tags'] ?? []));
    $password = trim((string)($payload['server_password'] ?? ''));
    if ($password !== '' && !preg_match('/^[A-Za-z0-9_!@#$%^&*.\-]{1,31}$/', $password)) {
        throw new RuntimeException('Server password in backup is invalid.');
    }

    backupAndAtomicWrite(FILE_SERVERS, implode("\n", $publicServers) . ($publicServers ? "\n" : ''));
    backupAndAtomicWrite(FILE_MIX_SERVERS, implode("\n", $mixServers) . ($mixServers ? "\n" : ''));
    backupAndAtomicWrite(FILE_TAGS, implode("\n", $tagRows) . ($tagRows ? "\n" : ''));
    backupAndAtomicWrite(FILE_PASSWORD, $password . ($password !== '' ? "\n" : ''));

    $rows = [];
    foreach (parseTagRows(readTextFile(FILE_TAGS)) as $row) {
        $rows[strtoupper($row['build'])] = $row;
    }
    foreach ((array)($payload['suspended_subscriptions'] ?? []) as $row) {
        if (!is_array($row)) { continue; }
        $validated = validateTags([(string)($row['build'] ?? '')], [(string)($row['player'] ?? '')],
            [(string)($row['expiry'] ?? '')], [(string)($row['upload_password'] ?? '')],
            [(string)($row['install_password'] ?? '')]);
        if (!$validated) { continue; }
        $parts = array_map('trim', explode('|', $validated[0]));
        $build = $parts[0]; $player = $parts[1]; $expiry = $parts[2];
        $uploadPassword = $parts[3] ?? ''; $installPassword = $parts[4] ?? '';
        $rows[strtoupper($build)] = ['build' => $build, 'player' => $player, 'expiry' => $expiry,
            'upload_password' => $uploadPassword, 'install_password' => $installPassword, 'suspended' => true];
    }
    writeSubscriptionRows($rows);

    // Restore home clients if present in backup
    if (isset($payload['home_clients']) && is_array($payload['home_clients'])) {
        $restoredHome = [];
        foreach ($payload['home_clients'] as $hashKey => $rec) {
            if (!is_array($rec)) { continue; }
            $normHash = normalizeDeviceHash((string)($rec['hash'] ?? $hashKey));
            if (strlen($normHash) === 24) {
                $restoredHome[$normHash] = [
                    'hash' => $normHash,
                    'phone' => normalizePhoneNumber((string)($rec['phone'] ?? '')),
                    'expiry' => trim((string)($rec['expiry'] ?? '')),
                    'notes' => trim((string)($rec['notes'] ?? '')),
                    'created_at' => (string)($rec['created_at'] ?? ''),
                    'suspended' => !empty($rec['suspended']),
                ];
            }
        }
        writeHomeClients($restoredHome);
    }

    // Restore security reports if present in backup
    if (isset($payload['security_reports']) && is_array($payload['security_reports'])) {
        $restoredReports = [];
        foreach ($payload['security_reports'] as $rep) {
            if (is_array($rep) && !empty($rep['id'])) {
                $restoredReports[] = $rep;
            }
        }
        writeSecurityReports($restoredReports);
    }
}

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

function validateTags(array $buildTags, array $playerTags, array $expiries, array $uploadPasswords = [], array $installPasswords = []): array
{
    $rowCount = max(count($buildTags), count($playerTags), count($expiries), count($uploadPasswords), count($installPasswords));
    if ($rowCount > MAX_TAGS) {
        throw new RuntimeException('تعداد گیمنت‌ها بیش از حد مجاز است.');
    }

    $result = [];
    for ($index = 0; $index < $rowCount; ++$index) {
        $build = trim((string)($buildTags[$index] ?? ''));
        $player = trim((string)($playerTags[$index] ?? ''));
        $expiry = trim((string)($expiries[$index] ?? ''));
        $uploadPassword = trim((string)($uploadPasswords[$index] ?? ''));
        $installPassword = trim((string)($installPasswords[$index] ?? ''));
        if ($build === '' && $player === '' && $expiry === '' && $uploadPassword === '' && $installPassword === '') { continue; }
        if (!preg_match('/^[A-Za-z0-9_-]{1,64}$/', $build)) {
            throw new RuntimeException('تگ Build در ردیف ' . ($index + 1) . ' نامعتبر است.');
        }
        if (!preg_match('/^[A-Za-z0-9_-]{1,12}$/', $player)) {
            throw new RuntimeException('تگ نام بازیکن در ردیف ' . ($index + 1) . ' نامعتبر است.');
        }
        if (!validJalaliDate($expiry)) {
            throw new RuntimeException('تاریخ شمسی ردیف ' . ($index + 1) . ' نامعتبر است؛ نمونه: 1405/06/25');
        }
        if ($uploadPassword !== '' && !preg_match('/^[A-Za-z0-9_!@#$%^&*.\-]{1,31}$/', $uploadPassword)) {
            throw new RuntimeException('Upload password in row ' . ($index + 1) . ' is invalid.');
        }
        if ($installPassword !== '' && !preg_match('/^[A-Za-z0-9_!@#$%^&*.\-]{1,31}$/', $installPassword)) {
            throw new RuntimeException('Install password in row ' . ($index + 1) . ' is invalid.');
        }
        $key = strtoupper($build);
        if (isset($result[$key])) {
            throw new RuntimeException('تگ Build تکراری است: ' . $build);
        }
        $result[$key] = $build . ' | ' . $player . ' | ' . $expiry . ' | ' . $uploadPassword . ' | ' . $installPassword;
    }
    return array_values($result);
}

function validateTagsFromLines(array $lines): array
{
    $buildTags = [];
    $playerTags = [];
    $expiries = [];
    $uploadPasswords = [];
    $installPasswords = [];
    foreach ($lines as $line) {
        $parts = array_map('trim', explode('|', (string)$line));
        if (count($parts) < 3 || count($parts) > 5) {
            throw new RuntimeException('Subscription row in backup is invalid.');
        }
        $buildTags[] = $parts[0];
        $playerTags[] = $parts[1];
        $expiries[] = $parts[2];
        $uploadPasswords[] = $parts[3] ?? '';
        $installPasswords[] = $parts[4] ?? '';
    }
    return validateTags($buildTags, $playerTags, $expiries, $uploadPasswords, $installPasswords);
}

function parseTagRows(string $content): array
{
    $rows = [];
    foreach (normalizedLines($content) as $line) {
        $parts = array_map('trim', explode('|', $line));
        if (count($parts) >= 3 && count($parts) <= 5) {
            $rows[] = ['build' => $parts[0], 'player' => $parts[1],
                'expiry' => $parts[2], 'upload_password' => $parts[3] ?? '', 'install_password' => $parts[4] ?? '', 'suspended' => false];
        }
    }
    return $rows;
}

function readSuspendedSubscriptionRows(): array
{
    $path = dataPath(FILE_SUSPENDED_SUBSCRIPTIONS);
    if (!is_file($path)) {
        return [];
    }
    if (!defined('ALLCLIENT_SUBSCRIPTIONS_INTERNAL')) {
        define('ALLCLIENT_SUBSCRIPTIONS_INTERNAL', true);
    }
    $stored = require $path;
    if (!is_array($stored)) {
        throw new RuntimeException('فایل اشتراک‌های معلق معتبر نیست.');
    }

    $rows = [];
    foreach ($stored as $row) {
        if (!is_array($row)) { continue; }
        $validated = validateTags([(string)($row['build'] ?? '')],
            [(string)($row['player'] ?? '')], [(string)($row['expiry'] ?? '')],
            [(string)($row['upload_password'] ?? '')], [(string)($row['install_password'] ?? '')]);
        if (!$validated) { continue; }
        $parts = array_map('trim', explode('|', $validated[0]));
        $build = $parts[0]; $player = $parts[1]; $expiry = $parts[2];
        $uploadPassword = $parts[3] ?? ''; $installPassword = $parts[4] ?? '';
        $rows[] = ['build' => $build, 'player' => $player,
            'expiry' => $expiry, 'upload_password' => $uploadPassword, 'install_password' => $installPassword, 'suspended' => true];
    }
    return $rows;
}

function normalizeDeviceHash(string $hash): string
{
    $hash = strtoupper(trim($hash));
    $hash = (string)preg_replace('/[^A-F0-9]/', '', $hash);
    return substr($hash, 0, 24);
}

function normalizePhoneNumber(string $phone): string
{
    $phone = normalizeAdminPassword($phone);
    return (string)preg_replace('/[^0-9]/', '', $phone);
}

function readHomeClients(): array
{
    $path = dataPath(FILE_HOME_CLIENTS);
    if (!is_file($path)) {
        return [];
    }
    $raw = file_get_contents($path);
    if (!is_string($raw) || trim($raw) === '') {
        return [];
    }
    $data = json_decode($raw, true);
    return is_array($data) ? $data : [];
}

function writeHomeClients(array $clients): void
{
    backupAndAtomicWrite(FILE_HOME_CLIENTS, json_encode($clients, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE) . "\n");
}

function readSecurityReports(): array
{
    $path = dataPath(FILE_SECURITY_REPORTS);
    if (!is_file($path)) {
        return [];
    }
    $raw = file_get_contents($path);
    if (!is_string($raw) || trim($raw) === '') {
        return [];
    }
    $data = json_decode($raw, true);
    return is_array($data) ? $data : [];
}

function writeSecurityReports(array $reports): void
{
    backupAndAtomicWrite(FILE_SECURITY_REPORTS, json_encode($reports, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE) . "\n");
}

function decorateHomeClientRows(array $clients): array
{
    $rows = [];
    $today = iranToday();
    foreach ($clients as $hash => $data) {
        if (!is_array($data)) continue;
        $row = $data;
        $row['hash'] = (string)$hash;
        $expiry = (string)($data['expiry'] ?? '');
        $row['days_remaining'] = -9999;
        if (validJalaliDate($expiry)) {
            $row['days_remaining'] = (int)$today->diff(jalaliDateToDateTime($expiry))->format('%r%a');
        }
        if (!empty($data['suspended'])) {
            $row['state'] = 'suspended';
            $row['state_label'] = 'معلق';
        } elseif ($row['days_remaining'] < 0) {
            $row['state'] = 'expired';
            $row['state_label'] = 'منقضی';
        } elseif ($row['days_remaining'] <= 3) {
            $row['state'] = 'expiring';
            $row['state_label'] = 'رو به اتمام';
        } else {
            $row['state'] = 'active';
            $row['state_label'] = 'فعال';
        }
        $rows[] = $row;
    }
    usort($rows, function ($a, $b) {
        return $a['days_remaining'] <=> $b['days_remaining'];
    });
    return $rows;
}

function subscriptionRowsByKey(): array
{
    $rows = [];
    foreach (readSuspendedSubscriptionRows() as $row) {
        $rows[strtoupper($row['build'])] = $row;
    }
    // Active records take precedence if a previous interrupted write left a duplicate.
    foreach (parseTagRows(readTextFile(FILE_TAGS)) as $row) {
        $rows[strtoupper($row['build'])] = $row;
    }
    return $rows;
}

function writeSubscriptionRows(array $rows): void
{
    $active = [];
    $suspended = [];
    foreach ($rows as $row) {
        $validated = validateTags([(string)($row['build'] ?? '')],
            [(string)($row['player'] ?? '')], [(string)($row['expiry'] ?? '')],
            [(string)($row['upload_password'] ?? '')], [(string)($row['install_password'] ?? '')]);
        if (!$validated) { continue; }
        if (!empty($row['suspended'])) {
            $suspended[] = [
                'build' => (string)$row['build'],
                'player' => (string)$row['player'],
                'expiry' => (string)$row['expiry'],
                'upload_password' => (string)($row['upload_password'] ?? ''),
                'install_password' => (string)($row['install_password'] ?? ''),
            ];
        } else {
            $active[] = $validated[0];
        }
    }

    $state = "<?php\ndeclare(strict_types=1);\n\n" .
        "if (!defined('ALLCLIENT_SUBSCRIPTIONS_INTERNAL')) {\n" .
        "    http_response_code(404);\n    exit;\n}\n\nreturn " .
        var_export($suspended, true) . ";\n";
    backupAndAtomicWrite(FILE_SUSPENDED_SUBSCRIPTIONS, $state);
    @chmod(dataPath(FILE_SUSPENDED_SUBSCRIPTIONS), 0600);
    backupAndAtomicWrite(FILE_TAGS, implode("\n", $active) . ($active ? "\n" : ''));
}

function readUpdatesData(): array
{
    $path = dataPath(FILE_UPDATES);
    if (!is_file($path)) {
        return ['home' => [], 'gamenet' => []];
    }
    $content = file_get_contents($path);
    if ($content === false) {
        return ['home' => [], 'gamenet' => []];
    }
    $decoded = json_decode($content, true);
    if (!is_array($decoded)) {
        return ['home' => [], 'gamenet' => []];
    }

    if (isset($decoded['home']) || isset($decoded['gamenet'])) {
        return [
            'home' => is_array($decoded['home'] ?? null) ? $decoded['home'] : [],
            'gamenet' => is_array($decoded['gamenet'] ?? null) ? $decoded['gamenet'] : []
        ];
    }

    $normalized = ['home' => [], 'gamenet' => []];
    foreach ($decoded as $key => $val) {
        if (!is_array($val)) continue;
        $ed = strtolower((string)($val['edition'] ?? ''));
        if ($ed === 'home' || strcasecmp((string)$key, 'HOME') === 0) {
            $normalized['home'][(string)$key] = $val;
        } else {
            $normalized['gamenet'][(string)$key] = $val;
        }
    }
    return $normalized;
}

function writeUpdatesData(array $data): void
{
    $content = json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
    backupAndAtomicWrite(FILE_UPDATES, $content !== false ? $content : '{"home":{},"gamenet":{}}');
}


function iranToday(): DateTimeImmutable
{
    return new DateTimeImmutable('today', new DateTimeZone('Asia/Tehran'));
}

function jalaliDateToDateTime(string $value): DateTimeImmutable
{
    if (!validJalaliDate($value)) {
        throw new RuntimeException('تاریخ شمسی اشتراک معتبر نیست.');
    }
    [$jy, $jm, $jd] = array_map('intval', explode('/', $value));
    [$gy, $gm, $gd] = jalaliToGregorian($jy, $jm, $jd);
    return (new DateTimeImmutable('now', new DateTimeZone('Asia/Tehran')))
        ->setDate($gy, $gm, $gd)->setTime(0, 0);
}

function dateTimeToJalali(DateTimeImmutable $date): string
{
    [$jy, $jm, $jd] = gregorianToJalali((int)$date->format('Y'),
        (int)$date->format('n'), (int)$date->format('j'));
    return sprintf('%04d/%02d/%02d', $jy, $jm, $jd);
}

function jalaliMonthLength(int $year, int $month): int
{
    if ($month <= 6) { return 31; }
    if ($month <= 11) { return 30; }
    return validJalaliDate(sprintf('%04d/12/30', $year)) ? 30 : 29;
}

function addJalaliMonths(string $value, int $months): string
{
    [$year, $month, $day] = array_map('intval', explode('/', $value));
    $monthIndex = $year * 12 + ($month - 1) + $months;
    $targetYear = intdiv($monthIndex, 12);
    $targetMonth = $monthIndex % 12 + 1;
    $targetDay = min($day, jalaliMonthLength($targetYear, $targetMonth));
    return sprintf('%04d/%02d/%02d', $targetYear, $targetMonth, $targetDay);
}

function extendSubscriptionExpiry(string $expiry, int $months, int $days): string
{
    $today = iranToday();
    $current = jalaliDateToDateTime($expiry);
    $base = $current < $today ? $today : $current;
    $result = dateTimeToJalali($base);
    if ($months > 0) {
        $result = addJalaliMonths($result, $months);
    }
    if ($days > 0) {
        $result = dateTimeToJalali(jalaliDateToDateTime($result)->modify('+' . $days . ' days'));
    }
    return $result;
}

function decorateSubscriptionRow(array $row): array
{
    $days = (int)iranToday()->diff(jalaliDateToDateTime((string)$row['expiry']))->format('%r%a');
    $row['days_remaining'] = $days;
    if (!empty($row['suspended'])) {
        $row['state'] = 'suspended';
        $row['state_label'] = 'معلق';
    } elseif ($days < 0) {
        $row['state'] = 'expired';
        $row['state_label'] = 'منقضی';
    } elseif ($days <= 7) {
        $row['state'] = 'urgent';
        $row['state_label'] = 'رو به پایان';
    } else {
        $row['state'] = 'active';
        $row['state_label'] = 'فعال';
    }
    return $row;
}


$action = (string)($_POST['action'] ?? '');
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    try {
        if ($action === 'setup') {
            requireValidCsrf();
            if ($configured) {
                throw new RuntimeException('راه‌اندازی اولیه قبلاً انجام شده است.');
            }
            $newPassword = validateNewAdminPassword((string)($_POST['new_password'] ?? ''),
                (string)($_POST['confirm_password'] ?? ''));
            savePanelConfig(password_hash($newPassword, PASSWORD_DEFAULT));
            session_regenerate_id(true);
            $_SESSION['authenticated'] = true;
            $_SESSION['last_activity'] = time();
            flash('success', 'رمز مدیریت ثبت و راه‌اندازی اولیه برای همیشه قفل شد.');
            redirectHome();
        }

        if ($action === 'login') {
            requireValidCsrf();
            $attempts = (int)($_SESSION['login_attempts'] ?? 0);
            $blockedUntil = (int)($_SESSION['blocked_until'] ?? 0);
            if (time() < $blockedUntil) {
                throw new RuntimeException('تلاش‌های ناموفق زیاد است؛ کمی بعد دوباره امتحان کنید.');
            }
            if (!password_verify(normalizeAdminPassword((string)($_POST['password'] ?? '')),
                (string)$config['password_hash'])) {
                $_SESSION['login_attempts'] = ++$attempts;
                if ($attempts >= 5) { $_SESSION['blocked_until'] = time() + 300; }
                throw new RuntimeException('رمز ورود صحیح نیست.');
            }
            session_regenerate_id(true);
            $_SESSION['authenticated'] = true;
            $_SESSION['last_activity'] = time();
            $_SESSION['login_attempts'] = 0;
            unset($_SESSION['blocked_until']);
            flash('success', 'ورود موفق بود.');
            redirectHome();
        }

        if ($action === 'logout') {
            requireValidCsrf();
            $_SESSION = [];
            session_regenerate_id(true);
            redirectHome();
        }
        if (empty($_SESSION['authenticated'])) {
            throw new RuntimeException('نشست شما منقضی شده است.');
        }
        requireValidCsrf();

        if ($action === 'change_admin_password') {
            $currentPassword = normalizeAdminPassword((string)($_POST['current_password'] ?? ''));
            if (!password_verify($currentPassword, (string)$config['password_hash'])) {
                throw new RuntimeException('رمز فعلی صحیح نیست.');
            }
            $newPassword = validateNewAdminPassword((string)($_POST['new_password'] ?? ''),
                (string)($_POST['confirm_password'] ?? ''));
            if (password_verify($newPassword, (string)$config['password_hash'])) {
                throw new RuntimeException('رمز جدید باید با رمز فعلی متفاوت باشد.');
            }
            savePanelConfig(password_hash($newPassword, PASSWORD_DEFAULT));
            session_regenerate_id(true);
            $_SESSION['authenticated'] = true;
            $_SESSION['last_activity'] = time();
            flash('success', 'رمز مدیریت با موفقیت تغییر کرد.');
        } elseif ($action === 'save_servers') {
            $servers = validateServers((string)($_POST['servers'] ?? ''));
            backupAndAtomicWrite(FILE_SERVERS, implode("\n", $servers) . ($servers ? "\n" : ''));
            flash('success', count($servers) . ' سرور با موفقیت ذخیره شد.');
        } elseif ($action === 'save_mix_servers') {
            $servers = validateServers((string)($_POST['mix_servers'] ?? ''));
            backupAndAtomicWrite(FILE_MIX_SERVERS, implode("\n", $servers) . ($servers ? "\n" : ''));
            flash('success', count($servers) . ' Mix servers saved successfully.');
        } elseif ($action === 'save_ftp_config') {
            $ftpHost = trim((string)($_POST['ftp_host'] ?? ''));
            $ftpUser = trim((string)($_POST['ftp_user'] ?? ''));
            $ftpPass = trim((string)($_POST['ftp_pass'] ?? ''));
            $ftpPath = trim((string)($_POST['ftp_path'] ?? ''));
            $content = $ftpHost . "\n" . $ftpUser . "\n" . $ftpPass . "\n" . $ftpPath . "\n";
            backupAndAtomicWrite(FILE_FTP_CONFIG, $content);
            flash('success', 'تنظیمات سرور دانلود (FTP) ذخیره شد.');
        } elseif ($action === 'download_backup') {
            sendPanelBackup();
        } elseif ($action === 'restore_backup') {
            restorePanelBackup((array)($_FILES['panel_backup'] ?? []));
            flash('success', 'Panel backup restored successfully.');
        } elseif ($action === 'add_home_client' || $action === 'save_home_client') {
            $hash = normalizeDeviceHash((string)($_POST['hash'] ?? ''));
            $phone = normalizePhoneNumber((string)($_POST['phone'] ?? ''));
            $expiry = trim((string)($_POST['expiry'] ?? ''));
            $notes = trim((string)($_POST['notes'] ?? ''));
            if (strlen($hash) !== 24) {
                throw new RuntimeException('کد هش دستگاه باید دقیقاً ۲۴ کاراکتر باشد.');
            }
            if ($phone === '') {
                throw new RuntimeException('شماره تلفن معتبر الزامی است.');
            }
            if (!validJalaliDate($expiry)) {
                throw new RuntimeException('تاریخ انقضای شمسی معتبر نیست. مثال: 1405/08/15');
            }
            $clients = readHomeClients();
            $clients[$hash] = [
                'hash' => $hash,
                'phone' => $phone,
                'expiry' => $expiry,
                'notes' => $notes,
                'created_at' => $clients[$hash]['created_at'] ?? (dateTimeToJalali(iranToday())),
                'suspended' => false,
            ];
            writeHomeClients($clients);
            flash('success', 'کلاینت خانگی با موفقیت ثبت شد و با شماره ' . $phone . ' فعال گردید.');
        } elseif ($action === 'adjust_home_client') {
            $hash = normalizeDeviceHash((string)($_POST['hash'] ?? ''));
            $op = (string)($_POST['operation'] ?? '');
            $clients = readHomeClients();
            if (!isset($clients[$hash])) {
                throw new RuntimeException('کلاینت خانگی مورد نظر یافت نشد.');
            }
            if ($op === 'suspend') {
                $clients[$hash]['suspended'] = true;
                $msg = 'اشتراک کلاینت خانگی به حالت تعلیق درآمد.';
            } elseif ($op === 'resume') {
                $clients[$hash]['suspended'] = false;
                $msg = 'اشتراک کلاینت خانگی دوباره فعال شد.';
            } elseif ($op === 'extend_days') {
                $days = (int)($_POST['days'] ?? 0);
                if ($days < 1 || $days > 3650) throw new RuntimeException('تعداد روز نامعتبر است.');
                $clients[$hash]['expiry'] = extendSubscriptionExpiry($clients[$hash]['expiry'], 0, $days);
                $msg = "اشتراک کلاینت خانگی {$days} روز تمدید شد.";
            } elseif ($op === 'extend_months') {
                $months = (int)($_POST['months'] ?? 0);
                if ($months < 1 || $months > 12) throw new RuntimeException('تعداد ماه نامعتبر است.');
                $clients[$hash]['expiry'] = extendSubscriptionExpiry($clients[$hash]['expiry'], $months, 0);
                $msg = "اشتراک کلاینت خانگی {$months} ماه تمدید شد.";
            } else {
                throw new RuntimeException('عملیات نامعتبر است.');
            }
            writeHomeClients($clients);
            flash('success', $msg);
        } elseif ($action === 'delete_home_client') {
            $hash = normalizeDeviceHash((string)($_POST['hash'] ?? ''));
            $clients = readHomeClients();
            if (isset($clients[$hash])) {
                unset($clients[$hash]);
                writeHomeClients($clients);
                flash('success', 'کلاینت خانگی با موفقیت حذف شد.');
            }
        } elseif ($action === 'delete_security_report') {
            $reportId = trim((string)($_POST['report_id'] ?? ''));
            $reports = readSecurityReports();
            $newReports = array_values(array_filter($reports, fn($r) => ($r['id'] ?? '') !== $reportId));
            writeSecurityReports($newReports);
            flash('success', 'گزارش امنیتی با موفقیت حذف شد.');
        } elseif ($action === 'clear_security_reports') {
            writeSecurityReports([]);
            flash('success', 'تمام گزارش‌های امنیتی تخلف با موفقیت پاک‌سازی شدند.');
        } elseif ($action === 'save_tags') {
            $tags = validateTags((array)($_POST['build_tag'] ?? []),
                (array)($_POST['player_tag'] ?? []), (array)($_POST['expiry'] ?? []),
                (array)($_POST['upload_password'] ?? []));
            $rows = subscriptionRowsByKey();
            foreach ($rows as $key => $row) {
                if (empty($row['suspended'])) { unset($rows[$key]); }
            }
            foreach ($tags as $tag) {
                [$build, $player, $expiry, $uploadPassword] = array_map('trim', explode('|', $tag));
                $rows[strtoupper($build)] = compact('build', 'player', 'expiry') +
                    ['upload_password' => $uploadPassword, 'suspended' => false];
            }
            writeSubscriptionRows($rows);
            flash('success', count($tags) . ' اشتراک با موفقیت ذخیره شد.');
        } elseif ($action === 'add_subscription' || $action === 'save_subscription') {
            $validated = validateTags([(string)($_POST['build_tag'] ?? '')],
                [(string)($_POST['player_tag'] ?? '')], [(string)($_POST['expiry'] ?? '')],
                [(string)($_POST['upload_password'] ?? '')], [(string)($_POST['install_password'] ?? '')]);
            [$build, $player, $expiry, $uploadPassword, $installPassword] = array_map('trim', explode('|', $validated[0]));
            $rows = subscriptionRowsByKey();
            $newKey = strtoupper($build);
            $originalKey = strtoupper(trim((string)($_POST['original_build'] ?? $build)));
            if ($action === 'add_subscription' && isset($rows[$newKey])) {
                throw new RuntimeException('این تگ Build قبلاً ثبت شده است.');
            }
            if ($action === 'save_subscription' && !isset($rows[$originalKey])) {
                throw new RuntimeException('اشتراک موردنظر دیگر وجود ندارد؛ صفحه را تازه‌سازی کنید.');
            }
            if ($newKey !== $originalKey && isset($rows[$newKey])) {
                throw new RuntimeException('تگ Build جدید متعلق به اشتراک دیگری است.');
            }
            $suspended = $action === 'save_subscription' && !empty($rows[$originalKey]['suspended']);
            unset($rows[$originalKey]);
            $rows[$newKey] = compact('build', 'player', 'expiry', 'uploadPassword', 'installPassword', 'suspended');
            $rows[$newKey]['upload_password'] = $uploadPassword;
            $rows[$newKey]['install_password'] = $installPassword;
            unset($rows[$newKey]['uploadPassword'], $rows[$newKey]['installPassword']);
            writeSubscriptionRows($rows);
            flash('success', $action === 'add_subscription' ?
                'گیمنت جدید با موفقیت اضافه شد.' : 'مشخصات اشتراک ذخیره شد.');
        } elseif ($action === 'adjust_subscription') {
            $key = strtoupper(trim((string)($_POST['build'] ?? '')));
            $operation = (string)($_POST['operation'] ?? '');
            $rows = subscriptionRowsByKey();
            if ($key === '' || !isset($rows[$key])) {
                throw new RuntimeException('اشتراک موردنظر پیدا نشد؛ صفحه را تازه‌سازی کنید.');
            }
            if ($operation === 'suspend') {
                $rows[$key]['suspended'] = true;
                $message = 'اشتراک فوراً معلق و دسترسی آنلاین آن قطع شد.';
            } elseif ($operation === 'resume') {
                if (jalaliDateToDateTime((string)$rows[$key]['expiry']) < iranToday()) {
                    throw new RuntimeException('این اشتراک منقضی شده است؛ ابتدا آن را تمدید و سپس فعال کنید.');
                }
                $rows[$key]['suspended'] = false;
                $message = 'اشتراک دوباره فعال شد.';
            } elseif ($operation === 'extend_months') {
                $months = (int)($_POST['months'] ?? 0);
                if (!in_array($months, [1, 2, 3], true)) {
                    throw new RuntimeException('مدت تمدید ماهانه معتبر نیست.');
                }
                $rows[$key]['expiry'] = extendSubscriptionExpiry($rows[$key]['expiry'], $months, 0);
                $message = 'اشتراک ' . $months . ' ماه تمدید شد.';
            } elseif ($operation === 'extend_days') {
                $days = (int)($_POST['days'] ?? 0);
                if ($days < 1 || $days > 3650) {
                    throw new RuntimeException('تعداد روز باید بین ۱ تا ۳۶۵۰ باشد.');
                }
                $rows[$key]['expiry'] = extendSubscriptionExpiry($rows[$key]['expiry'], 0, $days);
                $message = 'اشتراک ' . $days . ' روز تمدید شد.';
            } else {
                throw new RuntimeException('عملیات اشتراک معتبر نیست.');
            }
            writeSubscriptionRows($rows);
            flash('success', $message);
        } elseif ($action === 'delete_subscription') {
            $key = strtoupper(trim((string)($_POST['build'] ?? '')));
            $rows = subscriptionRowsByKey();
            if ($key === '' || !isset($rows[$key])) {
                throw new RuntimeException('اشتراک موردنظر پیدا نشد.');
            }
            unset($rows[$key]);
            writeSubscriptionRows($rows);
            flash('success', 'اشتراک گیمنت حذف شد.');
        } elseif ($action === 'save_password') {
            $password = trim((string)($_POST['server_password'] ?? ''));
            if ($password === '' || strlen($password) > 31 ||
                !preg_match('/^[!#-\[\]-~]+$/', $password) || str_contains($password, ';')) {
                throw new RuntimeException('رمز باید ۱ تا ۳۱ نویسه ASCII و بدون فاصله، کوتیشن، بک‌اسلش یا ; باشد.');
            }
            backupAndAtomicWrite(FILE_PASSWORD, $password . "\n");
            flash('success', 'رمز مشترک سرورها جایگزین شد.');
        } elseif ($action === 'generate_install_password') {
            $key = strtoupper(trim((string)($_POST['build'] ?? '')));
            $rows = subscriptionRowsByKey();
            if ($key === '' || !isset($rows[$key])) {
                throw new RuntimeException('اشتراک موردنظر پیدا نشد.');
            }
            $password = '';
            for ($i = 0; $i < 8; $i++) {
                $password .= (string)random_int(0, 9);
            }
            $rows[$key]['install_password'] = $password;
            writeSubscriptionRows($rows);
            flash('success', 'رمز نصب جدید برای گیمنت تولید شد.');
        } elseif ($action === 'revoke_install_password') {
            $key = strtoupper(trim((string)($_POST['build'] ?? '')));
            $rows = subscriptionRowsByKey();
            if ($key === '' || !isset($rows[$key])) {
                throw new RuntimeException('اشتراک موردنظر پیدا نشد.');
            }
            $rows[$key]['install_password'] = '';
            writeSubscriptionRows($rows);
            flash('success', 'رمز نصب گیمنت باطل شد.');
        } elseif ($action === 'upload_update') {
            $edition = strtolower(trim((string)($_POST['update_edition'] ?? 'home')));
            if ($edition !== 'gamenet') {
                $edition = 'home';
            }
            $tag = strtoupper(trim((string)($_POST['update_tag'] ?? '')));
            if ($edition === 'home') {
                $tag = 'HOME';
            }
            $version = trim((string)($_POST['update_version'] ?? ''));
            $forced = !empty($_POST['update_forced']);

            global $dataDir;
            $downloadsDir = $dataDir . DIRECTORY_SEPARATOR . DIR_DOWNLOADS;
            if (!is_dir($downloadsDir) && !mkdir($downloadsDir, 0755, true) && !is_dir($downloadsDir)) {
                throw new RuntimeException('امکان ساخت پوشه downloads وجود ندارد.');
            }

            $file = $_FILES['update_file'] ?? [];
            $customUrl = trim((string)($_POST['custom_url'] ?? ''));

            $downloadUrl = '';
            $filename = '';
            $sizeStr = '';
            $hash = '';
            $type = 'zip';

            if (!empty($file['tmp_name']) && is_uploaded_file($file['tmp_name'])) {
                if ($file['error'] !== UPLOAD_ERR_OK) {
                    throw new RuntimeException('خطا در بارگذاری فایل آپدیت.');
                }
                $origExt = strtolower(pathinfo((string)$file['name'], PATHINFO_EXTENSION));
                if (!in_array($origExt, ['zip', 'exe', 'rar', '7z'], true)) {
                    throw new RuntimeException('فرمت فایل نامعتبر است. فقط ZIP یا EXE مجاز می‌باشد.');
                }
                $type = ($origExt === 'exe') ? 'exe' : 'zip';
                $originalName = (string)$file['name'];

                // 1. هوشمندسازی: بررسی درون فایل فشرده ZIP برای استخراج نسخه و تگ
                if ($origExt === 'zip' && class_exists('ZipArchive')) {
                    $zip = new ZipArchive();
                    if ($zip->open($file['tmp_name']) === true) {
                        $vFile = $zip->getFromName('version.txt');
                        if ($vFile !== false && trim($vFile) !== '') {
                            $candVer = trim($vFile);
                            if (preg_match('/^\d+(\.\d+){1,3}$/', $candVer) && $version === '') {
                                $version = $candVer;
                            }
                        }

                        $bInfo = $zip->getFromName('build-info.txt');
                        if ($bInfo !== false) {
                            if ($version === '' && preg_match('/Version:\s*([0-9\.]+)/i', $bInfo, $mVer)) {
                                $version = trim($mVer[1]);
                            }
                            if ($edition === 'gamenet' && ($tag === '' || $tag === 'DEFAULT') && preg_match('/Client tag:\s*([A-Za-z0-9_-]+)/i', $bInfo, $mTag)) {
                                $detectedTag = strtoupper(trim($mTag[1]));
                                if ($detectedTag !== 'HOME') {
                                    $tag = $detectedTag;
                                }
                            }
                        }
                        $zip->close();
                    }
                }

                // 2. شناسایی نسخه از نام فایل در صورت خالی بودن
                if ($version === '') {
                    if (preg_match('/[vV]?(\d+\.\d+(\.\d+)?)/', $originalName, $fnVer)) {
                        $version = $fnVer[1];
                    }
                }

                // 3. شناسایی خودکار نسخه و تگ از نام فایل در صورت لزوم
                if (preg_match('/[aA]llclient[-_]?[hH]ome/i', $originalName) || preg_match('/[hH]ome[-_]?[pP]atch/i', $originalName)) {
                    $edition = 'home';
                    $tag = 'HOME';
                } elseif ($edition === 'gamenet') {
                    if ($tag === '' || $tag === 'DEFAULT') {
                        if (preg_match('/[aA]llclient-([A-Za-z0-9_-]+)-/', $originalName, $fnTag)) {
                            $detected = strtoupper($fnTag[1]);
                            if ($detected === 'HOME') {
                                $edition = 'home';
                                $tag = 'HOME';
                            } else {
                                $tag = $detected;
                            }
                        }
                    }
                    if ($tag === '') {
                        $tag = 'GAMELAND';
                    }
                }

                if ($edition === 'home') {
                    $tag = 'HOME';
                }

                // 4. افزایش خودکار نسخه در صورت عدم تشخیص
                $updates = readUpdatesData();
                if ($version === '') {
                    $lastVer = $updates[$edition][$tag]['version'] ?? '0.0.1';
                    $parts = explode('.', $lastVer);
                    if (count($parts) >= 2) {
                        $parts[count($parts) - 1] = (int)$parts[count($parts) - 1] + 1;
                        $version = implode('.', $parts);
                    } else {
                        $version = '0.0.2';
                    }
                }

                if (!preg_match('/^\d+(\.\d+){1,3}$/', $version)) {
                    throw new RuntimeException('شماره نسخه باید قالبی مانند 0.0.2 یا 1.0.0 داشته باشد.');
                }

                if ($edition === 'home') {
                    $filename = "Allclient_Patch_HOME_v{$version}.{$origExt}";
                } else {
                    $filename = "Allclient_Patch_GAMENET_{$tag}_v{$version}.{$origExt}";
                }
                $destPath = $downloadsDir . DIRECTORY_SEPARATOR . $filename;

                if (!move_uploaded_file($file['tmp_name'], $destPath)) {
                    throw new RuntimeException('انتقال فایل آپلود شده به پوشه downloads ناموفق بود.');
                }
                $fileBytes = filesize($destPath);
                $sizeStr = ($fileBytes !== false) ? round($fileBytes / (1024 * 1024), 2) . ' MB' : '';
                $hash = hash_file('sha256', $destPath) ?: '';

                $proto = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off') ? 'https://' : 'http://';
                $host = $_SERVER['HTTP_HOST'] ?? 'gameland.cam';
                $downloadUrl = $proto . $host . '/downloads/' . $filename;
            } elseif ($customUrl !== '') {
                if (!filter_var($customUrl, FILTER_VALIDATE_URL)) {
                    throw new RuntimeException('لینک مستقیم دانلود نامعتبر است.');
                }
                $downloadUrl = $customUrl;
                $filename = basename(parse_url($customUrl, PHP_URL_PATH) ?: 'update.zip');
                $origExt = strtolower(pathinfo($filename, PATHINFO_EXTENSION));
                $type = ($origExt === 'exe') ? 'exe' : 'zip';
                $sizeStr = 'لینک مستقیم';
                if ($version === '') {
                    if (preg_match('/[vV]?(\d+\.\d+(\.\d+)?)/', $filename, $fnVer)) {
                        $version = $fnVer[1];
                    } else {
                        $version = '0.0.2';
                    }
                }
                if ($edition === 'home') {
                    $tag = 'HOME';
                } elseif ($tag === '') {
                    $tag = 'GAMELAND';
                }
            } else {
                throw new RuntimeException('لطفاً یک فایل پچ آپلود کنید یا لینک مستقیم دانلود را وارد نمایید.');
            }

            $updates = readUpdatesData();
            $updates[$edition][$tag] = [
                'edition' => $edition,
                'tag' => $tag,
                'version' => $version,
                'download_url' => $downloadUrl,
                'filename' => $filename,
                'size' => $sizeStr,
                'hash' => $hash,
                'type' => $type,
                'forced' => $forced,
                'updated_at' => date('Y-m-d H:i:s'),
                'updated_at_jalali' => dateTimeToJalali(iranToday()) . ' ' . (new DateTime('now', new DateTimeZone('Asia/Tehran')))->format('H:i')
            ];
            writeUpdatesData($updates);
            $editionTitle = ($edition === 'home') ? 'نسخه خانگی (Home)' : "نسخه گیم‌نت ({$tag})";
            flash('success', "آپدیت نسخه {$version} برای {$editionTitle} با موفقیت فعال شد.");
        } elseif ($action === 'delete_update') {
            $edition = strtolower(trim((string)($_POST['update_edition'] ?? 'gamenet')));
            if ($edition !== 'home') {
                $edition = 'gamenet';
            }
            $tag = strtoupper(trim((string)($_POST['update_tag'] ?? '')));
            $updates = readUpdatesData();
            if (isset($updates[$edition][$tag])) {
                unset($updates[$edition][$tag]);
                writeUpdatesData($updates);
                $editionTitle = ($edition === 'home') ? 'نسخه خانگی' : "تگ گیم‌نت {$tag}";
                flash('success', "آپدیت مربوط به {$editionTitle} غیرفعال شد.");
            } else {
                throw new RuntimeException('آپدیت مورد نظر یافت نشد.');
            }
        } else {
            throw new RuntimeException('عملیات ناشناخته است.');
        }
    } catch (Throwable $error) {
        flash('error', $error->getMessage());
    }
    redirectHome();
}

$flash = takeFlash();
$authenticated = $configured && !empty($_SESSION['authenticated']);
$serverText = '';
$mixServerText = '';
$tagRows = [];
$activeSubscriptions = 0;
$suspendedSubscriptions = 0;
$expiringSubscriptions = 0;
$passwordConfigured = false;
$installerAccessState = ['active' => false, 'code_hash' => '', 'plain_code' => '', 'created_at' => ''];
$generatedInstallerCode = '';
if ($authenticated) {
    try {
        $serverText = trim(readTextFile(FILE_SERVERS));
        $mixServerText = trim(readMixServersText());
        $ftpLines = normalizedLines(readTextFile(FILE_FTP_CONFIG));
        $ftpConfig = [
            'host' => $ftpLines[0] ?? '',
            'user' => $ftpLines[1] ?? '',
            'pass' => $ftpLines[2] ?? '',
            'path' => $ftpLines[3] ?? '/'
        ];
        $tagRows = array_map('decorateSubscriptionRow', array_values(subscriptionRowsByKey()));
        usort($tagRows, static function (array $left, array $right): int {
            $priority = ['expired' => 0, 'urgent' => 1, 'suspended' => 2, 'active' => 3];
            return ($priority[$left['state']] <=> $priority[$right['state']]) ?:
                strcasecmp($left['build'], $right['build']);
        });
        foreach ($tagRows as $row) {
            if ($row['state'] === 'suspended') { ++$suspendedSubscriptions; }
            elseif ($row['state'] === 'expired' || $row['state'] === 'urgent') { ++$expiringSubscriptions; }
            else { ++$activeSubscriptions; }
        }
        $passwordConfigured = trim(readTextFile(FILE_PASSWORD, 256)) !== '';
        $updatesData = readUpdatesData();
$homeClientsRaw = readHomeClients();
$homeClientRows = decorateHomeClientRows($homeClientsRaw);
$activeHomeClients = 0;
foreach ($homeClientRows as $hcr) {
    if ($hcr['state'] === 'active' || $hcr['state'] === 'expiring') $activeHomeClients++;
}
$defaultHomeExpiry = addJalaliMonths(dateTimeToJalali(iranToday()), 1);
$securityReports = readSecurityReports();
    } catch (Throwable $error) {
        $flash = ['type' => 'error', 'message' => $error->getMessage()];
    }
}
?>
<!doctype html>
<html lang="fa" dir="rtl">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <meta name="theme-color" content="#0b1626">
  <meta name="mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <meta name="apple-mobile-web-app-title" content="Allclient">
  <title>مدیریت Allclient</title>
  <link rel="manifest" href="manifest.webmanifest">
  <link rel="icon" href="app-icon.ico" sizes="any">
  <link rel="apple-touch-icon" href="icon-192.png">
  <link rel="stylesheet" href="style.css">
  <script src="panel.js" defer></script>
</head>
<body>
<main class="shell <?= $authenticated ? '' : 'login-shell' ?>">
  <header class="topbar">
    <div><span class="eyebrow">GAMELAND PROJECT</span><h1>کنترل‌پنل Allclient</h1></div>
    <?php if ($authenticated): ?>
      <div class="top-actions"><button class="button secondary pwa-install" id="pwa-install" type="button" hidden>نصب روی گوشی</button><form method="post"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="logout"><button class="button ghost" type="submit">خروج امن</button></form></div>
    <?php else: ?>
      <button class="button secondary pwa-install" id="pwa-install" type="button" hidden>نصب روی گوشی</button>
    <?php endif; ?>
  </header>

  <?php if ($flash): ?><div class="notice <?= escape((string)$flash['type']) ?>"><?= escape((string)$flash['message']) ?></div><?php endif; ?>

  <?php if (!$configured): ?>
    <section class="card login-card">
      <div class="icon-lock">◆</div><h2>راه‌اندازی اولیه</h2><p>رمز مدیر را تعیین کنید. این صفحه پس از ثبت رمز برای همیشه بسته می‌شود.</p>
      <form method="post" autocomplete="off">
        <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="setup">
        <label>رمز ۸ کاراکتری جدید<input type="password" name="new_password" required autofocus autocomplete="new-password"></label>
        <label>تکرار رمز<input type="password" name="confirm_password" required autocomplete="new-password"></label>
        <button class="button primary wide" type="submit">ثبت رمز و فعال‌سازی پنل</button>
      </form>
      <p class="security-note">پیش از ثبت رمز، Directory Privacy سی‌پنل را برای این پوشه فعال کنید.</p>
    </section>
  <?php elseif (!$authenticated): ?>
    <section class="card login-card">
      <div class="icon-lock">◆</div><h2>ورود مدیر</h2><p>برای مدیریت سرویس‌های آنلاین وارد شوید.</p>
      <form method="post" autocomplete="off">
        <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="login">
        <label>رمز مدیریت<input type="password" name="password" required autofocus autocomplete="current-password"></label>
        <button class="button primary wide" type="submit">ورود به پنل</button>
      </form>
    </section>
  <?php else: ?>
    <section class="stats">
      <div class="stat"><strong><?= count(normalizedLines($serverText)) ?></strong><span>سرور پین‌شده</span></div>
      <div class="stat"><strong><?= count($tagRows) ?></strong><span>کل اشتراک‌ها</span></div>
      <div class="stat"><strong class="ok"><?= $activeSubscriptions ?></strong><span>اشتراک فعال</span></div>
      <div class="stat"><strong class="<?= $expiringSubscriptions ? 'warn' : 'ok' ?>"><?= $expiringSubscriptions ?></strong><span>نیازمند توجه</span></div>
      <div class="stat"><strong class="<?= $suspendedSubscriptions ? 'bad' : 'ok' ?>"><?= $suspendedSubscriptions ?></strong><span>اشتراک معلق</span></div>
      <div class="stat"><strong class="<?= count($securityReports) ? 'bad' : 'ok' ?>"><?= count($securityReports) ?></strong><span>گزارش شیلد</span></div>
    </section>

    <nav class="panel-tabs" aria-label="بخش‌های پنل">
      <button class="panel-tab active" type="button" data-panel="dashboard" aria-selected="true"><span class="tab-icon">⌂</span><span>داشبورد</span></button>
      <button class="panel-tab" type="button" data-panel="subscriptions" aria-selected="false"><span class="tab-icon">◫</span><span>اشتراک‌ها</span><b><?= count($tagRows) ?></b></button>
      <button class="panel-tab" type="button" data-panel="home_clients" aria-selected="false"><span class="tab-icon">🏠</span><span>کلاینت‌های خانگی</span><b><?= count($homeClientRows) ?></b></button>
      <button class="panel-tab" type="button" data-panel="updates" aria-selected="false"><span class="tab-icon">↑</span><span>آپدیت‌ها</span><b><?= (count($updatesData['home'] ?? []) + count($updatesData['gamenet'] ?? [])) ?></b></button>
      <button class="panel-tab" type="button" data-panel="security_reports" aria-selected="false"><span class="tab-icon">🛡</span><span>گزارش‌های تخلف</span><b><?= count($securityReports) ?></b></button>
      <button class="panel-tab" type="button" data-panel="settings" aria-selected="false"><span class="tab-icon">⚙</span><span>تنظیمات</span></button>
    </nav>

    <section class="panel-view active" data-panel-view="dashboard">
    <section class="grid dashboard-grid">
      <article class="card">
        <div class="card-title"><div><h2>سرورهای پین‌شده</h2><p>هر سرور در یک خط با قالب IP:PORT</p></div><span class="pill">حداکثر ۶۴</span></div>
        <form method="post"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="save_servers">
          <textarea name="servers" class="code" rows="11" spellcheck="false" placeholder="5.57.32.203:27015"><?= escape($serverText) ?></textarea>
          <button class="button primary" type="submit">ذخیره سرورها</button>
        </form>
      </article>

      <article class="card">
        <div class="card-title"><div><h2>Mix Servers</h2><p>Only these IP:PORT rows are shown in the Mix column.</p></div><span class="pill">MIX</span></div>
        <form method="post"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="save_mix_servers">
          <textarea name="mix_servers" class="code" rows="11" spellcheck="false" placeholder="5.57.32.203:45000"><?= escape($mixServerText) ?></textarea>
          <button class="button primary" type="submit">Save Mix Servers</button>
        </form>
      </article>

      <article class="card">
        <div class="card-title"><div><h2>فضای ذخیره‌سازی دمو (FTP)</h2><p>دموهای کلاینت‌ها به این هاست دانلود منتقل می‌شوند.</p></div><span class="pill">DEMO</span></div>
        <form method="post" autocomplete="off"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="save_ftp_config">
          <label>آدرس سرور FTP<input class="ltr" type="text" name="ftp_host" value="<?= escape($ftpConfig['host']) ?>" placeholder="ftp.example.com"></label>
          <label>نام کاربری FTP<input class="ltr" type="text" name="ftp_user" value="<?= escape($ftpConfig['user']) ?>"></label>
          <label>رمز عبور FTP<input class="ltr" type="password" name="ftp_pass" value="<?= escape($ftpConfig['pass']) ?>" autocomplete="new-password"></label>
          <label>پوشه ذخیره دموها (Path)<input class="ltr" type="text" name="ftp_path" value="<?= escape($ftpConfig['path']) ?>" placeholder="/domains/gameland.cam/public_html/demos/"></label>
          <button class="button primary" type="submit">ذخیره تنظیمات FTP</button>
        </form>
      </article>

      <article class="card password-card">
        <div class="card-title"><div><h2>رمز مشترک سرورها</h2><p>رمز موجود برای امنیت نمایش داده نمی‌شود.</p></div><span class="dot <?= $passwordConfigured ? 'active' : '' ?>"></span></div>
        <form method="post" autocomplete="off"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="save_password">
          <label>رمز جدید<input class="ltr" type="password" name="server_password" maxlength="31" required autocomplete="new-password"></label>
          <button class="button warning" type="submit">جایگزینی رمز</button>
        </form>
      </article>

      <!-- Installer Access Block Removed -->    </section>
    </section>

    <section class="subscriptions panel-view" data-panel-view="subscriptions">
      <div class="section-heading">
        <div><span class="eyebrow">SUBSCRIPTIONS</span><h2>مدیریت اشتراک گیمنت‌ها</h2><p>وضعیت، زمان باقی‌مانده، تمدید و تعلیق فوری</p></div>
        <button class="button primary" id="show-add-subscription" type="button">+ اشتراک جدید</button>
      </div>

      <form class="card add-subscription" id="add-subscription" method="post" hidden>
        <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="add_subscription">
        <div class="form-heading"><div><h3>افزودن گیمنت</h3><p>اشتراک پس از ذخیره فوراً برای کلاینت قابل استفاده است.</p></div><button class="icon-button" id="close-add-subscription" type="button" aria-label="بستن">×</button></div>
        <div class="subscription-fields"><label>نام یا تگ Build<input class="ltr" name="build_tag" maxlength="64" pattern="[A-Za-z0-9_-]+" placeholder="IMORTAL_GONBAD" required></label><label>تگ بازیکن<input class="ltr" name="player_tag" maxlength="12" pattern="[A-Za-z0-9_-]+" placeholder="IM" required></label><label>انقضای شمسی<input class="ltr" name="expiry" maxlength="10" pattern="\d{4}/\d{2}/\d{2}" placeholder="1405/06/25" required></label><label>رمز آپلود دمو<input class="ltr" name="upload_password" maxlength="31" pattern="[A-Za-z0-9_!@#$%^&*.\-]{0,31}" autocomplete="new-password"></label><label>رمز نصب کلاینت<input class="ltr" name="install_password" maxlength="31" pattern="[A-Za-z0-9_!@#$%^&*.\-]{0,31}" autocomplete="new-password"></label></div>
        <button class="button primary" type="submit">ساخت اشتراک</button>
      </form>

      <div class="subscription-toolbar card">
        <label class="search-box">جست‌وجو<input id="subscription-search" type="search" placeholder="نام گیمنت یا تگ بازیکن"></label>
        <div class="filter-buttons" role="group" aria-label="فیلتر اشتراک‌ها"><button class="filter-chip active" type="button" data-filter="all">همه</button><button class="filter-chip" type="button" data-filter="active">فعال</button><button class="filter-chip" type="button" data-filter="attention">نیازمند توجه</button><button class="filter-chip" type="button" data-filter="suspended">معلق</button></div>
      </div>

      <div class="subscription-table-head" aria-hidden="true"><span>پروفایل گیمنت</span><span>وضعیت</span><span>اعتبار باقی‌مانده</span><span>تاریخ انقضا</span><span>مدیریت</span></div>
      <div class="subscription-list" id="subscription-list">
        <?php foreach ($tagRows as $rowIndex => $row): ?>
          <?php $attention = in_array($row['state'], ['expired', 'urgent'], true); ?>
          <article class="subscription-card state-<?= escape($row['state']) ?>" data-state="<?= $attention ? 'attention' : escape($row['state']) ?>" data-search="<?= escape(strtolower($row['build'] . ' ' . $row['player'])) ?>">
            <div class="subscription-identity">
              <span class="profile-avatar"><?= escape(strtoupper(substr($row['player'], 0, 2))) ?></span>
              <div><h3><?= escape($row['build']) ?></h3><span class="player-tag ltr"><?= escape($row['player']) ?></span></div>
            </div>
            <div class="subscription-status"><span class="status-dot <?= escape($row['state']) ?>"></span><span class="status-badge <?= escape($row['state']) ?>"><?= escape($row['state_label']) ?></span></div>
            <div class="remaining <?= $row['days_remaining'] < 0 ? 'overdue' : '' ?>">
              <?php if ($row['state'] === 'suspended' && $row['days_remaining'] >= 0): ?><strong><?= (int)$row['days_remaining'] ?></strong><span>روز (معلق)</span>
              <?php elseif ($row['state'] === 'suspended'): ?><strong><?= abs((int)$row['days_remaining']) ?></strong><span>روز گذشته (معلق)</span>
              <?php elseif ($row['days_remaining'] < 0): ?><strong><?= abs((int)$row['days_remaining']) ?></strong><span>روز گذشته</span>
              <?php else: ?><strong><?= (int)$row['days_remaining'] ?></strong><span>روز باقی‌مانده</span><?php endif; ?>
            </div>
            <div class="expiry-line"><span>انقضا</span><strong class="ltr"><?= escape($row['expiry']) ?></strong></div>

            <button class="open-profile-modal" type="button" data-modal="profile-modal-<?= (int)$rowIndex ?>"><span>مدیریت پروفایل</span><i>←</i></button>
            <dialog class="profile-modal" id="profile-modal-<?= (int)$rowIndex ?>" aria-labelledby="profile-title-<?= (int)$rowIndex ?>">
              <div class="modal-shell">
                <header class="modal-header"><div class="modal-profile"><span class="profile-avatar"><?= escape(strtoupper(substr($row['player'], 0, 2))) ?></span><div><span class="status-badge <?= escape($row['state']) ?>"><?= escape($row['state_label']) ?></span><h3 id="profile-title-<?= (int)$rowIndex ?>"><?= escape($row['build']) ?></h3><p>تگ بازیکن: <b class="ltr"><?= escape($row['player']) ?></b> · انقضا: <b class="ltr"><?= escape($row['expiry']) ?></b> · رمز دمو: <b class="ltr"><?= escape($row['upload_password'] !== '' ? $row['upload_password'] : 'not set') ?></b> · رمز نصب: <b class="ltr"><?= escape(empty($row['install_password']) ? 'ندارد' : 'دارد') ?></b></p></div></div><button class="modal-close" type="button" data-close-modal aria-label="بستن پنجره">×</button></header>
                <div class="modal-overview"><div><span>وضعیت فعلی</span><strong class="<?= escape($row['state']) ?>"><?= escape($row['state_label']) ?></strong></div><div><span>اعتبار</span><strong><?= abs((int)$row['days_remaining']) ?> روز <?= $row['days_remaining'] < 0 ? 'گذشته' : 'باقی‌مانده' ?></strong></div><div><span>تاریخ انقضا</span><strong class="ltr"><?= escape($row['expiry']) ?></strong></div></div>
                <div class="manage-panel">
                <section class="manage-block renewal-block"><div class="manage-title"><strong>تمدید اعتبار</strong><span>از تاریخ فعلی یا امروز محاسبه می‌شود</span></div><div class="quick-actions"><form method="post" class="month-actions"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="adjust_subscription"><input type="hidden" name="operation" value="extend_months"><input type="hidden" name="build" value="<?= escape($row['build']) ?>"><button type="submit" name="months" value="1">+ ۱ ماه</button><button type="submit" name="months" value="2">+ ۲ ماه</button><button type="submit" name="months" value="3">+ ۳ ماه</button></form><form method="post" class="days-action"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="adjust_subscription"><input type="hidden" name="operation" value="extend_days"><input type="hidden" name="build" value="<?= escape($row['build']) ?>"><input type="number" name="days" min="1" max="3650" inputmode="numeric" placeholder="تعداد روز" required><button class="button secondary" type="submit">تمدید دلخواه</button></form></div></section>
                <section class="manage-block access-block"><div class="manage-title"><strong>کنترل دسترسی</strong><span>تغییر وضعیت بلافاصله روی فایل کلاینت اعمال می‌شود</span></div><form method="post" class="confirm-form" data-confirm="<?= $row['suspended'] ? 'اشتراک دوباره فعال شود؟' : 'دسترسی آنلاین این گیمنت فوراً معلق شود؟' ?>"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="adjust_subscription"><input type="hidden" name="operation" value="<?= $row['suspended'] ? 'resume' : 'suspend' ?>"><input type="hidden" name="build" value="<?= escape($row['build']) ?>"><button class="button <?= $row['suspended'] ? 'primary' : 'warning' ?>" type="submit"><?= $row['suspended'] ? 'فعال‌سازی مجدد' : 'تعلیق دسترسی آنلاین' ?></button></form></section>
                <section class="manage-block password-block"><div class="manage-title"><strong>رمز نصب کلاینت</strong><span>برای نصب کلاینت با این گیمنت استفاده می‌شود</span></div><?php if (empty($row['install_password'])): ?><form method="post"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="generate_install_password"><input type="hidden" name="build" value="<?= escape($row['build']) ?>"><button class="button primary wide" type="submit">تولید رمز جدید</button></form><?php else: ?><div class="quick-actions password-display" style="display: flex; gap: 8px; align-items: stretch; margin-top: 10px;"><div class="ltr" style="padding: 10px 16px; background: rgba(0,255,200,0.15); color: #00ffcc; border: 1px solid rgba(0,255,200,0.3); border-radius: 6px; font-size: 1.4em; font-weight: bold; letter-spacing: 3px; flex: 1; text-align: center; user-select: all; cursor: copy; display: flex; align-items: center; justify-content: center;" title="برای کپی کلیک کنید" onclick="navigator.clipboard.writeText('<?= escape($row['install_password']) ?>'); this.style.backgroundColor='rgba(255,255,255,0.2)'; setTimeout(()=>this.style.backgroundColor='rgba(0,255,200,0.15)', 200);"><?= escape($row['install_password']) ?></div><form method="post" class="confirm-form" data-confirm="آیا از ابطال رمز نصب این گیمنت اطمینان دارید؟" style="display: flex; margin: 0;"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="revoke_install_password"><input type="hidden" name="build" value="<?= escape($row['build']) ?>"><button class="button danger" type="submit" style="margin: 0; align-self: stretch;">ابطال رمز</button></form></div><?php endif; ?></section>
                <details class="subscription-edit"><summary>ویرایش اطلاعات پروفایل</summary><div class="edit-actions"><form method="post" class="edit-form"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="save_subscription"><input type="hidden" name="original_build" value="<?= escape($row['build']) ?>"><div class="subscription-fields"><label>تگ Build<input class="ltr" name="build_tag" maxlength="64" pattern="[A-Za-z0-9_-]+" value="<?= escape($row['build']) ?>" required></label><label>تگ بازیکن<input class="ltr" name="player_tag" maxlength="12" pattern="[A-Za-z0-9_-]+" value="<?= escape($row['player']) ?>" required></label><label>انقضای شمسی<input class="ltr" name="expiry" maxlength="10" pattern="\d{4}/\d{2}/\d{2}" value="<?= escape($row['expiry']) ?>" required></label><label>رمز آپلود دمو<input class="ltr" name="upload_password" maxlength="31" pattern="[A-Za-z0-9_!@#$%^&*.\-]{0,31}" value="<?= escape($row['upload_password']) ?>" autocomplete="new-password"></label><label>رمز نصب کلاینت<input class="ltr" name="install_password" maxlength="31" pattern="[A-Za-z0-9_!@#$%^&*.\-]{0,31}" value="<?= escape($row['install_password'] ?? '') ?>" autocomplete="new-password"></label></div><button class="button secondary" type="submit">ذخیره اطلاعات</button></form><form method="post" class="confirm-form delete-form" data-confirm="این اشتراک برای همیشه حذف شود؟"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="delete_subscription"><input type="hidden" name="build" value="<?= escape($row['build']) ?>"><button class="button danger" type="submit">حذف کامل پروفایل</button></form></div></details>
                </div>
              </div>
            </dialog>
          </article>
        <?php endforeach; ?>
        <?php if (!$tagRows): ?><div class="card empty-state"><strong>هنوز اشتراکی ثبت نشده است</strong><span>با دکمه «اشتراک جدید» اولین گیمنت را اضافه کنید.</span></div><?php endif; ?>
      </div>
    </section>

    <section class="home-clients panel-view" data-panel-view="home_clients">
      <div class="section-heading">
        <div>
          <span class="eyebrow">HOME CLIENTS</span>
          <h2>مدیریت اشتراک کلاینت‌های خانگی</h2>
          <p>فعال‌سازی با شناسه سخت‌افزاری ۲۴ کاراکتری، اتصال به شماره موبایل و قفل کامل در صورت اتمام اعتبار</p>
        </div>
      </div>

      <div class="grid dashboard-grid" style="margin-bottom: 24px;">
        <article class="card">
          <div class="card-title">
            <div>
              <h2>ثبت و فعال‌سازی کلاینت جدید</h2>
              <p>هش ۲۴ کاراکتری تولیدشده در اینستالر کاربر را همراه شماره موبایل وارد کنید.</p>
            </div>
            <span class="pill">ثبت آنی</span>
          </div>
          <form method="post" autocomplete="off">
            <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>">
            <input type="hidden" name="action" value="add_home_client">
            <label>کد سخت‌افزاری دستگاه (هش ۲۴ کاراکتری)
              <input class="ltr" type="text" name="hash" maxlength="24" placeholder="مثال: C4F89A12B3D5E70198ABCEFD" required style="font-family: monospace; font-size: 1.15em; letter-spacing: 2px;">
            </label>
            <label>شماره تلفن همراه (سابلینک)
              <input class="ltr" type="text" name="phone" maxlength="15" placeholder="مثال: 09123456789" required style="font-size: 1.1em;">
            </label>
            <label>تاریخ انقضای شمسی
              <input class="ltr" type="text" name="expiry" maxlength="10" pattern="\d{4}/\d{2}/\d{2}" value="<?= escape($defaultHomeExpiry) ?>" required style="font-size: 1.1em;">
            </label>
            <div style="display: flex; gap: 6px; margin-bottom: 12px;">
              <button type="button" class="button secondary" style="font-size: 12px; padding: 4px 8px;" onclick="this.form.expiry.value='<?= escape(addJalaliMonths(dateTimeToJalali(iranToday()), 1)) ?>'">+۱ ماه (۳۰ روز)</button>
              <button type="button" class="button secondary" style="font-size: 12px; padding: 4px 8px;" onclick="this.form.expiry.value='<?= escape(addJalaliMonths(dateTimeToJalali(iranToday()), 3)) ?>'">+۳ ماه (۹۰ روز)</button>
              <button type="button" class="button secondary" style="font-size: 12px; padding: 4px 8px;" onclick="this.form.expiry.value='<?= escape(addJalaliMonths(dateTimeToJalali(iranToday()), 6)) ?>'">+۶ ماه</button>
              <button type="button" class="button secondary" style="font-size: 12px; padding: 4px 8px;" onclick="this.form.expiry.value='<?= escape(addJalaliMonths(dateTimeToJalali(iranToday()), 12)) ?>'">+۱ سال</button>
            </div>
            <label>نام کاربر / توضیحات (اختیاری)
              <input type="text" name="notes" placeholder="نام یا یادداشت شخصی">
            </label>
            <button class="button primary wide" type="submit" style="margin-top: 8px;">ثبت و فعال‌سازی اشتراک</button>
          </form>
        </article>

        <article class="card">
          <div class="card-title">
            <div>
              <h2>وضعیت کلی کلاینت‌های خانگی</h2>
              <p>اطلاعات اشتراک‌های فعال و منقضی‌شده</p>
            </div>
            <span class="pill"><?= count($homeClientRows) ?> کلاینت</span>
          </div>
          <div class="stats" style="grid-template-columns: 1fr 1fr; margin-bottom: 16px;">
            <div class="stat"><strong class="ok"><?= $activeHomeClients ?></strong><span>اشتراک‌های فعال</span></div>
            <div class="stat"><strong class="<?= (count($homeClientRows) - $activeHomeClients > 0) ? 'bad' : 'ok' ?>"><?= count($homeClientRows) - $activeHomeClients ?></strong><span>منقضی یا معلق</span></div>
          </div>
          <div style="background: rgba(255,255,255,0.03); border: 1px solid var(--border); border-radius: 8px; padding: 12px; font-size: 13px; line-height: 1.6; color: var(--muted);">
            <strong style="color: var(--text);">نحوه کارکرد کلاینت خانگی:</strong>
            <p style="margin: 4px 0 0 0;">کلاینت خانگی فاقد بازی شبکه محلی (LAN) است. در صورت اتمام مدت اعتبار یا عدم تطابق کد دستگاه، لانچر اجازه باز شدن بازی را نخواهد داد. تمدید اشتراک بلافاصله از طریق همین بخش امکان‌پذیر است.</p>
          </div>
        </article>
      </div>

      <div class="card">
        <div class="card-title">
          <div>
            <h2>فهرست دستگاه‌ها و اشتراک‌های خانگی</h2>
            <p>دستگاه‌های متصل و وضعیت اشتراک هر شماره</p>
          </div>
          <span class="pill"><?= count($homeClientRows) ?> مورد</span>
        </div>

        <?php if (!empty($homeClientRows)): ?>
          <div style="overflow-x: auto;">
            <table style="width: 100%; border-collapse: collapse; text-align: right; font-size: 13px;">
              <thead>
                <tr style="border-bottom: 1px solid var(--border); color: var(--muted);">
                  <th style="padding: 10px;">کد دستگاه (هش ۲۴ تایی)</th>
                  <th style="padding: 10px;">شماره موبایل</th>
                  <th style="padding: 10px;">انقضا</th>
                  <th style="padding: 10px;">باقی‌مانده</th>
                  <th style="padding: 10px;">وضعیت</th>
                  <th style="padding: 10px;">یادداشت</th>
                  <th style="padding: 10px; text-align: center;">عملیات تمدید و مدیریت</th>
                </tr>
              </thead>
              <tbody>
                <?php foreach ($homeClientRows as $hc): ?>
                  <tr style="border-bottom: 1px solid rgba(255,255,255,0.05);">
                    <td style="padding: 12px 10px;">
                      <div class="ltr" style="font-family: monospace; font-size: 13px; font-weight: bold; color: #45cfff; cursor: pointer; user-select: all;" title="برای کپی کد کلیک کنید" onclick="navigator.clipboard.writeText('<?= escape($hc['hash']) ?>'); alert('کد دستگاه کپی شد: <?= escape($hc['hash']) ?>');">
                        <?= escape($hc['hash']) ?> 📋
                      </div>
                    </td>
                    <td style="padding: 12px 10px;"><strong class="ltr" style="font-size: 14px;"><?= escape($hc['phone']) ?></strong></td>
                    <td style="padding: 12px 10px;"><span class="ltr"><?= escape($hc['expiry']) ?></span></td>
                    <td style="padding: 12px 10px;">
                      <?php if ($hc['days_remaining'] >= 0): ?>
                        <strong class="ok"><?= $hc['days_remaining'] ?> روز</strong>
                      <?php else: ?>
                        <strong class="bad"><?= abs($hc['days_remaining']) ?> روز گذشته</strong>
                      <?php endif; ?>
                    </td>
                    <td style="padding: 12px 10px;">
                      <span class="status-badge <?= escape($hc['state']) ?>"><?= escape($hc['state_label']) ?></span>
                    </td>
                    <td style="padding: 12px 10px; color: var(--muted);"><?= escape($hc['notes'] ?: '-') ?></td>
                    <td style="padding: 12px 10px;">
                      <div style="display: flex; gap: 6px; justify-content: center; align-items: center; flex-wrap: wrap;">
                        <!-- Quick +30 days -->
                        <form method="post" style="margin: 0;">
                          <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>">
                          <input type="hidden" name="action" value="adjust_home_client">
                          <input type="hidden" name="operation" value="extend_days">
                          <input type="hidden" name="days" value="30">
                          <input type="hidden" name="hash" value="<?= escape($hc['hash']) ?>">
                          <button class="button secondary" type="submit" style="min-height: 30px; padding: 2px 8px; font-size: 12px;" title="تمدید ۳۰ روزه">+۳۰ روز</button>
                        </form>
                        <!-- Quick +90 days -->
                        <form method="post" style="margin: 0;">
                          <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>">
                          <input type="hidden" name="action" value="adjust_home_client">
                          <input type="hidden" name="operation" value="extend_days">
                          <input type="hidden" name="days" value="90">
                          <input type="hidden" name="hash" value="<?= escape($hc['hash']) ?>">
                          <button class="button secondary" type="submit" style="min-height: 30px; padding: 2px 8px; font-size: 12px;" title="تمدید ۹۰ روزه">+۹۰ روز</button>
                        </form>
                        <!-- Suspend / Resume -->
                        <form method="post" class="confirm-form" data-confirm="<?= !empty($hc['suspended']) ? 'اشتراک این دستگاه فعال شود؟' : 'اشتراک این دستگاه معلق شود؟' ?>" style="margin: 0;">
                          <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>">
                          <input type="hidden" name="action" value="adjust_home_client">
                          <input type="hidden" name="operation" value="<?= !empty($hc['suspended']) ? 'resume' : 'suspend' ?>">
                          <input type="hidden" name="hash" value="<?= escape($hc['hash']) ?>">
                          <button class="button <?= !empty($hc['suspended']) ? 'primary' : 'warning' ?>" type="submit" style="min-height: 30px; padding: 2px 8px; font-size: 12px;">
                            <?= !empty($hc['suspended']) ? 'رفع تعلیق' : 'تعلیق' ?>
                          </button>
                        </form>
                        <!-- Delete -->
                        <form method="post" class="confirm-form" data-confirm="آیا از حذف کامل این کلاینت خانگی اطمینان دارید؟" style="margin: 0;">
                          <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>">
                          <input type="hidden" name="action" value="delete_home_client">
                          <input type="hidden" name="hash" value="<?= escape($hc['hash']) ?>">
                          <button class="button danger" type="submit" style="min-height: 30px; padding: 2px 8px; font-size: 12px;">حذف</button>
                        </form>
                      </div>
                    </td>
                  </tr>
                <?php endforeach; ?>
              </tbody>
            </table>
          </div>
        <?php else: ?>
          <div class="empty-state" style="text-align: center; padding: 30px; color: var(--muted);">
            <strong>در حال حاضر هیچ کلاینت خانگی ثبت نشده است.</strong>
            <p style="margin-top: 6px;">هنگامی که کاربر اینستالر را اجرا می‌کند، کد ۲۴ کاراکتری خود را برای شما می‌فرستد. با ثبت آن در فرم بالا، اشتراک دستگاه فعال می‌شود.</p>
          </div>
        <?php endif; ?>
      </div>
    </section>

    <section class="updates panel-view" data-panel-view="updates">
      <div class="section-heading">
        <div><span class="eyebrow">UPDATES SYSTEM</span><h2>مدیریت و بارگذاری آپدیت کلاینت (تفکیک خانگی و گیم‌نت)</h2><p>انتشار مستقل پچ‌های سبک (ZIP) یا نصبی برای نسخه خانگی و نسخه گیم‌نت به صورت کاملاً ایزوله</p></div>
      </div>

      <div class="grid dashboard-grid" style="margin-bottom: 24px;">
        <article class="card">
          <div class="card-title"><div><h2>بارگذاری آپدیت جدید</h2><p>فایل پچ فشرده (.zip حدود ۳ تا ۱۰ مگابایت) یا اینستالر (.exe) را انتخاب کنید.</p></div><span class="pill">UPLOAD</span></div>
          <form method="post" enctype="multipart/form-data" autocomplete="off">
            <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>">
            <input type="hidden" name="action" value="upload_update">
            
            <label style="margin-bottom: 6px;">کانال و نسخه کلاینت هدف:
              <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-top: 6px;">
                <label id="lbl-edition-home" style="display: flex; flex-direction: column; gap: 6px; padding: 12px 14px; border: 2px solid var(--primary); background: rgba(0, 210, 160, 0.08); border-radius: 10px; cursor: pointer;">
                  <div style="display: flex; align-items: center; gap: 8px;">
                    <input type="radio" name="update_edition" value="home" checked id="edition-radio-home" style="width: auto; min-height: auto;">
                    <strong style="color: var(--text);">🏠 نسخه خانگی (Home)</strong>
                  </div>
                  <span style="font-size: 11px; color: var(--muted); line-height: 1.5;">ویژه بازیکنان خانگی (بدون Listen Server، احراز هویت با دیوایس‌هش)</span>
                </label>
                <label id="lbl-edition-gamenet" style="display: flex; flex-direction: column; gap: 6px; padding: 12px 14px; border: 1px solid var(--border); background: var(--input); border-radius: 10px; cursor: pointer;">
                  <div style="display: flex; align-items: center; gap: 8px;">
                    <input type="radio" name="update_edition" value="gamenet" id="edition-radio-gamenet" style="width: auto; min-height: auto;">
                    <strong style="color: var(--text);">🎮 نسخه گیم‌نت (GameNet)</strong>
                  </div>
                  <span style="font-size: 11px; color: var(--muted); line-height: 1.5;">ویژه سیستم‌های گیم‌نت و کلوب‌ها (دارای LAN، تگ‌های اختصاصی)</span>
                </label>
              </div>
            </label>

            <div id="home-tag-notice" style="padding: 10px 14px; background: rgba(0,210,160,0.1); border: 1px solid rgba(0,210,160,0.25); border-radius: 8px; color: #45d59a; font-size: 12px; margin-bottom: 12px;">
              ✓ پچ به صورت یکپارچه و ایزوله برای تمامی کلاینت‌های خانگی منتشر می‌شود (تگ هدف: <strong>HOME</strong>). کلاینت‌های گیم‌نت به این پچ دسترسی نخواهند داشت.
            </div>

            <div id="gamenet-tag-wrapper" style="display: none; margin-bottom: 12px;">
              <label>تگ گیم‌نت / کلوب
                <select id="update-tag-select" name="update_tag" class="ltr" style="min-height: 44px; border: 1px solid #334c67; border-radius: 10px; padding: 11px 13px; background: var(--input); color: var(--text);">
                  <option value="GAMELAND">GAMELAND (پیش‌فرض عمومی گیم‌نت)</option>
                  <option value="DEFAULT">DEFAULT (همگانی برای تمامی گیم‌نت‌ها)</option>
                  <?php foreach ($tagRows as $tRow): ?>
                    <?php if (strtoupper($tRow['build']) !== 'GAMELAND'): ?>
                      <option value="<?= escape($tRow['build']) ?>"><?= escape($tRow['build']) ?> (<?= escape($tRow['player']) ?>)</option>
                    <?php endif; ?>
                  <?php endforeach; ?>
                </select>
              </label>
            </div>

            <label>شماره نسخه جدید (اختیاری - خودکار از فایل استخراج می‌شود)
              <input id="update-version-input" class="ltr" type="text" name="update_version" placeholder="خودکار از فایل یا دلخواه (مثلاً 0.0.2)" pattern="^\d+(\.\d+){1,3}$">
            </label>

            <label>فایل آپدیت (ZIP سبک یا EXE)
              <input id="update-file-input" type="file" name="update_file" accept=".zip,.exe,.rar,.7z">
            </label>
            <div id="update-detect-banner" style="display:none; padding: 10px 14px; background: rgba(69,213,154,0.15); border: 1px solid rgba(69,213,154,0.3); border-radius: 8px; color: var(--green); margin: -5px 0 15px; font-size: 12px; line-height: 1.6;"></div>

            <label>یا لینک مستقیم دانلود (در صورت میزبانی روی سرور دیگر)
              <input class="ltr" type="url" name="custom_url" placeholder="https://gameland.cam/downloads/patch.zip">
            </label>

            <label style="display: flex; align-items: center; gap: 10px; cursor: pointer;">
              <input type="checkbox" name="update_forced" value="1" checked style="width: auto; min-height: auto;">
              <span>آپدیت اجباری (کلاینت تا زمان دریافت آپدیت اجازه ادامه بازی را ندارد)</span>
            </label>

            <button class="button primary wide" type="submit" style="margin-top: 10px;">بارگذاری هوشمند و فعال‌سازی خودکار آپدیت</button>
          </form>
        </article>

        <article class="card">
          <div class="card-title"><div><h2>راهنمای فنی و تفکیک کانال‌ها</h2><p>ایزولاسیون کامل نسخه خانگی و نسخه گیم‌نت</p></div><span class="pill">ISOLATED</span></div>
          <div style="font-size: 13px; line-height: 1.8; color: var(--muted);">
            <p><strong style="color: var(--text);">ایزولاسیون کامل:</strong> پچ نسخه خانگی فقط توسط لانچرهای نسخه خانگی دریافت می‌شود و به هیچ عنوان به گیم‌نت‌ها داده نمی‌شود. بالعکس، پچ‌های گیم‌نت فقط توسط سیستم‌های کلوب دریافت خواهند شد.</p>
            <p><strong style="color: var(--text);">پچ کم‌حجم ZIP:</strong> پچ زیپ شامل فایل‌های <code class="ltr">GameUI.dll</code>, <code class="ltr">client_mini.dll</code>, <code class="ltr">cstrike.exe</code> و کتابخانه‌ها است (حدود ۴ مگابایت فشرده) و توسط <code class="ltr">updater.exe</code> بدون دستکاری کانفیگ شخصی یا لیسنس کاربر سریعاً جایگزین می‌شود.</p>
          </div>
          <hr style="border: 0; border-top: 1px solid var(--border); margin: 15px 0;">
          <div style="display: flex; flex-direction: column; gap: 10px;">
            <label>تست زنده API برای نسخه خانگی (Home):
              <div style="display: flex; gap: 8px; margin-top: 4px;">
                <a href="../update_api.php?edition=home&version=0.0.1" target="_blank" class="button secondary" style="text-decoration: none; display: flex; align-items: center; justify-content: center; width: 100%;">بررسی پاسخ JSON نسخه خانگی (0.0.1) ↗</a>
              </div>
            </label>
            <label>تست زنده API برای نسخه گیم‌نت (GameNet):
              <div style="display: flex; gap: 8px; margin-top: 4px;">
                <a href="../update_api.php?edition=gamenet&tag=GAMELAND&version=0.0.1" target="_blank" class="button secondary" style="text-decoration: none; display: flex; align-items: center; justify-content: center; width: 100%;">بررسی پاسخ JSON گیم‌نت GAMELAND ↗</a>
              </div>
            </label>
          </div>
        </article>
      </div>

      <!-- 1. Home Edition Active Updates -->
      <div class="card" style="margin-bottom: 24px; border-top: 3px solid #00d2a0;">
        <div class="card-title">
          <div>
            <h2>🏠 آپدیت‌های فعال نسخه خانگی (Home Edition)</h2>
            <p>پچ اختصاصی برای کاربران خانگی Allclient در کانال home</p>
          </div>
          <span class="pill" style="background: rgba(0,210,160,0.15); color: #00d2a0; border: 1px solid rgba(0,210,160,0.3);"><?= count($updatesData['home'] ?? []) ?> فعال</span>
        </div>
        <?php if (!empty($updatesData['home'])): ?>
          <div style="overflow-x: auto;">
            <table style="width: 100%; border-collapse: collapse; text-align: right; font-size: 13px;">
              <thead>
                <tr style="border-bottom: 1px solid var(--border); color: var(--muted);">
                  <th style="padding: 10px;">کانال و تگ</th>
                  <th style="padding: 10px;">نسخه فعال</th>
                  <th style="padding: 10px;">نوع و حجم</th>
                  <th style="padding: 10px;">تاریخ انتشار</th>
                  <th style="padding: 10px;">لینک دانلود</th>
                  <th style="padding: 10px;">تست API</th>
                  <th style="padding: 10px;">عملیات</th>
                </tr>
              </thead>
              <tbody>
                <?php foreach ($updatesData['home'] as $uTag => $uInfo): ?>
                  <tr style="border-bottom: 1px solid rgba(255,255,255,0.05);">
                    <td style="padding: 12px 10px;"><strong class="ltr" style="color: #00d2a0;">🏠 <?= escape((string)$uTag) ?></strong></td>
                    <td style="padding: 12px 10px;"><span class="ltr" style="background: rgba(0, 210, 160, 0.15); padding: 4px 8px; border-radius: 6px; font-weight: bold; color: #00d2a0;"><?= escape((string)($uInfo['version'] ?? '')) ?></span></td>
                    <td style="padding: 12px 10px;"><?= escape((string)($uInfo['size'] ?? '')) ?> (<?= escape(strtoupper((string)($uInfo['type'] ?? 'ZIP'))) ?>)</td>
                    <td style="padding: 12px 10px;"><?= escape((string)($uInfo['updated_at_jalali'] ?? $uInfo['updated_at'] ?? '')) ?></td>
                    <td style="padding: 12px 10px;"><a href="<?= escape((string)($uInfo['download_url'] ?? '')) ?>" target="_blank" class="ltr" style="color: var(--primary); text-decoration: none; word-break: break-all;" title="دانلود مستقیم پچ">دریافت فایل ⤓</a></td>
                    <td style="padding: 12px 10px;"><a href="../update_api.php?edition=home&tag=<?= urlencode((string)$uTag) ?>&version=0.0.1" target="_blank" style="color: var(--green); text-decoration: none;">تست (0.0.1) ↗</a></td>
                    <td style="padding: 12px 10px;">
                      <form method="post" class="confirm-form" data-confirm="آیا از غیرفعال‌سازی این آپدیت برای نسخه خانگی مطمئن هستید؟" style="margin: 0;">
                        <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>">
                        <input type="hidden" name="action" value="delete_update">
                        <input type="hidden" name="update_edition" value="home">
                        <input type="hidden" name="update_tag" value="<?= escape((string)$uTag) ?>">
                        <button class="button danger" type="submit" style="min-height: 32px; padding: 4px 12px; font-size: 12px;">حذف</button>
                      </form>
                    </td>
                  </tr>
                <?php endforeach; ?>
              </tbody>
            </table>
          </div>
        <?php else: ?>
          <div class="empty-state" style="text-align: center; padding: 25px; color: var(--muted);">
            <strong>در حال حاضر هیچ آپدیتی برای نسخه خانگی ثبت نشده است.</strong>
            <p style="margin-top: 6px;">با استفاده از فرم بالا و انتخاب گزینه «نسخه خانگی» می‌توانید اولین پچ خانگی را بارگذاری کنید.</p>
          </div>
        <?php endif; ?>
      </div>

      <!-- 2. GameNet Edition Active Updates -->
      <div class="card" style="border-top: 3px solid #45cfff;">
        <div class="card-title">
          <div>
            <h2>🎮 آپدیت‌های فعال نسخه گیم‌نت (GameNet Edition)</h2>
            <p>پچ‌های اختصاصی کلوب‌ها و گیم‌نت‌ها به تفکیک تگ در کانال gamenet</p>
          </div>
          <span class="pill" style="background: rgba(69, 207, 255, 0.15); color: #45cfff; border: 1px solid rgba(69, 207, 255, 0.3);"><?= count($updatesData['gamenet'] ?? []) ?> فعال</span>
        </div>
        <?php if (!empty($updatesData['gamenet'])): ?>
          <div style="overflow-x: auto;">
            <table style="width: 100%; border-collapse: collapse; text-align: right; font-size: 13px;">
              <thead>
                <tr style="border-bottom: 1px solid var(--border); color: var(--muted);">
                  <th style="padding: 10px;">تگ گیم‌نت</th>
                  <th style="padding: 10px;">نسخه فعال</th>
                  <th style="padding: 10px;">نوع و حجم</th>
                  <th style="padding: 10px;">تاریخ انتشار</th>
                  <th style="padding: 10px;">لینک دانلود</th>
                  <th style="padding: 10px;">تست API</th>
                  <th style="padding: 10px;">عملیات</th>
                </tr>
              </thead>
              <tbody>
                <?php foreach ($updatesData['gamenet'] as $uTag => $uInfo): ?>
                  <tr style="border-bottom: 1px solid rgba(255,255,255,0.05);">
                    <td style="padding: 12px 10px;"><strong class="ltr" style="color: #45cfff;">🎮 <?= escape((string)$uTag) ?></strong></td>
                    <td style="padding: 12px 10px;"><span class="ltr" style="background: rgba(69, 207, 255, 0.15); padding: 4px 8px; border-radius: 6px; font-weight: bold; color: #45cfff;"><?= escape((string)($uInfo['version'] ?? '')) ?></span></td>
                    <td style="padding: 12px 10px;"><?= escape((string)($uInfo['size'] ?? '')) ?> (<?= escape(strtoupper((string)($uInfo['type'] ?? 'ZIP'))) ?>)</td>
                    <td style="padding: 12px 10px;"><?= escape((string)($uInfo['updated_at_jalali'] ?? $uInfo['updated_at'] ?? '')) ?></td>
                    <td style="padding: 12px 10px;"><a href="<?= escape((string)($uInfo['download_url'] ?? '')) ?>" target="_blank" class="ltr" style="color: var(--primary); text-decoration: none; word-break: break-all;" title="دانلود مستقیم فایل">دریافت فایل ⤓</a></td>
                    <td style="padding: 12px 10px;"><a href="../update_api.php?edition=gamenet&tag=<?= urlencode((string)$uTag) ?>&version=0.0.1" target="_blank" style="color: var(--green); text-decoration: none;">تست (0.0.1) ↗</a></td>
                    <td style="padding: 12px 10px;">
                      <form method="post" class="confirm-form" data-confirm="آیا از غیرفعال‌سازی این آپدیت برای گیم‌نت مطمئن هستید؟" style="margin: 0;">
                        <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>">
                        <input type="hidden" name="action" value="delete_update">
                        <input type="hidden" name="update_edition" value="gamenet">
                        <input type="hidden" name="update_tag" value="<?= escape((string)$uTag) ?>">
                        <button class="button danger" type="submit" style="min-height: 32px; padding: 4px 12px; font-size: 12px;">حذف</button>
                      </form>
                    </td>
                  </tr>
                <?php endforeach; ?>
              </tbody>
            </table>
          </div>
        <?php else: ?>
          <div class="empty-state" style="text-align: center; padding: 25px; color: var(--muted);">
            <strong>در حال حاضر هیچ آپدیتی برای نسخه گیم‌نت ثبت نشده است.</strong>
            <p style="margin-top: 6px;">با استفاده از فرم بالا و انتخاب گزینه «نسخه گیم‌نت» می‌توانید پچ‌های کلوب‌ها را بارگذاری کنید.</p>
          </div>
        <?php endif; ?>
      </div>
    </section>

    <section class="security-reports panel-view" data-panel-view="security_reports">
      <div class="section-heading">
        <div>
          <span class="eyebrow">GAMELAND SHIELD</span>
          <h2>گزارش‌های تخلف و ضد تقلب (Anti-Cheat Logs)</h2>
          <p>ثبت خودکار تلاش برای تقلب یا دستکاری فایل‌ها چه قبل از بازی و چه در حین اجرای بازی، ارسال‌شده توسط کلاینت‌ها</p>
        </div>
        <?php if (!empty($securityReports)): ?>
          <form method="post" class="confirm-form" data-confirm="آیا از پاک‌سازی تمامی گزارش‌های تخلف اطمینان دارید؟">
            <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>">
            <input type="hidden" name="action" value="clear_security_reports">
            <button class="button danger" type="submit">پاک‌سازی همه گزارش‌ها</button>
          </form>
        <?php endif; ?>
      </div>

      <div class="card" style="border-top: 3px solid #ff4d6d; margin-bottom: 24px;">
        <div class="card-title">
          <div>
            <h2>🛡 لاگ تخلفات ثبت‌شده کلاینت‌ها</h2>
            <p>مشخصات سیستم، شناسه سخت‌افزاری و نوع تخلف کشف‌شده همراه زمان دقیق</p>
          </div>
          <span class="pill" style="background: rgba(255, 77, 109, 0.15); color: #ff4d6d; border: 1px solid rgba(255, 77, 109, 0.3);"><?= count($securityReports) ?> گزارش</span>
        </div>

        <?php if (!empty($securityReports)): ?>
          <div style="overflow-x: auto;">
            <table style="width: 100%; border-collapse: collapse; text-align: right; font-size: 13px;">
              <thead>
                <tr style="border-bottom: 1px solid var(--border); color: var(--muted);">
                  <th style="padding: 10px;">زمان ثبت</th>
                  <th style="padding: 10px;">نوع کلاینت</th>
                  <th style="padding: 10px;">مشخصات / شناسه</th>
                  <th style="padding: 10px;">سیستم و کاربر</th>
                  <th style="padding: 10px;">نوع تخلف</th>
                  <th style="padding: 10px;">جزئیات تخلف</th>
                  <th style="padding: 10px;">عملیات</th>
                </tr>
              </thead>
              <tbody>
                <?php foreach ($securityReports as $rep): ?>
                  <?php 
                    $repId = (string)($rep['id'] ?? '');
                    $isHome = ($rep['client_type'] ?? '') === 'home';
                    $devHash = (string)($rep['device_hash'] ?? '');
                    $repTag = (string)($rep['tag'] ?? '');
                  ?>
                  <tr style="border-bottom: 1px solid rgba(255,255,255,0.05);">
                    <td style="padding: 12px 10px; white-space: nowrap;">
                      <div style="color: var(--text); font-weight: bold;"><?= escape((string)($rep['date_jalali'] ?? '')) ?></div>
                      <div class="ltr" style="font-size: 11px; color: var(--muted);"><?= escape((string)($rep['ip'] ?? '')) ?></div>
                    </td>
                    <td style="padding: 12px 10px; white-space: nowrap;">
                      <?php if ($isHome): ?>
                        <span class="status-badge" style="background: rgba(0, 210, 160, 0.15); color: #00d2a0; border: 1px solid rgba(0, 210, 160, 0.3);">🏠 خانگی</span>
                      <?php else: ?>
                        <span class="status-badge" style="background: rgba(69, 207, 255, 0.15); color: #45cfff; border: 1px solid rgba(69, 207, 255, 0.3);">🎮 گیم‌نت</span>
                      <?php endif; ?>
                    </td>
                    <td style="padding: 12px 10px;">
                      <?php if (!empty($repTag)): ?>
                        <strong class="ltr" style="color: #52d5ff;"><?= escape($repTag) ?></strong><br>
                      <?php endif; ?>
                      <?php if (!empty($rep['phone'])): ?>
                        <span class="ltr" style="color: #ffc05c; font-size: 12px;"><?= escape((string)$rep['phone']) ?></span><br>
                      <?php endif; ?>
                      <?php if (!empty($devHash)): ?>
                        <code class="ltr" style="font-size: 11px; color: var(--muted); letter-spacing: 1px;"><?= escape($devHash) ?></code>
                      <?php endif; ?>
                    </td>
                    <td style="padding: 12px 10px; white-space: nowrap;">
                      <div><strong>PC:</strong> <span class="ltr"><?= escape((string)($rep['computer_name'] ?? '-')) ?></span></div>
                      <div style="color: var(--muted); font-size: 11px;"><strong>User:</strong> <span class="ltr"><?= escape((string)($rep['user_name'] ?? '-')) ?></span></div>
                    </td>
                    <td style="padding: 12px 10px; white-space: nowrap;">
                      <span class="status-badge bad" style="font-size: 12px; font-weight: bold; background: rgba(255, 77, 109, 0.18); border: 1px solid rgba(255, 77, 109, 0.35); color: #ff667c;">
                        <?= escape((string)($rep['violation_type'] ?? 'تخلف')) ?>
                      </span>
                    </td>
                    <td style="padding: 12px 10px; max-width: 320px; word-break: break-word;">
                      <div style="font-size: 12px; line-height: 1.5; color: #ffb3c1;">
                        <?= nl2br(escape((string)($rep['violation_details'] ?? ''))) ?>
                      </div>
                    </td>
                    <td style="padding: 12px 10px; white-space: nowrap;">
                      <div style="display: flex; gap: 6px; align-items: center;">
                        <?php if ($isHome && strlen($devHash) === 24): ?>
                          <form method="post" class="confirm-form" data-confirm="آیا می‌خواهید اشتراک این کلاینت خانگی را فوراً تعلیق کنید؟" style="margin: 0;">
                            <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>">
                            <input type="hidden" name="action" value="adjust_home_client">
                            <input type="hidden" name="hash" value="<?= escape($devHash) ?>">
                            <input type="hidden" name="operation" value="suspend">
                            <button class="button warning" type="submit" style="min-height: 28px; padding: 3px 8px; font-size: 11px;">تعلیق کلاینت</button>
                          </form>
                        <?php elseif (!$isHome && !empty($repTag)): ?>
                          <form method="post" class="confirm-form" data-confirm="آیا می‌خواهید اشتراک این گیم‌نت را فوراً تعلیق کنید؟" style="margin: 0;">
                            <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>">
                            <input type="hidden" name="action" value="adjust_subscription">
                            <input type="hidden" name="build" value="<?= escape($repTag) ?>">
                            <input type="hidden" name="operation" value="suspend">
                            <button class="button warning" type="submit" style="min-height: 28px; padding: 3px 8px; font-size: 11px;">تعلیق اشتراک</button>
                          </form>
                        <?php endif; ?>
                        <form method="post" class="confirm-form" data-confirm="این گزارش حذف شود؟" style="margin: 0;">
                          <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>">
                          <input type="hidden" name="action" value="delete_security_report">
                          <input type="hidden" name="report_id" value="<?= escape($repId) ?>">
                          <button class="button danger" type="submit" style="min-height: 28px; padding: 3px 8px; font-size: 11px;">حذف</button>
                        </form>
                      </div>
                    </td>
                  </tr>
                <?php endforeach; ?>
              </tbody>
            </table>
          </div>
        <?php else: ?>
          <div class="empty-state" style="text-align: center; padding: 35px; color: var(--muted);">
            <div style="font-size: 32px; margin-bottom: 10px;">🛡️</div>
            <strong style="color: var(--green); font-size: 15px;">هیچ تخلف امنیتی تاکنون گزارش نشده است.</strong>
            <p style="margin-top: 6px; font-size: 13px;">کلاینت‌های بازی تحت نظارت GAMELAND Shield هستند و هرگونه تلاش برای تقلب یا دستکاری خودکار به این بخش ارسال می‌گردد.</p>
          </div>
        <?php endif; ?>
      </div>
    </section>

    <section class="settings panel-view" data-panel-view="settings">
      <div class="section-heading"><div><span class="eyebrow">SETTINGS</span><h2>تنظیمات پنل</h2><p>تنظیمات امنیتی و مدیریتی حساب شما</p></div></div>
      <article class="card settings-card">
        <div class="settings-mark">⚿</div>
        <div class="settings-content">
          <div class="card-title"><div><h2>تغییر رمز مدیریت</h2><p>برای امنیت، رمز فعلی را وارد کنید. رمز جدید باید دقیقاً ۸ کاراکتر باشد.</p></div><span class="status-badge active">امن</span></div>
          <form method="post" autocomplete="off"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="change_admin_password">
            <div class="settings-fields"><label>رمز فعلی<input type="password" name="current_password" required autocomplete="current-password"></label><label>رمز ۸ کاراکتری جدید<input type="password" name="new_password" minlength="8" maxlength="8" required autocomplete="new-password"></label><label>تکرار رمز جدید<input type="password" name="confirm_password" minlength="8" maxlength="8" required autocomplete="new-password"></label></div>
            <button class="button primary" type="submit">ذخیره رمز جدید</button>
          </form>
        </div>
      </article>
      <article class="card settings-card">
        <div class="settings-mark">↧</div>
        <div class="settings-content">
          <div class="card-title"><div><h2>Backup / Restore</h2><p>Public servers, Mix servers, subscriptions, home clients, security logs, server password and installer-code state are saved in one JSON backup.</p></div><span class="status-badge active">SAFE</span></div>
          <div class="settings-fields">
            <form method="post"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="download_backup">
              <button class="button primary" type="submit">Download Panel Backup</button>
            </form>
            <form method="post" enctype="multipart/form-data" class="confirm-form" data-confirm="Restore this backup and replace current panel data?">
              <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="restore_backup">
              <label>Backup JSON<input class="ltr" type="file" name="panel_backup" accept="application/json,.json" required></label>
              <button class="button warning" type="submit">Restore Backup</button>
            </form>
          </div>
        </div>
      </article>
    </section>
  <?php endif; ?>
</main>
</body>
</html>
