/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2023-2025 Shi Yanling
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
#include <ctype.h>
#include <locale.h>
#include <wchar.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>

#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED
#endif
#include <curses.h>

#define VERSION         "0.9"
#ifndef PATH_MAX
#define PATH_MAX        4096
#endif
#ifndef NAME_MAX
#define NAME_MAX        255
#endif
#define TABS_MAX         4 // Number of tabs, the range of acceptable values is 1-7
#define ENTRY_INCR      128 // Number of Entry structures to allocate per shot
#define NAME_INCR       4096 // 128 entries * avg. 32 chars per name = 4KB
#define FILT_MAX        128 // Maximum length of filter string
#define HSTAT_INCR      16 // Number of Histstat structures to allocate each time
#ifndef EXTFNNAME
#define EXTFNNAME       "sff-extfunc"
#endif
#ifndef EXTFNPREFIX
#define EXTFNPREFIX     "/usr/local/libexec"
#endif

#define LENGTH(X)       (sizeof X / sizeof X[0])
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#define TOUPPER(ch)     (((ch) >= 'a' && (ch) <= 'z') ? ((ch) - 32) : (ch))
#define TOLOWER(ch)     (((ch) >= 'A' && (ch) <= 'Z') ? ((ch) + 32) : (ch))

enum entryflag {
	E_REG_FILE = 0x01, E_DIR_DIRLNK = 0x02,
	E_SEL = 0x04, E_SEL_SCANED = 0x08, E_NEW = 0x10
};

enum filetypes {
	F_REG, F_DIR, F_CHR, F_BLK, F_IFO, F_LNK, F_SOCK,
	F_HLNK, F_EXEC, F_EMPT, F_ORPH, F_MISS, F_UNKN
};

enum colorflag {
	C_DETAIL = F_UNKN + 1, C_TABTAG, C_PATHBAR, C_STATBAR, C_WARN, C_NEWFILE
};

enum histstatflag {
	S_UNVIS, S_VIS, S_ROOT, S_SUBROOT
};

enum procctrl {
	GO_NONE, GO_FASTDRAW, GO_STATBAR, GO_REDRAW, GO_SORT, GO_RELOAD, GO_QUIT
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
	char name[NAME_MAX + 1];
	int cur;
	int scrl;
	int flag;
} Histstat;

typedef struct {
	char path[PATH_MAX];
	Histstat *hs;
	Histstat *stat;
	int ths;
	int nhs;
} Histpath;

struct selstat {
	char path[PATH_MAX];
	struct selstat *prev;
	struct selstat *next;
	char *nbuf;
	char *endp;
	size_t buflen;
};

typedef struct {
	unsigned int enabled    : 1;
	unsigned int showhidden : 1;  // Show hidden files
	unsigned int dirontop   : 1;  // Sort directories on the top
	unsigned int sortby     : 3;  // (0: name, 1: size, 2: time, 3: extension)
	unsigned int casesens   : 1;  // Case sensitive
	unsigned int natural    : 1;  // Natural numeric sorting
	unsigned int reverse    : 1;  // Reverse sort
	unsigned int showtime   : 1;  // Show time info
	unsigned int showowner  : 1;  // Show user/group info
	unsigned int showperm   : 1;  // Show permission info
	unsigned int showsize   : 1;  // Show size info
	unsigned int timetype   : 2;  // (0: access, 1: modify, 2: change)
	unsigned int havesel    : 1;  // (0: no selection in current path, 1: have selection)
	unsigned int selmode    : 1;  // (0: normal mode, 1: selection mode)
	// global settings
	unsigned int ct         : 3;  // Current tab
	unsigned int lt         : 3;  // Last tab
	unsigned int mode       : 3;  // (0: normal, 1: sudo, 2: permanent sudo, 3: browse, 4: permanent browse)
	unsigned int newent     : 1;  // (0: do not mark new entry, 1: mark new entry)
	unsigned int misc       : 6;
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

static int ndents = 0, cursel = 0, lastsel = -1, curscroll = 0, lastscroll = -1;
static int markent = -1, errline = 0, errnum = 0;
static int xlines, xcols, onscr, ncols;
static char *home, *editor;
static char *cfgpath = NULL, *extfunc = NULL, *selpath = NULL, *pipepath = NULL;
static char *pnamebuf = NULL, *pfindbuf = NULL, *ppipebuf = NULL;
static char *findname = NULL;
static size_t findlen = 0, pipelen = 0;
static time_t curtime;
static Entry *pdents = NULL;
static Tabs *ptab = NULL;

alignas(max_align_t) static char gpbuf[PATH_MAX] = {0};
alignas(max_align_t) static char gmbuf[PATH_MAX] = {0};
alignas(max_align_t) static Tabs gtab[TABS_MAX + 1] = {0};
alignas(max_align_t) static Histpath ghpath[(TABS_MAX + 1) * 2] = {0};

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

/* Convert unsigned integer to string. The maximum value it can handle is 4,294,967,295
   This is a modified version of xitoa() from nnn. https://github.com/jarun/nnn */
static char *xitoa(unsigned int val)
{
	static char dst[16] = {0};
	static const char digits[204] =
		"0001020304050607080910111213141516171819"
		"2021222324252627282930313233343536373839"
		"4041424344454647484950515253545556575859"
		"6061626364656667686970717273747576777879"
		"8081828384858687888990919293949596979899";
	unsigned int i, j, quo;

	for (i = 14; val >= 100; --i) { // Fill digits backward from dst[14]
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

/* Get directory portion of pathname. Source would be modified!!! */
static char *xdirname(char *path)
{
	char *p = memrchr(path, '/', strlen(path));

	if (p == path)
		path[1] = '\0';
	else if (p)
		*p = '\0';
	return path;
}

/* Get filename portion of pathname. Source would be untouched. */
static char *xbasename(char *path)
{
	char *p = memrchr(path, '/', strlen(path));

	return p ? p + 1 : path;
}

/* Make path/name in buf. Ensure buf size is not less than PATH_MAX. */
static char *makepath(const char *path, const char *name, char *buf)
{
	if (!path || !buf)
		return NULL;

	char *p = (path == buf) ? (char *)memchr(path, '\0', PATH_MAX - 2) + 1
		: memccpy(buf, path, '\0', PATH_MAX - 2);

	if (p) {
		if (path[0] == '/' && path[1] == '\0')
			--p;
		else
			p[-1] = '/';
		p = memccpy(p, name, '\0', PATH_MAX - (p - buf));
	}

	if (p)
		return buf;
	errno = ENAMETOOLONG;
	return NULL;
}

/* Get file extension. Extensions longer than 7 chars will be ignored. */
static char *getextension(char *name, int len)
{
	char *p;

	if (len > 3) {
		p = name + len - 2;
		len = (len > 10) ? 8 : len - 2;

		 while (--len != 0)
			if (*--p == '.')
				return p;
	}
	return NULL;
}

/* Get the absolute pathname without resolving symlinks.
   Neither path nor buf can be NULL. Ensure buf size is not less than PATH_MAX. */
static char *abspath(const char *path, char *buf)
{
	const char *src;
	char *dst;
	size_t len = 0;

	if (!path || !buf)
		return NULL;

	if (path[0] != '/') {
		if (!getcwd(buf, PATH_MAX - 1))
			return NULL;
		len = strlen(buf);
	} else
		++path;

	if (len + strlen(path) + 2 > PATH_MAX) {
		errno = ENAMETOOLONG;
		return NULL;
	}

	src = path;
	dst = buf + len;
	*dst++ = '/';

	while (*src) {
		if (src[0] == '/' && (dst[-1] == '/' || src[1] == '/' || src[1] == '\0')) {
			++src;
			continue;
		} else if (dst[-1] == '/' && src[0] == '.' && (src[1] == '/' || src[1] == '\0')) {
			src = (src[1] == '\0') ? src + 1 : src + 2;
			continue;
		} else if (dst[-1] == '/' && src[0] == '.' && src[1] == '.' && (src[2] == '/' || src[2] == '\0')) {
			dst = (char *)memrchr(buf, '/', MAX(1, dst - buf - 1)) + 1;
			src = (src[2] == '\0') ? src + 2 : src + 3;
			continue;
		}
		*dst++ = *src++;
	}

	if (dst - 1 > buf && dst[-1] == '/')
		dst[-1] = '\0';
	else
		dst[0] = '\0';
	return buf;
}

/* Convert integer size to string like 6.2K 25.0M 198.3G etc. */
static char *tohumansize(off_t size)
{
	static char sbuf[12] = {0};
	static const char unit[12] = "BKMGTPEZY";
	char *sp;
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

	sp = (char *)memccpy(sbuf, xitoa(numint), '\0', 6) - 1;

	if (i > 0) {
		*sp++ = '.';
		*sp++ = '0' + frac;
	}

	*sp = unit[i];
	*++sp = '\0';
	return sbuf;
}

/* Convert inode permission info into a symbolic string, except the inode type. */
static char *strperms(mode_t mode)
{
	static char str[12] = {0};

	str[0] = mode & S_IRUSR ? 'r' : '-';
	str[1] = mode & S_IWUSR ? 'w' : '-';
	str[2] = (mode & S_ISUID
		? (mode & S_IXUSR ? 's' : 'S')
		: (mode & S_IXUSR ? 'x' : '-'));
	str[3] = mode & S_IRGRP ? 'r' : '-';
	str[4] = mode & S_IWGRP ? 'w' : '-';
	str[5] = (mode & S_ISGID
		? (mode & S_IXGRP ? 's' : 'S')
		: (mode & S_IXGRP ? 'x' : '-'));
	str[6] = mode & S_IROTH ? 'r' : '-';
	str[7] = mode & S_IWOTH ? 'w' : '-';
	str[8] = (mode & S_ISVTX
		? (mode & S_IXOTH ? 't' : 'T')
		: (mode & S_IXOTH ? 'x' : '-'));
	return str;
}

/* Returns the cached name if the provided uid is the same as the previous uid.
   This is a modified version of getpwname() from nnn. https://github.com/jarun/nnn */
static char *getpwname(uid_t uid)
{
	static unsigned int uidcache = -1;
	static char *namecache = NULL;

	if (uidcache != uid) {
		struct passwd *pw = getpwuid(uid);

		uidcache = uid;
		namecache = pw ? pw->pw_name : NULL;
	}
	return namecache ? namecache : xitoa(uid);
}

/* Returns the cached group if the provided gid is the same as the previous gid.
   This is a modified version of getgrname() from nnn. https://github.com/jarun/nnn */
static char *getgrname(gid_t gid)
{
	static unsigned int gidcache = -1;
	static char *grpcache = NULL;

	if (gidcache != gid) {
		struct group *gr = getgrgid(gid);

		gidcache = gid;
		grpcache = gr ? gr->gr_name : NULL;
	}
	return grpcache ? grpcache : xitoa(gid);
}

static bool seterrnum(int line, int err)
{
	errline = line;
	errnum = err;
	return TRUE;
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
static int openfile(int n);
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
static int showhelp(int n);
static int quitsff(int n);

#include "config.h" // Configuration

static void spawn(char *arg0, char *arg1, char *arg2, bool detach, bool sudo)
{
	pid_t pid;
	char *args[5] = {SUDOER, arg0, arg1, arg2, NULL};
	char **argv = sudo ? &args[0] : &args[1];
	struct sigaction oldsigtstp, oldsigwinch;
	struct sigaction act = {.sa_handler = SIG_IGN};

	pid = fork();
	if (pid > 0) {
		sigaction(SIGTSTP, &act, &oldsigtstp);
		sigaction(SIGWINCH, &act, &oldsigwinch);
		waitpid(pid, NULL, 0);
		sigaction(SIGTSTP, &oldsigtstp, NULL);
		sigaction(SIGWINCH, &oldsigwinch, NULL);

	} else if (pid == 0) {
		if (detach) {
			pid = fork(); // Fork a grandchild to detach
			if (pid > 0) {
				_exit(EXIT_SUCCESS);
			} else if (pid == 0) {
				setsid();
			} else {
				perror("fork failed");
				_exit(EXIT_FAILURE);
			}

			// Suppress stdout and stderr
			int fd = open("/dev/null", O_WRONLY, 0200);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			close(fd);
		}
		sigaction(SIGTSTP, &act, NULL);
		act.sa_handler = SIG_DFL;
		sigaction(SIGINT, &act, NULL);
		execvp(*argv, argv);
		_exit(EXIT_SUCCESS);

	} else
		seterrnum(__LINE__, errno);
}

static int shiftcursor(int step, int scrl)
{
	lastsel = cursel;
	lastscroll = curscroll;
	cursel = MAX(0, MIN(ndents - 1, cursel + step));

	if ((step == 1 || step == -1) && scrl == 0) {
		if ((cursel < curscroll + ((onscr + 2) / 4) && step < 0)
		|| (cursel >= curscroll + onscr - ((onscr + 2) / 4) && step > 0))
			curscroll += step;
	} else
		curscroll += scrl;
	curscroll = MIN(curscroll, MIN(cursel, ndents - onscr));
	curscroll = MAX(curscroll, MAX(cursel - (onscr - 1), 0));

	if (lastsel == cursel && lastscroll == curscroll)
		return GO_FASTDRAW;
	return GO_REDRAW;
}

static int movecursor(int n)
{
	return shiftcursor(n, 0);
}

static int movequarterpage(int n)
{
	return shiftcursor(((xlines - 4) / 4) * n, 0);
}

static int scrollpage(int n)
{
	int step = (xlines - 5) * n;
	return shiftcursor(step, step);
}

static int scrolleighth(int n)
{
	int step = ((ndents + 3) / 8) * n;
	return shiftcursor(step, step);
}

static int movetoedge(int n)
{
	return shiftcursor(ndents * n, 0);
}

static inline void inithiststat(Histstat *hs)
{
	hs->name[0] = hs->name[NAME_MAX] = '\0';
	hs->cur = hs->scrl = 0;
	hs->flag = S_UNVIS;
}

static inline void savehiststat(Histstat *hs)
{
	if (ndents > 0) {
		strncpy(hs->name, pdents[cursel].name, NAME_MAX);
		hs->cur = cursel;
		hs->scrl = curscroll;
	}
}

static Histpath *inithistpath(Histpath *hp, char *path, bool check)
{
	char *name = NULL;
	struct stat sb;

	if (check) {
		if (lstat(path, &sb) == -1 && seterrnum(__LINE__, errno)) {
			return NULL;
		} else if (!S_ISDIR(sb.st_mode)) {
			name = xbasename(path);
			xdirname(path);
		}

		if (chdir(path) == -1 && seterrnum(__LINE__, errno))
			return NULL;
	}

	// Each level of path corresponds to a histstat. Add one more for current level
	hp->nhs = 0;
	for (char *p = path, *p2 = hp->path; *p != '\0' || p != p2; ++p, ++p2) {
		if (*p == '/' || (*p == '\0' && path[1] != '\0')) {
			if (hp->nhs == hp->ths) {
				Histstat *tmp = realloc(hp->hs, (hp->ths += HSTAT_INCR) * sizeof(Histstat));
				if (!tmp && seterrnum(__LINE__, errno)) {
					hp->path[0] = '\0';
					hp->nhs = 0;
					hp->ths -= HSTAT_INCR;
					return NULL;
				}
				hp->hs = tmp;
			}
			inithiststat(hp->hs + hp->nhs++);
		}

		*p2 = *p;
		if (*p == '\0')
			p2 = --p;
	}

	hp->stat = hp->hs + hp->nhs - 1;
	hp->stat->flag = S_VIS;
	if (name){
		findname = strncpy(hp->stat->name, name, NAME_MAX + 1);
		if (*name == '.')
			gcfg.showhidden = 1;
	}
	return hp;
}

static int newhistpath(char *path)
{
	Histpath *hp = ptab->hp;
	Histpath *hp2 = ((hp - ghpath) & 1) ? hp - 1 : hp + 1;

	if (strcmp(hp->path, path) == 0)
		return GO_NONE;

	if (!inithistpath(hp2, path, TRUE))
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

	if ((gcfg.ct == TABS_MAX || !hp2->path[0]) && n == 0)
		return GO_NONE;

	if (chdir(hp2->path) == -1)
		return GO_STATBAR;

	savehiststat(hp->stat);
	findname = hp2->stat->name;
	ptab->hp = hp2;
	return GO_RELOAD;
}

static int enterdir(int n __attribute__((unused)))
{
	Histpath *hp = ptab->hp;
	Histpath *hp2 = ((hp - ghpath) & 1) ? hp - 1 : hp + 1;
	Histstat *hs = hp->stat;
	int nhs = hs - hp->hs + 1;
	Entry *ent = &pdents[cursel];
	char *newpath = makepath(hp->path, ent->name, gpbuf);

	if (ndents == 0 || !(ent->flag & E_DIR_DIRLNK))
		return GO_NONE;

	if (!newpath && seterrnum(__LINE__, errno))
		return GO_STATBAR;

	if (hs->flag == S_ROOT) {
		if (strcmp(newpath, hp2->path) == 0)
			return switchhistpath(1);
		else
			return newhistpath(newpath);
	}

	if (chdir(newpath) == -1 && seterrnum(__LINE__, errno))
		return GO_STATBAR;

	if (nhs == hp->ths) {
		Histstat *tmp = realloc(hp->hs, (hp->ths += HSTAT_INCR) * sizeof(Histstat));
		if (!tmp && seterrnum(__LINE__, errno)) {
			hp->ths -= HSTAT_INCR;
			return GO_STATBAR;
		}
		hp->hs = tmp;
	}

	if (nhs < hp->nhs) {
		if (strcmp(ent->name, hs->name) != 0) {
			if ((strcmp(hp->path, hp2->path) != 0 || strcmp(ent->name, hp2->stat->name) != 0)
			&& (gcfg.ct == TABS_MAX || !inithistpath(hp2, hp->path, FALSE))) {
				hp->nhs = nhs;
			} else {
				hp = hp2;
				hs = hp->stat;
			}
		}
		findname = (hs + 1)->name;
	}

	if (nhs == hp->nhs) {
		inithiststat(hs + 1);
		(hs + 1)->flag = S_VIS;
		++hp->nhs;
	}

	savehiststat(hs);
	strncpy(hp->path, newpath, PATH_MAX);
	hp->stat = hp->hs + nhs;
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
			strncpy(hs->name, xbasename(path), NAME_MAX + 1);
			hs->flag = S_VIS;
		}
	} while (path[1] != '\0' && chdir(xdirname(path)) == -1 && seterrnum(__LINE__, errno));

	findname = hs->name;
	ptab->hp->stat = hs;
	return GO_RELOAD;
}

static int gotohome(int n __attribute__((unused)))
{
	if (gcfg.ct == TABS_MAX)
		return GO_NONE;

	return newhistpath(home ? home : "/");
}

static int refreshview(int n)
{
	Histstat *hs = ptab->hp->stat;

	if (ndents > 0) {
		savehiststat(hs);
		findname = hs->name;
	}

	if (n == 1)
		gcfg.newent ^= 1;
	if (n == 2)
		return GO_SORT;
	return GO_RELOAD;
}

static int openfile(int n)
{
	Entry *ent = &pdents[cursel];

	if (ndents == 0)
		return GO_NONE;

	makepath(ptab->hp->path, ent->name, gpbuf);

	switch (n) {
	case 1:
		if (!(ent->flag & E_REG_FILE))
			return GO_NONE;
		endwin();
		spawn(editor, gpbuf, NULL, FALSE, gcfg.mode == 1);
		refresh();
		return refreshview(0);

	default :
		if (ent->flag & E_DIR_DIRLNK)
			return enterdir(0);
		spawn(OPENER, gpbuf, NULL, TRUE, FALSE);
	}
	return GO_STATBAR;
}

static struct selstat *addselstat(struct selstat *ss, char *path)
{
	struct selstat *n = malloc(sizeof(struct selstat));

	if (!n && seterrnum(__LINE__, errno))
		return NULL;

	n->nbuf = malloc(PATH_MAX);
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

	strncpy(n->path, path, PATH_MAX);
	n->endp = n->nbuf;
	n->buflen = PATH_MAX;
	return n;
}

static void deleteselstat(struct selstat *ss)
{
	if (!ss)
		return;

	gtab[gcfg.ct].ss = NULL;
	if (ss->prev) {
		ss->prev->next = ss->next;
		gtab[gcfg.ct].ss = ss->prev;
	}
	if (ss->next) {
		ss->next->prev = ss->prev;
		gtab[gcfg.ct].ss = ss->next;
	}

	free(ss->nbuf);
	free(ss);
	ptab->cfg.havesel = 0;
	if (!ptab->ss)
		ptab->cfg.selmode = 0;
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

	if (ptab->cfg.havesel == 0) {
		ss = addselstat(ss, ptab->hp->path);
		if (ss)
			ptab->cfg.havesel = 1;
		ptab->ss = ss;
	}
	return ss;
}

static bool appendselection(Entry *ent)
{
	size_t len;
	struct selstat *ss = getselstat();

	if (!ss)
		return FALSE;

	len = ss->endp - ss->nbuf;
	if (ent->nlen >= ss->buflen - len) {
		char *tmp = realloc(ss->nbuf, ss->buflen += PATH_MAX);
		if (!tmp && seterrnum(__LINE__, errno)) {
			ss->buflen -= PATH_MAX;
			return FALSE;
		}
		ss->nbuf = tmp;
		ss->endp = len + ss->nbuf;
	}

	strncpy(ss->endp, ent->name, ent->nlen);
	ss->endp += ent->nlen;
	ent->flag |= E_SEL;
	++gtab[gcfg.ct].nsel;
	if (!ptab->cfg.selmode)
		ptab->cfg.selmode = 1;
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
	--gtab[gcfg.ct].nsel;
}

static int toggleselection(int n)
{
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

	if (!ss)
		return GO_STATBAR;

	ss->endp = ss->nbuf;

	for (int i = 0; i < ndents; ++i) {
		if (pdents[i].flag & E_SEL) {
			pdents[i].flag &= ~E_SEL;
			--gtab[gcfg.ct].nsel;
		} else
			appendselection(&pdents[i]);
	}

	if (ss->endp == ss->nbuf)
		deleteselstat(ss);
	return GO_REDRAW;
}

static int selectrange(int n)
{
	int i, start, end;

	if (ndents == 0)
		return GO_NONE;

	if (markent == -1) {
		markent = cursel;
		ptab->cfg.selmode = 1;
		return shiftcursor(0, 0);
	}

	if (cursel > markent) {
		start = markent;
		end = cursel;
	} else {
		start = cursel;
		end = markent;
	}

	if (n > 0) {
		for (i = start; i <= end; ++i)
			if (!(pdents[i].flag & E_SEL))
				appendselection(&pdents[i]);
	} else {
		for (i = start; i <= end; ++i)
			if (pdents[i].flag & E_SEL)
				removeselection(&pdents[i]);
	}

	markent = -1;
	if (!ptab->ss)
		ptab->cfg.selmode = 0;
	return GO_REDRAW;
}

static int clearselection(int n __attribute__((unused)))
{
	deleteallselstat(ptab->ss);
	ptab->ss = NULL;
	ptab->nsel = 0;
	ptab->cfg.havesel = 0;
	ptab->cfg.selmode = 0;
	markent = -1;

	for (int i = 0; i < ptab->nde; ++i)
		if (pdents[i].flag & E_SEL)
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
	int i, sta = (n == 0) ? 0 : cursel + n;

	if (ptab->fdlen == 0 || ptab->find[0] == '\0')
		return GO_NONE;

	if (n >= 0) {
		for (i = sta; i < ndents; ++i) {
			if (strcasestr(pdents[i].name, ptab->find)) {
				cursel = i;
				curscroll = MAX(i - (onscr * 3 >> 2), MIN(i - (onscr >> 2), curscroll));
				return GO_REDRAW;
			}
		}
	} else {
		for (i = sta; i >= 0; --i) {
			if (strcasestr(pdents[i].name, ptab->find)) {
				cursel = i;
				curscroll = MAX(i - (onscr * 3 >> 2), MIN(i - (onscr >> 2), curscroll));
				return GO_REDRAW;
			}
		}
	}
	return GO_REDRAW;
}

static bool inittab(char *path, int n)
{
	deleteallselstat(gtab[n].ss);
	gtab[n].ss = NULL;

	gtab[n].hp = inithistpath(&ghpath[n * 2], path, TRUE);
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

	if (chdir(gtab[n].hp->path) == -1 && seterrnum(__LINE__, errno) && !inittab(home ? home : "/", n))
		return GO_STATBAR;

	hp->stat->cur = cursel;
	hp->stat->scrl = curscroll;
	if (gcfg.ct < TABS_MAX)
		gcfg.lt = gcfg.ct;
	gcfg.ct = n;
	return GO_RELOAD;
}

static int closetab(int n)
{
	int ac = -1;

	n = gcfg.ct;
	for (int i = 0; i < TABS_MAX; ++i)
		if (i != n && gtab[i].cfg.enabled == 1)
			ac = i;

	if (gcfg.lt != n && gtab[gcfg.lt].cfg.enabled == 1)
		ac = gcfg.lt;

	if (ac == -1) {
		if (n == 0)
			return GO_NONE;
		else if (!inittab(home ? home : "/", 0))
			return GO_STATBAR;
		gcfg.ct = 0;
	} else {
		if (chdir(gtab[ac].hp->path) == -1 && seterrnum(__LINE__, errno) && !inittab(home ? home : "/", ac))
			return GO_STATBAR;
		gcfg.ct = ac;
	}

	if (n == TABS_MAX)
		findlen = 0;
	deleteallselstat(gtab[n].ss);
	gtab[n].ss = NULL;
	gtab[n].cfg.enabled = 0;
	gcfg.lt = n;
	return GO_RELOAD;
}

static int togglemode(int n)
{
	int def = (getuid() == 0) ? 2 : 0;

	if (gcfg.mode == 4 || (def == 2 && n == 1))
		return GO_STATBAR;

	gcfg.mode = (gcfg.mode == n) ? def : n;
	return GO_REDRAW;
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

static int viewoptions(int n __attribute__((unused)))
{
	int i, c = 0;
	int h = MIN(20, xlines), w = MIN(50, xcols);
	Settings *cfg = &gtab[gcfg.ct].cfg;
	WINDOW *dpo = newpad(h, w);

	werase(dpo);
	box(dpo, 0, 0);
	mvwaddstr(dpo, i = 0, 6, " View options ");
	mvwaddstr(dpo, i += 2, 2, "[.]");
	wattron(dpo, cfg->showhidden ? A_REVERSE : 0); waddstr(dpo, "show hidden"); wattroff(dpo, A_REVERSE);
	waddstr(dpo, "  [/]");
	wattron(dpo, cfg->dirontop ? A_REVERSE : 0); waddstr(dpo, "dirs on top"); wattroff(dpo, A_REVERSE);

	mvwaddstr(dpo, i += 2, 2, "Sort by:");
	mvwaddstr(dpo, ++i, 2, "  (n)");
	wattron(dpo, (cfg->sortby == 0) ? A_REVERSE : 0); waddstr(dpo, "name"); wattroff(dpo, A_REVERSE);
	waddstr(dpo, "  (s)");
	wattron(dpo, (cfg->sortby == 1) ? A_REVERSE : 0); waddstr(dpo, "size"); wattroff(dpo, A_REVERSE);
	waddstr(dpo, "  (t)");
	wattron(dpo, (cfg->sortby == 2) ? A_REVERSE : 0); waddstr(dpo, "time"); wattroff(dpo, A_REVERSE);
	waddstr(dpo, "  (e)");
	wattron(dpo, (cfg->sortby == 3) ? A_REVERSE : 0); waddstr(dpo, "extension"); wattroff(dpo, A_REVERSE);
	mvwaddstr(dpo, i += 2, 2, "  [c]");
	wattron(dpo, cfg->casesens ? A_REVERSE : 0); waddstr(dpo, "case-sensitive"); wattroff(dpo, A_REVERSE);
	waddstr(dpo, "  [v]");
	wattron(dpo, cfg->natural ? A_REVERSE : 0); waddstr(dpo, "natural"); wattroff(dpo, A_REVERSE);
	waddstr(dpo, "  [r]");
	wattron(dpo, cfg->reverse ? A_REVERSE : 0); waddstr(dpo, "reverse"); wattroff(dpo, A_REVERSE);

	mvwaddstr(dpo, i += 2, 2, "Detail info:");
	mvwaddstr(dpo, ++i, 2, "  [i]");
	wattron(dpo, cfg->showtime ? A_REVERSE : 0); waddstr(dpo, "time"); wattroff(dpo, A_REVERSE);
	waddstr(dpo, "  [u]");
	wattron(dpo, cfg->showowner ? A_REVERSE : 0); waddstr(dpo, "owner"); wattroff(dpo, A_REVERSE);
	waddstr(dpo, "  [p]");
	wattron(dpo, cfg->showperm ? A_REVERSE : 0); waddstr(dpo, "permissions"); wattroff(dpo, A_REVERSE);
	waddstr(dpo, "  [z]");
	wattron(dpo, cfg->showsize ? A_REVERSE : 0); waddstr(dpo, "size"); wattroff(dpo, A_REVERSE);
	mvwaddstr(dpo, i += 2, 2, "  (d)default  (x)none");

	mvwaddstr(dpo, i += 2, 2, "Time type:");
	mvwaddstr(dpo, ++i, 2, "  (a)");
	wattron(dpo, (cfg->timetype == 0) ? A_REVERSE : 0); waddstr(dpo, "access"); wattroff(dpo, A_REVERSE);
	waddstr(dpo, "  (m)");
	wattron(dpo, (cfg->timetype == 1) ? A_REVERSE : 0); waddstr(dpo, "modify"); wattroff(dpo, A_REVERSE);
	waddstr(dpo, "  (h)");
	wattron(dpo, (cfg->timetype == 2) ? A_REVERSE : 0); waddstr(dpo, "change"); wattroff(dpo, A_REVERSE);
	mvwaddstr(dpo, i += 2, 2, "Press 'o' or Esc to close");

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
		case 'c': cfg->casesens ^= 1;
			break;
		case 'v': cfg->natural ^= 1;
			break;
		case 'r': cfg->reverse ^= 1;
			break;

		case 'i': cfg->showtime ^= 1;
			break;
		case 'u': cfg->showowner ^= 1;
			break;
		case 'p': cfg->showperm ^= 1;
			break;
		case 'z': cfg->showsize ^= 1;
			break;
		case 'd': cfg->showtime = gcfg.showtime;
			cfg->showowner = gcfg.showowner;
			cfg->showperm = gcfg.showperm;
			cfg->showsize = gcfg.showsize;
			break;
		case 'x': cfg->showtime = cfg->showowner = cfg->showperm = cfg->showsize = 0;
			break;

		case 'a': cfg->timetype = 0;
			break;
		case 'm': cfg->timetype = 1;
			break;
		case 'h': cfg->timetype = 2;
			break;
		case 'o':
			break;
		case ESC:
			break;
		default: c = 0;
		}
	}

	delwin(dpo);
	if (c == ESC || strchr("oiupzdx", c))
		return GO_REDRAW;
	return refreshview(strchr(".amh", c) ? 0 : 2);
}

static int showhelp(int n __attribute__((unused)))
{
	int i, c = 0, klines = (int)LENGTH(keys);
	int start = 0, plines = klines + 8;
	WINDOW *help = newpad(plines, 80);

	keypad(help, TRUE);
	erase();
	refresh();
	waddstr(help, "sff - simple file finder\n"
			"version "VERSION"\n\n"
			" Builtin functions:\n");

	for (i = 0; i < klines; ++i)
		wprintw(help, "  %s\n", keys[i].cmnt);

	waddstr(help, "\nNote: All file operations are implemented by extension functions.\n"
			"To get help for extension functions, press Alt-/ in the main view.\n"
			"Press 'q' or Esc to leave this page.");

	while (c != ESC && c != 'q') {
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
	printf("Usage: sff [OPTIONS] [PATH]\n\n"
		"Option    Meaning\n"
		"  -b      Run in browse mode\n"
		"  -c      Sort with case sensitivity\n"
		"  -d keys Show details: 't'ime, 'o'wner, 'p'erm, 's'ize, 'n'one\n"
		"  -H      Show hidden files\n"
		"  -m      Mix dirs and files when sorting\n"
		"  -v      Print version and exit\n"
		"  -h      Print this help and exit\n");
}

/* states: S_N: normal, S_I: comparing integral part, S_F: comparing
           fractionnal parts, S_Z: idem but with leading Zeroes only */
#define  S_N    0x0
#define  S_I    0x3
#define  S_F    0x6
#define  S_Z    0x9

/* result_type: CMP: return diff; LEN: compare using len_diff/diff */
#define  CMP    2
#define  LEN    3

/* This is a modified version of the GLIBC and uClibc implementation of strverscmp()
   https://elixir.bootlin.com/uclibc-ng/latest/source/libc/string/strverscmp.c */

/* Compare S1 and S2 as strings holding indices/version numbers,
   returning less than, equal to or greater than zero if S1 is less than,
   equal to or greater than S2 (for more info, see the texinfo doc). */
static int xstrverscmp (const char *s1, const char *s2, bool cs)
{
	const unsigned char *p1 = (const unsigned char *) s1;
	const unsigned char *p2 = (const unsigned char *) s2;

	/* Symbol(s)    0       [1-9]   others
	   Transition   (10) 0  (01) d  (00) x  */
	static const uint8_t next_state[] =
	{
		/* state    x    d    0  */
		/* S_N */  S_N, S_I, S_Z,
		/* S_I */  S_N, S_I, S_I,
		/* S_F */  S_N, S_F, S_F,
		/* S_Z */  S_N, S_F, S_Z
	};

	static const int8_t result_type[] =
	{
		/* state   x/x  x/d  x/0  d/x  d/d  d/0  0/x  0/d  0/0  */
		/* S_N */  CMP, CMP, CMP, CMP, LEN, CMP, CMP, CMP, CMP,
		/* S_I */  CMP, -1,  -1,  +1,  LEN, LEN, +1,  LEN, LEN,
		/* S_F */  CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
		/* S_Z */  CMP, +1,  +1,  -1,  CMP, CMP, -1,  CMP, CMP
	};
	unsigned char c1, c2;
	int state, diff;

	if (p1 == p2)
		return 0;

	if (cs) {
		c1 = *p1++;
		c2 = *p2++;
	} else {
		c1 = TOLOWER(*p1);
		c2 = TOLOWER(*p2);
		++p1;
		++p2;
	}

	/* Hint: '0' is a digit too.  */
	state = S_N + ((c1 == '0') + (isdigit (c1) != 0));
	while ((diff = c1 - c2) == 0) {
		if (c1 == '\0')
			return diff;

		state = next_state[state];
		if (cs) {
			c1 = *p1++;
			c2 = *p2++;
		} else {
			c1 = TOLOWER(*p1);
			c2 = TOLOWER(*p2);
			++p1;
			++p2;
		}
		state += (c1 == '0') + (isdigit (c1) != 0);
	}

	state = result_type[state * 3 + (((c2 == '0') + (isdigit (c2) != 0)))];
	switch (state)	{
	case CMP:
		return diff;

	case LEN:
		while (isdigit (*p1++))
			if (!isdigit (*p2++))
				return 1;
		return isdigit (*p2) ? -1 : diff;

	default:
		return state;
	}
}

static int xstrcasecmp (const char *s1, const char *s2)
{
	const unsigned char *p1 = (const unsigned char *)s1;
	const unsigned char *p2 = (const unsigned char *)s2;
	int diff = 0;

	if (p1 == p2)
		return 0;

	for (unsigned char c1 = 1, c2 = 1; diff == 0 && c1 && c2; ++p1, ++p2) {
		c1 = TOLOWER(*p1);
		c2 = TOLOWER(*p2);
		diff = c1 - c2;
	}
	return diff;
}

static int entrycmp(const void *va, const void *vb)
{
	const Entry *pa = (Entry *)va, *pb = (Entry *)vb;
	int fa = pa->flag & E_DIR_DIRLNK, fb = pb->flag & E_DIR_DIRLNK;
	char *exta, *extb;

	if (ptab->cfg.dirontop && fa != fb) { // Dirs on top
		if (fb)
			return 1;
		return -1;
	}

	switch (ptab->cfg.sortby) {
	case 1:	// Sort by size
		if (pb->size > pa->size)
			return 1;
		if (pb->size < pa->size)
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
			if (!exta)
				return -1;
			if (!extb)
				return 1;

			int res = strcasecmp(exta, extb);
			if (res)
				return res;
		}
	}

	if (ptab->cfg.natural)
		return xstrverscmp(pa->name, pb->name, ptab->cfg.casesens);

	if (ptab->cfg.casesens)
		return strcoll(pa->name, pb->name);
	return xstrcasecmp(pa->name, pb->name);
}

static int reventrycmp(const void *va, const void *vb)
{
	const Entry *pa = (Entry *)va, *pb = (Entry *)vb;
	int fa = pa->flag & E_DIR_DIRLNK, fb = pb->flag & E_DIR_DIRLNK;

	if (gtab[gcfg.ct].cfg.dirontop && fa != fb) { // Dirs on top
 		if (fb)
			return 1;
		return -1;
	}
	return -entrycmp(va, vb);
}

static bool writeselection(void)
{
	char *pos, *end;
	ssize_t plen, len;
	struct selstat *ss;
	bool selcur = !ptab->cfg.selmode && ndents > 0;

	if (selcur && !appendselection(&pdents[cursel]))
		return FALSE;

	int fd = open(selpath, O_CREAT | O_WRONLY | O_TRUNC, S_IWUSR | S_IRUSR);
	if (fd == -1 && seterrnum(__LINE__, errno))
		return FALSE;

	ss = ptab->ss;
	while (ss && ss->prev)
		ss = ss->prev;

	while (ss && errline == 0) {
		plen = strlen(ss->path);
		for (pos = ss->nbuf; (end = memchr(pos, '\0', PATH_MAX)) && end < ss->endp; pos += len) {
			len = end - pos + 1;

			if (write(fd, ss->path, plen) != plen)
				errline = __LINE__;
			if (*(ss->path + 1) != '\0' && write(fd, "/", 1) != 1)
				errline = __LINE__;
			if (write(fd, pos, len) != len)
				errline = __LINE__;

			if (errline != 0)
				break;
		}
		ss = ss->next;
	}

	close(fd);
	if (selcur)
		clearselection(0);
	if (errline != 0 && seterrnum(errline, errno))
		return FALSE;
	return TRUE;
}

static bool readpipe(int fd, char **buf, size_t *plen)
{
	ssize_t len;
	size_t buflen = 0;

	*plen = 0;
	do {
		if (buflen - *plen < NAME_INCR) {
			char *tmp = realloc(*buf, buflen += NAME_INCR);
			if (!tmp && seterrnum(__LINE__, errno))
				return FALSE;
			*buf = tmp;
		}

		len = read(fd, *buf + *plen, NAME_INCR);
		if (len > 0)
			*plen += len;
	} while (len != -1 && (*buf)[*plen - 1] != 036); // '\036' as terminating char

	if (len == -1 && seterrnum(__LINE__, errno)) {
		free(*buf);
		*buf = NULL;
		return FALSE;
	}
	(*buf)[*plen - 1] = '\0';
	return TRUE;
}

static int handlepipedata(int fd)
{
	int op = 0;

	read(fd, &op, 1);

	switch (op) {
	case '-': // clear selection
		return clearselection(0);

	case '*': // refresh
		if (read(fd, &op, 1) == 1)
			clearselection(0);
		return refreshview(0);

	case 'n': // select new file
		if (!readpipe(fd, &ppipebuf, &pipelen))
			return GO_STATBAR;
		if (ppipebuf)
			strncpy(ptab->hp->stat->name, xbasename(ppipebuf), NAME_MAX);
		findname = ptab->hp->stat->name;
		gcfg.newent = 1;
		clearselection(0);
		return GO_RELOAD;

	case '>': // enter specified path
		if (!readpipe(fd, &ppipebuf, &pipelen))
			return GO_STATBAR;
		if (abspath(ppipebuf, gpbuf))
			return newhistpath(gpbuf);
		break;

	case 'f': // load search result
		if (!readpipe(fd, &pfindbuf, &findlen))
			return GO_STATBAR;
		if (!inittab(ptab->hp->path, TABS_MAX))
			return GO_STATBAR;
		switchtab(TABS_MAX);
		return GO_RELOAD;
	}
	return GO_REDRAW;
}

static int callextfunc(int c)
{
	pid_t pid;
	int rfd, wfd, ctl = GO_STATBAR;

	if (access(cfgpath, F_OK) == -1 && mkdir(cfgpath, 0700) == -1 && seterrnum(__LINE__, errno))
		return GO_STATBAR;

	if (!writeselection())
		return GO_STATBAR;

	if (mkfifo(pipepath, 0600) == -1 && seterrnum(__LINE__, errno))
		return GO_STATBAR;

	endwin();
	pid = fork();
	if (pid > 0) {
		rfd = open(pipepath, O_RDONLY);
		if (rfd != -1) {
			ctl = handlepipedata(rfd); // Process is blocked here until all writers are closed
			waitpid(pid, NULL, 0);
			close(rfd);
		} else
			seterrnum(__LINE__, errno);

	} else if (pid == 0) {
		wfd = open(pipepath, O_WRONLY | O_CLOEXEC);
		if (wfd != -1) {
			spawn(extfunc, (char [2]){c, '\0'}, pipepath, FALSE, gcfg.mode == 1);
			close(wfd); // Unblock the parent process
		}
		_exit(EXIT_SUCCESS);

	} else
		seterrnum(__LINE__, errno);
	refresh();
	unlink(pipepath);
	return ctl;
}

static inline void fillentry(int fd, Entry *ent, struct stat sb)
{
	switch (ptab->cfg.timetype) {
	case 0: ent->sec = sb.st_atime;
		ent->nsec = (unsigned int)sb.st_atim.tv_nsec;
		break;
	case 1: ent->sec = sb.st_mtime;
		ent->nsec = (unsigned int)sb.st_mtim.tv_nsec;
		break;
	case 2: ent->sec = sb.st_ctime;
		ent->nsec = (unsigned int)sb.st_ctim.tv_nsec;
		break;
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

	if (gcfg.newent && ((curtime - sb.st_mtime <= 180) || (curtime - sb.st_ctime <= 180)))
		ent->flag |= E_NEW;
}

static void loaddirentry(DIR *dirp, int fd)
{
	char *name, *tmp;
	int totents = 0;
	size_t buflen = 0, off = 0;
	struct dirent *dp;
	struct stat sb;
	Entry *ent;

	while ((dp = readdir(dirp))) {
		name = dp->d_name;

		if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
			continue;  // Skip self and parent
		if (name[0] == '.' && !ptab->cfg.showhidden)
			continue;
		if (fstatat(fd, name, &sb, AT_SYMLINK_NOFOLLOW) == -1)
			continue;

		if (ndents == totents) {
			Entry *tent = realloc(pdents, (totents += ENTRY_INCR) * sizeof(Entry));
			if (!tent && seterrnum(__LINE__, errno))
				return;
			pdents = tent;
		}

		if (buflen - off <= NAME_MAX) {
			tmp = realloc(pnamebuf, buflen += NAME_INCR);
			if (!tmp && seterrnum(__LINE__, errno))
				return;

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
	char *name, *end = pfindbuf;
	int totents = 0;
	struct stat sb;
	Entry *ent;

	while ((name = end) && (end = memchr(name, '\0', PATH_MAX)) && ++end < pfindbuf + findlen) {
		if (fstatat(fd, name, &sb, AT_SYMLINK_NOFOLLOW) == -1)
			continue;

		if (ndents == totents) {
			Entry *tent = realloc(pdents, (totents += ENTRY_INCR) * sizeof(Entry));
			if (!tent && seterrnum(__LINE__, errno))
				return;
			pdents = tent;
		}

		ent = pdents + ndents;
		ent->name = name;
		ent->nlen = end - name;

		fillentry(fd, ent, sb);
		++ndents;
	}
}

static void loadentries(char *path)
{
	int fd;
	DIR *dirp = opendir(path);

	ndents = 0;
	if (!dirp && seterrnum(__LINE__, errno))
		return;
	fd = dirfd(dirp);

	curtime = time(NULL);

	if (ptab->hp->stat->flag != S_ROOT)
		loaddirentry(dirp, fd); // Load dir entry
	else
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

static wchar_t *fitnamecols(const char *str, int maxcols)
{
	wchar_t *wbuf = (wchar_t *)gpbuf;
	int i, len = mbstowcs(wbuf, str, maxcols + 1); // Convert multi-byte to wide char

	for (i = 0; i < len; ++i)
		if (wbuf[i] <= L'\x1f')
			wbuf[i] = L'?';  // Replace escape chars with '?'

	if (len > maxcols >> 1) {
		for (i = len; wcswidth(wbuf, i) > maxcols; --i) // Reduce wide chars to fit room
			;
		if (i < len)
			wbuf[i - 1] = L'~';
		wbuf[i] = L'\0';
	}
	return wbuf;
}

static wchar_t *fitpathcols(const char *path, int maxcols)
{
	static int homelen = 0;
	wchar_t *tbuf = (wchar_t *)gmbuf, *wbuf = (wchar_t *)gpbuf;
	wchar_t *tbp = tbuf, *wbp = wbuf, *slash = NULL;
	int i, len;
	bool fold = FALSE;

	if (homelen == 0 && home)
		homelen = strlen(home);
	// Replace home path with '~'
	if (home && strncmp(home, path, homelen) == 0) {
		path += homelen;
		*wbp++ = L'~';
	}

	len = mbstowcs(tbp, path, PATH_MAX);

	if (wcswidth(tbuf, len) + (wbp - wbuf) > maxcols) {
		fold = TRUE;
		*wbp++ = *tbp++; // If fold path, keep the first level
	}

	for (; *tbp; ++tbp, ++wbp) {
		if (fold && *tbp == L'/' && slash)
			slash = wbp = slash + 2;
		if (fold && *tbp == L'/' && !slash)
			slash = wbp;

		*wbp = (*tbp > L'\x1f') ? *tbp : L'?'; // Replace escape chars with '?'
	}

	len = wbp - wbuf;
	for (i = len; wcswidth(wbuf, i) > maxcols; --i) // Reduce wide chars to fit room
		;
	if (i < len)
		wbuf[i - 1] = L'~';
	wbuf[i] = L'\0';
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

static inline void printenttime(const time_t *timep)
{
	struct tm t;

	localtime_r(timep, &t);
	printw("%s-%02d-%02d %02d:%02d ", xitoa(t.tm_year + 1900), t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
}

static inline void printentname(const Entry *ent, bool sel)
{
	int attr = COLOR_PAIR(ent->type)
		| (ent->flag & E_DIR_DIRLNK ? A_BOLD : 0)
		| ((ent->flag & E_SEL) || (sel && !ptab->cfg.selmode) ? A_REVERSE : 0)
		| ((sel && ptab->cfg.selmode) ? A_UNDERLINE : 0);

	attron(attr);
	if (ptab->hp->stat->flag != S_ROOT)
		addwstr(fitnamecols(ent->name, ncols));
	else
		addwstr(fitpathcols(ent->name, ncols));
	attroff(attr);
}


static void printent(const Entry *ent, bool sel, bool mark)
{
	int attr = COLOR_PAIR(C_DETAIL)	| (mark || (sel && ptab->cfg.selmode) ? A_REVERSE : 0);

	if (sel)
		addch('>' | A_BOLD);
	else
		addch(' ');

	attron(attr);
	if (ptab->cfg.showtime)
		printenttime(&ent->sec);

	if (ptab->cfg.showowner)
		printw("%7.6s:%-7.6s", getpwname(ent->uid), getgrname(ent->gid));

	if (ptab->cfg.showperm)
		printw(" %c%c%c ", '0' + ((ent->mode >> 6) & 7), '0' + ((ent->mode >> 3) & 7), '0' + (ent->mode & 7));

	if (ptab->cfg.showsize)
		printw("%7s ", (ent->flag & E_REG_FILE) ? tohumansize(ent->size) : filetypechar(ent->type));

	attron(gcfg.newent && (ent->flag & E_NEW) ? (COLOR_PAIR(C_NEWFILE) | A_REVERSE) : 0);
	if (sel)
		addch('>' | A_BOLD);
	else
		addch(' ');
	attroff(gcfg.newent && (ent->flag & E_NEW) ? (COLOR_PAIR(C_NEWFILE) | A_REVERSE) : 0);
	attroff(attr);

	if (ncols > 0)
		printentname(ent, sel);
}

static void redraw(char *path)
{
	getmaxyx(stdscr, xlines, xcols);
	int pcols = xcols - (TABS_MAX + 1) * 2;
	int dcols = (ptab->cfg.showtime ? 17 : 0) + (ptab->cfg.showowner ? 15 : 0)
		+ (ptab->cfg.showperm ? 5 : 0) + (ptab->cfg.showsize ? 8 : 0) + 2;
	int btm, i, j = 0;
	struct selstat *ss = ptab->ss;

	erase();
	shiftcursor(0, 0);
	onscr = xlines - 4;
	ncols = xcols - dcols - 1;

	// Print tabs tag
	for (i = 0; i <= TABS_MAX; ++i) {
		if (gtab[i].cfg.enabled == 1)
			addch((i < TABS_MAX ? i + '1' : '#')
			| (COLOR_PAIR(C_TABTAG) | (gcfg.ct == i ? A_REVERSE : 0) | A_BOLD));
		else
			addch(i < TABS_MAX ? '*' : '#');
		addch(' ');
	}

	// Print path
	attron(COLOR_PAIR(C_PATHBAR) | A_UNDERLINE);
	if (pcols > 0)
		addwstr(fitpathcols(path, pcols));
	attroff(COLOR_PAIR(C_PATHBAR) | A_UNDERLINE);

	// Print entries
	move(++j, dcols);
	if (curscroll > 0 && ncols > 0)
		addstr("<<");

	btm = MIN(onscr + curscroll, ndents);
	for (i = curscroll; i < btm; ++i) {
		if (ptab->cfg.havesel && !(pdents[i].flag & E_SEL_SCANED)) {
			if (findinbuf(ss->nbuf, ss->endp - ss->nbuf, pdents[i].name, pdents[i].nlen))
				pdents[i].flag |= E_SEL;
			pdents[i].flag |= E_SEL_SCANED;
		}

		move(++j, 0);
		printent(&pdents[i], i == cursel, i == markent);
	}

	move(++j, dcols);
	if (btm < ndents && ncols > 0)
		addstr(">>");

	// Print filter
	if (ptab->ftlen != 0) {
		move(xlines - 2, 0);
		attron(COLOR_PAIR(F_SOCK));
		addstr("Filter: ");
		mbstowcs((wchar_t *)gmbuf, ptab->filt, xcols - 12);
		addwstr((wchar_t *)gmbuf);
		attroff(COLOR_PAIR(F_SOCK));
		addch(' ' | (ptab->ftlen > 0 ? A_REVERSE : 0));
	}

	// Print quick find
	if (ptab->fdlen > 0) {
		move(xlines - 2, 0);
		attron(COLOR_PAIR(F_CHR));
		addstr("Quick find: ");
		mbstowcs((wchar_t *)gmbuf, ptab->find, xcols - 12);
		addwstr((wchar_t *)gmbuf);
		attroff(COLOR_PAIR(F_CHR));
		addch(' ' | A_REVERSE);
	}

	// Draw scroll indicator
	j = (ndents > 0) ? ndents : 1;
	btm = (j <= onscr) ? onscr
		: MAX(1, ((onscr * onscr << 1) / j + 1) >> 1); // indicator height, round a/b by (a*2/b+1)/2
	j = (curscroll == 0 || j <= onscr) ? 2
		: 2 + (((curscroll * (onscr - btm) << 1) / (j - onscr) + 1) >> 1); // starting row to drawing
	attron(COLOR_PAIR(C_DETAIL));
	mvaddch(1, xcols -1, '=');
	for (i = 0; i < btm; ++i, ++j)
		mvaddch(j, xcols - 1, ' ' | A_REVERSE);
	mvaddch(xlines - 2, xcols - 1 , '=');
	attroff(COLOR_PAIR(C_DETAIL));
	xcols = -xcols;
}

static void fastredraw(void)
{
	if (xcols < 0) { // Skip fastredraw if redraw() has already been called
		xcols = -xcols;
		return;
	} else if (ndents == 0)
		return;

	move(2 + lastsel - curscroll, 0);
	printent(&pdents[lastsel], FALSE, lastsel == markent);

	move(2 + cursel - curscroll, 0);
	printent(&pdents[cursel], TRUE, cursel == markent);
}

static void statusbar(void)
{
	int n, x;

	move(xlines - 1, 0);
	clrtoeol();

	if (gcfg.mode == 1 || gcfg.mode == 2) {
		attron(COLOR_PAIR(C_WARN) | A_REVERSE | A_BOLD);
		addstr(" S ");
		attroff(COLOR_PAIR(C_WARN) | A_REVERSE | A_BOLD);
		addch(' ');
	} else 	if (gcfg.mode > 2) {
		attron(COLOR_PAIR(F_EXEC) | A_REVERSE | A_BOLD);
		addstr(" B ");
		attroff(COLOR_PAIR(F_EXEC) | A_REVERSE | A_BOLD);
		addch(' ');
	}

	if (errline != 0) {
		attron(COLOR_PAIR(C_WARN));
		printw("Failed (%s): %s", xitoa(errline), strerror(errnum));
		attroff(COLOR_PAIR(C_WARN));
		errline = 0;
		return;
	}

	attron(COLOR_PAIR(C_STATBAR));
	printw("%d/%d ", ndents > 0 ? cursel + 1 : 0, ndents);

	attron(A_REVERSE);
	printw(" %d ", (ndents >0 && !ptab->cfg.selmode) ? 1 : ptab->nsel);
	attroff(A_REVERSE);
	addch(' ');

	if (ndents > 0) {
		Entry *ent = &pdents[cursel];

		addch(filetypechar(ent->type)[1]);
		addstr(strperms(ent->mode));
		addch(' ');

		addstr(getpwname(ent->uid));
		addch(':');
		addstr(getgrname(ent->gid));

		printw("%8s ", tohumansize(ent->size));

		printenttime(&ent->sec);

		getyx(stdscr, n, x);
		n = xcols - x;
		if (ent->type == F_LNK && n > 1) {
			if ((x = readlink(ent->name, gpbuf, PATH_MAX - 1)) > 1) {
				gpbuf[x] = '\0';
				addstr("->");
				addwstr(fitpathcols(gpbuf, n - 2)); // Show symlink target
			}

		} else if (ent->type == F_REG && n > 1) {
			char *p = getextension(ent->name, ent->nlen);
			if (p)
				addnstr(p , n); // Show file extension
		}
	}

	getyx(stdscr, n, x);
	if (xcols - x >= 8)
		mvaddstr(n, xcols - 8, " [?]help");
	attroff(COLOR_PAIR(C_STATBAR));
}

static void filterentry(void)
{
	Entry tmpent;

	if (ptab->ftlen == 0 || setfilter(2) == GO_REDRAW)
		return;

	for (int i = 0; i < ndents; ++i) {
		if (!strcasestr(pdents[i].name, ptab->filt)
		&& i != --ndents) {
			tmpent = pdents[i];
			pdents[i] = pdents[ndents];
			pdents[ndents] = tmpent;
			--i;
		}
	}
}

static int filterinput(int c)
{
	wchar_t *wbuf = (wchar_t *)gmbuf;

	if (ptab->ftlen <= 0) // ftlen=0 no filter, ftlen<0 inactive, ftlen>0 active
		return GO_NONE;

	if (c == '/') { // turn off filter
		setfilter(0);
		return refreshview(2);
	} else if (c == '\r' || c == ESC){ // set to inactive
		ptab->ftlen = (ptab->filt[0] == '\0') ? 0 : -ptab->ftlen;
		return GO_REDRAW;

	} else if (c == KEY_BACKSPACE || c == KEY_DC) {
		if (ptab->ftlen <= 1)
			return GO_REDRAW;
		size_t len = mbstowcs(wbuf, ptab->filt, ptab->ftlen);
		wbuf[len - 1] = L'\0';
		ptab->ftlen = wcstombs(ptab->filt, wbuf, ptab->ftlen) + 1;
		ptab->filt[ptab->ftlen - 1] = '\0';
		ndents = ptab->nde;

	} else if (c > 31 && c < 256 && ptab->ftlen < FILT_MAX) {
		ptab->filt[ptab->ftlen - 1] = c;
		ptab->filt[++ptab->ftlen - 1] = '\0';
	} else
		return GO_NONE;
	return refreshview(2);
}

static int qfindinput(int c)
{
	bool cd = FALSE;
	wchar_t *wbuf = (wchar_t *)gmbuf;

	if (ptab->fdlen <= 0) // fdlen=0 no quick find, fdlen<0 invisible, fdlen>0 active
		return GO_NONE;

	if (c == '\r' || c == ESC) { // turn of or set to invisible
		ptab->fdlen = (ptab->find[0] == '\0') ? 0 : -ptab->fdlen;
		return GO_REDRAW;

	} else if (c == '/' && ptab->find[0] == '\0') { // go to root dir
		ptab->find[0] = '\0';
		ptab->fdlen = 1;
		newhistpath("/");
		return GO_RELOAD;

	} else if (c == '\t' || c == '/') { // enter dir
		ptab->find[0] = '\0';
		ptab->fdlen = 1;
		cd = TRUE;

	} else if (c == KEY_BACKSPACE || c == KEY_DC) {
		if (ptab->fdlen <= 1)
			return GO_REDRAW;
		size_t len = mbstowcs(wbuf, ptab->find, ptab->fdlen);
		wbuf[len - 1] = L'\0';
		ptab->fdlen = wcstombs(ptab->find, wbuf, ptab->fdlen) + 1;
		ptab->find[ptab->fdlen - 1] = '\0';

	} else if (c > 31 && c < 256 && ptab->fdlen < FILT_MAX) {
		ptab->find[ptab->fdlen - 1] = c;
		ptab->find[++ptab->fdlen - 1] = '\0';
	} else
		return GO_NONE;

	if (cd) {
		if (enterdir(0) == GO_NONE)
			return GO_REDRAW;
		return GO_RELOAD;
	}

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
	int c, i, ctl = GO_RELOAD;

	for (;;) {
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

			// fallthrough
		case GO_STATBAR:
			statusbar();

			// fallthrough
		case GO_NONE:
			c = getinput(stdscr);
			ctl = GO_NONE;

			if (c == KEY_RESIZE) {
				ctl = GO_REDRAW;
				break;
			} else if (c == 0)
				break;

			if ((ctl = filterinput(c)) != GO_NONE)
				break;

			if ((ctl = qfindinput(c)) != GO_NONE)
				break;

			for (i = 0; i < (int)LENGTH(keys); ++i)
				if ((c == keys[i].keysym1 || c == keys[i].keysym2) && keys[i].func)
					ctl = keys[i].func(keys[i].arg);

			if (c < 0 && gcfg.mode < 3)
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

static bool initsff(char *arg0, char *argx)
{
	char *path, *xdgcfg = getenv("XDG_CONFIG_HOME");
	struct sigaction act = {.sa_handler = exitsighandler};

	// Handle/ignore certain signals
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	act.sa_handler = SIG_IGN;
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGQUIT, &act, NULL);

	// Reset standard input
	if (!freopen("/dev/null", "r", stdin)
	|| !freopen("/dev/tty", "r", stdin)) {
		perror(xitoa(__LINE__));
		return FALSE;
	}

	// Get environment variables
	home = getenv("HOME");
	if (!home || !home[0] || access(home, R_OK | W_OK | X_OK) == -1)
		home = NULL;

	editor = getenv("EDITOR");
	if (!editor || !editor[0])
		editor = EDITOR;

	// Set config path: xdgcfg+"/sff" or home+"/.config/sff"
	if ((xdgcfg && xdgcfg[0] && makepath(xdgcfg, "sff", gpbuf))
	|| (home && makepath(home, ".config/sff", gpbuf)))
		cfgpath = gpbuf;

	// Set extfunc path, and check sff-extfunc file
	if (cfgpath && makepath(cfgpath, EXTFNNAME, gmbuf)
	&& access(gmbuf, R_OK | X_OK) == 0) // check it in config path
		extfunc = gmbuf;

	if (!extfunc && realpath(arg0, gmbuf)
	&& makepath(xdirname(gmbuf), EXTFNNAME, gmbuf)
	&& access(gmbuf, R_OK | X_OK) == 0) // check it in where sff is located
		extfunc = gmbuf;

	if (!extfunc && makepath(EXTFNPREFIX, EXTFNNAME, gmbuf)
	&& access(gmbuf, R_OK | X_OK) == 0) // check it in EXTFNPREFIX
		extfunc = gmbuf;

	// Allocate memory for cfgpath + extfunc + selpath + pipepath and set these paths
	if (cfgpath && extfunc
	&& (cfgpath = malloc(strlen(gpbuf) * 3 + strlen(gmbuf) + 64))) {
		extfunc = memccpy(cfgpath, gpbuf, '\0', PATH_MAX);
		selpath = memccpy(extfunc, gmbuf, '\0', PATH_MAX);
		makepath(gpbuf, ".selection", selpath);
		pipepath = selpath + strlen(selpath) + 1;
		strcat(strcpy(gmbuf, ".sff-pipe."), xitoa(getpid()));
		makepath(gpbuf, gmbuf, pipepath);
	} else {
		cfgpath = NULL;
		seterrnum(__LINE__, errno);
	}

	// Initialize first tab
	path = argx ? abspath(argx, gpbuf) : getcwd(gpbuf, PATH_MAX - 1);
	if (!path || !inittab(path, 0)) {
		perror(xitoa(__LINE__));
		return FALSE;
	}

	if (getuid() == 0)
		gcfg.mode = 2;
	if (!cfgpath)
		gcfg.mode = 4;
	return TRUE;
}

static void setupcurses(void)
{
	cbreak();
	noecho();
	nonl();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
	set_escdelay(20);

	define_key("\033[1;5A", CTRL_UP);
	define_key("\033[1;5B", CTRL_DOWN);
	define_key("\033[1;5C", CTRL_RIGHT);
	define_key("\033[1;5D", CTRL_LEFT);
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
	for (int i = 0; i <= TABS_MAX; ++i) {
		free(ghpath[i * 2].hs);
		free(ghpath[i * 2 + 1].hs);
		deleteallselstat(gtab[i].ss);
	}

	free(cfgpath);
	free(pdents);
	free(pnamebuf);
	free(pfindbuf);
	free(ppipebuf);
}

int main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "bcd:Hmvh")) != -1) {
		switch (opt) {
		case 'b': gcfg.mode = 4;
			break;
		case 'c': gcfg.casesens = 1;
			break;
		case 'd': gcfg.showtime = gcfg.showowner = gcfg.showperm = gcfg.showsize = 0;
			for (int i = 0; optarg[i]; ++i) {
				if (optarg[i] == 't')
					gcfg.showtime = 1;
				if (optarg[i] == 'o')
					gcfg.showowner = 1;
				if (optarg[i] == 'p')
					gcfg.showperm = 1;
				if (optarg[i] == 's')
					gcfg.showsize = 1;
			}
			break;
		case 'H': gcfg.showhidden = 1;
			break;
		case 'm': gcfg.dirontop = 0;
			break;
		case 'v': printf("v"VERSION"\n");
			return EXIT_SUCCESS;
		case 'h': usage();
			return EXIT_SUCCESS;
		default: dprintf(STDOUT_FILENO,	"Try 'sff -h' for available options.\n");
			return EXIT_FAILURE;
		}
	}

	atexit(cleanup);

	if (!initsff(argv[0], argc == optind ? NULL : argv[optind]))
		return EXIT_FAILURE;

	if (!initscr())
		return EXIT_FAILURE;
	setupcurses();

	setlocale(LC_ALL, "");

	browse();

	endwin();
	return EXIT_SUCCESS;
}
