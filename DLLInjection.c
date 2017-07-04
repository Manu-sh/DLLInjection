#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <winbase.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#define MSG(MSG,TITLE) (MessageBoxA(NULL, MSG, TITLE, MB_OK|MB_ICONINFORMATION))
#define MSGE(MSG) (MessageBoxA(NULL, MSG, NULL, MB_OK|MB_ICONERROR))
#define SETGLOBERR(FORMAT, ...) do{memset(err, 0, sizeof(err)); sprintf(err, FORMAT, ##__VA_ARGS__);}while(0)
#define DLL_INJECT_WC "DLL Injection"
#define s_free(PTR) do{free(PTR); PTR = NULL;}while(0)
#define g_free() do{if (dll) s_free(dll);}while(0)
#define MAX_PNAME 250
#define MAX_ERR 500

// anonymous enum
enum {
	ID_BUTTON_DLL_CHOICE = 100,
	ID_BUTTON_INJECT,
	ID_TEXTBOX,
	ID_TEXTBOX2
};

char err[MAX_ERR+1], *pname[MAX_ERR+1], *dll;
HWND textbox, textbox2;

DWORD pidof(const char *pname) {

	PROCESSENTRY32 processInfo;
	HANDLE processSnapshot;
	memset(&processInfo, 0, sizeof(PROCESSENTRY32));
	processInfo.dwSize = sizeof(PROCESSENTRY32);

	if ((processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) == INVALID_HANDLE_VALUE) {
		SETGLOBERR("CreateToolhelp32Snapshot()");
		return 0;
	}

	if (!Process32First(processSnapshot, &processInfo)) {
		SETGLOBERR("Process32First()");
		CloseHandle(processSnapshot);
		return 0;
	}

	if (strcmp(pname, processInfo.szExeFile) == 0) {
		CloseHandle(processSnapshot);
		return processInfo.th32ProcessID;
	}

	while (Process32Next(processSnapshot, &processInfo)) {
		if (strcmp(pname, processInfo.szExeFile) == 0) {
			CloseHandle(processSnapshot);
			return processInfo.th32ProcessID;
		}
	}

	CloseHandle(processSnapshot);
	return 0;
}

bool dllInject(DWORD pid, const char *dllpath) {

	bool stat = false;
	HANDLE handle, remoteThread;
	LPVOID LoadLibraryAddr; // functptr
	LPVOID newspace;

	if (!(handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid))) {
		SETGLOBERR("OpenProcess()");
		goto exit;
	}

	if (!(LoadLibraryAddr = GetProcAddress(GetModuleHandleA("Kernel32.dll"), "LoadLibraryA"))) {
		SETGLOBERR("OpenProcess()");
		goto exit;
	}

	if (!(newspace = VirtualAllocEx(handle, NULL, strlen(dllpath)+1, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)))
		SETGLOBERR("VirtualAllocEx()");

	if (!WriteProcessMemory(handle, newspace, dllpath, strlen(dllpath)+1, NULL))
		SETGLOBERR("WriteProcessMemory()");

	if (!(remoteThread = CreateRemoteThread(handle, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryAddr, newspace, 0, NULL)))
		SETGLOBERR("CreateRemoteThread()");
	else
		stat = true;

	VirtualFreeEx(remoteThread, LoadLibraryAddr, strlen(dllpath)+1, MEM_RELEASE);

	if (remoteThread) CloseHandle(remoteThread);
	CloseHandle(handle);
exit:
	return stat;

}

char * openfile(HWND wparent, const char *file_suffix) {

	OPENFILENAME ofn;
	char *file_path, filter[100];

	if (!(file_path = calloc(MAX_PATH+1, sizeof(char)))) {
		SETGLOBERR("calloc()");
		return NULL;
	}

	memset(&ofn,   0, sizeof(OPENFILENAME));
	memset(filter, 0, sizeof(filter));

	//"DLL File (*.dll)\0*.dll\0\0";
	sprintf(filter, "%s File (*.%s)%c" "*.%s%c%c", file_suffix, file_suffix, '\0', file_suffix, '\0', '\0');

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner   = wparent;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile   = file_path;
	ofn.nMaxFile	= MAX_PATH;
	ofn.Flags	   = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = file_suffix;

	if (!GetOpenFileName(&ofn)) {
		free(file_path);
		return NULL;
	}

	return file_path;
}

void parsewmcmd(HWND wthis, WPARAM wParam) {

		switch (LOWORD(wParam)) {

			case ID_BUTTON_DLL_CHOICE:

				if (HIWORD(wParam) == BN_CLICKED) {
					if (dll) {
						s_free(dll);
						SendMessage(textbox2, WM_SETTEXT, 0, (LPARAM)"select a dll");
					}

					if ((dll = openfile(wthis, "dll")))
						SendMessage(textbox2, WM_SETTEXT, 0, (LPARAM)dll);
				}
			break;

			case ID_BUTTON_INJECT:

				{
					if (Edit_GetTextLength(textbox) == 0) {
						MSG("You must provide a process name", "process not found");
						break;
					}

					memset(pname, 0, sizeof(pname));
					Edit_GetLine(textbox, 0, pname, MAX_PNAME);
					printf("%s\n", pname);

					if (!dll) {
						MSG("You must provide a dll to inject", "DLL not found");
						break;
					}

					DWORD pid;
					if (!(pid = pidof((const char *)pname))) {
						SETGLOBERR("Cannot get pid of process \"%s\"\n", pname);
						MSGE(err);
						break;
					}

					if (!dllInject(pid, dll))
						MSGE("Something went wrong, you can inject 32bit dll on 32bit process and 64bit dll on 64bit process");
				}
			break;
		}
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT event, WPARAM wParam, LPARAM lParam) {

	switch(event) {

		case WM_CREATE:

			{
				HWND sel_button, label, inj_button;

				sel_button = CreateWindow("BUTTON", "DLL...", WS_TABSTOP|WS_VISIBLE|WS_CHILD,
											320, 70, 70, 25, hwnd, (HMENU)ID_BUTTON_DLL_CHOICE, GetModuleHandle(NULL), NULL);

				label      = CreateWindow("STATIC", "Type process name here (es. calc.exe):", WS_VISIBLE|WS_CHILD,
											40, 50, 260, 15, hwnd, (HMENU)ID_TEXTBOX, GetModuleHandle(NULL), NULL);

				textbox    = CreateWindow("EDIT",  NULL, WS_VISIBLE|WS_CHILD,
											40, 75, MAX_PNAME, 20, hwnd, (HMENU)ID_TEXTBOX, GetModuleHandle(NULL), NULL);

				textbox2   = CreateWindow("EDIT", "select a dll", WS_VISIBLE|WS_CHILD|WS_DISABLED,
											15, 15, 430, 15, hwnd, (HMENU)ID_TEXTBOX2, GetModuleHandle(NULL), NULL);

				inj_button = CreateWindow("BUTTON", "Inject", WS_VISIBLE|WS_CHILD,
											260, 180, 150, 50, hwnd, (HMENU)ID_BUTTON_INJECT, GetModuleHandle(NULL), NULL);

				if (!sel_button || !label || !textbox || !inj_button) {
					MSGE("Can't draw your window");
					DestroyWindow(hwnd);
				}
			}

		break;
		case WM_COMMAND:
			parsewmcmd(hwnd, wParam);
		break;
		case WM_CLOSE:
			DestroyWindow(hwnd);
		break;
		case WM_DESTROY:
			PostQuitMessage(0);
		break;
		default:
			return DefWindowProc(hwnd, event, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	WNDCLASSEX wc;
	MSG Msg;
	HWND wmain;

	wc.cbSize	 = sizeof(WNDCLASSEX);
	wc.style	 = 0;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra	 = 0;
	wc.cbWndExtra	 = 0;
	wc.hInstance	 = hInstance;
	wc.hIcon	 = (HICON)LoadImage(hInstance, "icon.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
	wc.hCursor	 = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW); // COLOR_WINDOW+1
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = DLL_INJECT_WC;
	wc.hIconSm	 = (HICON)LoadImage(hInstance, "icon.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);

	if(!RegisterClassEx(&wc)) {
		MSGE("Can't draw your window");
		return EXIT_FAILURE;
	}

	wmain = CreateWindowEx(WS_EX_CLIENTEDGE, DLL_INJECT_WC, "DLLInjection",
								  WS_SYSMENU|WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 450, 290, NULL, NULL, hInstance, NULL);

	if(!wmain) {
		MSGE("Can't draw your window");
		return EXIT_FAILURE;
	}

	ShowWindow(wmain, nCmdShow);
	UpdateWindow(wmain);

	while(GetMessage(&Msg, NULL, 0, 0) > 0) {
		TranslateMessage(&Msg);
		DispatchMessage(&Msg); // call callback
	}

	return Msg.wParam;
}
