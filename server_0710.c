#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT        6710                  /* 6000 + 0710 (written as 6710: 0710 would be octal in C) */
#define NID         "7007"                /* digits 3-6 of 23700710 */
#define REGNO       "IT23700710"
#define LOGFILE     "netmsg_" REGNO ".log"
#define MAX_CLIENTS 32
#define MAX_ROOMS   32
#define NAME_LEN    32
#define BUF_SIZE    4096
#define MAX_FILE    (10 * 1024 * 1024)    /* 10 MB */

int main(void) {
    signal(SIGPIPE, SIG_IGN);   /* don't die when writing to a closed socket */

    int lfd = socket(AF_INET, SOCK_STREAM, 0);       /* create a TCP socket */
    if (lfd < 0) { perror("socket"); return 1; }

    int opt = 1;   /* lets you restart the server immediately on the same port */
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;                  /* IPv4 */
    addr.sin_addr.s_addr = htonl(INADDR_ANY);        /* accept on any network interface */
    addr.sin_port        = htons(PORT);              /* port in network byte order */

    if (bind(lfd, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); return 1; }
    if (listen(lfd, 16) < 0) { perror("listen"); return 1; }
    printf("NetMessenger server listening on port %d\n", PORT);

    for (;;) {
        struct sockaddr_in cli;
        socklen_t len = sizeof cli;
        int cfd = accept(lfd, (struct sockaddr *)&cli, &len);  /* waits for a client */
        if (cfd < 0) continue;
        printf("Connection from %s\n", inet_ntoa(cli.sin_addr));
        send(cfd, "hello\n", 6, 0);
        close(cfd);
    }
}
