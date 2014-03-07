/* CMPSC 473, Project 1
 * Author: Yuan-Hsin Chen yuc200@psu.edu yuc200
 *
 * Sample program for a pipe
 *
 * See http://www.cse.psu.edu/~dheller/cmpsc311/Lectures/Interprocess-Communication.html
 * and http://www.cse.psu.edu/~dheller/cmpsc311/Lectures/Files-Directories.html
 * for some more information, examples, and references to the CMPSC 311 textbooks.
 */

//--------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "pthread_wrappers.h"
#include <semaphore.h>
#include "sem_wrappers.h"
/* This makes Solaris and Linux happy about waitpid(); it is not required on Mac OS X.*/
#include <sys/wait.h>

extern char *optarg; /* contains the next element of argv */

/* struct match is a linked-list of statistical data of matche pattern */
struct match{
	char *pattern; /* match pattern */
	int match_line; /* number of line which each has at least a match */
	int matches; /* how many matches in a file  */
	struct match *next;
};

void err_sys(char *msg); /* print message and quit */

/* In a pipe, the parent is upstream, and the child is downstream, connected by */
/* a pair of open file descriptors. */

static void p1_actions(char *ifile, int fd); /* write to fd */
static void p2_actions(struct match *m, int mnum, int fd, int fd1); /* read from fd, write to fd1 */
static void p3_actions(char *ofile, int fd); /* read from fd1 */
void createm(struct match **m, int *mnum, char *pattern);
void deletem(struct match *m);

#define BUFFER_SIZE 4096
#define LINE_ENTRY 5

int main(int argc, char *argv[])
{
	int fd[2], fd1[2]; /* pipe endpoints [0]:read [1]:write */
	pid_t child_pid;
	pid_t gchild_pid;
	int c;
	char *ifile = "/dev/null";
	char *ofile = "/dev/null";
	struct match *m = NULL;
	int mnum = 0;

	/* parse command line */
	while ((c = getopt(argc, argv, "i:o:m:")) != -1) {
		switch (c) {
		case 'i': /* input file */
			ifile = optarg;
			break;
		case 'o': /* output file */
			ofile = optarg;
			break;
		case 'm': /* match */
			createm(&m, &mnum, optarg);
			break;
		default:
			printf("invalid option\n");
			break;
		}
	}

	if (m == NULL)
		printf("warning: no -m option was supplied\n");
	/* create a pipe */
	if ((pipe(fd) < 0) || (pipe(fd1) < 0)) {
		err_sys("pipe error");
	}
	p2_actions(m, mnum, fd[0], fd1[1]); /* read from fd[0], write to fd1[1] */

#if 0
  	if ((child_pid = fork()) < 0) {
		err_sys("fork error");
	} else if (child_pid > 0) { /* this is the parent */
		close(fd[0]); /* close fd read endpoint */
		close(fd1[0]);
		close(fd1[1]);
		p1_actions(ifile, fd[1]); /* write to fd[1] */
		if (waitpid(child_pid, NULL, 0) < 0) /* wait for child */
			{ err_sys("waitpid error"); }
		printf("p1_action finished\n");
	} else { /* this is the child of first fork() */
			close(fd[1]); /* close fd write endpoint */
			close(fd1[0]); /* close fd1 read endpoint */
			p2_actions(m, mnum, fd[0], fd1[1]); /* read from fd[0], write to fd1[1] */
			deletem(m);
		printf("p2_action finished\n");

#if 0
		if ((gchild_pid = fork()) < 0) {
			err_sys("fork error");
		} else if (gchild_pid > 0) { /* this is the parent of 2nd fork() */
			close(fd[1]); /* close fd write endpoint */
			close(fd1[0]); /* close fd1 read endpoint */
			p2_actions(m, mnum, fd[0], fd1[1]); /* read from fd[0], write to fd1[1] */
			deletem(m);
			if (waitpid(gchild_pid, NULL, 0) < 0)
				err_sys("waitpid error");
		} else { /* this is the child of 2nd fork()*/
			close(fd1[1]); /* close fd1 write endpoint */
			close(fd[0]);
			close(fd[1]);
			//p3_actions(ofile, fd1[0]); /* read from fd1[0] */
		}
#endif
	}
#endif
  return 0;
}

/* print message and quit */
void err_sys(char *msg)
{
	printf("error: PID %d, %s\n", getpid(), msg);
	exit(0);
}

/* read a line from a file */
char *readline(FILE* fp)
{
	int max_byte = BUFFER_SIZE;
	char *buffer = (char *)malloc(sizeof(char) * max_byte);
	int count = 0; /* number of char in a line */
	char ch;
	//printf("readline\n");
	/* initialize buffer as NULL character */
	memset(buffer, '\0', max_byte);

	/* read a char */
	while (((ch = fgetc(fp)) != '\n') && (ch != EOF)) {
		/* if the size of the array exceed what we allocated */
		if (count == max_byte) {
			max_byte += BUFFER_SIZE;
			/* realloc more */
			buffer = realloc(buffer, sizeof(char) * max_byte);
			memset((buffer+max_byte-BUFFER_SIZE), '\0', max_byte);
		}
		buffer[count] = ch;
		count ++;
		//printf("%c\n", ch);
	}
	/* if touch the end of the file, return NULL */
	if (ch == EOF) {
		free(buffer);
		return NULL;
	} else {
		buffer[count] = ch;
		return buffer;
	}
}

/* procedure for P1 */
static void p1_actions(char* ifile, int fd)
{
	FILE *fp, *fp1;
	int fsize;

	fp = fdopen(fd, "w"); /* open write endpoint of pipe0 */
	if (fp == NULL)
		{ err_sys("fdopen(w) error"); }

	fp1 = fopen(ifile, "r"); /* open input file */
	if (fp1 == NULL)
		err_sys("fopen error");

	/* calculate file size */
	fseek(fp1, 0, SEEK_END);
	fsize = ftell(fp1);
	rewind(fp1);

	/* read from fp1 and write to fp */
	while (1) {
		char *line = readline(fp1);
		if (line == NULL)
			break;
		fwrite(line, sizeof(char), strlen(line), fp);
		free(line);
	}

	/* close pipe and files */
	fclose(fp1);
	fclose(fp);
	printf("P1: file %s, bytes %d\n", ifile, fsize);
}

/* create a struct match linked-list */
void createm(struct match **m, int *mnum, char *pattern)
{
	struct match *mp = malloc(sizeof(struct match));
	memset(mp, 0, sizeof(struct match));

	mp->pattern = pattern;
	*mnum++;

	/* link the new struct match */
	if (*m == NULL)
		*m = mp;
	else {
		/* put a new one to the beginning of the list */
		mp->next = *m;
		*m = mp;
	}
}

/* delete linked-list of struct match */
void deletem(struct match *m)
{
	struct match *cur = m;
	struct match *next;
	while (cur != NULL) {
		next = cur->next;
		free(cur);
		cur = next;
	}
}

/* string compare */
int cmpstr(struct match *m, char *buffer)
{
	char *tmp = buffer;
	int matches = 0; /* number of match */
	while ((tmp = strstr(tmp, m->pattern)) != NULL) {
		matches ++;
		/* move the pointer forward in the length of the pattern */
		/* in order to fine the next match */
		tmp += strlen(m->pattern);
	}
	/* update struct match */
	if (matches > 0) {
		m->matches += matches;
		m->match_line ++;
		return 1;
	}
	return 0;
}

struct line {
	char *line;
	int ccount; /* how manay consumers have accessed the line, for buffer deletion */
};

struct buffer {
	struct line *array;
	int bcount; /* number of entry of the buffer */
	sem_t mutex, full, empty;
};

struct producer {
	pthread_t tid; /* set via pthread_create() */
	FILE *fp;
	struct buffer *buffer;
	int index; /* last visited index */
	void *exit_status; /* set via pthread_join() */
};

struct consumer {
	pthread_t tid; /* set via pthread_create() */
	struct buffer *buffer;
	int index; /* last visited index of the buffer */
	struct match *m;
	void *exit_status; /* set via pthread_join() */
};

void init_buffer(struct buffer **b)
{
	*b = malloc(sizeof(struct buffer));
	memset(*b, 0, sizeof(struct buffer));
	(*b)->array = malloc(LINE_ENTRY * sizeof(struct line));
	memset((*b)->array, 0, LINE_ENTRY * sizeof(struct line));
	//char *buf = malloc(100);
	//(&b->array[0])->line = buf;
	//printf("(&b->array[0])->line %p\n", (&b->array[0])->line);

	(*b)->bcount = LINE_ENTRY;
	sem_init(&(*b)->mutex, 0, 1);
	sem_init(&(*b)->full, 0, 0);
	sem_init(&(*b)->empty, 0, (*b)->bcount);
	//print_buffer(b);
}

#if 0
void print_line(struct buffer *b)
{
	for (int i = 0; i < b->bcount; i++) {
		for ()
	}
		printf("");
}
#endif

void print_buffer(struct buffer *b)
{
	int mutex_val, full_val, empty_val;

	printf("b %p, b->array %p\n", b, b->array);
	Sem_getvalue(&b->mutex, &mutex_val, "print_buffer() mutex");
	Sem_getvalue(&b->full , &full_val , "print_buffer() full ");
	Sem_getvalue(&b->empty, &empty_val, "print_buffer() empty");
}

void init_producer(struct producer *p, FILE *fp)
{
	//p->tid = (pthread_t)-1;
	//p->fp = fp;
	init_buffer(&p->buffer);	
	//p->index = 0;
	//p->exit_status = (void *)-1;
	//p->buffer->array[0].line = malloc(100);
	printf("p->buffer->array[0] %p\n", p->buffer->array[0]);
}

void *producer_func(void *arg)
{
	struct producer *p = (struct producer *)arg;

	while (1) {
		char *line = readline(p->fp);
		//printf("p->buffer->array[%d] %p\n", p->index,  (&p->buffer->array[p->index])->line);
		printf("p->buffer->array[%d]\n", p->index);
		if (line == NULL)
			break;
		//sem_wait(&p->buffer->empty);
		//sem_wait(&p->buffer->mutex);
		//(&p->buffer->array[p->index])->line = malloc(100);
		printf("producer_func: line\n");
		p->index++;
		if (p->index >= p->buffer->bcount)
			p->index %= p->buffer->bcount;
		//sem_post(&p->buffer->mutex);
		//sem_post(&p->buffer->full);
	}
	return arg;
}

/* procedure for P2 */
static void p2_actions(struct match *m, int mnum, int fd, int fd1)
{
	FILE *fp, *fp1;
	struct match *mp;
	struct producer p;
	int err;

	fp = fdopen(fd, "r"); /* use fp as if it had come from fopen() */
	if (fp == NULL)
		{ err_sys("fdopen(r) error"); }

	fp1 = fdopen(fd1, "w"); /* use fp as if it had come from fopen() */
	if (fp1 == NULL)
		{ err_sys("fdopen(w) error"); }
#if 0
	while (1) {
		char *line = readline(fp);
		int match = 0;
		struct match *mp;
		if (line == NULL)
			break;
		/* find pattern match in each line */
		for (mp = m; mp != NULL; mp = mp->next)
			match += cmpstr(mp, line);

		/* write matched line to the pipe */
		if (match)
			fwrite(line, sizeof(char), strlen(line), fp1);
		free(line);
	}
#else
	init_producer(&p, fp);
	//Pthread_create(&p.tid, NULL, producer_func, (void *)&p, "producer");
	//Pthread_join(p.tid, &p.exit_status, "producer");
	printf("end p2_action\n");

#endif
	fclose(fp);
	fclose(fp1);

	/* print to stdout */
	for (mp = m; mp != NULL; mp = mp->next) {
		printf("P2: string %s, line %d, matches %d\n",
			mp->pattern, mp->match_line, mp->matches);
	}
}

/* procedure for P3 */
static void p3_actions(char *ofile, int fd)
{
	FILE *fp, *fp1;
	int line_count = 0; /* calculate the line of output file */

	fp = fdopen(fd, "r"); /* use fp as if it had come from fopen() */
	if (fp == NULL)
		{ err_sys("fdopen(w) error"); }

	fp1 = fopen(ofile, "w");
	if (fp1 == NULL)
		err_sys("fopen error");

	/* read a line from pipe and write it to a file */
	while (1) {
		char *line = readline(fp);
		if (line == NULL) /* no more lines */
			break;
		line_count++;
		fwrite(line, sizeof(char), strlen(line), fp1);
		free(line);
	}
	fclose(fp);
	fclose(fp1);

	printf("P3: file %s, lines %d\n", ofile, line_count);
}
