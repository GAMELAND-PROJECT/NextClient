# Online mix servers

Edit `mix_servers.txt` on the host next to `pinned_servers.txt`.
The incremental `BUILD_ALL` target copies it to
`https://gameland.cam/mix_servers.txt`.

Use the numeric connection address (IP and game port). The normal Online feed
continues to fill the Public column. Only addresses in this file appear in Mix:

```text
"OnlineMixServers"
{
    "5.57.32.203:45000" "5.57.32.203:45000"
}
```

There is no `p` or `m` classification. The key and value may both be the same
address; the value is what the client reads.

For a local change without rebuilding, edit the installed file and press
Refresh or reopen Online. A subsequent build copies the repository file over
the installed file, so retain permanent changes in the repository copy.

Each column displays only name, players and ping. Populations exclude bots.
Lists sort by most players first, then lowest ping, then address for stable
ties. Nonresponding servers appear last. Selecting a row, double-clicking or
pressing Enter uses the existing connection path and password dialog.
