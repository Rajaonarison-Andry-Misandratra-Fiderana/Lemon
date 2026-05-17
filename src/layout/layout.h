static void tile(Monitor *m);
static void center_tile(Monitor *m);
static void right_tile(Monitor *m);
static void overview(Monitor *m);
static void grid(Monitor *m);
static void scroller(Monitor *m);
static void deck(Monitor *mon);
static void monocle(Monitor *m);
static void vertical_tile(Monitor *m);
static void vertical_overview(Monitor *m);
static void vertical_grid(Monitor *m);
static void vertical_scroller(Monitor *m);
static void vertical_deck(Monitor *mon);
static void dwindle(Monitor *m);
static void fair(Monitor *m);
static void vertical_fair(Monitor *m);

Layout overviewlayout = {"󰃇", overview, "overview"};

enum {
	TILE,
	SCROLLER,
	GRID,
	MONOCLE,
	DECK,
	CENTER_TILE,
	VERTICAL_SCROLLER,
	VERTICAL_TILE,
	VERTICAL_GRID,
	VERTICAL_DECK,
	RIGHT_TILE,
	DWINDLE,
	FAIR,
	VERTICAL_FAIR,
};

Layout layouts[] = {

	{"T", tile, "tile", TILE},
	{"S", scroller, "scroller", SCROLLER},
	{"G", grid, "grid", GRID},
	{"M", monocle, "monocle", MONOCLE},
	{"K", deck, "deck", DECK},
	{"CT", center_tile, "center_tile", CENTER_TILE},
	{"RT", right_tile, "right_tile", RIGHT_TILE},
	{"VS", vertical_scroller, "vertical_scroller",
	 VERTICAL_SCROLLER},
	{"VT", vertical_tile, "vertical_tile", VERTICAL_TILE},
	{"VG", vertical_grid, "vertical_grid", VERTICAL_GRID},
	{"VK", vertical_deck, "vertical_deck", VERTICAL_DECK},
	{"DW", dwindle, "dwindle", DWINDLE},
	{"F", fair, "fair", FAIR},
	{"VF", vertical_fair, "vertical_fair", VERTICAL_FAIR},
};
