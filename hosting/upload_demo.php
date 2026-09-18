<?php
declare(strict_types=1);

const FILE_TAGS = 'client_tags.txt';
const FILE_FTP_CONFIG = 'ftp_config.txt';

header('Content-Type: text/plain; charset=UTF-8');

function panelDataDir(): string
{
    $configFile = __DIR__ . '/admin/config.php';
    $loadedConfig = is_file($configFile) ? require $configFile : null;
    return (is_array($loadedConfig) && !empty($loadedConfig['data_dir']))
        ? (string)$loadedConfig['data_dir']
        : __DIR__;
}

function normalizedLines(string $content): array
{
    return array_values(array_filter(array_map('trim',
        explode("\n", str_replace(["\r\n", "\r"], "\n", $content))),
        static fn(string $line): bool => $line !== '' && strpos($line, '#') !== 0));
}

function uploadPasswordValid(string $buildTag, string $password): bool
{
    $tagsFile = panelDataDir() . DIRECTORY_SEPARATOR . FILE_TAGS;
    if (!is_file($tagsFile)) {
        return false;
    }

    $content = file_get_contents($tagsFile);
    if ($content === false) {
        return false;
    }

    foreach (normalizedLines($content) as $line) {
        $parts = array_map('trim', explode('|', $line));
        if (count($parts) < 4) {
            continue;
        }
        if (strcasecmp($parts[0], $buildTag) === 0) {
            return hash_equals($parts[3], $password);
        }
    }
    return false;
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    exit('FAIL: Method Not Allowed');
}

$buildTag = trim((string)($_POST['build'] ?? ''));
$password = trim((string)($_POST['password'] ?? ''));

if ($buildTag === '' || $password === '') {
    exit('FAIL: Missing build or password');
}
if (!uploadPasswordValid($buildTag, $password)) {
    exit('FAIL: Incorrect password or subscription');
}
if (!isset($_FILES['demo'])) {
    exit('FAIL: File upload error (No file received)');
}

if ($_FILES['demo']['error'] !== UPLOAD_ERR_OK) {
    $err = $_FILES['demo']['error'];
    $errMsgs = [
        UPLOAD_ERR_INI_SIZE => 'The uploaded file exceeds the upload_max_filesize directive in php.ini.',
        UPLOAD_ERR_FORM_SIZE => 'The uploaded file exceeds the MAX_FILE_SIZE directive in the HTML form.',
        UPLOAD_ERR_PARTIAL => 'The uploaded file was only partially uploaded.',
        UPLOAD_ERR_NO_FILE => 'No file was uploaded.',
        UPLOAD_ERR_NO_TMP_DIR => 'Missing a temporary folder.',
        UPLOAD_ERR_CANT_WRITE => 'Failed to write file to disk.',
        UPLOAD_ERR_EXTENSION => 'A PHP extension stopped the file upload.',
    ];
    $msg = $errMsgs[$err] ?? "Unknown upload error code: $err";
    exit("FAIL: File upload error - $msg");
}

$filename = preg_replace('/[^A-Za-z0-9_.-]/', '_', basename((string)$_FILES['demo']['name']));
if ($filename === '' || !preg_match('/\.dem$/i', $filename)) {
    exit('FAIL: Invalid demo file');
}

$safeBuild = preg_replace('/[^A-Za-z0-9_-]/', '_', $buildTag);
$storedName = $safeBuild . '_' . date('Ymd_His') . '_' . $filename;
$dataDir = panelDataDir();
$ftpConfigFile = $dataDir . DIRECTORY_SEPARATOR . FILE_FTP_CONFIG;

$ftpError = '';

if (is_file($ftpConfigFile)) {
    $ftpLines = normalizedLines((string)file_get_contents($ftpConfigFile));
    if (count($ftpLines) >= 4) {
        $host = $ftpLines[0];
        $user = $ftpLines[1];
        $pass = $ftpLines[2];
        $path = rtrim($ftpLines[3], '/');
        $port = 21;
        if (substr($host, 0, 6) === 'ftp://') {
            $host = substr($host, 6);
        }
        if (strpos($host, ':') !== false) {
            [$host, $portText] = explode(':', $host, 2);
            $port = max(1, (int)$portText);
        }

        $conn = @ftp_connect($host, $port, 20);
        if ($conn) {
            if (@ftp_login($conn, $user, $pass)) {
                ftp_pasv($conn, true);
                if (@ftp_put($conn, $path . '/' . $storedName, $_FILES['demo']['tmp_name'], FTP_BINARY)) {
                    ftp_close($conn);
                    exit('OK');
                } else {
                    $ftpError = "ftp_put failed to path: $path/$storedName";
                }
            } else {
                $ftpError = "FTP Login failed for user: $user";
            }
            ftp_close($conn);
        } else {
            $ftpError = "FTP Connection failed to $host:$port";
        }
    } else {
        $ftpError = "Invalid FTP config format (needs at least 4 lines)";
    }
} else {
    $ftpError = "FTP config file not found";
}

// Fallback to local storage
$localDir = __DIR__ . '/demos';
if (!is_dir($localDir) && !mkdir($localDir, 0755, true) && !is_dir($localDir)) {
    exit("FAIL: Cannot create local demo storage (FTP Status: $ftpError)");
}

if (move_uploaded_file($_FILES['demo']['tmp_name'], $localDir . '/' . $storedName)) {
    // If it saved locally but we had FTP credentials, it means FTP failed
    if (is_file($ftpConfigFile)) {
        exit("FAIL: Demo saved locally, but FTP Failed: $ftpError");
    }
    exit('OK');
}

exit("FAIL: Failed to save demo locally (FTP Status: $ftpError)");
