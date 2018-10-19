#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>

#define BUF_SIZE 32
#define FIFO_NAME_SIZE 10
#define MAX_TIME 7

int reader (char* names_pathname);
int writer (char* names_pathname, char* target_pathname);
int err_printf (char* fifo_pathname, int first_fd, int second_fd, int third_fd, char* format, ...);

int main (int argc, char* argv[])
{
	char* names_pathname = "names_fifo";

	if ((mkfifo (names_pathname, 0644) == -1) && (errno != EEXIST))
	{
		printf ("can't create fifo\n");

		exit (EXIT_FAILURE);
	}

	int isreader = (argc == 1);
	
	if (isreader)
	{
		if (reader (names_pathname) == -1)
		{
			printf ("reader () error\n");

			exit (EXIT_FAILURE);
		}
	}	

	else
	{
		if (writer (names_pathname, argv[1]) == -1)
		{
			printf ("writer (%s) error\n", argv [1]);

			exit (EXIT_FAILURE);
		}
	}

	return 0;
}

int reader (char* names_pathname)
{
	char fifo_pathname [FIFO_NAME_SIZE];
	int names_fd	= -1;
	int fifo_fd	= -1;

	pid_t pid = getpid();

	sprintf (fifo_pathname, "f_%d", pid);

	if ((names_fd = open (names_pathname, O_RDWR)) == -1)
		return err_printf (NULL, fifo_fd, names_fd, -1,
				   "No such file or directory: \"%s\"\n",
				   names_pathname);

	if ((mkfifo (fifo_pathname, 0644) == -1) && (errno != EEXIST))
		return err_printf (NULL, fifo_fd, names_fd, -1,
				   "can't create %s\n", fifo_pathname);

	if (write (names_fd, &fifo_pathname, FIFO_NAME_SIZE) == -1)
		return err_printf (fifo_pathname, fifo_fd, names_fd, -1,
				   "err while writing to names_fifo\n");
	
	char buf [BUF_SIZE] = {};

	int read_result = 0;
	
	int time_until_death = MAX_TIME;

	if ((fifo_fd = open (fifo_pathname, O_RDONLY | O_NONBLOCK)) == -1)
		return err_printf (fifo_pathname, fifo_fd, names_fd, -1,
				   "No such file or directory :\"%s\"\n",
				   fifo_pathname);

	while (read_result == 0)
	{
		if ((time_until_death -= 1) <= 0)
		{
			return err_printf (fifo_pathname, fifo_fd, names_fd, -1,
				   "Waiting for writer for too long\n");
		}

		if ((read_result = read (fifo_fd, &buf, BUF_SIZE)) == -1)
			return err_printf (fifo_pathname, fifo_fd, -1, names_fd,
					   "err while reading from fifo\n");

		if (write (STDOUT_FILENO, &buf, read_result) == -1)
			return err_printf (fifo_pathname, fifo_fd, -1, names_fd, 
					   "err while writing to stdout\n");

		if (read_result)
			break;

		sleep (1);
	}

	close (names_fd);

	read_result = -1;

	while (read_result != 0)
	{	
		if ((read_result = read (fifo_fd, &buf, BUF_SIZE)) == -1)
			return err_printf (fifo_pathname, fifo_fd, -1, names_fd,
					   "err while reading from fifo\n");

		if (write (STDOUT_FILENO, &buf, read_result) == -1)
			return err_printf (fifo_pathname, fifo_fd, -1, names_fd, 
					   "err while writing to stdout\n");
	}
		
	close (fifo_fd);
	unlink (fifo_pathname);

	return 0;
}

int writer (char* names_pathname, char* target_pathname)
{
	char fifo_pathname [FIFO_NAME_SIZE] = {};
	int fifo_fd 	= -1;
	int names_fd	= -1;
	int target_fd 	= -1;
	
	if ((names_fd = open (names_pathname, O_RDWR)) == -1)
		return err_printf (NULL, fifo_fd, names_fd, target_fd,
				   "No such file or directory: \"%s\"\n",
				   names_pathname);

	int read_result = read (names_fd, &fifo_pathname, FIFO_NAME_SIZE);

	if (read_result == -1)
		return err_printf (NULL, fifo_fd, names_fd, target_fd,
		   		   "err while reading from names_fifo\n");

	close (names_fd);

	fifo_pathname [read_result] = 0;

	if ((fifo_fd = open (fifo_pathname, O_WRONLY | O_NONBLOCK)) == -1)
	{
		if (errno == ENXIO)
			return err_printf (fifo_pathname, fifo_fd, names_fd, target_fd,
					   "No reader found, errno = ENXIO\n");

		return err_printf (fifo_pathname, fifo_fd, names_fd, target_fd,
				   "No such file or directory: \"%s\"\n",
				   fifo_pathname);
	}

	if ((target_fd = open (target_pathname, O_RDONLY)) == -1)
		return err_printf (fifo_pathname, fifo_fd, target_fd, names_fd,
				   "No such file or directory: \"%s\"\n",
				   target_pathname);

	char buf [BUF_SIZE] = {};

	read_result = -1;

	while (read_result != 0)
	{
		if ((read_result = read (target_fd, &buf, BUF_SIZE)) == -1)
			return err_printf (fifo_pathname, fifo_fd, target_fd, names_fd,
			   		   "err while reading from file\n");
	
		if (write (fifo_fd, &buf, read_result) == -1)
			return err_printf (fifo_pathname, fifo_fd, target_fd, names_fd,
					   "err while writing to %s\n",
					   fifo_pathname);
	}

	close (fifo_fd);
	close (target_fd);

	return 0;
}

int err_printf (char* fifo_pathname, int first_fd, int second_fd, int third_fd, char* format, ...)
{
	int err_tmp = errno;

	(!fifo_pathname)  || unlink (fifo_pathname);
	(first_fd == -1)  || close (first_fd);
	(second_fd == -1) || close (second_fd);
	(third_fd == -1)  || close (third_fd);

	va_list ap;
	va_start (ap, format);
	
	vprintf (format, ap);

	va_end (ap);

	errno = err_tmp;

	return -1;
}
