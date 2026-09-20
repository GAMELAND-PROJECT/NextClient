<?php
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');
header('Content-Type: application/json');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    exit(0);
}

$action = $_GET['action'] ?? '';

$tags_file = '../client_tags.txt'; // from root NextClient-1/client_tags.txt
$updates_file = 'updates.json';
$download_host_dir = 'downloads/'; // Assuming local folder maps to download host
$demos_dir = 'demos/';

if (!file_exists($updates_file)) {
    file_put_contents($updates_file, json_encode([]));
}
if (!is_dir($download_host_dir)) {
    mkdir($download_host_dir, 0777, true);
}
if (!is_dir($demos_dir)) {
    mkdir($demos_dir, 0777, true);
}

if ($action === 'get_tags') {
    $tags = [];
    if (file_exists($tags_file)) {
        $lines = file($tags_file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
        foreach ($lines as $line) {
            $tags[] = trim($line);
        }
    }
    
    $updates = json_decode(file_get_contents($updates_file), true);
    
    $result = [];
    foreach ($tags as $tag) {
        $result[] = [
            'tag' => $tag,
            'latest_version' => $updates[$tag]['version'] ?? 'None',
            'updated_at' => $updates[$tag]['updated_at'] ?? 'Never'
        ];
    }
    
    echo json_encode(['success' => true, 'tags' => $result]);
    exit;
}

if ($action === 'upload_update') {
    $tag = $_POST['tag'] ?? '';
    $version = $_POST['version'] ?? '';
    
    if (!$tag || !$version || !isset($_FILES['file'])) {
        echo json_encode(['success' => false, 'error' => 'Missing parameters']);
        exit;
    }
    
    $file = $_FILES['file'];
    $ext = pathinfo($file['name'], PATHINFO_EXTENSION);
    $filename = "patch_{$tag}_v{$version}.{$ext}";
    $destination = $download_host_dir . $filename;
    
    if (move_uploaded_file($file['tmp_name'], $destination)) {
        $updates = json_decode(file_get_contents($updates_file), true);
        $updates[$tag] = [
            'version' => $version,
            'download_url' => "http://dl.gameland.cam/downloads/" . $filename,
            'updated_at' => date('Y-m-d H:i:s')
        ];
        file_put_contents($updates_file, json_encode($updates, JSON_PRETTY_PRINT));
        echo json_encode(['success' => true, 'message' => 'Upload successful']);
    } else {
        echo json_encode(['success' => false, 'error' => 'Failed to move uploaded file']);
    }
    exit;
}

if ($action === 'upload_demo') {
    $tag = $_POST['tag'] ?? 'unknown';
    
    if (!isset($_FILES['demo_file'])) {
        echo json_encode(['success' => false, 'error' => 'Missing demo file']);
        exit;
    }
    
    $file = $_FILES['demo_file'];
    $original_name = basename($file['name']);
    // Sanitize filename
    $original_name = preg_replace('/[^a-zA-Z0-9_\.-]/', '_', $original_name);
    
    $filename = "{$tag}_" . time() . "_{$original_name}";
    $destination = $demos_dir . $filename;
    
    if (move_uploaded_file($file['tmp_name'], $destination)) {
        echo json_encode(['success' => true, 'message' => 'Demo uploaded successfully']);
    } else {
        echo json_encode(['success' => false, 'error' => 'Failed to move demo file']);
    }
    exit;
}

if ($action === 'get_demos') {
    $tag = $_GET['tag'] ?? '';
    
    $files = glob($demos_dir . ($tag ? $tag . "_*.dem" : "*.dem"));
    $demos = [];
    foreach ($files as $f) {
        $demos[] = [
            'filename' => basename($f),
            'size' => filesize($f),
            'date' => date('Y-m-d H:i:s', filemtime($f)),
            'url' => "http://gameland.cam/" . $f
        ];
    }
    
    echo json_encode(['success' => true, 'demos' => $demos]);
    exit;
}

echo json_encode(['success' => false, 'error' => 'Unknown action']);
