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
#define MAXTIME 200

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

	if ((shmid = shmget (SHMKEY, SHMSIZE + 2, IPC_CREAT | 0666)) == -1)
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

	semctl (semid, 0, IPC_RMID, NULL);
	shmctl (shmid, IPC_RMID, NULL);
	
	return 0;
}

#define SEMPUSH( num, op, flg )		\
do					\
{					\
	sops[counter].sem_num = num;	\
	sops[counter].sem_op  = op;	\
	sops[counter++].sem_flg = flg;	\
} while(0)

#define SEMOP_NORET()					\
do							\
{							\
	semop (semid, sops, counter);			\
	counter = 0;					\
} while(0)

#define SEMOP()						\
do							\
{							\
	if (semop (semid, sops, counter) == -1)		\
		return err_printf ("semop error\n");	\
							\
	counter = 0;					\
} while(0)

#define SEMWAIT( num, val )		\
do					\
{					\
	SEMPUSH (num, -1 * val, 0);	\
	SEMPUSH (num, 0, 0);		\
	SEMPUSH (num, val, 0);		\
	SEMPUSH (num, 2, 0);		\
	SEMOP();			\
} while(0)

#define SEMCHECK( num, val )			\
({						\
	SEMPUSH (num, -1 * val, IPC_NOWAIT);	\
	SEMPUSH (num, 0, IPC_NOWAIT);		\
	SEMPUSH (num, val, IPC_NOWAIT);		\
						\
	int res = semop (semid, sops, counter);	\
	counter = 0;				\
	res;					\
})

#define SHMCHECK()					\
do							\
{							\
	if (BYTES_WRITTEN == BYTES_READ)		\
	{						\
		if (SEMCHECK (1, 0) == 0)		\
		{					\
			SEMPUSH (1, 1, 0);		\
			SEMOP();			\
		}					\
	}						\
							\
	else						\
	{						\
		SEMPUSH (1, -1, IPC_NOWAIT);		\
		SEMOP_NORET();				\
	}						\
							\
	if ((BYTES_WRITTEN - BYTES_READ) >= SHMSIZE)	\
	{						\
		if (SEMCHECK (2, 0) == 0)		\
		{					\
			SEMPUSH (2, 1, 0);		\
			SEMOP();			\
		}					\
	}						\
							\
	else						\
	{						\
		SEMPUSH (2, -1, IPC_NOWAIT);		\
		SEMOP_NORET();				\
	}						\
							\
} while(0)

#define BYTES_WRITTEN ((int *)shmaddr)[0]
#define BYTES_READ ((int *)shmaddr)[1]

int producer (char* pathname)
{
	int counter = 0;

	SEMPUSH (0, 0, 0);
	SEMPUSH (0, 1, 0);
	SEMOP();

	BYTES_WRITTEN	= 0;
	BYTES_READ	= 0;

	int filefd = -1;

	if ((filefd = open (pathname, O_RDONLY)) == -1)
		return err_printf ("can't open %s\n", pathname);

	int read_result = -1;
	
	while (read_result != 0)
	{
		SHMCHECK();
		
		SEMPUSH (2, 0, 0);
		SEMOP();

		void* buf = shmaddr + 8 + BYTES_WRITTEN % SHMSIZE;

		if ((read_result = read (filefd, buf, SHMSIZE - (BYTES_WRITTEN - BYTES_READ))) == -1)
			return err_printf ("err while read() from filen\n");

		BYTES_WRITTEN += read_result;
	}

	SEMPUSH (0, -1, 0);
	SEMOP();

	return 0;
}

int consumer ()
{
	int counter = 0;

	SEMWAIT (0, 1);

	int time_until_death = MAXTIME;

	while (!(BYTES_WRITTEN || BYTES_READ))
	{
		if ((time_until_death -= 1) <= 0)
			return err_printf ("waiting for producer for too long\n");

		nanosleep((const struct timespec[]){{0, 100*1e6L}}, NULL);
	}

	int write_result = -1;

	while (write_result != 0)
	{
		if (SEMCHECK (0, 2) == 0)
		{
			if (BYTES_READ == BYTES_WRITTEN)
				break;
		}

		else
		{
			SHMCHECK();
			SEMPUSH (1, 0, 0);
			SEMOP();
		}

		void* buf = shmaddr + 8 + BYTES_READ % SHMSIZE;

		if ((write_result = write (STDOUT_FILENO, buf, BYTES_WRITTEN - BYTES_READ)) == -1)
			return err_printf ("err while write() to stdout\n");

		BYTES_READ += write_result;
	}

	return 0;
}

#undef BYTES_WRITTEN
#undef BYTES_READ

#undef SEMWAIT
#undef SEMOP
#undef SEMOP_NORET
#undef SEMPUSH
#undef SEMCHECK
#undef SHMCHECK

#ifdef __GNUC__
	int err_printf (char *format, ...) __attribute__ ((format(printf, 1, 2)));
#else
	int err_printf (char *format, ...);
#endif /*__GNUC__*/

int err_printf (char* format, ...)
{
	int err_tmp = errno;

	(semid == -1) || semctl (semid, 0, IPC_RMID, NULL);
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
