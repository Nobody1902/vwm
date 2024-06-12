#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

#define MAX_CLIENTS 10

// Structs & enums
typedef struct Client Client;
struct Client
{
	char name[256];
	int x, y, width, height;
	bool transient;
	Window win;
};

// EWMH atoms
enum { NetSupported, NetWMName, NetWMState, NetWMCheck, NetWMFullscreen, NetActiveWindow, NetWMWindowType, NetWMWindowTypeDialog, NetClientList, NetLast };
// Default atoms
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast};

// Variables
static Atom wmatom[WMLast], netatom[NetLast];

static Display* display;
static int screen;
static Window root;

static int screen_width, screen_height;

static Client* clients[MAX_CLIENTS] = {};
static int next_client = 0;

// Util functions

void panic(char* msg)
{
	puts(msg);
	exit(EXIT_FAILURE);
}

void* ecalloc(size_t nmemb, size_t size)
{
	void* p;
	if(!(p = calloc(nmemb, size)))
		panic("Calloc failed!");
	return p;
}
int get_text_prop(Window w, Atom atom, char* text, unsigned int size)
{
	char** list = NULL;
	int n;
	XTextProperty name;

	if(!text || size == 0)
		return 0;

	text[0] = '\0';
	if(!XGetTextProperty(display, w, &name, atom) || !name.nitems)
		return 0;

	if(name.encoding == XA_STRING){
		strncpy(text, (char*)name.value, size-1);
	}
	else if(XmbTextPropertyToTextList(display, &name, &list, &n) >= Success && n > 0 && *list)
	{
		strncpy(text, *list, size-1);
		XFreeStringList(list);
	}
	text[size-1] = '\0';
	XFree(name.value);
	return 1;
}
	

int get_client_w(Window w)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(clients[i]->win == w)
			return i;
	}
	return -1;
}

void remove_client(int index)
{
	for(int k = index+1; k < index; k++)
	{
		clients[k-1] = clients[k];
	}

	next_client--;
}

// Functions

void update_focus()
{
	if(clients[next_client-1]->win != None){
		XSetInputFocus(display, clients[next_client-1]->win, RevertToParent, CurrentTime);
	}
	else{
		XSetInputFocus(display, root, RevertToParent, CurrentTime);
	}
}

void map_window(Window win, XWindowAttributes* wa)
{
	Client* c = NULL;
	Window trans = None;
	
	c = ecalloc(1, sizeof(Client));
	c->win = win;
	c->transient = false;

	if(XGetTransientForHint(display, c->win, &trans)){
		c->transient = true;
	}

	// Get the window title into Client.name
	if(!get_text_prop(c->win, netatom[NetWMName], c->name, sizeof c->name))
	{
		get_text_prop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	}
	// Broken client
	if(c->name[0] == '\0')
		strcpy(c->name, "broken");

	if(c->transient)
	{
		// FIXME: To change after a taskbar
		c->x = wa->x;
		c->y = wa->y;
		c->width = wa->width;
		c->height = wa->height;
	}
	else
	{
		// FIXME: To change after a taskbar
		c->x = wa->x;
		c->y = wa->y;
		c->width = screen_width;
		c->height = screen_height;
	}

	XChangeProperty(display, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char*) &(c->win), 1);
	
	if(!c->transient)
		XMoveResizeWindow(display, c->win, c->x, c->y, c->width, c->height);
	
	XMapWindow(display, win);


	// Reorder stack
	clients[next_client] = c;
	next_client++;

	update_focus();
}

void map_request(XMapRequestEvent* e)
{
	XWindowAttributes wa;

	if(!XGetWindowAttributes(display, e->window, &wa) || wa.override_redirect)
		return;
	
	
	map_window(e->window, &wa);
}

void unmap(XUnmapEvent* e)
{
	int c = get_client_w(e->window);
	
	remove_client(c);

	update_focus();
}


void setup()
{
	display = XOpenDisplay(NULL);

	if(display == NULL)
	{
		panic("Couldn't open an X display.");
	}

	screen = DefaultScreen(display);
	screen_width = DisplayWidth(display, screen);
	screen_height = DisplayHeight(display, screen);
	root = RootWindow(display, screen);

	// Init atoms
	wmatom[WMProtocols] = XInternAtom(display, "WM_PROTOCOLS", false);
	wmatom[WMDelete] = XInternAtom(display, "WM_DELETE_WINDOW", false);
	wmatom[WMState] = XInternAtom(display, "WM_STATE", false);
	wmatom[WMTakeFocus] = XInternAtom(display, "WM_TAKE_FOCUS", false);
	netatom[NetActiveWindow] = XInternAtom(display, "_NET_ACTIVE_WINDOW", false);	
	netatom[NetSupported] = XInternAtom(display, "_NET_SUPPORTED", false);	
	netatom[NetWMName] = XInternAtom(display, "_NET_WM_NAME", false);	
	netatom[NetWMState] = XInternAtom(display, "_NET_WM_STATE", false);	
	netatom[NetWMCheck] = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", false);	
	netatom[NetWMFullscreen] = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", false);	
	netatom[NetWMWindowType] = XInternAtom(display, "_NET_WM_WINDOW_TYPE", false);	
	netatom[NetWMWindowTypeDialog] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", false);	
	netatom[NetClientList] = XInternAtom(display, "_NET_CLIENT_LIST", false);
	
	// Request events from the X server
	XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask);
	XSync(display, 0);

	// Create the cursor
	Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
	XDefineCursor(display, root, cursor);
	XSync(display, 0);
}

KeyCode key_grab(char* key, unsigned int mod)
{
	KeySym sym  = XStringToKeysym(key);
	KeyCode code = XKeysymToKeycode(display, sym);
	
	XGrabKey(display, code, mod, root, false, GrabModeAsync, GrabModeAsync);
	XSync(display, false);

	return code;
}

int main()
{
	setup();
	
	KeyCode exit_keycode = key_grab("q", Mod4Mask);
	KeyCode kill_keycode = key_grab("k", Mod4Mask);

	XEvent e;
	for(;;)
	{
		XNextEvent(display, &e);

		switch(e.type)
		{

			case MapRequest:
				map_request(&e.xmaprequest);
				break;
			case UnmapNotify:
				unmap(&e.xunmap);
				break;
			case KeyPress:
				int key_code = e.xkey.keycode;

				if(key_code == exit_keycode){
					puts("Exit key pressed: Exiting!");
					exit(0);
				}
				else if(key_code == kill_keycode){
					puts("Closed opened window!");
					XDestroyWindow(display, clients[next_client-1]->win);
				}
				break;
			default:
				break;
		}

		XSync(display, 0);
	}

	XCloseDisplay(display);
}


