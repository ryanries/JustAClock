// Joseph Ryan Ries, 2021
// It sucks that I have to do this, but Windows 11 at this time does not
// allow us to display the clock on other monitors other than the primary
// monitor. It is very important to me to be able to display the clock on
// monitors other than the primary one.
// I currently only have two monitors to test with so this app only works 
// on 1 additional non-primary monitor. I'm happy to take pull requests 
// if you want to add support for multiple non-primary monitors.
// Also I haven't added logic for taskbar on the left or right or top of the monitor.
// IMPORTANT: "Per-monitor DPI Awareness" is set in the application's manifest!

#include <windows.h>

#include <stdio.h>

#define APP_NAME_W L"JustAClock"

#define CLOCK_WINDOW_WIDTH	80

volatile BOOL gShouldExit;

MONITORINFO gMonitorInfo = { sizeof(MONITORINFO) };


// Callback function called by EnumDisplayMonitors for each enabled monitor.
// Since EnumDisplayMonitors
// WARNING: CURRENTLY ONLY SUPPORTS ONE EXTRA MONITOR!
BOOL CALLBACK EnumDisplayProc(HMONITOR hMon, HDC dcMon, RECT* pRcMon, LPARAM Parameter)
{
	// Raymond Chen says: "The primary monitor by definition has its upper left corner at (0, 0)."
	if ((pRcMon->left != 0) && (pRcMon->top != 0))
	{
		// We found a secondary monitor!
		// We need to position our clock window at the bottom right corner of this monitor's taskbar.		

		if (GetMonitorInfoW(hMon, &gMonitorInfo) == 0)
		{
			MessageBoxW(NULL, L"GetMonitorInfoW() failed!", L"ERROR", MB_ICONERROR | MB_OK);

			gShouldExit = TRUE;
		}		
	}

	return(TRUE);
}

LRESULT CALLBACK ClockWindowProc(_In_ HWND WindowHandle, _In_ UINT Message, _In_ WPARAM WParam, _In_ LPARAM LParam)
{
	LRESULT Result = 0;

	switch (Message)
	{		
		case WM_CLOSE:
		{
			PostQuitMessage(0);

			gShouldExit = TRUE;

			break;
		}
		case WM_PAINT:
		{
			SYSTEMTIME Time = { 0 };

			wchar_t DateTimeString[32] = { 0 };			

			// The size of the entire clock window.
			RECT ClientRect = { 0 };

			// The size of the bounding rectangle where the text will go,
			// i.e., padding.
			RECT TextRect = { 0 };

			PAINTSTRUCT PaintStruct = { 0 };

			// For double buffering to avoid flicker.
			HDC MemoryDC = { 0 };

			HBITMAP MemoryBitmap = { 0 };

			GetClientRect(WindowHandle, &ClientRect);

			TextRect.top = ClientRect.top + 8;

			TextRect.bottom = ClientRect.bottom;

			TextRect.left = ClientRect.left;

			TextRect.right = ClientRect.right - 8;

			HDC Context = BeginPaint(WindowHandle, &PaintStruct);			

			MemoryDC = CreateCompatibleDC(Context);

			MemoryBitmap = CreateCompatibleBitmap(
				Context, 
				ClientRect.right - ClientRect.left, 
				ClientRect.bottom + ClientRect.left);

			SelectObject(MemoryDC, MemoryBitmap);

			SetTextColor(MemoryDC, RGB(255, 255, 255));

			SetBkMode(MemoryDC, TRANSPARENT);

			HBRUSH RedBrush = CreateSolidBrush(RGB(255, 0, 0));

			// Paint the entire memoryDC red, because remember we set red to be transparent.
			FillRect(MemoryDC, &ClientRect, RedBrush);

			//SetBkColor(MemoryDC, RGB(255, 0, 0));

			HFONT Font = GetStockObject(DEFAULT_GUI_FONT);
			
			LOGFONT logfont;

			GetObjectW(Font, sizeof(LOGFONT), &logfont);

			HFONT hNewFont = CreateFontIndirectW(&logfont);

			SelectObject(MemoryDC, hNewFont);

			GetLocalTime(&Time);
			
			_snwprintf_s(
				DateTimeString, 
				_countof(DateTimeString), 
				_TRUNCATE, 
				L"%02d:%02d:%02d\n%d/%d/%d", 
				Time.wHour, Time.wMinute, Time.wSecond, Time.wMonth, Time.wDay, Time.wYear);

			DrawTextW(MemoryDC, DateTimeString, -1, &TextRect, DT_NOCLIP | DT_RIGHT);

			BitBlt(
				Context, 
				0, 
				0, 
				ClientRect.right - ClientRect.left, 
				ClientRect.bottom + ClientRect.left, 
				MemoryDC, 
				0, 
				0, 
				SRCCOPY);

			DeleteObject(RedBrush);

			DeleteObject(MemoryBitmap);

			DeleteObject(hNewFont);

			DeleteDC(MemoryDC);

			EndPaint(WindowHandle, &PaintStruct);

			break;
		}
		default:
		{
			Result = DefWindowProcW(WindowHandle, Message, WParam, LParam);
		}
	}

	return(Result);
}

// The application's entry point.
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	DWORD Result = ERROR_SUCCESS;	
	
	MSG WindowMessage = { 0 };

	HWND ClockWindowHandle = NULL;

	WNDCLASSEXW WindowClass = { sizeof(WNDCLASSEXW) };	

	WindowClass.style = 0;

	WindowClass.lpfnWndProc = ClockWindowProc;

	// We will make red pixels transparent, effectively making the window transparent.
	WindowClass.hbrBackground = CreateSolidBrush(RGB(255, 0, 0));

	WindowClass.cbClsExtra = 0;

	WindowClass.cbWndExtra = 0;

	WindowClass.hInstance = GetModuleHandleW(NULL);	

	WindowClass.lpszMenuName = NULL;

	WindowClass.lpszClassName = APP_NAME_W L"_WINDOWCLASS";	

	if (RegisterClassExW(&WindowClass) == 0)
	{
		Result = GetLastError();

		OutputDebugStringW(L"RegisterClassExW failed!\n");

		goto Exit;
	}

	// I used Spy++ to basically trial-and-error my way into figuring out what the real
	// "taskbar window" was, so we can set that to be the parent of this clock window we're making.

	HWND TaskbarWindowHandle = FindWindowW(L"Shell_SecondaryTrayWnd", NULL);

	HWND RealTaskbarWindowHandle = FindWindowExW(TaskbarWindowHandle, NULL, L"WorkerW", NULL);

	HWND RealRealTaskbarWindowHandle = FindWindowExW(RealTaskbarWindowHandle, NULL, L"MSTaskListWClass", NULL);


	ClockWindowHandle = CreateWindowExW(
		0,
		WindowClass.lpszClassName,
		APP_NAME_W,
		WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CLOCK_WINDOW_WIDTH,
		100,
		RealRealTaskbarWindowHandle,
		NULL,
		GetModuleHandleW(NULL),
		NULL);

	if (ClockWindowHandle == NULL)
	{
		Result = GetLastError();

		OutputDebugStringW(L"CreateWindowExW failed!\n");

		goto Exit;
	}

	// Remove all window styles.
	SetWindowLongW(ClockWindowHandle, GWL_STYLE, 0);

	// Remove all extended window styles.
	SetWindowLongW(ClockWindowHandle, GWL_EXSTYLE, 0);

	// Set window style to layered so we can have transparency,
	// and TOOLWINDOW extended style to hide the window's taskbar icon.
	SetWindowLongW(
		ClockWindowHandle, 
		GWL_EXSTYLE, 
		GetWindowLongW(ClockWindowHandle, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TOOLWINDOW);

	// Make red pixels transparent.
	SetLayeredWindowAttributes(ClockWindowHandle, RGB(255, 0, 0), 0, LWA_COLORKEY);
	

	// Now we must determine where to place this window. It goes on the taskbar of the non-primary
	// monitor. How do we know which monitor is primary and which monitor is a secondary?
	// Raymond Chen says: "The primary monitor by definition has its upper left corner at (0, 0)."

	// The taskbar position is relative to the monitor that it's on, so some calculations will need to be
	// done to figure out the actual absolute coordinates where the clock window will need to be placed.

	EnumDisplayMonitors(NULL, NULL, EnumDisplayProc, (LPARAM)NULL);

	// Since the EnumDisplayMonitors function uses a callback, I assume it is non-blocking,
	// how to know when all callbacks are complete? What if the user has 24 monitors, it will take
	// longer than if the user only has 2 monitors. Don't know the best way to handle this.
	
	SetWindowPos(
		ClockWindowHandle, 
		HWND_TOPMOST,
		gMonitorInfo.rcMonitor.right - CLOCK_WINDOW_WIDTH - 1, 
		gMonitorInfo.rcWork.bottom,
		CLOCK_WINDOW_WIDTH, 
		48, // Height of the taskbar. TODO: Calculate this smartly
		0);
	

	// Make window visible again.
	SetWindowLongW(ClockWindowHandle, GWL_STYLE, WS_VISIBLE);

	// Message pump must never block or hang or become unresponsive.
	while (!gShouldExit)
	{
		while (PeekMessageW(&WindowMessage, ClockWindowHandle, 0, 0, PM_REMOVE))
		{
			DispatchMessageW(&WindowMessage);
		}

		Sleep(100);

		RedrawWindow(ClockWindowHandle, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);				
	}

Exit:

	if (Result != ERROR_SUCCESS)
	{
		wchar_t ErrorText[128] = { 0 };

		_snwprintf_s(ErrorText, _countof(ErrorText), _TRUNCATE, L"%s: Error 0x%08lx!", APP_NAME_W, Result);

		MessageBoxW(NULL, ErrorText, L"ERROR", MB_ICONERROR | MB_OK);
	}
}