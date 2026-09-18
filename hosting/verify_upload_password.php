<?php
declare(strict_types=1);

const FILE_TAGS = 'client_tags.txt';

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

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    exit('FAIL: Method Not Allowed');
}

$buildTag = trim((string)($_POST['build'] ?? ''));
$password = trim((string)($_POST['password'] ?? ''));
if ($buildTag === '' || $password === '') {
    exit('FAIL: Missing build or password');
}

$tagsFile = panelDataDir() . DIRECTORY_SEPARATOR . FILE_TAGS;
if (!is_file($tagsFile)) {
    exit('FAIL: No subscriptions');
}

$content = file_get_contents($tagsFile);
if ($content === false) {
    exit('FAIL: Internal error');
}

foreach (normalizedLines($content) as $line) {
    $parts = array_map('trim', explode('|', $line));
    if (count($parts) < 4) {
        continue;
    }
    if (strcasecmp($parts[0], $buildTag) === 0) {
        exit(hash_equals($parts[3], $password) ? 'OK' : 'FAIL: Incorrect password');
    }
}

exit('FAIL: Subscription not found or upload password is not set');
