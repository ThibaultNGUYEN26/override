# level00

For this level, we inspect `main` manually with GDB and decode the hexadecimal comparison value with `decode_address.c`.

## Get the binary

From the repository root, copy the `level00` ELF binary from the VM:

```sh
scp -P 2222 level00@127.0.0.1:/home/users/level00/level00 ./level00/level00
```

For the initial `level00` account, use `level00` as the password.

The local binary should now be:

```text
level00/level00
```

Keeping the ELF locally allows its sections, symbols, and stored data to be
inspected with `readelf`, `objdump`, or `decode_address.c`.

## Save `disas main` to a file

Connect to the VM:
```sh
ssh level00@127.0.0.1 -p 2222
```

For the initial `level00` account, use `level00` as the password.

Start GDB with the binary loaded:

```sh
gdb ./level00
```

The VM uses GDB 7.4, so use the old logging syntax:

```gdb
set logging file /tmp/dump_gdb
set logging overwrite on
set logging on
disas main
set logging off
quit
```

Verify the file exists before disconnecting:

```sh
ls -l /tmp/dump_gdb
```

From the host, if the current directory is already `OverRide/level00`, copy it
with:

```sh
scp -P 2222 level00@127.0.0.1:/tmp/dump_gdb ./dump_gdb
```

If the current directory is the `OverRide` repository root, use:

```sh
scp -P 2222 level00@127.0.0.1:/tmp/dump_gdb ./level00/dump_gdb
```

## Read `main` manually

The important part of `dump_gdb` is:

```asm
call   0x80483d0 <__isoc99_scanf@plt>
mov    0x1c(%esp),%eax
cmp    $0x149c,%eax
jne    0x804850d <main+121>
...
call   0x80483a0 <system@plt>
```

The call to `scanf` reads the input. The value is loaded into `eax`, then
compared with `0x149c`.

`jne` means "jump if not equal". If the input is not equal to `0x149c`, the
program jumps to the invalid-password branch. If it is equal, execution reaches
the call to `system`.

## Understand the check

The program compares the entered number with:

```c
0x149c
```

`0x149c` is an immediate number, not a memory address. Compile the shared
`decode_address.c` utility from the repository root:

```sh
gcc -Wall -Wextra -Werror decode_address.c -o decode_address
```

Use its decimal mode:

```sh
./decode_address --decimal 0x149c
```

Output:

```text
0x149c = 5276
```

So the password is:

```text
5276
```

## Get the shell

Open the VM:

```sh
ssh level00@127.0.0.1 -p 2222
```

Run the binary on the VM:

```sh
./level00
```

Enter:

```text
5276
```

The success branch runs:

```c
system("/bin/sh");
```

Cat the flag:

```sh
cat /home/users/level01/.pass
```

The flag is:

```text
uSq2ehEGT6c9S24zbshexZQBXUGrncxn5sD5QfGL
```

You can now acces to level01:

```sh
ssh level01@127.0.0.1 -p 2222
password: uSq2ehEGT6c9S24zbshexZQBXUGrncxn5sD5QfGL
```
