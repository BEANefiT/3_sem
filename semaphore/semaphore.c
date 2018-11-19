#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <stdio.h>
#include <sys/shm.h>
#include <stdarg.h>

#define SEMKEY 24
#define SHMKEY 23
#define SHMSIZE 32

int err_printf (int semid, int shmid, char* format, ...);

int main (int argc, char* argv[])
{
	int is_producer = (argc == 2);
	
	if (argc > 2)
		return err_printf (0, 0, "Too many arguments\n");

	int semid = -1;
	int shmid = -1;

	if ((semid = semget (SEMKEY, 2, IPC_CREAT | 0666)) == -1)
		return err_printf (0, 0, "Can't create semaphore's set\n");

	if ((shmid = shmget (SHMKEY, SHMSIZE, IPC_CREAT | 0666)) == -1)
		return err_printf (semid, 0, "Can't create shared memory\n");

	semctl (semid, 0, IPC_RMID, NULL);
	shmctl (shmid, IPC_RMID, NULL);
	
	return 0;
}

int err_printf (int semid, int shmid, char* format, ...)
{
	int err_tmp = errno;

	(semid == 0) || semctl (semid, 0, IPC_RMID, NULL);
	(shmid == 0) || shmctl (shmid, IPC_RMID, NULL);

	va_list ap;
	va_start (ap, format);
	vfprintf (stderr, format, ap);
	va_end (ap);

	errno = err_tmp;

	perror (NULL);

	return -1;
}
