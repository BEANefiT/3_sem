#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>

#define BUF_SIZE 256

int reader (char* fifo_pathname);
int writer (char* fifo_pathname, char* target_pathname);
int err_printf (int first_fd, int second_fd, char* format, ...);

int main (int argc, char* argv[])
{
	char* fifo_pathname = "tmp_fifo";

	if ((mkfifo (fifo_pathname, 0664) == -1) && (errno != EEXIST))
	{
		printf ("can't create fifo\n");

		exit (EXIT_FAILURE);
	}

	int isreader = (argc == 1);
	
	if (isreader)
	{
		if (reader (fifo_pathname) == -1)
		{
			printf ("reader () error\n");

			exit (EXIT_FAILURE);
		}
	}	

	else
	{
		if (writer (fifo_pathname, argv[1]) == -1)
		{
			printf ("writer (%s) error\n", argv [1]);

			exit (EXIT_FAILURE);
		}
	}

	return 0;
}

int reader (char* fifo_pathname)
{
	int fifo_fd = -1;
	
	if ((fifo_fd = open (fifo_pathname, O_RDONLY)) == -1)
			return err_printf (fifo_fd, -1, "No such file or directory: \"%s\"\n", fifo_pathname);

	char buf [BUF_SIZE] = {};

	int read_result = -1;
	
	while (read_result != 0)
	{	
		if ((read_result = read (fifo_fd, &buf, BUF_SIZE)) == -1)
			return err_printf (fifo_fd, -1,
					   "err while reading from fifo\n");

		if (write (STDOUT_FILENO, &buf, read_result) == -1)
			return err_printf (fifo_fd, -1, "err while writing to stdout\n");
	}
		
	close (fifo_fd);

	return 0;
}

int writer (char* fifo_pathname, char* target_pathname)
{
	int fifo_fd 	= -1;
	int target_fd 	= -1;
	
	if ((target_fd = open (target_pathname, O_RDONLY)) == -1)
		return err_printf (fifo_fd, target_fd,
				   "No such file or directory: \"%s\"\n",
				   target_pathname);

	if ((fifo_fd = open (fifo_pathname, O_WRONLY)) == -1)
		return err_printf (fifo_fd, target_fd,
			   	   "No such file or directory: \"%s\"\n",
			   	   fifo_pathname);

	char buf [BUF_SIZE] = {};

	int read_result = -1;

	while (read_result != 0)
	{
		if ((read_result = read (target_fd, &buf, BUF_SIZE)) == -1)
			return err_printf (fifo_fd, target_fd,
			   		   "err while reading from file\n");
	
		if (write (fifo_fd, &buf, read_result) == -1)
			return err_printf (fifo_fd, target_fd,
					   "err while writing to fifo\n");
	}

	close (fifo_fd);
	close (target_fd);

	return 0;
}

int err_printf (int first_fd, int second_fd, char* format, ...)
{
	int err_tmp = errno;

	(first_fd == -1)  || close (first_fd);
	(second_fd == -1) || close (second_fd);

	va_list ap;
	va_start (ap, format);
	
	vprintf (format, ap);

	va_end (ap);

	errno = err_tmp;

	return -1;
}
