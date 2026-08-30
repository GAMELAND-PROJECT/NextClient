<?php
declare(strict_types=1);

if (PHP_SAPI !== 'cli') {
    http_response_code(404);
    exit;
}

$password = $argv[1] ?? '';
if (!preg_match('/^\d{8}$/D', $password)) {
    fwrite(STDERR, "Password must contain exactly 8 ASCII digits.\n");
    exit(1);
}

echo password_hash($password, PASSWORD_DEFAULT), PHP_EOL;
