/*
  Class 4I, 2026, NIT Ibaraki College
  Computer Networks II sample code
  GUI Skeleton program for the "Tic-Tac-Toe" Web API Client
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
#define BUF_SIZE 4096
#define PROTOCOL_DELIMITER "-\n"
#define ErrorExit(x)          \
    {                         \
        fprintf(stderr, "-"); \
        perror(x);            \
        exit(0);              \
    }

unsigned char board[BOARD_SIZE][BOARD_SIZE] = {NONE};
unsigned char currentColor = CIRCLE;
char buf[BUF_SIZE];

Display *display;
Window window;
Colormap colormap;
GC gc;
XEvent event;
Pixmap tile[3];

void createWindow(int left, int top, char *title);
void onEvent();
bool placeStone(int x, int y);
void drawBoard();
void sendMessage(char *message);

int main(int argc, char *argv[])
{
    createWindow(GRID_SIZE * BOARD_SIZE, GRID_SIZE * BOARD_SIZE, "Tic-Tac-Toe");

    while (1)
    {
        // Respond to window events
        onEvent();
        // Initialize buffer and send message to server
        bzero(buf, sizeof(char) * BUF_SIZE);
        sprintf(buf, "GET http://api.islaytouch.com/ttt.php?board=%s&key=%s\r\n", "100000000", "2023000");
        sendMessage(buf);
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

void sendMessage(char *message)
{
    // This function should send the message to the server and handle the response.
    // STUB!
    // printf("Message to be sent: %s\n", message);
    // STUB!
    // printf("Message received from server: %s\n", "HTTP/1.1 200 OK\nContent-Type: text/plain\n\n{\"board\":[[1,0,0],[0,2,0],[0,0,0]]}");
}
