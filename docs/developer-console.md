# Developer console

The console command entry stays available in normal mode. Enter `developer 1`
to enable output, then run `meta list` while hosting a New Game. Enter
`developer 0` to discard the text history and restore the quiet console.

Output is limited to 32 KiB of history and formatted messages to 4096 characters.
The output path does not dispatch browser JavaScript events or write log files.
Developer mode may still enable diagnostics elsewhere in the engine; turn it off
after inspection when measuring gameplay performance.

Manual verification on the installed client:

1. Open the console and enter `developer 1`, then `version`. Check readable output.
2. Host New Game, run `meta list`, and check the actual plugin status.
3. Run `developer 0`. History should disappear while command input keeps working.
4. Enable it again and repeat after a map change and reconnect.
5. Check console scrolling, command history and keyboard focus at 640x480 and 1080p.

A successful GameUI build does not verify Hitbox Fixer compatibility or LAN gameplay.
