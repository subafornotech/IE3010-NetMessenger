# Design Diary - NetMessenger (IT23700710)

## Sat 3 Oct 2026
- Set up the GitHub repo and tools (gcc, git, nc, nano) in the CentOS 10 VM, accessed over SSH from my Mac.
- Calculated personalised values: port 6710, NID 7007. Noticed the leading zero in 0710:
  in C, 0710 is octal (= 456), so 6000 + 0710 would give 6456. I write the port as 6710 directly.
- Decision: one thread per client, with a mutex (state_lock) protecting the shared client and room tables.
  Chosen over select() because each client's code reads top to bottom and a blocking recv()
  only blocks that client's thread. Trade-off: shared state needs locking.
- Built step by step: basic socket/bind/listen/accept server, then threads (tested 5 clients at once),
  then line framing with a per-client buffer, REGISTER/LIST/QUIT, the NID tag via one reply() function,
  logging and disconnect cleanup, then BCAST and PMSG.
- Obstacle: compiler warning "built-in function 'logf' declared as non-function". My log file variable
  was named logf, which clashes with the maths library function logf(). Renamed it to log_fp.
- Obstacle: broadcast seemed not to reach some clients. The server had delivered it; the Mac Terminal
  windows were not showing their newest lines. Confirmed with LIST and a second broadcast.

## Sun 4 Oct - Mon 5 Oct 2026
- Studied my own code before adding more: socket/bind/listen/accept, blocking calls, byte order (htons),
  sockaddr_in and INADDR_ANY, why accept() returns a new socket, threads and mutexes, the Makefile.
- Practised small lab-style edits (adding a command) so I can modify the code without notes.

## Tue 6 Oct 2026
- Rooms: JOIN creates a room if needed; only members can send with RMSG; the last member to leave
  deletes the room. Each room stores a member[] flag per client slot.
- SENDFILE: the hardest part. After the header line, exactly <size> raw bytes follow. Some may already be
  in the line buffer from the same recv(), so read_exact() takes buffered bytes first, then recv()s the rest.
  The bytes are always consumed, even on an error, so the stream stays in sync.
  Files are forwarded as "MSG FILE <sender> <name> <size>" + bytes and stored in ./storage/IT23700710/<sender>/.
  Verified a 2 MB random file with sha256sum: original, received copy and stored copy match.
- Client: two threads - main thread reads the keyboard, receiver thread prints incoming messages and saves files.
- Extension: rate limiting, max 5 messages per second per client (ERR 015). No lock needed because each
  client's counter is only used by its own thread.
- Obstacle: the VM clock was about 10 hours behind, so early log timestamps and commits show the wrong time.
  Fixed the timezone and set the clock manually.
- Obstacle: committed a 2 MB test file by mistake; removed it with git rm --cached and added *.bin to .gitignore.
