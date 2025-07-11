/*
  Class 4I, 2025, NIT Ibaraki College
  Computer Networks II sample code
  Skeleton program of the "Tic-Tac-Toe" game
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
#include <X11/xpm.h>
#include "none.xpm"
#include "circle.xpm"
#include "cross.xpm"
#define NONE 0
#define CIRCLE 1
#define CROSS 2
#define BOARD_SIZE 3
#define GRID_SIZE 100
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
const char *NEW_COMMAND = "XYZ"; // STUB!

unsigned char board[BOARD_SIZE][BOARD_SIZE] = {NONE};
unsigned char currentColor = CIRCLE;
char buf[BUF_SIZE];
struct timeval timeVal;
fd_set fdSet;

Display *display;
Window window;
Colormap colormap;
GC gc;
XEvent event;
Pixmap tile[3];

void createWindow(int, int, char *);
void onEvent();
bool placeStone(int, int);
void drawBoard();
bool parseCommand(char *);
bool checkResult(int, int);

int main(int argc, char *argv[])
{
    int result;
    // Set the blocking time for select()
    timeVal.tv_sec = 0;
    timeVal.tv_usec = 10;
    createWindow(GRID_SIZE * BOARD_SIZE, GRID_SIZE * BOARD_SIZE, "Tic-Tac-Toe");

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
    hints.width = GRID_SIZE * BOARD_SIZE;
    hints.height = GRID_SIZE * BOARD_SIZE;

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
    XpmCreatePixmapFromData(display, window, none, &tile[NONE], NULL, NULL);
    XpmCreatePixmapFromData(display, window, circle, &tile[CIRCLE], NULL, NULL);
    XpmCreatePixmapFromData(display, window, cross, &tile[CROSS], NULL, NULL);
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
        gridX = (int)(event.xbutton.x) / GRID_SIZE;
        gridY = (int)(event.xbutton.y) / GRID_SIZE;
        placeStone(gridX, gridY);
        break;
    }
}

bool placeStone(int x, int y)
{
    if (x > -1 && x < BOARD_SIZE && y > -1 && y < BOARD_SIZE && board[y][x] == NONE)
    {
        board[y][x] = currentColor;
        drawBoard();
        // Flip the current stone color
        currentColor = (currentColor == CIRCLE ? CROSS : CIRCLE);
        return true;
    }
    return false;
}

void drawBoard()
{
    int x, y;
    for (y = 0; y < BOARD_SIZE; y++)
    {
        for (x = 0; x < BOARD_SIZE; x++)
        {
            switch (board[y][x])
            {
            case CIRCLE:
                XCopyArea(display, tile[CIRCLE], window, gc, 0, 0, GRID_SIZE, GRID_SIZE, x * GRID_SIZE, y * GRID_SIZE);
                break;
            case CROSS:
                XCopyArea(display, tile[CROSS], window, gc, 0, 0, GRID_SIZE, GRID_SIZE, x * GRID_SIZE, y * GRID_SIZE);
                break;
            default:
                XCopyArea(display, tile[NONE], window, gc, 0, 0, GRID_SIZE, GRID_SIZE, x * GRID_SIZE, y * GRID_SIZE);
                break;
            }
        }
    }
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
        else if (strcmp(cmdName, NEW_COMMAND) == 0)
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