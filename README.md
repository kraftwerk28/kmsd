# Kernel modules settings daemon

A simple program, that's supposed to run by `root` user and change settings of
various kernel modules on-the-fly. I wrote it to control my keyboard's
<kbd>Fn</kbd>mode via `hid_apple` module.

To interact with the daemon, use any tool that is able to connect to UNIX socket
and write data to it.


### Example

```bash
$ cat << EOF | socat - UNIX-CONNECT:/tmp/kmsd.sock
/sys/module/hid_apple/parameters/fnmode
2
EOF
```


### Message format

The following message format is used: a file to which data should be written to
comes first, then a newline character, then the actual data.
