<?php
declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store, max-age=0');
header('X-Content-Type-Options: nosniff');
header('Access-Control-Allow-Origin: *');

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'GET') {
    http_response_code(405);
    echo json_encode(["error" => "Method Not Allowed"]);
    exit;
}

$raw_edition = strtolower(trim((string)($_GET['edition'] ?? '')));
$tag = trim((string)($_GET['tag'] ?? ''));
$current_version = trim((string)($_GET['version'] ?? '0.0.0'));

// Sanitize tag parameter
if ($tag !== '' && !preg_match('/^[A-Za-z0-9_-]{1,64}$/', $tag)) {
    http_response_code(400);
    echo json_encode(["error" => "Invalid Tag Parameter"]);
    exit;
}

// Infer edition if not explicitly specified
$edition = '';
if ($raw_edition === 'home' || $raw_edition === 'gamenet') {
    $edition = $raw_edition;
} elseif (strcasecmp($tag, 'HOME') === 0) {
    $edition = 'home';
} else {
    $edition = 'gamenet';
}

if ($tag === '') {
    $tag = ($edition === 'home') ? 'HOME' : 'DEFAULT';
}

$updates_file = __DIR__ . '/updates.json';
if (!is_file($updates_file)) {
    echo json_encode([
        "update_available" => false,
        "edition" => $edition,
        "tag" => $tag,
        "installed_version" => $current_version
    ], JSON_UNESCAPED_SLASHES);
    exit;
}

$updates_raw = @file_get_contents($updates_file);
$updates_data = is_string($updates_raw) ? json_decode($updates_raw, true) : null;
if (!is_array($updates_data)) {
    echo json_encode([
        "update_available" => false,
        "edition" => $edition,
        "tag" => $tag,
        "installed_version" => $current_version
    ], JSON_UNESCAPED_SLASHES);
    exit;
}

$target_data = null;

if ($edition === 'home') {
    // HOME EDITION CHANNEL (Strict Isolation: NEVER fallback to GameNet updates)
    if (isset($updates_data['home']) && is_array($updates_data['home'])) {
        foreach ($updates_data['home'] as $key => $val) {
            if (is_array($val) && (strcasecmp((string)$key, $tag) === 0 || strcasecmp((string)$key, 'HOME') === 0)) {
                $target_data = $val;
                break;
            }
        }
        if ($target_data === null && isset($updates_data['home']['DEFAULT']) && is_array($updates_data['home']['DEFAULT'])) {
            $target_data = $updates_data['home']['DEFAULT'];
        }
    }
    // Also check flat structure for "HOME"
    if ($target_data === null) {
        foreach ($updates_data as $key => $val) {
            if (is_array($val) && strcasecmp((string)$key, 'HOME') === 0) {
                $target_data = $val;
                break;
            }
        }
    }
} else {
    // GAMENET EDITION CHANNEL (Strict Isolation: NEVER fallback to Home updates)
    if (isset($updates_data['gamenet']) && is_array($updates_data['gamenet'])) {
        foreach ($updates_data['gamenet'] as $key => $val) {
            if (is_array($val) && strcasecmp((string)$key, $tag) === 0) {
                $target_data = $val;
                break;
            }
        }
        if ($target_data === null && isset($updates_data['gamenet']['DEFAULT']) && is_array($updates_data['gamenet']['DEFAULT'])) {
            $target_data = $updates_data['gamenet']['DEFAULT'];
        }
    }
    // Also check flat structure for GameNet tag or DEFAULT (strictly excluding HOME)
    if ($target_data === null) {
        foreach ($updates_data as $key => $val) {
            if (is_array($val) && strcasecmp((string)$key, 'HOME') !== 0 && strcasecmp((string)$key, 'home') !== 0) {
                if (strcasecmp((string)$key, $tag) === 0) {
                    $target_data = $val;
                    break;
                }
            }
        }
        if ($target_data === null && isset($updates_data['DEFAULT']) && is_array($updates_data['DEFAULT'])) {
            $defEdition = strtolower((string)($updates_data['DEFAULT']['edition'] ?? 'gamenet'));
            if ($defEdition !== 'home') {
                $target_data = $updates_data['DEFAULT'];
            }
        }
    }
}

if (is_array($target_data)) {
    $latest_version = (string)($target_data['version'] ?? '0.0.0');

    // Compare versions (e.g. 0.0.2 > 0.0.1)
    if (version_compare($latest_version, $current_version, '>')) {
        echo json_encode([
            "update_available" => true,
            "has_update" => true,
            "edition" => $edition,
            "tag" => (string)($target_data['tag'] ?? $tag),
            "latest_version" => $latest_version,
            "download_url" => (string)($target_data['download_url'] ?? ''),
            "filename" => (string)($target_data['filename'] ?? ''),
            "hash" => (string)($target_data['hash'] ?? ''),
            "size" => (string)($target_data['size'] ?? ''),
            "type" => (string)($target_data['type'] ?? 'zip'),
            "forced" => isset($target_data['forced']) ? (bool)$target_data['forced'] : true,
            "release_notes" => (string)($target_data['release_notes'] ?? '')
        ], JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
        exit;
    }
}

echo json_encode([
    "update_available" => false,
    "has_update" => false,
    "edition" => $edition,
    "tag" => $tag,
    "installed_version" => $current_version
], JSON_UNESCAPED_SLASHES);

