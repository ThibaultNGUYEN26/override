# Level03 breach explanation

This level is not a buffer overflow. It is more like a puzzle hidden inside the
program.

The program asks for a number:

```text
Password:
```

If the number is right, the program runs:

```text
/bin/sh
```

That gives us a shell with level04 privileges.

## What the program checks

In `main`, the program reads a decimal number with:

```text
%d
```

That means it expects an integer, not a normal text password.

Then it calls a function named `test` with two values:

```c
test(input, 0x1337d00d);
```

The first value is our input.

The second value is a fixed number:

```text
0x1337d00d
```

## The key calculation

Inside `test`, the program subtracts our input from the fixed number:

```text
key = 0x1337d00d - input
```

Then it eventually calls another function named `decrypt` using that key.

So the number we type controls the key used by `decrypt`.

## What decrypt does

`decrypt` has an encrypted string stored inside the program:

```text
Q}|u`sfg~sf{}|a3
```

It loops over each character and XORs it with the key.

In simple form:

```c
decrypted[i] = encrypted[i] ^ key;
```

XOR is a bit operation. You can think of it like a reversible scramble:

```text
encrypted XOR key = decrypted
decrypted XOR key = encrypted
```

So if we find the right key, the encrypted text turns into the expected message.

## The expected message

The program compares the decrypted result with:

```text
Congratulations!
```

If the result matches, it runs:

```c
system("/bin/sh");
```

If it does not match, it prints:

```text
Invalid Password
```

So the goal is:

```text
Find the key that changes Q}|u`sfg~sf{}|a3 into Congratulations!
```

## Finding the XOR key

Look at the first character of each string:

```text
Encrypted: Q
Expected:  C
```

In hexadecimal:

```text
Q = 0x51
C = 0x43
```

To find the key:

```text
0x51 ^ 0x43 = 0x12
```

`0x12` is `18` in decimal.

Checking the next letters gives the same key:

```text
} ^ o = 0x12
| ^ n = 0x12
u ^ g = 0x12
```

So the correct key is:

```text
18
```

## Finding the number to type

The program calculates the key like this:

```text
key = 0x1337d00d - input
```

We need:

```text
key = 0x12
```

So:

```text
0x1337d00d - input = 0x12
```

Move the numbers around:

```text
input = 0x1337d00d - 0x12
```

That gives:

```text
input = 0x1337cffb
```

In decimal, that is:

```text
322424827
```

That is the password we type into the program.

## Final idea

The breach works like this:

1. The program asks for a number.
2. It subtracts our number from `0x1337d00d`.
3. The result becomes an XOR key.
4. The correct XOR key is `0x12`, or `18`.
5. So we solve `0x1337d00d - input = 0x12`.
6. The input must be `322424827`.
7. That makes the encrypted string decrypt to `Congratulations!`.
8. The program then runs `/bin/sh`.
9. From that shell, we can read the level04 password.

The important lesson is:

```text
Not every breach is about smashing memory. Sometimes the secret is in the
program logic. If you can reverse the checks, you can calculate the exact input
the program wants.
```
