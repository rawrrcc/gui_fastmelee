// fastmelee_gui.cpp
// C++17, FastMelee GUI для L4D2 з фоном і вибором методу вводу

#include <windows.h>
#include <thread>
#include <atomic>
#include <string>
#include <chrono>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

std::atomic<bool> running(false);
std::atomic<int> slot_mode(1); // 1, 4, 5
std::atomic<int> delay_before_mouse1(450);
std::atomic<int> mouse1_duration(500);
std::atomic<int> delay_before_melee(60);

enum InputMethod { SENDINPUT, KEYBD_EVENT, POSTMESSAGE };
std::atomic<InputMethod> input_method(SENDINPUT);

HWND hSlotCombo, hDelay1Edit, hDurationEdit, hDelay2Edit, hMethodCombo;
HBITMAP hBackground = nullptr;

// Відправка клавіші з вибраним методом
void send_key(WORD vk, WORD sc) {
    switch (input_method.load()) {
    case SENDINPUT: {
        INPUT input[2] = {};
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = 0;
        input[0].ki.wScan = sc;
        input[0].ki.dwFlags = KEYEVENTF_SCANCODE;
        input[1] = input[0];
        input[1].ki.dwFlags |= KEYEVENTF_KEYUP;
        SendInput(2, input, sizeof(INPUT));
        break;
    }
    case KEYBD_EVENT:
        keybd_event(static_cast<BYTE>(vk), 0, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        keybd_event(static_cast<BYTE>(vk), 0, KEYEVENTF_KEYUP, 0);
        break;
    case POSTMESSAGE:
        HWND fg = GetForegroundWindow();
        PostMessage(fg, WM_KEYDOWN, static_cast<WPARAM>(vk), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        PostMessage(fg, WM_KEYUP, static_cast<WPARAM>(vk), 0);
        break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
}

// Клік Mouse1
void send_mouse1() {
    INPUT input[2] = {};
    input[0].type = INPUT_MOUSE;
    input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    input[1].type = INPUT_MOUSE;
    input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, input, sizeof(INPUT));
    std::this_thread::sleep_for(std::chrono::milliseconds(mouse1_duration.load()));
}

// Основний цикл
void fastmelee_loop() {
    while (running) {
        send_key('2', 0x03); // Slot 2
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_before_mouse1.load()));
        send_mouse1();
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_before_melee.load()));
        switch (slot_mode.load()) {
        case 1: send_key('1', 0x02); break;
        case 4: send_key('4', 0x05); break;
        case 5: send_key('5', 0x06); break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }
}

// Потік для Mouse5
DWORD WINAPI hotkey_thread(LPVOID) {
    bool prev_mouse5 = false;
    while (true) {
        SHORT mouse5 = GetAsyncKeyState(VK_XBUTTON2);
        bool pressed = (mouse5 & 0x8000);
        if (pressed && !prev_mouse5) {
            running = true;
            std::thread(fastmelee_loop).detach();
        }
        if (!pressed && prev_mouse5) {
            running = false;
        }
        prev_mouse5 = pressed;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return 0;
}

// Зчитування GUI
void read_gui_values() {
    wchar_t buf[16];
    GetWindowTextW(hDelay1Edit, buf, 16);
    delay_before_mouse1 = _wtoi(buf);
    GetWindowTextW(hDurationEdit, buf, 16);
    mouse1_duration = _wtoi(buf);
    GetWindowTextW(hDelay2Edit, buf, 16);
    delay_before_melee = _wtoi(buf);
    int sel = SendMessage(hSlotCombo, CB_GETCURSEL, 0, 0);
    slot_mode = (sel == 0) ? 1 : (sel == 1) ? 4 : 5;
    int method = SendMessage(hMethodCombo, CB_GETCURSEL, 0, 0);
    input_method = (method == 0) ? SENDINPUT : (method == 1) ? KEYBD_EVENT : POSTMESSAGE;
}

// Малювання фону
void draw_background(HWND hwnd, HDC hdc) {
    if (!hBackground) return;
    HDC memDC = CreateCompatibleDC(hdc);
    SelectObject(memDC, hBackground);
    BITMAP bmp;
    GetObject(hBackground, sizeof(BITMAP), &bmp);
    RECT rc;
    GetClientRect(hwnd, &rc);
    StretchBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY);
    DeleteDC(memDC);
}

// Завантаження JPEG
HBITMAP LoadJPEG(const wchar_t* path) {
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    Bitmap* bmp = Bitmap::FromFile(path, FALSE);
    HBITMAP hBmp;
    bmp->GetHBITMAP(Color::White, &hBmp);
    delete bmp;
    GdiplusShutdown(gdiplusToken);
    return hBmp;
}

// Callback
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        draw_background(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
        read_gui_values();
        return 0;
    case WM_DESTROY:
        running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Старт
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();
    hBackground = LoadJPEG(L"background.jpeg"); // ⚠️ Заміни на свій шлях до картинки

    const wchar_t CLASS_NAME[] = L"FastMeleeWindow";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"FastMelee GUI",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 320,
        nullptr, nullptr, hInstance, nullptr);

    CreateWindowW(L"STATIC", L"Slot:", WS_CHILD | WS_VISIBLE, 20, 20, 80, 20, hwnd, nullptr, hInstance, nullptr);
    hSlotCombo = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        100, 20, 100, 100, hwnd, nullptr, hInstance, nullptr);
    SendMessage(hSlotCombo, CB_ADDSTRING, 0, (LPARAM)L"Slot 1");
    SendMessage(hSlotCombo, CB_ADDSTRING, 0, (LPARAM)L"Slot 4");
    SendMessage(hSlotCombo, CB_ADDSTRING, 0, (LPARAM)L"Slot 5");
    SendMessage(hSlotCombo, CB_SETCURSEL, 0, 0);

    CreateWindowW(L"STATIC", L"Delay before Mouse1:", WS_CHILD | WS_VISIBLE, 20, 60, 160, 20, hwnd, nullptr, hInstance, nullptr);
    hDelay1Edit = CreateWindowW(L"EDIT", L"450", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        190, 60, 60, 20, hwnd, nullptr, hInstance, nullptr);
    hDelay1Edit = CreateWindowW(L"EDIT", L"450", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        190, 60, 60, 20, hwnd, nullptr, hInstance, nullptr);

    hDurationEdit = CreateWindowW(L"EDIT", L"500", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        190, 90, 60, 20, hwnd, nullptr, hInstance, nullptr);

    CreateWindowW(L"STATIC", L"Delay before melee:", WS_CHILD | WS_VISIBLE, 20, 120, 160, 20, hwnd, nullptr, hInstance, nullptr);
    hDelay2Edit = CreateWindowW(L"EDIT", L"60", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        190, 120, 60, 20, hwnd, nullptr, hInstance, nullptr);

    CreateWindowW(L"STATIC", L"Input method:", WS_CHILD | WS_VISIBLE, 20, 150, 160, 20, hwnd, nullptr, hInstance, nullptr);
    hMethodCombo = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        190, 150, 120, 100, hwnd, nullptr, hInstance, nullptr);
    SendMessage(hMethodCombo, CB_ADDSTRING, 0, (LPARAM)L"SendInput");
    SendMessage(hMethodCombo, CB_ADDSTRING, 0, (LPARAM)L"keybd_event");
    SendMessage(hMethodCombo, CB_ADDSTRING, 0, (LPARAM)L"PostMessage");
    SendMessage(hMethodCombo, CB_SETCURSEL, 0, 0);

    CreateWindowW(L"STATIC", L"Hold Mouse5 to activate", WS_CHILD | WS_VISIBLE,
        20, 190, 200, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"STATIC", L"⚠️ Run as administrator if not working", WS_CHILD | WS_VISIBLE,
        20, 220, 300, 20, hwnd, nullptr, hInstance, nullptr);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Запускаємо потік моніторингу Mouse5
    CreateThread(nullptr, 0, hotkey_thread, nullptr, 0, nullptr);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}