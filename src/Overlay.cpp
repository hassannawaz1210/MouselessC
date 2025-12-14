#include <dwmapi.h>
#include <iostream>
#include <algorithm>
#include "Overlay.h"
#include "util.cpp"

// Static member initialization
const std::vector<std::string> Overlay::KEYBOARD_ROWS = {
    "QWERTYUIOP",
    "ASDFGHJKL;",
    "ZXCVBNM,./"};

int Overlay::SQUARE_SIZE = 0;
const int Overlay::FONT_SIZE = 13;
const UINT Overlay::TOGGLE_KEY = VK_OEM_3; // Backquote
const UINT Overlay::TOGGLE_MODIFIER = MOD_ALT;

std::map<char, POINT> Overlay::keyboardPositions;
std::map<std::string, std::pair<double, double>> Overlay::pairPositions;
std::string Overlay::selectedPair;
char Overlay::lastKeyPressed = '\0';
bool Overlay::isShowingKeyboard = false;
bool Overlay::isVisible = false;
HWND Overlay::hwnd = NULL;
HFONT Overlay::gridFont = NULL;
HFONT Overlay::layoutFont = NULL;
HHOOK Overlay::keyboardHook = NULL;
UINT_PTR Overlay::clickTimer = 0;
POINT Overlay::pendingClickPos = {0, 0};
int Overlay::clickCount = 0;
u_int Overlay::SCREEN_WIDTH = 0;
u_int Overlay::SCREEN_HEIGHT = 0;

bool Overlay::Initialize()
{
    // Register window class
    WNDCLASSEXW wc = {
        sizeof(WNDCLASSEXW),                 // cbSize
        CS_HREDRAW | CS_VREDRAW,             // style
        WndProc,                             // lpfnWndProc
        0,                                   // cbClsExtra
        0,                                   // cbWndExtra
        GetModuleHandle(NULL),               // hInstance
        LoadIcon(NULL, IDI_APPLICATION),     // hIcon
        LoadCursor(NULL, IDC_ARROW),         // hCursor
        (HBRUSH)GetStockObject(BLACK_BRUSH), // hbrBackground
        NULL,                                // lpszMenuName
        L"MouselessOverlay",                 // lpszClassName
        LoadIcon(NULL, IDI_APPLICATION)      // hIconSm
    };

    if (!RegisterClassExW(&wc))
    {
        return false;
    }

    if (!CalculateCellSize())
    {
        return 1;
    }

    hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        L"MouselessOverlay",
        L"Mouseless",
        WS_POPUP,
        0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!hwnd)
    {
        std::cout << "Failed to create window" << std::endl;
        return false;
    }

    // Modified transparency setup
    SetLayeredWindowAttributes(hwnd, 0, 200, LWA_ALPHA);

    // Create fonts
    gridFont = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Arial");

    layoutFont = CreateFontW(FONT_SIZE, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Arial");

    // Install keyboard hook
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc,
                                    GetModuleHandle(NULL), 0);

    //ShowWindow(hwnd, SW_SHOW);
    //UpdateWindow(hwnd);
    //InvalidateRect(hwnd, NULL, TRUE);
    
    std::cout << "Overlay initialized successfully." << std::endl;
    //Hide OVerlay
    
    return true;
}

bool Overlay::CalculateCellSize()
{
    SCREEN_WIDTH = GetSystemMetrics(SM_CXSCREEN);
    if (SCREEN_WIDTH <= 0)
	{
		std::cout << "Could not get the screen width\b" << std::endl;
		return 1;
	}
    SCREEN_HEIGHT = GetSystemMetrics(SM_CYSCREEN);
    if (SCREEN_HEIGHT <= 0)
	{
		std::cout << "Could not get the screen height\b" << std::endl;
		return 1;
	}

    std::vector<int> divisors_x = find_divisors(SCREEN_WIDTH);
    std::vector<int> divisors_y = find_divisors(SCREEN_HEIGHT);
    if (divisors_x.empty() || divisors_y.empty())
    {
        return false;
    }
    std::vector<int> common_divs = common_divisors(divisors_x, divisors_y);
    sort(common_divs.begin(), common_divs.end());
    SQUARE_SIZE =  common_divs.back();
    return true; 
}

void Overlay::Shutdown()
{
    if (keyboardHook)
    {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = NULL;
    }

    if (hwnd)
    {
        DestroyWindow(hwnd);
        hwnd = NULL;
    }

    if (gridFont)
    {
        DeleteObject(gridFont);
        gridFont = NULL;
    }

    if (layoutFont)
    {
        DeleteObject(layoutFont);
        layoutFont = NULL;
    }
}

LRESULT CALLBACK Overlay::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Draw(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_TIMER:
    {
        if (wParam == clickTimer)
        {
            KillTimer(hwnd, clickTimer);
            clickTimer = 0;

            // Log total clicks before hiding
            // Perform the click after delay
            for (int i = 0; i < clickCount; i++)
            {
                SetCursorPos(pendingClickPos.x, pendingClickPos.y);
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            }
            std::cout << "Total clicks performed: " << clickCount << std::endl;
            clickCount = 0; // Reset counter

            // Now hide overlay and cleanup

            // lastKeyPressed = '\0';
            ToggleVisibility();
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

static bool isShiftPressed = false;
LRESULT CALLBACK Overlay::KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;
        bool isAltPressed = (p->flags & LLKHF_ALTDOWN) != 0;
        if (p->vkCode == VK_LSHIFT || p->vkCode == VK_RSHIFT)
        {
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
                isShiftPressed = true;
            else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                isShiftPressed = false;
        }
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            if (p->vkCode == TOGGLE_KEY && isAltPressed)
            {
                ToggleVisibility();
                return 1;
            }

            if (isVisible)
            {
                char key = MapVirtualKey(p->vkCode, MAPVK_VK_TO_CHAR);
                ProcessKeyPress(key, isAltPressed, isShiftPressed);
                return 1;
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
void Overlay::Draw(HDC hdc)
{
    if (!CalculateCellSize()) return; 
    int cols = SCREEN_WIDTH / (SQUARE_SIZE);
    int rows = SCREEN_HEIGHT / (SQUARE_SIZE);
    for (int row = 0; row < rows; row++)
    {
        for (int col = 0; col < cols; col += 2)
        {
            char pairKey1 = (char)('A' + (col / 2) % 26);
            char pairKey2 = (char)('A' + row % 26);

            // current iteration coordinates
            int x = col * SQUARE_SIZE;
            int y = row * SQUARE_SIZE;

            // Draw rectangle background
            HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
            RECT rect = {x, y, x + ((SQUARE_SIZE) * 2), y + SQUARE_SIZE};
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);

            // Draw border
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(100, 0, 0));
            HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

            // Select a null brush so Rectangle won't fill it again
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, x, y, x + ((SQUARE_SIZE) * 2), y + SQUARE_SIZE); // create border/outline
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(hPen);
            // Draw character grids
            DrawCharacterGrid(hdc, pairKey1, x, y);
            DrawCharacterGrid(hdc, pairKey2, x + SQUARE_SIZE, y);

            // Store pair position
            std::string pair;
            pair += pairKey1;
            pair += pairKey2;
            pairPositions[pair] = std::make_pair(x, y);
        }
    }

    if (isShowingKeyboard && !selectedPair.empty())
    {
        auto pos = pairPositions[selectedPair];
        DrawKeyboardLayout(hdc, (int)pos.first, (int)pos.second);
    }
}

void Overlay::DrawKeyboardLayout(HDC hdc, int startX, int startY)
{
    int rectWidth = ((SQUARE_SIZE) * 2);
    int rectHeight = SQUARE_SIZE;
    keyboardPositions.clear();

    int maxKeysInRow = KEYBOARD_ROWS[0].length();
    int keyWidth = rectWidth / maxKeysInRow;
    int keyHeight = rectHeight / KEYBOARD_ROWS.size();

    int totalRows = static_cast<int>(KEYBOARD_ROWS.size());
    for (int row = 0; row < totalRows; row++)
    {
        std::string keys = KEYBOARD_ROWS[row];
        int rowOffset = (maxKeysInRow - keys.length()) * keyWidth / 2;
        int totalCols = static_cast<int>(keys.length());

        for (int col = 0; col < totalCols; col++)
        {
            char key = keys[col];
            int x = startX + rowOffset + (col * keyWidth);
            int y = startY + (row * keyHeight);

            // TODO: remove repetitive portion
            {
                // Draw key background
                HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
                RECT rect = {x, y, x + keyWidth, y + keyHeight};
                FillRect(hdc, &rect, hBrush);
                DeleteObject(hBrush);
                
                // Draw key outline
                HPEN hPen = CreatePen(PS_SOLID, 1, RGB(100, 0, 0));
                HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

                // Select a null brush so Rectangle won't fill it again
                HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, x, y, x + keyWidth, y + keyHeight); // create border/outline
                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(hPen);


                // Draw key character
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkMode(hdc, TRANSPARENT);
                SelectObject(hdc, layoutFont);
                int stringX = x + keyWidth / 2 - FONT_SIZE / 3;
                int stringY = y + keyHeight / 2 - FONT_SIZE / 3;
                wchar_t wch = static_cast<wchar_t>(key);
                TextOutW(hdc, stringX, stringY, &wch, 1);
            }

            // Store key position
            keyboardPositions[key] = {x + keyWidth / 2, y + keyHeight / 2};
        }
    }
}

void Overlay::DrawCharacterGrid(HDC hdc, char c, int x, int y)
{
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, RGB(0, 0, 0));
    SelectObject(hdc, gridFont);
    int stringX = x + (SQUARE_SIZE/2) - 8;// 8 -> font offset
    int stringY = y + (SQUARE_SIZE/2) - 8;
    wchar_t wch = static_cast<wchar_t>(c);
    TextOutW(hdc, stringX, stringY, &wch, 1);
}

void Overlay::ToggleVisibility()
{
    isVisible = !isVisible;
    isShowingKeyboard = false;
    selectedPair.clear();
    lastKeyPressed = '\0';
    pendingClickPos = {0, 0};
    if (isVisible)
    {
        ShowWindow(hwnd, SW_SHOW);
        SetWindowLong(hwnd, GWL_EXSTYLE,
                      GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
    }
    else
    {
        ShowWindow(hwnd, SW_HIDE);
    }

    // Release modifier keys
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
}

void Overlay::PerformDragRelease(POINT releasePos)
{
    POINT startPos;
    GetCursorPos(&startPos);

    // Move in small increments to simulate real dragging
    int stepCount = 10; // Increase for smoother movement
    for (int i = 1; i <= stepCount; i++)
    {
        int x = startPos.x + (i * (releasePos.x - startPos.x)) / stepCount;
        int y = startPos.y + (i * (releasePos.y - startPos.y)) / stepCount;
        SetCursorPos(x, y);
        Sleep(5); // Small pause (remove or adjust as needed)
    }

    // Now release the button
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}

void Overlay::ProcessKeyPress(char key, bool isAltPressed, bool isShiftPressed)
{
    if (isShowingKeyboard)
    {
        if (keyboardPositions.find(key) != keyboardPositions.end())
        {
            // Perform immediate click if alt key is pressed
            if (isAltPressed)
            {
                SetCursorPos(keyboardPositions[key].x, keyboardPositions[key].y);
                mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
                mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
                ToggleVisibility();
                return;
            }
            if (isShiftPressed)
            {
                if (pendingClickPos.x != 0 && pendingClickPos.y != 0)
                {
                    PerformDragRelease(keyboardPositions[key]);
                    ToggleVisibility();
                }
                else
                {
                    pendingClickPos = keyboardPositions[key];
                    SetCursorPos(pendingClickPos.x, pendingClickPos.y);
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    isShowingKeyboard = false;
                    InvalidateRect(hwnd, NULL, TRUE); // Force redraw
                    UpdateWindow(hwnd);               // Force redraw
                }
                return;
            }

            // Check if there is a pending click
            if (pendingClickPos.x != 0 && pendingClickPos.y != 0)
            {
                // Check if position has changed
                if (pendingClickPos.x != keyboardPositions[key].x || pendingClickPos.y != keyboardPositions[key].y)
                {
                    // Reset click count
                    clickCount = 0;
                }
            }
            pendingClickPos = keyboardPositions[key];

            clickCount++; // Increment click counter

            // Start/reset timer if not already running
            if (clickTimer)
            {
                KillTimer(hwnd, clickTimer);
            }
            clickTimer = SetTimer(hwnd, 1, CLICK_DELAY_THRESHOLD, NULL);

            // Don't hide overlay yet - wait for timer
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateWindow(hwnd);
            return;
        }
    }

    if (lastKeyPressed != '\0')
    {
        std::string pairKey = std::string(1, lastKeyPressed) + key;
        if (pairPositions.find(pairKey) != pairPositions.end())
        {
            selectedPair = pairKey;
            isShowingKeyboard = true;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        lastKeyPressed = '\0';
    }
    else
    {
        lastKeyPressed = key;
    }
}