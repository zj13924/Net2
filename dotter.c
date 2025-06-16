/*
  Class 4I, 2024, NIT Ibaraki College
  Information Networks II sample code
  Skeleton program of the "Gomoku" game
*/
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/times.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#define NONE 0b00
#define BLACK 0b01
#define WHITE 0b10
#define GRID_SIZE 30
#define STONE_RADIUS 13
#define BUF_SIZE 1024
#define PROTOCOL_DELIMITER "-\n"
#define ErrorExit(x)          \
    {                         \
        fprintf(stderr, "-"); \
        perror(x);            \
        exit(0);              \
    }

const char *PLACE = "PLACE";
const char *XYZ = "XYZ"; // STUB!

unsigned char board[16][16] = {NONE};
unsigned char currentColor = BLACK;
char buf[BUF_SIZE];
struct timeval timeVal;
fd_set fdSet;

Display *display;
Window window;
Colormap colormap;
GC gc;
XEvent event;

void createWindow(int, int, char *);
void onEvent();
bool placeStone(int, int);
void drawStone(int, int);
void drawBoard();
bool parseCommand(char *);
bool checkResult(int, int);

int main(int argc, char *argv[])
{
    int result;
    // Set the blocking time for select()
    timeVal.tv_sec = 0;
    timeVal.tv_usec = 10;
    createWindow(400, 400, "Gomoku");

    while (1)
    {
        // Respond to window events
        onEvent();

        // Initialize the fd_set and attach the target FDs to be supervised
        FD_ZERO(&fdSet);
        FD_SET(0, &fdSet);
        result = select(0 + 1, &fdSet, NULL, NULL, &timeVal);
        if (result < 0)
            ErrorExit("select");

        // When stdin is ready for read (fd = 0 means standard input)
        if (FD_ISSET(0, &fdSet))
        {
            bzero(buf, sizeof(char) * BUF_SIZE);
            // "Quit" command input
            if (read(0, buf, BUF_SIZE) == 0 || strcmp(buf, "Q\n") == 0)
                break;
            else
                parseCommand(buf);
        }
    }

    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}

void createWindow(int left, int top, char *title)
{
    XSetWindowAttributes attributes;
    XSizeHints hints = {0};
    attributes.backing_store = WhenMapped;
    hints.flags = PPosition | PSize;
    hints.x = left;
    hints.y = top;
    hints.width = GRID_SIZE * 19;
    hints.height = GRID_SIZE * 19;

    display = XOpenDisplay(NULL);
    window = XCreateSimpleWindow(display, RootWindow(display, DefaultScreen(display)),
                                 left, top, hints.width, hints.height, 1,
                                 BlackPixel(display, DefaultScreen(display)),
                                 WhitePixel(display, DefaultScreen(display)));
    colormap = DefaultColormap(display, DefaultScreen(display));
    XChangeWindowAttributes(display, window, CWBackingStore, &attributes);
    XSelectInput(display, window, ExposureMask | ButtonPressMask | ButtonReleaseMask);
    XStoreName(display, window, title);
    XSetNormalHints(display, window, &hints);
    XMapWindow(display, window);

    gc = XCreateGC(display, DefaultRootWindow(display), 0, 0);
    XSetLineAttributes(display, gc, 1, LineSolid, CapRound, JoinRound);
}

void onEvent()
{
    int gridX, gridY;
    // If the event queue is empty
    if (XEventsQueued(display, QueuedAfterFlush) == 0)
        return;
    // Retrieve an event from the event queue
    XNextEvent(display, &event);
    switch (event.type)
    {
    case Expose:
        XClearWindow(display, window);
        drawBoard();
        break;
    case ButtonRelease:
        gridX = (int)(event.xbutton.x - GRID_SIZE * 1.5) / GRID_SIZE;
        gridY = (int)(event.xbutton.y - GRID_SIZE * 1.5) / GRID_SIZE;
        placeStone(gridX, gridY);
        break;
    }
}

bool placeStone(int x, int y)
{
    if (x > -1 && x < 16 && y > -1 && y < 16 && board[y][x] == NONE)
    {
        board[y][x] = currentColor;
        drawStone(x, y);
        // Flip the current stone color
        currentColor = (currentColor == BLACK ? WHITE : BLACK);
        return true;
    }
    return false;
}

void drawStone(int x, int y)
{
    switch (board[y][x])
    {
    case BLACK:
        XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
        break;
    case WHITE:
        XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
        break;
    default:
        return;
    }
    XFillArc(display, window, gc,
             (int)(GRID_SIZE * (x + 2) - STONE_RADIUS), (int)(GRID_SIZE * (y + 2) - STONE_RADIUS),
             (unsigned int)(STONE_RADIUS * 2), (unsigned int)(STONE_RADIUS * 2), 0, 360 * 64);
}

void drawBoard()
{
    int i, x, y;
    XColor color, exactColor;
    // Draw background color
    XAllocNamedColor(display, colormap, "rgb:ff/99/00", &color, &exactColor);
    XSetForeground(display, gc, color.pixel);
    XFillRectangle(display, window, gc,
                   GRID_SIZE, GRID_SIZE, GRID_SIZE * 17, GRID_SIZE * 17);
    // Draw grid
    XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
    for (i = 0; i < 16; i++)
    {
        XDrawLine(display, window, gc,
                  GRID_SIZE * 2, (i + 2) * GRID_SIZE, GRID_SIZE * 17, (i + 2) * GRID_SIZE);
        XDrawLine(display, window, gc,
                  (i + 2) * GRID_SIZE, GRID_SIZE * 2, (i + 2) * GRID_SIZE, GRID_SIZE * 17);
    }
    // Draw stones
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            drawStone(x, y);
}

bool parseCommand(char *cmd)
{
    bool captured = false;
    char *cmdDup = strdup(cmd);
    char *cp = strtok(cmdDup, PROTOCOL_DELIMITER);
    while (cp != NULL)
    {
        char *cmdName = cp;
        cp = strtok(NULL, PROTOCOL_DELIMITER);
        // PLACE-<XY>: place a stone of current color at <X,Y>
        if (strcmp(cmdName, PLACE) == 0 && cp != NULL)
        {
            int position;
            if (sscanf(cp, "%02x", &position) > 0 && strlen(cp) == 2)
            {
                int x = position / 16, y = position % 16;
                if (!placeStone(x, y))
                {
                    // STUB!
                }
                else if (checkResult(x, y))
                {
                    // STUB!
                }
                captured = true;
                break;
            }
        }
        // Stub branch for a new command
        else if (strcmp(cmdName, XYZ) == 0)
        {
            // STUB!
            captured = true;
            break;
        }
    }
    free(cmdDup);
    return captured;
}

bool checkResult(int x, int y)
{
    // STUB!
    return false;
}