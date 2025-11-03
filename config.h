/* Default settings */
#ifdef __APPLE__
#define OPENER    "/usr/bin/open"  // File opener on macOS
#else
#define OPENER    "xdg-open"  // File opener on Linux/BSD
#endif
#define EDITOR    "vi"        // Default editor
#define SUDOER    "sudo"      // Backend for sudo mode

static Settings gcfg = {
	.showhidden = 0,  // Show hidden files
	.dirontop   = 1,  // Sort directories on the top
	.sortby     = 0,  // (0: name, 1: size, 2: time, 3: extension)
	.caseinsen  = 1,  // Case insensitive
	.natural    = 1,  // Natural numeric sorting
	.reverse    = 0,  // Reverse sort
	.showtime   = 1,  // Show time info
	.showowner  = 0,  // Show owner:group info
	.showperm   = 0,  // Show permissions info
	.showsize   = 1,  // Show size info
	.timetype   = 1,  // (0: access, 1: modify, 2: change)
};

/* Key definitions */
#define CTRL(c)     ((c) & 0x1f)
#define ESC         27
#define CTRL_UP     601
#define CTRL_DOWN   602
#define CTRL_RIGHT  603
#define CTRL_LEFT   604
#define SHIFT_UP    605
#define SHIFT_DOWN  606

static const Key keys[] = {
	// key1         key2        function      argument   comment(Up to 39 characters)
	{ KEY_UP,       'k',        movecursor,      -1,    "     Up, k  Move up" },
	{ KEY_DOWN,     'j',        movecursor,       1,    "   Down, j  Move down" },
	{ KEY_LEFT,     'h',        gotoparent,       0,    "   Left, h  Go to parent dir" },
	{ KEY_RIGHT,    'l',        enterdir,         0,    "  Right, l  Enter dir" },
	{ CTRL_UP,      CTRL('K'),  movequarterpage, -1,    "  C-Up, ^K  Quarter page up" },
	{ CTRL_DOWN,    CTRL('J'),  movequarterpage,  1,    "C-Down, ^J  Quarter page down" },
	{ KEY_PPAGE,    CTRL('B'),  scrollpage,      -1,    "  PgUp, ^B  Scroll page up" },
	{ KEY_NPAGE,    CTRL('F'),  scrollpage,       1,    "PgDown, ^F  Scroll page down" },
	{ 'B',           0,         scrolleighth,    -1,    "         B  Scroll eighth up" },
	{ 'F',           0,         scrolleighth,     1,    "         F  Scroll eighth down" },
	{ KEY_HOME,     'g',        movetoedge,      -1,    "   Home, g  Move to top" },
	{ KEY_END,      'G',        movetoedge,       1,    "    End, G  Move to bottom" },
	{ 'e',           0,         openfile,         1,    "         e  Edit file" },
	{ '\r',         KEY_ENTER,  openfile,         2,    "     Enter  Open file" },
	{ 'r',          KEY_F(5),   refreshview,      1,    "     F5, r  Reload" },
	{ '`',           0,         gotohome,         1,    "         `  Go to home dir" },
	{ '~',           0,         gotohome,         2,    "         ~  Go to root dir" },
	{ CTRL_LEFT,    CTRL('H'),  switchhistpath,   0,    "C-Left, ^H  Toggle previous path" },
	{ '1',           0,         switchtab,        0,    "         1  Tab 1" },
	{ '2',           0,         switchtab,        1,    "         2  Tab 2" },
	{ '3',           0,         switchtab,        2,    "         3  Tab 3" },
	{ '4',           0,         switchtab,        3,    "         4  Tab 4" },
	{ '5',           0,         switchtab,        4,    "         5  Search result tab" },
	{ 'q',           0,         closetab,         0,    "         q  Close tab" },
	{ ' ',           0,         toggleselection,  0,    "     Space  (Un)select current" },
	{ SHIFT_UP,     'K',        toggleselection, -1,    "  Sh-Up, K  (Un)select and move up" },
	{ SHIFT_DOWN,   'J',        toggleselection,  1,    "Sh-Down, J  (Un)select and move down" },
	{ 'a',           0,         selectall,        0,    "         a  Select all" },
	{ 'A',           0,         invertselection,  0,    "         A  Invert selection" },
	{ CTRL('A'),    ESC,        clearselection,   0,    "   Esc, ^A  Clear selection" },
	{ 'm',           0,         selectrange,      1,    "         m  Select range" },
	{ 'M',           0,         selectrange,     -1,    "         M  Deselect range" },
	{ '/',           0,         setfilter,        1,    "         /  (Un)filter" },
	{ 'f',           0,         quickfind,        0,    "         f  Quick find" },
	{ 'n',           0,         qfindnext,        1,    "         n  Find next" },
	{ 'N',           0,         qfindnext,       -1,    "         N  Find previous" },
	{ CTRL('T'),     0,         togglemode,       0,    "        ^T  Toggle sudo mode" },
	{ 'o',           0,         viewoptions,      0,    "         o  View options" },
	{ '?',          KEY_F(1),   showhelp,         0,    "     F1, ?  Show this help" },
	{ 'Q',           0,         quitsff,          0,    "         Q  Quit" },
};

/* Color definitions for 256 color*/
static void setcolorpair256(void)
{
	        // type        fg color     bg color  (default color: -1)
	init_pair( F_REG,           -1,      -1 ); // Regular file
	init_pair( F_DIR,     COLOR_BLUE,    -1 ); // Directory
	init_pair( F_LNK,     COLOR_CYAN,    -1 ); // Symbolic link
	init_pair( F_CHR,     COLOR_YELLOW,  -1 ); // Char device
	init_pair( F_BLK,     COLOR_YELLOW,  -1 ); // Block device
	init_pair( F_IFO,     COLOR_YELLOW,  -1 ); // FIFO
	init_pair( F_SOCK,    COLOR_MAGENTA, -1 ); // Socket
	init_pair( F_HLNK,    COLOR_MAGENTA, -1 ); // Hard link
	init_pair( F_EXEC,    COLOR_GREEN,   -1 ); // Executable
	init_pair( C_DETAIL,        245,     -1 ); // Detail info, Grey54
	init_pair( C_TABTAG,  COLOR_YELLOW,  -1 ); // Tabs tag
	init_pair( C_PATHBAR, COLOR_YELLOW,  -1 ); // Path bar
	init_pair( C_STATBAR, COLOR_YELLOW,  -1 ); // Status bar
	init_pair( C_WARN,          196,     -1 ); // Warning, Red1
	init_pair( C_NEWFILE,       168,     -1 ); // New file, HotPink3
}

/* Color definitions for 8 color*/
static void setcolorpair8(void)
{
	        // type        fg color     bg color  (default color: -1)
	init_pair( F_REG,           -1,      -1 ); // Regular file
	init_pair( F_DIR,     COLOR_BLUE,    -1 ); // Directory
	init_pair( F_LNK,     COLOR_CYAN,    -1 ); // Symbolic link
	init_pair( F_CHR,     COLOR_YELLOW,  -1 ); // Char device
	init_pair( F_BLK,     COLOR_YELLOW,  -1 ); // Block device
	init_pair( F_IFO,     COLOR_YELLOW,  -1 ); // FIFO
	init_pair( F_SOCK,    COLOR_MAGENTA, -1 ); // Socket
	init_pair( F_HLNK,    COLOR_MAGENTA, -1 ); // Hard link
	init_pair( F_EXEC,    COLOR_GREEN,   -1 ); // Executable
	init_pair( C_DETAIL,        -1,      -1 ); // Detail info
	init_pair( C_TABTAG,  COLOR_YELLOW,  -1 ); // Tabs tag
	init_pair( C_PATHBAR, COLOR_YELLOW,  -1 ); // Path bar
	init_pair( C_STATBAR, COLOR_YELLOW,  -1 ); // Status bar
	init_pair( C_WARN,    COLOR_RED,     -1 ); // Warning
	init_pair( C_NEWFILE, COLOR_MAGENTA, -1 ); // New file
}
