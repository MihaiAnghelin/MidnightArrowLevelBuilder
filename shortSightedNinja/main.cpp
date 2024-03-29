#include <Windows.h>
#include "gameLayer.h"
#include <GL/glew.h>
#include <ctime>
#include <stdio.h>
#include <algorithm>
//#include <io.h>
//#include <fcntl.h>

#include "tools.h"
#include "opengl2Dlib.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#include "input.h"

extern bool g_WantUpdateHasGamepad;

LRESULT CALLBACK windProc(HWND, UINT, WPARAM, LPARAM);
static bool quit = 0;

void EnableOpenGL(HWND hwnd, HDC*, HGLRC*);

HWND wind;

static int lbutton = 0;
static int rbutton = 0;
static int lbuttonPressed = 0;
static int rbuttonPressed = 0;
static int bMouseMoved = 0;

static bool isFocus = 0;

extern "C"
{
//	__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
//	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int MAIN
{

	HINSTANCE h = GetModuleHandle(0);

	WNDCLASS wc = {};
	wc.hInstance = h;
	wc.lpfnWndProc = windProc;
	wc.lpszClassName = "wclass";
	wc.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

	RegisterClass(&wc);

	float width = 960;
	float heigth = 680;

	wind = CreateWindow(
		wc.lpszClassName,
		"Short sight ninja",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		550,
		100,
		width,
		heigth,
		NULL,
		NULL,
		h,
		0
	);

	HDC hdc;
	HGLRC hrc;

	EnableOpenGL(wind, &hdc, &hrc);
	if (glewInit() != GLEW_OK)
	{
		MessageBoxA(0, "glewInit", "Error from glew", MB_ICONERROR);
		return 1;
	}

	gl2d::setErrorFuncCallback([](const char* c) {elog(c); });
	gl2d::init();


	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   

	ImGui_ImplWin32_Init(wind);
	const char *glslVersion = "#version 330";
	ImGui_ImplOpenGL3_Init(glslVersion);
	ImGui::StyleColorsDark();

	input::loadXinput();

	if (!initGame())
	{
		return 0;
	}

	int time1 = clock();
	int time2 = clock();

	while (!quit)
	{
		MSG msg;
		if (PeekMessage(&msg, wind, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			time2 = clock();
			int deltaTime = time2 - time1;
			time1 = clock();
		
			float fDeltaTime = (float)deltaTime / CLOCKS_PER_SEC;
		
			//todo
			//fDeltaTime = std::min(fDeltaTime, 1.f / 20.f);
		
			input::updateInput();

			if (!gameLogic(fDeltaTime))
			{
				quit = true;
			}

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			imguiFunc(deltaTime);

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			SwapBuffers(hdc);
		
			lbuttonPressed = false;
			rbuttonPressed = false;
			bMouseMoved = false;
		}
	
	}

	closeGame();

	return 0;
}

LRESULT CALLBACK windProc(HWND wind, UINT m, WPARAM wp, LPARAM lp)
{
	if (ImGui::GetCurrentContext() == NULL)
		goto endImgui;
	
	{
		ImGuiIO& io = ImGui::GetIO();
		switch (m)
		{
		case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
		{
			int button = 0;
			if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) { button = 0; }
			if (m == WM_RBUTTONDOWN || m == WM_RBUTTONDBLCLK) { button = 1; }
			if (m == WM_MBUTTONDOWN || m == WM_MBUTTONDBLCLK) { button = 2; }
			if (m == WM_XBUTTONDOWN || m == WM_XBUTTONDBLCLK) { button = (GET_XBUTTON_WPARAM(wp) == XBUTTON1) ? 3 : 4; }
			if (!ImGui::IsAnyMouseDown() && ::GetCapture() == NULL)
				::SetCapture(wind);
			io.MouseDown[button] = true;
			goto endImgui;
		}
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONUP:
		{
			int button = 0;
			if (m == WM_LBUTTONUP) { button = 0; }
			if (m == WM_RBUTTONUP) { button = 1; }
			if (m == WM_MBUTTONUP) { button = 2; }
			if (m == WM_XBUTTONUP) { button = (GET_XBUTTON_WPARAM(wp) == XBUTTON1) ? 3 : 4; }
			io.MouseDown[button] = false;
			if (!ImGui::IsAnyMouseDown() && ::GetCapture() == wind)
				::ReleaseCapture();
			goto endImgui;
		}
		case WM_MOUSEWHEEL:
			io.MouseWheel += (float)GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA;
			goto endImgui;
		case WM_MOUSEHWHEEL:
			io.MouseWheelH += (float)GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA;
			goto endImgui;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (wp < 256)
				io.KeysDown[wp] = 1;
			goto endImgui;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (wp < 256)
				io.KeysDown[wp] = 0;
			goto endImgui;
		case WM_CHAR:
			// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
			io.AddInputCharacter((unsigned int)wp);
			goto endImgui;
		case WM_SETCURSOR:
			if (LOWORD(lp) == HTCLIENT && ImGui_ImplWin32_UpdateMouseCursor())
				goto endImgui;;
			goto endImgui;
		case WM_DEVICECHANGE:
			if ((UINT)wp == DBT_DEVNODES_CHANGED)
				g_WantUpdateHasGamepad = true;
			goto endImgui;
		}
	}
	endImgui:

	LRESULT l = 0;

	switch (m)
	{
	case WM_LBUTTONDOWN:
		lbutton = true;
		lbuttonPressed = true;
		break;
	case WM_RBUTTONDOWN:
		rbutton = true;
		rbuttonPressed = true;
		break;
	case WM_RBUTTONUP:
		rbutton = false;
		break;
	case WM_LBUTTONUP:
		lbutton = false;
		break;

	case WM_CLOSE:
		quit = true;
		break;
	case WM_MOUSEMOVE:
		bMouseMoved = 1;
		break;
	case WM_ACTIVATE:
		if (wp == WA_ACTIVE)
		{
			isFocus = true;
		}
		else if (wp == WA_INACTIVE)
		{
			isFocus = false;
		}
		break;

	default:
		l = DefWindowProc(wind, m, wp, lp);
		break;
	}

	return l;
}


void EnableOpenGL(HWND hwnd, HDC* hDC, HGLRC* hRC)
{
	//todo look into this more (all of this function)
	PIXELFORMATDESCRIPTOR pfd = {};

	int iFormat;

	/* get the device context (DC) */
	*hDC = GetDC(hwnd);


	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW |
		PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32; //todo look into this (24)
	pfd.cDepthBits = 16;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	iFormat = ChoosePixelFormat(*hDC, &pfd);

	SetPixelFormat(*hDC, iFormat, &pfd);

	/* create and enable the render context (RC) */
	*hRC = wglCreateContext(*hDC);

	wglMakeCurrent(*hDC, *hRC);
}


//platform independent functions implementation
///////////////////////////////////////////////
void errorMessage(const char *c)
{
	MessageBoxA(0, c, "error", MB_ICONERROR);
}

///sets the mouse pos relative to the window's drawing area
void setRelMousePosition(int x, int y)
{
	POINT p = { x, y };
	WINDOWPLACEMENT wp;

	GetWindowPlacement(wind, &wp);
	p.x += wp.rcNormalPosition.left;
	p.y += wp.rcNormalPosition.top;

	SetCursorPos(p.x, p.y);
}

//gets the drawing region sizes
glm::ivec2 getWindowSize()
{
	RECT r;
	GetClientRect(wind, &r);
	glm::ivec2 d;
	d.x = r.right - r.left;
	d.y = r.bottom - r.top;
	return d;
}

namespace platform
{
	///gets the mouse pos relative to the window's drawing area
	glm::ivec2 getRelMousePosition()
	{
		//todo refactor

		POINT p = {};
		GetCursorPos(&p);

		//WINDOWPLACEMENT wp;
		//
		//GetWindowPlacement(wind, &wp);
		//
		//p.x -= wp.rcNormalPosition.left;
		//p.y -= wp.rcNormalPosition.top;

		ScreenToClient(wind, &p);

		return { p.x, p.y };
	}


	int isKeyHeld(int key)
	{
		return GetAsyncKeyState(key);
	}

	int isKeyPressedOn(int key)
	{
		return GetAsyncKeyState(key) & 0x8000;
	}

	int isRMouseButtonPressed()
	{
		return rbuttonPressed;
	}

	int isLMouseButtonPressed()
	{
		ImGuiIO& io = ImGui::GetIO();
		return (!io.WantCaptureMouse) && lbuttonPressed;
	}

	int isLMouseHeld()
	{
		ImGuiIO& io = ImGui::GetIO();

		return (!io.WantCaptureMouse) && lbutton;
	}

	int isRMouseHeld()
	{
		return rbutton;
	}

	void showMouse(bool show)
	{
		ShowCursor(show);
	}

	bool isFocused()
	{
		return GetActiveWindow() == wind;
	}

	bool mouseMoved()
	{
		return bMouseMoved;
	}

};
