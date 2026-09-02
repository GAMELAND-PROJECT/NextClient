<?php
declare(strict_types=1);

const FILE_SERVERS = 'pinned_servers.txt';
const FILE_TAGS = 'client_tags.txt';
const FILE_PASSWORD = 'server_password.txt';
const FILE_INSTALLER_ACCESS = '.installer_access.php';
const FILE_SUSPENDED_SUBSCRIPTIONS = '.suspended_subscriptions.php';
const MAX_SERVERS = 64;
const MAX_TAGS = 256;

header('X-Content-Type-Options: nosniff');
header('X-Frame-Options: DENY');
header('Referrer-Policy: no-referrer');
header("Content-Security-Policy: default-src 'self'; style-src 'self'; script-src 'self'; worker-src 'self'; manifest-src 'self'; img-src 'self'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'");
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

function validateTags(array $buildTags, array $playerTags, array $expiries): array
{
    $rowCount = max(count($buildTags), count($playerTags), count($expiries));
    if ($rowCount > MAX_TAGS) {
        throw new RuntimeException('تعداد گیمنت‌ها بیش از حد مجاز است.');
    }

    $result = [];
    for ($index = 0; $index < $rowCount; ++$index) {
        $build = trim((string)($buildTags[$index] ?? ''));
        $player = trim((string)($playerTags[$index] ?? ''));
        $expiry = trim((string)($expiries[$index] ?? ''));
        if ($build === '' && $player === '' && $expiry === '') { continue; }
        if (!preg_match('/^[A-Za-z0-9_-]{1,64}$/', $build)) {
            throw new RuntimeException('تگ Build در ردیف ' . ($index + 1) . ' نامعتبر است.');
        }
        if (!preg_match('/^[A-Za-z0-9_-]{1,12}$/', $player)) {
            throw new RuntimeException('تگ نام بازیکن در ردیف ' . ($index + 1) . ' نامعتبر است.');
        }
        if (!validJalaliDate($expiry)) {
            throw new RuntimeException('تاریخ شمسی ردیف ' . ($index + 1) . ' نامعتبر است؛ نمونه: 1405/06/25');
        }
        $key = strtoupper($build);
        if (isset($result[$key])) {
            throw new RuntimeException('تگ Build تکراری است: ' . $build);
        }
        $result[$key] = $build . ' | ' . $player . ' | ' . $expiry;
    }
    return array_values($result);
}

function parseTagRows(string $content): array
{
    $rows = [];
    foreach (normalizedLines($content) as $line) {
        $parts = array_map('trim', explode('|', $line));
        if (count($parts) === 3) {
            $rows[] = ['build' => $parts[0], 'player' => $parts[1],
                'expiry' => $parts[2], 'suspended' => false];
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
            [(string)($row['player'] ?? '')], [(string)($row['expiry'] ?? '')]);
        if (!$validated) { continue; }
        [$build, $player, $expiry] = array_map('trim', explode('|', $validated[0]));
        $rows[] = ['build' => $build, 'player' => $player,
            'expiry' => $expiry, 'suspended' => true];
    }
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
            [(string)($row['player'] ?? '')], [(string)($row['expiry'] ?? '')]);
        if (!$validated) { continue; }
        if (!empty($row['suspended'])) {
            $suspended[] = [
                'build' => (string)$row['build'],
                'player' => (string)$row['player'],
                'expiry' => (string)$row['expiry'],
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

function readInstallerAccessState(): array
{
    $path = dataPath(FILE_INSTALLER_ACCESS);
    if (!is_file($path)) {
        return ['active' => false, 'code_hash' => '', 'created_at' => ''];
    }

    if (!defined('ALLCLIENT_INSTALLER_ACCESS_INTERNAL')) {
        define('ALLCLIENT_INSTALLER_ACCESS_INTERNAL', true);
    }
    $state = require $path;
    if (!is_array($state)) {
        throw new RuntimeException('فایل وضعیت کد نصب معتبر نیست.');
    }
    return [
        'active' => !empty($state['active']),
        'code_hash' => (string)($state['code_hash'] ?? ''),
        'created_at' => (string)($state['created_at'] ?? ''),
    ];
}

function writeInstallerAccessState(bool $active, string $codeHash = ''): void
{
    if ($active && !preg_match('/\A[a-f0-9]{64}\z/', $codeHash)) {
        throw new RuntimeException('هش کد نصب معتبر نیست.');
    }

    $state = [
        'active' => $active,
        'code_hash' => $active ? $codeHash : '',
        'created_at' => $active ? gmdate('c') : '',
    ];
    $content = "<?php\ndeclare(strict_types=1);\n\n" .
        "if (!defined('ALLCLIENT_INSTALLER_ACCESS_INTERNAL')) {\n" .
        "    http_response_code(404);\n    exit;\n}\n\nreturn " .
        var_export($state, true) . ";\n";
    backupAndAtomicWrite(FILE_INSTALLER_ACCESS, $content);
    @chmod(dataPath(FILE_INSTALLER_ACCESS), 0600);
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
        } elseif ($action === 'save_tags') {
            $tags = validateTags((array)($_POST['build_tag'] ?? []),
                (array)($_POST['player_tag'] ?? []), (array)($_POST['expiry'] ?? []));
            $rows = subscriptionRowsByKey();
            foreach ($rows as $key => $row) {
                if (empty($row['suspended'])) { unset($rows[$key]); }
            }
            foreach ($tags as $tag) {
                [$build, $player, $expiry] = array_map('trim', explode('|', $tag));
                $rows[strtoupper($build)] = compact('build', 'player', 'expiry') + ['suspended' => false];
            }
            writeSubscriptionRows($rows);
            flash('success', count($tags) . ' اشتراک با موفقیت ذخیره شد.');
        } elseif ($action === 'add_subscription' || $action === 'save_subscription') {
            $validated = validateTags([(string)($_POST['build_tag'] ?? '')],
                [(string)($_POST['player_tag'] ?? '')], [(string)($_POST['expiry'] ?? '')]);
            [$build, $player, $expiry] = array_map('trim', explode('|', $validated[0]));
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
            $rows[$newKey] = compact('build', 'player', 'expiry', 'suspended');
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
        } elseif ($action === 'generate_installer_code') {
            $installerCode = str_pad((string)random_int(0, 99999999), 8, '0', STR_PAD_LEFT);
            writeInstallerAccessState(true, hash('sha256', $installerCode));
            $_SESSION['generated_installer_code'] = $installerCode;
            flash('success', 'کد نصب جدید فعال شد. همین حالا آن را کپی کنید.');
        } elseif ($action === 'revoke_installer_code') {
            writeInstallerAccessState(false);
            unset($_SESSION['generated_installer_code']);
            flash('success', 'کد نصب فعال فوراً لغو شد.');
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
$tagRows = [];
$activeSubscriptions = 0;
$suspendedSubscriptions = 0;
$expiringSubscriptions = 0;
$passwordConfigured = false;
$installerAccessState = ['active' => false, 'code_hash' => '', 'created_at' => ''];
$generatedInstallerCode = '';
if ($authenticated) {
    try {
        $serverText = trim(readTextFile(FILE_SERVERS));
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
        $installerAccessState = readInstallerAccessState();
        $generatedInstallerCode = (string)($_SESSION['generated_installer_code'] ?? '');
        unset($_SESSION['generated_installer_code']);
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
      <div class="stat"><strong class="<?= $installerAccessState['active'] ? 'ok' : 'bad' ?>"><?= $installerAccessState['active'] ? 'فعال' : 'غیرفعال' ?></strong><span>کد نصب</span></div>
    </section>

    <nav class="panel-tabs" aria-label="بخش‌های پنل">
      <button class="panel-tab active" type="button" data-panel="dashboard" aria-selected="true"><span class="tab-icon">⌂</span><span>داشبورد</span></button>
      <button class="panel-tab" type="button" data-panel="subscriptions" aria-selected="false"><span class="tab-icon">◫</span><span>اشتراک‌ها</span><b><?= count($tagRows) ?></b></button>
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

      <article class="card password-card">
        <div class="card-title"><div><h2>رمز مشترک سرورها</h2><p>رمز موجود برای امنیت نمایش داده نمی‌شود.</p></div><span class="dot <?= $passwordConfigured ? 'active' : '' ?>"></span></div>
        <form method="post" autocomplete="off"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="save_password">
          <label>رمز جدید<input class="ltr" type="password" name="server_password" maxlength="31" required autocomplete="new-password"></label>
          <button class="button warning" type="submit">جایگزینی رمز</button>
        </form>
      </article>

      <article class="card password-card">
        <div class="card-title"><div><h2>کد نصب Allclient</h2><p>کد ۸ رقمی جدید بسازید یا کد فعال را فوراً لغو کنید.</p></div><span class="dot <?= $installerAccessState['active'] ? 'active' : '' ?>"></span></div>
        <?php if ($generatedInstallerCode !== ''): ?>
          <div class="installer-code ltr" aria-label="کد نصب جدید"><?= escape($generatedInstallerCode) ?></div>
          <p class="security-note">این کد فقط همین یک بار نمایش داده می‌شود؛ اکنون آن را کپی کنید.</p>
        <?php elseif ($installerAccessState['active']): ?>
          <p class="security-note">یک کد نصب فعال است. برای امنیت، مقدار آن دوباره نمایش داده نمی‌شود.</p>
        <?php else: ?>
          <p class="security-note">در حال حاضر هیچ کد آنلاین فعالی وجود ندارد.</p>
        <?php endif; ?>
        <form method="post"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>">
          <?php if ($installerAccessState['active']): ?>
            <input type="hidden" name="action" value="revoke_installer_code"><button class="button warning" type="submit">لغو فوری کد فعال</button>
          <?php else: ?>
            <input type="hidden" name="action" value="generate_installer_code"><button class="button primary" type="submit">تولید کد ۸ رقمی جدید</button>
          <?php endif; ?>
        </form>
      </article>

    </section>
    </section>

    <section class="subscriptions panel-view" data-panel-view="subscriptions">
      <div class="section-heading">
        <div><span class="eyebrow">SUBSCRIPTIONS</span><h2>مدیریت اشتراک گیمنت‌ها</h2><p>وضعیت، زمان باقی‌مانده، تمدید و تعلیق فوری</p></div>
        <button class="button primary" id="show-add-subscription" type="button">+ اشتراک جدید</button>
      </div>

      <form class="card add-subscription" id="add-subscription" method="post" hidden>
        <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="add_subscription">
        <div class="form-heading"><div><h3>افزودن گیمنت</h3><p>اشتراک پس از ذخیره فوراً برای کلاینت قابل استفاده است.</p></div><button class="icon-button" id="close-add-subscription" type="button" aria-label="بستن">×</button></div>
        <div class="subscription-fields"><label>نام یا تگ Build<input class="ltr" name="build_tag" maxlength="64" pattern="[A-Za-z0-9_-]+" placeholder="IMORTAL_GONBAD" required></label><label>تگ بازیکن<input class="ltr" name="player_tag" maxlength="12" pattern="[A-Za-z0-9_-]+" placeholder="IM" required></label><label>انقضای شمسی<input class="ltr" name="expiry" maxlength="10" pattern="\d{4}/\d{2}/\d{2}" placeholder="1405/06/25" required></label></div>
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
                <header class="modal-header"><div class="modal-profile"><span class="profile-avatar"><?= escape(strtoupper(substr($row['player'], 0, 2))) ?></span><div><span class="status-badge <?= escape($row['state']) ?>"><?= escape($row['state_label']) ?></span><h3 id="profile-title-<?= (int)$rowIndex ?>"><?= escape($row['build']) ?></h3><p>تگ بازیکن: <b class="ltr"><?= escape($row['player']) ?></b> · انقضا: <b class="ltr"><?= escape($row['expiry']) ?></b></p></div></div><button class="modal-close" type="button" data-close-modal aria-label="بستن پنجره">×</button></header>
                <div class="modal-overview"><div><span>وضعیت فعلی</span><strong class="<?= escape($row['state']) ?>"><?= escape($row['state_label']) ?></strong></div><div><span>اعتبار</span><strong><?= abs((int)$row['days_remaining']) ?> روز <?= $row['days_remaining'] < 0 ? 'گذشته' : 'باقی‌مانده' ?></strong></div><div><span>تاریخ انقضا</span><strong class="ltr"><?= escape($row['expiry']) ?></strong></div></div>
                <div class="manage-panel">
                <section class="manage-block renewal-block"><div class="manage-title"><strong>تمدید اعتبار</strong><span>از تاریخ فعلی یا امروز محاسبه می‌شود</span></div><div class="quick-actions"><form method="post" class="month-actions"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="adjust_subscription"><input type="hidden" name="operation" value="extend_months"><input type="hidden" name="build" value="<?= escape($row['build']) ?>"><button type="submit" name="months" value="1">+ ۱ ماه</button><button type="submit" name="months" value="2">+ ۲ ماه</button><button type="submit" name="months" value="3">+ ۳ ماه</button></form><form method="post" class="days-action"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="adjust_subscription"><input type="hidden" name="operation" value="extend_days"><input type="hidden" name="build" value="<?= escape($row['build']) ?>"><input type="number" name="days" min="1" max="3650" inputmode="numeric" placeholder="تعداد روز" required><button class="button secondary" type="submit">تمدید دلخواه</button></form></div></section>
                <section class="manage-block access-block"><div class="manage-title"><strong>کنترل دسترسی</strong><span>تغییر وضعیت بلافاصله روی فایل کلاینت اعمال می‌شود</span></div><form method="post" class="confirm-form" data-confirm="<?= $row['suspended'] ? 'اشتراک دوباره فعال شود؟' : 'دسترسی آنلاین این گیمنت فوراً معلق شود؟' ?>"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="adjust_subscription"><input type="hidden" name="operation" value="<?= $row['suspended'] ? 'resume' : 'suspend' ?>"><input type="hidden" name="build" value="<?= escape($row['build']) ?>"><button class="button <?= $row['suspended'] ? 'primary' : 'warning' ?>" type="submit"><?= $row['suspended'] ? 'فعال‌سازی مجدد' : 'تعلیق دسترسی آنلاین' ?></button></form></section>
                <details class="subscription-edit"><summary>ویرایش اطلاعات پروفایل</summary><div class="edit-actions"><form method="post" class="edit-form"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="save_subscription"><input type="hidden" name="original_build" value="<?= escape($row['build']) ?>"><div class="subscription-fields"><label>تگ Build<input class="ltr" name="build_tag" maxlength="64" pattern="[A-Za-z0-9_-]+" value="<?= escape($row['build']) ?>" required></label><label>تگ بازیکن<input class="ltr" name="player_tag" maxlength="12" pattern="[A-Za-z0-9_-]+" value="<?= escape($row['player']) ?>" required></label><label>انقضای شمسی<input class="ltr" name="expiry" maxlength="10" pattern="\d{4}/\d{2}/\d{2}" value="<?= escape($row['expiry']) ?>" required></label></div><button class="button secondary" type="submit">ذخیره اطلاعات</button></form><form method="post" class="confirm-form delete-form" data-confirm="این اشتراک برای همیشه حذف شود؟"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="delete_subscription"><input type="hidden" name="build" value="<?= escape($row['build']) ?>"><button class="button danger" type="submit">حذف کامل پروفایل</button></form></div></details>
                </div>
              </div>
            </dialog>
          </article>
        <?php endforeach; ?>
        <?php if (!$tagRows): ?><div class="card empty-state"><strong>هنوز اشتراکی ثبت نشده است</strong><span>با دکمه «اشتراک جدید» اولین گیمنت را اضافه کنید.</span></div><?php endif; ?>
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
    </section>
  <?php endif; ?>
</main>
</body>
</html>
