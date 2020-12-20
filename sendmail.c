/*
 * Simple sendmail for trap Cron output or PHP mail() function
 *
 * Redirects mails from stdin to the log file specified in the
 * environment variable SENDMAIL_LOG
 *
 * gcc -Wall -static -o sendmail sendmail.c
 *
 * License MIT
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUFERR_SIZE 4096
#define MAX_RETRIES 10

volatile int sigcaught = 0;

void term(int signum)
{
    sigcaught = signum;
}

int main(int argc, char *argv[])
{
    FILE *fout = NULL;
    char *logfile = NULL;
    int locked = -1;
    int narg, nretries = 0;
    size_t nread, nwrite = 0;
    struct tm *timeinfo = NULL;
    time_t rawtime = 0;
    static char buf[BUFERR_SIZE];

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = term;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    time(&rawtime);

    if ((logfile = getenv("SENDMAIL_LOG")) == NULL || !*logfile) {
        fprintf(stderr, "SENDMAIL_LOG env not set\n");
        return 1;
    }

    if ((fout = fopen(logfile, "a+t")) == NULL) {
        perror("sendmail fopen error");
        return 2;
    }

    if (getenv("SENDMAIL_LOCK_EX") != NULL) {
        /* it's a good idea to get the lock before writing */
        if ((locked = flock(fileno(fout), LOCK_EX)) != 0) {
            perror("sendmail flock error");
            fclose(fout);
            return 2;
        }
    }

    memset(buf, 0, sizeof(buf));
    if ((timeinfo = localtime(&rawtime)) != NULL)
        strftime(buf, sizeof(buf) - 1, "%c", timeinfo);

    fprintf(fout, "\r\nX-Sendmail-Proc: %d:%d %d %d",
        getuid(), getgid(), getppid(), getpid());
    for (narg = 0; narg < argc; narg++) {
        fprintf(fout, " %s", argv[narg]);
    }
    fprintf(fout, "\r\nX-Sendmail-Date: %s\r\n", buf);

    while (!sigcaught)
    {
        nread = fread(buf, 1, sizeof(buf), stdin);

        /* safe-read, retry if EINTR occurred */
        if (nread <= 0 && errno == EINTR && ++nretries < MAX_RETRIES)
            continue;

        if (nread <= 0)
            break;

        nwrite = fwrite(buf, 1, nread, fout);

        if (nwrite != nread) {
            perror("sendmail fwrite error");
            break;
        }
    }

    fprintf(fout, "\r\n");

    if (locked == 0)
        flock(fileno(fout), LOCK_UN);

    fclose(fout);

    if (nread > 0 && nwrite != nread)
        return 3;

    return 0;
}