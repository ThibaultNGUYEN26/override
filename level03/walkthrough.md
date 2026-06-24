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
scp -P 2222 level03@127.0.0.1:/home/users/level03/level03 ./level03/src/level03
```

Use the content of `level02/flag` as the password.

The local binary should now be:

```text
level03/src/level03
```

## Save `main_dump_gdb`

On the VM, connect as `level03` and start GDB with the binary loaded:

```sh
gdb ./level03/src/level03
```

The VM uses GDB 7.4, so use the old logging syntax:

```gdb
set disassembly-flavor intel
set logging file /tmp/level03_main_dump_gdb
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
scp -P 2222 level03@127.0.0.1:/tmp/main_dump_gdb ./level03/src/main_dump_gdb
```

Use the content of `level02/flag` as the password.

The local files for the next analysis step are:

```text
level03/src/level03
level03/src/main_dump_gdb
```

## Decode the addresses from `main`

Compile the shared address decoder from the repository root:

```sh
gcc -Wall -Wextra -Werror decode_address.c -o decode_address
```

In `main_dump_gdb`, the values loaded before `puts`, `printf`, and `scanf` are addresses of strings stored in the binary:

```asm
mov    DWORD PTR [esp],0x8048a48
mov    DWORD PTR [esp],0x8048a6c
mov    eax,0x8048a7b
mov    eax,0x8048a85
```

Decode them using the local level03 binary:

```sh
./decode_address level03/src/level03 0x8048a48
./decode_address level03/src/level03 0x8048a6c
./decode_address level03/src/level03 0x8048a7b
./decode_address level03/src/level03 0x8048a85
```

The output is:

```text
0x08048a48 -> "***********************************"
0x08048a6c -> "*\t\tlevel03\t\t**"
0x08048a7b -> "Password:"
0x08048a85 -> "%d"
```

This shows that `main` reads a decimal integer. It then passes that value and the constant `0x1337d00d` to `test`:

```asm
mov    eax,DWORD PTR [esp+0x1c]
mov    DWORD PTR [esp+0x4],0x1337d00d
mov    DWORD PTR [esp],eax
call   0x8048747 <test>
```

`0x8048747` is executable code, not a string address. The next step is therefore to save and inspect the disassembly of `test`.

## Save `test_dump_gdb`

Connect to the VM as `level03` and load the correct binary:

```sh
ssh level03@127.0.0.1 -p 2222
gdb ./level03
```

Save only the disassembly of `test`:

```gdb
set disassembly-flavor intel
set logging file /tmp/level03_test_dump_gdb
set logging overwrite on
set logging on
disas test
set logging off
quit
```

Before leaving the VM, verify that the file exists:

```sh
ls -l /tmp/level03_test_dump_gdb
```

From the repository root on the host, copy the dump:

```sh
scp -P 2222 level03@127.0.0.1:/tmp/level03_test_dump_gdb ./level03/src/test_dump_gdb
```

Use the content of `level02/flag` as the password. The new local file should be:

```text
level03/src/test_dump_gdb
```

## Inspect the addresses in `test`

`test_dump_gdb` does not contain addresses of readable strings, so there is nothing here to pass to `decode_address` as text.

It contains three important addresses:

```text
0x80489f0 -> jump table stored in `.rodata`
0x8048660 -> address of the `decrypt` function
0x8048520 -> address of `rand@plt`
```

The instruction sequence below uses the value calculated at `[ebp-0xc]` as an index into the jump table:

```asm
mov    eax,DWORD PTR [ebp-0xc]
shl    eax,0x2
add    eax,0x80489f0
mov    eax,DWORD PTR [eax]
jmp    eax
```

Each jump-table entry is a four-byte code address. Inspect all 22 entries in GDB with:

```gdb
x/22wx 0x80489f0
```

Do not decode `0x80489f0` as a string. It selects a branch for values from `0` through `0x15`. The branches eventually call:

```asm
call   0x8048660 <decrypt>
```

This discovers the next function that must be saved and analyzed: `decrypt`.

## Save `decrypt_dump_gdb`

Connect to the VM as `level03` and load the binary:

```sh
ssh level03@127.0.0.1 -p 2222
gdb ./level03
```

Save only the disassembly of `decrypt`:

```gdb
set disassembly-flavor intel
set logging file /tmp/level03_decrypt_dump_gdb
set logging overwrite on
set logging on
disas decrypt
set logging off
quit
```

Before leaving the VM, verify that the dump exists:

```sh
ls -l /tmp/level03_decrypt_dump_gdb
```

From the repository root on the host, copy it into `level03`:

```sh
scp -P 2222 level03@127.0.0.1:/tmp/level03_decrypt_dump_gdb ./level03/src/decrypt_dump_gdb
```

Use the content of `level02/flag` as the password. The new local file should be:

```text
level03/src/decrypt_dump_gdb
```

## Decode the addresses in `decrypt`

`decrypt_dump_gdb` contains three addresses of readable strings:

```asm
mov    eax,0x80489c3
mov    DWORD PTR [esp],0x80489d4
mov    DWORD PTR [esp],0x80489dc
```

Decode them from the repository root:

```sh
./decode_address level03/src/level03 0x80489c3
./decode_address level03/src/level03 0x80489d4
./decode_address level03/src/level03 0x80489dc
```

The output is:

```text
0x080489c3 -> "Congratulations!"
0x080489d4 -> "/bin/sh"
0x080489dc -> "\nInvalid Password"
```

This shows that `decrypt` compares its decoded local string with `"Congratulations!"`. If they match, it executes `system("/bin/sh")`; otherwise, it prints `"Invalid Password"`.

The following constants are not addresses:

```text
0x757c7d51
0x67667360
0x7b66737e
0x33617c7d
```

They are the encrypted bytes copied into a local buffer. They will be processed by the XOR loop later in the analysis.

### Understand the byte order

Check the binary format:

```sh
file level03/src/level03
```

The relevant part of the output is:

```text
ELF 32-bit LSB executable, Intel 80386
```

`LSB` means that the least-significant byte is stored first. This is little-endian byte order.

For example, `decrypt` contains:

```asm
mov DWORD PTR [ebp-0x1d],0x757c7d51
```

`DWORD` is 32 bits, or four bytes. First split the value into four hexadecimal byte pairs:

```text
75 | 7c | 7d | 51
```

The least-significant byte is the rightmost pair, `51`. Because the machine is little-endian, it is stored at the lowest memory address first:

```text
Value:  0x75 0x7c 0x7d 0x51
Memory: 0x51 0x7d 0x7c 0x75
ASCII:     Q    }    |    u
```

Therefore, `0x757c7d51` represents these four encrypted characters in memory:

```text
Q}|u
```

Verify the four bytes with the shell `printf` command:

```sh
printf '\x51\x7d\x7c\x75\n'
```

It prints:

```text
Q}|u
```

The general rule for a 32-bit value is:

```text
Value:  0xAABBCCDD
Memory: 0xDD 0xCC 0xBB 0xAA
```

This reversal is needed when interpreting the raw bytes of a packed value. Addresses displayed normally by GDB are still read as ordinary hexadecimal numbers.

Apply the same conversion to all four constants:

```sh
printf '\x51\x7d\x7c\x75\n'
printf '\x60\x73\x66\x67\n'
printf '\x7e\x73\x66\x7b\n'
printf '\x7d\x7c\x61\x33\n'
```

This prints the four decoded blocks on separate lines:

```text
Q}|u
`sfg
~sf{
}|a3
```

Their correspondence is:

```text
0x757c7d51 -> Q}|u
0x67667360 -> `sfg
0x7b66737e -> ~sf{
0x33617c7d -> }|a3
```

Inside `decrypt`, the four blocks are contiguous and contain no newline between them. Print the complete encrypted string with one command:

```sh
printf '\x51\x7d\x7c\x75\x60\x73\x66\x67\x7e\x73\x66\x7b\x7d\x7c\x61\x33\n'
```

The complete encrypted string is:

```text
Q}|u`sfg~sf{}|a3
```

## Find the XOR key

The loop in `decrypt` takes each byte from the encrypted string and XORs it with the argument received by the function:

```asm
lea    eax,[ebp-0x1d]
add    eax,DWORD PTR [ebp-0x28]
movzx  eax,BYTE PTR [eax]
mov    edx,eax
mov    eax,DWORD PTR [ebp+0x8]
xor    eax,edx
mov    edx,eax
lea    eax,[ebp-0x1d]
add    eax,DWORD PTR [ebp-0x28]
mov    BYTE PTR [eax],dl
```

In simplified C, the operation is:

```c
decrypted[i] = encrypted[i] ^ key;
```

We already decoded the string at `0x80489c3` as `"Congratulations!"`. That is the value expected after the XOR operation.

Compare the encrypted and expected characters in hexadecimal:

```text
Encrypted:  Q     }     |     u
Hex:       0x51  0x7d  0x7c  0x75

Expected:   C     o     n     g
Hex:       0x43  0x6f  0x6e  0x67
```

XOR each encrypted byte with the corresponding expected byte:

```text
0x51 ^ 0x43 = 0x12
0x7d ^ 0x6f = 0x12
0x7c ^ 0x6e = 0x12
0x75 ^ 0x67 = 0x12
```

Every pair gives the same key:

```text
key = 0x12 = 18
```

Verify the XOR operation with `level03/src/xor_decode.c`:

```sh
gcc -Wall -Wextra -Werror level03/src/xor_decode.c -o level03/src/xor_decode
./level03/src/xor_decode
```

The program does not hardcode the key. It tries every possible one-byte value from `0x00` through `0xff`:

```c
decrypted[i] = encrypted[i] ^ key;
```

It keeps the key whose result matches the string used by `decrypt`, `"Congratulations!"`. It prints:

```text
key: 0x12 (18)
encrypted: Q}|u`sfg~sf{}|a3
decrypted: Congratulations!
password: 322424827
```

## Find the required input

In `main`, the entered number is the first argument to `test`, and `0x1337d00d` is its second argument:

```asm
mov    eax,DWORD PTR [esp+0x1c]
mov    DWORD PTR [esp+0x4],0x1337d00d
mov    DWORD PTR [esp],eax
call   0x8048747 <test>
```

This is equivalent to:

```c
test(input, 0x1337d00d);
```

At the beginning of `test`, the second argument is copied and the first argument is subtracted from it:

```asm
mov    eax,DWORD PTR [ebp+0x8]  /* input */
mov    edx,DWORD PTR [ebp+0xc]  /* 0x1337d00d */
mov    ecx,edx
sub    ecx,eax
```

Therefore, the value passed to `decrypt` is:

```text
key = 0x1337d00d - input
```

We need that key to equal `0x12`, so solve for the input:

```text
0x1337d00d - input = 0x12
input = 0x1337d00d - 0x12
input = 0x1337cffb
input = 322424827
```

The hexadecimal calculation can be verified in the shell:

```sh
printf '%d\n' $((0x1337d00d - 0x12))
```

Run the program on the VM and enter the resulting decimal value:

```sh
./level03
```

```text
Password: 322424827
```

## Get the level04 password

Run `level03` on the VM:

```sh
./level03
```

Enter the recovered decimal password:

```text
322424827
```

The correct value makes `decrypt` execute `/bin/sh`. Confirm that the shell has level04 privileges, then read the next password:

```sh
cat /home/users/level04/.pass
```

Back on the host, store the displayed password in:

```text
level03/flag
```

## Connect to level04

Connect using the password saved in `level03/flag`:

```sh
ssh level04@127.0.0.1 -p 2222
```
