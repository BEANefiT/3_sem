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

#define SEMKEY 24
#define SEMNUM 2
#define SHMKEY 23
#define SHMSIZE 32

int err_printf (char* format, ...);
int producer (char* pathname);
int consumer ();

int semid = -1;
int shmid = -1;
void* shmaddr = (void *) -1;
struct sembuf sops [SEMNUM] = {};

int main (int argc, char* argv[])
{
	int is_producer = (argc == 2);
	
	if (argc > 2)
		return err_printf ("Too many arguments\n");

	if ((semid = semget (SEMKEY, SEMNUM, IPC_CREAT | 0666)) == -1)
		return err_printf ("Can't create semaphore's set\n");

	if ((shmid = shmget (SHMKEY, SHMSIZE, IPC_CREAT | 0666)) == -1)
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

int producer (char* pathname)
{
	int counter = 0;

	SEMPUSH (1, 0, 0);
	SEMPUSH (0, 1, 0);
	semop (semid, sops, counter);
	counter = 0;

	int filefd = -1;

	if ((filefd = open (pathname, O_RDONLY)) == -1)
		return err_printf ("can't open %s\n", pathname);
}

int consumer ()
{
	int counter = 0;

	SEMPUSH (0, 0, 0);
	SEMPUSH (1, 1, 0);
	semop (semid, sops, counter);
	counter = 0;
}

#undef SEMPUSH

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
