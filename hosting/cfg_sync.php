<?php
/**
 * GAMELAND Cloud Configuration Sync API
 * 
 * Endpoints:
 *   GET  action=pull (token)        -> Returns user's stored userconfig.cfg content
 *   POST action=push (token, data)  -> Saves user's uploaded userconfig.cfg content
 *   GET  action=info (token)        -> Returns last sync timestamp & file size
 */

header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type, Authorization');
header('Content-Type: application/json; charset=utf-8');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    exit(0);
}

$DATA_DIR = __DIR__ . DIRECTORY_SEPARATOR . 'storage' . DIRECTORY_SEPARATOR . 'user_configs';
$DB_FILE = __DIR__ . DIRECTORY_SEPARATOR . 'storage' . DIRECTORY_SEPARATOR . 'cloud_auth' . DIRECTORY_SEPARATOR . 'gameland_users.sqlite';

if (!is_dir($DATA_DIR)) {
    @mkdir($DATA_DIR, 0755, true);
}

function authenticateUser($dbFile, $token) {
    if (!file_exists($dbFile) || empty($token)) {
        return null;
    }
    $db = new PDO("sqlite:" . $dbFile);
    $stmt = $db->prepare("SELECT id, mobile FROM users WHERE token = :token");
    $stmt->execute([':token' => $token]);
    return $stmt->fetch(PDO::FETCH_ASSOC) ?: null;
}

$token = trim($_REQUEST['token'] ?? $_SERVER['HTTP_AUTHORIZATION'] ?? '');
$token = str_replace('Bearer ', '', $token);
$action = $_REQUEST['action'] ?? '';

$user = authenticateUser($DB_FILE, $token);
if (!$user) {
    http_response_code(401);
    echo json_encode(['success' => false, 'message' => 'دسترسی غیرمجاز. لطفاً مجدداً وارد شوید.']);
    exit;
}

$userId = (int)$user['id'];
$mobileClean = preg_replace("/[^0-9]/", "", $user["mobile"]);
$userConfigFile = $DATA_DIR . DIRECTORY_SEPARATOR . "cfg_{$mobileClean}.cfg";

if ($action === 'pull') {
    if (!file_exists($userConfigFile)) {
        echo json_encode([
            'success' => true,
            'exists' => false,
            'message' => 'هیچ کانفیگی برای این کاربر ذخیره نشده است.',
            'cfg_content' => ''
        ]);
        exit;
    }

    $content = file_get_contents($userConfigFile);
    echo json_encode([
        'success' => true,
        'exists' => true,
        'updated_at' => date('Y-m-d H:i:s', filemtime($userConfigFile)),
        'size' => filesize($userConfigFile),
        'cfg_content' => $content
    ]);
    exit;
}

if ($action === 'push') {
    $input = json_decode(file_get_contents('php://input'), true) ?? $_POST;
    $cfgContent = $input['cfg_content'] ?? '';

    if (empty($cfgContent)) {
        echo json_encode(['success' => false, 'message' => 'محتوای کانفیگ خالی است.']);
        exit;
    }

    // Limit CFG file size to 256 KB max for security
    if (strlen($cfgContent) > 256 * 1024) {
        echo json_encode(['success' => false, 'message' => 'حجم فایل کانفیگ بیش از حد مجاز است.']);
        exit;
    }

    file_put_contents($userConfigFile, $cfgContent, LOCK_EX);

    echo json_encode([
        'success' => true,
        'message' => 'کانفیگ ابری با موفقیت ذخیره و به‌روزرسانی شد.',
        'updated_at' => date('Y-m-d H:i:s'),
        'size' => strlen($cfgContent)
    ]);
    exit;
}

if ($action === 'info') {
    $exists = file_exists($userConfigFile);
    echo json_encode([
        'success' => true,
        'mobile' => $user['mobile'],
        'exists' => $exists,
        'updated_at' => $exists ? date('Y-m-d H:i:s', filemtime($userConfigFile)) : null,
        'size' => $exists ? filesize($userConfigFile) : 0
    ]);
    exit;
}

echo json_encode(['success' => false, 'message' => 'اکشن نامعتبر است.']);