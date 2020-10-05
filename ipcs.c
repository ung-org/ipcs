/*
 * UNG's Not GNU
 *
 * Copyright (c) 2020, Jakob Kaivo <jkk@ung.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _XOPEN_SOURCE 700
#include <errno.h>
#include <locale.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>

enum {
	TYPE_WIDTH = 1,
	ID_WIDTH = 10,
	KEY_WIDTH = (sizeof(key_t) * 2),
	MODE_WIDTH = 11,
	OWNER_WIDTH = 8,
	GROUP_WIDTH = 8,
	CREATOR_WIDTH = OWNER_WIDTH,
	CGROUP_WIDTH = GROUP_WIDTH,
	CBYTES_WIDTH = 10,
	QNUM_WIDTH = 5,
	QBYTES_WIDTH = 10,
	LSPID_WIDTH = 7,
	LRPID_WIDTH = 7,
	STIME_WIDTH = 9,
	RTIME_WIDTH = 9,
	NATTCH_WIDTH = 8,
	SEGSZ_WIDTH = 6,
	CPID_WIDTH = 7,
	LPID_WIDTH = 7,
	ATIME_WIDTH = 9,
	DTIME_WIDTH = 9,
	NSEMS_WIDTH = 6,
	OTIME_WIDTH = 9,
	CTIME_WIDTH = 9,
};

enum {
	MSG = 'q',
	SHM = 'm',
	SEM = 's',
};

enum {
	BYTES = (1<<0),
	CREATOR = (1<<1),
	OUTSTANDING = (1<<2),
	PROCESS = (1<<3),
	TIME = (1<<4),
};

struct ipc {
	struct ipc *next;
	struct ipc *prev;

	/* always */
	char type;
	int id;
	unsigned int key;
	char mode[11];
	uid_t owner;
	gid_t group;

	/* CREATOR */
	uid_t creator;
	gid_t cgroup;

	/* OUTSTANDING */
	int cbytes;	/* MSG */
	int qnum;	/* MSG */
	int natcch;	/* SHM */

	/* BYTES */
	int qbytes;	/* MSG */
	int segsz;	/* SHM */
	int nsems;	/* SEM */

	/* PROCESS */
	pid_t lspid;	/* MSG */
	pid_t lrpid;	/* MSG */
	pid_t cpid;	/* SHM */
	pid_t lpid;	/* SHM */

	/* TIME */
	time_t stime;	/* MSG */
	time_t rtime;	/* MSG */
	time_t atime;	/* SHM */
	time_t dtime;	/* SHM */
	time_t otime;	/* SEM */
	time_t ctime;
};

static void print_header(void)
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	char date[128];
	strftime(date, sizeof(date), "%a %b %e %H:%M:%S %Z %Y", tm);
	printf("IPC status from %s as of %s\n", "<source>", date); /* FIXME */
}

static void print_user(int width, uid_t uid)
{
	printf(" %-*d", width, uid);
}

static void print_group(int width, gid_t gid)
{
	printf(" %-*d", width, gid);
}

static void print_record(char type, int opts, struct ipc *entry)
{
	printf("%-*c", TYPE_WIDTH, entry->type);
	printf(" %-*d", ID_WIDTH, entry->id);
	printf(" 0x%0*x", KEY_WIDTH - 2, entry->key);
	printf(" %-*s", MODE_WIDTH, entry->mode);
	print_user(OWNER_WIDTH, entry->owner);
	print_group(GROUP_WIDTH, entry->group);

	printf("\n");
}

static void print_report(char type, int opts, struct ipc *list)
{
	char *facility = NULL;
	
	if (type == MSG) {
		facility = "Message Queues";
	} else if (type == SHM) {
		facility = "Shared Memory";
	} else if (type == SEM) {
		facility = "Semaphores";
	}

	if (list == NULL) {
		printf("%s facility not in system.\n", facility);
		return;
	}

	printf("%s:\n", facility);

	printf("%-*c", TYPE_WIDTH, 'T');
	printf(" %-*s", ID_WIDTH, "ID");
	printf(" %-*s", KEY_WIDTH, "KEY");
	printf(" %-*s", MODE_WIDTH, "MODE");
	printf(" %-*s", OWNER_WIDTH, "OWNER");
	printf(" %-*s", GROUP_WIDTH, "GROUP");

	if (opts & CREATOR) {
		printf(" %-*s", CREATOR_WIDTH, "CREATOR");
		printf(" %-*s", CGROUP_WIDTH, "CGROUP");
	}

	if (opts & OUTSTANDING) {
		if (type == MSG) {
			printf(" %-*s", CBYTES_WIDTH, "CBYTES");
			printf(" %-*s", QNUM_WIDTH, "QNUM");
		} else if (type == SHM) {
			printf(" %-*s", NATTCH_WIDTH, "NATTCH");
		}
	}

	if (opts & BYTES) {
		if (type == MSG) {
			printf(" %-*s", QBYTES_WIDTH, "QBYTES");
		} else if (type == SHM) {
			printf(" %-*s", SEGSZ_WIDTH, "SEGSZ");
		} else if (type == SEM) {
			printf(" %-*s", NSEMS_WIDTH, "NSEMS");
		}
	}

	if (opts & PROCESS) {
		if (type == MSG) {
			printf(" %-*s", LSPID_WIDTH, "LSPID");
			printf(" %-*s", LRPID_WIDTH, "LRPID");
		} else if (type == SHM) {
			printf(" %-*s", CPID_WIDTH, "CPID");
			printf(" %-*s", LPID_WIDTH, "LPID");
		}
	}

	if (opts & TIME) {
		if (type == MSG) {
			printf(" %-*s", STIME_WIDTH, "STIME");
			printf(" %-*s", RTIME_WIDTH, "RTIME");
		} else if (type == SHM) {
			printf(" %-*s", ATIME_WIDTH, "ATIME");
			printf(" %-*s", DTIME_WIDTH, "DTIME");
		} else if (type == SEM) {
			printf(" %-*s", OTIME_WIDTH, "OTIME");
		}

		printf(" %-*s", CTIME_WIDTH, "CTIME");
	}

	printf("\n");

	while (list) {
		print_record(type, opts, list);
		list = list->next;
	}
}

static struct ipc *ipcs(char type)
{
	struct ipc *r = calloc(1, sizeof(*r));
	r->type = type;
	memset(r->mode, '-', sizeof(r->mode));

	return r;
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	int c;
	int msgs = 0;
	int shms = 0;
	int sems = 0;
	unsigned int print = 0;

	while ((c = getopt(argc, argv, "qmsabcopt")) != -1) {
		switch(c) {
		case 'q':
			msgs = 1;
			break;

		case 'm':
			shms = 1;
			break;

		case 's':
			sems = 1;
			break;

		case 'a':
			print = BYTES | CREATOR | OUTSTANDING | PROCESS | TIME;
			break;

		case 'b':
			print |= BYTES;
			break;

		case 'c':
			print |= CREATOR;
			break;

		case 'o':
			print |= OUTSTANDING;
			break;

		case 'p':
			print |= PROCESS;
			break;

		case 't':
			print |= TIME;
			break;

		default:
			return 1;
		}
	}

	if (msgs == 0 && shms == 0 && sems == 0) {
		msgs = 1;
		shms = 1;
		sems = 1;
	}

	struct ipc *msglist = msgs ? ipcs(MSG) : NULL;
	struct ipc *shmlist = shms ? ipcs(SHM) : NULL;
	struct ipc *semlist = sems ? ipcs(SEM) : NULL;

	print_header();

	if (msgs) {
		print_report(MSG, print, msglist);
	}

	if (shms) {
		print_report(SHM, print, shmlist);
	}

	if (sems) {
		print_report(SEM, print, semlist);
	}

	return 0;
}
