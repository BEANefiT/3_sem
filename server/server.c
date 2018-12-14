#include <sys/select.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef struct pair_t
{
    int rfd;
    int wfd;
} pair_t;

typedef struct buf_t
{
    void*   buf;
    size_t  buf_sz;
    int     nbytes;
} buf_t;

int     err_printf  (char* format, ...);
int     get_int     (char *str);
int     child       (int idx);
int     parent      (int N);
int     piperd      (int i);
int     pipewr      (int i);
buf_t*  bufs  = NULL;
pair_t* pairs = NULL;

#define WAIT_RET( count )           \
do {                                \
    for (int j = 0; j < count; j++) \
        wait (NULL);                \
                                    \
    return -1;                      \
} while(0)

#define MIN( a,b ) (((a) < (b)) ? (a) : (b))

#define CLOSE( fd ) \
do {                \
    int tmpfd = fd; \
    fd = -1;        \
    close (tmpfd);  \
} while(0)

int main (int argc, char *argv[])
{
    if (argc != 3)
    {
        errno = EINVAL;
        return err_printf ("Usage: %s {child_num} {filename}\n", argv[0]);
    }

    int N = -1;
    
    if ((N = get_int (argv[1])) == -1)
        errno = EINVAL;

    if (errno != 0)
        return err_printf ("Bad child_num\n");

    if ((pairs = (pair_t *) calloc (2 * N, sizeof (pair_t))) == NULL)
        return err_printf ("can't alloc memory (pair_t[sz = 2 * \'%d\'])\n", N);

    pid_t   id = 0;
    int     i = 0;

    if ((pairs[0].rfd = open (argv[2], O_RDONLY)) == -1)
        return err_printf ("cannot open file\n");

    pairs[2 * N - 1].wfd = STDOUT_FILENO;

    for (i = 0; i < N; i++)
    {
        int pipefd1 [2];
        int pipefd2 [2];

        if ((pipe (pipefd1) == -1) || (pipe (pipefd2) == -1))
        {
            err_printf ("Can't create pipe, iter = %d\n", i);

            WAIT_RET (i - 1);
        }

        if (i != 0)
        {
            pairs[2 * i - 1].wfd    = pipefd1[1];
            pairs[2 * i].rfd        = pipefd1[0];
        }

        if (i == 0)
        {
            close (pipefd1[0]);
            close (pipefd1[1]);
        }

        pairs[2 * i].wfd        = pipefd2[1];
        pairs[2 * i + 1].rfd    = pipefd2[0];

        if((id = fork ()) == -1)
        {
            err_printf ("Can't fork new process; iter = %d\n", i);

            WAIT_RET (i - 1);
        }

        if (id == 0)
        {
            if (child (2 * i) == -1)
                return err_printf ("child #%d error\n", i);

            return 0;
        }
    }

    if (parent (N) == -1)
        return err_printf ("parent error\n");

    for (int j = 0; j < N; j++)
        wait (NULL);

    return 0;
}

int child (int idx)
{
    for (int i = 0; i < idx / 2; i++)
    {
        CLOSE (pairs[2 * i].wfd);
        CLOSE (pairs[2 * i].rfd);
        CLOSE (pairs[2 * i + 1].wfd);
        CLOSE (pairs[2 * i + 1].rfd);
    }

    CLOSE (pairs[idx + 1].rfd);

    char buf[4096] = {};

    int read_result = -1;

    while (read_result != 0)
    {
        if ((read_result = read (pairs[idx].rfd, buf, 4096)) == -1)
            return err_printf ("cannot read from rfd\n");

        if (write (pairs[idx].wfd, buf, read_result) == -1)
            return err_printf ("cannot write to wfd\n");
    }

    CLOSE (pairs[idx].wfd);
    CLOSE (pairs[idx].rfd);

    return 0;
}

int parent (int N)
{
    for (int i = 0; i < N; i++)
    {
        fcntl (pairs[2 * i + 1].rfd, F_SETFL, O_RDONLY | O_NONBLOCK);
        fcntl (pairs[2 * i + 1].wfd, F_SETFL, O_WRONLY | O_NONBLOCK);

        CLOSE (pairs[2 * i].wfd);
        CLOSE (pairs[2 * i].rfd);
    }

    if ((bufs = (buf_t *) calloc (N, sizeof (buf_t))) == NULL)
        return err_printf ("cannot create buf_ptrs array\n");

    for (int i = 0; i < N; i++)
    {
        bufs[i].buf_sz = MIN (1024 * pow (3, N - i), 128 << 10);

        if ((bufs[i].buf = calloc (bufs[i].buf_sz, 1)) == NULL)
            return err_printf ("cannot allocate %zd buf\n", bufs[i].buf_sz);
    }

    while(1)
    {
        int nfds = 0;

        fd_set  rfds, wfds;
        FD_ZERO (&rfds);
        FD_ZERO (&wfds);

        for (int i = 0; i < N; i++)
        {
            int idx = 2 * i + 1;

            if (pairs[idx].rfd > nfds) nfds = pairs[idx].rfd;

            if (pairs[idx].wfd > nfds) nfds = pairs[idx].wfd;

            if (pairs[idx].rfd != -1) FD_SET (pairs[idx].rfd, &rfds);

            if (pairs[idx].wfd != -1) FD_SET (pairs[idx].wfd, &wfds);
        }

        if (!nfds) break;

        if (select (nfds + 1, &rfds, &wfds, NULL, NULL) == -1)
            return err_printf ("select error\n");

        for (int i = 0; i < N; i++)
        {
            int idx = 2 * i + 1;

            if (FD_ISSET (pairs[idx].rfd, &rfds))
            {
                if (piperd (i) == -1)
                    return err_printf ("piperd error\n");
            }

            if (FD_ISSET (pairs[idx].wfd, &wfds))
            {
                if (pipewr (i) == -1)
                    return err_printf ("pipewr error\n");
            }
        }
    }
}

int piperd (int i)
{
    int idx = 2 * i + 1;

    int read_result = read (pairs[idx].rfd, bufs[i].buf, bufs[i].buf_sz);
    
    bufs[i].nbytes += read_result;

    if (read_result == -1)
        return -1;

    if (read_result == 0)
        CLOSE (pairs[idx].rfd);

    return 0;
}

int pipewr (int i)
{
    int idx = 2 * i + 1;

    int write_result = 0;

    if (bufs[i].nbytes > 0)
        write_result = write (pairs[idx].wfd, bufs[i].buf, bufs[i].nbytes);

    if (write_result == -1)
        return -1;

    bufs[i].nbytes -= write_result;

    if ((bufs[i].nbytes == 0) && (pairs[idx].rfd == -1))
        CLOSE (pairs[idx].wfd);

    return 0;
}

int get_int (char *str)
{
    char *_endptr;
    errno = 0;

    int arg = (int) strtol (str, &_endptr, 10);

    if (*_endptr != '\0' && *str != '\0')
    {
        errno = EINVAL;
        return -1;
    }

    if (errno == ERANGE)
    {
        return -1;
    }

    return arg;
}

#ifdef __GNUC__

    int err_printf (char *format, ...) __attribute__ ((format(printf, 1, 2)));

#endif /*__GNUC__*/

int err_printf (char* format, ...)
{
    perror (NULL);
    fprintf (stderr, "\t");

    va_list ap;
    va_start (ap, format);
    vfprintf (stderr, format, ap);
    va_end (ap);

    return -1;
}
