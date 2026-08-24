# Bundled Chromium 109 x86

Put the portable archive here using this exact name:

`chrome.nosync.7z`

The archive must contain a 32-bit Chromium 109 `chrome.exe`. The installer finds
the executable recursively, so the directory layout inside the archive is not
fixed. Large browser binaries are intentionally ignored by Git.

Use a trusted archive, scan it before packaging, and record its SHA-256 hash for
release reproducibility. Chromium 109 is obsolete and should only be used by the
installer as the Windows 7/8 compatibility fallback.
