#include <shlwapi.h>
#include <windows.h>
#include <strsafe.h>
#include <shellapi.h>
#include <errno.h>
#include <time.h>

#define MAX_LINE_SIZE 255
#define WND_WIDTH 300
#define WND_HEIGHT 50

enum STATUS
{
    NOTFETCHED,
    UNKNOWN,
    DISABLED,
    ENABLED
};

struct INIPARAM
{
    LPWSTR lpszArgFile;
    LPWSTR lpszArgKey;
    LPWSTR lpszArgExpectedValue;
};

static const LPCSTR lpszAppName = "IniWatcher";
static const LPCSTR lpszTitle = "IniWatcher";

static const LPCWSTR lpszStatus = L"Status von %ls: %ls";
static const LPCWSTR STATUS_STRING[] = {L"", L"Unknown", L"Disabled", L"Enabled"};
static const COLORREF STATUS_COLOR[] = {0, RGB(240, 240, 240), RGB(255, 0, 0), RGB(0, 255, 0)};
static const COLORREF STATUS_BGCOLOR[] = {0, RGB(100, 100, 100), RGB(80, 0, 0), RGB(0, 80, 0)};

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

/* Helper methods */
POINTS getScreenResolution()
{
    POINT aPoint;
    LPPOINT lpPoint = &aPoint;

    GetCursorPos(lpPoint);
    HMONITOR hMonitor = MonitorFromPoint(aPoint, MONITOR_DEFAULTTONEAREST);

    MONITORINFO target;
    target.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(hMonitor, &target);

    POINTS res = {.x = target.rcMonitor.right, .y = target.rcMonitor.bottom};
    return res;
}

struct INIPARAM getIniParameters()
{
    int iArgs;
    struct INIPARAM iniParam;
    LPWSTR *aCommandline = CommandLineToArgvW(GetCommandLineW(), &iArgs);

    wprintf(L"Parsing %s..\n", GetCommandLineW());

    // skipping file name, hence i=1
    for (int i = 1; i < iArgs; i += 2)
    {
        if (!wcscmp(aCommandline[i], L"-file"))
        {
            iniParam.lpszArgFile = (LPWSTR)malloc((wcslen(aCommandline[i + 1]) + 1) * sizeof(WCHAR));
            wcsncpy(iniParam.lpszArgFile, aCommandline[i + 1], wcslen(aCommandline[i + 1]) + 1);
        }
        else if (!wcscmp(aCommandline[i], L"-key"))
        {
            iniParam.lpszArgKey = (LPWSTR)malloc((wcslen(aCommandline[i + 1]) + 1) * sizeof(WCHAR));
            wcsncpy(iniParam.lpszArgKey, aCommandline[i + 1], wcslen(aCommandline[i + 1]) + 1);
        }
        else if (!wcscmp(aCommandline[i], L"-value"))
        {
            iniParam.lpszArgExpectedValue = (LPWSTR)malloc((wcslen(aCommandline[i + 1]) + 1) * sizeof(WCHAR));
            wcsncpy(iniParam.lpszArgExpectedValue, aCommandline[i + 1], wcslen(aCommandline[i + 1]) + 1);
        }
        else
        {
            wprintf(L"Invalid parameter: %s - %s", aCommandline[i], aCommandline[i + 1]);
        }
    }

    // debug output
    wprintf(L"Watching %s for %s to be %s\n", iniParam.lpszArgFile, iniParam.lpszArgKey, iniParam.lpszArgExpectedValue);
    LocalFree(aCommandline);

    return iniParam;
}

void GetConfigPath(LPWSTR lpszTempPath)
{
    GetTempPathW(MAX_PATH, lpszTempPath);
    PathCombineW(lpszTempPath, lpszTempPath, L"iniwatcher.dat");
}

POINTS getWindowCoordinates()
{
    // file handle for permanent storage of the coordinates
    FILE *tmpFile;
    WCHAR lpszTempPath[MAX_PATH];
    WCHAR lpszLineBuffer[MAX_LINE_SIZE];

    POINTS retValue;

    GetConfigPath(lpszTempPath);

    if ((tmpFile = _wfopen(lpszTempPath, L"r")) != NULL)
    {
        fgetws(lpszLineBuffer, MAX_LINE_SIZE, tmpFile);
        retValue.x = StrToIntW(lpszLineBuffer);

        fgetws(lpszLineBuffer, MAX_LINE_SIZE, tmpFile);
        retValue.y = StrToIntW(lpszLineBuffer);
    }
    else
    {
        retValue = getScreenResolution();
        retValue.x -= WND_WIDTH;
        retValue.y -= WND_HEIGHT;
        retValue.y += 10;
    }

    fclose(tmpFile);

    return retValue;
}

/* Functional helper methods */
enum STATUS getStatus(struct INIPARAM inival)
{
    static int iLastGotStatusTime = 0;

    if (iLastGotStatusTime == time(NULL))
    {
        return NOTFETCHED;
    }

    iLastGotStatusTime = time(NULL);

    WCHAR sIniLineBuffer[MAX_LINE_SIZE];
    FILE *fp;

    if ((fp = _wfopen(inival.lpszArgFile, L"r")) == NULL)
    {
        perror("Failed to open file");
        return UNKNOWN;
    }

    while (fgetws(sIniLineBuffer, MAX_LINE_SIZE, fp) != NULL)
    {
        // if line starts with key..
        if (!wcsncmp(sIniLineBuffer, inival.lpszArgKey, wcslen(inival.lpszArgKey)))
        {
            // and then an equal sign follows..
            if (!wcsncmp(sIniLineBuffer + wcslen(inival.lpszArgKey), L"=", 1))
            {
                // .. AND the value is equal ..
                if (!wcsncmp(sIniLineBuffer + wcslen(inival.lpszArgKey) + 1, inival.lpszArgExpectedValue, wcslen(inival.lpszArgExpectedValue)))
                {
                    return ENABLED;
                }
                else
                {
                    return DISABLED;
                }
            }
            else
            {
                fputs("Failed to parse line: Missing '='.", stderr);
            }
        }
    }

    fclose(fp);
    LocalFree(sIniLineBuffer);

    return UNKNOWN;
}

void drawStatus(enum STATUS status, HWND hWnd, struct INIPARAM iniParam)
{
    static LPWSTR lpszCurrentDisplay = NULL;

    if (status == NOTFETCHED) return;

    // invalidate window to get a new label to be visible
    RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);

    HDC hDC;
    PAINTSTRUCT ps;
    COLORREF crStatusColor = STATUS_COLOR[status];
    COLORREF cStatusBgColor = STATUS_BGCOLOR[status];
    LPCWSTR cStatusString = STATUS_STRING[status];

    lpszCurrentDisplay = malloc(MAX_LINE_SIZE * sizeof(WCHAR) + sizeof(WCHAR));
    _snwprintf_s(lpszCurrentDisplay, MAX_LINE_SIZE, MAX_LINE_SIZE, lpszStatus, iniParam.lpszArgKey, cStatusString);

    hDC = BeginPaint(hWnd, &ps);

    SetBkColor(hDC, cStatusBgColor);
    SetTextColor(hDC, crStatusColor);
    TextOutW(hDC, 10, 10, lpszCurrentDisplay, wcslen(lpszCurrentDisplay));

    EndPaint(hWnd, &ps);

    // set window to top
    POINTS coord = getWindowCoordinates();
    SetWindowPos(hWnd, HWND_TOPMOST, coord.x, coord.y, WND_WIDTH, WND_HEIGHT, SWP_NOMOVE | SWP_NOSIZE);

    DeleteObject(hDC);
    DeleteObject(&ps);
}

/* Window functions */

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow)
{
    HWND hWnd;
    MSG msg;
    WNDCLASSEX wc;

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)RGB(0, 0, 0);
    wc.lpszClassName = lpszAppName;
    wc.lpszMenuName = lpszAppName;

    if (RegisterClassEx(&wc) == 0)
        return 0;

    POINTS coord = getWindowCoordinates();

    hWnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        lpszAppName,
        lpszTitle,
        WS_POPUP,
        coord.x, coord.y,
        WND_WIDTH, WND_HEIGHT,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (hWnd == NULL)
        return 0;

    struct INIPARAM iniParam = getIniParameters();

    // make window itself transparent since we only need the control on it to be displayed
    SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), (BYTE)0, LWA_COLORKEY);

    ShowWindow(hWnd, iCmdShow);
    UpdateWindow(hWnd);

    while (GetMessage(&msg, NULL, 0, 0))
    {
        // always draw status on window update - e.g. if hovering over it with the mouse
        drawStatus(getStatus(iniParam), hWnd, iniParam);

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT umsg, WPARAM wParam, LPARAM lParam)
{
    RECT rect;
    // cache for manual hit handling
    LRESULT hit;
    // memory storage for current window coordinates
    WINDOWPLACEMENT wpm;
    // file handle for permanent storage of the coordinates
    FILE *tmpFile;
    WCHAR lpszTempPath[MAX_PATH];

    switch (umsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        GetClientRect(hWnd, &rect);
        FillRect((HDC)wParam, &rect, CreateSolidBrush(RGB(0, 0, 0)));

        break;
    case WM_NCHITTEST:
        hit = DefWindowProc(hWnd, umsg, wParam, lParam);
        if (hit == HTCLIENT)
            hit = HTCAPTION;

        return hit;
    case WM_EXITSIZEMOVE:    
        // store window position for after restart
        GetWindowPlacement(hWnd, &wpm);
        GetConfigPath(lpszTempPath);

        if ((tmpFile = _wfopen(lpszTempPath, L"w")) != NULL)
        {
            fprintf(tmpFile, "%ld\n", wpm.rcNormalPosition.left);
            fprintf(tmpFile, "%ld\n", wpm.rcNormalPosition.top);
        }

        fclose(tmpFile);
        break;
    }

    return DefWindowProc(hWnd, umsg, wParam, lParam);
}
