#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/times.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#define NAME_SIZE 40
#define BUF_SIZE 1024
#define BUF_SIZE_LONG 2048
#define MAX_CLIENTS 10
#define PORT_NO (u_short)20000
#define GRID_X 5
#define GRID_Y 4
#define ErrorExit(x)          \
    {                         \
        fprintf(stderr, "-"); \
        perror(x);            \
        exit(0);              \
    }

struct Stroke
{
    XPoint p0;
    XPoint p1;
    unsigned long color;
    char *message;
    bool en;
    struct Stroke *next;
};

int i, j, msgLength, inputLength, result;
char inputBuf[BUF_SIZE];
struct timeval timeVal;
fd_set mask;

Display *display;
Window window;
XSetWindowAttributes attributes;
GC gc;
XEvent event;
XPoint startPoint;

void createWindow(int, int, char *);
void onEvent();
void parseCommand(char *);
unsigned long parseColor(char *);
void queryColor(unsigned long, int *, int *, int *);
void startPainter();

int main(int argc, char *argv[])
{

    timeVal.tv_sec = 0;
    timeVal.tv_usec = 1;
    startPainter();
}

void createWindow(int x, int y, char *title)
{
    display = XOpenDisplay(NULL);

    // https://stackoverflow.com/a/15089506/1355992
    XSizeHints hints = {0};
    hints.flags = PPosition | PSize;
    hints.x = x;
    hints.y = y;
    hints.width = 400;
    hints.height = 300;

    window = XCreateSimpleWindow(display, RootWindow(display, 0), x, y,
                                 400, 300, 5, BlackPixel(display, 0),
                                 WhitePixel(display, 0));
    attributes.backing_store = WhenMapped;
    XChangeWindowAttributes(display, window, CWBackingStore, &attributes);
    XSelectInput(display, window,
                 ExposureMask | ButtonPressMask | ButtonMotionMask);
    XStoreName(display, window, title);
    XSetNormalHints(display, window, &hints);
    XMapWindow(display, window);
    gc = XCreateGC(display, DefaultRootWindow(display), 0, 0);
    XSetForeground(display, gc, parseColor("000000"));
    XSetLineAttributes(display, gc, 3, LineSolid, CapRound, JoinRound);
}
void onEvent()
{
    if (XEventsQueued(display, QueuedAfterFlush) == 0)
        return;
    XNextEvent(display, &event);
    switch (event.type)
    {
    case Expose:
        // Redraw the window content here!
        break;
    case ButtonPress:
        startPoint.x = event.xbutton.x;
        startPoint.y = event.xbutton.y;
        break;
    case MotionNotify:
        XDrawLine(display, window, gc, startPoint.x, startPoint.y,
                  event.xmotion.x, event.xmotion.y);
        startPoint.x = event.xmotion.x;
        startPoint.y = event.xmotion.y;
        break;
    }
}

void parseCommand(char *input)
{
    // C-RRGGBB or C-RGB: Set pen color
    char cmd, param[sizeof(char) * BUF_SIZE];
    char *token, separator[2] = "\n";
    token = strtok(input, separator);
    while (token != NULL)
    {
        bzero(param, sizeof(char) * BUF_SIZE);
        sscanf(token, "%c-%s", &cmd, param);
        switch (cmd)
        {
        case 'C':
        case 'c':
        {
            unsigned long colorVal = parseColor(param);
            XSetForeground(display, gc, colorVal);
        }
        case 'D':
        case 'd':
        {
            int x0, y0, x1, y1;
            char color[BUF_SIZE];
            bzero(color, BUF_SIZE);
            if (sscanf(param, "%d-%d-%d-%d-%s", &x0, &y0, &x1, &y1, color) >= 4)
            {
                unsigned long colorVal = parseColor(color);
                XSetForeground(display, gc, colorVal);
                XDrawLine(display, window, gc, x0, y0, x1, y1);
            }
        }
        break;
        default:
            printf("%s: Invalid command!\n", input);
            break;
        }
        token = strtok(NULL, separator);
    }
    // recover the last line break trimmed by strtok()
    if (strlen(input) > 0 && input[strlen(input) - 1] != '\n')
        strcat(input, "\n");
}

unsigned long parseColor(char *colorHex)
{
    int sr, sg, sb, r = 0, g = 0, b = 0;
    unsigned long colorVal;
    char colorBuf[16];
    if (sscanf(colorHex, "%lx", &colorVal) == 1)
    {
        if (strlen(colorHex) == 3 &&
            sscanf(colorHex, "%1x%1x%1x", &sr, &sg, &sb) == 3)
        {
            r = sr + sr * 16;
            g = sg + sg * 16;
            b = sb + sb * 16;
        }
        else if (strlen(colorHex) >= 6 &&
                 sscanf(colorHex, "%02x%02x%02x", &sr, &sg, &sb) == 3)
        {
            r = sr;
            g = sg;
            b = sb;
        }
    }
    sprintf(colorBuf, "rgb:%02x/%02x/%02x", r, g, b);
    XColor scrColor, exactColor;
    XAllocNamedColor(display, DefaultColormap(display, DefaultScreen(display)),
                     colorBuf, &scrColor, &exactColor);
    printf("\033[48;2;%d;%d;%dm          ", r, g, b);
    printf("\033[0m\n");
    return scrColor.pixel;
}

void queryColor(unsigned long color, int *r, int *g, int *b)
{
    XColor xColor;
    xColor.pixel = color;
    XQueryColor(display, DefaultColormap(display, DefaultScreen(display)), &xColor);
    *r = xColor.red / 256;
    *g = xColor.green / 256;
    *b = xColor.blue / 256;
}

void startPainter()
{
    bool hasNext;
    createWindow(200, 200, "Painter");

    while (1)
    {
        onEvent();

        FD_ZERO(&mask);
        // Set the fd to be watched (fd=0 means standard input)
        FD_SET(0, &mask);
        hasNext = true;
        result = select(0 + 1, &mask, NULL, NULL, &timeVal);
        if (result < 0)
            ErrorExit("select");

        // Message input via stdio (fd=0 means standard input)
        // Q: Quit from the running program
        if (FD_ISSET(0, &mask))
        {
            bzero(inputBuf, sizeof(char) * BUF_SIZE);
            int inputLength = read(0, inputBuf, sizeof(char) * BUF_SIZE);
            // "Quit" command input
            if (strcmp(inputBuf, "Q\n") == 0 || inputLength == 0)
                hasNext = false;
            else
                parseCommand(inputBuf);
        }

        if (!hasNext)
            break;
    }

    XDestroyWindow(display, window);
    XCloseDisplay(display);
}
