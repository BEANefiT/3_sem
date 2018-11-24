#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <stdio.h>
#include <sys/shm.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define SEMKEY 21
#define SEMNUM 3
#define NSOPS 4
#define SHMKEY 22
#define SHMSIZE 32

int err_printf (char* format, ...);
int producer (char* pathname);
int consumer ();

int semid = -1;
int shmid = -1;
void* shmaddr = (void *) -1;
struct sembuf sops [NSOPS] = {};

int main (int argc, char* argv[])
{
    int is_producer = (argc == 2);

    if (argc > 2)
        return err_printf ("Too many arguments\n");

    if ((semid = semget (SEMKEY, SEMNUM, IPC_CREAT | 0666)) == -1)
        return err_printf ("Can't create semaphore's set\n");

    if ((shmid = shmget (SHMKEY, SHMSIZE + 8, IPC_CREAT | 0666)) == -1)
        return err_printf ("Can't create shared memory\n");

    if ((shmaddr = shmat (shmid, NULL, 0)) == (void *) -1)
        return err_printf ("Can't attach shared memory\n");

    if (is_producer)
    {
        if (producer (argv[1]) == -1)
        {
            fprintf (stderr, "producer error\n");
            exit (EXIT_FAILURE);
        }
    }

    else
    {
        if (consumer () == -1)
        {
            fprintf (stderr, "consumer error\n");
            exit (EXIT_FAILURE);
        }
    }

    shmctl (shmid, IPC_RMID, NULL);

    return 0;
}

#define BYTES_WRITTEN ((int *)shmaddr)[0]
#define BYTES_READ ((int *)shmaddr)[1]
#define SHMRD 1
#define SHMWR 2

#define SEMPUSH( num, op, flg )     \
do                                  \
{                                   \
    sops[counter].sem_num = num;    \
    sops[counter].sem_op  = op;     \
    sops[counter++].sem_flg = flg;  \
} while(0)

#define SEMOP_NORET()               \
do                                  \
{                                   \
    semop (semid, sops, counter);   \
    counter = 0;                    \
} while(0)

#define SEMOP()                                 \
do                                              \
{                                               \
    if (semop (semid, sops, counter) == -1)     \
        return err_printf ("semop error\n");    \
                                                \
    counter = 0;                                \
} while(0)

#define SEMCHECK( num, val )                \
({                                          \
    SEMPUSH (num, -1 * val, IPC_NOWAIT);    \
    SEMPUSH (num, 0, IPC_NOWAIT);           \
    SEMPUSH (num, val, IPC_NOWAIT);         \
                                            \
    int res = semop (semid, sops, counter); \
    counter = 0;                            \
    res;                                    \
})

#define SHMCHECK( role )                                            \
do                                                                  \
{                                                                   \
    if (SEMCHECK (0, 2) == 0)                                       \
    {                                                               \
        errno = ECANCELED;                                          \
                                                                    \
        switch (role)                                               \
        {                                                           \
            case SHMRD:                                             \
            {                                                       \
                return err_printf ("producer is dead:(\n");         \
            }                                                       \
                                                                    \
            case SHMWR:                                             \
            {                                                       \
                return err_printf ("consumer is dead:(\n");         \
            }                                                       \
        }                                                           \
                                                                    \
        return -1;                                                  \
    }                                                               \
                                                                    \
    if ((SEMCHECK (0, 5) == 0) && (BYTES_WRITTEN == BYTES_READ))    \
    {                                                               \
        SEMPUSH (0, -4, 0);                                         \
        SEMPUSH (SHMWR, 1, 0);                                      \
        SEMOP();                                                    \
                                                                    \
        return 0;                                                   \
    }                                                               \
                                                                    \
} while(0)

/*
    ALGORITHM:

    switch (MAIN SEMAPHORE VALUE)
    {
        case 0: no pair producer + consumer

        case 1: producer has access to shm and waits for consumer

        case 2: producer or consumer is dead

        case 4: there is a pair producer + consumer

        case 5: producer done and exited, consumer is still obtaining from shm
    }
*/

int producer (char* pathname)
{
    int counter = 0;

    SEMPUSH (0, 0, 0);
    SEMPUSH (0, 2, SEM_UNDO);
    SEMPUSH (0, -1, 0);
    SEMOP();            //producers enters if there is no other producer/consumer

    SEMPUSH (0, -4, 0);
    SEMPUSH (0, 0, 0);
    SEMPUSH (0, 4, 0);
    SEMOP();            //producers waits for consumer

    BYTES_WRITTEN   = 0;
    BYTES_READ  = 0;
    SEMPUSH (SHMWR, -1, IPC_NOWAIT);
    SEMOP_NORET();

    int filefd = -1;

    if ((filefd = open (pathname, O_RDONLY)) == -1)
        return err_printf ("can't open %s\n", pathname);

    int read_result = -1;

    while (1)
    {
        SHMCHECK (SHMWR);
        SEMPUSH (SHMWR, 0, 0);
        SEMPUSH (SHMWR, 1, 0);
        SEMPUSH (SHMRD, 1, SEM_UNDO);
        SEMPUSH (SHMRD, -1, 0);
        SEMOP();

        void* buf = shmaddr + 8 + BYTES_WRITTEN % SHMSIZE;

        int size_available = SHMSIZE - (BYTES_WRITTEN - BYTES_READ);

        if ((read_result = read (filefd, buf, size_available)) == -1)
            return err_printf ("err while read() from filen\n");

        if ((size_available != 0) && (read_result == 0))
        {
            SEMPUSH (SHMRD, 1, 0);
            SEMOP();
            break;
        }

        BYTES_WRITTEN += read_result;

        SEMPUSH (SHMRD, 1, 0);
        SEMPUSH (SHMRD, -1, SEM_UNDO);
        SEMPUSH (SHMRD, -1, 0);
        SEMOP();
    }

    SEMPUSH (0, 3, 0);
    SEMPUSH (0, -2, SEM_UNDO);
    SEMOP();            //successful exit, main semaphore value = 5
                    //if producer dies before this time, main_sem_val = 2
    SEMPUSH (SHMRD, -1, IPC_NOWAIT);
    SEMOP_NORET();

    return 0;
}

int consumer ()
{
    int counter = 0;

    SEMPUSH (0, -1, 0);
    SEMPUSH (0, 0, 0);
    SEMPUSH (0, 2, 0);
    SEMPUSH (0, 2, SEM_UNDO);
    SEMOP();            //consumer waits for free producer

    int write_result = -1;

    while (1)
    {
        SHMCHECK (SHMRD);
        SEMPUSH (SHMRD, 0, 0);
        SEMPUSH (SHMRD, 1, 0);
        SEMPUSH (SHMWR, 1, SEM_UNDO);
        SEMPUSH (SHMWR, -1, 0);
        SEMOP();

        void* buf = shmaddr + 8 + BYTES_READ % SHMSIZE;

        int size_available = BYTES_WRITTEN - BYTES_READ;

        if ((write_result = write (STDOUT_FILENO, buf, size_available)) == -1)
            return err_printf ("err while write() to stdout\n");

        BYTES_READ += write_result;

        SEMPUSH (SHMWR, 1, 0);
        SEMPUSH (SHMWR, -1, SEM_UNDO);
        SEMPUSH (SHMWR, -1, 0);
        SEMOP();
    }
}

#undef BYTES_WRITTEN
#undef BYTES_READ
#undef SHMRD
#undef SHMWR

#undef SEMOP
#undef SEMOP_NORET
#undef SEMPUSH
#undef SEMCHECK
#undef SHMCHECK

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

#undef SEMKEY
#undef SEMNUM
#undef SHMKEY
#undef SHMSIZE
