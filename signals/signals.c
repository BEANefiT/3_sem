#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/wait.h>

int err_printf (char* format, ...);
void catch_bit (int signum);

int main (int argc, char *argv[])
{
    if (argc < 2)
        return err_printf ("Enter argument\n");

    if (argc > 2)
        return err_printf ("Too many arguments\n");

    pid_t id = -1;

    struct sigaction act = {};

    act.sa_handler = die;
    if (sigaction (SIGCHLD, &act, NULL) == -1)
        return err_printf ("can't change disposition of SIGCHLD\n\t");

    act.sa_handler = catch_bit;
    if (sigaction (SIGUSR1, &act, NULL) == -1)
        return err_printf ("can't change disposition of SIGUSR1\n\t");

    act.sa_handler = catch_bit;
    if (sigaction (SIGUSR2, &act, NULL) == -1)
        return err_printf ("can't change disposition of SIGUSR2\n\t");

    sigset_t set = {};
    if (sigfillset (&set) == -1 ||
        sigdelset (&set, SIGCHLD) == -1 ||
        sigdelset (&set. SIGUSR1) == -1 ||
        sigdelset (&set, SIGUSR2) == -1 ||
        sigdelset (&set, SIGINT)  == -1)
    {
        return err_printf ("can't create set of signals\n\t");
    }

    if (sigprocmask (SIG_SETMASK, &set, NULL)
            return err_printf ("can't create mask\n\t");

    if((id = fork ()) == -1)
        return err_printf ("can't fork new process\n\t");

    if (id != 0)
    {
        
    }

    if (id == 0)
    {

    }

    return 0;
}

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

    errno = err_tmp;

    perror (NULL);

    return -1;
}
