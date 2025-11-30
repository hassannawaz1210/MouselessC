#pragma once
#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include <dwmapi.h>

class Overlay
{
public:
    static bool Initialize();
    static void Shutdown();
    static void ToggleVisibility();
    static void ProcessKeyPress(char key, bool isAltPressed, bool isShiftPressed);
    static void PerformDragRelease(POINT releasePos);

private:
    static const std::vector<std::string> KEYBOARD_ROWS;
    static int SQUARE_SIZE;
    static const int FONT_SIZE;
    static const UINT TOGGLE_KEY;
    static const UINT TOGGLE_MODIFIER;
    static UINT_PTR clickTimer;
    static POINT pendingClickPos;
    static const UINT CLICK_DELAY_THRESHOLD = 200; // 1 second in milliseconds
    static int clickCount; // Track number of clicks

    static u_int SCREEN_WIDTH;
    static u_int SCREEN_HEIGHT;

    // State variables
    static std::map<char, POINT> keyboardPositions;
    static std::map<std::string, std::pair<double, double>> pairPositions;
    static std::string selectedPair;
    static char lastKeyPressed;
    static bool isShowingKeyboard;
    static bool isVisible;
    static HWND hwnd;
    static HFONT gridFont;
    static HFONT layoutFont;
    static HHOOK keyboardHook;

    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void Draw(HDC hdc);
    static void DrawKeyboardLayout(HDC hdc, int centerX, int centerY);
    static void DrawCharacterGrid(HDC hdc, char c, int x, int y);
};

