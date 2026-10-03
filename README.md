# NetMessenger - IE3010 Network Programming Assignment

Multi-client chat and file-sharing platform over TCP/IP, written in C using BSD sockets.

## Student details

- Registration number: IT23700710

## Personalised values

| Item | Formula | Value |
|---|---|---|
| Port | 6000 + last four digits (0710) | 6710 |
| Server source | server_<last4>.c | server_0710.c |
| Client source | client_<last4>.c | client_0710.c |
| Makefile | Makefile_<last4> | Makefile_0710 |
| NID tag | digits 3-6 of 23700710 | NID:7007 |
| Log file | netmsg_<regno>.log | netmsg_IT23700710.log |
| Storage path | ./storage/<regno>/<sender>/<filename> | ./storage/IT23700710/<sender>/<filename> |
| Submission archive | IE3010_<regno>.zip | IE3010_IT23700710.zip |

Note: the last four digits are 0710. The port is written as 6710 in the code,
because a number with a leading zero (0710) is treated as octal in C.

## Build

    make -f Makefile_0710

## Run

Start the server:

    ./server_0710

Start a client (in another terminal):

    ./client_0710              # connects to 127.0.0.1
    ./client_0710 <server-ip>  # connects to another machine

## Files

- server_0710.c - server program
- client_0710.c - client program
- Makefile_0710 - build file
- DESIGN_DIARY.md - design decisions and obstacles
- PROMPT_LOG.md - record of AI tool use
