/*	$Csoft: window.h,v 1.23 2002/06/09 10:08:08 vedge Exp $	*/
/*	Public domain	*/

#include <engine/widget/region.h>

enum window_type {
	WINDOW_SOLID,		/* Plain, no decorations. */
	WINDOW_GRADIENT,	/* Blue/red gradient. */
	WINDOW_CUBIC,		/* Weird algorithm #1 */
	WINDOW_CUBIC2		/* Weird algorithm #2 */
};

enum window_event {
	WINDOW_MOUSEBUTTONUP,
	WINDOW_MOUSEBUTTONDOWN,
	WINDOW_KEYUP,
	WINDOW_KEYDOWN
};

TAILQ_HEAD(regionsq, region);

struct window {
	struct	 object obj;

	/* Read-only once attached */
	int	 flags;
#define WINDOW_PLAIN		0x01	/* Solid, no borders */
#define WINDOW_ANIMATE		0x04	/* Redraw each tick */
#define WINDOW_TITLEBAR		0x08	/* Draw title bar */
#define WINDOW_ROUNDEDGES	0x10	/* Round edges */
#define WINDOW_ABSOLUTE		0x20	/* Requested coordinates/geometry
					   is absolute (scaled otherwise) */
#define WINDOW_SHOW		0x40	/* Visible */
	enum	 window_type type;
	char	*caption;		/* Titlebar text */
	Uint32	 bgcolor, fgcolor;	/* Gradient colors, if applicable */
	Uint32	*border;		/* Border colors */
	int	 borderw;		/* Border width */
	int	 titleh;		/* Titlebar height */
	int	 x, y;			/* Absolute coordinates */
	int	 w, h;			/* Geometry */
	int	 spacing;		/* Spacing between regions */
	SDL_Rect body;			/* Area reserved for regions */
	struct	 viewport *view;	/* Parent view */
	SDL_Rect vmask;			/* View mask (units) */

	/* Read-write, thread-safe */
	int	 redraw;		/* Redraw at next tick */
	struct	 widget *focus;		/* Focused widget */

	struct	 regionsq regionsh;
	TAILQ_ENTRY(window) windows;	/* Windows in view */
	pthread_mutex_t	lock;
};

#define WINDOW(w)	((struct window *)(w))

#ifdef DEBUG

# define WINDOW_PUT_PIXEL(win, wrx, wry, c) do {			\
	if ((wrx) > (win)->w || (wry) > (win)->h ||			\
	    (wrx) < 0 || (wry) < 0) {					\
		fatal("%s: %d,%d > %dx%d\n", OBJECT(win)->name,		\
		    (wrx), (wry), (win)->w, (win)->h);			\
	}								\
	VIEW_PUT_PIXEL((win)->view->v, (win)->x+(wrx),			\
	    (win)->y+(wry), (c));					\
} while (/*CONSTCOND*/0)

# define WINDOW_PUT_ALPHAPIXEL(win, wrx, wry, c, wa) do {		\
	if ((wrx) > (win)->w || (wry) > (win)->h ||			\
	    (wrx) < 0 || (wry) < 0) {					\
		fatal("%s: %d,%d > %dx%d\n", OBJECT(win)->name,		\
		    (wrx), (wry), (win)->w, (win)->h);			\
	}								\
	VIEW_PUT_ALPHAPIXEL((win)->view->v, (win)->x+(wrx),		\
	    (win)->y+(wry), (c), (wa));					\
} while (/*CONSTCOND*/0)

#else

# define WINDOW_PUT_PIXEL(win, wrx, wry, c)				\
 	 VIEW_PUT_PIXEL((win)->view->v, (win)->x+(wrx), (win)->y+(wry),	\
	    (c))

# define WINDOW_PUT_ALPHAPIXEL(win, wrx, wry, c) \
	VIEW_PUT_ALPHAPIXEL((win)->view->v, (win)->x+(wrx), (win)->y+(wry), (c))

#endif

#define WINDOW_SURFACE(win)	((win)->view->v)

#define WINDOW_INSIDE(wina, xa, ya)					\
	((xa) > (wina)->x		&& (ya) > (wina)->y &&		\
	 (xa) < ((wina)->x+(wina)->w)	&& (ya) < ((wina)->y+(wina)->h))

struct window	*window_new(char *, int, enum window_type, int, int, int, int);
void	 	 window_init(struct window *, struct viewport *, char *,
		     int, enum window_type, int, int, int, int);

void	 window_destroy(void *);
void	 window_attach(void *, void *);
void	 window_detach(void *, void *);

int	 window_show(struct window *);
int	 window_hide(struct window *);
void	 window_draw(struct window *);
int	 window_event_all(struct viewport *, SDL_Event *);
void	 window_resize(struct window *);

