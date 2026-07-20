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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
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
#include <pthread.h>
#define TB_IMPL
#include "termbox2.h"

#define VERSION       "1.3"
#define EXTFNNAME     "sff-extfunc"
#define EXTFNPREFIX   "/usr/local/lib/sff"
#define EXTFNPREFIX2  "/usr/lib/sff"
#ifndef PATH_MAX
#define PATH_MAX      4096
#endif
#ifndef NAME_MAX
#define NAME_MAX      255
#endif
#define TABS_MAX      4 // Number of tabs, the range of acceptable values is 1-7
#define ENTRY_INCR    128 // Number of Entry structures to allocate per shot
#define NAME_INCR     4096 // 128 entries * avg. 32 chars per name = 4KB
#define FILT_MAX      128 // Maximum length of filter string
#define HSTAT_MAX     64 // Maximum number of Histstat per Histpath

#define TRUE          1
#define FALSE         0
#define LENGTH(X)     (sizeof X / sizeof X[0])
#define MIN(x, y)     ((x) < (y) ? (x) : (y))
#define MAX(x, y)     ((x) > (y) ? (x) : (y))

enum entryflag {
	E_REG_FILE = 0x01, E_DIR_DIRLNK = 0x02,
	E_SELECTED = 0x04, E_NEW = 0x8
};

enum filetypes {
	F_REG = 0, F_DIR, F_CHR, F_BLK, F_IFO, F_LNK, F_SOCK,
	F_HLNK, F_EXEC, F_EMPT, F_ORPH, F_MISS, F_UNKN
};

enum uiflag {
	U_DETAIL = F_UNKN + 1, U_TABTAG, U_PATHBAR, U_STATBAR, U_WARN, U_NEWFILE
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
	uint32_t nsec; // 4 bytes
	mode_t mode; // 4 bytes
	uid_t uid; // 4 bytes
	gid_t gid; // 4 bytes
	uint16_t type; // 2 bytes
	uint16_t flag; // 2 bytes
	uint16_t nlen; // 2 bytes
	uint16_t misc; // 2 bytes
} Entry;

typedef struct {
	int cur;
	int scrl;
	int flag;
	int pend;
} Histstat;

typedef struct {
	char *path;
	char *stub;
	char *end;
	Histstat *stat;
	char pa[PATH_MAX];
	Histstat hs[HSTAT_MAX];
} Histpath;

typedef struct {
	uint64_t *hash;
	char *buf;
	char *end;
	size_t nhash;
	size_t mask;
	size_t plen;
	size_t buflen;
} Selstat;

typedef struct {
	char cols[8]; // Columns: 't'ime, 'o'wner, 'p'erm, 's'ize, 'n'ame, Uppercase for placeholders
	uint32_t enabled    : 1;
	uint32_t showhidden : 1; // Show hidden files
	uint32_t dirontop   : 1; // Sort directories on the top
	uint32_t sortby     : 3; // (0: name, 1: size, 2: time, 3: extension)
	uint32_t natural    : 1; // Natural numeric sorting
	uint32_t reverse    : 1; // Reverse sort
	uint32_t timetype   : 2; // (0: access, 1: modify, 2: change)
	uint32_t mansel     : 1; // Manual select mode
	// global settings
	uint32_t ct         : 3; // Current tab
	uint32_t lt         : 3; // Last tab
	uint32_t runmode    : 2; // (0: normal mode, 1: sudo mode, 2: permanent sudo mode)
	uint32_t marknew    : 1; // Show marks for new file
	uint32_t redrawn    : 2; // Screen has been redrawn
	uint32_t openfile   : 1; // Open files on right arrow or 'l' key
	uint32_t symbperm   : 1; // Show permissions as symbolic strings
	uint32_t abbrdate   : 1; // Use ls-style date format
	uint32_t showpvp    : 1; // Show preview pane
} Settings;

typedef struct {
	Histpath *hp;
	Selstat *ss;
	char filt[FILT_MAX];
	char find[FILT_MAX];
	int ftlen;
	int fdlen;
	int nde;
	int nsel;
	int nss;
	Settings cfg;
} Tabs;

typedef struct {
	int keysym1;
	int keysym2;
	int (*func)(int);
	int arg;
	const char *cmnt;
} Key;

typedef struct {
	char *script;
	char *path;
	int sig;
	int lines;
	int cols;
} Pvargs;

/*** Global Variables ***/

static int ndents = 0, tdents = 0, cursel = 0, lastsel = -1, curscroll = 0;
static int markent = -1, errline = 0, errnum = 0;
static int xlines, xcols, onscr, ncols, pvcols;
static size_t namebuflen = 0;
static time_t curtime;
static char *home, *opener, *root = "/";
static char *cfgpath = NULL, *extfunc = NULL, *pipepath = NULL;
static char *pnamebuf = NULL, *pfindbuf = NULL, *pfindend = NULL, *findname = NULL, *pvbuf = NULL;
static Entry *pdents = NULL;
static Tabs *ptab = NULL;
static _Atomic int pvdraw = 0;
static pthread_mutex_t pvmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pvcond = PTHREAD_COND_INITIALIZER;

alignas(max_align_t) static char gnbuf[NAME_MAX + 1] = {0};
alignas(max_align_t) static char gpbuf[PATH_MAX * sizeof(wchar_t)] = {0};
alignas(max_align_t) static Tabs gtab[TABS_MAX + 1] = {{0}};
alignas(max_align_t) static Histpath ghpath[(TABS_MAX + 1) * 2] = {{0}};

/*** Generic Functions ***/

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
	char *p = NULL;

	if (path == buf)
		p = memchr(buf, '\0', PATH_MAX - 3);
	else if ((p = memccpy(buf, path, '\0', PATH_MAX - 3)))
		--p;

	if (p) {
		if (p > buf && *(p - 1) != '/')
			*p++ = '/';
		p = memccpy(p, name, '\0', PATH_MAX - (p - buf) - 2);
	}
	return p ? p - buf : 0;
}

/* Get file extension. Ignore extensions > 8 chars. len includes terminating '\0'. */
static const char *getextension(const char *name, size_t len)
{
	const char *p;

	if (len < 4)
		return NULL;
	p = name + len - 2; // skip last char (before '\0')
	len = (len > 11) ? 9 : len - 2; // If name length exceeds 8+2+1, check max 8+1 times

	 while (--len > 0)
		if (*(--p) == '.')
			return p;
	return NULL;
}

/* Get the absolute pathname without resolving symlinks. buf can not be NULL. */
static char *abspath(const char *src, char *buf)
{
	size_t len = 0;
	char *dst;

	if (!src || !buf)
		return NULL;
	if (src[0] != '/') {
		if (!getcwd(buf, PATH_MAX))
			return NULL;
		if (!src[0])
			return buf;
		len = strlen(buf);
	} else
		++src;
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
static char *xitoa(uint32_t val)
{
	static char dst[24] = {0};
	static const char digits[] =
		"0001020304050607080910111213141516171819"
		"2021222324252627282930313233343536373839"
		"4041424344454647484950515253545556575859"
		"6061626364656667686970717273747576777879"
		"8081828384858687888990919293949596979899";
	uint32_t i, j, quo;

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
	char *sp;

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

static uint64_t fnv1ahash(const char *str, size_t len)
{
	uint64_t hash = 0xcbf29ce484222325ULL;

	for (size_t i = 0; i < len; i++) {
		hash ^= (const uint8_t)str[i];
		hash *= 0x100000001b3ULL;
	}
	return hash;
}

static int inittermbox(void)
{
	int ret = tb_init();

	if (ret) {
		fprintf(stderr, "tb_init() failed with error code %d\n", ret);
		return FALSE;
	}
	tb_set_input_mode(TB_INPUT_ESC);
	tb_set_output_mode(TB_OUTPUT_256);

	xcols = tb_width();
	xlines = tb_height();
	onscr = xlines - 4;
	return TRUE;
}

static int spawn(char *arg0, char *arg1, char *arg2, int detach, int (*callbackfn)(void))
{
	pid_t pid;
	int ctl = GO_STATBAR;
	char *argv[4] = {arg0, arg1, arg2, NULL};
	struct sigaction oldsigtstp, oldsigwinch;

	if (!detach)
		tb_shutdown();
	pid = fork();
	if (pid > 0) {
		sigaction(SIGTSTP, &(struct sigaction){.sa_handler = SIG_IGN}, &oldsigtstp);
		sigaction(SIGWINCH, &(struct sigaction){.sa_handler = SIG_IGN}, &oldsigwinch);
		if (callbackfn)
			ctl = callbackfn();
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
	if (!detach)
		inittermbox();
	return ctl;
}

/*** Key Functions ***/

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

static Histpath *inithistpath(Histpath *hp, const char *path)
{
	const char *name = NULL;
	struct stat sb;

	if (lstat(path, &sb) == -1 && seterrnum(__LINE__, errno))
		return NULL;
	if (!S_ISDIR(sb.st_mode)
	&& !((sb.st_mode & S_IFMT) == S_IFLNK && stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)))
		name = xbasename(path);

	if (path[0] == '/' && path[1] == '\0')
		path = "";
	hp->end = (path == hp->pa) ? hp->pa + strlen(hp->pa) + 1 : memccpy(hp->pa, path, '\0', PATH_MAX - 1);
	if (name) {
		hp->end = strrchr(hp->pa, '/');
		*hp->end = '\0';
		findname = ++hp->end;
	}
	hp->path = (hp->pa[0] == '\0') ? root : hp->pa;
	hp->stub = hp->end;

	memset(hp->hs, 0, sizeof(Histstat) * HSTAT_MAX);
	hp->stat = hp->hs;
	for (char *p = hp->pa + 2; p < hp->end; ++p) {
		if (*p == '/' || *p == '\0') { // Each level of path corresponds to a histstat
			if (hp->stat - hp->hs < HSTAT_MAX - 1)
				++hp->stat;
			else
				++hp->stat->pend;
		}
	}
	return hp;
}

static Selstat *getselstat(Tabs *tab)
{
	Selstat *ss = NULL;

	if (!tab)
		return NULL;

	if (tab->ss) {
		for (int i = 0; i < tab->nss && !ss; ++i)
			if (tab->ss[i].plen == 0)
				ss = tab->ss + i;
	}
	if (!ss) {
		if (!(ss = realloc(tab->ss, sizeof(Selstat) * ++tab->nss)) && seterrnum(__LINE__, errno)) {
			--tab->nss;
			return NULL;
		}
		tab->ss = ss;
		ss = tab->ss + (tab->nss - 1);
		memset(ss, 0, sizeof(Selstat));
	}
	return ss;
}

static void saveselection(Tabs *tab)
{
	Selstat *ss = getselstat(tab);
	size_t nsel = 0, len = 0, n = 64;
	uint64_t hash;

	if (!ss)
		return;

	for (int i = 0; i < tab->nde; ++i) {
		if ((pdents[i].flag & E_SELECTED)) {
			++nsel;
			len += pdents[i].nlen;
		}
	}
	if (nsel == 0)
		return;
	len += PATH_MAX;

	if (len > ss->buflen) {
		char *p = realloc(ss->buf, len);
		if (!p && seterrnum(__LINE__, errno))
			return;
		ss->buf = p;
		ss->buflen = len;
	}
	ss->end = memccpy(ss->buf, tab->hp->path, '\0', PATH_MAX); // Buffer layout: dir path, then selected file names
	ss->plen = ss->end - ss->buf;

	while ((n <<= 1) < (nsel << 1));
	if (n > ss->nhash) {
		uint64_t *p1 = realloc(ss->hash, sizeof(uint64_t) * n);
		if (!p1 && seterrnum(__LINE__, errno))
			return;
		ss->hash = p1;
		ss->nhash = n;
	}
	memset(ss->hash, 0, sizeof(uint64_t) * n);
	ss->mask = n - 1;

	for (size_t idx, i = 0; i < (size_t)tab->nde; ++i) {
		if ((pdents[i].flag & E_SELECTED)) {
			hash = fnv1ahash(pdents[i].name, pdents[i].nlen);
			idx = (hash ^ (hash >> 33)) & ss->mask;
			while (ss->hash[idx] != 0)
				idx = (idx + 1) & ss->mask;
			ss->hash[idx] = hash;
			ss->end = memccpy(ss->end, pdents[i].name, '\0', pdents[i].nlen);
		}
	}
}

static void savedirstat(Tabs *tab)
{
	if (ndents > 0 && tab->hp->stat->pend == 0) {
		tab->hp->stat->cur = cursel;
		tab->hp->stat->scrl = curscroll;
	}
	saveselection(tab);
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

	savedirstat(ptab);
	ptab->hp = hp2;
	return GO_RELOAD;
}

static int switchhistpath(int n)
{
	Histpath *hp = ptab->hp;
	Histpath *hp2 = ((hp - ghpath) & 1) ? hp - 1 : hp + 1;

	if ((gcfg.ct == TABS_MAX && n == 0) || chdir(hp2->path) == -1)
		return GO_NONE;

	savedirstat(ptab);
	findname = hp2->stub;
	ptab->hp = hp2;
	return GO_RELOAD;
}

static int enterdir(int n)
{
	Histpath *hp = ptab->hp;
	Histpath *hp2 = ((hp - ghpath) & 1) ? hp - 1 : hp + 1;
	Entry *ent;
	char *newpath = gpbuf;

	if (ndents == 0)
		return GO_NONE;
	ent = &pdents[cursel];
	makepath(hp->path, ent->name, newpath);

	if (!(ent->flag & E_DIR_DIRLNK)) {
		if (n == 1 || gcfg.openfile)
			spawn(opener, newpath, NULL, *opener != '/', NULL);
		return GO_STATBAR;
	}
	if (hp->stat->flag == S_ROOT) {
		if (strcmp(newpath, hp2->path) == 0)
			return switchhistpath(1);
		else
			return newhistpath(newpath, TRUE);
	}
	if (chdir(newpath) == -1 && seterrnum(__LINE__, errno))
		return GO_STATBAR;

	savedirstat(ptab);
	if (hp->stub < hp->end && strcmp(ent->name, hp->stub) != 0) {
		if ((strcmp(hp->path, hp2->path) == 0 && strcmp(ent->name, hp2->stub) == 0)
		|| (gcfg.ct < TABS_MAX && inithistpath(hp2, hp->path)))
			hp = hp2;
	}
	hp->path = hp->pa;
	*(hp->stub - 1) = '/';
	hp->stub = memccpy(hp->stub, ent->name, '\0', NAME_MAX + 1);
	if (hp->end < hp->stub)
		hp->end = hp->stub;

	if (hp->stat - hp->hs < HSTAT_MAX - 1)
		++hp->stat;
	else
		++hp->stat->pend;
	ptab->hp = hp;
	return GO_RELOAD;
}

static int gotoparent(int n __attribute__((unused)))
{
	Histpath *hp = ptab->hp;

	if (hp->path == root || hp->stat->flag == S_ROOT)
		return GO_NONE;
	if (hp->stat->flag == S_SUBROOT)
		return switchhistpath(1);

	savedirstat(ptab);
	do {
		hp->stat -= (hp->stat == hp->hs || hp->stat->pend > 0) ? 0 : 1;
		hp->stat->pend -= (hp->stat->pend > 0) ? 1 : 0;
		hp->stub = strrchr(hp->pa, '/');
		*hp->stub = '\0';
		hp->path = (hp->stub == hp->pa) ? root : hp->pa;
		++hp->stub;
	} while (chdir(hp->path) == -1 && hp->path != root && hp->stat->flag != S_SUBROOT);

	findname = hp->stub;
	return GO_RELOAD;
}

static int gotohome(int n __attribute__((unused)))
{
	return newhistpath(home ? home : root, FALSE);
}

static int refreshview(int n)
{
	if (ndents > 0) {
		savedirstat(ptab);
		memccpy(gnbuf, pdents[cursel].name, '\0', NAME_MAX);
		findname = gnbuf;
	}
	if (n == 1)
		gcfg.marknew ^= 1;
	else if (n == 2)
		return GO_SORT;
	return GO_RELOAD;
}

static void clearselstat(Tabs *tab, int idx, int setfree)
{
	for (int i = 0; i < tab->nss; ++i) {
		if (idx == -1 || idx == i) {
			tab->ss[i].plen = 0;
			if (setfree) {
				free(tab->ss[i].hash);
				free(tab->ss[i].buf);
				memset(tab->ss + i, 0, sizeof(Selstat));
			}
			tab->ss[i].end = tab->ss[i].buf;
		}
	}
	if (idx == -1 && setfree) {
		free(tab->ss);
		tab->ss = NULL;
		tab->nss = 0;
	}
}

static void appendselection(Entry *ent)
{
	if ((ent->flag & E_SELECTED))
		return;
	ent->flag |= E_SELECTED;
	++ptab->nsel;
	ptab->cfg.mansel = 1;
}

static void removeselection(Entry *ent)
{
	if (!(ent->flag & E_SELECTED))
		return;
	ent->flag &= ~E_SELECTED;
	--ptab->nsel;
	if (ptab->nsel == 0)
		ptab->cfg.mansel = 0;
}

static int toggleselection(int n)
{
	if (ndents == 0)
		return GO_NONE;

	if (pdents[cursel].flag & E_SELECTED)
		removeselection(&pdents[cursel]);
	else
		appendselection(&pdents[cursel]);
	return shiftcursor(n, 0);
}

static int selectall(int n)
{
	int autosel = !ptab->cfg.mansel;

	for (int i = 0; i < ndents; ++i) {
		if (!(pdents[i].flag & E_SELECTED))
			appendselection(&pdents[i]);
		else if (n == -1)
			removeselection(&pdents[i]);
	}
	if (n == -1 && autosel && ndents > 0)
		removeselection(&pdents[cursel]);
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
			appendselection(&pdents[i]);
	} else {
		for (int i = markent; (step == 1) ? i <= cursel : i >= cursel; i += step)
			removeselection(&pdents[i]);
	}
	markent = -1;
	return GO_REDRAW;
}

static int clearselection(int n __attribute__((unused)))
{
	clearselstat(ptab, -1, FALSE);
	ptab->nsel = 0;
	ptab->cfg.mansel = 0;
	markent = -1;
	for (int i = 0; i < ptab->nde; ++i)
		pdents[i].flag &= ~E_SELECTED;
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
	gtab[n].hp = inithistpath(&ghpath[n * 2], path);
	if (!gtab[n].hp || !inithistpath(&ghpath[n * 2 + 1], path))
		return FALSE;
	if (n == TABS_MAX)
		gtab[n].hp->stat->flag = S_ROOT;

	clearselstat(&gtab[n], -1, FALSE);
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
	if (!gtab[n].cfg.enabled && !inittab(hp->path, n) && !inittab(home ? home : root, n))
		return GO_STATBAR;

	savedirstat(ptab);
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
		if (i != ct && gtab[i].cfg.enabled)
			lt = i;

	if (gcfg.lt != ct && gtab[gcfg.lt].cfg.enabled)
		lt = gcfg.lt;

	if (lt == -1) {
		if (ct == 0)
			return GO_NONE;
		if (!inittab(home ? home : root, 0) || (chdir(home ? home : root) == -1 && seterrnum(__LINE__, errno)))
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

	clearselstat(&gtab[ct], -1, FALSE);
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

static int getinput(int ms)
{
	int c = 0;
	struct tb_event ev = {0};

	if (tb_peek_event(&ev, ms) == TB_ERR_NO_EVENT) {
		c = KEY_TIMEOUT;
	} else if (ev.type == TB_EVENT_KEY) {
		c = ev.ch ? ev.ch : ev.key;
		if (c == TB_KEY_ESC && tb_peek_event(&ev, 10) != TB_ERR_NO_EVENT && ev.ch > 31 && ev.ch < 127)
			c = -ev.ch;
		else if ((ev.mod & TB_MOD_CTRL) && ev.key == TB_KEY_ARROW_UP)
			c = CTRL_UP;
		else if ((ev.mod & TB_MOD_CTRL) && ev.key == TB_KEY_ARROW_DOWN)
			c = CTRL_DOWN;
		else if ((ev.mod & TB_MOD_SHIFT) && ev.key == TB_KEY_ARROW_UP)
			c = SHIFT_UP;
		else if ((ev.mod & TB_MOD_SHIFT) && ev.key == TB_KEY_ARROW_DOWN)
			c = SHIFT_DOWN;
	}
	return (ev.type == TB_EVENT_RESIZE) ? KEY_RESIZE : c;
}

static void cleararea(int x, int y, int w, int h)
{
	for (int i = y; i < y + h; ++i)
		for (int j = x; j < x + w; ++j)
			tb_set_cell(j, i, ' ', C_DEF, C_DEF);
}

static void setcolumns(char *cols, int c)
{
	for (signed char *p = (signed char *)cols; *p; ++p)
		if (*p == c || *p == c - 32 || *p == -c) {
			if (*p > 96)
				*p = *p - 32;
			else
				*p = (*p > 0) ? *p + 32 : -*p - 32;
		}
}

static int viewoptions(int n __attribute__((unused)))
{
	int c = 0, w = 50, h = 20, x = MAX((xcols - w) / 2, 0), y = MAX((xlines - h) / 2, 0);
	Settings *cfg = &ptab->cfg;

	while (c == 0) {
		tb_set_cell(x, y, 0x250C, C_DEF, C_DEF);
		tb_set_cell(x + w - 1, y, 0x2510, C_DEF, C_DEF);
		tb_set_cell(x, y + h - 1, 0x2514, C_DEF, C_DEF);
		tb_set_cell(x + w - 1, y + h - 1, 0x2518, C_DEF, C_DEF);
		for (int i = x + 1; i < x + w - 1; ++i) {
			tb_set_cell(i, y, 0x2500, C_DEF, C_DEF);
			tb_set_cell(i, y + h - 1, 0x2500, C_DEF, C_DEF);
		}
		for (int i = y + 1; i < y + h - 1; ++i) {
			tb_set_cell(x, i, 0x2502, C_DEF, C_DEF);
			tb_set_cell(x + w - 1, i, 0x2502, C_DEF, C_DEF);
		}
		cleararea(x + 1, y + 1, w - 2, h - 2);
		tb_print(x + 8, y +  0, C_DEF, C_DEF, " View Options ");
		tb_print(x + 2, y +  2, C_DEF, C_DEF, "[.]show hidden  [/]dirs on top");
		tb_print(x + 2, y +  4, C_DEF, C_DEF, "Sort by:");
		tb_print(x + 2, y +  5, C_DEF, C_DEF, "  (n)name  (s)size  (t)time  (e)extension");
		tb_print(x + 2, y +  7, C_DEF, C_DEF, "  [v]natural  [r]reverse");
		tb_print(x + 2, y +  9, C_DEF, C_DEF, "Detail info:");
		tb_print(x + 2, y + 10, C_DEF, C_DEF, "  [i]time  [u]owner  [p]permissions  [y]size");
		tb_print(x + 2, y + 12, C_DEF, C_DEF, "  (d)default  (x)none");
		tb_print(x + 2, y + 14, C_DEF, C_DEF, "Time type:");
		tb_print(x + 2, y + 15, C_DEF, C_DEF, "  (a)access  (m)modify  (c)change");
		tb_print(x + 2, y + 17, C_DEF, C_DEF, "Press 'o' or Esc to close");

		tb_print(x +  5, y +  2, cfg->showhidden ? TB_REVERSE : C_DEF, C_DEF, "show hidden");
		tb_print(x + 21, y +  2, cfg->dirontop ? TB_REVERSE : C_DEF, C_DEF, "dirs on top");
		tb_print(x +  7, y +  5, cfg->sortby == 0 ? TB_REVERSE : C_DEF, C_DEF, "name");
		tb_print(x + 16, y +  5, cfg->sortby == 1 ? TB_REVERSE : C_DEF, C_DEF, "size");
		tb_print(x + 25, y +  5, cfg->sortby == 2 ? TB_REVERSE : C_DEF, C_DEF, "time");
		tb_print(x + 34, y +  5, cfg->sortby == 3 ? TB_REVERSE : C_DEF, C_DEF, "extension");
		tb_print(x +  7, y +  7, cfg->natural ? TB_REVERSE : C_DEF, C_DEF, "natural");
		tb_print(x + 19, y +  7, cfg->reverse ? TB_REVERSE : C_DEF, C_DEF, "reverse");
		tb_print(x +  7, y + 10, strchr(cfg->cols, 't') || strchr(cfg->cols, -'t') ? TB_REVERSE : C_DEF, C_DEF, "time");
		tb_print(x + 16, y + 10, strchr(cfg->cols, 'o') || strchr(cfg->cols, -'o') ? TB_REVERSE : C_DEF, C_DEF, "owner");
		tb_print(x + 26, y + 10, strchr(cfg->cols, 'p') || strchr(cfg->cols, -'p') ? TB_REVERSE : C_DEF, C_DEF, "permissions");
		tb_print(x + 42, y + 10, strchr(cfg->cols, 's') || strchr(cfg->cols, -'s') ? TB_REVERSE : C_DEF, C_DEF, "size");
		tb_print(x +  7, y + 15, cfg->timetype == 0 ? TB_REVERSE : C_DEF, C_DEF, "access");
		tb_print(x + 18, y + 15, cfg->timetype == 1 ? TB_REVERSE : C_DEF, C_DEF, "modify");
		tb_print(x + 29, y + 15, cfg->timetype == 2 ? TB_REVERSE : C_DEF, C_DEF, "change");

		tb_present();
		c = getinput(-1);
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
		case 'x': for (signed char *p = (signed char *)cfg->cols; *p; ++p) {
				if (*p > 96 && *p != 'n')
					*p = *p - 32;
				else if (*p < 0)
					*p = -*p - 32;
			}
			break;

		case 'a': cfg->timetype = 0;
			break;
		case 'm': cfg->timetype = 1;
			break;
		case 'c': cfg->timetype = 2;
			break;
		case 'o':
			break;
		case TB_KEY_ESC:
			break;
		default: c = 0;
		}
	}
	if (c == TB_KEY_ESC || strchr("oiupydx", c))
		return GO_REDRAW;
	return refreshview(strchr(".amc", c) ? 0 : 2);
}

static int prefixkey(int n __attribute__((unused)))
{
	int c, ctl = GO_REDRAW;

	tb_print(0, xlines - 2, C_DEF, C_DEF, "Key for extension function:    ");
	tb_present();
	if ((c = getinput(2000)) > 31)
		ctl = callextfunc(c);
	return (ctl < GO_REDRAW) ? GO_REDRAW : ctl;
}

static int showhelp(int n __attribute__((unused)))
{
	int y = 0, d = 0, klines = (int)LENGTH(keys);

	for (int c = 0; c != TB_KEY_ESC && c != 'q'; y = d) {
		xlines = tb_height();
		tb_clear();
		tb_print(0, y, C_DEF, C_DEF, "sff "VERSION);
		tb_print(1, y += 2, C_DEF, C_DEF, "Builtin functions:");

		for (int i = 0; i < klines; ++i)
			tb_print(2, ++y, C_DEF, C_DEF, keys[i].cmnt);

		tb_print(0, y += 2, C_DEF, C_DEF, "Note: File operations are implemented by extension functions");
		tb_print(0, ++y, C_DEF, C_DEF, "For help with that, press Alt+'/' or 'u'-'/' in main view");
		tb_print(0, ++y, C_DEF, C_DEF, "Press 'q' or Esc to leave this page");

		tb_present();
		c = getinput(-1);
		if (c == TB_KEY_ARROW_UP || c == 'k')
			d = MIN(d + 1, 0);
		else if (c == TB_KEY_ARROW_DOWN || c == 'j')
			d = MAX(d - 1, xlines - klines - 8);
		else if (c == TB_KEY_PGUP || c == CTRL_('B'))
			d = MIN(d + xlines - 1, 0);
		else if (c == TB_KEY_PGDN || c == CTRL_('F'))
			d = MAX(d - (xlines - 1), xlines - klines - 8);
	}
	return GO_REDRAW;
}

static int quitsff(int n __attribute__((unused)))
{
	return GO_QUIT;
}

/*** Core Functions ***/

static void usage(void)
{
	printf("sff "VERSION"\n\n"
		"Usage: sff [OPTIONS] [PATH]\n\n"
		"Options:\n"
		" -d        use ls-style date format\n"
		" -H        show hidden files\n"
		" -l <keys> set columns: (use uppercase to hide)\n"
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
	int res, fa = pa->flag & E_DIR_DIRLNK, fb = pb->flag & E_DIR_DIRLNK;
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
			if ((res = strcasecmp(exta, extb)))
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

static int xmbtowc(wchar_t *wc, const char *str, int *cols)
{
	int n = 1, w = 1;

	if ((signed char)*str < 0) { // non-ASCII
		if ((n = mbtowc(wc, str, MB_CUR_MAX)) < 0)
			*wc = L'\uFFFD'; // invalid char
		w = tb_wcwidth(*wc);
	} else if ((unsigned int)*str - 32 < 95) { // ASCII 32-126
		*wc = (wchar_t)*str;
	} else // ASCII 1-31 and 127
		*wc = L'?';

	*cols = (w < 0) ? 0 : w;
	return (n < 1) ? 1 : n;
}

static int printnstr(int x, int y, int fg, const char *str, int maxcols)
{
	int w = 0, x2 = x;
	wchar_t wc;

	for (int n, cols; *str; str += n, x += cols) {
		n = xmbtowc(&wc, str, &cols);
		if ((w += cols) > maxcols) {
			if (w - cols > 0)
				tb_set_cell(x2, y, '~', fg, C_DEF);
			w -= cols;
			break;
		}
		tb_set_cell(x2 = x, y, (uint32_t)wc, fg, C_DEF);
	}
	return w;
}

#define PV_CACHE_X  256  // maximum columns for text preview
#define PV_CACHE_Y  128  // maximum lines for text preview
static void *runpvscript(void *args)
{
	char *pbuf, *pn, cmd[PATH_MAX + 64];
	int lines, cols, lastsig = -1;
	struct timespec ts = {0, 10 * 1000000L};
	Pvargs *pva = (Pvargs *)args;
	FILE *fp;
	int len = snprintf(cmd, PATH_MAX + 4, "\"%s\" ", pva->script);

	for (;;) {
		pthread_mutex_lock(&pvmutex);
		while (lastsig == pva->sig) {
			pvdraw = 2;
			pthread_cond_wait(&pvcond, &pvmutex);
		}
		lastsig = pva->sig;
		lines = pva->lines;
		cols = pva->cols;
		setenv("SFF_PV_PATH", pva->path, 1);
		pthread_mutex_unlock(&pvmutex);

		if (lastsig == 0)
			break;
		nanosleep(&ts, NULL);
		if (pvdraw == 1) {
			lastsig = -1;
			continue;
		}
		pvdraw = 3;
		snprintf(cmd + len, 60, "%d %d 2>/dev/null", lines, cols);
		if (!(fp = popen(cmd, "r")))
			continue;

		pbuf = pvbuf;
		for (int i = 0; i < lines && i < PV_CACHE_Y - 1; ++i, pbuf += PV_CACHE_X) {
			if (fgets(pbuf, PV_CACHE_X, fp) != NULL) {
				if (!(pn = strchr(pbuf, '\n'))) {
					pn = pbuf + PV_CACHE_X - 1;
					while (fgets(pn + 1, PV_CACHE_X, fp) != NULL && !strchr(pn + 1, '\n'));
				}
			} else
				pn = pbuf;
			*pn = '\0';
		}
		pclose(fp);
	}
	return NULL;
}

static int setpreview(int op, char *path)
{
	static char pvpath[PATH_MAX] = {0};
	static Pvargs pva = {0};
	static pthread_t pvthid;

	switch (op) {
	case 1: // open preview
		if (!path || (access(path, X_OK) != 0 && seterrnum(__LINE__, errno)))
			return GO_STATBAR;
		if (!pvbuf && !(pvbuf = calloc(PV_CACHE_X * PV_CACHE_Y, 1)) && seterrnum(__LINE__, errno))
			return GO_STATBAR;
		pva = (Pvargs){.script = path, .path = pvpath, .sig = -1};
		gcfg.showpvp = 1;
		pthread_create(&pvthid, NULL, runpvscript, &pva);

		break;
	case 0: // close preview
		gcfg.showpvp = 0;
		if (pva.sig == 0)
			return GO_NONE;
		pthread_mutex_lock(&pvmutex);
		pva.sig = 0;
		pthread_cond_signal(&pvcond);
		pthread_mutex_unlock(&pvmutex);
		pthread_join(pvthid, NULL);

		break;
	case 2: // refresh
		if (ndents == 0)
			return GO_NONE;
		pthread_mutex_lock(&pvmutex);
		pva.sig = cursel + ((intptr_t)ptab->hp->stat ^ ndents);
		pva.lines = xlines - 2;
		pva.cols = pvcols - 1;
		makepath(ptab->hp->path, pdents[cursel].name, pvpath);
		pthread_cond_signal(&pvcond);
		pthread_mutex_unlock(&pvmutex);

		break;
	case 3: // draw preview
		if (!gcfg.showpvp || ndents == 0 || pvdraw == 3)
			return GO_NONE;
		pvdraw = 1;
		cleararea(xcols - pvcols, 1, pvcols, xlines - 3);
		for (int i = 1, j = 0; i < xlines - 2 && i < PV_CACHE_Y - 1; ++i, j += PV_CACHE_X)
			printnstr(xcols - pvcols + 1, i, C_DEF, &pvbuf[j], pvcols - 1);
		pvdraw = 0;
		gcfg.redrawn = 0;
		return GO_NONE;
	}
	return GO_REDRAW;
}

static int writeselection(int fd)
{
	Selstat *ss;
	ssize_t len;

	for (int autosel = !ptab->cfg.mansel && ndents > 0, i = 0; i < ptab->nde; ++i) {
		if ((pdents[i].flag & E_SELECTED) || (autosel && i == cursel)) {
			len = makepath(ptab->hp->path, pdents[i].name, gpbuf);
			if (write(fd, gpbuf, len) != len && seterrnum(__LINE__, errno))
				break;
		}
	}
	for (int i = 0; i < ptab->nss && errline == 0; ++i) {
		ss = &ptab->ss[i];
		for (char *p = ss->buf + ss->plen, *end; p < ss->end && (end = memchr(p, '\0', PATH_MAX)); p = end + 1) {
			len = makepath(ss->buf, p, gpbuf);
			if (write(fd, gpbuf, len) != len && seterrnum(__LINE__, errno))
				break;
		}
	}
	return (errline == 0) ? TRUE : FALSE;
}

static int readfindresult(int fd)
{
	ssize_t len = 1;
	size_t buflen = 0, reslen = 0;

	while (len > 0) {
		if (buflen - reslen < NAME_INCR) {
			char *p = realloc(pfindbuf, buflen += NAME_INCR);
			if (!p && seterrnum(__LINE__, errno)) {
				len = -1;
				break;
			}
			pfindbuf = p;
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

static int handlepipedata(int fd, int n)
{
	if (n == 0 && read(fd, &n, 1) == -1 && seterrnum(__LINE__, errno))
		return GO_STATBAR;

	switch (n) {
	case '.': // clear selection
		return clearselection(0);

	case '*': // refresh
		if (read(fd, &n, 1) == 1)
			clearselection(0);
		return refreshview(0);

	case '@': // select specified file
		if ((n = read(fd, gpbuf, PATH_MAX)) == -1 && seterrnum(__LINE__, errno))
			return GO_STATBAR;
		gpbuf[n] = '\0';
		memccpy(gnbuf, xbasename(gpbuf), '\0', NAME_MAX);
		findname = gnbuf;
		savedirstat(ptab);
		clearselection(0);
		return GO_RELOAD;

	case '>': // enter specified path
		if ((n = read(fd, gpbuf, PATH_MAX)) == -1 && seterrnum(__LINE__, errno))
			return GO_STATBAR;
		gpbuf[n] = '\0';
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
		if ((n = read(fd, gpbuf, PATH_MAX)) == -1 && seterrnum(__LINE__, errno))
			return GO_STATBAR;
		gpbuf[n] = '\0';
		return setpreview(gcfg.showpvp ^ 1, gpbuf);
	}
	return GO_REDRAW;
}

static int readpipe(void)
{
	pid_t gpid = 0;
	int fd, len, ctl = GO_REDRAW;

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
	return ctl;
}

static int callextfunc(int c)
{
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

	return spawn(extfunc, (char [2]){c, '\0'}, gcfg.runmode == 1 ? "su" : NULL, FALSE, &readpipe);
}

#ifdef __APPLE__
#define STVNSEC(X)  X##timespec.tv_nsec
#else
#define STVNSEC(X)  X##tim.tv_nsec
#endif
static void fillentry(int fd, Entry *ent, struct stat *sb)
{
	switch (ptab->cfg.timetype) {
	case 0: ent->sec = sb->st_atime;
		ent->nsec = (uint32_t)STVNSEC(sb->st_a);
		break;
	case 1: ent->sec = sb->st_mtime;
		ent->nsec = (uint32_t)STVNSEC(sb->st_m);
		break;
	case 2: ent->sec = sb->st_ctime;
		ent->nsec = (uint32_t)STVNSEC(sb->st_c);
	}

	ent->size = sb->st_size;
	ent->mode = sb->st_mode;
	ent->uid = sb->st_uid;
	ent->gid = sb->st_gid;
	ent->flag = 0;
	if (gcfg.marknew && (curtime - sb->st_ctime < 180))
		ent->flag |= E_NEW;

	switch (ent->mode & S_IFMT) {
	case S_IFREG: ent->type = F_REG;
		if (sb->st_nlink > 1)
			ent->type = F_HLNK;
		if (sb->st_mode & S_IXUSR)
			ent->type = F_EXEC;
		ent->flag |= E_REG_FILE;
		break;

	case S_IFDIR: ent->type = F_DIR;
		ent->flag |= E_DIR_DIRLNK;
		break;
	case S_IFLNK: ent->type = F_LNK;
		fstatat(fd, ent->name, sb, 0);
		if (S_ISDIR(sb->st_mode))
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
}

static void loaddirentry(DIR *dirp, int fd)
{
	char *name, *tmp;
	size_t off = 0;
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

		if (ndents == tdents) {
			Entry *p = realloc(pdents, (tdents += ENTRY_INCR) * sizeof(Entry));
			if (!p && seterrnum(__LINE__, errno)) {
				tdents -= ENTRY_INCR;
				return;
			}
			pdents = p;
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

		fillentry(fd, ent, &sb);
		++ndents;
	}
}

static void loadsrchentry(int fd)
{
	struct stat sb;
	Entry *ent;

	for (char *name = pfindbuf, *end; name < pfindend && (end = memchr(name, '\0', PATH_MAX)); name = end + 1) {
		if (fstatat(fd, name, &sb, AT_SYMLINK_NOFOLLOW) == -1)
			continue;

		if (ndents == tdents) {
			Entry *p = realloc(pdents, (tdents += ENTRY_INCR) * sizeof(Entry));
			if (!p && seterrnum(__LINE__, errno)) {
				tdents -= ENTRY_INCR;
				return;
			}
			pdents = p;
		}

		ent = pdents + ndents;
		ent->name = name;
		ent->nlen = end - name + 1;

		fillentry(fd, ent, &sb);
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

static void restoredirstat(Tabs *tab)
{
	Histstat *hs = tab->hp->stat;
	Selstat *ss = NULL;
	uint64_t hash;

	// Find current entry, and set cursel
	cursel = hs->cur;
	curscroll = hs->scrl;
	if (findname) {
		if (hs->cur >= ndents || strcmp(findname, pdents[hs->cur].name) != 0) {
			for (int i = 0; i < ndents; ++i) {
				if (strcmp(findname, pdents[i].name) == 0) {
					cursel = i;
					curscroll = MAX(i - (onscr * 3 >> 2), MIN(i - (onscr >> 2), hs->scrl));
					break;
				}
			}
		}
		findname = NULL;
	}

	// Find corresponding selstat, and restore selection
	markent = -1;
	for (int i = 0; i < tab->nss && !ss; ++i)
		if (tab->ss[i].plen != 0 && strcmp(tab->ss[i].buf, tab->hp->path) == 0)
			ss = &tab->ss[i];
	if (ss) {
		for (size_t idx, i = 0; i < (size_t)tab->nde; ++i) {
			hash = fnv1ahash(pdents[i].name, pdents[i].nlen);
			idx = (hash ^ (hash >> 33)) & ss->mask;
			while (ss->hash[idx] != 0) {
				if (ss->hash[idx] == hash) {
					pdents[i].flag |= E_SELECTED;
					break;
				}
				idx = (idx + 1) & ss->mask;
			}
		}
		clearselstat(tab, ss - tab->ss, FALSE);
	}
}

static int printpath(int x, int y, int fg, const char *str, int maxcols)
{
	int n, cols, x2 = x, w = 0;
	wchar_t *wbuf = (wchar_t *)gpbuf, *wbp = wbuf;

	for (const char *s = str; *s && w < PATH_MAX; s += n, w += cols, ++wbp)
		n = xmbtowc(wbp, s, &cols);
	*wbp = L'\0';
	if (w > maxcols) {
		wbp = wbuf + 1; // When fold path, keep the first level
		for (wchar_t *tbp = wbp, *slash = NULL; *tbp; ++tbp, ++wbp) {
			if (*tbp == L'/') {
				if (slash)
					slash = wbp = slash + 2;
				else
					slash = wbp;
			}
			*wbp = *tbp;
		}
		*wbp = L'\0';
	}

	w = 0;
	for (wbp = wbuf; *wbp; ++wbp, x += cols) {
		cols = tb_wcwidth(*wbp);
		if ((w += cols) > maxcols) {
			if (w - cols > 0)
				tb_set_cell(x2, y, '~', fg, C_DEF);
			w -= cols;
			break;
		}
		tb_set_cell(x2 = x, y, (uint32_t)*wbp, fg, C_DEF);
	}
	return w;
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

static void printenttime(int x, int y, int fg, size_t *w, const time_t *timep, int useabbr)
{
	static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	struct tm t, now;

	localtime_r(timep, &t);
	if (useabbr) {
		localtime_r(&curtime, &now);
		if (t.tm_year == now.tm_year)
			tb_printf_ex(x, y, fg, C_DEF, w, " %s %2d %02d:%02d ", months[t.tm_mon], t.tm_mday, t.tm_hour, t.tm_min);
		else
			tb_printf_ex(x, y, fg, C_DEF, w, " %s %2d  %s ", months[t.tm_mon], t.tm_mday, xitoa(t.tm_year + 1900));
	} else
		tb_printf_ex(x, y, fg, C_DEF, w, " %s-%02d-%02d %02d:%02d ",
			xitoa(t.tm_year + 1900), t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
}

static void printent(int y, const Entry *ent, int sel, int mark)
{
	int x = 0;
	int fg1 = sel ? 0 : color[U_DETAIL]; // for details
	int fg2 = TB_BOLD | (mark || (sel && ptab->cfg.mansel) ? color[U_STATBAR] | TB_REVERSE // for marks
				: (gcfg.marknew && (ent->flag & E_NEW) ? color[U_NEWFILE] | TB_REVERSE : 0));
	int fg3 = color[ent->type] // for filename
				| (ent->flag & E_DIR_DIRLNK ? TB_BOLD : 0)
				| ((ent->flag & E_SELECTED) || (sel && !ptab->cfg.mansel) ? TB_REVERSE : 0)
				| (sel && ptab->cfg.mansel ? TB_UNDERLINE : 0);
	size_t w = 0;

	for (char *p = ptab->cfg.cols; *p; ++p) {
		switch (*p) {
		case 'n': tb_set_cell(x += w, y, sel ? '>' : ' ', fg2, C_DEF);
			if (ptab->hp->stat->flag != S_ROOT)
				printnstr(x + 1, y, fg3, ent->name, ncols);
			else
				printpath(x + 1, y, fg3, ent->name, ncols);
			w = ncols;
			break;
		case 's': tb_printf_ex(x += w, y, fg1, C_DEF, &w, "%7s ",
				(ent->flag & E_REG_FILE) ? tohumansize(ent->size) : filetypechar(ent->type));
			break;
		case 't': printenttime(x += w, y, fg1, &w, &ent->sec, gcfg.abbrdate);
			break;
		case 'p': if (gcfg.symbperm)
				tb_printf_ex(x += w, y, fg1, C_DEF, &w, " %c%s ", filetypechar(ent->type)[1], strperms(ent->mode));
			else
				tb_printf_ex(x += w, y, fg1, C_DEF, &w, " %c%c%c ",
					'0' + ((ent->mode >> 6) & 7), '0' + ((ent->mode >> 3) & 7), '0' + (ent->mode & 7));
			break;
		case 'o': tb_printf_ex(x += w, y, fg1, C_DEF, &w, "%7.6s:%-7.6s", getpwname(ent->uid), getgrname(ent->gid));
		}
	}
}

static void redraw(const char *path)
{
	int dcols = 0, x = 0, sp = 0, n = 0;
	int colmap[128] = {['o'] = 15, ['p'] = gcfg.symbperm ? 12 : 5, ['s'] = 8, ['t'] = gcfg.abbrdate ? 14 : 18};
	size_t w = 0;

	xcols = tb_width();
	xlines = tb_height();
	onscr = xlines - 4;
	pvcols = gcfg.showpvp ? xcols * PV_WIDTH_PCT / 100 : 0;
	ncols = xcols - pvcols - 2;
	for (signed char *p = (signed char *)ptab->cfg.cols; *p; ++p) {
		if (*p < 0)
			*p = -*p;
		dcols += colmap[(int)*p];
	}
	for (char *c, *p = COLS_HIDE_PRIO; *p; ++p) {
		if (ncols - dcols < MIN_NAME_COLS && (c = strchr(ptab->cfg.cols, *p))) {
			*(signed char *)c = -*(signed char *)c;
			dcols -= colmap[(int)*p];
		}
	}
	ncols -= dcols;
	shiftcursor(0, 0);

	// Print tabs tag
	cleararea(0, 0, xcols, 1);
	for (int i = 0; i <= TABS_MAX; ++i, x += 2) {
		if (gtab[i].cfg.enabled)
			tb_set_cell(x, 0, i < TABS_MAX ? i + '1' : '#',
				color[U_TABTAG] | (gcfg.ct == i ? TB_REVERSE : 0) | TB_BOLD, C_DEF);
		else
			tb_set_cell(x, 0, i < TABS_MAX ? '*' : '#', C_DEF, C_DEF);
	}
	// Print path
	n = xcols - (TABS_MAX + 1) * 2 - 1;
	if (home && (w = strlen(home)) && strncmp(home, path, w) == 0 && (path[w] == '/' || path[w] == '\0')) {
		path += w;
		--n;
		tb_set_cell(++x, 0, '~', color[U_PATHBAR] | TB_BOLD, C_DEF); // replace home path with '~'
	}
	printpath(++x, 0, color[U_PATHBAR] | TB_BOLD, path, n);

	// Print entries
	cleararea(0, 1, xcols - pvcols - 1, xlines - 3);
	n = MIN(onscr + curscroll, ndents);
	for (int i = curscroll, j = 2; i < n; ++i, ++j)
		printent(j, &pdents[i], i == cursel, i == markent);
	cleararea(0, xlines - 2, xcols, 1);
	if (curscroll > 0 && ncols > 0)
		tb_print(*ptab->cfg.cols == 'n' ? 1 : dcols + 1, 1, color[U_DETAIL], C_DEF, "<<");
	if (n < ndents && ncols > 0)
		tb_print(*ptab->cfg.cols == 'n' ? 1 : dcols + 1, xlines - 2, color[U_DETAIL], C_DEF, ">>");

	// Draw scroll indicator
	sp = MAX(1, ndents);
	n = (sp <= onscr) ? onscr
		: ((onscr * onscr << 1) / sp + 1) >> 1; // indicator height, round a/b by (a*2/b+1)/2
	n = MAX(1, n);
	sp = (curscroll == 0 || sp <= onscr) ? 2
		: 2 + (((curscroll * (onscr - n) << 1) / (sp - onscr) + 1) >> 1); // starting line
	for (int i = 2; i < xlines - 2; ++i)
		tb_set_cell(xcols - pvcols - 1, i, gcfg.showpvp ? 0x2502 : ' ',	color[U_DETAIL],
			(i >= sp && i < sp + n) ? color[U_DETAIL] : C_DEF);
	tb_set_cell(xcols - pvcols - 1, 1, '=', color[U_DETAIL], C_DEF);
	tb_set_cell(xcols - pvcols - 1, xlines - 2, '=', color[U_DETAIL], C_DEF);

	// Print filter
	if (ptab->ftlen != 0) {
		tb_printf_ex(0, xlines - 2, color[F_SOCK], C_DEF, &w, "Filter: %s", ptab->filt);
		tb_set_cell(w, xlines - 2, ' ', C_DEF, ptab->ftlen > 0 ? color[F_SOCK] : C_DEF);
	}

	// Print quick find
	if (ptab->fdlen > 0) {
		tb_printf_ex(0, xlines - 2, color[F_EXEC], C_DEF, &w, "Quick find: %s", ptab->find);
		tb_set_cell(w, xlines - 2, ' ', C_DEF, color[F_EXEC]);
	}
	gcfg.redrawn = 1; // set to skip fastredraw
}

static void fastredraw(void)
{
	if (gcfg.redrawn != 1 && ndents != 0) { // bypass fastredraw after a full redraw
		if (lastsel >= curscroll && lastsel < onscr + curscroll && lastsel < ndents && lastsel != cursel)
			printent(2 + lastsel - curscroll, &pdents[lastsel], FALSE, lastsel == markent);

		if (cursel >= curscroll && cursel < onscr + curscroll)
			printent(2 + cursel - curscroll, &pdents[cursel], TRUE, cursel == markent);
	}
	gcfg.redrawn = 2;
}

static void statusbar(void)
{
	int n, x = 0, u = (gcfg.runmode != 0) ? U_WARN : U_STATBAR;
	size_t w = 0;
	const char *p;
	Entry *ent;

	cleararea(0, xlines - 1, xcols, 1);
	if (errline != 0) {
		tb_printf(0, xlines - 1, color[U_WARN], C_DEF, "Failed (%s): %s", xitoa(errline), strerror(errnum));
		errline = 0;
		return;
	}
	tb_printf_ex(0, xlines - 1, color[u], C_DEF, &w, "%d/%d ", ndents > 0 ? cursel + 1 : 0, ndents);
	tb_printf_ex(x += w, xlines - 1, color[u] | TB_REVERSE, C_DEF, &w, " %d ",
		(ndents > 0 && !ptab->cfg.mansel) ? 1 : ptab->nsel);

	if (ndents == 0)
		return;
	ent = &pdents[cursel];
	tb_printf_ex(x += w, xlines - 1, color[u], C_DEF, &w, "  %c%s %s:%s  %s", filetypechar(ent->type)[1],
		strperms(ent->mode), getpwname(ent->uid), getgrname(ent->gid), tohumansize(ent->size));
	printenttime(x += w, xlines - 1, color[u], &w, &ent->sec, FALSE);

	if (ent->type == F_LNK && (n = readlink(ent->name, gpbuf, PATH_MAX - 1)) > 0) {
		gpbuf[n] = '\0';
		tb_printf(x + w, xlines - 1, color[u], C_DEF, "->%s", gpbuf); // Show symlink target

	} else if ((ent->flag & E_REG_FILE) && (p = getextension(ent->name, ent->nlen)))
		tb_print(x + w, xlines - 1, color[u], C_DEF, p); // Show file extension
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
	} else if (c == '\r' || c == TB_KEY_ESC){ // set to inactive
		ptab->ftlen = (ptab->filt[0] == '\0') ? 0 : -ptab->ftlen;
		return GO_REDRAW;

	} else if (c == TB_KEY_BACKSPACE || c == TB_KEY_DELETE || c == 127) {
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

	if (c == '\r' || c == TB_KEY_ESC) { // turn of or set to invisible
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

	} else if (c == TB_KEY_BACKSPACE || c == TB_KEY_DELETE || c == 127) {
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
	int c, ctl = GO_RELOAD;

	for (;;) {
		switch (ctl) {
		case GO_RELOAD:
			ptab = &gtab[gcfg.ct];
			loadentries(ptab->hp->path);

			// fallthrough
		case GO_SORT:
			filterentry();
			qsort(pdents, ndents, sizeof(*pdents), ptab->cfg.reverse ? &reventrycmp : &entrycmp);
			restoredirstat(ptab);

			// fallthrough
		case GO_REDRAW:
			redraw(ptab->hp->path);

			// fallthrough
		case GO_FASTDRAW:
			fastredraw();
			setpreview(2, NULL);

			// fallthrough
		case GO_STATBAR:
			statusbar();

			// fallthrough
		case GO_NONE:
			tb_present();
			c = getinput(gcfg.showpvp && gcfg.redrawn ? 25 : -1);

			if ((ctl = filterinput(c)) != GO_NONE)
				break;
			if ((ctl = qfindinput(c)) != GO_NONE)
				break;

			if (c > 0) {
				for (int i = 0; i < (int)LENGTH(keys); ++i)
					if ((c == keys[i].keysym1 || c == keys[i].keysym2) && keys[i].func)
						ctl = keys[i].func(keys[i].arg);
			} else if (c == KEY_TIMEOUT) {
				ctl = setpreview(3, NULL);
			} else if (c == KEY_RESIZE) {
				ctl = GO_REDRAW;
			} else if (c < -31)
				ctl = callextfunc(-c);

			break;
		case GO_QUIT:
			return;
		}
	}
}

static void exitsighandler(int sig __attribute__((unused)))
{
	tb_shutdown();
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

	// Set pipepath and SFF_PIPE environment variable
	if (cfgpath && makepath(cfgpath, ".sff-pipe.", gpbuf))
		pipepath = strdup(strcat(gpbuf, xitoa(getpid())));
	if (!cfgpath || !extfunc || !pipepath || setenv("SFF_PIPE", pipepath, 1) == -1)
		seterrnum(__LINE__, errno);

	// Initialize first tab
	for (char *p = gcfg.cols; *p; ++p)
		if (*p == 'n')
			*p = '@';
	if (gcfg.cols[0] == '@')
		gcfg.cols[0] = 'n';
	else
		memccpy(gcfg.cols + MIN(strlen(gcfg.cols), 4), "n", '\0', 2);
	if (!abspath(argx, gpbuf) || !inittab(gpbuf, 0) || chdir(ghpath[0].path) == -1) {
		perror(xitoa(__LINE__));
		return FALSE;
	}

	if (*xbasename(gpbuf) == '.')
		gtab[0].cfg.showhidden = 1;
	if (getuid() == 0)
		gcfg.runmode = 2;
	return TRUE;
}

static void cleanup(void)
{
	setpreview(0, NULL);
	if (pipepath)
		unlink(pipepath);
	for (int i = 0; i <= TABS_MAX; ++i)
		clearselstat(&gtab[i], -1, TRUE);
	free(pdents);
	free(pnamebuf);
	free(pfindbuf);
	free(cfgpath);
	free(extfunc);
	free(pipepath);
	free(pvbuf);
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
	if (!inittermbox())
		return EXIT_FAILURE;

	browse();

	tb_shutdown();
	return EXIT_SUCCESS;
}
