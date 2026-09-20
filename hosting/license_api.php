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

// The secret key MUST match the kLicenseSecretKey in GameNetAccessConfig.h
$secret_key = "NextClientSecureRC4Key2026!";

// The data you want to encrypt. In this case, just the GameNet tag.
// e.g., "branch_8832" or "vip_center_tehran"
$tag_data = isset($_GET['tag']) ? $_GET['tag'] : "default_tag";

// We use OpenSSL with 'rc4' cipher. OPENSSL_RAW_DATA ensures we get the raw binary bytes, not base64.
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
