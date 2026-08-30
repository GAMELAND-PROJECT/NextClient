<?php
declare(strict_types=1);

const FILE_SERVERS = 'pinned_servers.txt';
const FILE_TAGS = 'client_tags.txt';
const FILE_PASSWORD = 'server_password.txt';
const MAX_SERVERS = 64;
const MAX_TAGS = 256;

header('X-Content-Type-Options: nosniff');
header('X-Frame-Options: DENY');
header('Referrer-Policy: no-referrer');
header("Content-Security-Policy: default-src 'self'; style-src 'self'; script-src 'self'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'");
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

function validateNewAdminPassword(string $password, string $confirmation): void
{
    if ($password !== $confirmation) {
        throw new RuntimeException('تکرار رمز با رمز جدید یکسان نیست.');
    }
    if (strlen($password) < 12 || strlen($password) > 128) {
        throw new RuntimeException('رمز مدیریت باید بین ۱۲ تا ۱۲۸ کاراکتر باشد.');
    }
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
            $rows[] = ['build' => $parts[0], 'player' => $parts[1], 'expiry' => $parts[2]];
        }
    }
    return $rows;
}

$action = (string)($_POST['action'] ?? '');
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    try {
        if ($action === 'setup') {
            requireValidCsrf();
            if ($configured) {
                throw new RuntimeException('راه‌اندازی اولیه قبلاً انجام شده است.');
            }
            $newPassword = (string)($_POST['new_password'] ?? '');
            validateNewAdminPassword($newPassword, (string)($_POST['confirm_password'] ?? ''));
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
            if (!password_verify((string)($_POST['password'] ?? ''), (string)$config['password_hash'])) {
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
            $currentPassword = (string)($_POST['current_password'] ?? '');
            if (!password_verify($currentPassword, (string)$config['password_hash'])) {
                throw new RuntimeException('رمز فعلی صحیح نیست.');
            }
            $newPassword = (string)($_POST['new_password'] ?? '');
            validateNewAdminPassword($newPassword, (string)($_POST['confirm_password'] ?? ''));
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
            backupAndAtomicWrite(FILE_TAGS, implode("\n", $tags) . ($tags ? "\n" : ''));
            flash('success', count($tags) . ' اشتراک با موفقیت ذخیره شد.');
        } elseif ($action === 'save_password') {
            $password = trim((string)($_POST['server_password'] ?? ''));
            if ($password === '' || strlen($password) > 31 ||
                !preg_match('/^[!#-\[\]-~]+$/', $password) || str_contains($password, ';')) {
                throw new RuntimeException('رمز باید ۱ تا ۳۱ نویسه ASCII و بدون فاصله، کوتیشن، بک‌اسلش یا ; باشد.');
            }
            backupAndAtomicWrite(FILE_PASSWORD, $password . "\n");
            flash('success', 'رمز مشترک سرورها جایگزین شد.');
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
$passwordConfigured = false;
if ($authenticated) {
    try {
        $serverText = trim(readTextFile(FILE_SERVERS));
        $tagRows = parseTagRows(readTextFile(FILE_TAGS));
        $passwordConfigured = trim(readTextFile(FILE_PASSWORD, 256)) !== '';
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
  <title>مدیریت Allclient</title>
  <link rel="stylesheet" href="style.css">
  <script src="panel.js" defer></script>
</head>
<body>
<main class="shell <?= $authenticated ? '' : 'login-shell' ?>">
  <header class="topbar">
    <div><span class="eyebrow">GAMELAND PROJECT</span><h1>کنترل‌پنل Allclient</h1></div>
    <?php if ($authenticated): ?>
      <form method="post"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="logout"><button class="button ghost" type="submit">خروج امن</button></form>
    <?php endif; ?>
  </header>

  <?php if ($flash): ?><div class="notice <?= escape((string)$flash['type']) ?>"><?= escape((string)$flash['message']) ?></div><?php endif; ?>

  <?php if (!$configured): ?>
    <section class="card login-card">
      <div class="icon-lock">◆</div><h2>راه‌اندازی اولیه</h2><p>رمز مدیر را تعیین کنید. این صفحه پس از ثبت رمز برای همیشه بسته می‌شود.</p>
      <form method="post" autocomplete="off">
        <input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="setup">
        <label>رمز جدید<input type="password" name="new_password" minlength="12" maxlength="128" required autofocus autocomplete="new-password"></label>
        <label>تکرار رمز<input type="password" name="confirm_password" minlength="12" maxlength="128" required autocomplete="new-password"></label>
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
      <div class="stat"><strong><?= count($tagRows) ?></strong><span>اشتراک گیمنت</span></div>
      <div class="stat"><strong class="<?= $passwordConfigured ? 'ok' : 'bad' ?>"><?= $passwordConfigured ? 'فعال' : 'تنظیم‌نشده' ?></strong><span>رمز سرورها</span></div>
    </section>

    <section class="grid">
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
        <div class="card-title"><div><h2>رمز مدیریت پنل</h2><p>برای تغییر، رمز فعلی نیز الزامی است.</p></div><span class="dot active"></span></div>
        <form method="post" autocomplete="off"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="change_admin_password">
          <label>رمز فعلی<input type="password" name="current_password" required autocomplete="current-password"></label>
          <label>رمز جدید<input type="password" name="new_password" minlength="12" maxlength="128" required autocomplete="new-password"></label>
          <label>تکرار رمز جدید<input type="password" name="confirm_password" minlength="12" maxlength="128" required autocomplete="new-password"></label>
          <button class="button secondary" type="submit">تغییر رمز مدیریت</button>
        </form>
      </article>
    </section>

    <section class="card subscriptions">
      <div class="card-title"><div><h2>اشتراک گیمنت‌ها</h2><p>تگ Build، تگ نمایش بازیکن و تاریخ انقضای شمسی</p></div><button class="button secondary" id="add-row" type="button">+ گیمنت جدید</button></div>
      <form method="post"><input type="hidden" name="csrf" value="<?= escape(csrfToken()) ?>"><input type="hidden" name="action" value="save_tags">
        <div class="table-head"><span>تگ Build</span><span>تگ بازیکن</span><span>انقضا</span><span></span></div>
        <div id="tag-rows">
          <?php foreach ($tagRows as $row): ?>
            <div class="tag-row"><input name="build_tag[]" maxlength="64" pattern="[A-Za-z0-9_-]+" value="<?= escape($row['build']) ?>" required><input name="player_tag[]" maxlength="12" pattern="[A-Za-z0-9_-]+" value="<?= escape($row['player']) ?>" required><input name="expiry[]" class="ltr" maxlength="10" pattern="\d{4}/\d{2}/\d{2}" value="<?= escape($row['expiry']) ?>" required><button class="remove-row" type="button" aria-label="حذف">×</button></div>
          <?php endforeach; ?>
        </div>
        <button class="button primary" type="submit">ذخیره اشتراک‌ها</button>
      </form>
    </section>
    <template id="tag-row-template"><div class="tag-row"><input name="build_tag[]" maxlength="64" pattern="[A-Za-z0-9_-]+" placeholder="IMORTAL_GONBAD" required><input name="player_tag[]" maxlength="12" pattern="[A-Za-z0-9_-]+" placeholder="IM" required><input name="expiry[]" class="ltr" maxlength="10" pattern="\d{4}/\d{2}/\d{2}" placeholder="1405/06/25" required><button class="remove-row" type="button" aria-label="حذف">×</button></div></template>
  <?php endif; ?>
</main>
</body>
</html>
