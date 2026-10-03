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

/* ================= shared state ================= */

typedef struct {
    int  active;                   /* 1 = this slot is in use */
    int  fd;                       /* this client's socket */
    char name[NAME_LEN];           /* "" until REGISTER succeeds */
    char ip[INET_ADDRSTRLEN];      /* client's IP address as text */
    char buf[BUF_SIZE];            /* bytes received but not yet processed */
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

FILE *log_fp;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;     /* one log line at a time */

int slot_of(client_t *c) { return (int)(c - clients); }   /* array index of a client */

/* ================= logging ================= */

/* Writes "[date time] message" to netmsg_IT23700710.log and to the screen. */
void log_event(const char *fmt, ...) {
    char msg[BUF_SIZE], ts[32];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);                  /* thread-safe version of localtime */
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tmv);

    pthread_mutex_lock(&log_lock);
    fprintf(log_fp, "[%s] %s\n", ts, msg);
    fflush(log_fp);                             /* write to disk now */
    printf("[%s] %s\n", ts, msg);
    pthread_mutex_unlock(&log_lock);
}

/* ================= sending ================= */

/* send() may send fewer bytes than asked: keep going until all are sent. */
int send_all(int fd, const void *data, size_t n) {
    const char *p = data;
    while (n > 0) {
        ssize_t s = send(fd, p, n, 0);
        if (s < 0 && errno == EINTR) continue;   /* interrupted: try again */
        if (s <= 0) return -1;                    /* client is gone */
        p += s;
        n -= s;
    }
    return 0;
}

/* Reply to the client who sent the command. Adds " NID:7007" to EVERY OK/ERR line. */
void reply(client_t *c, const char *fmt, ...) {
    char body[BUF_SIZE], line[BUF_SIZE + 32];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof body, fmt, ap);
    va_end(ap);
    int n = snprintf(line, sizeof line, "%s NID:%s\n", body, NID);
    pthread_mutex_lock(&c->send_lock);
    send_all(c->fd, line, n);
    pthread_mutex_unlock(&c->send_lock);
}

/* Forward a MSG line to another client. No NID tag. Call with state_lock held. */
void deliver(client_t *c, const char *line) {
    pthread_mutex_lock(&c->send_lock);
    send_all(c->fd, line, strlen(line));
    pthread_mutex_unlock(&c->send_lock);
}

/* ================= helpers ================= */

/* Find a registered user by name. Call with state_lock held. */
client_t *find_user(const char *name) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].active && !strcmp(clients[i].name, name))
            return &clients[i];
    return NULL;
}

/* Names: 1-31 chars, letters, digits, _ or - only (no spaces or commas). */
int valid_name(const char *s) {
    size_t n = strlen(s);
    if (n == 0 || n >= NAME_LEN) return 0;
    for (; *s; s++)
        if (!isalnum((unsigned char)*s) && *s != '_' && *s != '-') return 0;
    return 1;
}

/* Send a line to every registered client except 'skip' (NULL = everyone). */
void send_to_all_except(client_t *skip, const char *line) {
    pthread_mutex_lock(&state_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_t *o = &clients[i];
        if (o->active && o->name[0] && o != skip) deliver(o, line);
    }
    pthread_mutex_unlock(&state_lock);
}

int room_empty(room_t *r) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (r->member[i]) return 0;
    return 1;
}

/* ================= commands ================= */

void cmd_register(client_t *c, char *name) {
    if (c->name[0]) { reply(c, "ERR 010 ALREADY_REGISTERED"); return; }
    if (!valid_name(name) || !strcmp(name, "SERVER")) {   /* SERVER is reserved for notices */
        reply(c, "ERR 009 INVALID_USERNAME");
        return;
    }
    pthread_mutex_lock(&state_lock);
    if (find_user(name)) {                 /* check and set under ONE lock: no race */
        pthread_mutex_unlock(&state_lock);
        reply(c, "ERR 001 USERNAME_TAKEN");
        return;
    }
    strcpy(c->name, name);
    pthread_mutex_unlock(&state_lock);

    reply(c, "OK REGISTERED %s", c->name);
    char note[128];
    snprintf(note, sizeof note, "MSG BCAST SERVER %s has joined\n", c->name);
    send_to_all_except(c, note);
    log_event("REGISTER %s from %s", c->name, c->ip);
}

void cmd_list(client_t *c) {
    char out[BUF_SIZE] = "";
    pthread_mutex_lock(&state_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].name[0]) {
            if (out[0]) strcat(out, ",");
            strcat(out, clients[i].name);
        }
    }
    pthread_mutex_unlock(&state_lock);
    reply(c, "OK USERS %s", out);
}

/* ================= dispatcher ================= */

/* Returns -1 when the connection should close (QUIT), otherwise 0. */
int handle_command(client_t *c, char *line) {
    char cmd[16] = "";
    sscanf(line, "%15s", cmd);
    char *args = line + strlen(cmd);
    while (*args == ' ') args++;          /* args = everything after the command word */

    if (cmd[0] == '\0') return 0;         /* ignore empty lines */

    if (!strcmp(cmd, "QUIT")) { reply(c, "OK BYE"); return -1; }
    if (!strcmp(cmd, "REGISTER")) { cmd_register(c, args); return 0; }

    if (c->name[0] == '\0') {             /* REGISTER must come first */
        reply(c, "ERR 006 NOT_REGISTERED");
        return 0;
    }

    if (!strcmp(cmd, "LIST")) cmd_list(c);
    else reply(c, "ERR 007 UNKNOWN_COMMAND");
    return 0;
}

/* ================= disconnect cleanup ================= */

void cleanup_client(client_t *c, int graceful) {
    int s = slot_of(c);
    char name[NAME_LEN], ip[INET_ADDRSTRLEN];
    strcpy(name, c->name);        /* copy now: the slot may be reused after we free it */
    strcpy(ip, c->ip);

    pthread_mutex_lock(&state_lock);
    for (int i = 0; i < MAX_ROOMS; i++) {        /* remove from every room */
        if (rooms[i].active && rooms[i].member[s]) {
            rooms[i].member[s] = 0;
            if (room_empty(&rooms[i])) rooms[i].active = 0;
        }
    }
    close(c->fd);
    c->name[0] = '\0';
    c->len = 0;
    c->active = 0;                /* slot is free for the next connection */
    pthread_mutex_unlock(&state_lock);

    if (name[0]) {
        char note[128];
        snprintf(note, sizeof note, "MSG BCAST SERVER %s has left\n", name);
        send_to_all_except(NULL, note);
    }
    log_event("%s %s (%s)", graceful ? "QUIT" : "CONNECTION LOST",
              name[0] ? name : "(unregistered)", ip);
}

/* ================= one thread per client ================= */

void *client_thread(void *arg) {
    int slot = *(int *)arg;
    free(arg);
    client_t *c = &clients[slot];
    log_event("CONNECT from %s", c->ip);
    int graceful = 0;

    for (;;) {
        ssize_t n = recv(c->fd, c->buf + c->len, BUF_SIZE - c->len, 0);
        if (n <= 0) break;            /* 0 = client closed, -1 = error/reset */
        c->len += n;

        char *nl;
        while ((nl = memchr(c->buf, '\n', c->len)) != NULL) {   /* one full line at a time */
            *nl = '\0';
            size_t linelen = nl - c->buf + 1;          /* includes the \n */
            char line[BUF_SIZE];
            memcpy(line, c->buf, linelen);
            /* remove the line from the buffer BEFORE handling it */
            memmove(c->buf, c->buf + linelen, c->len - linelen);
            c->len -= linelen;

            size_t L = strlen(line);
            if (L > 0 && line[L - 1] == '\r') line[L - 1] = '\0';  /* accept \r\n too */

            if (handle_command(c, line) < 0) { graceful = 1; goto done; }
        }
        if (c->len == BUF_SIZE) {     /* buffer full and still no \n */
            reply(c, "ERR 005 LINE_TOO_LONG");
            c->len = 0;
        }
    }
done:
    cleanup_client(c, graceful);
    return NULL;
}

/* ================= main: accept loop ================= */

int main(void) {
    signal(SIGPIPE, SIG_IGN);   /* don't die when writing to a closed socket */

    log_fp = fopen(LOGFILE, "a");
    if (!log_fp) { perror("log file"); return 1; }

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
    log_event("SERVER START on port %d", PORT);

    for (;;) {
        struct sockaddr_in cli;
        socklen_t len = sizeof cli;
        int cfd = accept(lfd, (struct sockaddr *)&cli, &len);
        if (cfd < 0) continue;

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

        if (slot < 0) {
            const char *msg = "ERR 011 SERVER_FULL NID:" NID "\n";
            send(cfd, msg, strlen(msg), 0);
            close(cfd);
            continue;
        }

        int *arg = malloc(sizeof *arg);
        *arg = slot;
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, arg);
        pthread_detach(tid);
    }
}
