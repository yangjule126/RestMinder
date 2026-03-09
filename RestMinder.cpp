#include <windows.h>
#include <shellapi.h>
#include <string>

#pragma comment(lib, "user32.lib")
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#define WM_TRAYICON (WM_USER + 1)

#define ID_TRAY_STATUS   1001
#define ID_TRAY_SETTINGS 1002
#define ID_TRAY_ABOUT    1003
#define ID_TRAY_EXIT     1004

#define ID_BTN_SAVE      2001
#define ID_BTN_CLOSE     2002


#define APP_VERSION      L"v1.2.0"

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
HWND hCheckAutoStart;
HWND g_hGithub = NULL;
HFONT g_hLinkFont = NULL;

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

// ================= 获取程序路径 =================
std::wstring GetExePath()
{
    wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    return path;
}

// ================= 是否开机启动 =================
bool IsAutoStartEnabled()
{
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        &hKey) != ERROR_SUCCESS)
        return false;

    wchar_t value[MAX_PATH];
    DWORD size = sizeof(value);

    bool enabled = (RegQueryValueEx(
        hKey,
        L"SedentaryReminder",
        NULL,
        NULL,
        (LPBYTE)value,
        &size) == ERROR_SUCCESS);

    RegCloseKey(hKey);
    return enabled;
}

// ================= 设置开机启动 =================
void SetAutoStart(bool enable)
{
    HKEY hKey;

    if (RegOpenKey(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        &hKey) != ERROR_SUCCESS)
        return;

    if (enable)
    {
        std::wstring path = GetExePath();

        RegSetValueEx(
            hKey,
            L"SedentaryReminder",
            0,
            REG_SZ,
            (BYTE*)path.c_str(),
            (DWORD)((path.size() + 1) * sizeof(wchar_t)));
    }
    else
    {
        RegDeleteValue(hKey, L"SedentaryReminder");
    }

    RegCloseKey(hKey);
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
    HWND hWnd = CreateWindowEx(
        WS_EX_TOPMOST,
        L"AboutWindow",
        L"关于",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        600, 300, 260, 200,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
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
        CreateWindow(L"STATIC", L"连续使用电脑多久后提醒(分钟):",
            WS_VISIBLE | WS_CHILD,
            20, 20, 220, 20,
            hWnd, NULL, NULL, NULL);

        hEditWork = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            250, 20, 100, 20,
            hWnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"未使用电脑多久重置计数(分钟):",
            WS_VISIBLE | WS_CHILD,
            20, 60, 220, 20,
            hWnd, NULL, NULL, NULL);

        hEditAway = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            250, 60, 100, 20,
            hWnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"自动检测间隔(秒):",
            WS_VISIBLE | WS_CHILD,
            20, 100, 150, 20,
            hWnd, NULL, NULL, NULL);

        hEditCheck = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            180, 100, 100, 20,
            hWnd, NULL, NULL, NULL);

        hCheckAutoStart = CreateWindow(
            L"BUTTON",
            L"开机自启",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            20, 140, 120, 20,
            hWnd,
            NULL,
            NULL,
            NULL);

        if (IsAutoStartEnabled())
            SendMessage(hCheckAutoStart, BM_SETCHECK, BST_CHECKED, 0);

        CreateWindow(L"BUTTON", L"保存",
            WS_VISIBLE | WS_CHILD,
            50, 180, 80, 30,
            hWnd, (HMENU)ID_BTN_SAVE, NULL, NULL);

        CreateWindow(L"BUTTON", L"关闭",
            WS_VISIBLE | WS_CHILD,
            150, 180, 80, 30,
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

            bool autoStart =
                SendMessage(hCheckAutoStart, BM_GETCHECK, 0, 0) == BST_CHECKED;

            SetAutoStart(autoStart);

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

LRESULT CALLBACK AboutProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        HFONT hTitleFont = CreateFont(
            26, 0, 0, 0, FW_BOLD,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS,
            L"Microsoft YaHei");

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        HWND hTitle = CreateWindow(
            L"STATIC",
            L"久坐提醒",
            WS_VISIBLE | WS_CHILD,
            20, 20, 200, 30,
            hWnd,
            NULL, NULL, NULL);

        SendMessage(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);

        std::wstring version = L"版本 ";
        version += APP_VERSION;

        HWND hVersion = CreateWindow(
            L"STATIC",
            version.c_str(),
            WS_VISIBLE | WS_CHILD,
            20, 70, 200, 20,
            hWnd,
            NULL, NULL, NULL);

        SendMessage(hVersion, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hAuthor = CreateWindow(
            L"STATIC",
            L"作者：基础运维部",
            WS_VISIBLE | WS_CHILD,
            20, 95, 200, 20,
            hWnd,
            NULL, NULL, NULL);

        SendMessage(hAuthor, WM_SETFONT, (WPARAM)hFont, TRUE);

        // GitHub 链接
        g_hGithub = CreateWindow(
            L"STATIC",
            L"GitHub 项目",
            WS_VISIBLE | WS_CHILD | SS_NOTIFY,
            20, 130, 120, 20,
            hWnd,
            (HMENU)3001,
            NULL,
            NULL);

        // 设置下划线字体
        LOGFONT lf;
        GetObject(hFont, sizeof(LOGFONT), &lf);
        lf.lfUnderline = TRUE;

        g_hLinkFont = CreateFontIndirect(&lf);

        SendMessage(g_hGithub, WM_SETFONT, (WPARAM)g_hLinkFont, TRUE);
    }
    break;

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == 3001 && HIWORD(wParam) == STN_CLICKED)
        {
            ShellExecute(
                NULL,
                L"open",
                L"https://github.com/yangjule126/RestMinder",
                NULL,
                NULL,
                SW_SHOWNORMAL);
        }
    }
    break;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;

        if (hCtrl == g_hGithub)
        {
            SetTextColor(hdc, RGB(0, 102, 204));
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
    }
    break;
    case WM_SETCURSOR:
    {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hWnd, &pt);

        RECT rc;
        GetWindowRect(g_hGithub, &rc);
        MapWindowPoints(HWND_DESKTOP, hWnd, (LPPOINT)&rc, 2);

        if (PtInRect(&rc, pt))
        {
            SetCursor(LoadCursor(NULL, IDC_HAND));  // 手型
            return TRUE;
        }
        else
        {
            SetCursor(LoadCursor(NULL, IDC_ARROW)); // 恢复箭头
            return TRUE;
        }
    }
    break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
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
            swprintf_s(buffer, L"已连续使用电脑时间: %llu 分钟", workingTime / 60000);
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
                    600, 300, 390, 250,
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

    WNDCLASS wcAbout = { 0 };
    wcAbout.lpfnWndProc = AboutProc;
    wcAbout.hInstance = hInstance;
    wcAbout.lpszClassName = L"AboutWindow";
    RegisterClass(&wcAbout);

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
