<?php
declare(strict_types=1);

return [
    // Generate with: php make-password.php "YOUR-STRONG-PASSWORD"
    'password_hash' => 'REPLACE_WITH_PASSWORD_HASH',

    // Correct when this folder is uploaded as public_html/admin.
    'data_dir' => dirname(__DIR__),
    'session_name' => 'allclient_admin',
    'session_idle_seconds' => 1800,
    'backup_limit' => 30,
];

