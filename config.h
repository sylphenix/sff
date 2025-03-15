/* Default settings */
#define OPENER    "xdg-open"  // Default opener
#define EDITOR    "nano"        // Default editor
#define SUDOER    "doas"      // Default sudo utility

static Settings gcfg = {
	.showhidden = 1,  // Show hidden files
	.dirontop   = 1,  // Sort directories on the top
	.sortby     = 0,  // (0: name, 1: size, 2: time, 3: extension)
	.caseinsen  = 1,  // Case insensitive
	.natural    = 1,  // Natural numeric sorting
	.reverse    = 0,  // Reverse sort
	.showtime   = 1,  // Show time info
	.showowner  = 0,  // Show owner info
	.showperm   = 0,  // Show permission info
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
	{ 'r',          KEY_F(5),   refreshview,      1,    "     F5, r  Refresh" },
	{ '`',           0,         gotohome,         1,    "         `  Go to home dir" },
	{ '~',           0,         gotohome,         2,    "         ~  Go to root dir" },
	{ CTRL_LEFT,    CTRL('H'),  switchhistpath,   0,    "C-Left, ^H  Toggle previous path" },
	{ '1',           0,         switchtab,        0,    "         1  Tab 1" },
	{ '2',           0,         switchtab,        1,    "         2  Tab 2" },
	{ '3',           0,         switchtab,        2,    "         3  Tab 3" },
	{ '4',           0,         switchtab,        3,    "         4  Tab 4" },
	{ '5',           0,         switchtab,        4,    "         5  Search result tab" },
	{ 'q',           0,         closetab,        -1,    "         q  Close tab" },
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
	{ 'T',           0,         togglemode,       3,    "         T  Toggle browse mode" },
	{ CTRL('T'),     0,         togglemode,       1,    "        ^T  Toggle sudo mode" },
	{ 'o',           0,         viewoptions,      0,    "         o  View options" },
	{ '?',          KEY_F(1),   showhelp,         0,    "     F1, ?  Show this help" },
	{  0,            0,         NULL,             0,    "     Alt-/  Extension functions help" },
	{ 'Q',           0,         quitsff,          1,    "         Q  Quit" },
};

/* Color definitions for 256 color*/
static void setcolorpair256(void)
{
	        // type      fg color  bg color  (default color: -1)
	init_pair( F_REG,       -1,    -1 ); // Regular file, Default
	init_pair( F_DIR,       39,    -1 ); // Directory, DeepSkyBlue1
	init_pair( F_LNK,       51,    -1 ); // Symbolic link, Cyan1
	init_pair( F_CHR,      226,    -1 ); // Char device, Yellow1
	init_pair( F_BLK,      193,    -1 ); // Block device, DarkSeaGreen1
	init_pair( F_IFO,      214,    -1 ); // FIFO, Orange1
	init_pair( F_SOCK,     171,    -1 ); // Socket, MediumOrchid1
	init_pair( F_HLNK,      96,    -1 ); // Hard link, Plum4
	init_pair( F_EXEC,      46,    -1 ); // Executable, Green1
	init_pair( C_DETAIL,   246,    -1 ); // Detail info, Grey62
	init_pair( C_TABTAG,   226,    -1 ); // Tabs tag, Yellow1
	init_pair( C_PATHBAR,  214,    -1 ); // Path bar, Orange1
	init_pair( C_STATBAR,  214,    -1 ); // Status bar, Orange1
	init_pair( C_WARN,     196,    -1 ); // Warning, Red1
	init_pair( C_NEWFILE,  168,    -1 ); // New file, DeepPink1
}

/* Color definitions for 8 color*/
static void setcolorpair8(void)
{
	        // type     fg color  bg color  (default color: -1)
	init_pair( F_REG,      -1,    -1 ); // Regular file, Default
	init_pair( F_DIR,       4,    -1 ); // Directory, Blue
	init_pair( F_LNK,       5,    -1 ); // Symbolic link, Magenta
	init_pair( F_CHR,       3,    -1 ); // Char device, Yellow
	init_pair( F_BLK,       3,    -1 ); // Block device, Yellow
	init_pair( F_IFO,       5,    -1 ); // FIFO, Magenta
	init_pair( F_SOCK,      5,    -1 ); // Socket, Magenta
	init_pair( F_HLNK,     -1,    -1 ); // Hard link, Default
	init_pair( F_EXEC,      2,    -1 ); // Executable, Green
	init_pair( C_DETAIL,   -1,    -1 ); // Detail info, Default
	init_pair( C_TABTAG,    3,    -1 ); // Tabs tag, Yellow
	init_pair( C_PATHBAR,   3,    -1 ); // Path bar, Yellow
	init_pair( C_STATBAR,   3,    -1 ); // Status bar, Yellow
	init_pair( C_WARN,      1,    -1 ); // Warning, Red
	init_pair( C_NEWFILE,   5,    -1 ); // New file, Magenta
}
