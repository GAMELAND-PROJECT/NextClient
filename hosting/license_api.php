<?php
/**
 * gameland_license.dat Generator
 * 
 * This script generates the encrypted license payload.
 * The payload is encrypted using RC4 and the shared secret key.
 * 
 * You can integrate this logic into your installer_access.php
 * or host it as a separate endpoint that the installer downloads from.
 */

$secret_key = "NextClientSecureRC4Key2026!";

// Add basic rate-limiting to prevent spamming
function checkRateLimit(string $ip): bool {
    $rateLimitFile = __DIR__ . DIRECTORY_SEPARATOR . 'admin' . DIRECTORY_SEPARATOR . 'backups' . DIRECTORY_SEPARATOR . '.license_api.rate';
    $dir = dirname($rateLimitFile);
    if (!is_dir($dir)) { @mkdir($dir, 0700, true); }
    $now = time();
    $rates = is_file($rateLimitFile) ? (json_decode(file_get_contents($rateLimitFile), true) ?? []) : [];
    $rates = array_filter($rates, static fn($entry) => is_array($entry) && $now - (int)($entry['time'] ?? 0) < 60);
    $ipRate = $rates[$ip] ?? ['time' => $now, 'count' => 0];
    $ipRate['count']++;
    $rates[$ip] = $ipRate;
    @file_put_contents($rateLimitFile, json_encode($rates), LOCK_EX);
    return $ipRate['count'] <= 30; // Max 30 downloads per minute per IP
}

if (!checkRateLimit((string)($_SERVER['REMOTE_ADDR'] ?? '0.0.0.0'))) {
    http_response_code(429);
    exit('Too Many Requests');
}

$tag_data = isset($_GET['tag']) ? (string)$_GET['tag'] : "default_tag";
if (!preg_match('/^[A-Za-z0-9_-]{1,64}$/', $tag_data)) {
    http_response_code(400);
    exit('Bad Request');
}

$encrypted_payload = openssl_encrypt($tag_data, 'rc4', $secret_key, OPENSSL_RAW_DATA);

// Set headers to force download as a binary file
header('Content-Description: File Transfer');
header('Content-Type: application/octet-stream');
header('Content-Disposition: attachment; filename="gameland_license.dat"');
header('Expires: 0');
header('Cache-Control: must-revalidate');
header('Pragma: public');
header('Content-Length: ' . strlen($encrypted_payload));

// Output the binary payload
echo $encrypted_payload;
exit;
