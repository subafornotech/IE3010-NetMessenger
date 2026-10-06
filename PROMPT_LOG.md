# Prompt Log - AI use in Part 1 (IT23700710)

Tool used throughout: Claude (Anthropic).

| Date | What I asked | How I used the output |
|---|---|---|
| 3 Oct | Break down the assignment brief; recommend a resource to learn sockets | Used as a plan; read Beej's Guide to Network Programming |
| 3 Oct | Step-by-step build guide; my personalised values from IT23700710 | Checked the port/NID calculation myself; noted the octal problem with 0710 |
| 3 Oct | How to use GitHub, tokens and git from the VM | Set up the repo; fixed my own login error (wrong username, password instead of token) |
| 3 Oct | Code for the first server, then threads, then the protocol core and BCAST/PMSG | Typed in step by step, compiled and tested each step with nc before moving on |
| 3 Oct | Help with a compiler warning about logf | AI's code had used logf as a variable name, clashing with the C maths function; I renamed it to log_fp |
| 3 Oct | Why broadcasts seemed missing in some windows | Found it was a terminal display issue, not a server bug, by testing with LIST |
| 4-5 Oct | Explanations of socket(), bind(), listen(), accept(), blocking, sockaddr_in, INADDR_ANY, threads, mutexes, make, .gitignore | Used to understand my own code for the viva; asked follow-up questions where unclear |
| 6 Oct | Report requirements and a plan for the remaining work | Built the remaining features before writing the report |
| 6 Oct | Code for rooms (JOIN/LEAVE/ROOMS/RMSG) | Added to my server, tested every success and error case with three clients |
| 6 Oct | Code for SENDFILE and the client | Tested with nc first, then with the client and a 2 MB file; verified with sha256sum |
| 6 Oct | Rate-limiting extension | Added the code myself in nano; tested with 12 rapid broadcasts (5 accepted, 7 rejected) |
| 6 Oct | Fixing the VM clock and an accidentally committed test file | Used timedatectl/date and git rm --cached |
| 6 Oct | Drafts of this log, the design diary, README additions and report structure | Reviewed and edited them to match what I actually did |
