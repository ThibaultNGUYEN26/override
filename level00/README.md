# level00

For this level, Binary Ninja gives a clean enough decompilation directly.

## What Binary Ninja does here

Binary Ninja reads the compiled ELF binary and rebuilds readable pseudo-C from
the assembly. Here it identifies `main`, resolves calls like `puts`, `printf`,
`scanf`, and `system`, shows the strings behind their addresses, and turns the
password comparison into simple C logic.

## Get the binary

From the VM:

```sh
scp -P <port> level00@<host>:/home/users/level00/level00 ./level00
```

The local binary used for this writeup is:

```text
level00
```

Open this file in [Binary Ninja](https://cloud.binary.ninja/).

## Find `main`

In Binary Ninja:

1. Open `level00`.
2. Go to the symbol list.
3. Select `main`.
4. Use the decompiler view (Linear Disassembly).

The relevant decompiled logic is:

```c
int32_t main(int32_t argc, char** argv, char** envp)
{
    puts("***********************************");
    puts("* \t     -Level00 -\t\t  *");
    puts("***********************************");
    printf("Password:");

    int32_t var_14;
    __isoc99_scanf("%d", &var_14);

    if (var_14 != 0x149c)
    {
        puts("\nInvalid Password!");
        return 1;
    }

    puts("\nAuthenticated!");
    system("/bin/sh");
    return 0;
}
```

## Understand the check

The program reads a decimal integer:

```c
__isoc99_scanf("%d", &var_14);
```

Then it compares it with:

```c
0x149c
```

Convert it to decimal:

```sh
printf "%d\n" 0x149c
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
