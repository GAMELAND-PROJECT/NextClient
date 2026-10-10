<?php
/**
 * GAMELAND Global Lobby Chat API
 * High-Speed Circular Memory-Buffer (Zero DB Overhead, Ultra-Scalable)
 * 
 * Endpoints:
 *   GET  ?action=get_messages&since_id=X  -> Fetches new messages since last ID
 *   POST ?action=send_message             -> Posts a new message (rate-limited)
 *   POST ?action=admin_mute               -> Moderation: mute abusive device/IP
 *   POST ?action=admin_delete             -> Moderation: delete message by ID
 */

declare(strict_types=1);

header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type, Authorization');
header('Content-Type: application/json; charset=utf-8');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    exit(0);
}

$DATA_DIR = __DIR__ . DIRECTORY_SEPARATOR . 'storage' . DIRECTORY_SEPARATOR . 'chat';
$BUFFER_FILE = $DATA_DIR . DIRECTORY_SEPARATOR . 'chat_buffer.json';
$MUTED_FILE = $DATA_DIR . DIRECTORY_SEPARATOR . 'chat_muted.json';
$RATE_FILE = $DATA_DIR . DIRECTORY_SEPARATOR . 'chat_rate.json';
$MAX_MESSAGES = 50; // Keep only latest 50 messages in circular buffer

if (!is_dir($DATA_DIR)) {
    @mkdir($DATA_DIR, 0755, true);
}

// Bad words filter list (basic Iranian gaming profanity & scam links filter)
$BAD_WORDS = [
    'kos', 'kir', 'koon', 'madar', 'nane', 'jende', 'daus', 'haroomi', 'shakh'
];

$input = $_POST;
if (empty($input) && $_SERVER['REQUEST_METHOD'] === 'POST') {
    $raw = file_get_contents('php://input');
    if (!empty($raw)) {
        $input = json_decode($raw, true) ?: [];
    }
}
$action = $_GET['action'] ?? $input['action'] ?? 'get_messages';

function readChatBuffer(string $file): array {
    if (!is_file($file)) return ['last_id' => 0, 'messages' => []];
    $data = json_decode((string)file_get_contents($file), true);
    if (!is_array($data) || !isset($data['messages'])) {
        return ['last_id' => 0, 'messages' => []];
    }
    return $data;
}

function writeChatBuffer(string $file, array $data): void {
    @file_put_contents($file, json_encode($data, JSON_UNESCAPED_UNICODE), LOCK_EX);
}

if ($action === 'get_latest') {
    $buffer = readChatBuffer($BUFFER_FILE);
    $latest = !empty($buffer['messages']) ? end($buffer['messages']) : null;
    echo json_encode([
        'success' => true,
        'message' => $latest
    ], JSON_UNESCAPED_UNICODE);
    exit;
}

// ─────────────────────────────────────────────────────────────
// ACTION: GET MESSAGES (Delta Polling)
// ─────────────────────────────────────────────────────────────
if ($action === 'get_messages') {
    $sinceId = (int)($_GET['since_id'] ?? 0);
    $buffer = readChatBuffer($BUFFER_FILE);

    $messages = [];
    foreach ($buffer['messages'] as $msg) {
        if ((int)$msg['id'] > $sinceId) {
            $messages[] = $msg;
        }
    }

    echo json_encode([
        'success' => true,
        'last_id' => (int)($buffer['last_id'] ?? 0),
        'count' => count($messages),
        'messages' => $messages
    ], JSON_UNESCAPED_UNICODE);
    exit;
}

// ─────────────────────────────────────────────────────────────
// ACTION: SEND MESSAGE
// ─────────────────────────────────────────────────────────────
if ($action === 'send_message') {
    $deviceHash = strtoupper(trim((string)($input['device_hash'] ?? '')));
    $name = trim((string)($input['name'] ?? 'Player'));
    $edition = strtolower(trim((string)($input['edition'] ?? 'gamenet')));
    $tag = trim((string)($input['tag'] ?? 'DEFAULT'));
    $text = trim((string)($input['message'] ?? ''));
    $ip = (string)($_SERVER['REMOTE_ADDR'] ?? '');

    if (empty($deviceHash)) {
        $deviceHash = strtoupper(substr(md5($ip . ($_SERVER['HTTP_USER_AGENT'] ?? '')), 0, 24));
    }

    if (empty($text)) {
        echo json_encode(['success' => false, 'error' => 'Message is empty']);
        exit;
    }

    if (mb_strlen($text) > 180) {
        $text = mb_substr($text, 0, 180);
    }

    // 1. Check if user is muted
    $muted = is_file($MUTED_FILE) ? (json_decode((string)file_get_contents($MUTED_FILE), true) ?: []) : [];
    $now = time();
    if (isset($muted[$deviceHash]) && (int)$muted[$deviceHash] > $now) {
        $remain = (int)$muted[$deviceHash] - $now;
        echo json_encode([
            'success' => false,
            'muted' => true,
            'error' => "شما به مدت {$remain} ثانیه دیگر در چت لابی مسدود (Mute) هستید."
        ], JSON_UNESCAPED_UNICODE);
        exit;
    }

    // 2. Rate Limiting & Anti-Spam Check
    $rates = is_file($RATE_FILE) ? (json_decode((string)file_get_contents($RATE_FILE), true) ?: []) : [];
    $userRate = $rates[$deviceHash] ?? ['last_time' => 0, 'count' => 0, 'reset_time' => $now + 15];

    // Minimum 3 seconds cooldown
    if (($now - (int)$userRate['last_time']) < 3) {
        echo json_encode([
            'success' => false,
            'cooldown' => true,
            'error' => 'لطفاً ۳ ثانیه بین ارسال هر پیام صبر کنید.'
        ], JSON_UNESCAPED_UNICODE);
        exit;
    }

    if ($now > (int)$userRate['reset_time']) {
        $userRate['count'] = 0;
        $userRate['reset_time'] = $now + 15;
    }
    $userRate['count']++;
    $userRate['last_time'] = $now;

    // Flood detection (more than 5 messages in 15 seconds -> 60s auto-mute)
    if ($userRate['count'] > 5) {
        $muted[$deviceHash] = $now + 60;
        @file_put_contents($MUTED_FILE, json_encode($muted), LOCK_EX);
        echo json_encode([
            'success' => false,
            'muted' => true,
            'error' => 'به دلیل ارسال پشت‌سرهم پیام، به مدت ۱ دقیقه مسدود شدید.'
        ], JSON_UNESCAPED_UNICODE);
        exit;
    }
    $rates[$deviceHash] = $userRate;
    @file_put_contents($RATE_FILE, json_encode($rates), LOCK_EX);

    // 3. Profanity filter
    $filteredText = $text;
    foreach ($BAD_WORDS as $bad) {
        if (mb_stripos($filteredText, $bad) !== false) {
            $filteredText = preg_replace('/' . preg_quote($bad, '/') . '/iu', '***', $filteredText);
        }
    }
    // Strip raw HTML tags to prevent injection, preserve Persian/Unicode and punctuation cleanly
    $cleanText = trim(strip_tags($filteredText));
    $cleanText = preg_replace('/[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/u', '', $cleanText);

    // 4. Append to circular buffer
    $buffer = readChatBuffer($BUFFER_FILE);
    $nextId = ((int)($buffer['last_id'] ?? 0)) + 1;

    $msgItem = [
        'id' => $nextId,
        'sender' => mb_substr($name, 0, 24),
        'device_hash' => substr($deviceHash, 0, 8), // Show partial hash for identification
        'edition' => $edition,
        'tag' => mb_substr($tag, 0, 16),
        'text' => $cleanText,
        'time' => date('H:i'),
        'timestamp' => $now
    ];

    $buffer['messages'][] = $msgItem;
    // Circular trim to MAX_MESSAGES
    if (count($buffer['messages']) > $MAX_MESSAGES) {
        $buffer['messages'] = array_slice($buffer['messages'], -$MAX_MESSAGES);
    }
    $buffer['last_id'] = $nextId;

    writeChatBuffer($BUFFER_FILE, $buffer);

    echo json_encode(['success' => true, 'message' => $msgItem], JSON_UNESCAPED_UNICODE);
    exit;
}

// ─────────────────────────────────────────────────────────────
// ACTION: ADMIN MODERATION (Mute / Delete)
// ─────────────────────────────────────────────────────────────
if ($action === 'admin_mute') {
    $targetHash = strtoupper(trim((string)($input['device_hash'] ?? '')));
    $duration = max(60, (int)($input['duration_sec'] ?? 300));
    if (!empty($targetHash)) {
        $muted = is_file($MUTED_FILE) ? (json_decode((string)file_get_contents($MUTED_FILE), true) ?: []) : [];
        $muted[$targetHash] = time() + $duration;
        @file_put_contents($MUTED_FILE, json_encode($muted), LOCK_EX);
        echo json_encode(['success' => true, 'message' => "کاربر برای {$duration} ثانیه میوت شد."]);
        exit;
    }
}

if ($action === 'admin_delete') {
    $msgId = (int)($input['id'] ?? 0);
    if ($msgId > 0) {
        $buffer = readChatBuffer($BUFFER_FILE);
        $buffer['messages'] = array_values(array_filter($buffer['messages'], static fn($m) => (int)$m['id'] !== $msgId));
        writeChatBuffer($BUFFER_FILE, $buffer);
        echo json_encode(['success' => true]);
        exit;
    }
}

echo json_encode(['success' => false, 'error' => 'Unknown action']);
