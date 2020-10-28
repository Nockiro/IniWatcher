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

static LPCWSTR lpszStatus = L"Status von %ls: %ls";
static LPCWSTR STATUS_STRING[] = {L"", L"Unknown", L"Disabled", L"Enabled"};
static const COLORREF STATUS_COLOR[] = {0, RGB(220, 220, 220), RGB(255, 0, 0), RGB(0, 255, 0)};
static const COLORREF STATUS_BGCOLOR[] = {0, RGB(220, 220, 220), RGB(80, 0, 0), RGB(0, 80, 0)};

static LPWSTR argFile;
static LPWSTR argKey;
static LPWSTR argExpectedValue;

static long screenResX = 0;
static long screenResY = 0;

//Deklaration von WndProc
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

//Name der Applikation
LPCSTR lpszAppName = "AppName";
//Titelleiste der Applikation
LPCSTR lpszTitle = "IniWatcher";

static int lastGotStatusTime;
enum STATUS getStatus()
{
    if (lastGotStatusTime == time(NULL))
    {
        return NOTFETCHED;
    }

    lastGotStatusTime = time(NULL);

    WCHAR buffer[MAX_LINE_SIZE];
    FILE *fp;

    if ((fp = _wfopen(argFile, L"r")) == NULL)
    {
        perror("Failed to open file");
        return UNKNOWN;
    }

    while (fgetws(buffer, MAX_LINE_SIZE, fp) != NULL)
    {
        // if line starts with key..
        if (!wcsncmp(buffer, argKey, wcslen(argKey)))
        {
            // and then an equal sign follows..
            if (!wcsncmp(buffer + wcslen(argKey), L"=", 1))
            {
                // .. AND the value is equal ..
                if (!wcsncmp(buffer + wcslen(argKey) + 1, argExpectedValue, wcslen(argExpectedValue)))
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
    LocalFree(buffer);

    return UNKNOWN;
}

static LPWSTR lpszCurrentOutString = NULL;
void drawStatus(enum STATUS status, HWND hWnd)
{
    if (status == NOTFETCHED) {
        return;
    }

    // invalidate window to get a new label to be visible
    RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);

    HDC hDC;
    PAINTSTRUCT ps;
    COLORREF cStatusColor = STATUS_COLOR[status];
    COLORREF cStatusBgColor = STATUS_BGCOLOR[status];
    LPCWSTR cStatusString = STATUS_STRING[status];

    lpszCurrentOutString = malloc (MAX_LINE_SIZE * sizeof(WCHAR)) + sizeof(WCHAR);
    _snwprintf_s(lpszCurrentOutString, MAX_LINE_SIZE, MAX_LINE_SIZE, lpszStatus, argKey, cStatusString);

    hDC = BeginPaint(hWnd, &ps);

    SetBkColor(hDC, cStatusBgColor);
    SetTextColor(hDC, cStatusColor);
    TextOutW(hDC, 10, 10, lpszCurrentOutString, wcslen(lpszCurrentOutString));

    EndPaint(hWnd, &ps);

    // set window to top
    SetWindowPos(hWnd, HWND_TOPMOST, screenResX - WND_WIDTH, screenResY - WND_HEIGHT + 10, WND_WIDTH, WND_HEIGHT, SWP_NOMOVE | SWP_NOSIZE);

    DeleteObject(hDC);
    DeleteObject(&ps);
}

void processCommandLineArgs()
{
    int nArgs;
    LPWSTR *szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);

    wprintf(L"Parsing %s..\n", GetCommandLineW());

    int i = 1;
    // skipping file name..
    for (i = 1; i < nArgs; i += 2)
    {
        if (!wcscmp(szArglist[i], L"-file"))
        {
            argFile = (LPWSTR)malloc((wcslen(szArglist[i + 1]) + 1) * sizeof(WCHAR));
            wcsncpy(argFile, szArglist[i + 1], wcslen(szArglist[i + 1]) + 1);
        }
        else if (!wcscmp(szArglist[i], L"-key"))
        {
            argKey = (LPWSTR)malloc((wcslen(szArglist[i + 1]) + 1) * sizeof(WCHAR));
            wcsncpy(argKey, szArglist[i + 1], wcslen(szArglist[i + 1]) + 1);
        }
        else if (!wcscmp(szArglist[i], L"-value"))
        {
            argExpectedValue = (LPWSTR)malloc((wcslen(szArglist[i + 1]) + 1) * sizeof(WCHAR));
            wcsncpy(argExpectedValue, szArglist[i + 1], wcslen(szArglist[i + 1]) + 1);
        }
        else
        {
            wprintf(L"Invalid parameter: %s - %s", szArglist[i], szArglist[i + 1]);
        }
    }

    wprintf(L"Watching %s for %s to be %s\n", argFile, argKey, argExpectedValue);

    // Free memory allocated for CommandLineToArgvW arguments.
    LocalFree(szArglist);
}

void cleanUp()
{
    free(argFile);
    free(argKey);
    free(argExpectedValue);
    free(lpszCurrentOutString);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow)
{   
    HWND hWnd;     //Fensterhandle
    MSG msg;       //Nachrichten
    WNDCLASSEX wc; //Fensterklasse

    //Größe der Struktur WNDCLASSEX
    wc.cbSize = sizeof(WNDCLASSEX);
    //Fensterstil
    wc.style = CS_HREDRAW | CS_VREDRAW;
    //Prozedur für das Fenster
    wc.lpfnWndProc = WndProc;
    //Speicher für weitere Infos reservieren
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    //Instanz-Handle
    wc.hInstance = hInstance;
    //Mauscursor
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    //Icon
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    //Hintergrundfarbe
    wc.hbrBackground = (HBRUSH)RGB(0,0,0);
    //Applikationsname
    wc.lpszClassName = lpszAppName;
    //Menüname
    wc.lpszMenuName = lpszAppName;
    //Icon neben dem Fenstertitel
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    //Fensterklasse registrieren
    if (RegisterClassEx(&wc) == 0)
        return 0;

    MONITORINFO target;
    target.cbSize = sizeof(MONITORINFO);

    POINT aPoint;
    LPPOINT lpPoint = &aPoint;

    GetCursorPos(lpPoint);
    HMONITOR hMonitor = MonitorFromPoint(aPoint, MONITOR_DEFAULTTONEAREST);

    GetMonitorInfo(hMonitor, &target);

    screenResX = target.rcMonitor.right;
    screenResY = target.rcMonitor.bottom;

    //Fenster erstellen
    hWnd = CreateWindowEx( //Fensterstil
        WS_EX_LAYERED| WS_EX_TOOLWINDOW,
        //Name Fensterklasse
        lpszAppName,
        //Fenstertitel
        lpszTitle,
        //Art des Fensters
        WS_POPUP,
        //x, y
        screenResX - WND_WIDTH, screenResY - WND_HEIGHT + 10,
        //Fensterbreite
        WND_WIDTH,
        //Fensterhöhe
        WND_HEIGHT,
        //Fenster-Handle von übergeordnetem Fenster
        NULL,
        //Fenster Menü
        NULL,
        //Instanz der Anwendung aus WinMain()
        hInstance,
        //für MDI-Anwendungen
        NULL);

    if (hWnd == NULL)
        return 0;

    processCommandLineArgs();
    SetLayeredWindowAttributes(hWnd, RGB(0,0,0), (BYTE)0, LWA_COLORKEY);
    
    //Anzeigestatus
    ShowWindow(hWnd, iCmdShow);

    //Fenster zeichnen
    UpdateWindow(hWnd);

    //Nachrichten aus der Warteschlange abfragen
    while (GetMessage(&msg, NULL, 0, 0))
    {
        drawStatus(getStatus(), hWnd);

        //für virt. Tastaturcode
        TranslateMessage(&msg);
        //Nachrichten an die Prozedur senden
        DispatchMessage(&msg);
    }
    return msg.wParam;
}

//Prozedur für das Hauptfenster
LRESULT CALLBACK WndProc(HWND hWnd, UINT umsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT hit; //Zwischenspeicher fuer manuelles Hit-Handling
    RECT rect;

    //Nachrichten auswerten
    switch (umsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        cleanUp();

        return 0;
    case WM_CREATE:
        break;
    case WM_ERASEBKGND:    
        GetClientRect(hWnd, &rect);
        FillRect((HDC)wParam, &rect, CreateSolidBrush(RGB(0, 0, 0)));
        break;

    case WM_NCHITTEST:
        hit = DefWindowProc(hWnd, umsg, wParam, lParam);
        if (hit == HTCLIENT)
            hit = HTCAPTION;
        return hit;
    }

    //falls Nachricht nicht behandelt wurde von
    //Windows bearbeiten lassen
    return DefWindowProc(hWnd, umsg, wParam, lParam);
}
