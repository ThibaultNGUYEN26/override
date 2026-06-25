# Level01 breach explanation

Imagine the program is a locked door with two questions:

1. What is your username?
2. What is your password?

At first, it looks like we just need to find the correct username and password.
But the real problem is that the password box is badly built, and we can make it
spill into important memory.

## The username

By looking inside the program with GDB, we find that it compares the username
with this text:

```text
dat_wil
```

So the first part is simple. The username must start with:

```text
dat_wil
```

The program stores this username in a global memory area. That matters because
global memory stays in a predictable place while the program runs.

For this level, that username memory starts here:

```text
0x0804a040
```

## The fake password

The program also contains this password:

```text
admin
```

So it looks like `admin` should work. But when we try:

```text
username: dat_wil
password: admin
```

the program still says the password is wrong.

That is because the normal success path is basically unreachable. The program
checks the password result in a broken way, so even the password found inside the
binary does not let us pass normally.

So this level is not solved by logging in normally. We need to take control of
where the program goes next.

## The real bug

The password is stored in a small stack buffer.

Think of the stack like a row of boxes:

```text
[ password buffer ][ other data ][ saved return address ]
```

The password buffer has room for about `64` bytes.

But the program lets us type up to `100` bytes for the password.

That is the mistake.

If we type too much, our password does not stop inside the password box. It
overflows into the next boxes. Eventually, it reaches the saved return address.

The saved return address is very important. It tells the CPU where to continue
after the current function finishes.

If we overwrite it, we can say:

```text
When you are done, jump to my code instead.
```

That is called a buffer overflow.

## Where do we put our code?

We need two things:

1. Some code we want the program to run.
2. A way to make the program jump to that code.

The code we use is shellcode. It is a small piece of machine code that runs:

```text
/bin/sh
```

That gives us a shell as the current user.

We put the shellcode right after the valid username:

```text
dat_wil + shellcode
```

Because the username is stored at:

```text
0x0804a040
```

and `dat_wil` is 7 bytes long, the shellcode starts at:

```text
0x0804a040 + 7 = 0x0804a047
```

So our target address is:

```text
0x0804a047
```

That is the address we want to write over the saved return address.

## Why the payload is one long input

The program reads input twice:

```c
fgets(username, 0x100, stdin);
fgets(password, 0x64, stdin);
```

The first `fgets` reads the username. It can read up to `255` real characters.

If our first line is long enough and does not end early, some bytes are left
waiting in standard input. Then the second `fgets` reads those remaining bytes as
the password.

So we send one long payload:

```text
dat_wil + shellcode + padding + new return address
```

The payload does three jobs:

1. Starts with `dat_wil`, so the username check passes.
2. Stores shellcode in the global username buffer.
3. Overflows the password buffer and replaces the saved return address.

## The padding

Padding is just filler bytes. In the exploit, the filler is `X`.

We do not care about the filler itself. We only use it to reach the exact spot
where the saved return address is stored.

Using a cyclic pattern in GDB, we find that the saved return address is reached
after `80` bytes in the second input.

Because one leftover byte from the first input is already read by the second
`fgets`, we add `79` more bytes before overwriting the return address.

That is why the exploit uses:

```python
padding = "X" * (256 - len(user + shellcode) + 79)
```

## Little-endian address

The CPU is 32-bit x86, and x86 stores addresses backwards in memory.

So this address:

```text
0x0804a047
```

must be written as these bytes:

```text
\x47\xa0\x04\x08
```

The exploit writes that at the end of the payload. When the function returns,
the CPU reads this overwritten return address and jumps to our shellcode.

## Final idea

The breach works like this:

1. Give the correct username: `dat_wil`.
2. Hide shellcode right after the username in global memory.
3. Send too much password data.
4. Overflow the password buffer.
5. Replace the saved return address with `0x0804a047`.
6. The program returns into our shellcode.
7. The shellcode opens `/bin/sh`.
8. From that shell, read the next level password.

The important lesson is:

```text
If a program writes more data into a buffer than the buffer can hold, nearby
memory can be overwritten. If the overwritten memory controls execution, an
attacker can make the program run their own code.
```
