#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

#define BUFSIZE 32

void    nop         (int signum){}
void    die         (int signum);
int     receive     (pid_t pid, void* buf, size_t count){}
int     send        (pid_t pid, void* buf, size_t count){}
int     err_printf  (char* format, ...);

#define SIGACTION( signum, foo )                                        \
do                                                                      \
{                                                                       \
    act.sa_handler = foo;                                               \
    if (sigaction (signum, &act, NULL) == -1)                           \
        return err_printf ("can't change disposition of"#signum"\n");   \
} while(0)

int main (int argc, char *argv[])
{
    if (argc < 2)
        return err_printf ("Enter argument\n");

    if (argc > 2)
        return err_printf ("Too many arguments\n");

    pid_t pid   = -1;
    pid_t ppid  = getpid();

    struct sigaction act = {};


    SIGACTION (SIGCHLD, die);

    sigset_t set = {};
    if (sigemptyset (&set)          == -1 ||
        sigaddset   (&set, SIGUSR1) == -1 ||
        sigaddset   (&set, SIGUSR2) == -1)
    {
        return err_printf ("can't create set of signals\n");
    }

    if((pid = fork ()) == -1)
        return err_printf ("can't fork new process\n");

    if (pid != 0)
    {
        SIGACTION (SIGUSR1, nop);
        SIGACTION (SIGUSR2, nop);

        int kek = sigtimedwait (&set, NULL, (const struct timespec[]){{0, 5 * 1e8L}});

        char    buf [BUFSIZE] = {};

        size_t  nbytes = -1;

        while (nbytes != 0)
        {
            if (receive (pid, &nbytes, sizeof (size_t)) == -1)
                return err_printf ("can't receive (&nbytes)\n");

            if (receive (pid, buf, nbytes) == -1)
                return err_printf ("can'r receive bits\n");

            if (write (STDOUT_FILENO, buf, nbytes) == -1)
                return err_printf ("can't write to stdout\n");
        }
    }

    if (pid == 0)
    {
        SIGACTION (SIGUSR1, nop);

        char    buf [BUFSIZE] = {};

        size_t  nbytes = -1;

        while (nbytes != 0)
        {
            if ((nbytes = read(fd, buf, BUFSIZE)) == -1)
               return err_printf ("can't read from file\n");

            if (send (ppid, &nbytes, sizeof (size_t)) == -1)
                return err_printf ("can't send (&nbytes)\n");

            if (send (ppid, buf, nbytes) == -1)
                return err_printf ("can't send bits\n");
        }
    }

    return 0;
}

#undef BUFSIZE
#undef SIGACTION

void die (int signum)
{
    errno = CHILD;
    err_printf ("EXIT_FAILURE\n");

    exit (EXIT_FAILURE);
}

#ifdef __GNUC__

    int err_printf (char *format, ...) __attribute__ ((format(printf, 1, 2)));

#endif /*__GNUC__*/

int err_printf (char* format, ...)
{
    int err_tmp = errno;

    va_list ap;
    va_start (ap, format);
    vfprintf (stderr, format, ap);
    va_end (ap);

    fprintf (stderr, "\t");

    errno = err_tmp;
    perror (NULL);

    return -1;
}
