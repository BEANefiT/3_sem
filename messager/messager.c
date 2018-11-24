#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdarg.h>

struct msgbuf
{
    long mtype;
};

int err_printf (int msgqid, char* format, ...);
long    get_int (char *str);
int send_msg (int msgqid, long mtype);
int get_msg(int msgqid, long mtype);

int main (int argc, char *argv[])
{
    if (argc < 2)
        return err_printf (0, "Enter argument\n");

    if (argc > 2)
        return err_printf (0, "Too many arguments\n");

    long    counter = get_int (argv [1]);
    pid_t   id = -1;
    int     msgqid = -1;
    int critqid = -1;
    long    N = 0;

    if ((msgqid = msgget(IPC_PRIVATE, IPC_CREAT | 0644)) == -1)
    {
        return err_printf (0, "Can't create msg queue\n");
    }

    for (long i = 0; i < counter; i++)
    {
        if (id != 0)
        {
            if((id = fork ()) == -1)
            {
                err_printf (msgqid, "Can't fork new process; iter = %d\n", i);

                for (long j = 0; j < i - 1; j++)
                    wait (NULL);

                return -1;
            }
        }

        if (id == 0)
        {
            N = i + 1;

            break;
        }
    }

    if (id != 0)
    {
        if (send_msg (msgqid, 1) == -1)
            return err_printf (msgqid, "Parent can't send start msg\n");
    }

    if (id == 0)
    {
        if (get_msg (msgqid, N) == -1)
            return err_printf (msgqid, "child #%d can't get msg\n", N);

        printf ("%d ", N);

        fflush (stdout);

        if (send_msg (msgqid, N + 1) == -1)
            return err_printf (msgqid, "child #%d can't send msg\n", N);
    }

    if (id != 0)
    {
        for (long i = 0; i < counter; i++)
            wait (NULL);

        msgctl (msgqid, IPC_RMID, NULL);
        
        printf ("\n");
    }

    return 0;
}

int send_msg (int msgqid, long mtype)
{
    struct msgbuf message;

    message.mtype = mtype;

    return msgsnd(msgqid, (void*) &message, 0, 0);
}

int get_msg (int msgqid, long mtype)
{
    struct msgbuf message = {};

    int result = -1;

    result = msgrcv (msgqid, (void*) &message, 0, mtype, 0);
}

long get_int (char *str)
{
    char *_endptr;

    long arg = strtol (str, &_endptr, 10);

    if (*_endptr != '\0' && *str != '\0')
    {
        printf ("Bad argument\n");

        exit (2);
    }

    if (errno == ERANGE)
    {
        printf ("Overflow\n");

        exit (3);
    }

    return arg;
}

int err_printf (int msgqid, char* format, ...)
{
    int err_tmp = errno;

    msgctl (msgqid, IPC_RMID, NULL);

    va_list ap;
    va_start (ap, format);
    vprintf (format, ap);
    va_end (ap);

    errno = err_tmp;

    perror (NULL);

    return -1;
}
