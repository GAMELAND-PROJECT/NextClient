const CODE_DIGITS = 8;

/** Run once. Re-running revokes the active code and changes the admin token. */
function initializeAllclientAccess() {
  const properties = PropertiesService.getScriptProperties();
  const adminToken = Utilities.getUuid().replace(/-/g, '') +
    Utilities.getUuid().replace(/-/g, '');
  properties.setProperties({
    ALLCLIENT_ADMIN_TOKEN: adminToken,
    ALLCLIENT_ENABLED: 'true'
  });
  properties.deleteProperty('ALLCLIENT_ACTIVE_CODE');
  properties.deleteProperty('ALLCLIENT_CODE_CREATED_AT');
  console.log('ADMIN_TOKEN=' + adminToken);
  console.log('Allclient access was initialized with no active code.');
}

function doGet(event) {
  const action = String((event && event.parameter.action) || '').toLowerCase();
  if (action === 'verify') {
    return verifyResponse_(String(event.parameter.code || ''));
  }
  if (action === 'current') {
    return currentCodePage_(String(event.parameter.token || ''), '');
  }
  return jsonResponse_({
    service: 'allclient-access', online: true, mode: 'manual-one-time-code'
  });
}

function doPost(event) {
  const action = String((event && event.parameter.action) || '').toLowerCase();
  const adminToken = String((event && event.parameter.token) || '');
  if (action !== 'toggle') {
    return jsonResponse_({ success: false, error: 'unsupported_action' });
  }
  if (!adminTokenIsValid_(adminToken)) {
    return HtmlService.createHtmlOutput(
      '<h2>Access denied</h2><p>The administrator token is invalid.</p>');
  }

  const properties = PropertiesService.getScriptProperties();
  const activeCode = properties.getProperty('ALLCLIENT_ACTIVE_CODE') || '';
  let message;
  if (activeCode) {
    properties.deleteProperty('ALLCLIENT_ACTIVE_CODE');
    properties.deleteProperty('ALLCLIENT_CODE_CREATED_AT');
    message = 'The previous installation code was revoked immediately.';
  } else {
    properties.setProperty('ALLCLIENT_ACTIVE_CODE', generateRandomCode_());
    properties.setProperty('ALLCLIENT_CODE_CREATED_AT', new Date().toISOString());
    message = 'A new installation code was generated.';
  }
  return currentCodePage_(adminToken, message);
}

function verifyResponse_(enteredCode) {
  const properties = PropertiesService.getScriptProperties();
  const enabled = properties.getProperty('ALLCLIENT_ENABLED') === 'true';
  const activeCode = properties.getProperty('ALLCLIENT_ACTIVE_CODE') || '';
  const normalizedCode = enteredCode.trim();
  const valid = enabled && activeCode !== '' && /^\d{8}$/.test(normalizedCode) &&
    constantTimeEqual_(normalizedCode, activeCode);
  return jsonResponse_({ valid: valid });
}

function currentCodePage_(adminToken, message) {
  if (!adminTokenIsValid_(adminToken)) {
    return HtmlService.createHtmlOutput(
      '<h2>Access denied</h2><p>The administrator token is invalid.</p>');
  }

  const properties = PropertiesService.getScriptProperties();
  const enabled = properties.getProperty('ALLCLIENT_ENABLED') === 'true';
  const activeCode = properties.getProperty('ALLCLIENT_ACTIVE_CODE') || '';
  const createdAt = properties.getProperty('ALLCLIENT_CODE_CREATED_AT') || '';
  const displayCode = enabled ? (activeCode || 'NO ACTIVE CODE') : 'DISABLED';
  const buttonText = activeCode ? 'Revoke active code' : 'Generate new code';
  const buttonClass = activeCode ? 'danger' : 'primary';
  const statusClass = activeCode && enabled ? 'active' : 'inactive';
  const statusText = activeCode && enabled ? 'ACTIVE' : 'INACTIVE';
  const serviceUrl = ScriptApp.getService().getUrl();

  return HtmlService.createHtmlOutput(`<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>Allclient Access Control</title><style>
body{font-family:Segoe UI,sans-serif;background:#0d1320;color:#fff;display:grid;place-items:center;min-height:100vh;margin:0}.card{width:min(520px,calc(100% - 40px));box-sizing:border-box;background:#192235;padding:36px;border-radius:18px;text-align:center;box-shadow:0 15px 50px #0008}.code{font:700 42px Consolas,monospace;letter-spacing:6px;color:#62d6ff;margin:25px 0;word-break:break-word}.badge{display:inline-block;padding:6px 12px;border-radius:20px;font-weight:700}.active{background:#173f31;color:#68e0a8}.inactive{background:#43252b;color:#ff8997}.message{color:#a9d7ff;min-height:24px}.meta{color:#9eabc0;font-size:13px}.button{border:0;border-radius:10px;padding:13px 22px;color:#fff;font-size:16px;font-weight:700;cursor:pointer}.primary{background:#1379d1}.danger{background:#c33e50}</style></head>
<body><main class="card"><h1>Allclient Access</h1><span class="badge ${statusClass}">${statusText}</span><div class="code">${displayCode}</div><p class="message">${escapeHtml_(message || '')}</p><p class="meta">${createdAt ? 'Created: ' + escapeHtml_(createdAt) : 'No installation code is currently active.'}</p>
<form method="post" action="${serviceUrl}" target="_top"><input type="hidden" name="action" value="toggle"><input type="hidden" name="token" value="${escapeHtml_(adminToken)}"><button class="button ${buttonClass}" type="submit">${buttonText}</button></form></main></body></html>`);
}

function generateRandomCode_() {
  const seed = Utilities.getUuid() + Utilities.getUuid() + Date.now();
  const digest = Utilities.computeDigest(
    Utilities.DigestAlgorithm.SHA_256, seed, Utilities.Charset.UTF_8);
  const value = (((digest[0] & 255) << 24) |
    ((digest[1] & 255) << 16) |
    ((digest[2] & 255) << 8) |
    (digest[3] & 255)) >>> 0;
  return String(value % Math.pow(10, CODE_DIGITS)).padStart(CODE_DIGITS, '0');
}

function adminTokenIsValid_(adminToken) {
  const expectedToken = PropertiesService.getScriptProperties()
    .getProperty('ALLCLIENT_ADMIN_TOKEN') || '';
  return expectedToken !== '' && constantTimeEqual_(adminToken, expectedToken);
}

function constantTimeEqual_(left, right) {
  const a = String(left);
  const b = String(right);
  let difference = a.length ^ b.length;
  const length = Math.max(a.length, b.length);
  for (let index = 0; index < length; index++) {
    difference |= (a.charCodeAt(index % Math.max(a.length, 1)) || 0) ^
      (b.charCodeAt(index % Math.max(b.length, 1)) || 0);
  }
  return difference === 0;
}

function escapeHtml_(value) {
  return String(value).replace(/&/g, '&amp;').replace(/</g, '&lt;')
    .replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

function jsonResponse_(value) {
  return ContentService.createTextOutput(JSON.stringify(value))
    .setMimeType(ContentService.MimeType.JSON);
}
