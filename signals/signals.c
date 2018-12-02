#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

#define BUFSIZE 32

void    nop         (int signum) {}
void    catch_bit   (int signum);
void    die         (int signum);
int     receive     (pid_t pid, void* buf, size_t count);
int     send        (pid_t pid, void* buf, size_t count);
int     err_printf  (char* format, ...);

sigset_t    set = {};
char        symb = 0;

#define SIGACTION( signum, foo )                                        \
do                                                                      \
{                                                                       \
    act.sa_handler = foo;                                               \
    if (sigaction (signum, &act, NULL) == -1)                           \
        return err_printf ("can't change disposition of"#signum"\n");   \
} while(0)

#define SIGSUSPEND( set, time ) \
do                              \
{                               \
    alarm (time);               \
    sigsuspend (set);           \
    alarm (0);                  \
} while (0)

int main (int argc, char *argv[])
{
    if (argc != 2)
    {   
        errno = EINVAL;

        return err_printf ("Usage: %s {filename}\n", argv[0]);
    }

    pid_t pid   = -1;
    pid_t ppid  = getpid();

    struct sigaction act = {};

    SIGACTION (SIGCHLD, die);

    if (sigfillset (&set)           == -1 ||
        sigdelset   (&set, SIGINT)  == -1 ||
        sigdelset   (&set, SIGCHLD) == -1 ||
        sigdelset   (&set, SIGALRM) == -1)
    {
        return err_printf ("can't edit set\n");
    }

    sigprocmask(SIG_SETMASK, &set, NULL);

    if (sigdelset   (&set, SIGUSR1) == -1 ||
        sigdelset   (&set, SIGUSR2) == -1)
    {
        return err_printf ("can't edit set\n");
    }

    if((pid = fork ()) == -1)
        return err_printf ("can't fork new process\n");

    if (pid != 0)
    {
        SIGACTION (SIGUSR1, catch_bit);
        SIGACTION (SIGUSR2, catch_bit);

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

        SIGACTION (SIGCHLD, SIG_DFL);

        if (kill (pid, SIGUSR1) == -1)
            return err_printf ("EXIT_FAILURE\n");
    }

    if (pid == 0)
    {
        SIGACTION (SIGUSR1, nop);

        char    buf [BUFSIZE] = {};
        size_t  nbytes  = -1;
        int     fd      = -1;

        if ((fd = open (argv[1], O_RDONLY)) == -1)
            return err_printf ("can't open file %s\n", argv[1]);

        while (nbytes != 0)
        {
            if ((nbytes = read(fd, buf, BUFSIZE)) == -1)
               return err_printf ("can't read from file\n");

            if (send (ppid, &nbytes, sizeof (size_t)) == -1)
                return err_printf ("can't send (&nbytes)\n");

            if (send (ppid, buf, nbytes) == -1)
                return err_printf ("can't send bits\n");
        }

        SIGSUSPEND (&set, 1);
    }

    return 0;
}

void die (int signum)
{
    errno = ECHILD;
    err_printf ("EXIT_FAILURE\n");

    exit (EXIT_FAILURE);
}

void catch_bit (int signum)
{
    switch (signum)
    {
        case SIGUSR1:   {symb += 0; break;}

        case SIGUSR2:   {symb += 1; break;}
    }
}

int receive (pid_t pid, void* buf, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        symb = 0;

        for (int j = 0; j < 8; j++)
        {
            SIGSUSPEND (&set, 1);

            if (j != 7) {symb <<= 1;}

            if (kill (pid, SIGUSR1) == -1)
                return -1;
        }

        ((char *) buf) [i] = symb;
    }

    return count;
}

int send (pid_t pid, void* buf, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        symb = ((char *) buf) [i];

        for (int j = 0; j < 8; j++)
        {
            switch ((symb & (1 << 7)) >> 7)
            {
                case 0: {
                            if (kill (pid, SIGUSR1) == -1)
                                return -1;

                            break;
                        }

                case 1: {
                            if (kill (pid, SIGUSR2) == -1)
                                return -1;

                            break;
                        }
            }

            symb <<= 1;

            SIGSUSPEND (&set, 1);
        }
    }

    return count;
}

#undef BUFSIZE
#undef SIGACTION
#undef SIGSUSPEND

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
