# Online server categories

Edit `assets/platform/config/online_server_categories.vdf` before building.
The incremental `BUILD_ALL` target copies it to
`<game>/platform/config/online_server_categories.vdf`.

Use the numeric connection address (IP and game port), with one letter:

```text
"OnlineServerCategories"
{
    "5.57.32.203:44000" "p"
    "5.57.32.203:45000" "m"
}
```

`p` means Public; `m` means Mix. The example does not assert the actual modes
of these servers. Unlisted addresses appear in Public. Server names do not
determine categories. DNS names are not category keys: use the numeric game
address from the server details dialog.

For a local change without rebuilding, edit the installed file and press
Refresh or reopen Online. A subsequent build copies the repository file over
the installed file, so retain permanent changes in the repository copy.

Each column displays only name, players and ping. Populations exclude bots.
Lists sort by most players first, then lowest ping, then address for stable
ties. Nonresponding servers appear last. Selecting a row, double-clicking or
pressing Enter uses the existing connection path and password dialog.
