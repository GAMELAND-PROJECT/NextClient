# Google Apps Script deployment

1. Create a new project at https://script.google.com.
2. Replace the default `Code.gs` with this folder's `Code.gs`.
3. Select `initializeAllclientAccess` and click **Run** once.
4. Approve the permission prompt.
5. Open **Deploy > New deployment > Web app**.
6. Set **Execute as** to `Me` and **Who has access** to `Anyone`.
7. Deploy and copy the URL ending in `/exec`.
8. Open the `/exec` URL directly to manage the installation code.

Code-management page:

```text
YOUR_EXEC_URL
```

Use the button on this page to generate an 8-digit installation code. Press
the same button again to revoke it immediately. Only one online code can be
active at a time.

There is intentionally no administrator token. Anyone who obtains the web-app
URL can generate or revoke the active code, so keep the URL private.

Verification endpoint used by the installer:

```text
YOUR_EXEC_URL?action=verify&code=12345678
```

Service-status endpoint used by the installer:

```text
YOUR_EXEC_URL?action=status
```

To disable online codes, change the `ALLCLIENT_ENABLED` Script Property to
`false`. The installer's offline recovery code remains independent.
