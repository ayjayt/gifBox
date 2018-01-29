#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#include <X11/keysym.h>
#include <X11/Xresource.h>
#include <X11/cursorfont.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <dirent.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

Display *disp;
Window root;

Window scrot_get_window(Display * display, Window window, int x, int y)
{
  Window source, target;

  int status, x_offset, y_offset;

  source = root;
  target = window;
  if (window == None)
    window = root;
  while (1) {
    status =
      XTranslateCoordinates(display, source, window, x, y, &x_offset,
                            &y_offset, &target);
    if (status != True)
      break;
    if (target == None)
      break;
    source = window;
    window = target;
    x = x_offset;
    y = y_offset;
  }
  if (target == None)
    target = window;
  return (target);
}

int main() {
	disp = XOpenDisplay(NULL);
	root = 0;
	Screen *scr = NULL;
	scr = ScreenOfDisplay(disp, DefaultScreen(disp));
  root = RootWindow(disp, XScreenNumberOfScreen(scr));	
  // Initializing variables
	static int xfd = 0;
  static int fdsize = 0;
  XEvent ev;
  fd_set fdset;
  int count = 0, done = 0;
  int rx = 0, ry = 0, rw = 0, rh = 0, btn_pressed = 0;
  int rect_x = 0, rect_y = 0, rect_w = 0, rect_h = 0;
  Cursor cursor, cursor_nw, cursor_ne, cursor_se, cursor_sw;
  Window target = None;
  GC gc;
  XGCValues gcval;

	// Hopefully we can delete this
  xfd = ConnectionNumber(disp);
  fdsize = xfd + 1;

  cursor    = XCreateFontCursor(disp, XC_crosshair);
  cursor_nw = XCreateFontCursor(disp, XC_ul_angle);
  cursor_ne = XCreateFontCursor(disp, XC_ur_angle);
  cursor_se = XCreateFontCursor(disp, XC_lr_angle);
  cursor_sw = XCreateFontCursor(disp, XC_ll_angle);

  gcval.foreground = XWhitePixel(disp, 0);
  gcval.function = GXxor;
  gcval.background = XBlackPixel(disp, 0);
  gcval.plane_mask = gcval.background ^ gcval.foreground;
  gcval.subwindow_mode = IncludeInferiors;

  gc =
    XCreateGC(disp, root,
              GCFunction | GCForeground | GCBackground | GCSubwindowMode,
              &gcval);

  if ((XGrabPointer
       (disp, root, False,
        ButtonMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync,
        GrabModeAsync, root, cursor, CurrentTime) != GrabSuccess))
    //gib_eprintf("couldn't grab pointer:");
		printf("couldn't grab pointer:");

  if ((XGrabKeyboard
       (disp, root, False, GrabModeAsync, GrabModeAsync,
        CurrentTime) != GrabSuccess))
    //gib_eprintf("couldn't grab keyboard:");
		printf("couldn't grab keyboard:");

  while (1) {
    /* handle events here */
    while (!done && XPending(disp)) {
      XNextEvent(disp, &ev);
      switch (ev.type) {
        case MotionNotify:
          if (btn_pressed) {
            if (rect_w) {
              /* re-draw the last rect to clear it */
              XDrawRectangle(disp, root, gc, rect_x, rect_y, rect_w, rect_h);
            }

            rect_x = rx;
            rect_y = ry;
            rect_w = ev.xmotion.x - rect_x;
            rect_h = ev.xmotion.y - rect_y;

            /* Change the cursor to show we're selecting a region */
            if (rect_w < 0 && rect_h < 0)
              XChangeActivePointerGrab(disp,
                                       ButtonMotionMask | ButtonReleaseMask,
                                       cursor_nw, CurrentTime);
            else if (rect_w < 0 && rect_h > 0)
              XChangeActivePointerGrab(disp,
                                       ButtonMotionMask | ButtonReleaseMask,
                                       cursor_sw, CurrentTime);
            else if (rect_w > 0 && rect_h < 0)
              XChangeActivePointerGrab(disp,
                                       ButtonMotionMask | ButtonReleaseMask,
                                       cursor_ne, CurrentTime);
            else if (rect_w > 0 && rect_h > 0)
              XChangeActivePointerGrab(disp,
                                       ButtonMotionMask | ButtonReleaseMask,
                                       cursor_se, CurrentTime);

            if (rect_w < 0) {
              rect_x += rect_w;
              rect_w = 0 - rect_w;
            }
            if (rect_h < 0) {
              rect_y += rect_h;
              rect_h = 0 - rect_h;
            }
            /* draw rectangle */
            XDrawRectangle(disp, root, gc, rect_x, rect_y, rect_w, rect_h);
            XFlush(disp);
          }
          break;
        case ButtonPress:
          btn_pressed = 1;
          rx = ev.xbutton.x;
          ry = ev.xbutton.y;
          target =
            scrot_get_window(disp, ev.xbutton.subwindow, ev.xbutton.x,
                             ev.xbutton.y);
          if (target == None)
            target = root;
          break;
        case ButtonRelease:
          done = 1;
          break;
        case KeyPress:
          fprintf(stderr, "Key was pressed, aborting shot\n");
          done = 2;
          break;
        case KeyRelease:
          /* ignore */
          break;
        default:
          break;
      }
    }
    if (done)
      break;

    /* now block some */
    FD_ZERO(&fdset);
    FD_SET(xfd, &fdset);
    errno = 0;
    count = select(fdsize, &fdset, NULL, NULL, NULL);
    if ((count < 0)
        && ((errno == ENOMEM) || (errno == EINVAL) || (errno == EBADF)))
      //gib_eprintf("Connection to X display lost");
			printf("Connection to X display lost");
  }
  if (rect_w) {
    XDrawRectangle(disp, root, gc, rect_x, rect_y, rect_w, rect_h);
    XFlush(disp);
  }
  XUngrabPointer(disp, CurrentTime);
  XUngrabKeyboard(disp, CurrentTime);
  XFreeCursor(disp, cursor);
  XFreeGC(disp, gc);
  XSync(disp, True);


  if (done < 2) {
    //scrot_do_delay();
		int gifBox_i = 200;
		while (--gifBox_i){
			asm("nop");
		}

    Window client_window = None;

    if (rect_w > 5) {
      /* if a rect has been drawn, it's an area selection */
      rw = ev.xbutton.x - rx;
      rh = ev.xbutton.y - ry;

      if (rw < 0) {
        rx += rw;
        rw = 0 - rw;
      }
      if (rh < 0) {
        ry += rh;
        rh = 0 - rh;
      }
			printf("%d\n%d\n%d\n%d\n", rx, ry, rw, rh);
    } else {
      /* else it's a window click */
      //if (!scrot_get_geometry(target, &client_window, &rx, &ry, &rw, &rh))
        return 1;
    }
    //scrot_nice_clip(&rx, &ry, &rw, &rh);

    XBell(disp, 0);
  }
	return 0;

}
