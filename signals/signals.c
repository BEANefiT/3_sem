#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFSIZE 32

int     err_printf (char* format, ...);
void    catch_bit (int signum);
void    die (int signum);

int main (int argc, char *argv[])
{
    if (argc < 2)
        return err_printf ("Enter argument\n");

    if (argc > 2)
        return err_printf ("Too many arguments\n");

    pid_t pid   = -1;
    pid_t ppid  = getpid();

    struct sigaction act = {};

    act.sa_handler = die;
    if (sigaction (SIGCHLD, &act, NULL) == -1)
        return err_printf ("can't change disposition of SIGCHLD\n");

    act.sa_handler = catch_bit;
    if (sigaction (SIGUSR1, &act, NULL) == -1)
        return err_printf ("can't change disposition of SIGUSR1\n");

    act.sa_handler = catch_bit;
    if (sigaction (SIGUSR2, &act, NULL) == -1)
        return err_printf ("can't change disposition of SIGUSR2\n");

    sigset_t set = {};
    if (sigfillset (&set) == -1 ||
        sigdelset (&set, SIGCHLD) == -1 ||
        sigdelset (&set. SIGUSR1) == -1 ||
        sigdelset (&set, SIGUSR2) == -1 ||
        sigdelset (&set, SIGINT)  == -1 ||
        sigdelset (&set. SIGALRM) == -1)
    {
        return err_printf ("can't create set of signals\n");
    }

    if (sigprocmask (SIG_SETMASK, &set, NULL))
            return err_printf ("can't create mask\n");

    if((id = fork ()) == -1)
        return err_printf ("can't fork new process\n");

    if (id != 0)
    {
        char    buf [BUFSIZE] = {};

        int     nbytes = -1;

        while (nbytes != 0)
        {
            if (receive (&nbytes, sizeof (int)) == -1)
                return err_printf ("can't receive (&nbytes)\n");

            if (receive (buf, nbytes) == -1)
                return err_printf ("can'r receive bits\n");

            if (write (STDOUT_FILENO, buf, nbytes) == -1)
                return err_printf ("can't write to stdout\n");
        }
    }

    if (id == 0)
    {
        char    buf [BUFSIZE] = {};

        int     nbytes = -1;

        while (nbytes != 0)
        {
            if ((nbytes = read(fd, buf, BUFSIZE)) == -1)
               return err_printf ("can't read from file\n");

            if (send (ppid, &nbytes, sizeof (int)) == -1)
                return err_printf ("can't send (&nbytes)\n");

            if (send (ppid, buf, nbytes) == -1)
                return err_printf ("can't send bits\n");
        }
    }

    return 0;
}

#undef BUFSIZE

#ifdef __GNUC__

    int err_printf (char *format, ...) __attribute__ ((format(printf, 1, 2)));

#endif /*__GNUC__*/

int err_printf (char* format, ...)
{
    int err_tmp = errno;

    (shmid == -1) || shmctl (shmid, IPC_RMID, NULL);

    va_list ap;
    va_start (ap, format);
    vfprintf (stderr, format, ap);
    va_end (ap);

    printf (stderr, "\t");

    errno = err_tmp;
    perror (NULL);

    return -1;
}
