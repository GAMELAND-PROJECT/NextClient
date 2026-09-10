# Runtime prerequisites and subscription updates

Run `powershell -ExecutionPolicy Bypass -File installer/Prepare-Runtimes.ps1`
before compiling Allclient.iss. GitHub Actions runs the same preparation.
The script verifies Microsoft Authenticode signatures. It copies the supplied
VC 2015 packages from F:\ when available and downloads VC 2010 SP1 from Microsoft.
These packages cover the observed SmartSteamLoader and SSEFirewall imports;
they do not guarantee compatibility with arbitrary newer dynamically linked binaries.

Upload `hosting/update_access.php` next to `client_tags.txt` on gameland.cam.
It uses the admin configuration's data_dir and Tehran's current date.
Installation code is skipped only for a registered, existing installation with
the same GameNetTag and a live ACTIVE response. Offline/unrecognized/expired
installations still require a code. Legacy installations without GameNetTag need
one code-authorized upgrade before they support this feature.

This follows the existing HTTP/tag access model, not cryptographic proof of
ownership: a local administrator can modify the registry and HTTP can be altered
in transit. It must not be treated as tamper-proof licensing.

Prerequisites run elevated, after authorization and before deleting old files.
Cancellation/failure stops cleanup. x64 packages only run on 64-bit Windows.
Restart-required codes are forwarded to Setup; no forced reboot is requested.
