#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/Xutil.h>
#include <X11/X.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>

#define LENGTH(X) (sizeof X / sizeof X[0])

/*
 * Copyright (C) 2024  Gabriel de Brito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

typedef struct Client {
	unsigned char *name;
	Window id;
	short int is_float;
	struct Client *next;
	struct Client *prev;
} Client;

typedef struct Workspace {
	unsigned int n_cli;
	unsigned int n_float;
	Client *clients;
	Client *floats;
	Client *current;
	struct Workspace *next;
	struct Workspace *prev;
} Workspace;

typedef struct Cursors {
	Cursor left_ptr;
	Cursor sizing;
} Cursors;

typedef struct {
	const char *label;
	const char **command;
	unsigned int size;
	unsigned int x;
	unsigned int w;
} MenuItem;

typedef struct {
	unsigned int n_works;
	unsigned int n_cur;
	Workspace *workspaces;
	unsigned int n_hidden;
	Client *hidden;
	unsigned int bar_height;
	int bar_y;
	Window bar;
	Window cli_win;
	int screen;
	int sw;
	int sh;
	Window root;
	KeyCode fkey;
	KeyCode dkey;
	KeyCode akey;
	KeyCode rkey;
	Display *dpy;
	Cursors cursors;
	unsigned char *status;
	XftFont *xftfont;
	XRenderColor xrcolor;
	XftColor xftcolor;
	int hidex;
	int hidew;
	int zoomx;
	int zoomw;
	int closex;
	int closew;
	int floatx;
	int floatw;
	int movex;
	int movew;
	int wx;
	int ww;
} Wm;

#include "config.h"

static const char *wincmds[] = {
	"Hide",
	"Zoom",
	"Close",
	"Float",
	"Move",
};

void handle_event(Wm *wm, XEvent *ev);

int error_handler(Display *dpy, XErrorEvent *_e)
{
	return 0;
}

void child_handler(int _a)
{
	wait(NULL);
}

typedef struct {
	Client *c;
	Workspace *w;
	short int is_float;
} FindResult;

FindResult find_window(Wm *wm, Window win)
{
	Workspace *w;
	Client *c;
	FindResult r;

	r.c = NULL;
	r.w = NULL;
	r.is_float = 0;

	for (w = wm->workspaces; w != NULL; w = w->next) {
		for (c = w->clients; c != NULL; c = c->next) {
			if (c->id == win) {
				r.c = c;
				r.w = w;
				return r;
			}
		}
		for (c = w->floats; c != NULL; c = c->next) {
			if (c->id == win) {
				r.c = c;
				r.w = w;
				r.is_float = 1;
				return r;
			}
		}
	}

	for (c = wm->hidden; c != NULL; c = c->next) {
		if (c->id == win) {
			r.c = c;
			return r;
		}
	}

	return r;
}

short int managed(Wm *wm, Window win)
{
	return (find_window(wm, win).c != NULL);
}

void update_status(Wm *wm)
{
	XTextProperty prop;
	if (wm->status != NULL)
		XFree(wm->status);
	wm->status = NULL;
	if (XGetWMName(wm->dpy, wm->root, &prop))
		wm->status = (unsigned char*) prop.value;
}

void render_bar(Wm *wm)
{
	int size;
	int i;
	int y;
	char wp, wn;
	Client *cli;
	XGlyphInfo extents;
	unsigned char buf[128];

	XClearWindow(wm->dpy, wm->bar);
	XftDraw	*xftdraw = XftDrawCreate(wm->dpy, wm->bar, DefaultVisual(wm->dpy, 0), DefaultColormap(wm->dpy, 0));

	if ((cli = wm->workspaces->current) != NULL) {
		size = strlen((char*) cli->name);
		size = size < MAX_WNAME_CHAR ? size : MAX_WNAME_CHAR;
		XftTextExtentsUtf8(wm->dpy, wm->xftfont, (unsigned char*) cli->name, size, &extents);
		XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, extents.x, wm->bar_y, cli->name, size);
	} else {
		XftTextExtentsUtf8(wm->dpy, wm->xftfont, (unsigned char*) "<no client>", 11, &extents);
		XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, extents.x, wm->bar_y, (unsigned char*) "<no client>", 11);
	}

	y = wm->bar_y;

	/* Workspace thing. */
	wp = wm->workspaces->prev == NULL ? ' ' : '<';
	wn = wm->workspaces->next == NULL ? ' ' : '>';
	sprintf((char*) buf, "%c%d%c", wp, wm->n_works, wn);
	size = strlen((char*) buf);
	XftTextExtentsUtf8(wm->dpy, wm->xftfont, buf, size, &extents);
	wm->ww = extents.x + extents.width;
	XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, extents.x + wm->wx, y, buf, size);

	/* Buttons. */
	XftTextExtentsUtf8(wm->dpy, wm->xftfont, (unsigned char*) wincmds[0], 4, &extents);
	wm->hidex = wm->sw / 4 + extents.x;
	wm->hidew = extents.width;
	XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, wm->hidex, y, (unsigned char*) wincmds[0], 4);

	XftTextExtentsUtf8(wm->dpy, wm->xftfont, (unsigned char*) wincmds[1], 4, &extents);
	wm->zoomx = wm->hidex + wm->hidew + extents.x + MENU_PADDING;
	wm->zoomw = extents.width;
	XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, wm->zoomx, y, (unsigned char*) wincmds[1], 4);

	XftTextExtentsUtf8(wm->dpy, wm->xftfont, (unsigned char*) wincmds[2], 5, &extents);
	wm->closex = wm->zoomx + wm->zoomw + extents.x + MENU_PADDING;
	wm->closew = extents.width;
	XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, wm->closex, y, (unsigned char*) wincmds[2], 5);

	XftTextExtentsUtf8(wm->dpy, wm->xftfont, (unsigned char*) wincmds[3], 5, &extents);
	wm->floatx = wm->closex + wm->closew + extents.x + MENU_PADDING;
	wm->floatw = extents.width;
	XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, wm->floatx, y, (unsigned char*) wincmds[3], 5);

	XftTextExtentsUtf8(wm->dpy, wm->xftfont, (unsigned char*) wincmds[4], 4, &extents);
	wm->movex = wm->floatx + wm->floatw + extents.x + MENU_PADDING;
	wm->movew = extents.width;
	XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, wm->movex, y, (unsigned char*) wincmds[4], 4);

	/* Custom buttons. */
	XftTextExtentsUtf8(wm->dpy, wm->xftfont, (unsigned char*) items[0].label, items[0].size, &extents);
	items[0].x = wm->movex + wm->movew + extents.x + MENU_PADDING * 3;
	items[0].w = extents.width;
	XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, items[0].x, y, (unsigned char*) items[0].label, items[0].size);

	for (i = 1; i < LENGTH(items); i++) {
		XftTextExtentsUtf8(wm->dpy, wm->xftfont, (unsigned char*) items[i].label, items[i].size, &extents);
		items[i].x = items[i-1].x + items[i-1].w + extents.x + MENU_PADDING;
		items[i].w = extents.width;
		XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, items[i].x, y, (unsigned char*) items[i].label, items[i].size);
	}

	if (wm->status != NULL) {
		size = strlen((char*) wm->status);
		XftTextExtentsUtf8(wm->dpy, wm->xftfont, wm->status, size, &extents);
		XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, extents.x + (wm->sw - extents.width), y, wm->status, size);
	}

	XftDrawDestroy(xftdraw);
}

void movewin(Wm *wm)
{
	XEvent ev;
	Window _dumb;
	unsigned int _dumbu;
	int x, y;
	unsigned int w, h;
	Client *c = wm->workspaces->current;
	short int resizing = 0;

	if (c == NULL || !c->is_float)
		return;

	XGetGeometry(wm->dpy, c->id, &_dumb, &x, &y, &w, &h, &_dumbu, &_dumbu);

	XWarpPointer(wm->dpy, wm->root, wm->root, 0, 0, 0, 0, x + w/2, y + h/2);
	XFlush(wm->dpy);

	XGrabPointer(
		wm->dpy,
		wm->root,
		True,
		PointerMotionMask | ButtonPressMask,
		GrabModeAsync,
		GrabModeAsync,
		None,
		wm->cursors.sizing,
		CurrentTime);

	for (;;) {
		XSync(wm->dpy, False);
		XNextEvent(wm->dpy, &ev);

		switch (ev.type) {
		case MotionNotify:
			if (resizing) {
				XGetGeometry(wm->dpy, c->id, &_dumb, &x, &y, &w, &h, &_dumbu, &_dumbu);
				if (ev.xmotion.x_root - x > 0 && ev.xmotion.y_root - y > 0)
					XResizeWindow(wm->dpy, c->id, ev.xmotion.x_root - x, ev.xmotion.y_root - y);
			} else {
				XGetGeometry(wm->dpy, c->id, &_dumb, &x, &y, &w, &h, &_dumbu, &_dumbu);
				XMoveWindow(wm->dpy, c->id, ev.xmotion.x_root - w/2, ev.xmotion.y_root - h/2);
			}
			break;
		case ButtonPress:
			if (ev.xbutton.button == Button3) {
				resizing = !resizing;
				if (resizing) {
					XGetGeometry(wm->dpy, c->id, &_dumb, &x, &y, &w, &h, &_dumbu, &_dumbu);
					XWarpPointer(wm->dpy, None, wm->root, 0, 0, 0, 0, x + w, y + h);
				} else {
					XGetGeometry(wm->dpy, c->id, &_dumb, &x, &y, &w, &h, &_dumbu, &_dumbu);
					XWarpPointer(wm->dpy, None, wm->root, 0, 0, 0, 0, x + w/2, y + h/2);
				}
			} else {
				goto out;
			}
			break;
		default:
			handle_event(wm, &ev);
			break;
		}
	}
out:
	XUngrabPointer(wm->dpy, CurrentTime);
}

void update_view(Wm *wm)
{
	Workspace *cur = wm->workspaces;
	Client *c;
	int height;
	int n;

	if (cur->current == NULL)
		goto bar;

	for (c = cur->floats; c != NULL; c = c->next) {
		XLowerWindow(wm->dpy, c->id);
		XSetWindowBorder(wm->dpy, c->id, BORDER_COLOR);
		XGrabButton(wm->dpy,
			AnyButton,
			AnyModifier,
			c->id,
			False,
			ButtonPressMask,
			GrabModeAsync,
			GrabModeSync,
			None,
			None);
		n++;
	}

	if (cur->n_cli == 0)
		goto focus;

	if (cur->n_cli == 1) {
		XLowerWindow(wm->dpy, cur->clients->id);
		XMoveResizeWindow(wm->dpy, cur->clients->id, 0, wm->bar_height, wm->sw - BORDER_WIDTH * 2, wm->sh - wm->bar_height - BORDER_WIDTH * 2);
		XSetWindowBorder(wm->dpy, cur->clients->id, BORDER_COLOR);
		XGrabButton(wm->dpy,
			AnyButton,
			AnyModifier,
			cur->clients->id,
			False,
			ButtonPressMask,
			GrabModeAsync,
			GrabModeSync,
			None,
			None);
		goto focus;
	}

	XLowerWindow(wm->dpy, cur->clients->id);
	XMoveResizeWindow(wm->dpy, cur->clients->id, 0, wm->bar_height, wm->sw / 2 - BORDER_WIDTH * 2, wm->sh - BORDER_WIDTH * 2 - wm->bar_height);
	XSetWindowBorder(wm->dpy, cur->clients->id, BORDER_COLOR);
	XGrabButton(wm->dpy,
		AnyButton,
		AnyModifier,
		cur->clients->id,
		False,
		ButtonPressMask,
		GrabModeAsync,
		GrabModeSync,
		None,
		None);

	n = 0;
	height = (wm->sh - wm->bar_height) / (cur->n_cli - 1);

	for (c = cur->clients->next; c != NULL; c = c->next) {
		XLowerWindow(wm->dpy, c->id);
		XMoveResizeWindow(wm->dpy, c->id, wm->sw / 2, n * height + wm->bar_height, wm->sw / 2 - BORDER_WIDTH * 2, height - BORDER_WIDTH * 2);
		XSetWindowBorder(wm->dpy, c->id, BORDER_COLOR);
		XGrabButton(wm->dpy,
			AnyButton,
			AnyModifier,
			c->id,
			False,
			ButtonPressMask,
			GrabModeAsync,
			GrabModeSync,
			None,
			None);
		n++;
	}

focus:
	XSetWindowBorder(wm->dpy, cur->current->id, BORDER_FOCUS);
	XSetInputFocus(wm->dpy, cur->current->id, RevertToParent, CurrentTime);
	XUngrabButton(wm->dpy, AnyButton, AnyModifier, cur->current->id);

bar:
	render_bar(wm);
}

void new_workspace(Wm *wm)
{
	Workspace *w = wm->workspaces;
	w->next = malloc(sizeof(Workspace));
	assert(w->next != NULL && "Buy more ram lol");

	w->next->prev = w;
	w->next->next = NULL;
	wm->n_works++;

	w->next->n_cli = 0;
	w->next->n_float = 0;
	w->next->clients = NULL;
	w->next->floats = NULL;
	w->next->current = NULL;
}

void manage(Wm *wm, Window win)
{
	XTextProperty prop;
	Client *new = malloc(sizeof(Client));
	assert(new != NULL && "Buy more ram lol");

	new->id = win;
	new->is_float = 0;
	new->prev = NULL;
	new->next = wm->workspaces->clients;
	if (wm->workspaces->clients != NULL)
		wm->workspaces->clients->prev = new;
	wm->workspaces->clients = new;
	wm->workspaces->current = new;
	wm->workspaces->n_cli++;

	if (wm->workspaces->n_cli == 1)
		new_workspace(wm);

	new->name = NULL;
	if (XGetWMName(wm->dpy, win, &prop))
		new->name = (unsigned char*) prop.value;

	XGrabButton(wm->dpy,
		AnyButton,
		AnyModifier,
		win,
		False,
		ButtonPressMask,
		GrabModeAsync,
		GrabModeSync,
		None,
		None);

	XSelectInput(wm->dpy,
		win,
		PointerMotionMask
		| PropertyChangeMask);

	XSetWindowBorder(wm->dpy, win, BORDER_COLOR);
	XSetWindowBorderWidth(wm->dpy, win, BORDER_WIDTH);
	XMapWindow(wm->dpy, win);

	update_view(wm);
}

void map_requested(Wm *wm, XEvent *ev)
{
	XWindowAttributes wa;
	XMapRequestEvent *e = &ev->xmaprequest;

	if (!XGetWindowAttributes(wm->dpy, e->window, &wa)
		|| wa.override_redirect
		|| managed(wm, e->window))

		return;

	manage(wm, e->window);
}

void unmanage_hidden(Wm *wm, Client *c)
{
	if (c->prev != NULL)
		c->prev->next = c->next;
	if (c->next != NULL)
		c->next->prev = c->prev;
	if (wm->hidden == c)
		wm->hidden = c->next;
	wm->n_hidden--;
}

void unmanage_from_workspace(Wm *wm, Client *c, Workspace *w)
{
	Client *cit;
	if (c->prev != NULL)
		c->prev->next = c->next;
	if (c->next != NULL)
		c->next->prev = c->prev;
	if (w->clients == c)
		w->clients = c->next;
	if (w->floats == c)
		w->floats = c->next;

	if (w->current == c) {
		if (c->prev != NULL)
			w->current = c->prev;
		else if (c->next != NULL)
			w->current = c->next;
		else if (w->floats != NULL)
			w->current = w->floats;
		else if (w->clients != NULL)
			w->current = w->clients;
		else
			w->current = NULL;
	}

	if (c->is_float)
		w->n_float--;
	else
		w->n_cli--;

	if (w->n_cli == 0 && w->n_float == 0) {
		if (w->prev != NULL)
			w->prev->next = w->next;
		w->next->prev = w->prev;
		if (wm->workspaces == w)
			wm->workspaces = w->next;
		free(w);
		wm->n_works--;

		for (cit = wm->workspaces->clients; cit != NULL; cit = cit->next)
			XMapWindow(wm->dpy, cit->id);
		for (cit = wm->workspaces->floats; cit != NULL; cit = cit->next)
			XMapRaised(wm->dpy, cit->id);
	}

	update_view(wm);
}

void destroy_notify(Wm *wm, XEvent *ev)
{
	XDestroyWindowEvent *e = &ev->xdestroywindow;
	FindResult r = find_window(wm, e->window);
	if (r.c == NULL)
		return;
	if (r.w == NULL)
		unmanage_hidden(wm, r.c);
	else
		unmanage_from_workspace(wm, r.c, r.w);
	if (r.c->name != NULL)
		XFree(r.c->name);
	free(r.c);
}

void property_notify(Wm *wm, XEvent *ev)
{
	XPropertyEvent *e = &ev->xproperty;
	Client *c = NULL;
	XTextProperty prop;

	if (e->window == wm->root) {
		update_status(wm);
	} else {
		c = find_window(wm, e->window).c;
		if (c != NULL) {
			if (c->name != NULL)
				XFree(c->name);
			c->name = NULL;
			if (XGetWMName(wm->dpy, c->id, &prop))
				c->name = (unsigned char*) prop.value;
		}
	}

	render_bar(wm);
}

void expose(Wm *wm, XEvent *ev)
{
	XExposeEvent *e = &ev->xexpose;

	if (e->window == wm->bar)
		render_bar(wm);
}

void switch_workspace(Wm *wm, short int to_next)
{
	Client *c;
	Workspace *new = to_next ? wm->workspaces->next : wm->workspaces->prev;
	if (new != NULL) {
		for (c = wm->workspaces->clients; c != NULL; c = c->next)
			XUnmapWindow(wm->dpy, c->id);
		for (c = wm->workspaces->floats; c != NULL; c = c->next)
			XUnmapWindow(wm->dpy, c->id);
		wm->workspaces = new;
		for (c = wm->workspaces->clients; c != NULL; c = c->next)
			XMapWindow(wm->dpy, c->id);
		for (c = wm->workspaces->floats; c != NULL; c = c->next)
			XMapRaised(wm->dpy, c->id);
		update_view(wm);
	}
}

void hide(Wm *wm)
{
	Client *c = wm->workspaces->current;
	Workspace *w = wm->workspaces;

	if (c != NULL) {
		XUnmapWindow(wm->dpy, c->id);
		unmanage_from_workspace(wm, c, w);
		c->prev = NULL;
		c->next = wm->hidden;
		if (wm->hidden != NULL)
			wm->hidden->prev = c;
		wm->hidden = c;
		wm->n_hidden++;
		c->is_float = 0;
	}
}

void zoom(Wm *wm)
{
	Workspace *w = wm->workspaces;
	Client *c = wm->workspaces->current;
	if (c != NULL && c != wm->workspaces->clients) {
		if (c->prev != NULL)
			c->prev->next = c->next;
		if (c->next != NULL)
			c->next->prev = c->prev;

		if (c->is_float) {
			w->n_float--;
			w->n_cli++;
		}

		c->prev = NULL;
		c->next = wm->workspaces->clients;
		if (wm->workspaces->clients != NULL)
			wm->workspaces->clients->prev = c;
		wm->workspaces->clients = c;

		update_view(wm);
	}
}

void floatwin(Wm *wm)
{
	Client *c = wm->workspaces->current;
	Workspace *w = wm->workspaces;

	if (c == NULL)
		return;

	if (c->is_float && w->floats == c)
		w->floats = c->next;
	else if (!c->is_float && w->clients == c)
		w->clients = c->next;

	if (c->next != NULL)
		c->next->prev = c->prev;
	if (c->prev != NULL)
		c->prev->next = c->next;

	c->prev = NULL;
	if (c->is_float) {
		c->next = w->clients;
		w->clients = c;
		w->n_float--;
		w->n_cli++;
	} else {
		c->next = w->floats;
		w->floats = c;
		w->n_float++;
		w->n_cli--;
	}

	c->is_float = !c->is_float;

	if (c->next != NULL)
		c->next->prev = c;

	update_view(wm);
}

void closewin(Wm *wm)
{
	Client *c = wm->workspaces->current;
	if (c != NULL)
		XKillClient(wm->dpy, c->id);
}

void unhide_by_idx(Wm *wm, int idx)
{
	Client *c;

	if (idx < 0)
		return;

	for (c = wm->hidden; c != NULL && idx > 1; c = c->next)
		idx--;

	if (c != NULL) {
		wm->n_hidden--;
		wm->workspaces->n_cli++;
		if (c->prev != NULL)
			c->prev->next = c->next;
		if (c->next != NULL)
			c->next->prev = c->prev;
		if (wm->hidden == c)
			wm->hidden = c->next;
		c->prev = NULL;
		c->next = wm->workspaces->clients;
		if (wm->workspaces->clients != NULL)
			wm->workspaces->clients->prev = c;
		wm->workspaces->clients = c;
		wm->workspaces->current = c;

		XMapWindow(wm->dpy, c->id);

		if (wm->workspaces->n_cli == 1)
			new_workspace(wm);

		update_view(wm);
	}
}

void hidden_window(Wm *wm)
{
	XGlyphInfo extents;
	XEvent ev;
	Client *cli;
	int lasty;
	int size;

	if (wm->n_hidden == 0)
		return;

	XMapWindow(wm->dpy, wm->cli_win);
	XMoveResizeWindow(wm->dpy, wm->cli_win, 0, 0, wm->sw / 5, wm->bar_height * (wm->n_hidden + 1));

	XClearWindow(wm->dpy, wm->cli_win);
	XftDraw	*xftdraw = XftDrawCreate(wm->dpy, wm->cli_win, DefaultVisual(wm->dpy, 0), DefaultColormap(wm->dpy, 0));

	if ((cli = wm->workspaces->current) != NULL) {
		size = strlen((char*) cli->name);
		size = size < MAX_WNAME_CHAR ? size : MAX_WNAME_CHAR;
		XftTextExtentsUtf8(wm->dpy, wm->xftfont, (unsigned char*) cli->name, size, &extents);
		XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, extents.x, extents.y + BAR_PADDING, cli->name, size);
	} else {
		XftTextExtentsUtf8(wm->dpy, wm->xftfont, (unsigned char*) "<no client>", 11, &extents);
		XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, extents.x, extents.y + BAR_PADDING, (unsigned char*) "<no client>", 11);
	}

	lasty = extents.y + BAR_PADDING;

	for (cli = wm->hidden; cli != NULL; cli = cli->next) {
		lasty += wm->bar_height;
		size = strlen((char*) cli->name);
		XftTextExtentsUtf8(wm->dpy, wm->xftfont, (unsigned char*) cli->name, size, &extents);
		XftDrawStringUtf8(xftdraw, &wm->xftcolor, wm->xftfont, extents.x, lasty, (unsigned char*) cli->name, size);
	}

	XftDrawDestroy(xftdraw);

	XGrabPointer(
		wm->dpy,
		wm->cli_win,
		True,
		PointerMotionMask | ButtonPressMask,
		GrabModeAsync,
		GrabModeAsync,
		None,
		wm->cursors.left_ptr,
		CurrentTime);

	for (;;) {
		XSync(wm->dpy, False);
		XNextEvent(wm->dpy, &ev);

		switch (ev.type) {
		case MotionNotify:
			if (ev.xmotion.x_root > wm->sw / 5 || ev.xmotion.y_root > wm->bar_height * (wm->n_hidden + 1))
				goto ungrab;
			break;
		case ButtonPress:
			unhide_by_idx(wm, ev.xmotion.y_root / wm->bar_height);
			goto ungrab;
			break;
		default:
			handle_event(wm, &ev);
			break;
		}
	}

ungrab:
	XUngrabPointer(wm->dpy, CurrentTime);
	XUnmapWindow(wm->dpy, wm->cli_win);
}

void button_press(Wm *wm, XEvent *ev)
{
	XButtonEvent *e = &ev->xbutton;
	FindResult r;
	int i;

	if (e->window == wm->bar) {
		if (e->x < wm->sw / 5) {
			hidden_window(wm);
		} else if (e->x >= wm->wx && e->x <= wm->wx + wm->ww) {
			if (e->button == Button3)
				switch_workspace(wm, 1);
			else
				switch_workspace(wm, 0);
		} else if (e->x >= wm->hidex && e->x <= wm->hidex + wm->hidew) {
			hide(wm);
		} else if (e->x >= wm->zoomx && e->x <= wm->zoomx + wm->zoomw) {
			zoom(wm);
		} else if (e->x >= wm->closex && e->x <= wm->closex + wm->closew) {
			closewin(wm);
		} else if (e->x >= wm->floatx && e->x <= wm->floatx + wm->floatw) {
			floatwin(wm);
		} else if (e->x >= wm->movex && e->x <= wm->movex + wm->movew) {
			movewin(wm);
		} else {
			for (i = 0; i < LENGTH(items); i++) {
				if (e->x >= items[i].x && e->x <= items[i].x + items[i].w) {
					if (fork() == 0) {
						execvp(items[i].command[0], (char * const *) items[i].command);
						exit(1);
					}
					break;
				}
			}
		}
	} else {
		r = find_window(wm, e->window);
		if (r.c != NULL && r.w != NULL) {
			r.w->current = r.c;
			update_view(wm);
		}
	}
}

void fullscreen(Wm *wm)
{
	XEvent ev;

	if (wm->workspaces->current == NULL)
		return;

	XSetWindowBorderWidth(wm->dpy, wm->workspaces->current->id, 0);
	XMoveResizeWindow(wm->dpy, wm->workspaces->current->id, 0, 0, wm->sw, wm->sh);
	XRaiseWindow(wm->dpy, wm->workspaces->current->id);

	for (;;) {
		XSync(wm->dpy, False);
		XNextEvent(wm->dpy, &ev);

		switch (ev.type) {
		case MapRequest:
		case DestroyNotify:
		case ButtonPress:
			handle_event(wm, &ev);
		case KeyPress:
			XSetWindowBorderWidth(wm->dpy, wm->workspaces->current->id, BORDER_WIDTH);
			update_view(wm);
			return;
		}
	}
}

void redraw(Wm *wm)
{
	Window _dumb;
	int x, y;
	unsigned int w, h, _dumbi;

	if (wm->workspaces->current != NULL) {
		XSync(wm->dpy, False);
		XGetGeometry(wm->dpy, wm->workspaces->current->id, &_dumb, &x, &y, &w, &h, &_dumbi, &_dumbi);
		XSync(wm->dpy, False);
		XMoveResizeWindow(wm->dpy, wm->workspaces->current->id, x, y, w-1, h);
		XSync(wm->dpy, False);
		XMoveResizeWindow(wm->dpy, wm->workspaces->current->id, x, y, w, h);
		XSync(wm->dpy, False);
	}
}

void key_press(Wm *wm, XEvent *e)
{
	XKeyEvent *ev = &e->xkey;

	if (ev->state == MODMASK) {
		if (wm->fkey == ev->keycode)
			fullscreen(wm);
		else if (wm->rkey == ev->keycode)
			movewin(wm);
		else if (wm->dkey == ev->keycode)
			floatwin(wm);
		else if (wm->akey == ev->keycode)
			redraw(wm);
	}
}

void config_request(Wm *wm, XEvent *ev)
{
	Window _dumb;
	int x, y, nx, ny;
	unsigned int w, h, nw, nh, _dumbi;
	XConfigureRequestEvent *e = &ev->xconfigurerequest;

	FindResult r = find_window(wm, e->window);

	if (r.c != NULL && r.w != NULL && r.is_float) {
		XGetGeometry(wm->dpy, e->window, &_dumb, &x, &y, &w, &h, &_dumbi, &_dumbi);
		x = e->value_mask & CWX ? e->x : x;
		y = e->value_mask & CWY ? e->y : y;
		w = e->value_mask & CWWidth ? e->width : w;
		h = e->value_mask & CWHeight ? e->height : h;

		XMoveResizeWindow(wm->dpy, e->window, x, y, w, h);
		XSync(wm->dpy, False);
	}
}

void handle_event(Wm *wm, XEvent *ev)
{
	switch (ev->type) {
	case MapRequest:
		map_requested(wm, ev);
		break;
	case DestroyNotify:
		destroy_notify(wm, ev);
		break;
	case PropertyNotify:
		property_notify(wm, ev);
		break;
	case ConfigureRequest:
		config_request(wm, ev);
	case Expose:
		expose(wm, ev);
		break;
	case ButtonPress:
		button_press(wm, ev);
		break;
	case KeyPress:
		key_press(wm, ev);
		break;
	}
}

void main_loop(Wm *wm)
{
	XEvent ev;

	for (;;) {
		XSync(wm->dpy, False);
		XNextEvent(wm->dpy, &ev);
		handle_event(wm, &ev);
	}
}

void scan(Wm *wm)
{
	unsigned int j, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(wm->dpy, wm->root, &d1, &d2, &wins, &num)) {
		for (j = 0; j < num; j++) {
			if (!XGetWindowAttributes(wm->dpy, wins[j], &wa))
				continue;
			if (wa.override_redirect || wa.map_state != IsViewable)
				continue;
			if (wins[j] == wm->bar || wins[j] == wm->cli_win)
				continue;
			if (!managed(wm, wins[j]))
				manage(wm, wins[j]);
		}
		if (wins)
			XFree(wins);
	}
}

int main(void)
{
	Wm wm;
	XGlyphInfo extents;

	if (!(wm.dpy = XOpenDisplay(NULL)))
		return 1;

	wm.xftfont = NULL;

	wm.screen = DefaultScreen(wm.dpy);
	wm.sw = DisplayWidth(wm.dpy, wm.screen);
	wm.sh = DisplayHeight(wm.dpy, wm.screen);
	wm.root = RootWindow(wm.dpy, wm.screen);
	wm.xftfont = XftFontOpenName(wm.dpy, 0, font);
	if (!wm.xftfont)
		return 1;
	wm.xrcolor.red = (BAR_FOREGROUND & 0xff0000) >> 16;
	wm.xrcolor.green = (BAR_FOREGROUND & 0x00ff00) >> 8;
	wm.xrcolor.blue = BAR_FOREGROUND & 0x0000ff;
	wm.xrcolor.alpha = 0xffff;
	XftColorAllocValue(wm.dpy, DefaultVisual(wm.dpy, 0), DefaultColormap(wm.dpy, 0), &wm.xrcolor, &wm.xftcolor);

	/* Register to get the events. */
	long mask = SubstructureRedirectMask
		| SubstructureNotifyMask
		| ButtonPressMask
		| ButtonReleaseMask
		| PointerMotionMask
		| StructureNotifyMask
		| PropertyChangeMask;

	XSelectInput(wm.dpy, wm.root, mask);

	XftTextExtentsUtf8(wm.dpy, wm.xftfont, (unsigned char*) "abcdefghijklmnopqrstuvwxyzABCDEFJHIJKLMNOPQRSTUVWXYZ", 52, &extents);

	wm.bar_height = extents.height + BAR_PADDING * 2;
	wm.bar_y = extents.y + BAR_PADDING;
	wm.bar = XCreateSimpleWindow(
		wm.dpy,
		wm.root,
		0,
		0,
		wm.sw,
		wm.bar_height,
		0,
		0,
		BAR_BACKGROUND);
	wm.cli_win = XCreateSimpleWindow(
		wm.dpy,
		wm.root,
		0,
		0,
		10,
		10,
		0,
		0,
		BAR_BACKGROUND);
	wm.status = NULL;
	XStoreName(wm.dpy, wm.root, "tibaji");
	XSelectInput(wm.dpy, wm.bar, ExposureMask | ButtonPressMask);
	XSelectInput(wm.dpy, wm.cli_win, ExposureMask | ButtonPressMask);
	XMapRaised(wm.dpy, wm.bar);

	wm.cursors.left_ptr = XCreateFontCursor(wm.dpy, 68);
	wm.cursors.sizing = XCreateFontCursor(wm.dpy, 120);

#ifdef BACKGROUND
	XSetWindowBackground(wm.dpy, wm.root, BACKGROUND);
	XClearWindow(wm.dpy, wm.root);
#endif

	XDefineCursor(wm.dpy, wm.root, wm.cursors.left_ptr);

	/* Grab keys. */
	wm.fkey = XKeysymToKeycode(wm.dpy, FULLSCREEN_KEY);
	XGrabKey(wm.dpy,
		wm.fkey,
		MODMASK,
		wm.root,
		True,
		GrabModeAsync,
		GrabModeAsync);
	wm.rkey = XKeysymToKeycode(wm.dpy, RESIZE_KEY);
	XGrabKey(wm.dpy,
		wm.rkey,
		MODMASK,
		wm.root,
		True,
		GrabModeAsync,
		GrabModeAsync);
	wm.akey = XKeysymToKeycode(wm.dpy, REDRAW_KEY);
	XGrabKey(wm.dpy,
		wm.akey,
		MODMASK,
		wm.root,
		True,
		GrabModeAsync,
		GrabModeAsync);
	wm.dkey = XKeysymToKeycode(wm.dpy, DETACH_KEY);
	XGrabKey(wm.dpy,
		wm.dkey,
		MODMASK,
		wm.root,
		True,
		GrabModeAsync,
		GrabModeAsync);

	XSetErrorHandler(error_handler);
	signal(SIGCHLD, child_handler);

	wm.n_works = 1;
	wm.n_cur = 0;
	wm.n_hidden = 0;
	wm.hidden = NULL;
	wm.workspaces = malloc(sizeof(Workspace));
	assert(wm.workspaces != NULL && "Buy more ram lol");
	wm.workspaces->n_cli = 0;
	wm.workspaces->n_float = 0;
	wm.workspaces->clients = NULL;
	wm.workspaces->floats = NULL;
	wm.workspaces->next = NULL;
	wm.workspaces->prev = NULL;
	wm.workspaces->current = NULL;

	wm.wx = wm.sw / 5;

	scan(&wm);
	update_status(&wm);
	render_bar(&wm);
	main_loop(&wm);

	return 0;
}
