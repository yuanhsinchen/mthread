/* CMPSC 473, Project 2
 * Author: Yuan-Hsin Chen yuc200@psu.edu yuc200
 *
 * See http://www.cse.psu.edu/~dheller/cmpsc311/Lectures/Interprocess-Communication.html
 * and http://www.cse.psu.edu/~dheller/cmpsc311/Lectures/Files-Directories.html
 * for some more information, examples, and references to the CMPSC 311 textbooks.
 */

//--------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <inttypes.h>
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
	char **line; /* store pointers to matched line */
	struct match *next;
};

struct queue{
	void **queue;
	int size;
	int index; /* next index of queue */
};

struct line {
	char *line;
	int ccount; /* how manay consumers have accessed the line, for buffer deletion */
	bool match;
	sem_t mutex;
};

struct buffer {
	//struct line *array;
	struct queue *q;
	int bcount; /* number of entry of the buffer */
	int maxccount; /* if line->ccount == maxccount, free the line */
	sem_t mutex, full, empty;
};

struct producer {
	pthread_t tid; /* set via pthread_create() */
	FILE *fp;
	struct buffer *buffer;
	int cnum;
	//int index; /* last visited index */
	struct queue *line; /* queue all lines */
	void *exit_status; /* set via pthread_join() */
};

struct consumer {
	pthread_t tid; /* set via pthread_create() */
	struct buffer *buffer;
	struct buffer *matched;
	struct match *m;
	void *exit_status; /* set via pthread_join() */
};

struct copy_line {
	pthread_t tid;
	FILE *fp;
	struct buffer *buffer;
	void *exit_status;
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
#define QUEUE_SIZE 20

void print_string(char *s)
{
	int len = strlen(s);
	for (int i = 0; i < len; i++)
		printf("%c", s[i]);
	printf("\n");
}

void init_queue(struct queue *q)
{
//	printf("init_queue: q %p\n", q);
	q->queue = malloc(QUEUE_SIZE * sizeof(void *));
//	printf("q->queue %p\n\n", q->queue);
	q->size = QUEUE_SIZE;
	q->index = 0;
}

void enqueue(struct queue *q, void *c)
{
	if (q->index == q->size) {
		q->size += QUEUE_SIZE;
		q->queue = realloc(q->queue, sizeof(void *) * q->size);
	}
	q->queue[q->index] = c;
	q->index++;
	//printf("enqueue %p index %d c %p\n", q, q->index, c);
	//print_string((char *)(((struct line*)c)->line));
}

void *dequeue(struct queue *q)
{
	void *c = q->queue[0];

	for (int i = 0; i < q->index - 1; i++)
		q->queue[i] = q->queue[i + 1];

	q->index--;

	//printf("dequeue %p index %d c %p\n", q, q->index, c);
	//print_string((char *)(((struct line*)c)->line));
	return c;
}

bool is_q_empty(struct queue *q)
{
	if (q->index <= 0) {
	//	printf("queue %p is empty!\n", q);
		return true;
	} else {
	//	printf("queue %p has elements\n", q);
		return false;
	}
}

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
  	if ((child_pid = fork()) < 0) {
		err_sys("fork error");
	} else if (child_pid > 0) { /* this is the parent */
		close(fd[0]); /* close fd read endpoint */
		close(fd1[0]);
		close(fd1[1]);
		p1_actions(ifile, fd[1]); /* write to fd[1] */
		if (waitpid(child_pid, NULL, 0) < 0) /* wait for child */
			{ err_sys("waitpid error"); }
	} else { /* this is the child of first fork() */
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
			p3_actions(ofile, fd1[0]); /* read from fd1[0] */
		}
	}
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
	(*mnum)++;

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
	//printf("cmpstr: buffer %p matches %d\n", buffer, matches);
	/* update struct match */
	if (matches > 0) {
		m->matches += matches;
		m->match_line ++;
		return 1;
	}
	return 0;
}

void init_buffer(struct buffer *b, int mnum)
{
	b->q = malloc(sizeof(struct queue));
	//printf("init_buffer b %p b->q %p\n", b,  b->q);
	init_queue(b->q);

	b->bcount = b->q->size;
	b->maxccount = mnum;
	sem_init(&b->mutex, 0, 1);
	sem_init(&b->full, 0, 0);
	sem_init(&b->empty, 0, b->bcount);
}

void print_buffer(struct buffer *b)
{
	int mutex_val, full_val, empty_val;

	//printf("b %p, b->array %p\n", b, b->array);
	Sem_getvalue(&b->mutex, &mutex_val, "print_buffer() mutex");
	Sem_getvalue(&b->full , &full_val , "print_buffer() full ");
	Sem_getvalue(&b->empty, &empty_val, "print_buffer() empty");
}


void init_producer(struct producer *p, FILE *fp, struct buffer *b, int mnum)
{
	p->tid = (pthread_t)-1;
	p->fp = fp;
	p->buffer = b;
	p->cnum = mnum;
//	printf("p->buffer %p\n", p->buffer);
	//printf("p->buffer->array %p\n", p->buffer->array);
	//printf("p->buffer->array[0] %p\n", p->buffer->array[0]);

	//p->index = 0;
	p->line = malloc(sizeof(struct queue));
	init_queue(p->line);
	//printf("init_producer : init_queue\n");
	p->exit_status = (void *)-1;                                  
}


void *producer_func(void *arg)
{
	struct producer *p = (struct producer *)arg;

	while (1) {
		char *line = readline(p->fp);
		struct line *l;
		if (line == NULL)
			break;
		l = malloc(sizeof(struct line));
		l->line = line;
		l->ccount = 0;
		l->match = false;
		sem_init(&l->mutex, 0, 1);
		for (int i = 0; i < p->cnum; i++) {
			sem_wait(&p->buffer[i].empty);
			sem_wait(&p->buffer[i].mutex);
			enqueue(p->buffer[i].q, (void *)l);
			sem_post(&p->buffer[i].mutex);
			sem_post(&p->buffer[i].full);
			//printf("enqueue %p\n", p->buffer[i].q);
		}
		enqueue(p->line, (void *)l);
	}

	return arg;
}

void init_consumer(struct consumer *c, struct match *m, struct buffer *b, struct buffer *matched)
{
	c->tid = (pthread_t)-1;
	c->buffer = b;
	c->matched = matched;
	c->m = m;
	c->exit_status = (void *)-1;
}

void free_line(struct line *l)
{
	if (l->match)
		l->line = NULL;
	else
		free(l->line);
	l->ccount = 0;
	l->match = false;
}

void *consumer_func(void *arg)
{
	struct consumer *c = (struct consumer *)arg;
	struct line *l;
	if (c == NULL) {
		printf("consumer_func(), thread 0x%jx, NULL arg\n", (uintmax_t)pthread_self());
		exit(1);
	}
	//printf("consumer_func(), thread 0x%jx, tid 0x%jx, queue %p starting\n", (uintmax_t)pthread_self(), (uintmax_t)c->tid, c->buffer->q);

	//while (!is_q_empty(c->buffer->q) || !c->buffer->q->nomore){
	do{
	/* queue is not empty */
		sem_wait(&c->buffer->full);
		sem_wait(&c->buffer->mutex);
		l = (struct line*)dequeue(c->buffer->q);
		sem_post(&c->buffer->mutex);
		sem_post(&c->buffer->empty);
		//printf("TID: 0x%x consumer_func: dequeue line %p\n", c->tid, l);
		if (cmpstr(c->m, l->line)) {
			if (!l->match) {
				l->match = true;
				sem_wait(&c->matched->empty);
				sem_wait(&c->matched->mutex);
				enqueue(c->matched->q, (void *)l);
				sem_post(&c->matched->mutex);
				sem_post(&c->matched->full);
			}
		}
		/* update ccount for a line */
		sem_wait(&l->mutex);
		l->ccount++;
		sem_post(&l->mutex);
	/* if all consumers have accessed the line, free it */
#if 0
	if (c->buffer->array[c->index].ccount >= c->buffer->maxccount) {
		//printf("TID: 0x%x free line %p\n", c->tid, c->buffer->array[c->index].line);
		if (c->buffer->array[c->index].line != NULL) {
			sem_wait(&c->buffer->full);
			sem_wait(&c->buffer->mutex);
			free_line(&c->buffer->array[c->index]);
			sem_post(&c->buffer->mutex);	
			sem_post(&c->buffer->empty);
		}
	}
#endif
	} while (!is_q_empty(c->buffer->q));
	//sem_post(&c->buffer->empty);
	//printf("consumer_func(), thread 0x%jx, tid 0x%jx, queue %p ended\n", (uintmax_t)pthread_self(), (uintmax_t)c->tid, c->buffer->q);
#if 0
	/* print to stdout */
	printf("P2: string %s, line %d, matches %d\n",
		c->m->pattern, c->m->match_line, c->m->matches);
#endif
#if 0
	/* write matched line to the pipe */
	if (match)
		fwrite(line, sizeof(char), strlen(line), fp1);
#endif
	return arg;
}

void init_copyline(struct copy_line *c, struct buffer *matched, FILE* fp)
{
	c->tid = (pthread_t)-1;
	c->buffer = matched;
	c->fp = fp;
	c->exit_status = (void *)-1;
}

void *copy_matched(void *arg)
{
	struct copy_line *c = (struct copy_line *)arg;
	struct line *l;

	do{
		sem_wait(&c->buffer->full);
		sem_wait(&c->buffer->mutex);
		l = (struct line*)dequeue(c->buffer->q);
		sem_post(&c->buffer->mutex);
		sem_post(&c->buffer->empty);
		fwrite(l->line, sizeof(char), strlen(l->line), c->fp);
	} while(!is_q_empty(c->buffer->q));

	return arg;
}

/* procedure for P2 */
static void p2_actions(struct match *m, int mnum, int fd, int fd1)
{
	FILE *fp, *fp1;
	struct match *mp;
	struct buffer b[mnum];
	struct producer p;
	struct consumer c[mnum];
	struct buffer matched;
	struct copy_line cl;
	int n = 0;

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
	for (int i = 0; i < mnum; i++)
		init_buffer(&b[i], mnum);

	init_producer(&p, fp, b, mnum);
	Pthread_create(&p.tid, NULL, producer_func, (void *)&p, "producer");
#if 1
	init_buffer(&matched, 1); /* init buffer for matched lines */
	for (mp = m; mp != NULL; mp = mp->next) {
		init_consumer(&c[n], mp, &b[n], &matched);
		Pthread_create(&c[n].tid, NULL, consumer_func, (void *)&c[n], "consumer");
		n++;
	}
	for (int i = 0; i < mnum; i++)
		Pthread_join(c[i].tid, &c[i].exit_status, "consumer");
	Pthread_join(p.tid, &p.exit_status, "producer");
	init_copyline(&cl, &matched, fp1);
	Pthread_create(&cl.tid, NULL, copy_matched, (void *)&cl, "copy_matched");

	Pthread_join(cl.tid, &cl.exit_status, "copy_matched");
	//printf("end p2_action\n");
#endif
#endif
#if 0
	while(!is_q_empty(p.line)) {
		struct line *l = dequeue(p.line);
		if (l->match)
			//print_string(l->line);
			fwrite(l->line, sizeof(char), strlen(l->line), fp1);
	}
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
