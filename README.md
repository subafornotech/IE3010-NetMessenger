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

## Using the client

    REGISTER <name>               register first
    LIST                          list online users
    BCAST <message>               message everyone
    PMSG <user> <message>         private message
    JOIN <room> / LEAVE <room>    join or leave a room (JOIN creates it)
    ROOMS                         list rooms
    RMSG <room> <message>         message a room (members only)
    SENDFILE <user|room> <path>   send a file (the client adds the size)
    QUIT                          leave (Ctrl+D also quits)

Received files are saved in ./downloads/. The server keeps a copy in ./storage/IT23700710/<sender>/.

## Error codes

| Code | Reason | When |
|---|---|---|
| 001 | USERNAME_TAKEN | REGISTER with a name already in use |
| 002 | USER_NOT_FOUND | PMSG or SENDFILE to an unknown user or room |
| 003 | ROOM_NOT_FOUND | LEAVE or RMSG on a room that does not exist |
| 004 | FILE_TOO_LARGE | SENDFILE over 10 MB |
| 005 | LINE_TOO_LONG | a line longer than 4096 bytes |
| 006 | NOT_REGISTERED | any command before REGISTER |
| 007 | UNKNOWN_COMMAND | unrecognised command |
| 008 | MISSING_ARGUMENT | a command without its required arguments |
| 009 | INVALID_USERNAME | empty, too long, bad characters, or "SERVER" |
| 010 | ALREADY_REGISTERED | REGISTER twice on one connection |
| 011 | SERVER_FULL | more than 32 clients |
| 012 | NOT_IN_ROOM | LEAVE, RMSG or SENDFILE to a room you are not in |
| 013 | ROOM_LIMIT_REACHED | no free room slots (max 32) |
| 014 | INVALID_NAME | bad room name or filename |
| 015 | RATE_LIMITED | more than 5 messages per second (extension) |

Codes 001-004 are from the brief; 005-015 are my own.

## Design assumptions

- Join/leave notices use `MSG BCAST SERVER <name> has joined/left`, so "SERVER" is a reserved username.
- Names (users and rooms): 1-31 characters, letters, digits, `_` or `-`.
- Only room members can send to a room. A room is deleted when its last member leaves.
- Files are forwarded as `MSG FILE <sender> <filename> <size>` followed by the raw bytes.
- SENDFILE checks users before rooms if a name matches both. Maximum file size is 10 MB.
- File paths given to the client cannot contain spaces.

## Optional extension

Rate limiting: each client may send at most 5 BCAST/PMSG/RMSG messages per second; extra messages get ERR 015.
