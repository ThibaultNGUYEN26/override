# level06

For now, retrieve the `level06` binary from the VM.

## Connect to level06

Read the password obtained from level05:

```sh
cat level05/flag
```

Connect to the VM:

```sh
ssh level06@127.0.0.1 -p 2222
```

When SSH asks for a password, paste the content of `level05/flag`.

## Get the binary

Copy the ELF binary from the VM:

```sh
scp -P 2222 level06@127.0.0.1:/home/users/level06/level06 ./level06/src/level06
```

Use the content of `level05/flag` as the password.

Verify the downloaded binary:

```sh
file level06/src/level06
```

The local binary should now be:

```text
level06/src/level06
```

## Save `main_dump_gdb`

Inside the level06 SSH session, load the binary using its absolute path:

```sh
gdb ./level06
```

Use a level-specific temporary filename and disable pagination so the complete function is saved:

```gdb
set disassembly-flavor intel
set logging file /tmp/level06_main_dump_gdb
set logging overwrite on
set logging on
disas main
set logging off
quit
```

Verify that the dump exists on the VM:

```sh
ls -l /tmp/level06_main_dump_gdb
```

From the repository root on the host, copy it locally:

```sh
scp -P 2222 level06@127.0.0.1:/tmp/level06_main_dump_gdb ./level06/src/main_dump_gdb
```

Use the content of `level05/flag` as the password.

The local files should now be:

```text
level06/src/level06
level06/src/main_dump_gdb
```

## Decode addresses from `main_dump_gdb`

Compile the shared decoder from the repository root if necessary:

```sh
gcc -Wall -Wextra -Werror decode_address.c -o decode_address
```

The data addresses used by `main` are:

```text
0x8048a60
0x8048ad4
0x8048af8
0x8048b08
0x8048b1c
0x8048b40
0x8048b52
0x8048b61
0x804a060
```

Decode them against the local level06 binary:

```sh
./decode_address level06/src/level06 0x8048a60
./decode_address level06/src/level06 0x8048ad4
./decode_address level06/src/level06 0x8048af8
./decode_address level06/src/level06 0x8048b08
./decode_address level06/src/level06 0x8048b1c
./decode_address level06/src/level06 0x8048b40
./decode_address level06/src/level06 0x8048b52
./decode_address level06/src/level06 0x8048b61
./decode_address level06/src/level06 0x804a060
```

The output is:

```text
0x08048a60 -> "%u"
0x08048ad4 -> "***********************************"
0x08048af8 -> "*\t\tlevel06\t\t  *"
0x08048b08 -> "-> Enter Login: "
0x08048b1c -> "***** NEW ACCOUNT DETECTED ********"
0x08048b40 -> "-> Enter Serial: "
0x08048b52 -> "Authenticated!"
0x08048b61 -> "/bin/sh"
0x0804a060 -> zero-initialized BSS data (`stdin`), not a string
```

This shows that `main` reads a login with `fgets`, reads an unsigned serial using `scanf("%u", ...)`, and passes both values to:

```asm
call 0x8048748 <auth>
```

If `auth` returns zero, the program prints `"Authenticated!"` and executes `system("/bin/sh")`. The next function to save and analyze is therefore `auth`.

## Check the visible `fgets` and XOR instructions

The login buffer starts at `[esp+0x2c]`, and `fgets` receives a size of `0x20`:

```asm
mov    DWORD PTR [esp+0x4],0x20
lea    eax,[esp+0x2c]
mov    DWORD PTR [esp],eax
call   0x8048550 <fgets@plt>
```

`fgets` reads at most 31 input characters plus the terminating null byte. The stack canary is at `[esp+0x4c]`, exactly `0x20` bytes after the beginning of the buffer, so this call does not overflow into it.

The XOR near the start of `main`:

```asm
xor eax,eax
```

only sets `eax` to zero. The XOR near the end:

```asm
xor edx,DWORD PTR gs:0x14
```

checks the saved stack canary. Neither instruction decodes the serial.

The login and entered serial are passed to `auth`, so the relevant validation algorithm must be analyzed there.

## Save `auth_dump_gdb`

Inside the level06 SSH session, load the binary:

```sh
gdb /home/users/level06/level06
```

Save the complete `auth` disassembly:

```gdb
set disassembly-flavor intel
set logging file /tmp/level06_auth_dump_gdb
set logging overwrite on
set logging on
disas auth
set logging off
quit
```

Verify the file on the VM:

```sh
ls -l /tmp/level06_auth_dump_gdb
```

From the repository root on the host, copy it locally:

```sh
scp -P 2222 level06@127.0.0.1:/tmp/level06_auth_dump_gdb ./level06/src/auth_dump_gdb
```

## Decode addresses from `auth_dump_gdb`

The readable data addresses in `auth` are:

```text
0x8048a63
0x8048a68
0x8048a8c
0x8048ab0
```

Decode them from the repository root:

```sh
./decode_address level06/src/level06 0x8048a63
./decode_address level06/src/level06 0x8048a68
./decode_address level06/src/level06 0x8048a8c
./decode_address level06/src/level06 0x8048ab0
```

The output is:

```text
0x08048a63 -> "\n"
0x08048a68 -> "\x1b[32m.---------------------------."
0x08048a8c -> "\x1b[31m| !! TAMPERING DETECTED !!  |"
0x08048ab0 -> "\x1b[32m'---------------------------'"
```

`0x1b[32m` and `0x1b[31m` are ANSI terminal color sequences. These messages are printed when the `ptrace` anti-debugging check detects a debugger.

The values below are numeric constants used by the serial algorithm, not addresses:

```text
0x1337
0x5eeded
0x88233b2b
0x539
```

The function removes the login's newline using the string at `0x8048a63`, rejects logins shorter than six characters, performs the anti-debugging check, calculates a serial from the login, and compares it with the entered serial.

## Reconstruct the serial algorithm

The initial serial value uses the fourth login character, at index 3:

```asm
mov    eax,DWORD PTR [ebp+0x8]
add    eax,0x3
movzx  eax,BYTE PTR [eax]
xor    eax,0x1337
add    eax,0x5eeded
```

Equivalent C:

```c
serial = (login[3] ^ 0x1337) + 0x5eeded;
```

The loop rejects bytes below `0x20`, then updates the serial for every login character. The multiplication by `0x88233b2b`, shifts, multiplication by `0x539`, and subtraction are the compiler's optimized implementation of a remainder modulo `0x539`:

```c
serial += (login[i] ^ serial) % 0x539;
```

`0x539` is 1337 in decimal.

The complete reconstructed algorithm is available in `level06/src/serial_generator.c`.

Compile it from the repository root:

```sh
gcc -Wall -Wextra -Werror level06/src/serial_generator.c -o level06/src/serial_generator
```

Choose a printable login containing between 6 and 31 characters, then generate its serial:

```sh
./level06/src/serial_generator aaaaaa
```

The output is:

```text
login: aaaaaa
serial: 6231562
```

Use the same login and generated decimal serial when running level06 on the VM:

```sh
/home/users/level06/level06
```

Enter:

```text
Login:  aaaaaa
Serial: 6231562
```

Do not run this final test under GDB because `auth` uses `ptrace` to detect a debugger. Successful authentication executes `/bin/sh`.

From the resulting shell, verify the account and read the next password:

```sh
cat /home/users/level07/.pass
```

## Save the flag

Back on the host, store the password displayed by `/home/users/level07/.pass` in:

```text
level06/flag
```

Verify it with:

```sh
cat level06/flag
```

## Connect to level07

Use the content of `level06/flag` as the SSH password:

```sh
ssh level07@127.0.0.1 -p 2222
```
