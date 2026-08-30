<?php
declare(strict_types=1);

if (PHP_SAPI !== 'cli') {
    http_response_code(404);
    exit;
}

$password = strtr($argv[1] ?? '', [
    '۰' => '0', '۱' => '1', '۲' => '2', '۳' => '3', '۴' => '4',
    '۵' => '5', '۶' => '6', '۷' => '7', '۸' => '8', '۹' => '9',
    '٠' => '0', '١' => '1', '٢' => '2', '٣' => '3', '٤' => '4',
    '٥' => '5', '٦' => '6', '٧' => '7', '٨' => '8', '٩' => '9',
]);
if (!preg_match('/^\d{8}$/D', $password)) {
    fwrite(STDERR, "Password must contain exactly 8 Persian, Arabic or ASCII digits.\n");
    exit(1);
}

echo password_hash($password, PASSWORD_DEFAULT), PHP_EOL;
