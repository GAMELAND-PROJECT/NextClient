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
        token TEXT UNIQUE NOT NULL,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        last_login DATETIME DEFAULT CURRENT_TIMESTAMP
    )");

    $db->exec("CREATE TABLE IF NOT EXISTS otp_sessions (
        mobile TEXT PRIMARY KEY,
        otp TEXT NOT NULL,
        ussd_code TEXT NOT NULL,
        expires_at INTEGER NOT NULL,
        created_at INTEGER NOT NULL
    )");

    return $db;
}

$input = json_decode(file_get_contents('php://input'), true) ?? $_POST;
$action = $input['action'] ?? $_GET['action'] ?? '';

try {
    $db = getDb($DB_FILE);

    if ($action === 'request_otp') {
        $mobile = trim($input['mobile'] ?? '');
        if (!preg_match('/^09[0-9]{9}$/', $mobile)) {
            echo json_encode(['success' => false, 'message' => 'شماره موبایل نامعتبر است (مثال: 09121234567)']);
            exit;
        }

        // Generate 4-digit numeric OTP
        $randomOtp = (string)random_int(1000, 9999);
        $validMinutes = 3; // 3 minutes validity

        // Call MrOTP setOTP API
        $ch = curl_init();
        curl_setopt_array($ch, [
            CURLOPT_URL => 'https://my.mrotp.ir/api/OTP/v1/setRandomOTP',
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_TIMEOUT => 10,
            CURLOPT_POST => true,
            CURLOPT_POSTFIELDS => [
                'apiKey' => $MROTP_API_KEY,
                'mobile' => $mobile,
                'length' => '5',
                'validTime' => (string)$validMinutes,
                'type' => 'SMS'
            ]
        ]);
        $response = curl_exec($ch);
        $curlError = curl_error($ch);
        curl_close($ch);

        if ($curlError) {
            echo json_encode(['success' => false, 'message' => 'خطا در ارتباط با سامانه OTP: ' . $curlError]);
            exit;
        }

        $mrotpResult = json_decode($response, true);
        $generatedOtp = (string)($mrotpResult['OTP'] ?? $randomOtp);
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

        echo json_encode([
            'success' => true,
            'message' => 'کد تایید پیامکی ارسال شد. لطفاً آن را وارد کنید.',
            'ussd' => $ussdCode,
            'mobile' => $mobile,
            'expires_in' => $validMinutes * 60
        ]);
        exit;
    }

    if ($action === 'verify_otp') {
        $mobile = trim($input['mobile'] ?? '');
        $otp = trim($input['otp'] ?? '');
        $password = (string)($input['password'] ?? '');

        if (!preg_match('/^09[0-9]{9}$/', $mobile)) {
            echo json_encode(['success' => false, 'message' => 'شماره موبایل نامعتبر است.']);
            exit;
        }
        if (strlen($password) < 4) {
            echo json_encode(['success' => false, 'message' => 'رمز عبور باید حداقل ۴ نویسه باشد.']);
            exit;
        }

        // Verify OTP session
        $stmt = $db->prepare("SELECT * FROM otp_sessions WHERE mobile = :mobile");
        $stmt->execute([':mobile' => $mobile]);
        $session = $stmt->fetch(PDO::FETCH_ASSOC);

        if (!$session) {
            echo json_encode(['success' => false, 'message' => 'درخواست کدی برای این شماره یافت نشد. ابتدا درخواست کد دهید.']);
            exit;
        }

        if (time() > (int)$session['expires_at']) {
            echo json_encode(['success' => false, 'message' => 'کد تایید منقضی شده است. لطفاً مجدداً درخواست دهید.']);
            exit;
        }

        if ($session['otp'] !== $otp) {
            echo json_encode(['success' => false, 'message' => 'کد تایید وارد شده اشتباه است.']);
            exit;
        }

        // OTP verified successfully! Create user or update password.
        $passwordHash = password_hash($password, PASSWORD_BCRYPT);
        $token = bin2hex(random_bytes(24));

        $stmt = $db->prepare("INSERT INTO users (mobile, password_hash, token, last_login) VALUES (:mobile, :hash, :token, datetime('now'))
            ON CONFLICT(mobile) DO UPDATE SET password_hash = :hash, token = :token, last_login = datetime('now')");
        $stmt->execute([
            ':mobile' => $mobile,
            ':hash' => $passwordHash,
            ':token' => $token
        ]);

        // Clear used OTP session
        $stmt = $db->prepare("DELETE FROM otp_sessions WHERE mobile = :mobile");
        $stmt->execute([':mobile' => $mobile]);

        echo json_encode([
            'success' => true,
            'message' => 'ثبت‌نام و احراز هویت با موفقیت انجام شد.',
            'token' => $token,
            'mobile' => $mobile
        ]);
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

        // Refresh token on login
        $newToken = bin2hex(random_bytes(24));
        $stmt = $db->prepare("UPDATE users SET token = :token, last_login = datetime('now') WHERE id = :id");
        $stmt->execute([':token' => $newToken, ':id' => $user['id']]);

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