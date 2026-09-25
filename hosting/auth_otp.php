<?php
/**
 * GAMELAND Cloud Authentication & MrOTP USSD Gateway
 * 
 * Endpoints:
 *   POST action=request_otp   (mobile) -> returns USSD code to dial
 *   POST action=verify_otp    (mobile, otp, password) -> creates account, returns token
 *   POST action=login         (mobile, password) -> validates credentials, returns token
 *   GET  action=check_token   (token) -> validates active session
 */

header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type, Authorization');
header('Content-Type: application/json; charset=utf-8');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    exit(0);
}

// Configuration
$MROTP_API_KEY = "e8b8180a-d6c5-4065-94dc-e902e3151789"; // Set your MrOTP API Key here
$DATA_DIR = __DIR__ . DIRECTORY_SEPARATOR . 'storage' . DIRECTORY_SEPARATOR . 'cloud_auth';
$DB_FILE = $DATA_DIR . DIRECTORY_SEPARATOR . 'gameland_users.sqlite';

if (!is_dir($DATA_DIR)) {
    @mkdir($DATA_DIR, 0755, true);
}

// Initialize SQLite database
function getDb($dbFile) {
    $db = new PDO("sqlite:" . $dbFile);
    $db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    
    $db->exec("CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        mobile TEXT UNIQUE NOT NULL,
        password_hash TEXT NOT NULL,
        password_plain TEXT,
        token TEXT UNIQUE NOT NULL,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        last_login DATETIME DEFAULT CURRENT_TIMESTAMP
    )");
    try {
        $db->exec("ALTER TABLE users ADD COLUMN password_plain TEXT");
    } catch (Exception $e) {}

    $db->exec("CREATE TABLE IF NOT EXISTS otp_sessions (
        mobile TEXT PRIMARY KEY,
        otp TEXT NOT NULL,
        ussd_code TEXT NOT NULL,
        expires_at INTEGER NOT NULL,
        created_at INTEGER NOT NULL
    )");

    $db->exec("CREATE TABLE IF NOT EXISTS otp_logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        mobile TEXT NOT NULL,
        created_at INTEGER NOT NULL
    )");
    $db->exec("CREATE INDEX IF NOT EXISTS idx_otp_logs_mobile_time ON otp_logs(mobile, created_at)");

    return $db;
}

$input = json_decode(file_get_contents('php://input'), true) ?? $_POST;
$action = $input['action'] ?? $_GET['action'] ?? '';

try {
    $db = getDb($DB_FILE);

    if ($action === 'request_otp') {
        $mobile = trim($input['mobile'] ?? '');
        if (!preg_match('/^09[0-9]{9}$/', $mobile)) {
            echo json_encode(['success' => false, 'message' => 'شماره موبایل نامعتبر است (مثال: 09121234567)'], JSON_UNESCAPED_UNICODE);
            exit;
        }

        // Check monthly quota (Max 5 requests per 30 days)
        $maxMonthly = 5;
        $thirtyDaysAgo = time() - (30 * 86400);
        $stmt = $db->prepare("SELECT COUNT(*) FROM otp_logs WHERE mobile = :mobile AND created_at > :since");
        $stmt->execute([':mobile' => $mobile, ':since' => $thirtyDaysAgo]);
        $monthlyCount = (int)$stmt->fetchColumn();

        if ($monthlyCount >= $maxMonthly) {
            echo json_encode([
                'success' => false,
                'message' => 'سقف مجاز درخواست کد (حداکثر ۵ بار در ماه) برای این شماره تکمیل شده است.'
            ], JSON_UNESCAPED_UNICODE);
            exit;
        }

        // Check 120-second cooldown between requests (Prevent bypass by closing dialog)
        $stmt = $db->prepare("SELECT * FROM otp_sessions WHERE mobile = :mobile");
        $stmt->execute([':mobile' => $mobile]);
        $activeSession = $stmt->fetch(PDO::FETCH_ASSOC);

        if ($activeSession && isset($activeSession['created_at'])) {
            $timePassed = time() - (int)$activeSession['created_at'];
            if ($timePassed < 120) {
                $remaining = 120 - $timePassed;
                echo json_encode([
                    'success' => false,
                    'cooldown' => $remaining,
                    'message' => "جهت درخواست مجدد، لطفاً {$remaining} ثانیه صبوری فرمایید."
                ], JSON_UNESCAPED_UNICODE);
                exit;
            }
        }

        // Generate 5-digit numeric OTP
        $randomOtp = (string)random_int(10000, 99999);
        $validMinutes = 3; // 3 minutes validity

        // Call MrOTP setOTP API with IPv4 enforcement, DNS cache, and auto-retry to prevent DNS timeout on first call
        $response = false;
        $curlError = '';
        for ($attempt = 1; $attempt <= 2; $attempt++) {
            $ch = curl_init();
            curl_setopt_array($ch, [
                CURLOPT_URL => 'https://my.mrotp.ir/api/OTP/v1/setOTP',
                CURLOPT_RETURNTRANSFER => true,
                CURLOPT_CONNECTTIMEOUT => 8,
                CURLOPT_TIMEOUT => 15,
                CURLOPT_IPRESOLVE => CURL_IPRESOLVE_V4,
                CURLOPT_DNS_CACHE_TIMEOUT => 7200,
                CURLOPT_SSL_VERIFYPEER => true,
                CURLOPT_SSL_VERIFYHOST => 2,
                CURLOPT_POST => true,
                CURLOPT_POSTFIELDS => [
                    'apiKey' => $MROTP_API_KEY,
                    'mobile' => $mobile,
                    'OTP' => $randomOtp,
                    'validTime' => (string)$validMinutes,
                    'type' => 'SMS'
                ]
            ]);
            $response = curl_exec($ch);
            $curlError = curl_error($ch);
            curl_close($ch);

            if (!$curlError && $response) {
                break;
            }
            if ($attempt < 2) {
                usleep(250000); // 250ms backoff before second attempt
            }
        }

        if ($curlError || !$response) {
            echo json_encode(['success' => false, 'message' => 'خطا در ارتباط با سامانه OTP: ' . ($curlError ?: 'پاسخی دریافت نشد')]);
            exit;
        }

        $mrotpResult = json_decode($response, true);
        if (!$mrotpResult || !isset($mrotpResult['code']) || (int)$mrotpResult['code'] <= 0) {
            $errDetail = $mrotpResult['message'] ?? 'خطای ناشناخته در سامانه پیامکی';
            echo json_encode(['success' => false, 'message' => 'سامانه پیامک: ' . $errDetail]);
            exit;
        }

        $generatedOtp = $randomOtp;
        $ussdCode = $mrotpResult['USSD'] ?? '';

        // Store OTP in local db with 3 minutes expiration
        $expiresAt = time() + ($validMinutes * 60);
        $stmt = $db->prepare("INSERT OR REPLACE INTO otp_sessions (mobile, otp, ussd_code, expires_at, created_at) VALUES (:mobile, :otp, :ussd, :exp, :created)");
        $stmt->execute([
            ':mobile' => $mobile,
            ':otp' => $generatedOtp,
            ':ussd' => $ussdCode,
            ':exp' => $expiresAt,
            ':created' => time()
        ]);

        // Log successful request for monthly quota
        $db->prepare("INSERT INTO otp_logs (mobile, created_at) VALUES (:mobile, :created)")
           ->execute([':mobile' => $mobile, ':created' => time()]);

        $remainingRequests = $maxMonthly - ($monthlyCount + 1);

        echo json_encode([
            'success' => true,
            'message' => 'کد تایید پیامکی ارسال شد. (' . $remainingRequests . ' بار دیگر در این ماه مجاز هستید)',
            'ussd' => $ussdCode,
            'mobile' => $mobile,
            'expires_in' => $validMinutes * 60,
            'cooldown' => 120,
            'remaining_monthly' => $remainingRequests
        ], JSON_UNESCAPED_UNICODE);
        exit;
    }

    if ($action === 'verify_otp') {
        $mobile = trim($input['mobile'] ?? '');
        $otp = trim($input['otp'] ?? '');
        $password = trim((string)($input['password'] ?? ''));

        if (!preg_match('/^09[0-9]{9}$/', $mobile)) {
            echo json_encode(['success' => false, 'message' => 'شماره موبایل نامعتبر است.'], JSON_UNESCAPED_UNICODE);
            exit;
        }

        // Verify OTP session
        $stmt = $db->prepare("SELECT * FROM otp_sessions WHERE mobile = :mobile");
        $stmt->execute([':mobile' => $mobile]);
        $session = $stmt->fetch(PDO::FETCH_ASSOC);

        if (!$session) {
            echo json_encode(['success' => false, 'message' => 'درخواست کدی برای این شماره یافت نشد. ابتدا درخواست کد دهید.'], JSON_UNESCAPED_UNICODE);
            exit;
        }

        if (time() > (int)$session['expires_at']) {
            echo json_encode(['success' => false, 'message' => 'کد تایید منقضی شده است. لطفاً مجدداً درخواست دهید.'], JSON_UNESCAPED_UNICODE);
            exit;
        }

        if ($session['otp'] !== $otp) {
            echo json_encode(['success' => false, 'message' => 'کد تایید وارد شده اشتباه است.'], JSON_UNESCAPED_UNICODE);
            exit;
        }

        // Check if user already exists
        $userStmt = $db->prepare("SELECT * FROM users WHERE mobile = :mobile");
        $userStmt->execute([':mobile' => $mobile]);
        $existingUser = $userStmt->fetch(PDO::FETCH_ASSOC);

        // Case 1: User left password empty -> Retrieve existing password
        if (empty($password)) {
            if (!$existingUser) {
                echo json_encode(['success' => false, 'message' => 'شماره شما هنوز ثبت‌نام نشده است. لطفاً یک رمز عبور تعیین کنید.'], JSON_UNESCAPED_UNICODE);
                exit;
            }
            $plainPass = $existingUser['password_plain'] ?? '';
            if (empty($plainPass)) {
                // If legacy account has no plain password saved, generate a clean 6-digit PIN so user always gets their password!
                $plainPass = (string)mt_rand(100000, 999999);
                $newHash = password_hash($plainPass, PASSWORD_BCRYPT);
                $db->prepare("UPDATE users SET password_hash = :h, password_plain = :p WHERE id = :id")
                   ->execute([':h' => $newHash, ':p' => $plainPass, ':id' => $existingUser['id']]);
            }

            // Successfully retrieve forgotten password!
            $newToken = bin2hex(random_bytes(24));
            $db->prepare("UPDATE users SET token = :token, last_login = datetime('now') WHERE id = :id")->execute([':token' => $newToken, ':id' => $existingUser['id']]);
            $db->prepare("DELETE FROM otp_sessions WHERE mobile = :mobile")->execute([':mobile' => $mobile]);

            echo json_encode([
                'success' => true,
                'is_existing' => true,
                'saved_password' => $plainPass,
                'token' => $newToken,
                'mobile' => $mobile,
                'message' => 'رمز عبور با موفقیت بازیابی شد.'
            ], JSON_UNESCAPED_UNICODE);
            exit;
        }

        // Case 2: New password is provided (Register or Reset)
        if (strlen($password) < 4) {
            echo json_encode(['success' => false, 'message' => 'رمز عبور باید حداقل ۴ نویسه باشد.'], JSON_UNESCAPED_UNICODE);
            exit;
        }

        $passwordHash = password_hash($password, PASSWORD_BCRYPT);
        $token = bin2hex(random_bytes(24));

        $stmt = $db->prepare("INSERT INTO users (mobile, password_hash, password_plain, token, last_login) VALUES (:mobile, :hash, :plain, :token, datetime('now'))
            ON CONFLICT(mobile) DO UPDATE SET password_hash = :hash, password_plain = :plain, token = :token, last_login = datetime('now')");
        $stmt->execute([
            ':mobile' => $mobile,
            ':hash' => $passwordHash,
            ':plain' => $password,
            ':token' => $token
        ]);

        $stmt = $db->prepare("DELETE FROM otp_sessions WHERE mobile = :mobile");
        $stmt->execute([':mobile' => $mobile]);

        echo json_encode([
            'success' => true,
            'is_existing' => (bool)$existingUser,
            'saved_password' => $password,
            'token' => $token,
            'mobile' => $mobile,
            'message' => $existingUser ? 'رمز عبور با موفقیت تغییر یافت و وارد شدید.' : 'ثبت‌نام با موفقیت انجام شد و وارد شدید.'
        ], JSON_UNESCAPED_UNICODE);
        exit;
    }

    if ($action === 'login') {
        $mobile = trim($input['mobile'] ?? '');
        $password = (string)($input['password'] ?? '');

        $stmt = $db->prepare("SELECT * FROM users WHERE mobile = :mobile");
        $stmt->execute([':mobile' => $mobile]);
        $user = $stmt->fetch(PDO::FETCH_ASSOC);

        if (!$user || !password_verify($password, $user['password_hash'])) {
            echo json_encode(['success' => false, 'message' => 'شماره موبایل یا رمز عبور اشتباه است.']);
            exit;
        }

        // Refresh token on login & cache password_plain
        $newToken = bin2hex(random_bytes(24));
        $stmt = $db->prepare("UPDATE users SET token = :token, password_plain = :plain, last_login = datetime('now') WHERE id = :id");
        $stmt->execute([':token' => $newToken, ':plain' => $password, ':id' => $user['id']]);

        echo json_encode([
            'success' => true,
            'message' => 'ورود با موفقیت انجام شد.',
            'token' => $newToken,
            'mobile' => $mobile
        ]);
        exit;
    }

    if ($action === 'check_token') {
        $token = trim($input['token'] ?? $_SERVER['HTTP_AUTHORIZATION'] ?? '');
        $token = str_replace('Bearer ', '', $token);

        $stmt = $db->prepare("SELECT mobile, created_at, last_login FROM users WHERE token = :token");
        $stmt->execute([':token' => $token]);
        $user = $stmt->fetch(PDO::FETCH_ASSOC);

        if (!$user) {
            echo json_encode(['success' => false, 'authenticated' => false]);
            exit;
        }

        echo json_encode([
            'success' => true,
            'authenticated' => true,
            'mobile' => $user['mobile']
        ]);
        exit;
    }

    echo json_encode(['success' => false, 'message' => 'اکشن نامعتبر است.']);
} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['success' => false, 'message' => 'Server Error: ' . $e->getMessage()]);
}