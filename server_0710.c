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

/* ---------- shared state ---------- */

typedef struct {
    int  active;                   /* 1 = this slot is in use */
    int  fd;                       /* this client's socket */
    char name[NAME_LEN];           /* "" until REGISTER succeeds */
    char ip[INET_ADDRSTRLEN];      /* client's IP address as text */
    char buf[BUF_SIZE];            /* bytes received but not yet processed (step 4) */
    size_t len;                    /* how many bytes are in buf */
    pthread_mutex_t send_lock;     /* only one thread writes to this socket at a time */
} client_t;

typedef struct {
    int  active;
    char name[NAME_LEN];
    int  member[MAX_CLIENTS];      /* member[i] = 1 if clients[i] is in this room */
} room_t;

client_t clients[MAX_CLIENTS];
room_t   rooms[MAX_ROOMS];
pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;   /* protects clients[] and rooms[] */

int slot_of(client_t *c) { return (int)(c - clients); }   /* array index of a client */

/* ---------- one thread per client ---------- */

/* TEMPORARY: echoes back whatever the client sends. Replaced in step 4. */
void *client_thread(void *arg) {
    int slot = *(int *)arg;
    free(arg);
    client_t *c = &clients[slot];
    printf("Client %d connected from %s\n", slot, c->ip);

    char buf[BUF_SIZE];
    ssize_t n;
    while ((n = recv(c->fd, buf, sizeof buf, 0)) > 0) {
        send(c->fd, buf, n, 0);            /* echo it back */
    }
    /* recv returned 0 (client closed) or -1 (error) */
    printf("Client %d disconnected\n", slot);

    pthread_mutex_lock(&state_lock);
    close(c->fd);
    c->active = 0;                         /* free the slot for a new client */
    pthread_mutex_unlock(&state_lock);
    return NULL;
}

/* ---------- main: accept loop ---------- */

int main(void) {
    signal(SIGPIPE, SIG_IGN);   /* don't die when writing to a closed socket */

    for (int i = 0; i < MAX_CLIENTS; i++)
        pthread_mutex_init(&clients[i].send_lock, NULL);

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(PORT);

    if (bind(lfd, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); return 1; }
    if (listen(lfd, 16) < 0) { perror("listen"); return 1; }
    printf("NetMessenger server listening on port %d\n", PORT);

    for (;;) {
        struct sockaddr_in cli;
        socklen_t len = sizeof cli;
        int cfd = accept(lfd, (struct sockaddr *)&cli, &len);
        if (cfd < 0) continue;

        /* find a free slot in clients[] */
        int slot = -1;
        pthread_mutex_lock(&state_lock);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) { slot = i; break; }
        }
        if (slot >= 0) {
            client_t *c = &clients[slot];
            c->active  = 1;
            c->fd      = cfd;
            c->len     = 0;
            c->name[0] = '\0';
            inet_ntop(AF_INET, &cli.sin_addr, c->ip, sizeof c->ip);
        }
        pthread_mutex_unlock(&state_lock);

        if (slot < 0) {   /* all 32 slots taken */
            const char *msg = "ERR 011 SERVER_FULL NID:" NID "\n";
            send(cfd, msg, strlen(msg), 0);
            close(cfd);
            continue;
        }

        /* start a thread for this client */
        int *arg = malloc(sizeof *arg);    /* heap copy: 'slot' changes on the next loop */
        *arg = slot;
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, arg);
        pthread_detach(tid);               /* thread cleans itself up when it ends */
    }
}
