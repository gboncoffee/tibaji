static const char *font = "Liberation Sans:size=12";

#define BORDER_WIDTH 5
#define BAR_PADDING 3
/* Comment out this if you want to set the background with other
 * application. */
#define BACKGROUND 0x757373
#define BORDER_COLOR 0x000000
#define BORDER_FOCUS 0x52aaad
#define BAR_BACKGROUND 0xffffff
#define BAR_FOREGROUND 0X000000

/* Maximum number of characters of the window name drawn on the bar.
 * Should allow to the buttons after it (i.e., should be < 1/5 of the screen. */
#define MAX_WNAME_CHAR 30

#define MODMASK (Mod4Mask)
#define FULLSCREEN_KEY XK_f
#define RESIZE_KEY XK_r
#define REDRAW_KEY XK_a
#define DETACH_KEY XK_d

/* Space in pixels between buttons. */
#define MENU_PADDING 5

static const char *acme[] = { "acme", NULL };
static const char *term[] = { "alacritty", NULL };
static const char *brow[] = { "chromium", NULL };
static const char *volp[] = { "pamixer", "-i", "5", NULL };
static const char *volm[] = { "pamixer", "-d", "5", NULL };
static const char *mute[] = { "pamixer", "-t", NULL };
static const char *brip[] = { "brightnessctl", "set", "+5%", NULL };
static const char *brim[] = { "brightnessctl", "set", "5%-", NULL };

/* The last two does not matter. */
static MenuItem items[] = {
	{ "acme", acme, 4, 0, 0 },
	{ "term", term, 4, 0, 0 },
	{ "browser", brow, 7, 0, 0 },
	{ "vol+", volp, 4, 0, 0 },
	{ "vol-", volm, 4, 0, 0 },
	{ "mute", mute, 4, 0, 0 },
	{ "bri+", brip, 4, 0, 0 },
	{ "bri-", brim, 4, 0, 0 },
};
