#ifndef __WIN32_TYPES_LINUX_H
#define __WIN32_TYPES_LINUX_H

// Win32 type definitions for Linux
// These types allow Win32-style code to compile on Linux

#include <cstdint>

// Basic Win32 types
typedef void* HWND;
typedef void* OSW INDOWHANDLE;
typedef void* HDC;
typedef void* HINSTANCE;
typedef void* HANDLE;
typedef void* HMENU;
typedef void* HICON;
typedef void* HCURSOR;
typedef void* HRGN;
typedef void* HBRUSH;
typedef void* HFONT;
typedef void* HPEN;

// Integer types for Win32 compatibility
typedef unsigned long WPARAM;
typedef long LPARAM;
typedef long LRESULT;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef int BOOL;
typedef long LONG;
typedef unsigned long ULONG;
typedef void* LPVOID;
typedef const void* LPCVOID;

// Color type
typedef unsigned long COLORREF;

// Rectangle structure
typedef struct tagRECT {
    int left;
    int top;
    int right;
    int bottom;
} RECT, *LPRECT;

typedef const RECT* LPCRECT;

// Point structure
typedef struct tagPOINT {
    int x;
    int y;
} POINT, *LPPOINT;

// Size structure
typedef struct tagSIZE {
    int cx;
    int cy;
} SIZE, *LPSIZE;

// Boolean values
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

// Color macros
#ifndef RGB
#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#define GetRValue(rgb) ((BYTE)(rgb))
#define GetGValue(rgb) ((BYTE)(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) ((BYTE)((rgb)>>16))
#endif

// Parameter packing macros
#ifndef MAKELPARAM
#define MAKELPARAM(l, h) ((LPARAM)(DWORD)MAKELONG(l, h))
#endif

#ifndef MAKELONG
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#endif

#ifndef LOWORD
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#endif

// Window messages (subset needed for Winamp)
#define WM_CREATE           0x0001
#define WM_DESTROY          0x0002
#define WM_MOVE             0x0003
#define WM_SIZE             0x0005
#define WM_PAINT            0x000F
#define WM_CLOSE            0x0010
#define WM_QUIT             0x0012
#define WM_ERASEBKGND       0x0014
#define WM_SHOWWINDOW       0x0018

#define WM_MOUSEMOVE        0x0200
#define WM_LBUTTONDOWN      0x0201
#define WM_LBUTTONUP        0x0202
#define WM_RBUTTONDOWN      0x0204
#define WM_RBUTTONUP        0x0205

#define WM_KEYDOWN          0x0100
#define WM_KEYUP            0x0101
#define WM_CHAR             0x0102

#define WM_COMMAND          0x0111
#define WM_SYSCOMMAND       0x0112
#define WM_TIMER            0x0113

#define WM_USER             0x0400

// ShowWindow commands
#define SW_HIDE             0
#define SW_SHOWNORMAL       1
#define SW_SHOWMAXIMIZED    3
#define SW_MINIMIZE         6

// Raster operation codes
#define SRCCOPY     0x00CC0020
#define SRCPAINT    0x00EE0086
#define SRCAND      0x008800C6
#define SRCINVERT   0x00660046
#define NOTSRCCOPY  0x00330008

// Background modes
#define TRANSPARENT 1
#define OPAQUE      2

#endif // __WIN32_TYPES_LINUX_H
