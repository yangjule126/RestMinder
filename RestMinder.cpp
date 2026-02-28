#include <windows.h>
#include <shellapi.h>
#include <string>

#pragma comment(lib, "user32.lib")

#define WM_TRAYICON (WM_USER + 1)

#define ID_TRAY_STATUS   1001
#define ID_TRAY_SETTINGS 1002
#define ID_TRAY_ABOUT    1003
#define ID_TRAY_EXIT     1004

#define ID_BTN_SAVE      2001
#define ID_BTN_CLOSE     2002

#define APP_VERSION      L"v1.1.0"

NOTIFYICONDATA nid = { 0 };
UINT WM_TASKBARCREATED;   // ? 新增

std::wstring g_configPath;
bool g_running = true;

ULONGLONG WORK_LIMIT = 0;
ULONGLONG AWAY_RESET = 0;
ULONGLONG CHECK_INTERVAL = 5000;
ULONGLONG workingTime = 0;

HWND g_hSettings = NULL;
HWND hEditWork, hEditAway, hEditCheck;

// ================= 获取EXE目录 =================
std::wstring GetExeDirectory()
{
    wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    std::wstring full(path);
    return full.substr(0, full.find_last_of(L"\\/"));
}

// ================= 读取配置 =================
int ReadConfigInt(const wchar_t* key, int defaultValue)
{
    return GetPrivateProfileInt(
        L"Settings",
        key,
        defaultValue,
        g_configPath.c_str());
}

// ================= 重新加载配置 =================
void ReloadConfig()
{
    int workMin = ReadConfigInt(L"WorkLimitMinutes", 60);
    int awayMin = ReadConfigInt(L"AwayResetMinutes", 5);
    int checkSec = ReadConfigInt(L"CheckIntervalSeconds", 5);

    WORK_LIMIT = (ULONGLONG)workMin * 60 * 1000;
    AWAY_RESET = (ULONGLONG)awayMin * 60 * 1000;
    CHECK_INTERVAL = (ULONGLONG)checkSec * 1000;
}

// ================= 获取系统空闲时间 =================
ULONGLONG GetIdleTime()
{
    LASTINPUTINFO lii = { 0 };
    lii.cbSize = sizeof(LASTINPUTINFO);

    if (!GetLastInputInfo(&lii))
        return 0;

    return GetTickCount64() - lii.dwTime;
}

// ================= 提醒 =================
void ShowReminder()
{
    MessageBox(NULL,
        L"你已经连续使用电脑达到设定时间！\n\n请起来活动一下。",
        L"久坐提醒",
        MB_OK | MB_ICONWARNING | MB_TOPMOST);
}

// ================= 关于 =================
void ShowAbout()
{
    std::wstring msg =
        L"久坐提醒程序\n\n"
        L"作者：基础运维部\n"
        L"版本号：" APP_VERSION;

    MessageBox(NULL,
        msg.c_str(),
        L"关于",
        MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

// ================= 托盘 =================
void AddTrayIcon(HWND hWnd)
{
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_INFORMATION);
    wcscpy_s(nid.szTip, L"久坐提醒");

    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon()
{
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

// ================= 设置窗口过程 =================
LRESULT CALLBACK SettingsProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CreateWindow(L"STATIC", L"工作提醒(分钟):",
            WS_VISIBLE | WS_CHILD,
            20, 20, 120, 20,
            hWnd, NULL, NULL, NULL);

        hEditWork = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            150, 20, 100, 20,
            hWnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"离开重置(分钟):",
            WS_VISIBLE | WS_CHILD,
            20, 60, 120, 20,
            hWnd, NULL, NULL, NULL);

        hEditAway = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            150, 60, 100, 20,
            hWnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"检测间隔(秒):",
            WS_VISIBLE | WS_CHILD,
            20, 100, 120, 20,
            hWnd, NULL, NULL, NULL);

        hEditCheck = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            150, 100, 100, 20,
            hWnd, NULL, NULL, NULL);

        CreateWindow(L"BUTTON", L"保存",
            WS_VISIBLE | WS_CHILD,
            50, 150, 80, 30,
            hWnd, (HMENU)ID_BTN_SAVE, NULL, NULL);

        CreateWindow(L"BUTTON", L"关闭",
            WS_VISIBLE | WS_CHILD,
            150, 150, 80, 30,
            hWnd, (HMENU)ID_BTN_CLOSE, NULL, NULL);

        wchar_t buf[32];

        swprintf_s(buf, L"%llu", WORK_LIMIT / 60000);
        SetWindowText(hEditWork, buf);

        swprintf_s(buf, L"%llu", AWAY_RESET / 60000);
        SetWindowText(hEditAway, buf);

        swprintf_s(buf, L"%llu", CHECK_INTERVAL / 1000);
        SetWindowText(hEditCheck, buf);

        break;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_BTN_SAVE:
        {
            wchar_t buf[32];

            GetWindowText(hEditWork, buf, 32);
            WritePrivateProfileString(L"Settings", L"WorkLimitMinutes", buf, g_configPath.c_str());

            GetWindowText(hEditAway, buf, 32);
            WritePrivateProfileString(L"Settings", L"AwayResetMinutes", buf, g_configPath.c_str());

            GetWindowText(hEditCheck, buf, 32);
            WritePrivateProfileString(L"Settings", L"CheckIntervalSeconds", buf, g_configPath.c_str());

            ReloadConfig();
            MessageBox(hWnd, L"保存成功", L"提示", MB_OK);
            break;
        }
        case ID_BTN_CLOSE:
            DestroyWindow(hWnd);
            break;
        }
        break;

    case WM_DESTROY:
        g_hSettings = NULL;
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ================= 主窗口过程 =================
LRESULT CALLBACK MainProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // ? 新增：Explorer 重启后重新添加托盘图标
    if (msg == WM_TASKBARCREATED)
    {
        AddTrayIcon(hWnd);
        return 0;
    }

    if (msg == WM_TRAYICON && lParam == WM_RBUTTONUP)
    {
        HMENU menu = CreatePopupMenu();
        AppendMenu(menu, MF_STRING, ID_TRAY_STATUS, L"查看状态");
        AppendMenu(menu, MF_STRING, ID_TRAY_SETTINGS, L"设置");
        AppendMenu(menu, MF_STRING, ID_TRAY_ABOUT, L"关于");
        AppendMenu(menu, MF_SEPARATOR, 0, NULL);
        AppendMenu(menu, MF_STRING, ID_TRAY_EXIT, L"退出");

        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(hWnd);
        TrackPopupMenu(menu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
        DestroyMenu(menu);
    }
    else if (msg == WM_COMMAND)
    {
        switch (LOWORD(wParam))
        {
        case ID_TRAY_STATUS:
        {
            wchar_t buffer[128];
            swprintf_s(buffer, L"当前工作时间: %llu 分钟", workingTime / 60000);
            MessageBox(NULL, buffer, L"状态", MB_OK);
            break;
        }
        case ID_TRAY_SETTINGS:
        {
            if (g_hSettings == NULL)
            {
                g_hSettings = CreateWindow(
                    L"SettingsWindow",
                    L"设置",
                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                    600, 300, 300, 250,
                    NULL, NULL,
                    GetModuleHandle(NULL),
                    NULL);

                ShowWindow(g_hSettings, SW_SHOW);
            }
            else
            {
                ShowWindow(g_hSettings, SW_SHOW);
                SetForegroundWindow(g_hSettings);
            }
            break;
        }
        case ID_TRAY_ABOUT:
            ShowAbout();
            break;

        case ID_TRAY_EXIT:
            g_running = false;
            DestroyWindow(hWnd);
            break;
        }
    }
    else if (msg == WM_DESTROY)
    {
        RemoveTrayIcon();
        PostQuitMessage(0);
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ================= WinMain =================
int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE,
    LPSTR,
    int)
{
    g_configPath = GetExeDirectory() + L"\\config.ini";
    ReloadConfig();

    // ? 新增：注册TaskbarCreated消息
    WM_TASKBARCREATED = RegisterWindowMessage(L"TaskbarCreated");

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = MainProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MainWindow";
    RegisterClass(&wc);

    WNDCLASS wc2 = { 0 };
    wc2.lpfnWndProc = SettingsProc;
    wc2.hInstance = hInstance;
    wc2.lpszClassName = L"SettingsWindow";
    RegisterClass(&wc2);

    HWND hWnd = CreateWindow(L"MainWindow", L"", WS_OVERLAPPEDWINDOW,
        0, 0, 0, 0,
        NULL, NULL, hInstance, NULL);

    AddTrayIcon(hWnd);

    ULONGLONG lastTick = GetTickCount64();
    MSG msg;

    while (g_running)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        ULONGLONG now = GetTickCount64();

        if (now - lastTick >= CHECK_INTERVAL)
        {
            ULONGLONG idle = GetIdleTime();

            if (idle < AWAY_RESET)
                workingTime += CHECK_INTERVAL;
            else
                workingTime = 0;

            if (workingTime >= WORK_LIMIT)
            {
                ShowReminder();
                workingTime = 0;
            }

            lastTick = now;
        }

        Sleep(100);
    }

    return 0;
}
