/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2024-2026 Shi Yanling
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <stdalign.h>
#include <stddef.h>
#include <ctype.h>
#include <locale.h>
#include <wchar.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#define NCURSES_WIDECHAR 1
#include <curses.h>

#define VERSION        "1.3"
#define EXTFNNAME      "sff-extfunc"
#define EXTFNPREFIX    "/usr/local/lib/sff"
#define EXTFNPREFIX2   "/usr/lib/sff"
#ifndef PATH_MAX
#define PATH_MAX       4096
#endif
#ifndef NAME_MAX
#define NAME_MAX       255
#endif
#define TABS_MAX       4 // Number of tabs, the range of acceptable values is 1-7
#define ENTRY_INCR     128 // Number of Entry structures to allocate per shot
#define NAME_INCR      4096 // 128 entries * avg. 32 chars per name = 4KB
#define FILT_MAX       128 // Maximum length of filter string
#define HSTAT_INCR     16 // Number of Histstat structures to allocate each time

#define LENGTH(X)      (sizeof X / sizeof X[0])
#define MIN(x, y)      ((x) < (y) ? (x) : (y))
#define MAX(x, y)      ((x) > (y) ? (x) : (y))

enum entryflag {
	E_REG_FILE = 0x01, E_DIR_DIRLNK = 0x02,
	E_SEL = 0x04, E_SEL_SCANED = 0x08, E_NEW = 0x10
};

enum filetypes {
	F_REG = 0, F_DIR, F_CHR, F_BLK, F_IFO, F_LNK, F_SOCK,
	F_HLNK, F_EXEC, F_EMPT, F_ORPH, F_MISS, F_UNKN
};

enum colorflag {
	C_DETAIL = F_UNKN + 1, C_TABTAG, C_PATHBAR, C_STATBAR, C_WARN, C_NEWFILE
};

enum histstatflag {
	S_UNVIS = 0, S_VIS, S_ROOT, S_SUBROOT
};

enum procctrl {
	GO_NONE = 0, GO_STATBAR, GO_FASTDRAW, GO_REDRAW, GO_SORT, GO_RELOAD, GO_QUIT
};

typedef struct {
	char *name; // 8 bytes
	off_t size; // 8 bytes
	time_t sec; // 8 bytes
	unsigned int nsec; // 4 bytes
	mode_t mode; // 4 bytes
	uid_t uid; // 4 bytes
	gid_t gid; // 4 bytes
	unsigned short type; // 2 bytes
	unsigned short flag; // 2 bytes
	unsigned short nlen; // 2 bytes
	unsigned short misc; // 2 bytes
} Entry;

typedef struct {
	int cur;
	int scrl;
	char name[NAME_MAX + 1];
	int flag;
} Histstat;

typedef struct {
	Histstat *hs;
	Histstat *stat;
	char path[PATH_MAX];
	unsigned int nhs;
	unsigned int ths;
} Histpath;

struct selstat {
	struct selstat *prev;
	struct selstat *next;
	char path[PATH_MAX];
	char *nbuf;
	char *endp;
	size_t buflen;
};

typedef struct {
	char cols[8];  // Columns: 't'ime, 'o'wner, 'p'erm, 's'ize, 'n'ame, Uppercase for placeholders
	unsigned int enabled    : 1;
	unsigned int showhidden : 1;  // Show hidden files
	unsigned int dirontop   : 1;  // Sort directories on the top
	unsigned int sortby     : 3;  // (0: name, 1: size, 2: time, 3: extension)
	unsigned int natural    : 1;  // Natural numeric sorting
	unsigned int reverse    : 1;  // Reverse sort
	unsigned int timetype   : 2;  // (0: access, 1: modify, 2: change)
	unsigned int havesel    : 1;  // Have selection in current path
	unsigned int mansel     : 1;  // Manual select mode
	// global settings
	unsigned int ct         : 3;  // Current tab
	unsigned int lt         : 3;  // Last tab
	unsigned int runmode    : 2;  // (0: normal mode, 1: sudo mode, 2: permanent sudo mode)
	unsigned int marknew    : 1;  // Show marks for new file
	unsigned int refresh    : 1;  // Force screen refresh during redraw
	unsigned int redrawn    : 1;  // Screen has been redrawn
	unsigned int openfile   : 1;  // Open files on right arrow or 'l' key
	unsigned int symbperm   : 1;  // Show permissions as symbolic strings
	unsigned int abbrdate   : 1;  // Use ls-style date format
} Settings;

typedef struct {
	Histpath *hp;
	struct selstat *ss;
	char filt[FILT_MAX];
	char find[FILT_MAX];
	int ftlen;
	int fdlen;
	int nde;
	int nsel;
	Settings cfg;
} Tabs;

typedef struct {
	int keysym1;
	int keysym2;
	int (*func)(int);
	int arg;
	char cmnt[40];
} Key;

/*** Global Variables ***/

static int ndents = 0, tdents = 0, cursel = 0, lastsel = -1, curscroll = 0;
static int markent = -1, errline = 0, errnum = 0;
static int xlines, xcols, onscr, ncols;
static size_t namebuflen = 0;
static time_t curtime;
static char *home, *opener, *sudoer;
static char *cfgpath = NULL, *extfunc = NULL, *pipepath = NULL, *pvfifo = NULL;
static char *pnamebuf = NULL, *pfindbuf = NULL, *pfindend = NULL, *findname = NULL;
static Entry *pdents = NULL;
static Tabs *ptab = NULL;

alignas(max_align_t) static char gpbuf[PATH_MAX * sizeof(wchar_t)] = {0};
alignas(max_align_t) static Tabs gtab[TABS_MAX + 1] = {{0}};
alignas(max_align_t) static Histpath ghpath[(TABS_MAX + 1) * 2] = {{0}};

/****** Generic Functions ******/

#ifdef DEBUG
static void dbgprint(char *vn, char *str, int n)
{
	FILE *fp = fopen("/tmp/sffdbg", "a");
	if (!fp) {
		perror("dbg");
		return;
	}

	fprintf(fp, "--- %s: %s %d\n", vn, str, n);
	fclose(fp);
}
#endif

/* Get directory portion of pathname. Source would be modified!!! */
static char *xdirname(char *path)
{
	char *p = strrchr(path, '/');

	if (p == path)
		path[1] = '\0';
	else if (p)
		*p = '\0';
	return path;
}

/* Get filename portion of pathname. Source would be untouched. */
static const char *xbasename(const char *path)
{
	const char *p = strrchr(path, '/');
	return p ? p + 1 : path;
}

/* Make path/name in buf. Returns the number of bytes copied including terminating '\0'. */
static int makepath(const char *path, const char *name, char *buf)
{
	if (!path || !path[0] || !name || !buf)
		return 0;

	char *p = NULL;
	if (path == buf)
		p = memchr(buf, '\0', PATH_MAX - 2);
	else if ((p = memccpy(buf, path, '\0', PATH_MAX - 2)))
		--p;

	if (p) {
		if (p[-1] != '/')
			*p++ = '/';
		p = memccpy(p, name, '\0', PATH_MAX - (p - buf) - 1);
	}
	return p ? p - buf : 0;
}

/* Get file extension. Ignore extensions > 8 chars. len includes terminating '\0'. */
static const char *getextension(const char *name, size_t len)
{
	if (len < 4)
		return NULL;

	const char *p = name + len - 2; // skip last char (before '\0')
	len = (len > 11) ? 9 : len - 2; // If name length exceeds 2+8, check max 8 times

	 while (--len > 0)
		if (*(--p) == '.')
			return p;
	return NULL;
}

/* Get the absolute pathname without resolving symlinks. buf can not be NULL. */
static char *abspath(const char *src, char *buf)
{
	if (!src || !buf)
		return NULL;

	size_t len = 0;
	if (src[0] != '/') {
		if (!getcwd(buf, PATH_MAX))
			return NULL;
		if (!src[0])
			return buf;
		len = strlen(buf);
	} else
		++src;

	char *dst = buf + len;
	*dst++ = '/';

	while (*src) {
		if (src[0] == '/' && (dst[-1] == '/' || src[1] == '/' || src[1] == '\0')) {
			++src;
			continue;
		} else if (dst[-1] == '/' && src[0] == '.' && (src[1] == '/' || src[1] == '\0')) {
			src = (src[1] == '\0') ? src + 1 : src + 2;
			continue;
		} else if (dst[-1] == '/' && src[0] == '.' && src[1] == '.' && (src[2] == '/' || src[2] == '\0')) {
			if (dst > buf + 1)
				dst[-1] = '\0';
			dst = strrchr(buf, '/') + 1;
			src = (src[2] == '\0') ? src + 2 : src + 3;
			continue;
		}

		if (++len == PATH_MAX - 1) {
			errno = ENAMETOOLONG;
			return NULL;
		}
		*dst++ = *src++;
	}

	if (dst > buf + 1 && dst[-1] == '/')
		dst[-1] = '\0';
	else
		dst[0] = '\0';
	return buf;
}

/* Convert unsigned integer to string. The maximum value it can handle is 4,294,967,295.
   This is a modified version of xitoa() from nnn. https://github.com/jarun/nnn */
static char *xitoa(unsigned int val)
{
	static char dst[24] = {0};
	static const char digits[] =
		"0001020304050607080910111213141516171819"
		"2021222324252627282930313233343536373839"
		"4041424344454647484950515253545556575859"
		"6061626364656667686970717273747576777879"
		"8081828384858687888990919293949596979899";
	unsigned int i, j, quo;

	for (i = 21; val >= 100; --i) { // Fill digits backward from dst[21]
		quo = val / 100;
		j = (val - (quo * 100)) << 1;
		val = quo;
		dst[i] = digits[j + 1];
		dst[--i] = digits[j];
	}

	if (val >= 10) {
		j = val << 1;
		dst[i] = digits[j + 1];
		dst[--i] = digits[j];
	} else
		dst[i] = '0' + val;
	return &dst[i];
}

/* Convert integer size to string like 6.2K 25.0M 198.3G etc. */
static char *tohumansize(off_t size)
{
	static char sbuf[12] = {0};
	static const char unit[12] = "BKMGTPEZY";
	int i, numint, frac = 0;

	for (i = 0; size >= 1024000; ++i)
		size >>= 10;

	if (i > 0 || size >= 1024) {
		size += 51; // round frac by (x + 51) / 100
		numint = size >> 10;
		frac = (size & 1023) * 10 >> 10; // by simplifying (size % 1024) * 1000 / 1024 / 100
		++i;
	} else
		numint = size;

	char *sp = (char *)memccpy(sbuf, xitoa(numint), '\0', 6) - 1;
	if (i > 0) {
		*sp++ = '.';
		*sp++ = '0' + frac;
	}

	*sp = unit[i];
	*(++sp) = '\0';
	return sbuf;
}

/* Convert inode permission info into a symbolic string, except the inode type. */
static char *strperms(mode_t mode)
{
	static char str[12] = {0};

	str[0] = mode & S_IRUSR ? 'r' : '-';
	str[1] = mode & S_IWUSR ? 'w' : '-';
	str[2] = mode & S_ISUID	? (mode & S_IXUSR ? 's' : 'S') : (mode & S_IXUSR ? 'x' : '-');

	str[3] = mode & S_IRGRP ? 'r' : '-';
	str[4] = mode & S_IWGRP ? 'w' : '-';
	str[5] = mode & S_ISGID ? (mode & S_IXGRP ? 's' : 'S') : (mode & S_IXGRP ? 'x' : '-');

	str[6] = mode & S_IROTH ? 'r' : '-';
	str[7] = mode & S_IWOTH ? 'w' : '-';
	str[8] = mode & S_ISVTX ? (mode & S_IXOTH ? 't' : 'T') : (mode & S_IXOTH ? 'x' : '-');
	return str;
}

/* Returns the cached user name if the provided uid is the same as the previous uid. */
static char *getpwname(uid_t uid)
{
	static char *unamecache = NULL;
	static uid_t uidcache = (uid_t)-1;

	if (uid != uidcache) {
		struct passwd *pw = getpwuid(uid);
		unamecache = pw ? pw->pw_name : NULL;
		uidcache = uid;
	}
	return unamecache ? unamecache : xitoa(uid);
}

/* Returns the cached group name if the provided gid is the same as the previous gid. */
static char *getgrname(gid_t gid)
{
	static char *gnamecache = NULL;
	static gid_t gidcache = (gid_t)-1;

	if (gid != gidcache) {
		struct group *gr = getgrgid(gid);
		gnamecache = gr ? gr->gr_name : NULL;
		gidcache = gid;
	}
	return gnamecache ? gnamecache : xitoa(gid);
}

static int seterrnum(int line, int err)
{
	errline = line;
	errnum = err;
	return TRUE;
}

static void spawn(char *arg0, char *arg1, char *arg2, int detach)
{
	pid_t pid;
	char *argv[4] = {arg0, arg1, arg2, NULL};
	struct sigaction oldsigtstp, oldsigwinch;

	pid = fork();
	if (pid > 0) {
		sigaction(SIGTSTP, &(struct sigaction){.sa_handler = SIG_IGN}, &oldsigtstp);
		sigaction(SIGWINCH, &(struct sigaction){.sa_handler = SIG_IGN}, &oldsigwinch);
		waitpid(pid, NULL, 0);
		sigaction(SIGTSTP, &oldsigtstp, NULL);
		sigaction(SIGWINCH, &oldsigwinch, NULL);

	} else if (pid == 0) {
		if (detach) {
			pid = fork(); // Fork a grandchild to detach
			if (pid != 0)
				_exit(EXIT_SUCCESS);
			setsid();
			// Suppress stdout and stderr
			int fd = open("/dev/null", O_WRONLY, 0200);
			if (fd != -1) {
				dup2(fd, STDOUT_FILENO);
				dup2(fd, STDERR_FILENO);
				close(fd);
			}
		}

		sigaction(SIGTSTP, &(struct sigaction){.sa_handler = SIG_IGN}, NULL);
		sigaction(SIGINT, &(struct sigaction){.sa_handler = SIG_DFL}, NULL);
		sigaction(SIGPIPE, &(struct sigaction){.sa_handler = SIG_DFL}, NULL);
		execvp(*argv, argv);
		_exit(EXIT_SUCCESS);
	} else
		seterrnum(__LINE__, errno);
}

/****** Key Functions ******/

static int movecursor(int n);
static int movequarterpage(int n);
static int scrollpage(int n);
static int scrolleighth(int n);
static int movetoedge(int n);
static int switchhistpath(int n);
static int enterdir(int n);
static int gotoparent(int n);
static int gotohome(int n);
static int refreshview(int n);
static int toggleselection(int n);
static int selectall(int n);
static int invertselection(int n);
static int selectrange(int n);
static int clearselection(int n);
static int setfilter(int n);
static int quickfind(int n);
static int qfindnext(int n);
static int switchtab(int n);
static int closetab(int n);
static int togglemode(int n);
static int viewoptions(int n);
static int prefixkey(int n);
static int showhelp(int n);
static int quitsff(int n);
static int callextfunc(int c);

#include "config.h" // Configuration

static int shiftcursor(int step, int scrl)
{
	int lastscroll = curscroll;

	lastsel = cursel;
	cursel = MAX(0, MIN(ndents - 1, cursel + step));

	if ((step == 1 || step == -1) && scrl == 0) {
		if ((cursel < curscroll + ((onscr + 2) >> 2) && step < 0)
		|| (cursel >= curscroll + onscr - ((onscr + 2) >> 2) && step > 0))
			curscroll += step;
	} else
		curscroll += scrl;
	curscroll = MIN(curscroll, MIN(cursel, ndents - onscr));
	curscroll = MAX(curscroll, MAX(cursel - (onscr - 1), 0));

	if (lastscroll == curscroll)
		return GO_FASTDRAW;
	return GO_REDRAW;
}

static int movecursor(int n)
{
	return shiftcursor(n, 0);
}

static int movequarterpage(int n)
{
	return shiftcursor(n * MAX(2, onscr >> 2), 0);
}

static int scrollpage(int n)
{
	int step = n * MAX(2, onscr - 1);
	return shiftcursor(step, step);
}

static int scrolleighth(int n)
{
	int step = n * MAX(1, (ndents + 3) >> 3);
	return shiftcursor(step, step);
}

static int movetoedge(int n)
{
	return shiftcursor(n * ndents, 0);
}

static void savehiststat(Histstat *hs)
{
	if (ndents > 0) {
		memccpy(hs->name, pdents[cursel].name, '\0', NAME_MAX);
		hs->cur = cursel;
		hs->scrl = curscroll;
	}
}

static Histpath *inithistpath(Histpath *hp, const char *path)
{
	const char *name = NULL;
	struct stat sb;

	if (lstat(path, &sb) == -1 && seterrnum(__LINE__, errno))
		return NULL;
	if (hp->path == path)
		return hp;
	if (!S_ISDIR(sb.st_mode))
		name = xbasename(path);

	char *end = memccpy(hp->path, path, '\0', PATH_MAX - 1);
	if (name) {
		xdirname(hp->path);
		end -= 2;
	}

	// Each level of path corresponds to a histstat. Add one more for current level
	hp->nhs = 0;
	for (char *p = hp->path; p < end; ++p) {
		if (*p == '/' || (*p == '\0' && path[1] != '\0')) {
			if (hp->nhs == hp->ths) {
				Histstat *tmphs = realloc(hp->hs, (hp->ths += HSTAT_INCR) * sizeof(Histstat));
				if (!tmphs && seterrnum(__LINE__, errno)) {
					free(hp->hs);
					memset(hp, 0, sizeof(Histpath));
					return NULL;
				}
				hp->hs = tmphs;
			}
			memset((hp->hs + hp->nhs++), 0, sizeof(Histstat));
		}
	}

	hp->stat = hp->hs + hp->nhs - 1;
	hp->stat->flag = S_VIS;
	if (name) {
		memccpy(hp->stat->name, name, '\0', NAME_MAX);
		findname = hp->stat->name;
		if (*name == '.')
			gcfg.showhidden = 1;
	}
	return hp;
}

static int newhistpath(const char *path, int force)
{
	Histpath *hp = ptab->hp;
	Histpath *hp2 = ((hp - ghpath) & 1) ? hp - 1 : hp + 1;

	if (strcmp(hp->path, path) == 0 || (gcfg.ct == TABS_MAX && !force))
		return GO_NONE;

	if (!inithistpath(hp2, path) || (chdir(hp2->path) == -1 && seterrnum(__LINE__, errno)))
		return GO_STATBAR;

	if (hp->stat->flag == S_ROOT)
		hp2->stat->flag = S_SUBROOT;

	savehiststat(hp->stat);
	ptab->hp = hp2;
	return GO_RELOAD;
}

static int switchhistpath(int n)
{
	Histpath *hp = ptab->hp;
	Histpath *hp2 = ((hp - ghpath) & 1) ? hp - 1 : hp + 1;

	if ((gcfg.ct == TABS_MAX && n == 0) || !hp2->path[0] || chdir(hp2->path) == -1)
		return GO_NONE;

	savehiststat(hp->stat);
	findname = hp2->stat->name;
	ptab->hp = hp2;
	return GO_RELOAD;
}

static int enterdir(int n)
{
	Histpath *hp = ptab->hp;
	Histpath *hp2 = ((hp - ghpath) & 1) ? hp - 1 : hp + 1;
	Histstat *hs = hp->stat;
	unsigned int nhs = hs - hp->hs + 1;
	char *newpath = gpbuf;

	if (ndents == 0)
		return GO_NONE;

	Entry *ent = &pdents[cursel];
	makepath(hp->path, ent->name, newpath);
	if (!(ent->flag & E_DIR_DIRLNK)) {
		if (n == 1 || gcfg.openfile == 1)
			spawn(opener, gpbuf, NULL, TRUE);
		return GO_STATBAR;
	}

	if (hs->flag == S_ROOT) {
		if (strcmp(newpath, hp2->path) == 0)
			return switchhistpath(1);
		else
			return newhistpath(newpath, TRUE);
	}

	if (nhs == hp->ths) {
		Histstat *tmphs = realloc(hp->hs, (hp->ths += HSTAT_INCR) * sizeof(Histstat));
		if (!tmphs && seterrnum(__LINE__, errno)) {
			hp->ths -= HSTAT_INCR;
			return GO_STATBAR;
		}
		hp->hs = tmphs;
	}

	if (chdir(newpath) == -1 && seterrnum(__LINE__, errno))
		return GO_STATBAR;

	if (nhs < hp->nhs) {
		if (strcmp(ent->name, hs->name) != 0) {
			if ((strcmp(hp->path, hp2->path) == 0 && strcmp(ent->name, hp2->stat->name) == 0)
			|| (gcfg.ct < TABS_MAX && inithistpath(hp2, hp->path)))
				hp = hp2;
			else
				hp->nhs = nhs;
		}
		findname = (hp->stat + 1)->name;
	}

	hp->stat = hp->hs + nhs;
	if (nhs == hp->nhs) {
		memset(hp->stat, 0, sizeof(Histstat));
		hp->stat->flag = S_VIS;
		++hp->nhs;
	}

	savehiststat(hp->stat - 1);
	memccpy(hp->path, newpath, '\0', PATH_MAX - 1);
	ptab->hp = hp;
	return GO_RELOAD;
}

static int gotoparent(int n __attribute__((unused)))
{
	char *path = ptab->hp->path;
	Histstat *hs = ptab->hp->stat;

	if ((path[0] == '/' && path[1] == '\0') || hs->flag == S_ROOT)
		return GO_NONE;

	if (hs->flag == S_SUBROOT)
		return switchhistpath(1);

	savehiststat(hs);
	do {
		--hs;
		if (hs->flag == S_UNVIS) {
			memccpy(hs->name, xbasename(path), '\0', NAME_MAX);
			hs->flag = S_VIS;
		}
	} while (chdir(xdirname(path)) == -1 && path[1] != '\0' && hs->flag != S_SUBROOT);

	findname = hs->name;
	ptab->hp->stat = hs;
	return GO_RELOAD;
}

static int gotohome(int n __attribute__((unused)))
{
	return newhistpath(home ? home : "/", FALSE);
}

static int refreshview(int n)
{
	if (ndents > 0) {
		savehiststat(ptab->hp->stat);
		findname = ptab->hp->stat->name;
	}

	if (n == 1) {
		gcfg.marknew ^= 1;
		gcfg.refresh = 1;
	} else if (n == 2)
		return GO_SORT;
	return GO_RELOAD;
}

static struct selstat *addselstat(struct selstat *ss, const char *path)
{
	struct selstat *n = malloc(sizeof(struct selstat));

	if (!n && seterrnum(__LINE__, errno))
		return NULL;

	n->nbuf = calloc(NAME_INCR, 1);
	if (!n->nbuf && seterrnum(__LINE__, errno)) {
		free(n);
		return NULL;
	}

	if (ss) {
		while (ss->next)
			ss = ss->next;
		ss->next = n;
	}
	n->prev = ss;
	n->next = NULL;

	memccpy(n->path, path, '\0', PATH_MAX);
	n->endp = n->nbuf;
	n->buflen = NAME_INCR;
	return n;
}

static void deleteselstat(struct selstat *ss)
{
	if (!ss)
		return;

	ptab->ss = NULL;
	if (ss->prev) {
		ss->prev->next = ss->next;
		ptab->ss = ss->prev;
	}
	if (ss->next) {
		ss->next->prev = ss->prev;
		ptab->ss = ss->next;
	}

	free(ss->nbuf);
	free(ss);
	ptab->cfg.havesel = 0;
	if (!ptab->ss)
		ptab->cfg.mansel = 0;
}

static void deleteallselstat(struct selstat *ss)
{
	struct selstat *tmp;

	if (!ss)
		return;

	while (ss->next)
		ss = ss->next;

	while (ss) {
		tmp = ss->prev;
		free(ss->nbuf);
		free(ss);
		ss = tmp;
	}
}

static struct selstat *getselstat(void)
{
	struct selstat *ss = ptab->ss;

	if (ndents == 0)
		return NULL;

	if (!ptab->cfg.havesel) {
		ss = addselstat(ss, ptab->hp->path);
		if (ss)
			ptab->cfg.havesel = 1;
		ptab->ss = ss;
	}
	return ss;
}

static int appendselection(Entry *ent)
{
	size_t len;
	struct selstat *ss = getselstat();

	if (!ss)
		return FALSE;

	len = ss->endp - ss->nbuf;
	if (ent->nlen >= ss->buflen - len) {
		char *tmp = realloc(ss->nbuf, ss->buflen += NAME_INCR);
		if (!tmp && seterrnum(__LINE__, errno)) {
			ss->buflen -= NAME_INCR;
			return FALSE;
		}
		ss->nbuf = tmp;
		ss->endp = len + ss->nbuf;
	}

	memccpy(ss->endp, ent->name, '\0', ent->nlen);
	ss->endp += ent->nlen;
	ent->flag |= E_SEL;
	++ptab->nsel;
	ptab->cfg.mansel = 1;
	return TRUE;
}

static char *findinbuf(char *buf, size_t len, char *name, int nlen)
{
	char *dst, *src = buf;

	for (;;) {
		dst = memmem(src, len, name, nlen);
		if (!dst)
			return NULL;
		if (dst == buf || dst[-1] == '\0')
			return dst;
		src += nlen; // found name as a substring of another name, move forward
		if (src >= buf + len)
			return NULL;
	}
}

static void removeselection(Entry *ent)
{
	char *dst, *src;
	struct selstat *ss = ptab->ss;

	if (!ss || !ptab->cfg.havesel)
		return;

	dst = findinbuf(ss->nbuf, ss->endp - ss->nbuf, ent->name, ent->nlen);
	if (!dst)
		return;

	src = dst + ent->nlen;
	memmove(dst, src, ss->endp - src);
	ss->endp -= ent->nlen;
	if (ss->endp <= ss->nbuf)
		deleteselstat(ss);

	ent->flag &= ~E_SEL;
	--ptab->nsel;
}

static int toggleselection(int n)
{
	if (ndents == 0)
		return GO_NONE;

	if (pdents[cursel].flag & E_SEL)
		removeselection(&pdents[cursel]);
	else
		appendselection(&pdents[cursel]);
	return shiftcursor(n, 0);
}

static int selectall(int n __attribute__((unused)))
{
	for (int i = 0; i < ndents; ++i)
		if (!(pdents[i].flag & E_SEL))
			appendselection(&pdents[i]);
	return GO_REDRAW;
}

static int invertselection(int n __attribute__((unused)))
{
	struct selstat *ss = getselstat();
	int mansel = ptab->cfg.mansel;

	if (!ss)
		return GO_STATBAR;

	ss->endp = ss->nbuf;
	for (int i = 0; i < ndents; ++i) {
		if (pdents[i].flag & E_SEL) {
			pdents[i].flag &= ~E_SEL;
			--ptab->nsel;
		} else if (i != cursel || mansel)
			appendselection(&pdents[i]);
	}

	if (ss->endp == ss->nbuf)
		deleteselstat(ss);
	return GO_REDRAW;
}

static int selectrange(int n)
{
	int step = (cursel >= markent) ? 1 : -1;

	if (ndents == 0)
		return GO_NONE;

	if (markent == -1) {
		markent = cursel;
		ptab->cfg.mansel = 1;
		return GO_FASTDRAW;
	}

	if (n > 0) {
		for (int i = markent; (step == 1) ? i <= cursel : i >= cursel; i += step)
			if (!(pdents[i].flag & E_SEL))
				appendselection(&pdents[i]);
	} else {
		for (int i = markent; (step == 1) ? i <= cursel : i >= cursel; i += step)
			if (pdents[i].flag & E_SEL)
				removeselection(&pdents[i]);
	}

	markent = -1;
	if (!ptab->ss)
		ptab->cfg.mansel = 0;
	return GO_REDRAW;
}

static int clearselection(int n __attribute__((unused)))
{
	deleteallselstat(ptab->ss);
	ptab->ss = NULL;
	ptab->nsel = 0;
	ptab->cfg.havesel = 0;
	ptab->cfg.mansel = 0;
	markent = -1;
	for (int i = 0; i < ptab->nde; ++i)
		pdents[i].flag &= ~E_SEL;
	return GO_REDRAW;
}

static int setfilter(int n)
{
	static Histstat *hs = NULL;

	switch (n) {
	case 1: // turn on or activate filter
		if (ptab->ftlen == 0) { // ftlen=0 no filter, ftlen<0 inactive, ftlen>0 active
			ptab->ftlen = 1;
			ptab->filt[0] = '\0';
			hs = ptab->hp->stat;
		} else if (ptab->ftlen < 0)
			ptab->ftlen = -ptab->ftlen;
		break;

	case 2: // turn off filter when path changed
		if (hs == ptab->hp->stat)
			return GO_NONE;
		// fallthrough
	case 0: // turn off filter
		ptab->ftlen = 0;
		ndents = ptab->nde;
	}
	return GO_REDRAW;
}

static int quickfind(int n __attribute__((unused)))
{
	if (ptab->fdlen <= 0) { // fdlen=0 no quick find, fdlen<0 invisible, fdlen>0 active
		ptab->find[0] = '\0';
		ptab->fdlen = 1;
	}
	return GO_REDRAW;
}

static int qfindnext(int n)
{
	int sta = (n == 0) ? 0 : cursel + n;

	if (ptab->fdlen == 0 || ptab->find[0] == '\0')
		return GO_NONE;

	n = (n == 0) ? 1 : n;
	for (int i = sta; i >= 0 && i < ndents; i += n) {
		if (strcasestr(pdents[i].name, ptab->find)) {
			cursel = i;
			curscroll = MAX(i - (onscr * 3 >> 2), MIN(i - (onscr >> 2), curscroll));
			return GO_REDRAW;
		}
	}
	return GO_REDRAW;
}

static int inittab(const char *path, int n)
{
	deleteallselstat(gtab[n].ss);
	gtab[n].ss = NULL;

	gtab[n].hp = inithistpath(&ghpath[n * 2], path);
	if (!gtab[n].hp)
		return FALSE;

	if (n == TABS_MAX)
		gtab[n].hp->stat->flag = S_ROOT;

	gtab[n].ftlen = gtab[n].fdlen = 0;
	gtab[n].nde = gtab[n].nsel = 0;
	gtab[n].cfg = gcfg;
	gtab[n].cfg.enabled = 1;
	return TRUE;
}

static int switchtab(int n)
{
	Histpath *hp = ptab->hp;

	if (n == gcfg.ct)
		return GO_NONE;

	if (gtab[n].cfg.enabled == 0 && !inittab(hp->path, n) && !inittab(home ? home : "/", n))
		return GO_STATBAR;

	hp->stat->cur = cursel;
	hp->stat->scrl = curscroll;
	if (gcfg.ct < TABS_MAX)
		gcfg.lt = gcfg.ct;
	gcfg.ct = n;
	if (chdir(gtab[n].hp->path) == -1)
		seterrnum(__LINE__, errno);
	return GO_RELOAD;
}

static int closetab(int n __attribute__((unused)))
{
	int ct = gcfg.ct, lt = -1;

	for (int i = 0; i < TABS_MAX; ++i)
		if (i != ct && gtab[i].cfg.enabled == 1)
			lt = i;

	if (gcfg.lt != ct && gtab[gcfg.lt].cfg.enabled == 1)
		lt = gcfg.lt;

	if (lt == -1) {
		if (ct == 0)
			return GO_NONE;
		if (!inittab(home ? home : "/", 0) || (chdir(home ? home : "/") == -1 && seterrnum(__LINE__, errno)))
			return GO_STATBAR;
		gcfg.ct = 0;
	} else {
		if (chdir(gtab[lt].hp->path) == -1)
			seterrnum(__LINE__, errno);
		gcfg.ct = lt;
	}

	if (ct == TABS_MAX) {
		free(pfindbuf);
		pfindbuf = pfindend = NULL;
	} else
		gcfg.lt = ct;

	deleteallselstat(gtab[ct].ss);
	gtab[ct].ss = NULL;
	gtab[ct].cfg.enabled = 0;
	return GO_RELOAD;
}

static int togglemode(int n __attribute__((unused)))
{
	if (gcfg.runmode == 2)
		return GO_NONE;
	gcfg.runmode ^= 1;
	return GO_FASTDRAW;
}

static int getinput(WINDOW *w)
{
	int c, i, tmp;

	c = wgetch(w);
	if (c == ESC) { // alt+key or esc
		wtimeout(w, 0);
		for (i = 0; (tmp = wgetch(w)) != ERR; ++i)
			c = tmp;
		wtimeout(w, -1);

		if (i == 1 && c > 31 && c < 127)
			c = -c;
		else if (i > 0) // when i=0, keep c=ESC
			c = 0;
	}
	return (c == ERR) ? 0 : c;
}

static void setcolumns(char *cols, int c)
{
	for (char *p = cols; *p; ++p)
		if (*p == c || *p == c - 32)
			*p = *p > 96 ? *p - 32 : *p + 32;
}

static int viewoptions(int n __attribute__((unused)))
{
	int i = 0, c = 0, h = MIN(20, xlines), w = MIN(50, xcols);
	Settings *cfg = &ptab->cfg;
	WINDOW *dpo = newpad(h, w);

	werase(dpo);
	box(dpo, 0, 0);
	mvwaddstr(dpo, 0, 6, " View Options ");
	mvwaddstr(dpo, i += 2, 2, "[.]");
	wattron(dpo, cfg->showhidden ? A_REVERSE : 0); waddstr(dpo, "show hidden");
	wattrset(dpo, A_NORMAL); waddstr(dpo, "  [/]");
	wattron(dpo, cfg->dirontop ? A_REVERSE : 0); waddstr(dpo, "dirs on top");

	wattrset(dpo, A_NORMAL); mvwaddstr(dpo, i += 2, 2, "Sort by:");
	mvwaddstr(dpo, ++i, 2, "  (n)");
	wattron(dpo, (cfg->sortby == 0) ? A_REVERSE : 0); waddstr(dpo, "name");
	wattrset(dpo, A_NORMAL); waddstr(dpo, "  (s)");
	wattron(dpo, (cfg->sortby == 1) ? A_REVERSE : 0); waddstr(dpo, "size");
	wattrset(dpo, A_NORMAL); waddstr(dpo, "  (t)");
	wattron(dpo, (cfg->sortby == 2) ? A_REVERSE : 0); waddstr(dpo, "time");
	wattrset(dpo, A_NORMAL); waddstr(dpo, "  (e)");
	wattron(dpo, (cfg->sortby == 3) ? A_REVERSE : 0); waddstr(dpo, "extension");
	wattrset(dpo, A_NORMAL); mvwaddstr(dpo, i += 2, 2, "  [v]");
	wattron(dpo, cfg->natural ? A_REVERSE : 0); waddstr(dpo, "natural");
	wattrset(dpo, A_NORMAL); waddstr(dpo, "  [r]");
	wattron(dpo, cfg->reverse ? A_REVERSE : 0); waddstr(dpo, "reverse");

	wattrset(dpo, A_NORMAL); mvwaddstr(dpo, i += 2, 2, "Detail info:");
	mvwaddstr(dpo, ++i, 2, "  [i]");
	wattron(dpo, strchr(cfg->cols, 't') ? A_REVERSE : 0); waddstr(dpo, "time");
	wattrset(dpo, A_NORMAL); waddstr(dpo, "  [u]");
	wattron(dpo, strchr(cfg->cols, 'o') ? A_REVERSE : 0); waddstr(dpo, "owner");
	wattrset(dpo, A_NORMAL); waddstr(dpo, "  [p]");
	wattron(dpo, strchr(cfg->cols, 'p') ? A_REVERSE : 0); waddstr(dpo, "permissions");
	wattrset(dpo, A_NORMAL); waddstr(dpo, "  [y]");
	wattron(dpo, strchr(cfg->cols, 's') ? A_REVERSE : 0); waddstr(dpo, "size");
	wattrset(dpo, A_NORMAL); mvwaddstr(dpo, i += 2, 2, "  (d)default  (x)none");

	wattrset(dpo, A_NORMAL); mvwaddstr(dpo, i += 2, 2, "Time type:");
	mvwaddstr(dpo, ++i, 2, "  (a)");
	wattron(dpo, (cfg->timetype == 0) ? A_REVERSE : 0); waddstr(dpo, "access");
	wattrset(dpo, A_NORMAL); waddstr(dpo, "  (m)");
	wattron(dpo, (cfg->timetype == 1) ? A_REVERSE : 0); waddstr(dpo, "modify");
	wattrset(dpo, A_NORMAL); waddstr(dpo, "  (c)");
	wattron(dpo, (cfg->timetype == 2) ? A_REVERSE : 0); waddstr(dpo, "change");
	wattrset(dpo, A_NORMAL); mvwaddstr(dpo, i += 2, 2, "Press 'o' or Esc to close");

	while (c == 0) {
		prefresh(dpo, 0, 0, (xlines - h) / 2, (xcols - w) / 2, (xlines - h) / 2 + h, (xcols - w) / 2 + w);
		c = getinput(dpo);
		switch (c) {
		case '.': cfg->showhidden ^= 1;
			break;
		case '/': cfg->dirontop ^= 1;
			break;

		case 'n': cfg->sortby = 0;
			break;
		case 's': cfg->sortby = 1;
			break;
		case 't': cfg->sortby = 2;
			break;
		case 'e': cfg->sortby = 3;
			break;
		case 'v': cfg->natural ^= 1;
			break;
		case 'r': cfg->reverse ^= 1;
			break;

		case 'i': setcolumns(cfg->cols, 't');
			break;
		case 'u': setcolumns(cfg->cols, 'o');
			break;
		case 'p': setcolumns(cfg->cols, 'p');
			break;
		case 'y': setcolumns(cfg->cols, 's');
			break;
		case 'd': memccpy(cfg->cols, gcfg.cols, '\0', 5);
			break;
		case 'x': for (char *p = cfg->cols; *p; ++p)
				*p = (*p > 96 && *p != 'n') ? *p - 32 : *p;
			break;

		case 'a': cfg->timetype = 0;
			break;
		case 'm': cfg->timetype = 1;
			break;
		case 'c': cfg->timetype = 2;
			break;
		case 'o':
			break;
		case ESC:
			break;
		default: c = 0;
		}
	}

	delwin(dpo);
	if (c == ESC || strchr("oiupydx", c))
		return GO_REDRAW;
	return refreshview(strchr(".amc", c) ? 0 : 2);
}

static int prefixkey(int n __attribute__((unused)))
{
	attrset(A_NORMAL);
	mvaddstr(xlines - 2, 0, "Key for extension function:    ");
	timeout(2000);
	int ctl = GO_REDRAW, c = getinput(stdscr);
	timeout(-1);
	if (c > 31)
		ctl = callextfunc(c);
	return (ctl < GO_REDRAW) ? GO_REDRAW : ctl;
}

static int showhelp(int n __attribute__((unused)))
{
	int klines = (int)LENGTH(keys), plines = klines + 8;
	WINDOW *help = newpad(plines, 80);

	keypad(help, TRUE);
	erase();
	refresh();
	waddstr(help, "sff "VERSION"\n\n"
			" Builtin functions:\n");

	for (int i = 0; i < klines; ++i)
		wprintw(help, "  %s\n", keys[i].cmnt);

	waddstr(help, "\nNote: File operations are implemented by extension functions\n"
			"For help with that, press Alt+'/' or 'u'-'/' in main view\n"
			"Press 'q' or Esc to leave this page");

	for (int c = 0, start = 0; c != ESC && c != 'q'; ) {
		getmaxyx(stdscr, xlines, xcols);
		start = MAX(0, MIN(start, plines - xlines));
		prefresh(help, start, 0, 0, 0, xlines - 1, xcols - 1);

		c = getinput(help);
		if (c == KEY_UP || c == 'k')
			--start;
		else if (c == KEY_DOWN || c == 'j')
			++start;
		else if (c == KEY_PPAGE || c == CTRL('B'))
			start -= xlines - 1;
		else if (c == KEY_NPAGE || c == CTRL('F'))
			start += xlines - 1;
	}

	delwin(help);
	return GO_REDRAW;
}

static int quitsff(int n __attribute__((unused)))
{
	return GO_QUIT;
}

/****** Core Functions ******/

static void usage(void)
{
	printf("sff "VERSION"\n\n"
		"Usage: sff [OPTIONS] [PATH]\n\n"
		"Options:\n"
		" -d        use ls-style date format\n"
		" -H        show hidden files\n"
		" -l <keys> set column order: (uppercase to hide)\n"
		"           't'ime, 'o'wner, 'p'erm, 's'ize, 'n'ame\n"
		" -m        mix directories and files when sorting\n"
		" -o        open files on right arrow or 'l' key\n"
		" -p        show permissions as symbolic strings\n"
		" -h        display this help and exit\n");
}

static int xstrverscasecmp(const char *s1, const char *s2)
{
	static unsigned char lowertb[256] = {0};
	const unsigned char *p1 = (const unsigned char *)s1, *p2 = (const unsigned char *)s2;
	int isdig1, isdig2, diff = 0, indig = 0;

	if (s1 == s2)
		return 0;

	if (lowertb[1] == 0) {
		for (int i = 0; i < 256; ++i)
			lowertb[i] = i;
		for (int i = 'A'; i <= 'Z'; ++i)
			lowertb[i] = i + 32;
	}

	for (unsigned int c1, c2; diff == 0 || indig; ++p1, ++p2) {
		c1 = *p1;
		c2 = *p2;

		if (indig) {
			isdig1 = c1 - '0' < 10;
			isdig2 = c2 - '0' < 10;
			if (isdig1 & isdig2) { // c1 and c2 are both digits
				if (diff == 0)
					diff = c1 - c2;
				continue;
			}
			if (isdig1) // c1 is digit and c2 is not
				return 1;
			if (isdig2) // c2 is digit and c1 is not
				return -1;
			if (diff) // both are not digits
				return diff;
			indig = 0;
		}

		indig = (c1 - '1' < 9) & (c2 - '1' < 9); // c1 and c2 are both 1-9
		diff = lowertb[c1] - lowertb[c2];
		if (c1 == '\0' || c2 == '\0')
			break;
	}

	while ((const char *)p1 > s1 && ((*(--p1) | *(--p2)) & 0xC0) == 0x80);
	// Let strcoll() handle non-ASCII and letters. Must pass ASCII 123-127 to strcoll for correct sorting
	if ((*p1 > 122 || *p2 > 122) && lowertb[*p1] > 96 && lowertb[*p2] > 96)
		return strcoll((const char *)p1, (const char *)p2);
	return diff ? diff : strcoll(s1, s2);
}

static int entrycmp(const void *va, const void *vb)
{
	const Entry *pa = (Entry *)va, *pb = (Entry *)vb;
	int fa = pa->flag & E_DIR_DIRLNK, fb = pb->flag & E_DIR_DIRLNK;
	const char *exta, *extb;

	if (ptab->cfg.dirontop && fa != fb) { // Dirs on top
		if (fb)
			return 1;
		return -1;
	}

	switch (ptab->cfg.sortby) {
	case 1:	// Sort by size
		if (pa->size > pb->size)
			return 1;
		if (pa->size < pb->size)
			return -1;
		break;

	case 2: // Sort by time
		if (pb->sec > pa->sec)
			return 1;
		if (pb->sec < pa->sec)
			return -1;
		if (pb->nsec > pa->nsec)
			return 1;
		if (pb->nsec < pa->nsec)
			return -1;
		break;

	case 3: // Sort by extension
		exta = fa ? NULL : getextension(pa->name, pa->nlen);
		extb = fb ? NULL : getextension(pb->name, pb->nlen);
		if (exta || extb) {
			if (!extb)
				return 1;
			if (!exta)
				return -1;
			int res = strcasecmp(exta, extb);
			if (res)
				return res;
		}
	}

	if (ptab->cfg.natural)
		return xstrverscasecmp(pa->name, pb->name);
	return strcoll(pa->name, pb->name);
}

static int reventrycmp(const void *va, const void *vb)
{
	const Entry *pa = (Entry *)va, *pb = (Entry *)vb;
	int fa = pa->flag & E_DIR_DIRLNK, fb = pb->flag & E_DIR_DIRLNK;

	if (ptab->cfg.dirontop && fa != fb) { // Dirs on top
 		if (fb)
			return 1;
		return -1;
	}
	return -entrycmp(va, vb);
}

static void setpreview(int op)
{
	static int fd = -1;

	switch (op) {
	case 0: // open preview
		if (fd == -1) {
			if (!pvfifo && seterrnum(__LINE__, ENOENT))
				return;
			fd = open(pvfifo, O_WRONLY|O_NONBLOCK|O_CLOEXEC);
			if (fd == -1 && seterrnum(__LINE__, errno))
				return;
		}

		// fallthrough
	case 1: // send file path
		if (fd == -1 || ndents == 0)
			return;

		int len = makepath(ptab->hp->path, pdents[cursel].name, gpbuf);
		gpbuf[len - 1] = '\n';
		if (write(fd, gpbuf, len) == len)
			return;
		seterrnum(__LINE__, errno);

		// fallthrough
	case 2: // close preview
		if (fd != -1) {
			close(fd);
			fd = -1;
		}
		if (pvfifo)
			unlink(pvfifo);
	}
}

static int writeselection(int fd)
{
	ssize_t len;
	struct selstat *ss;
	int selcur = !ptab->cfg.mansel && ndents > 0;

	if (selcur && !appendselection(&pdents[cursel]))
		return FALSE;

	ss = ptab->ss;
	while (ss && ss->prev)
		ss = ss->prev;

	while (ss && errline == 0) {
		for (char *pos = ss->nbuf, *end; pos < ss->endp && (end = memchr(pos, '\0', PATH_MAX)); pos = end + 1) {
			len = makepath(ss->path, pos, gpbuf);
			if (write(fd, gpbuf, len) != len && seterrnum(__LINE__, errno))
				break;
		}
		ss = ss->next;
	}

	if (selcur)
		clearselection(0);
	return (errline == 0) ? TRUE : FALSE;
}

static int readfindresult(int fd)
{
	ssize_t len = 1;
	size_t buflen = 0, reslen = 0;

	while (len > 0) {
		if (buflen - reslen < NAME_INCR) {
			char *tmp = realloc(pfindbuf, buflen += NAME_INCR);
			if (!tmp && seterrnum(__LINE__, errno)) {
				len = -1;
				break;
			}
			pfindbuf = tmp;
		}

		len = read(fd, pfindbuf + reslen, NAME_INCR);
		reslen += len;
	}

	if (len == -1 && seterrnum(__LINE__, errno)) {
		free(pfindbuf);
		pfindbuf = NULL;
		return FALSE;
	}

	pfindend = pfindbuf + reslen;
	*pfindend = '\0';
	return TRUE;
}

static int handlepipedata(int fd, int op)
{
	if (op == 0 && read(fd, &op, 1) == -1 && seterrnum(__LINE__, errno))
		return GO_STATBAR;

	switch (op) {
	case '.': // clear selection
		return clearselection(0);

	case '*': // refresh
		if (read(fd, &op, 1) == 1)
			clearselection(0);
		return refreshview(0);

	case '@': // select specified file
		if (read(fd, gpbuf, PATH_MAX) == -1 && seterrnum(__LINE__, errno))
			return GO_STATBAR;
		memccpy(ptab->hp->stat->name, xbasename(gpbuf), '\0', NAME_MAX);
		findname = ptab->hp->stat->name;
		ptab->hp->stat->cur = cursel;
		ptab->hp->stat->scrl = curscroll;
		clearselection(0);
		return GO_RELOAD;

	case '>': // enter specified path
		if (read(fd, gpbuf, PATH_MAX) == -1 && seterrnum(__LINE__, errno))
			return GO_STATBAR;
		if (gpbuf[0] == '/')
			return newhistpath(gpbuf, FALSE);
		break;

	case '?': // load search result
		if (!readfindresult(fd))
			return GO_STATBAR;
		if (!inittab(ptab->hp->path, TABS_MAX))
			return GO_STATBAR;
		switchtab(TABS_MAX);
		return GO_RELOAD;

	case '#': // set preview
		if (read(fd, &op, 1) == -1 && seterrnum(__LINE__, errno))
			return GO_STATBAR;
		if (op == 'p')
			setpreview(0);
		else if (op == 'q')
			setpreview(2);
		return GO_FASTDRAW;
	}
	return GO_REDRAW;
}

static int callextfunc(int c)
{
	pid_t pid, gpid = 0;
	int fd, len, ctl = GO_STATBAR;
	struct sigaction oldsigtstp, oldsigwinch;
	char *args[5] = {sudoer, extfunc, pipepath, (char [2]){c, '\0'}, NULL};
	char **argv = (gcfg.runmode == 1) ? &args[0] : &args[1];

	if ((!cfgpath || !extfunc || !pipepath) && seterrnum(__LINE__, ENOENT))
		return GO_STATBAR;

	if (access(cfgpath, F_OK) == -1) {
		memccpy(gpbuf, cfgpath, '\0', PATH_MAX);
		xdirname(gpbuf);
		if (mkdir(gpbuf, 0700) == -1 && errno != EEXIST && seterrnum(__LINE__, errno))
			return GO_STATBAR;
		if (mkdir(cfgpath, 0700) == -1 && seterrnum(__LINE__, errno))
			return GO_STATBAR;
	}
	if (mkfifo(pipepath, 0600) == -1 && errno != EEXIST && seterrnum(__LINE__, errno))
		return GO_STATBAR;

	endwin();
	pid = fork();
	if (pid > 0) {
		sigaction(SIGTSTP, &(struct sigaction){.sa_handler = SIG_IGN}, &oldsigtstp);
		sigaction(SIGWINCH, &(struct sigaction){.sa_handler = SIG_IGN}, &oldsigwinch);
		if ((fd = open(pipepath, O_RDONLY)) != -1) { // Blocking can be interrupted by SIGCHLD (set in initsff)
			if (read(fd, gpbuf, 1) == 1) {
				if (isdigit(gpbuf[0]) && (len = read(fd, &gpbuf[1], 9)) != -1) {
					gpbuf[len + 1] = '\0';
					gpid = (pid_t)strtol(gpbuf, NULL, 10);
				} else
					ctl = handlepipedata(fd, gpbuf[0]);
			}
			close(fd);

			if (gpid > 9 && (fd = open(pipepath, O_WRONLY)) != -1) {
				if (!writeselection(fd))
					kill(gpid, SIGTERM);
				close(fd);
			}
			if (gpid > 9 && (fd = open(pipepath, O_RDONLY)) != -1) {
				ctl = handlepipedata(fd, 0);
				close(fd);
			}
		} else if (errno != EINTR)
			seterrnum(__LINE__, errno);
		waitpid(pid, NULL, 0);
		sigaction(SIGTSTP, &oldsigtstp, NULL);
		sigaction(SIGWINCH, &oldsigwinch, NULL);

	} else if (pid == 0) {
		sigaction(SIGTSTP, &(struct sigaction){.sa_handler = SIG_IGN}, NULL);
		sigaction(SIGINT, &(struct sigaction){.sa_handler = SIG_DFL}, NULL);
		sigaction(SIGPIPE, &(struct sigaction){.sa_handler = SIG_DFL}, NULL);
		execvp(*argv, argv);
		_exit(EXIT_SUCCESS);

	} else
		seterrnum(__LINE__, errno);
	refresh();
	return ctl;
}

#ifdef __APPLE__
#define STVNSEC(X)  X##timespec.tv_nsec
#else
#define STVNSEC(X)  X##tim.tv_nsec
#endif

static void fillentry(int fd, Entry *ent, struct stat sb)
{
	switch (ptab->cfg.timetype) {
	case 0: ent->sec = sb.st_atime;
		ent->nsec = (unsigned int)STVNSEC(sb.st_a);
		break;
	case 1: ent->sec = sb.st_mtime;
		ent->nsec = (unsigned int)STVNSEC(sb.st_m);
		break;
	case 2: ent->sec = sb.st_ctime;
		ent->nsec = (unsigned int)STVNSEC(sb.st_c);
	}

	ent->size = sb.st_size;
	ent->mode = sb.st_mode;
	ent->uid = sb.st_uid;
	ent->gid = sb.st_gid;
	ent->flag = 0;

	switch (ent->mode & S_IFMT) {
	case S_IFREG: ent->type = F_REG;
		if (sb.st_nlink > 1)
			ent->type = F_HLNK;
		if (sb.st_mode & S_IXUSR)
			ent->type = F_EXEC;
		ent->flag |= E_REG_FILE;
		break;

	case S_IFDIR: ent->type = F_DIR;
		ent->flag |= E_DIR_DIRLNK;
		break;
	case S_IFLNK: ent->type = F_LNK;
		fstatat(fd, ent->name, &sb, 0);
		if (S_ISDIR(sb.st_mode))
			ent->flag |= E_DIR_DIRLNK;
		break;

	case S_IFCHR: ent->type = F_CHR;
		break;
	case S_IFBLK: ent->type = F_BLK;
		break;
	case S_IFIFO: ent->type = F_IFO;
		break;
	case S_IFSOCK: ent->type = F_SOCK;
		break;
	default: ent->type = F_UNKN;
	}

	if (gcfg.marknew && (curtime - sb.st_ctime < 300))
		ent->flag |= E_NEW;
}

static void loaddirentry(DIR *dirp, int fd)
{
	char *name, *tmp;
	size_t off = 0;
	struct dirent *dp;
	struct stat sb;
	Entry *ent, *tmpent;

	while ((dp = readdir(dirp))) {
		name = dp->d_name;

		if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
			continue;  // Skip self and parent
		if (name[0] == '.' && !ptab->cfg.showhidden)
			continue;
		if (fstatat(fd, name, &sb, AT_SYMLINK_NOFOLLOW) == -1)
			continue;

		if (ndents == tdents) {
			tmpent = realloc(pdents, (tdents += ENTRY_INCR) * sizeof(Entry));
			if (!tmpent && seterrnum(__LINE__, errno)) {
				tdents -= ENTRY_INCR;
				return;
			}
			pdents = tmpent;
		}

		if (namebuflen - off <= NAME_MAX) {
			tmp = realloc(pnamebuf, namebuflen += NAME_INCR);
			if (!tmp && seterrnum(__LINE__, errno)) {
				namebuflen -= NAME_INCR;
				return;
			}

			// Reset entry names if realloc() causes memory move
			if (pnamebuf != tmp) {
				pnamebuf = tmp;
				for (int i = 0; i < ndents; tmp += pdents[i].nlen, ++i)
					pdents[i].name = tmp;
			}
		}

		ent = pdents + ndents;
		ent->name = pnamebuf + off;
		tmp = memccpy(ent->name, name, '\0', NAME_MAX + 1);
		ent->nlen = tmp - ent->name; // include terminational '\0'
		off += ent->nlen;

		fillentry(fd, ent, sb);
		++ndents;
	}
}

static void loadsrchentry(int fd)
{
	struct stat sb;
	Entry *ent, *tmpent;

	for (char *name = pfindbuf, *end; name < pfindend && (end = memchr(name, '\0', PATH_MAX)); name = end + 1) {
		if (fstatat(fd, name, &sb, AT_SYMLINK_NOFOLLOW) == -1)
			continue;

		if (ndents == tdents) {
			tmpent = realloc(pdents, (tdents += ENTRY_INCR) * sizeof(Entry));
			if (!tmpent && seterrnum(__LINE__, errno)) {
				tdents -= ENTRY_INCR;
				return;
			}
			pdents = tmpent;
		}

		ent = pdents + ndents;
		ent->name = name;
		ent->nlen = end - name + 1;

		fillentry(fd, ent, sb);
		++ndents;
	}
}

static void loadentries(const char *path)
{
	ndents = 0;
	curtime = time(NULL);
	DIR *dirp = opendir(path);
	if (!dirp && seterrnum(__LINE__, errno))
		return;

	int fd = dirfd(dirp);
	if (ptab->hp->stat->flag != S_ROOT)
		loaddirentry(dirp, fd); // Load dir entry
	else if (pfindbuf)
		loadsrchentry(fd); // Load search result

	closedir(dirp);
	ptab->nde = ndents;
}

static void setcurrentstat(Histpath *hp, struct selstat *ss)
{
	Histstat *hs = hp->stat;

	// Find current entry, and set cursel
	if (findname) {
		if (hs->cur >= ndents || strcmp(findname, pdents[hs->cur].name) != 0) {
			for (int i = 0; i < ndents; ++i) {
				if (strcmp(findname, pdents[i].name) == 0) {
					hs->cur = i;
					hs->scrl = MAX(i - (onscr * 3 >> 2), MIN(i - (onscr >> 2), hs->scrl));
					break;
				}
			}
		}
		findname = NULL;
	}
	cursel = hs->cur;
	curscroll = hs->scrl;

	// Find corresponding selstat, and set selection status
	ptab->cfg.havesel = 0;
	markent = -1;
	if (!ss)
		return;
	while (ss->next)
		ss = ss->next;

	do {
		if (strcmp(ss->path, hp->path) == 0) {
			ptab->cfg.havesel = 1;
			ptab->ss = ss;
			break;
		}
	} while ((ss = ss->prev));
}

static int xmbstowcs(wchar_t *dst, const char *str, int maxcols)
{
	wchar_t *wcp = dst;
	int nb, wcw = 1, dstwidth = 0;

	for (wchar_t wc; *str; ++str, wcw = 1) {
		if ((signed char)*str < 0) {
			if ((nb = mbtowc(&wc, str, MB_CUR_MAX)) > 0) {
				*wcp = wc;
				str += nb - 1;
			} else
				*wcp = L'\uFFFD'; // invalid char
			if ((wcw = wcwidth(*wcp)) == -1) // Skip non-printable chars
				continue;
		} else if (*str < 0x20) {
			*wcp = L'?'; // Replace escape chars with '?'
		} else
			*wcp = (wchar_t)*str;

		if ((dstwidth += wcw) > maxcols) {
			if (wcp != dst)
				*(wcp - 1) = L'~';
			break;
		}
		++wcp;
	}
	*wcp = L'\0';
	return dstwidth;
}

static wchar_t *fitnamecols(const char *name, int maxcols)
{
	wchar_t *wbuf = (wchar_t *)gpbuf;

	if (maxcols <= 0)
		*wbuf = L'\0';
	else
		xmbstowcs(wbuf, name, maxcols);
	return wbuf;
}

static wchar_t *fitpathcols(const char *path, int maxcols)
{
	wchar_t *wbuf = (wchar_t *)gpbuf, *wbp = wbuf;

	if (maxcols <= 0) {
		*wbuf = L'\0';

	} else if (xmbstowcs(wbp, path, PATH_MAX) + (wbp - wbuf) > maxcols) {
		++wbp; // When fold path, keep the first level
		for (wchar_t *tbp = wbp, *slash = NULL; *tbp; ++tbp, ++wbp) {
			if (*tbp == L'/') {
				if (slash)
					slash = wbp = slash + 2;
				else
					slash = wbp;
			}
			*wbp = *tbp;
		}

		int i, len;
		for (i = len = wbp - wbuf; wcswidth(wbuf, i) > maxcols; --i) // Reduce wide chars to fit room
			;
		if (i < len)
			wbuf[i - 1] = L'~';
		wbuf[i] = L'\0';
	}
	return wbuf;
}

static char *filetypechar(int type)
{
	switch (type) {
	case F_DIR: return "<d>";
	case F_CHR: return "<c>";
	case F_BLK: return "<b>";
	case F_IFO: return "<p>";
	case F_LNK: return "<l>";
	case F_SOCK: return "<s>";
	case F_UNKN: return "<?>";
	}
	return "<->";
}

static void printenttime(const time_t *timep, int useabbr)
{
	static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	struct tm t, now;

	localtime_r(timep, &t);
	if (useabbr) {
		localtime_r(&curtime, &now);
		if (t.tm_year == now.tm_year)
			printw(" %s %2d %02d:%02d ", months[t.tm_mon], t.tm_mday, t.tm_hour, t.tm_min);
		else
			printw(" %s %2d  %s ", months[t.tm_mon], t.tm_mday, xitoa(t.tm_year + 1900));
	} else
		printw(" %s-%02d-%02d %02d:%02d ", xitoa(t.tm_year + 1900), t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
}

static void printent(const Entry *ent, int sel, int mark)
{
	int x, y;
	int attr1 = sel ? 0 : COLOR_PAIR(C_DETAIL); // for details
	int attr2 = A_BOLD | (mark || (sel && ptab->cfg.mansel) ? COLOR_PAIR(C_STATBAR) | A_REVERSE // for marks
				: (gcfg.marknew && (ent->flag & E_NEW) ? COLOR_PAIR(C_NEWFILE) | A_REVERSE : 0));
	int attr3 = COLOR_PAIR(ent->type) // for filename
				| (ent->flag & E_DIR_DIRLNK ? A_BOLD : 0)
				| ((ent->flag & E_SEL) || (sel && !ptab->cfg.mansel) ? A_REVERSE : 0)
				| (sel && ptab->cfg.mansel ? A_UNDERLINE : 0);

	attrset(attr1);
	for (char *p = ptab->cfg.cols; *p; ++p) {
		switch (*p) {
		case 'n': addch((sel ? '>' : ' ') | attr2);
			getyx(stdscr, y, x);
			attrset(attr3);
			if (ptab->hp->stat->flag != S_ROOT)
				addwstr(fitnamecols(ent->name, ncols));
			else
				addwstr(fitpathcols(ent->name, ncols));
			move(y, x + ncols);
			attrset(attr1);
			break;
		case 's': printw("%7s ", (ent->flag & E_REG_FILE) ? tohumansize(ent->size) : filetypechar(ent->type));
			break;
		case 't': printenttime(&ent->sec, gcfg.abbrdate);
			break;
		case 'p': if (gcfg.symbperm)
				printw(" %c%s ", filetypechar(ent->type)[1], strperms(ent->mode));
			else
				printw(" %c%c%c ", '0' + ((ent->mode >> 6) & 7), '0' + ((ent->mode >> 3) & 7), '0' + (ent->mode & 7));
			break;
		case 'o': printw("%7.6s:%-7.6s", getpwname(ent->uid), getgrname(ent->gid));
		}
	}
}

static void redraw(const char *path)
{
	static int homelen = 0;
	int dcols = 0, sp = 0, n = 0;

	for (char *p = ptab->cfg.cols; *p; ++p) {
		switch (*p) {
		case 'n': if (++n == 1)
				sp = dcols + 1;
			else
				*p = '@';
			break;
		case 's': dcols += 8;
			break;
		case 't': dcols += gcfg.abbrdate ? 14 : 18;
			break;
		case 'p': dcols += gcfg.symbperm ? 12 : 5;
			break;
		case 'o': dcols += 15;
		}
	}
	getmaxyx(stdscr, xlines, xcols);
	onscr = xlines - 4;
	ncols = xcols - dcols - 2;

	shiftcursor(0, 0);
	erase();
	if (gcfg.refresh) {
		refresh();
		gcfg.refresh = 0;
	}

	// Print tabs tag
	attrset(A_NORMAL);
	for (int i = 0; i <= TABS_MAX; ++i) {
		if (gtab[i].cfg.enabled == 1)
			addch((i < TABS_MAX ? i + '1' : '#')
			| (COLOR_PAIR(C_TABTAG) | (gcfg.ct == i ? A_REVERSE : 0) | A_BOLD));
		else
			addch(i < TABS_MAX ? '*' : '#');
		addch(' ');
	}

	// Print path
	n = xcols - (TABS_MAX + 1) * 2 - 1;
	if (homelen == 0 && home)
		homelen = strlen(home);
	attrset(COLOR_PAIR(C_PATHBAR) | A_BOLD);
	addch(' ');
	if (home && strncmp(home, path, homelen) == 0 && (path[homelen] == '/' || path[homelen] == '\0')) {
		path += homelen;
		--n;
		addch('~'); // Replace home path with '~'
	}
	addwstr(fitpathcols(path, n));

	// Print entries
	if (curscroll > 0 && ncols > 0) {
		attrset(COLOR_PAIR(C_DETAIL));
		mvaddstr(1, sp, "<<");
	}
	n = MIN(onscr + curscroll, ndents);
	for (int i = curscroll, j = 2; i < n; ++i, ++j) {
		if (ptab->cfg.havesel && !(pdents[i].flag & E_SEL_SCANED)) {
			if (findinbuf(ptab->ss->nbuf, ptab->ss->endp - ptab->ss->nbuf, pdents[i].name, pdents[i].nlen))
				pdents[i].flag |= E_SEL;
			pdents[i].flag |= E_SEL_SCANED;
		}
		move(j, 0);
		printent(&pdents[i], i == cursel, i == markent);
	}
	if (n < ndents && ncols > 0) {
		attrset(COLOR_PAIR(C_DETAIL));
		mvaddstr(xlines - 2, sp, ">>");
	}

	// Print filter
	if (ptab->ftlen != 0) {
		attrset(COLOR_PAIR(F_SOCK));
		mvaddstr(xlines - 2, 0, "Filter: ");
		addnstr(ptab->filt, xcols - 8);
		addch(' ' | (ptab->ftlen > 0 ? A_REVERSE : 0));
	}

	// Print quick find
	if (ptab->fdlen > 0) {
		attrset(COLOR_PAIR(F_EXEC));
		mvaddstr(xlines - 2, 0, "Quick find: ");
		addnstr(ptab->find, xcols - 12);
		addch(' ' | A_REVERSE);
	}

	// Draw scroll indicator
	sp = MAX(1, ndents);
	n = (sp <= onscr) ? onscr
		: ((onscr * onscr << 1) / sp + 1) >> 1; // indicator height, round a/b by (a*2/b+1)/2
	n = MAX(1, n);
	sp = (curscroll == 0 || sp <= onscr) ? 1
		: 1 + (((curscroll * (onscr - n) << 1) / (sp - onscr) + 1) >> 1); // starting row to drawing
	attrset(COLOR_PAIR(C_DETAIL));
	mvaddch(1, xcols - 1, '=');
	while (--n >= 0)
		mvaddch(++sp, xcols - 1, ' ' | A_REVERSE);
	mvaddch(xlines - 2, xcols - 1, '=');
	gcfg.redrawn = 1; // set to skip fastredraw
}

static void fastredraw(void)
{
	if (gcfg.redrawn || ndents == 0) { // bypass fastredraw after a full redraw
		gcfg.redrawn = 0;
		return;
	}
	if (lastsel >= curscroll && lastsel < onscr + curscroll && lastsel < ndents && lastsel != cursel) {
		move(2 + lastsel - curscroll, 0);
		printent(&pdents[lastsel], FALSE, lastsel == markent);
	}
	if (cursel >= curscroll && cursel < onscr + curscroll) {
		move(2 + cursel - curscroll, 0);
		printent(&pdents[cursel], TRUE, cursel == markent);
	}
}

static void statusbar(void)
{
	move(xlines - 1, 0);
	clrtoeol();

	if (errline != 0) {
		attrset(COLOR_PAIR(C_WARN));
		printw("Failed (%s): %s", xitoa(errline), strerror(errnum));
		errline = 0;
		return;
	}

	attrset(COLOR_PAIR(gcfg.runmode != 0 ? C_WARN : C_STATBAR));
	printw("%d/%d ", ndents > 0 ? cursel + 1 : 0, ndents);
	attron(A_REVERSE);
	printw(" %d ", (ndents > 0 && !ptab->cfg.mansel) ? 1 : ptab->nsel);
	attroff(A_REVERSE);

	int n, x;
	if (ndents > 0) {
		Entry *ent = &pdents[cursel];
		printw("  %c%s %s:%s  %s", filetypechar(ent->type)[1], strperms(ent->mode),
			getpwname(ent->uid), getgrname(ent->gid), tohumansize(ent->size));
		printenttime(&ent->sec, FALSE);

		getyx(stdscr, n, x);
		n = xcols - x;
		if (ent->type == F_LNK && n > 1) {
			char *p = &gpbuf[PATH_MAX * (sizeof(wchar_t) - 1) - 1]; // fitnamecols use gpbuf, so use last portion here
			if ((x = readlink(ent->name, p, PATH_MAX - 1)) > 1) {
				p[x] = '\0';
				addstr("->");
				addwstr(fitnamecols(p, n - 2)); // Show symlink target
			}

		} else if ((ent->flag & E_REG_FILE) && n > 2) {
			const char *p = getextension(ent->name, ent->nlen);
			if (p)
				addwstr(fitnamecols(p, n)); // Show file extension
		} else
			addch(' ');
	}

	getyx(stdscr, n, x);
	if (xcols - x > 7)
		mvaddstr(n, xcols - 7, "[?]help");
}

static void filterentry(void)
{
	Entry tmpent;

	if (ptab->ftlen == 0 || setfilter(2) == GO_REDRAW)
		return;

	for (int i = 0; i < ndents; ++i) {
		if (!strcasestr(pdents[i].name, ptab->filt) && i != --ndents) {
			tmpent = pdents[i];
			pdents[i] = pdents[ndents];
			pdents[ndents] = tmpent;
			--i;
		}
	}
}

static int filterinput(int c)
{
	if (ptab->ftlen <= 0) // ftlen=0 no filter, ftlen<0 inactive, ftlen>0 active
		return GO_NONE;

	if (c == '/') { // turn off filter
		setfilter(0);
		return refreshview(2);
	} else if (c == '\r' || c == ESC){ // set to inactive
		ptab->ftlen = (ptab->filt[0] == '\0') ? 0 : -ptab->ftlen;
		return GO_REDRAW;

	} else if (c == KEY_BACKSPACE || c == KEY_DC || c == 127) {
		if (ptab->ftlen <= 1)
			return GO_REDRAW;
		char *end = ptab->filt + ptab->ftlen - 1;
		while (--end >= ptab->filt && (*end & 0xC0) == 0x80);
		*end = '\0';
		ptab->ftlen = end - ptab->filt + 1;
		ndents = ptab->nde;

	} else if (c > 31 && c < 256) {
		ptab->filt[ptab->ftlen - 1] = c;
		ptab->filt[ptab->ftlen == FILT_MAX - 1 ? ptab->ftlen : ++ptab->ftlen - 1] = '\0';
	} else
		return GO_NONE;
	return refreshview(2);
}

static int qfindinput(int c)
{
	if (ptab->fdlen <= 0) // fdlen=0 no quick find, fdlen<0 invisible, fdlen>0 active
		return GO_NONE;

	if (c == '\r' || c == ESC) { // turn of or set to invisible
		ptab->fdlen = (ptab->find[0] == '\0') ? 0 : -ptab->fdlen;
		return GO_REDRAW;

	} else if (c == '/' && ptab->find[0] == '\0') { // go to root dir
		ptab->fdlen = 1;
		newhistpath("/", FALSE);
		return GO_RELOAD;

	} else if (c == '\t' || c == '/') { // enter dir
		if (enterdir(0) != GO_RELOAD)
			return GO_REDRAW;
		ptab->find[0] = '\0';
		ptab->fdlen = 1;
		return GO_RELOAD;

	} else if (c == KEY_BACKSPACE || c == KEY_DC || c == 127) {
		if (ptab->fdlen <= 1)
			return GO_REDRAW;
		char *end = ptab->find + ptab->fdlen - 1;
		while (--end >= ptab->find && (*end & 0xC0) == 0x80);
		*end = '\0';
		ptab->fdlen = end - ptab->find + 1;

	} else if (c > 31 && c < 256) {
		ptab->find[ptab->fdlen - 1] = c;
		ptab->find[ptab->fdlen == FILT_MAX - 1 ? ptab->fdlen : ++ptab->fdlen - 1] = '\0';
	} else
		return GO_NONE;

	if (ptab->find[0] == '\0')
		return GO_REDRAW;

	for (int i = 0; i < ndents; ++i) {
		if (strncasecmp(pdents[i].name, ptab->find, ptab->fdlen - 1) == 0) {
			cursel = i;
			curscroll = MAX(i - (onscr * 3 >> 2), MIN(i - (onscr >> 2), curscroll));
			return GO_REDRAW;
		}
	}
	return qfindnext(0);
}

static void browse(void)
{
	for (int c, ctl = GO_RELOAD;;) {
		switch (ctl) {
		case GO_RELOAD:
			ptab = &gtab[gcfg.ct];
			loadentries(ptab->hp->path);

			// fallthrough
		case GO_SORT:
			filterentry();
			qsort(pdents, ndents, sizeof(*pdents), ptab->cfg.reverse ? &reventrycmp : &entrycmp);
			setcurrentstat(ptab->hp, ptab->ss);

			// fallthrough
		case GO_REDRAW:
			redraw(ptab->hp->path);

			// fallthrough
		case GO_FASTDRAW:
			fastredraw();
			setpreview(1);

			// fallthrough
		case GO_STATBAR:
			statusbar();

			// fallthrough
		case GO_NONE:
			c = getinput(stdscr);
			if (c == KEY_RESIZE) {
				ctl = GO_REDRAW;
				break;
			}

			if ((ctl = filterinput(c)) != GO_NONE)
				break;
			if ((ctl = qfindinput(c)) != GO_NONE)
				break;

			if (c > 0) {
				for (size_t i = 0; i < LENGTH(keys); ++i)
					if ((c == keys[i].keysym1 || c == keys[i].keysym2) && keys[i].func)
						ctl = keys[i].func(keys[i].arg);
			} else if (c < 0)
				ctl = callextfunc(-c);

			break;
		case GO_QUIT:
			return;
		}
	}
}

static void exitsighandler(int sig __attribute__((unused)))
{
	endwin();
	exit(EXIT_SUCCESS);
}

static void childsighandler(int sig __attribute__((unused)))
{
	while (waitpid(-1, NULL, WNOHANG) > 0);
}

static int initsff(char *arg0, char *argx)
{
	// Reset standard input, ignore any pipe/redirected input
	if (!freopen("/dev/tty", "r", stdin)) {
		perror(xitoa(__LINE__));
		return FALSE;
	}

	// Handle certain signals
	sigaction(SIGHUP, &(struct sigaction){.sa_handler = exitsighandler}, NULL);
	sigaction(SIGTERM, &(struct sigaction){.sa_handler = exitsighandler}, NULL);
	sigaction(SIGCHLD, &(struct sigaction){.sa_handler = childsighandler}, NULL);
	sigaction(SIGINT, &(struct sigaction){.sa_handler = SIG_IGN}, NULL);
	sigaction(SIGQUIT, &(struct sigaction){.sa_handler = SIG_IGN}, NULL);
	sigaction(SIGPIPE, &(struct sigaction){.sa_handler = SIG_IGN}, NULL);

	// Get environment variables
	home = getenv("HOME");
	if (!home || !home[0] || access(home, R_OK | X_OK) == -1)
		home = NULL;

	opener = getenv("SFF_OPENER");
	if (!opener || !opener[0])
		opener = OPENER;

	sudoer = getenv("SFF_SUDOER");
	if (!sudoer || !sudoer[0])
		sudoer = SUDOER;

	// Set config path: XDG_CONFIG_HOME/sff or ~/.config/sff
	char *xdgcfg = getenv("XDG_CONFIG_HOME");
	if ((xdgcfg && xdgcfg[0] && makepath(xdgcfg, "sff", gpbuf))
	|| (home && makepath(home, ".config/sff", gpbuf)))
		cfgpath = strdup(gpbuf);

	// Set extfunc path, and check sff-extfunc file
	if ((cfgpath && makepath(cfgpath, EXTFNNAME, gpbuf) && access(gpbuf, R_OK | X_OK) == 0)
	|| (realpath(arg0, gpbuf) && xdirname(gpbuf) && makepath(gpbuf, EXTFNNAME, gpbuf) && access(gpbuf, R_OK | X_OK) == 0)
	|| (makepath(EXTFNPREFIX, EXTFNNAME, gpbuf) && access(gpbuf, R_OK | X_OK) == 0)
	|| (makepath(EXTFNPREFIX2, EXTFNNAME, gpbuf) && access(gpbuf, R_OK | X_OK) == 0))
		extfunc = strdup(gpbuf);

	// Set pipepath and pvfifo paths
	if (cfgpath && makepath(cfgpath, ".sff-pipe.", gpbuf))
		pipepath = strdup(strcat(gpbuf, xitoa(getpid())));
	if (pipepath)
		pvfifo = strdup(strcat(gpbuf, ".pv"));
	if (!cfgpath || !extfunc || !pipepath || !pvfifo)
		seterrnum(__LINE__, errno);

	// Initialize first tab
	if (!strchr(gcfg.cols, 'n'))
		memccpy(gcfg.cols + MIN(strlen(gcfg.cols), 4), "n", '\0', 2);
	if (!abspath(argx, gpbuf) || !inittab(gpbuf, 0) || chdir(ghpath[0].path) == -1) {
		perror(xitoa(__LINE__));
		return FALSE;
	}

	if (getuid() == 0)
		gcfg.runmode = 2;
	return TRUE;
}

static void setupcurses(void)
{
	cbreak();
	noecho();
	nonl();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
	set_escdelay(50);

	define_key("\033[1;5A", CTRL_UP);
	define_key("\033[1;5B", CTRL_DOWN);
	define_key("\033[1;2A", SHIFT_UP);
	define_key("\033[1;2B", SHIFT_DOWN);

	start_color();
	use_default_colors();
	if (COLORS >= 256)
		setcolorpair256();
	else
		setcolorpair8();
	getmaxyx(stdscr, xlines, xcols);
	onscr = xlines - 4;
}

static void cleanup(void)
{
	setpreview(2);
	if (pipepath)
		unlink(pipepath);
	for (int i = 0; i <= TABS_MAX; ++i) {
		free(ghpath[i * 2].hs);
		free(ghpath[i * 2 + 1].hs);
		deleteallselstat(gtab[i].ss);
	}

	free(pdents);
	free(pnamebuf);
	free(pfindbuf);
	free(cfgpath);
	free(extfunc);
	free(pipepath);
	free(pvfifo);
}

int main(int argc, char *argv[])
{
	for (int opt; (opt = getopt(argc, argv, "dHl:moph")) != -1;) {
		switch (opt) {
		case 'd': gcfg.abbrdate = 1;
			break;
		case 'H': gcfg.showhidden = 1;
			break;
		case 'l': memccpy(gcfg.cols, optarg, '\0', 5);
			break;
		case 'm': gcfg.dirontop = 0;
			break;
		case 'o': gcfg.openfile = 1;
			break;
		case 'p': gcfg.symbperm = 1;
			break;
		case 'h': usage();
			return EXIT_SUCCESS;
		default: usage();
			return EXIT_FAILURE;
		}
	}

	atexit(cleanup);

	if (!initsff(argv[0], argc == optind ? "" : argv[optind]))
		return EXIT_FAILURE;

	setlocale(LC_ALL, "");

	if (!initscr())
		return EXIT_FAILURE;
	setupcurses();

	browse();

	endwin();
	return EXIT_SUCCESS;
}
