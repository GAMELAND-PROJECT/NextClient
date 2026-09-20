<?php
declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store, max-age=0');
header('X-Content-Type-Options: nosniff');

$tag = (string)($_GET['tag'] ?? '');
$current_version = (string)($_GET['version'] ?? '0.0.0');

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'GET' || !preg_match('/\\A[A-Za-z0-9_-]{1,64}\\z/', $tag)) {
    http_response_code(400);
    echo json_encode(["error" => "Invalid Request"]);
    exit;
}

$updates_file = __DIR__ . '/updates.json';
if (!is_file($updates_file)) {
    echo json_encode(["update_available" => false]);
    exit;
}

$updates_data = json_decode(file_get_contents($updates_file), true);

if (is_array($updates_data) && isset($updates_data[$tag])) {
    $tag_data = $updates_data[$tag];
    $latest_version = (string)($tag_data['version'] ?? '0.0.0');
    
    // Check if the server version is greater than the client's current version
    if (version_compare($latest_version, $current_version, '>')) {
        echo json_encode([
            "update_available" => true,
            "latest_version" => $latest_version,
            "download_url" => (string)($tag_data['download_url'] ?? ''),
            "hash" => (string)($tag_data['hash'] ?? ''),
            "forced" => true // Based on user preference
        ]);
        exit;
    }
}

echo json_encode(["update_available" => false]);
