# Google Apps Script deployment

1. Create a new project at https://script.google.com.
2. Replace the default `Code.gs` with this folder's `Code.gs`.
3. Select `initializeAllclientAccess` and click **Run** once.
4. Approve the permission prompt, then copy `ADMIN_TOKEN` from **Execution log**.
5. Open **Deploy > New deployment > Web app**.
6. Set **Execute as** to `Me` and **Who has access** to `Anyone`.
7. Deploy and copy the URL ending in `/exec`.
8. Send that `/exec` URL so it can replace `PASTE_GOOGLE_APPS_SCRIPT_WEB_APP_URL_HERE` in `Allclient.iss`.

Administrator page:

```text
YOUR_EXEC_URL?action=current&token=YOUR_ADMIN_TOKEN
```

Use the button on this page to generate an 8-digit installation code. Press
the same button again to revoke it immediately. Only one online code can be
active at a time.

Verification endpoint used by the installer:

```text
YOUR_EXEC_URL?action=verify&code=12345678
```

To disable online codes, change the `ALLCLIENT_ENABLED` Script Property to
`false`. The installer's offline recovery code remains independent.
