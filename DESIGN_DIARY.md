# Design Diary

## 3 Oct 2026
- Set up GitHub repo, CentOS VM tools and personalised Makefile.
- Calculated personalised values from IT23700710: port 6710, NID 7007.
- Noticed the leading zero in 0710: writing 6000 + 0710 in C would be octal (= 6456), so the port is written as 6710.
- Plan: one thread per client, with a mutex protecting the shared user and room lists.
