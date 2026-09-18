<?php
// F:\NextClient-1\hosting\upload_demo.php
// Receives .dem files uploaded from NextClient's Demo Manager and transfers them to FTP

// First, read the FTP configuration from the admin panel data directory
$ftpConfigFile = __DIR__ . '/ftp_config.txt';

if (!file_exists($ftpConfigFile)) {
    http_response_code(500);
    exit("ERROR: FTP is not configured in the AllClient panel.");
}

$ftpLines = file($ftpConfigFile, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
if (count($ftpLines) < 4) {
    http_response_code(500);
    exit("ERROR: Incomplete FTP configuration.");
}

$ftpHost = $ftpLines[0];
$ftpUser = $ftpLines[1];
$ftpPass = $ftpLines[2];
$ftpPath = $ftpLines[3];

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    // The raw POST body contains the file contents if sent directly via WinINet
    $fileData = file_get_contents('php://input');
    
    // Check if filename was sent via header (Custom header or query string)
    $filename = isset($_GET['name']) ? $_GET['name'] : 'uploaded_demo_' . time() . '.dem';
    $filename = basename($filename);
    
    if (strlen($fileData) > 0) {
        // Save locally first as a temporary file
        $tempFile = sys_get_temp_dir() . '/' . $filename;
        if (!file_put_contents($tempFile, $fileData)) {
            http_response_code(500);
            exit("ERROR: Could not save temp file.");
        }
        
        // Connect to FTP
        $conn_id = ftp_connect($ftpHost);
        if (!$conn_id) {
            @unlink($tempFile);
            http_response_code(500);
            exit("ERROR: Could not connect to FTP host.");
        }
        
        $login_result = ftp_login($conn_id, $ftpUser, $ftpPass);
        if (!$login_result) {
            ftp_close($conn_id);
            @unlink($tempFile);
            http_response_code(500);
            exit("ERROR: FTP login failed.");
        }
        
        // Turn passive mode on
        ftp_pasv($conn_id, true);
        
        // Ensure path ends with slash
        if (substr($ftpPath, -1) !== '/') {
            $ftpPath .= '/';
        }
        
        $remote_file = $ftpPath . $filename;
        
        // Upload the file
        if (ftp_put($conn_id, $remote_file, $tempFile, FTP_BINARY)) {
            echo "SUCCESS: Demo uploaded to FTP storage.";
        } else {
            http_response_code(500);
            echo "ERROR: Failed to upload file to FTP.";
        }
        
        // Close the connection and delete temp file
        ftp_close($conn_id);
        @unlink($tempFile);
    } else {
        http_response_code(400);
        echo "ERROR: Empty file data.";
    }
} else {
    http_response_code(405);
    echo "ERROR: Invalid request method.";
}
?>
