<?php
/**
 * GAMELAND Realtime Presence & Lobby Tracking API
 * High-Performance, SQLite WAL + Micro-Cache Engine (Zero Host Pressure)
 * 
 * Endpoints:
 *   POST ?action=heartbeat  -> Registers/updates player status (Lobby / InGame)
 *   GET  ?action=summary    -> Returns cached counts (Total, Lobby, InGame) [~0.1ms]
 *   GET  ?action=players    -> Returns active players list (with map, server, tag)
 *   POST ?action=leave      -> Instantly removes player on game exit
 */

declare(strict_types=1);

header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type, Authorization');
header('Content-Type: application/json; charset=utf-8');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    exit(0);
}

$DATA_DIR = __DIR__ . DIRECTORY_SEPARATOR . 'storage' . DIRECTORY_SEPARATOR . 'presence';
$DB_FILE = $DATA_DIR . DIRECTORY_SEPARATOR . 'presence.sqlite';
$SUMMARY_CACHE_FILE = $DATA_DIR . DIRECTORY_SEPARATOR . 'summary_cache.json';
$EXPIRY_SECONDS = 45; // TTL lease: player pruned if no heartbeat for 45s

if (!is_dir($DATA_DIR)) {
    @mkdir($DATA_DIR, 0755, true);
}

$action = $_GET['action'] ?? $_POST['action'] ?? '';
if (empty($action) && $_SERVER['REQUEST_METHOD'] === 'POST') {
    $rawInput = file_get_contents('php://input');
    if (!empty($rawInput)) {
        $json = json_decode($rawInput, true);
        if (is_array($json) && isset($json['action'])) {
            $action = $json['action'];
            $_POST = $json;
        }
    }
}

// ─────────────────────────────────────────────────────────────
// FAST PATH: SUMMARY (Direct file read, zero DB access)
// ─────────────────────────────────────────────────────────────
if ($action === 'summary' || (empty($action) && $_SERVER['REQUEST_METHOD'] === 'GET')) {
    // Serve from cache if fresh (< 4 seconds old)
    if (is_file($SUMMARY_CACHE_FILE) && (time() - filemtime($SUMMARY_CACHE_FILE)) < 4) {
        header('X-Presence-Cache: HIT');
        readfile($SUMMARY_CACHE_FILE);
        exit;
    }
}

function getPresenceDb(string $dbPath): PDO {
    $db = new PDO("sqlite:" . $dbPath);
    $db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    // WAL mode for extreme concurrency (thousands of simultaneous reads/writes without lock contention)
    $db->exec("PRAGMA journal_mode = WAL;");
    $db->exec("PRAGMA synchronous = NORMAL;");
    $db->exec("PRAGMA busy_timeout = 3000;");

    $db->exec("CREATE TABLE IF NOT EXISTS active_players (
        device_hash TEXT PRIMARY KEY,
        player_name TEXT NOT NULL,
        state TEXT NOT NULL DEFAULT 'lobby',
        map_name TEXT DEFAULT '',
        server_name TEXT DEFAULT '',
        edition TEXT DEFAULT 'gamenet',
        tag TEXT DEFAULT 'DEFAULT',
        ip TEXT DEFAULT '',
        last_heartbeat INTEGER NOT NULL,
        first_seen INTEGER NOT NULL
    )");
    $db->exec("CREATE INDEX IF NOT EXISTS idx_active_players_hb ON active_players(last_heartbeat)");
    $db->exec("CREATE INDEX IF NOT EXISTS idx_active_players_state ON active_players(state)");

    return $db;
}

try {
    $db = getPresenceDb($DB_FILE);
    $now = time();

    // ─────────────────────────────────────────────────────────────
    // ACTION: HEARTBEAT (Update/Renew Lease)
    // ─────────────────────────────────────────────────────────────
    if ($action === 'heartbeat') {
        $input = $_POST;
        if (empty($input)) {
            $raw = file_get_contents('php://input');
            $input = json_decode($raw, true) ?: [];
        }

        $deviceHash = strtoupper(trim((string)($input['device_hash'] ?? '')));
        if (empty($deviceHash)) {
            // Generate temporary fallback hash from IP and User Agent if missing
            $deviceHash = strtoupper(substr(md5(($_SERVER['REMOTE_ADDR'] ?? '127.0.0.1') . ($_SERVER['HTTP_USER_AGENT'] ?? '')), 0, 24));
        }

        $playerName = trim((string)($input['name'] ?? 'Player'));
        if (empty($playerName)) $playerName = 'Player';
        $playerName = mb_substr($playerName, 0, 32);

        $state = strtolower(trim((string)($input['state'] ?? 'lobby')));
        if ($state !== 'ingame') $state = 'lobby';

        $mapName = trim((string)($input['map'] ?? ''));
        $mapName = mb_substr(preg_replace('/[^a-zA-Z0-9_\.-]/', '', $mapName), 0, 32);

        $serverName = trim((string)($input['server'] ?? ''));
        $serverName = mb_substr($serverName, 0, 64);

        $edition = strtolower(trim((string)($input['edition'] ?? 'gamenet')));
        $tag = trim((string)($input['tag'] ?? 'DEFAULT'));
        $ip = (string)($_SERVER['REMOTE_ADDR'] ?? '');

        // Atomic Upsert
        $stmt = $db->prepare("
            INSERT INTO active_players (device_hash, player_name, state, map_name, server_name, edition, tag, ip, last_heartbeat, first_seen)
            VALUES (:hash, :name, :state, :map, :server, :edition, :tag, :ip, :now, :now)
            ON CONFLICT(device_hash) DO UPDATE SET
                player_name = :name,
                state = :state,
                map_name = :map,
                server_name = :server,
                edition = :edition,
                tag = :tag,
                ip = :ip,
                last_heartbeat = :now
        ");

        $stmt->execute([
            ':hash' => $deviceHash,
            ':name' => $playerName,
            ':state' => $state,
            ':map' => $mapName,
            ':server' => $serverName,
            ':edition' => $edition,
            ':tag' => $tag,
            ':ip' => $ip,
            ':now' => $now
        ]);

        echo json_encode(['success' => true]);
        exit;
    }

    // ─────────────────────────────────────────────────────────────
    // ACTION: LEAVE (Player closed game)
    // ─────────────────────────────────────────────────────────────
    if ($action === 'leave') {
        $deviceHash = strtoupper(trim((string)($_POST['device_hash'] ?? $_GET['device_hash'] ?? '')));
        if (!empty($deviceHash)) {
            $stmt = $db->prepare("DELETE FROM active_players WHERE device_hash = :hash");
            $stmt->execute([':hash' => $deviceHash]);
            @unlink($SUMMARY_CACHE_FILE); // Invalidate cache immediately
        }
        echo json_encode(['success' => true]);
        exit;
    }

    // Prune stale records (> 45 seconds old)
    $cutoff = $now - $EXPIRY_SECONDS;
    $db->prepare("DELETE FROM active_players WHERE last_heartbeat < :cutoff")->execute([':cutoff' => $cutoff]);

    // ─────────────────────────────────────────────────────────────
    // ACTION: SUMMARY (Total, In-Lobby, In-Game)
    // ─────────────────────────────────────────────────────────────
    if ($action === 'summary' || empty($action)) {
        $stmt = $db->query("
            SELECT 
                COUNT(*) as total,
                SUM(CASE WHEN state = 'lobby' THEN 1 ELSE 0 END) as in_lobby,
                SUM(CASE WHEN state = 'ingame' THEN 1 ELSE 0 END) as in_game
            FROM active_players
        ");
        $row = $stmt->fetch(PDO::FETCH_ASSOC);

        $summary = [
            'success' => true,
            'total_online' => (int)($row['total'] ?? 0),
            'in_lobby' => (int)($row['in_lobby'] ?? 0),
            'in_game' => (int)($row['in_game'] ?? 0),
            'timestamp' => $now
        ];

        $payload = json_encode($summary, JSON_UNESCAPED_UNICODE);
        // Atomically write cache file
        @file_put_contents($SUMMARY_CACHE_FILE, $payload, LOCK_EX);

        header('X-Presence-Cache: REFRESHED');
        echo $payload;
        exit;
    }

    // ─────────────────────────────────────────────────────────────
    // ACTION: PLAYERS (Detailed list for the dialog)
    // ─────────────────────────────────────────────────────────────
    if ($action === 'players') {
        $stmt = $db->query("
            SELECT 
                player_name as name,
                state,
                map_name as map,
                server_name as server,
                edition,
                tag,
                (:now - first_seen) as duration_sec,
                (:now - last_heartbeat) as idle_sec
            FROM active_players
            ORDER BY 
                CASE WHEN state = 'ingame' THEN 0 ELSE 1 END,
                last_heartbeat DESC
            LIMIT 200
        ");
        $stmt->execute([':now' => $now]);
        $players = $stmt->fetchAll(PDO::FETCH_ASSOC);

        // Format clean output
        $formatted = [];
        $totalOnline = count($players);
        $inLobby = 0;
        $inGame = 0;

        foreach ($players as $p) {
            $isIngame = ($p['state'] === 'ingame');
            if ($isIngame) $inGame++; else $inLobby++;

            $formatted[] = [
                'name' => (string)$p['name'],
                'state' => (string)$p['state'],
                'map' => (string)($p['map'] ?: ($isIngame ? 'Unknown' : 'Main Menu')),
                'server' => (string)($p['server'] ?: ($isIngame ? 'Server' : '-')),
                'edition' => (string)$p['edition'],
                'tag' => (string)$p['tag'],
                'duration' => (int)$p['duration_sec'],
                'idle' => (int)$p['idle_sec']
            ];
        }

        echo json_encode([
            'success' => true,
            'total_online' => $totalOnline,
            'in_lobby' => $inLobby,
            'in_game' => $inGame,
            'players' => $formatted
        ], JSON_UNESCAPED_UNICODE);
        exit;
    }

    echo json_encode(['success' => false, 'error' => 'Unknown action']);
} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['success' => false, 'error' => $e->getMessage()]);
}
