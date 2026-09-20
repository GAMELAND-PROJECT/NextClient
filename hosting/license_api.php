<?php
declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store, max-age=0');
header('X-Content-Type-Options: nosniff');

// A simple Secret Key to decrypt the token (in a real scenario, use AES decryption)
$SECRET_KEY = "my_super_secret_key"; 

$token = (string)($_GET['token'] ?? '');
$hwid = (string)($_GET['hwid'] ?? '');
$client_ip = $_SERVER['REMOTE_ADDR'] ?? 'UNKNOWN';

if (empty($token) || empty($hwid)) {
    http_response_code(400);
    echo json_encode(["status" => "DENIED", "error" => "Missing parameters"]);
    exit;
}

// In a real system, the token would be decrypted here. 
// For demonstration, we assume the token is the tag itself (e.g., GAMELAND).
$tag = $token; 

$db_file = __DIR__ . '/license_db.json';
$db = file_exists($db_file) ? json_decode(file_get_contents($db_file), true) : [];

if (!isset($db[$tag])) {
    // Initialize the tag if it doesn't exist (In production, the Admin panel creates this)
    $db[$tag] = [
        'max_pcs' => 20,
        'allowed_ip' => '', // Will be set on first PC connection
        'hwids' => []
    ];
}

$tag_data = &$db[$tag];

// If allowed_ip is empty, we lock it to the first IP that connects
if (empty($tag_data['allowed_ip'])) {
    $tag_data['allowed_ip'] = $client_ip;
}

// 1. Check IP address match
if ($tag_data['allowed_ip'] !== $client_ip && $client_ip !== '127.0.0.1') { // allow localhost for testing
    echo json_encode(["status" => "DENIED", "error" => "IP mismatch. This game is locked to another network."]);
    exit;
}

// 2. HWID checking and auto-registration
if (!in_array($hwid, $tag_data['hwids'], true)) {
    // It's a new PC, check capacity
    if (count($tag_data['hwids']) >= $tag_data['max_pcs']) {
        echo json_encode(["status" => "DENIED", "error" => "Maximum PC limit reached for this GameNet."]);
        exit;
    }
    
    // Register the new HWID
    $tag_data['hwids'][] = $hwid;
    file_put_contents($db_file, json_encode($db, JSON_PRETTY_PRINT));
}

// Everything is good, allow access
echo json_encode([
    "status" => "ACTIVE", 
    "tag" => $tag, 
    "hwid" => $hwid, 
    "registered_pcs" => count($tag_data['hwids']),
    "max_pcs" => $tag_data['max_pcs']
]);
