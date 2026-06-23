# level03

For now, we only retrieve the `level03` binary from the VM.

## Connect to level03

Read the password obtained from level02:

```sh
cat level02/flag
```

Connect to the VM:

```sh
ssh level03@127.0.0.1 -p 2222
```

When SSH asks for a password, paste the content of `level02/flag`.

## Get the binary

From the repository root on the host, copy the ELF binary:

```sh
scp -P 2222 level03@127.0.0.1:/home/users/level03/level03 ./level03/level03
```

Use the content of `level02/flag` as the password.

The local binary should now be:

```text
level03/level03
```

## Save `main_dump_gdb`

On the VM, connect as `level03` and start GDB with the binary loaded:

```sh
gdb ./level03
```

The VM uses GDB 7.4, so use the old logging syntax:

```gdb
set disassembly-flavor intel
set logging file /tmp/main_dump_gdb
set logging overwrite on
set logging on
disas main
set logging off
quit
```

Verify that the dump exists on the VM:

```sh
ls -l /tmp/main_dump_gdb
```

From the repository root on the host, copy it into `level03`:

```sh
scp -P 2222 level03@127.0.0.1:/tmp/main_dump_gdb ./level03/main_dump_gdb
```

Use the content of `level02/flag` as the password.

The local files for the next analysis step are:

```text
level03/level03
level03/main_dump_gdb
```
