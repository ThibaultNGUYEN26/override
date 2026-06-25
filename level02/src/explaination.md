# Level02 breach explanation

This level looks like a normal login program:

1. It asks for a username.
2. It asks for a password.
3. It checks whether the password is correct.

But the program accidentally leaks the real password when the login fails.

## What the program does

Inside the program, we find that it opens this file:

```text
/home/users/level03/.pass
```

That file contains the password for the next level.

The program reads that password into memory, then compares it with the password
we type.

So the secret is already inside the program's memory while it is running. We
just need a way to make the program print it.

## The mistake

When the password is wrong, the program prints the username.

The safe way would be:

```c
printf("%s", username);
```

That means:

```text
Print this as normal text.
```

But the program does this instead:

```c
printf(username);
```

That is dangerous.

`printf` does not treat the username as simple text anymore. It treats it as a
format string.

## What is a format string?

Format strings are special instructions for `printf`.

For example:

```c
printf("Hello %s", name);
```

`%s` means:

```text
Print a string here.
```

There are many format codes. One important one is:

```text
%p
```

`%p` means:

```text
Print a pointer/address-looking value from memory.
```

So if our username is:

```text
%p %p %p
```

the program does not print the literal text `%p %p %p`. It starts printing
values from the stack.

That is called a format string vulnerability.

## Why this leaks the password

The real level03 password is stored in a local stack buffer.

The stack is memory used by the current function. It contains local variables,
temporary values, saved addresses, and sometimes secrets.

Because the username is passed directly to `printf`, we can ask `printf` to
print values from the stack.

By checking the stack layout, we find that the secret starts around the 22nd
printed value.

So we use this as the username:

```text
%22$p
```

Then we type any wrong password, like:

```text
test
```

The login fails, and the program prints a value from where the secret password
is stored.

## Getting the full password

One `%p` prints 8 bytes on this 64-bit program.

The password is about 40 characters, so we print several stack values:

```text
%22$p %23$p %24$p %25$p %26$p %27$p
```

The output looks like hexadecimal numbers, for example:

```text
0x756e505234376848
```

This is not directly readable because the computer stores bytes in
little-endian order.

Little-endian means the bytes are stored backwards inside each 8-byte block.

So this value:

```text
0x756e505234376848
```

is split into bytes:

```text
75 6e 50 52 34 37 68 48
```

Then reversed:

```text
48 68 37 34 52 50 6e 75
```

And converted to text:

```text
Hh74RPnu
```

Doing that for all the leaked blocks gives the full password:

```text
Hh74RPnuQ9sa5JAEXgNWCqz7sXGnh5J5M9KfPg3H
```

## Final idea

The breach works like this:

1. The program reads the level03 password into memory.
2. The login fails because we do not know the password yet.
3. On failure, the program prints our username using `printf(username)`.
4. We use `%p` format codes as the username.
5. `printf` leaks stack values.
6. Those stack values contain the password.
7. We reverse the bytes of each leaked block because the machine is
   little-endian.
8. We recover the level03 password.

The important lesson is:

```text
Never pass user input directly as the first argument to printf. Use a fixed
format like printf("%s", user_input), or attackers can make printf read and
print memory it was never supposed to show.
```
