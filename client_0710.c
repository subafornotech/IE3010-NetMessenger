#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT     6710                 /* 6000 + 0710 */
#define BUF_SIZE 4096

int sock;                             /* our connection to the server */
char rbuf[BUF_SIZE];                  /* bytes received but not yet processed */
size_t rlen = 0;

/* send() may send fewer bytes than asked: keep going until all are sent */
int send_all(int fd, const void *data, size_t n) {
    const char *p = data;
    while (n > 0) {
        ssize_t s = send(fd, p, n, 0);
        if (s <= 0) return -1;
        p += s;
        n -= s;
    }
    return 0;
}

/* Read exactly n bytes: first what is already in rbuf, then recv() the rest */
int read_exact(char *dst, size_t n) {
    size_t take = rlen < n ? rlen : n;
    memcpy(dst, rbuf, take);
    memmove(rbuf, rbuf + take, rlen - take);
    rlen -= take;
    size_t got = take;
    while (got < n) {
        ssize_t r = recv(sock, dst + got, n - got, 0);
        if (r <= 0) return -1;
        got += r;
    }
    return 0;
}

/* After a "MSG FILE <from> <name> <size>" line, read the file bytes and save them */
void receive_file(const char *from, const char *fname, long long size) {
    char *data = malloc(size > 0 ? size : 1);
    if (!data || read_exact(data, size) < 0) { printf("File transfer failed\n"); exit(1); }
    if (strchr(fname, '/') || strstr(fname, "..")) { free(data); return; }   /* unsafe name */
    mkdir("./downloads", 0755);
    char path[600];
    snprintf(path, sizeof path, "./downloads/%s_%s", from, fname);
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(data, 1, size, f); fclose(f); }
    printf("[file] %s from %s (%lld bytes) saved to %s\n", fname, from, size, path);
    free(data);
}

/* Receiver thread: prints everything the server sends, line by line */
void *receiver(void *arg) {
    (void)arg;
    for (;;) {
        ssize_t n = recv(sock, rbuf + rlen, sizeof rbuf - rlen, 0);
        if (n <= 0) { printf("Disconnected from server\n"); exit(0); }
        rlen += n;
        char *nl;
        while ((nl = memchr(rbuf, '\n', rlen)) != NULL) {
            *nl = '\0';
            size_t ll = nl - rbuf + 1;
            char line[BUF_SIZE];
            memcpy(line, rbuf, ll);
            memmove(rbuf, rbuf + ll, rlen - ll);
            rlen -= ll;

            char from[64], fname[256];
            long long size;
            if (sscanf(line, "MSG FILE %63s %255s %lld", from, fname, &size) == 3)
                receive_file(from, fname, size);          /* raw bytes follow */
            else
                printf("%s\n", line);
            fflush(stdout);
        }
        if (rlen == sizeof rbuf) rlen = 0;                /* overlong line: drop it */
    }
    return NULL;
}

/* Typed as: SENDFILE <user|room> <path>. Sends the header with the size, then the bytes. */
void send_file_cmd(char *args) {
    char target[64], path[512];
    if (sscanf(args, "%63s %511s", target, path) != 2) {
        printf("Usage: SENDFILE <user|room> <path>\n");
        return;
    }
    FILE *f = fopen(path, "rb");
    if (!f) { perror("open"); return; }
    struct stat st;
    fstat(fileno(f), &st);
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;                        /* send only the bare file name */

    char hdr[700];
    int n = snprintf(hdr, sizeof hdr, "SENDFILE %s %s %lld\n", target, name, (long long)st.st_size);
    send_all(sock, hdr, n);                               /* the command line ... */
    char chunk[BUF_SIZE];
    size_t r;
    while ((r = fread(chunk, 1, sizeof chunk, f)) > 0)
        send_all(sock, chunk, r);                         /* ... then exactly st_size bytes */
    fclose(f);
}

int main(int argc, char *argv[]) {
    const char *host = argc > 1 ? argv[1] : "127.0.0.1";

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons(PORT);
    if (inet_pton(AF_INET, host, &a.sin_addr) != 1) { fprintf(stderr, "Bad address\n"); return 1; }
    if (connect(sock, (struct sockaddr *)&a, sizeof a) < 0) { perror("connect"); return 1; }
    printf("Connected to %s:%d. Start with: REGISTER <name>\n", host, PORT);

    pthread_t t;
    pthread_create(&t, NULL, receiver, NULL);

    char line[BUF_SIZE];
    while (fgets(line, sizeof line, stdin)) {             /* fgets keeps the \n */
        if (!strncmp(line, "SENDFILE ", 9)) { send_file_cmd(line + 9); continue; }
        send_all(sock, line, strlen(line));
    }
    send_all(sock, "QUIT\n", 5);                          /* Ctrl+D: leave politely */
    sleep(1);
    return 0;
}
