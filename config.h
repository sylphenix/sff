/* Default settings */
#ifdef __APPLE__
#define OPENER    "open"      // File opener on macOS
#else
#define OPENER    "xdg-open"  // File opener on Linux/BSD
#endif
#define MIN_NAME_COLS  32     // Hide detail columns if name column is narrower than this value
#define COLS_HIDE_PRIO "opts" // Hide priority of detail columns, leftmost first. 'o'wner, 'p'erm, 't'ime, 's'ize
#define PV_WIDTH_PCT   50     // Preview pane width as a percentage of screen

static Settings gcfg = {
	.cols = "tOPsn",  // Columns: 't'ime, 'o'wner, 'p'erm, 's'ize, 'n'ame, Uppercase for placeholders
	.showhidden = 0,  // Show hidden files
	.dirontop   = 1,  // Sort directories on the top
	.sortby     = 0,  // (0: name, 1: size, 2: time, 3: extension)
	.natural    = 1,  // Natural numeric sorting
	.reverse    = 0,  // Reverse sort
	.timetype   = 1,  // (0: access, 1: modify, 2: change)
	.openfile   = 0,  // Open files on right arrow or 'l' key
	.symbperm   = 0,  // Show permissions as symbolic strings
	.abbrdate   = 0,  // Use ls-style date format
};

/* Key definitions */
#define CTRL_(c)     ((c) & 0x1f)
#define KEY_TIMEOUT  -1
#define KEY_RESIZE   -2
#define CTRL_UP      601
#define CTRL_DOWN    602
#define SHIFT_UP     603
#define SHIFT_DOWN   604

static const Key keys[] = {
	// key1                key2    function      argument     comment
	{ TB_KEY_ARROW_UP,     'k',   movecursor,      -1,    "     Up, k  Move up" },
	{ TB_KEY_ARROW_DOWN,   'j',   movecursor,       1,    "   Down, j  Move down" },
	{ TB_KEY_ARROW_LEFT,   'h',   gotoparent,       0,    "   Left, h  Go to parent dir" },
	{ TB_KEY_ARROW_RIGHT,  'l',   enterdir,         0,    "  Right, l  Enter dir" },
	{ CTRL_UP,       CTRL_('K'),  movequarterpage, -1,    "  C-Up, ^K  Quarter page up" },
	{ CTRL_DOWN,     CTRL_('J'),  movequarterpage,  1,    "C-Down, ^J  Quarter page down" },
	{ TB_KEY_PGUP,   CTRL_('B'),  scrollpage,      -1,    "  PgUp, ^B  Scroll page up" },
	{ TB_KEY_PGDN,   CTRL_('F'),  scrollpage,       1,    "PgDown, ^F  Scroll page down" },
	{ 'B',                  0,    scrolleighth,    -1,    "         B  Scroll eighth up" },
	{ 'F',                  0,    scrolleighth,     1,    "         F  Scroll eighth down" },
	{ TB_KEY_HOME,         'g',   movetoedge,      -1,    "   Home, g  Move to top" },
	{ TB_KEY_END,          'G',   movetoedge,       1,    "    End, G  Move to bottom" },
	{ '-',                  0,    switchhistpath,   0,    "         -  Toggle previous path" },
	{ 'r',                  0,    refreshview,      1,    "         r  Reload & toggle new marks" },
	{ TB_KEY_ENTER,        '\r',  enterdir,         1,    " Enter, ^M  Open file" },
	{ '`',                  0,    gotohome,         0,    "         `  Go to home dir" },
	{ '1',                  0,    switchtab,        0,    "         1  Tab 1" },
	{ '2',                  0,    switchtab,        1,    "         2  Tab 2" },
	{ '3',                  0,    switchtab,        2,    "         3  Tab 3" },
	{ '4',                  0,    switchtab,        3,    "         4  Tab 4" },
	{ '5',                  0,    switchtab,        4,    "         5  Search result tab" },
	{ 'q',                  0,    closetab,         0,    "         q  Close tab" },
	{ ' ',                  0,    toggleselection,  0,    "     Space  (Un)select current" },
 	{ SHIFT_UP,            'K',   toggleselection, -1,    "  Sh-Up, K  (Un)select and move up" },
	{ SHIFT_DOWN,          'J',   toggleselection,  1,    "Sh-Down, J  (Un)select and move down" },
	{ CTRL_('A'),           0,    selectall,        0,    "        ^A  Select all" },
	{ 'A',                  0,    selectall,       -1,    "         A  Invert selection" },
	{ TB_KEY_ESC,          '[',   clearselection,   0,    "    Esc, [  Clear selection" },
	{ 'm',                  0,    selectrange,      1,    "         m  Select range" },
	{ 'M',                  0,    selectrange,     -1,    "         M  Deselect range" },
	{ '/',                  0,    setfilter,        1,    "         /  Toggle filter" },
	{ 'f',                  0,    quickfind,        0,    "         f  Quick find" },
	{ 'n',                  0,    qfindnext,        1,    "         n  Find next" },
	{ 'N',                  0,    qfindnext,       -1,    "         N  Find previous" },
	{ CTRL_('T'),           0,    togglemode,       0,    "        ^T  Toggle sudo mode" },
	{ 'o',                  0,    viewoptions,      0,    "         o  View options" },
	{ 'u',                  0,    prefixkey,        0,    "         u  Extension function prefix" },
	{ TB_KEY_F1,           '?',   showhelp,         0,    "     F1, ?  Show this help" },
	{ 'Q',                  0,    quitsff,          0,    "         Q  Quit" },
};

/* Color definitions for 256-color */
enum colordef {
	C_DEF     = TB_DEFAULT, // Default color
	C_RED     = 1,    // Standard palette
	C_GREEN   = 2,    // Standard palette
	C_YELLOW  = 3,    // Standard palette
	C_BLUE    = 4,    // Standard palette
	C_MAGENTA = 5,    // Standard palette
	C_CYAN    = 6,    // Standard palette
	C_WHITE   = 7,    // Standard palette
	C_BR_BLACK   = 8,    // Bright palette
	C_BR_RED     = 9,    // Bright palette
	C_BR_GREEN   = 10,   // Bright palette
	C_BR_YELLOW  = 11,   // Bright palette
	C_BR_BLUE    = 12,   // Bright palette
	C_BR_MAGENTA = 13,   // Bright palette
	C_BR_CYAN    = 14,   // Bright palette
	C_BR_WHITE   = 15,   // Bright palette
	C_GREY    = 244,  // Grey50
	C_PINK    = 168,  // HotPink3
};

static int color[] = {
	// type          color
	[F_REG]      =  C_DEF,       // Regular file
	[F_DIR]      =  C_BR_BLUE,   // Directory
	[F_LNK]      =  C_CYAN,      // Symbolic link
	[F_CHR]      =  C_YELLOW,    // Char device
	[F_BLK]      =  C_BR_YELLOW, // Block device
	[F_IFO]      =  C_YELLOW,    // FIFO
	[F_SOCK]     =  C_MAGENTA,   // Socket
	[F_HLNK]     =  C_MAGENTA,   // Hard link
	[F_EXEC]     =  C_GREEN,     // Executable
	[U_DETAIL]   =  C_GREY,      // Detail columns
	[U_TABTAG]   =  C_BR_YELLOW, // Tabs tag
	[U_PATHBAR]  =  C_YELLOW,    // Path bar
	[U_STATBAR]  =  C_YELLOW,    // Status bar
	[U_WARN]     =  C_RED,       // Warning
	[U_NEWFILE]  =  C_BR_MAGENTA // New file mark
};
