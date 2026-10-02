#ifndef MAIN_WIN32_H__
#define MAIN_WIN32_H__

/*
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
*/

#include "client/renderer/gles.h"
#ifndef NO_EGL
#include <EGL/egl.h>
#endif
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <windowsx.h>

#include <winsock2.h>
#include <process.h>

#include <cstdio>
#include <malloc.h>
#include <exception>
#include <cstdlib>

#ifndef TWF_WANT_PALM
#define TWF_WANT_PALM 0x00000002
#endif
#ifndef WM_TOUCH
#define WM_TOUCH 0x0240
#endif
#include "platform/input/Mouse.h"
#include "platform/input/Multitouch.h"
#include "util/Mth.h"
#include "AppPlatform_win32.h"

static App* g_app = 0;
static volatile bool g_running = true;

// ------------------------------------------------------------------
// Real touch input (WM_TOUCH). The window is registered for raw touch
// so every contact becomes its own Multitouch pointer (mouse emulation
// only gives one finger). The primary contact also feeds the Mouse to
// keep GUI clicks working as before, but only while a screen is open:
// in-game the touch must reach Multitouch/TouchInputHolder only, or a
// look-drag would be seen by Minecraft as a left click (digging).
// ------------------------------------------------------------------
static DWORD g_touchSlotIds[Multitouch::MAX_POINTERS];
static bool g_touchSlotInUse[Multitouch::MAX_POINTERS];

static inline bool isGuiScreenOpen() {
	Minecraft* mc = (Minecraft*)g_app;
	return mc != NULL && mc->screen != NULL;
}

static void initTouchSlots() {
	for (int i = 0; i < Multitouch::MAX_POINTERS; ++i) {
		g_touchSlotIds[i] = (DWORD)-1;
		g_touchSlotInUse[i] = false;
	}
}

static int findTouchSlot(DWORD id) {
	for (int i = 0; i < Multitouch::MAX_POINTERS; ++i)
		if (g_touchSlotInUse[i] && g_touchSlotIds[i] == id)
			return i;
	return -1;
}

static int allocTouchSlot(DWORD id) {
	for (int i = 0; i < Multitouch::MAX_POINTERS; ++i)
		if (!g_touchSlotInUse[i]) {
			g_touchSlotInUse[i] = true;
			g_touchSlotIds[i] = id;
			return i;
		}
	return -1;
}

static void freeTouchSlot(int slot) {
	if (slot < 0 || slot >= Multitouch::MAX_POINTERS)
		return;
	g_touchSlotInUse[slot] = false;
	g_touchSlotIds[slot] = (DWORD)-1;
}

// TOUCHINPUT x/y are in hundredths of a pixel of screen coordinates
static void feedTouchInput(HWND hWnd, const TOUCHINPUT& ti) {
	DWORD id = ti.dwID;
	POINT pt;
	pt.x = ti.x / 100;
	pt.y = ti.y / 100;
	ScreenToClient(hWnd, &pt);

	if (pt.x < SHRT_MIN || pt.x > SHRT_MAX || pt.y < SHRT_MIN || pt.y > SHRT_MAX)
		return;

	bool primary = (ti.dwFlags & TOUCHEVENTF_PRIMARY) != 0;
	Minecraft* mc = (Minecraft*)g_app;
	bool isGuiOrHotbar = isGuiScreenOpen() || (mc && mc->gui.getSlotIdAt((int)pt.x, (int)pt.y) != -1);

	if (ti.dwFlags & TOUCHEVENTF_UP) {
		int slot = findTouchSlot(id);
		if (slot < 0) return;
		Multitouch::feed(1, 0, (short)pt.x, (short)pt.y, slot);
		if (primary && isGuiOrHotbar) Mouse::feed(MouseAction::ACTION_LEFT, 0, (short)pt.x, (short)pt.y);
		freeTouchSlot(slot);
		return;
	}

	if (ti.dwFlags & TOUCHEVENTF_DOWN) {
		int slot = allocTouchSlot(id);
		if (slot < 0) return;
		Multitouch::feed(1, 1, (short)pt.x, (short)pt.y, slot);
		if (primary && isGuiOrHotbar) Mouse::feed(MouseAction::ACTION_LEFT, 1, (short)pt.x, (short)pt.y);
		return;
	}

	// TOUCHEVENTF_MOVE
	int slot = findTouchSlot(id);
	if (slot < 0) return;
	Multitouch::feed(0, 0, (short)pt.x, (short)pt.y, slot);
	if (primary && isGuiOrHotbar) Mouse::feed(MouseAction::ACTION_MOVE, 0, (short)pt.x, (short)pt.y);
}

// Keeps the system cursor inside our client area while the mouse is
// grabbed, so it can never wander off a small window. Unclipped as soon
// as the mouse is released or the window loses focus.
static HWND g_mainHwnd = NULL;
static bool clippedToGameWindow = false;

static void updateCursorClip(HWND hWnd, bool on) {
	static HWND clippedWnd = NULL;

	if (on && hWnd != NULL) {
		RECT rc;
		GetClientRect(hWnd, &rc);
		POINT tl, br;
		tl.x = rc.left; tl.y = rc.top;
		br.x = rc.right; br.y = rc.bottom;
		ClientToScreen(hWnd, &tl);
		ClientToScreen(hWnd, &br);
		RECT screenRect;
		screenRect.left = tl.x;
		screenRect.top = tl.y;
		screenRect.right = br.x;
		screenRect.bottom = br.y;
		clippedWnd = hWnd;
		ClipCursor(&screenRect);
		clippedToGameWindow = true;
	} else if (clippedWnd != NULL) {
		ClipCursor(NULL);
		clippedWnd = NULL;
		clippedToGameWindow = false;
	}
}

// Keeps the OS cursor clip in step with the game's mouse-grab state. The game
// releases the mouse as soon as a screen opens (Esc, pause, menus), but
// ClipCursor() used to survive that, so the cursor stayed trapped inside the
// game window instead of being free in the menus.
void win32SyncCursorClip(bool grabbed) {
	if(g_mainHwnd == NULL)
		return;
	if(grabbed == clippedToGameWindow)
		return;
	updateCursorClip(g_mainHwnd, grabbed);
}

static int getBits(int bits, int startBitInclusive, int endBitExclusive, int shiftTruncate) {
	int sum = 0;
	for (int i = startBitInclusive; i<endBitExclusive; ++i)
		sum += (bits & (2<<i));
	return shiftTruncate? (sum >> startBitInclusive) : sum;
}

void resizeWindow(HWND hWnd, int nWidth, int nHeight) {
   RECT rcClient, rcWindow;
   POINT ptDiff;
     GetClientRect(hWnd, &rcClient);
     GetWindowRect(hWnd, &rcWindow);
   ptDiff.x = (rcWindow.right - rcWindow.left) - rcClient.right;
   ptDiff.y = (rcWindow.bottom - rcWindow.top) - rcClient.bottom;
   MoveWindow(hWnd,rcWindow.left, rcWindow.top, nWidth + ptDiff.x, nHeight + ptDiff.y, TRUE);
}

void toggleResolutions(HWND hwnd, int direction) {
	static int n = 0;
	static int sizes[][3] = {
		{854, 480, 1},
		{800, 480, 1},
		{480, 320, 1},
		{1024, 768, 1},
		{1280, 800, 1},
		{1024, 580, 1}
	};
	static int count = sizeof(sizes) / sizeof(sizes[0]);
	n = (count + n + direction) % count;
	
	int* size = sizes[n];
	int k = size[2];
	
	resizeWindow(hwnd, k * size[0], k * size[1]);
}

static inline bool isTouchEvent() {
	LPARAM extra = (LPARAM)GetMessageExtraInfo();
	return (extra & 0xFFFFFF00) == 0xFF515700;
}

LRESULT WINAPI windowProc ( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
	LRESULT retval = 1;
	
	switch (uMsg)
	{
	case WM_KEYDOWN: {
		if (wParam == 33) toggleResolutions(hWnd, -1);
		if (wParam == 34) toggleResolutions(hWnd, +1);
		
		Keyboard::feed((unsigned char) wParam, 1);
		return 0;
	}
	case WM_KEYUP: {
		Keyboard::feed((unsigned char) wParam, 0);
		return 0;
	}
	// Modifier keys (shift in particular) are delivered as WM_SYSKEYDOWN /
	// WM_SYSKEYUP, never as WM_KEYDOWN / WM_KEYUP, so without these two
	// shift could never be held down at all.
	case WM_SYSKEYDOWN: {
		Keyboard::feed((unsigned char) wParam, 1);
		return 0;
	}
	case WM_SYSKEYUP: {
		Keyboard::feed((unsigned char) wParam, 0);
		return 0;
	}
	case WM_CHAR: {
		if(wParam >= 32)
			Keyboard::feedText(wParam);
		return 0;
	}

	case WM_LBUTTONDOWN: {
		if (isTouchEvent()) break;
		short mx = GET_X_LPARAM(lParam);
		short my = GET_Y_LPARAM(lParam);
		Minecraft* mc = (Minecraft*)g_app;
		if (mc && !mc->screen && mc->isLevelGenerated() && !mc->mouseGrabbed && !mc->useTouchscreen()) {
			mc->grabMouse();
			RECT rc; GetClientRect(hWnd, &rc);
			POINT pt = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
			ClientToScreen(hWnd, &pt);
			SetCursorPos(pt.x, pt.y);
			updateCursorClip(hWnd, true);
		}
		Mouse::feed( MouseAction::ACTION_LEFT, 1, mx, my);
		break;
	}
	case WM_LBUTTONUP: {
		if (isTouchEvent()) break;
		Mouse::feed( MouseAction::ACTION_LEFT, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	}
	case WM_RBUTTONDOWN: {
		if (isTouchEvent()) break;
		Mouse::feed( MouseAction::ACTION_RIGHT, 1, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	}
	case WM_RBUTTONUP: {
		if (isTouchEvent()) break;
		Mouse::feed( MouseAction::ACTION_RIGHT, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	}
	case WM_MOUSEWHEEL: {
		if (isTouchEvent()) break;
		short delta = (short)(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
		if (delta == 0) break;
		POINT pt;
		pt.x = GET_X_LPARAM(lParam);
		pt.y = GET_Y_LPARAM(lParam);
		ScreenToClient(hWnd, &pt);
		if (pt.x < SHRT_MIN || pt.x > SHRT_MAX || pt.y < SHRT_MIN || pt.y > SHRT_MAX)
			break;
		short step = (short)(delta > 0 ? 1 : -1);
		Mouse::feed(MouseAction::ACTION_WHEEL, delta > 0 ? MouseAction::DATA_UP : MouseAction::DATA_DOWN,
			(short)pt.x, (short)pt.y, 0, step);
		break;
	}
	case WM_MOUSEMOVE: {
		if (isTouchEvent()) break;
		short mx = GET_X_LPARAM(lParam);
		short my = GET_Y_LPARAM(lParam);
		Minecraft* mc = (Minecraft*)g_app;
		if (mc && mc->mouseGrabbed && !mc->screen && !mc->useTouchscreen()) {
			RECT rc; GetClientRect(hWnd, &rc);
			int cx = (rc.right - rc.left) / 2;
			int cy = (rc.bottom - rc.top) / 2;
			int dx = mx - cx;
			int dy = my - cy;
			if (dx != 0 || dy != 0) {
				Mouse::feed(MouseAction::ACTION_MOVE, 0, mx, my, (short)dx, (short)dy);
				Mouse::feedRawDelta((short)dx, (short)dy);
				POINT pt = { cx, cy };
				ClientToScreen(hWnd, &pt);
				SetCursorPos(pt.x, pt.y);
			}
			updateCursorClip(hWnd, true);
		} else {
			if(clippedToGameWindow)
				updateCursorClip(hWnd, false);
			Mouse::feed(MouseAction::ACTION_MOVE, 0, mx, my);
		}
		break;
	}
	case WM_KILLFOCUS: {
		updateCursorClip(hWnd, false);
		Minecraft* mc = (Minecraft*)g_app;
		if (mc && mc->mouseGrabbed) {
			mc->releaseMouse();
		}
		break;
	}
	case WM_TOUCH: {
		UINT count = LOWORD(wParam);
		if (count > 0) {
			TOUCHINPUT* pInputs = (TOUCHINPUT*)_alloca(sizeof(TOUCHINPUT) * count);
			if (GetTouchInputInfo((HTOUCHINPUT)lParam, count, pInputs, sizeof(TOUCHINPUT))) {
				for (UINT i = 0; i < count; ++i) {
					feedTouchInput(hWnd, pInputs[i]);
				}
			}
			CloseTouchInputHandle((HTOUCHINPUT)lParam);
		}
		return 0;
	}
	default:
		if (uMsg == WM_NCDESTROY) {
			updateCursorClip(hWnd, false);
			g_running = false;
		}
		else {
			if (uMsg == WM_SIZE) {
				if (g_app) g_app->setSize( GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) );
				// The clip rect follows the client area, so re-apply it
				Minecraft* mc = (Minecraft*)g_app;
				updateCursorClip(hWnd, mc != NULL && mc->mouseGrabbed);
			}
		}
		retval = DefWindowProc (hWnd, uMsg, wParam, lParam);
		break;
	}
	return retval;
}

void platform(HWND *result, int width, int height) {
	WNDCLASS wc;
	RECT wRect;
	HWND hwnd;
	HINSTANCE hInstance;

	wRect.left = 0L;
	wRect.right = (long)width;
	wRect.top = 0L;
	wRect.bottom = (long)height;

	hInstance = GetModuleHandle(NULL);

	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC)windowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "OGLES";

	RegisterClass(&wc);

	AdjustWindowRectEx(&wRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);

	hwnd = CreateWindowEx(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, "OGLES", "main", WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, wRect.right-wRect.left, wRect.bottom-wRect.top, NULL, NULL, hInstance, NULL);
	*result = hwnd;
	g_mainHwnd = hwnd;

	// Enable raw multi-touch input for the window (works from Windows 7 up).
	initTouchSlots();
	if (hwnd && !RegisterTouchWindow(hwnd, TWF_WANT_PALM)) {
		printf("RegisterTouchWindow failed: %d\n", GetLastError());
	}
}

/** Thread that reads input data via UDP network datagrams
    and fills Mouse and Keyboard structures accordingly.
	@note: The bound local net address is unfortunately
	       hard coded right now (to prevent wrong Interface) */
void inputNetworkThread(void* userdata)
{
	// set up an UDP socket for listening
	WSADATA wsaData;
	if (WSAStartup(0x101, &wsaData)) {
		printf("Couldn't initialize winsock\n");
		return;
	}

	SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET) {
		printf("Couldn't create socket\n");
		return;
	}
	
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9991);
	addr.sin_addr.s_addr = inet_addr("192.168.0.119");

	if (bind(s, (sockaddr*)&addr, sizeof(addr))) {
		printf("Couldn't bind socket to port 9991\n");
		return;
	}
	
	sockaddr fromAddr;
	int fromAddrLen = sizeof(fromAddr);

	char buf[1500];
	int* iptrBuf = (int*)buf;

	printf("input-server listening...\n");

	while (1) {
		int read = recvfrom(s, buf, 1500, 0, &fromAddr, &fromAddrLen);
		if (read < 0)
		{
			printf("recvfrom failed with code: %d\n", WSAGetLastError());
			return;
		}
		// Keyboard
		if (read == 2) {
			Keyboard::feed((unsigned char) buf[0], (int)buf[1]);
		}
		// Mouse
		else if (read == 16) {
			Mouse::feed(iptrBuf[0], iptrBuf[1], iptrBuf[2], iptrBuf[2]);
		}
	}
}

static void dbgTrace(const char* m) {
	FILE* f = fopen("dbg_trace.txt", "a");
	if (f) { fprintf(f, "%s\n", m); fclose(f); }
}

static LONG WINAPI crashFilter(_EXCEPTION_POINTERS* ep) {
	FILE* f = fopen("crash_arm32.txt", "w");
	if (f) {
		fprintf(f, "code=0x%08X addr=0x%08X\n",
			(unsigned)ep->ExceptionRecord->ExceptionCode,
			(unsigned)(ULONG_PTR)ep->ExceptionRecord->ExceptionAddress);
		fprintf(f, "pc=0x%08X lr=0x%08X sp=0x%08X cpsr=0x%08X\n",
			(unsigned)(ULONG_PTR)ep->ContextRecord->Pc,
			(unsigned)(ULONG_PTR)ep->ContextRecord->Lr,
			(unsigned)(ULONG_PTR)ep->ContextRecord->Sp,
			(unsigned)ep->ContextRecord->Cpsr);
		HMODULE m = GetModuleHandle(NULL);
		fprintf(f, "modbase=0x%08X lr_rva=0x%08X addr_rva=0x%08X\n",
			(unsigned)(ULONG_PTR)m,
			(unsigned)((ULONG_PTR)ep->ContextRecord->Lr - (ULONG_PTR)m),
			(unsigned)((ULONG_PTR)ep->ExceptionRecord->ExceptionAddress - (ULONG_PTR)m));
		fclose(f);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

// An uncaught C++ exception calls terminate() and then abort(), and the
// unhandled-exception filter is never reached, so the crash used to
// leave no trace at all (a bad_alloc while joining a server was the
// most likely candidate).
static void crashTerminate() {
	FILE* f = fopen("crash_arm32.txt", "w");
	if (f) {
		fprintf(f, "terminate: uncaught C++ exception (see mcpe_log.txt)\n");
		fclose(f);
	}
	_exit(3);
}

int main(void) {
	SetUnhandledExceptionFilter(crashFilter);
	std::set_terminate(crashTerminate);
	dbgTrace("a: main enter");
	AppContext appContext;
	MSG sMessage;

#ifndef STANDALONE_SERVER

	HWND hwnd;
	HDC hdc = 0;
	HGLRC hglrc = 0;
	g_running = true;

	// Platform init.
	dbgTrace("b: appContext.platform");
	appContext.platform = new AppPlatform_win32();
	dbgTrace("c: platform()");
	platform(&hwnd, appContext.platform->getScreenWidth(), appContext.platform->getScreenHeight());
	dbgTrace("d: ShowWindow");
	ShowWindow(hwnd, SW_SHOW);
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);

#ifdef NO_EGL
	// WGL init.
	hdc = GetDC(hwnd);
	printf("[dbg] wgl: got dc\n"); fflush(stdout);

	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd, 0, sizeof(pfd));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int pixelFormat = ChoosePixelFormat(hdc, &pfd);
	printf("[dbg] wgl: ChoosePixelFormat=%d\n", pixelFormat); fflush(stdout);
	int spf = SetPixelFormat(hdc, pixelFormat, &pfd);
	printf("[dbg] wgl: SetPixelFormat=%d\n", spf); fflush(stdout);

	hglrc = wglCreateContext(hdc);
	printf("[dbg] wgl: CreateContext=%p\n", (void*)hglrc); fflush(stdout);
	int cur = wglMakeCurrent(hdc, hglrc);
	printf("[dbg] wgl: MakeCurrent=%d\n", cur); fflush(stdout);

	glInit();
	printf("[dbg] wgl: glInit done\n"); fflush(stdout);
	{
		FILE* gi = fopen("gl_info.txt", "w");
		if (gi) {
			fprintf(gi, "version=%s\n", (const char*)glGetString(GL_VERSION));
			fprintf(gi, "vendor=%s\n", (const char*)glGetString(GL_VENDOR));
			fprintf(gi, "renderer=%s\n", (const char*)glGetString(GL_RENDERER));
			fprintf(gi, "glewDepthRangef=%p glewBindBuffer=%p\n",
				(void*)(void**)__glewDepthRangef, (void*)(void**)__glewBindBuffer);
			fclose(gi);
		}
	}
#else
	// EGL init.
	EGLint aEGLAttributes[] = {
		EGL_RED_SIZE,		8,
		EGL_GREEN_SIZE,		8,
		EGL_BLUE_SIZE,		8,
		EGL_ALPHA_SIZE,		8,
		EGL_DEPTH_SIZE,		16,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
		EGL_NONE
	};
	EGLConfig m_eglConfig[1];
	EGLint nConfigs;

	appContext.display = eglGetDisplay(GetDC(hwnd));
	eglInitialize(appContext.display, NULL, NULL);
	eglChooseConfig(appContext.display, aEGLAttributes, m_eglConfig, 1, &nConfigs);
	printf("EGLConfig = %p\n", m_eglConfig[0]);

	appContext.surface = eglCreateWindowSurface(appContext.display, m_eglConfig[0], (NativeWindowType)hwnd, 0);
	printf("EGLSurface = %p\n", appContext.surface);

	appContext.context = eglCreateContext(appContext.display, m_eglConfig[0], EGL_NO_CONTEXT, NULL);
	printf("EGLContext = %p\n", appContext.context);
	if (!appContext.context) {
		printf("EGL error: %d\n", eglGetError());
	}

	eglMakeCurrent(appContext.display, appContext.surface, appContext.surface, appContext.context);

	glInit();
#endif

#endif
	App* app = new MAIN_CLASS();
	printf("[dbg] new MAIN_CLASS done\n"); fflush(stdout);

	g_app = app;
	((MAIN_CLASS*)g_app)->externalStoragePath = ".";
	((MAIN_CLASS*)g_app)->externalCacheStoragePath = ".";
	printf("[dbg] pre g_app->init()\n"); fflush(stdout);
	dbgTrace("e: pre init()");
	g_app->init(appContext);
	dbgTrace("f: post init()");
	printf("[dbg] init done\n"); fflush(stdout);
	g_app->setSize(appContext.platform->getScreenWidth(), appContext.platform->getScreenHeight());
	printf("[dbg] setSize done\n"); fflush(stdout);

	//_beginthread(inputNetworkThread, 0, 0);
	
	// Main event loop
	while(g_running && !app->wantToQuit())
	{
		// Do Windows stuff:
		while (PeekMessage (&sMessage, NULL, 0, 0, PM_REMOVE) > 0) {
			if(sMessage.message == WM_QUIT) {
				g_running = false;
				break;
			}
			else {
				TranslateMessage(&sMessage);
				DispatchMessage(&sMessage);
			}
		}
		Multitouch::commit();
		static int framecnt = 0;
		app->update();
#ifdef NO_EGL
		SwapBuffers(hdc);
#endif
		if ((framecnt++ % 30) == 0) printf("[dbg] frame %d\n", framecnt); fflush(stdout);
		//Sleep(30);
	}

	Sleep(50);
	delete app;
	Sleep(50);
	appContext.platform->finish();
	Sleep(50);
	delete appContext.platform;
	Sleep(50);
	//printf("_crtDumpMemoryLeaks: %d\n", _CrtDumpMemoryLeaks());
	
#ifndef STANDALONE_SERVER
	// Exit.
#ifdef NO_EGL
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hglrc);
	ReleaseDC(hwnd, hdc);
#else
	eglMakeCurrent(appContext.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(appContext.display, appContext.context);
	eglDestroySurface(appContext.display, appContext.surface);
	eglTerminate(appContext.display);
#endif
#endif

	return 0;
}

#endif /*MAIN_WIN32_H__*/
