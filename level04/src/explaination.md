# Level04 breach explanation

This level is a buffer overflow, but with an extra protection.

The program creates a child process, lets the child read our input, and the
parent watches the child with `ptrace`.

The important bug is that the child reads input with:

```c
gets(buffer);
```

`gets` is dangerous because it does not know the size of the buffer. It keeps
reading until a newline, even if that means writing past the end.

## The vulnerable buffer

The child has a local buffer on the stack.

Think of the stack like this:

```text
[ buffer ][ other saved data ][ saved return address ]
```

If we type a short input, it stays inside the buffer.

If we type a very long input, it overflows past the buffer and reaches the saved
return address.

The saved return address tells the CPU where to continue when the function
finishes.

If we overwrite it, we control where the program jumps next.

## Finding the offset

We test with:

```text
A * 156 + BBBB
```

In other words:

```text
156 filler bytes, then 4 B characters
```

When the program crashes, the instruction pointer becomes:

```text
0x42424242
```

`0x42` is the ASCII value for `B`.

So `0x42424242` means the four `B` characters became the next address to
execute.

That proves the saved return address is reached after `156` bytes.

## Why normal shellcode is blocked

In some earlier levels, we could put shellcode in memory and jump to it.

Here, the parent process watches the child with `ptrace`.

It checks whether the child tries to use syscall number:

```text
0xb
```

On 32-bit Linux, syscall `0xb` is:

```text
execve
```

`execve` is the syscall normal shellcode uses to run `/bin/sh`.

If the parent sees the child trying to use `execve`, it kills the child.

So direct shellcode is not the best plan.

## Return-to-libc

Instead of injecting new code, we reuse code that already exists in memory.

The C library, libc, already contains a function named:

```text
system
```

If we can call:

```c
system("/bin/sh");
```

we get a shell.

This technique is called return-to-libc.

It means:

```text
Do not jump to our own shellcode. Return into an existing libc function.
```

## The addresses

In GDB, we find three useful addresses:

```text
system     = 0xf7e6aed0
exit       = 0xf7e5eb70
"/bin/sh" = 0xf7f897ec
```

`system` is the function we want to call.

`"/bin/sh"` is the string we want to give to `system`.

`exit` is used as the fake return address after `system` finishes, so the stack
looks clean enough.

## The payload layout

After `156` padding bytes, we overwrite the stack like this:

```text
[ address of system ][ address of exit ][ address of "/bin/sh" ]
```

That makes the program behave like it called:

```c
system("/bin/sh");
```

The payload is:

```text
"A" * 156
+ system address
+ exit address
+ "/bin/sh" address
```

Because this is 32-bit x86, each address must be written in little-endian order.

That means the bytes are stored backwards. For example:

```text
0xf7e6aed0
```

is written as:

```text
\xd0\xae\xe6\xf7
```

## Why this bypasses the protection

The parent is watching for a direct `execve` syscall from the child.

Our payload does not jump to shellcode that immediately executes `execve`.

Instead, it returns into libc and calls `system`.

So we reuse trusted code already loaded in the process instead of injecting and
running our own code.

## Final idea

The breach works like this:

1. The child process reads input with unsafe `gets`.
2. We send more data than the stack buffer can hold.
3. After `156` bytes, we reach the saved return address.
4. We replace it with the address of `system`.
5. We place `exit` after it as a fake return address.
6. We place the address of `"/bin/sh"` after that as the argument to `system`.
7. The program returns into libc.
8. libc runs `system("/bin/sh")`.
9. From that shell, we can read the level05 password.

The important lesson is:

```text
Even if a program blocks injected shellcode, a stack overflow can still be
dangerous. If an attacker can control the return address, they may reuse
existing code in memory, such as libc's system function.
```
