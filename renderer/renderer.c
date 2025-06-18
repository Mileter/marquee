/* renderer.c - Marquee Layout Renderer for Windows with TPF and PM support */
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define RENDERER
#include "../rc/resource.h"

#define MAX_SEGMENTS 10
#define MAX_LINES_PER_SEGMENT 50
#define MAX_COLORED_TEXTS_PER_LINE 20
#define MAX_TEXT_LENGTH 1000
#define MAX_NESTING_DEPTH 255

typedef struct {
    int linesPerScreen;
    int screenWidth;
    int screenHeight;
    int screenCount;
    int screenDelay;
    int centerDelay;  /* CD - delay for non-scrolling centered text */
    
    /* Optional flags from standard.mly */
    int timePerFrame;     /* TPF - millis per frame (default 50) */
    int pixelsPerFrame;   /* PM - pixel movement per frame (default 3) */
} MarqueeConfig;

typedef struct {
    wchar_t text[MAX_TEXT_LENGTH];
    COLORREF color;
} ColoredText;

typedef struct {
    ColoredText texts[MAX_COLORED_TEXTS_PER_LINE];
    int textCount;
} TextLine;

typedef struct {
    TextLine lines[MAX_LINES_PER_SEGMENT];
    int lineCount;
} TextSegment;

typedef struct {
    HWND hwnd;
    MarqueeConfig config;
    TextSegment segments[MAX_SEGMENTS];
    int segmentCount;
    int currentScreen;
    BOOL isRunning;
    int scrollPosition;
    DWORD lastUpdate;
    HFONT font;
    BOOL isCurrentScreenCentered;  /* Track if current screen should be centered */
    DWORD centerStartTime;         /* When centered display started */
} MarqueeRenderer;

MarqueeRenderer* g_renderer = NULL;

void InitRenderer(MarqueeRenderer* renderer, HWND hwnd) {
    renderer->hwnd = hwnd;
    renderer->config.linesPerScreen = 2;
    renderer->config.screenWidth = 600;
    renderer->config.screenHeight = 80;
    renderer->config.screenCount = 2;
    renderer->config.screenDelay = 500;
    renderer->config.centerDelay = 1500;
    
    /* Set default values for optional flags */
    renderer->config.timePerFrame = 50;    /* Default TPF: 50ms per frame */
    renderer->config.pixelsPerFrame = 3;   /* Default PM: 3 pixels per frame */
    
    renderer->segmentCount = 0;
    renderer->currentScreen = 0;
    renderer->isRunning = FALSE;
    renderer->scrollPosition = 0;
    renderer->lastUpdate = 0;
    renderer->isCurrentScreenCentered = FALSE;
    renderer->centerStartTime = 0;
    
    renderer->font = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
        L"MingLiU"
    );
    if (!renderer->font) {
        renderer->font = CreateFontW(
            -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
            L"Courier New"
        );
    }
}

void CleanupRenderer(MarqueeRenderer* renderer) {
    if (renderer->font) {
        DeleteObject(renderer->font);
    }
}

COLORREF ParseHexColor(const wchar_t* colorStr) {
    if (wcslen(colorStr) != 6) return RGB(255, 255, 255);
    
    wchar_t rStr[3] = {colorStr[0], colorStr[1], 0};
    wchar_t gStr[3] = {colorStr[2], colorStr[3], 0};
    wchar_t bStr[3] = {colorStr[4], colorStr[5], 0};
    
    int r = (int)wcstoul(rStr, NULL, 16);
    int g = (int)wcstoul(gStr, NULL, 16);
    int b = (int)wcstoul(bStr, NULL, 16);
    
    return RGB(r, g, b);
}

typedef struct {
    int position;     /* Position in the line where backtick opened */
    BOOL hasColor;    /* Whether this backtick has a color specification */
} BacktickState;

typedef struct {
    BacktickState stack[MAX_NESTING_DEPTH];
    int depth;
} BacktickStack;

void InitBacktickStack(BacktickStack* stack) {
    stack->depth = 0;
}

BOOL PushBacktick(BacktickStack* stack, int position, BOOL hasColor) {
    if (stack->depth >= MAX_NESTING_DEPTH) {
        return FALSE; /* Stack overflow */
    }
    stack->stack[stack->depth].position = position;
    stack->stack[stack->depth].hasColor = hasColor;
    stack->depth++;
    return TRUE;
}

BOOL PopBacktick(BacktickStack* stack, BacktickState* state) {
    if (stack->depth <= 0) {
        return FALSE; /* Stack underflow */
    }
    stack->depth--;
    if (state) {
        *state = stack->stack[stack->depth];
    }
    return TRUE;
}

BOOL IsStackEmpty(BacktickStack* stack) {
    return stack->depth == 0;
}

/* Enhanced parser for the renderer that handles nested colors */
void ParseColoredLine(const wchar_t* line, TextLine* textLine) {
    textLine->textCount = 0;
    wchar_t current[MAX_TEXT_LENGTH] = {0};
    int currentPos = 0;
    
    /* Color stack to handle nested colors */
    COLORREF colorStack[MAX_NESTING_DEPTH];
    int colorDepth = 0;
    COLORREF currentColor = RGB(255, 255, 255); /* Default white */
    colorStack[0] = currentColor;
    
    int len = (int)wcslen(line);
    for (int i = 0; i < len; i++) {
        if (line[i] == L'\\' && i + 1 < len) {
            /* Escaped character */
            current[currentPos++] = line[++i];
        } else if (line[i] == L'`') {
            /* Save current text if any */
            if (currentPos > 0) {
                current[currentPos] = 0;
                wcscpy(textLine->texts[textLine->textCount].text, current);
                textLine->texts[textLine->textCount].color = currentColor;
                textLine->textCount++;
                currentPos = 0;
            }
            
            /* Find the colon that separates parameters from text */
            int colonPos = -1;
            for (int j = i + 1; j < len; j++) {
                if (line[j] == L'\\' && j + 1 < len) {
                    j++; /* Skip escaped character */
                    continue;
                }
                if (line[j] == L':') {
                    colonPos = j;
                    break;
                } else if (line[j] == L'\'' || line[j] == L'`') {
                    break;
                }
            }
            
            if (colonPos != -1) {
                /* Extract color specification */
                int paramLen = colonPos - i - 1;
                if (paramLen == 6) {
                    wchar_t colorSpec[7] = {0};
                    wcsncpy(colorSpec, &line[i + 1], 6);
                    
                    BOOL validHex = TRUE;
                    for (int k = 0; k < 6; k++) {
                        wchar_t c = colorSpec[k];
                        if (!((c >= L'0' && c <= L'9') || 
                              (c >= L'A' && c <= L'F') || 
                              (c >= L'a' && c <= L'f'))) {
                            validHex = FALSE;
                            break;
                        }
                    }
                    
                    if (validHex) {
                        /* Push current color onto stack and set new color */
                        if (colorDepth < MAX_NESTING_DEPTH - 1) {
                            colorDepth++;
                            currentColor = ParseHexColor(colorSpec);
                            colorStack[colorDepth] = currentColor;
                        }
                    }
                }
                /* Skip to after the colon */
                i = colonPos;
            } else {
                /* No colon found - treat as regular text */
                current[currentPos++] = line[i];
            }
        } else if (line[i] == L'\'') {
            /* End of colored text - save current text and pop color */
            if (currentPos > 0) {
                current[currentPos] = 0;
                wcscpy(textLine->texts[textLine->textCount].text, current);
                textLine->texts[textLine->textCount].color = currentColor;
                textLine->textCount++;
                currentPos = 0;
            }
            
            /* Pop color from stack */
            if (colorDepth > 0) {
                colorDepth--;
                currentColor = colorStack[colorDepth];
            } else {
                currentColor = RGB(255, 255, 255); /* Reset to default white */
            }
        } else {
            current[currentPos++] = line[i];
        }
    }
    
    /* Add remaining text */
    if (currentPos > 0) {
        current[currentPos] = 0;
        wcscpy(textLine->texts[textLine->textCount].text, current);
        textLine->texts[textLine->textCount].color = currentColor;
        textLine->textCount++;
    }
}

BOOL LoadLayoutFile(MarqueeRenderer* renderer, const wchar_t* filename) {
    FILE* file = _wfopen(filename, L"r, ccs=UTF-8");
    if (!file) return FALSE;
    
    renderer->segmentCount = 0;
    wchar_t line[MAX_TEXT_LENGTH];
    BOOL inSegment = FALSE;
    TextSegment* currentSegment = NULL;
    
    while (fgetws(line, MAX_TEXT_LENGTH, file)) {
        /* Remove newline */
        int len = (int)wcslen(line);
        if (len > 0 && line[len-1] == L'\n') {
            line[len-1] = 0;
            len--;
        }
        if (len > 0 && line[len-1] == L'\r') {
            line[len-1] = 0;
            len--;
        }
        
        /* Skip comments and empty lines outside segments */
        if (len == 0 || line[0] == L'/') {
            if (inSegment && currentSegment && line[0] != L'/') {
                /* Empty lines in segments are preserved, but comments are skipped */
                currentSegment->lines[currentSegment->lineCount].textCount = 0;
                currentSegment->lineCount++;
            }
            continue;
        }
        
        /* Parse configuration */
        if (wcsncmp(line, L"LPS", 3) == 0) {
            renderer->config.linesPerScreen = _wtoi(&line[4]);
        } else if (wcsncmp(line, L"SW", 2) == 0) {
            renderer->config.screenWidth = _wtoi(&line[3]);
        } else if (wcsncmp(line, L"SH", 2) == 0) {
            renderer->config.screenHeight = _wtoi(&line[3]);
        } else if (wcsncmp(line, L"SC", 2) == 0) {
            renderer->config.screenCount = _wtoi(&line[3]);
        } else if (wcsncmp(line, L"SD", 2) == 0) {
            renderer->config.screenDelay = _wtoi(&line[3]);
        } else if (wcsncmp(line, L"CD", 2) == 0) {
            renderer->config.centerDelay = _wtoi(&line[3]);
        
        /* Parse optional flags */
        } else if (wcsncmp(line, L"TPF", 3) == 0) {
            int value = _wtoi(&line[4]);
            if (value > 0) {
                renderer->config.timePerFrame = value;
            }
        } else if (wcsncmp(line, L"PM", 2) == 0) {
            int value = _wtoi(&line[3]);
            if (value > 0) {
                renderer->config.pixelsPerFrame = value;
            }
        
        } else if (wcscmp(line, L"START") == 0) {
            inSegment = TRUE;
            currentSegment = &renderer->segments[renderer->segmentCount];
            currentSegment->lineCount = 0;
        } else if (wcscmp(line, L"END") == 0) {
            inSegment = FALSE;
            renderer->segmentCount++;
            currentSegment = NULL;
        } else if (inSegment && currentSegment) {
            ParseColoredLine(line, &currentSegment->lines[currentSegment->lineCount]);
            currentSegment->lineCount++;
        }
    }
    
    fclose(file);
    
    /* Calculate font size based on screen height and lines per screen */
    int fontSize = -(renderer->config.screenHeight / renderer->config.linesPerScreen);
    if (fontSize > -8) fontSize = -8; /* Minimum readable size */
    
    /* Delete old font and create new one with calculated size */
    if (renderer->font) DeleteObject(renderer->font);
    
    renderer->font = CreateFontW(
        fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
        L"MingLiU"
    );
    if (!renderer->font) {
        renderer->font = CreateFontW(
            fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
            L"Courier New"
        );
    }
    
    /* Resize window to match screen width and height from file */
    SetWindowPos(renderer->hwnd, NULL, 0, 0, 
                 renderer->config.screenWidth + 20, 
                 renderer->config.screenHeight + 80, 
                 SWP_NOMOVE | SWP_NOZORDER);
    
    return TRUE;
}

void StartMarquee(MarqueeRenderer* renderer) {
    renderer->isRunning = TRUE;
    renderer->scrollPosition = renderer->config.screenWidth;
    renderer->lastUpdate = GetTickCount();
    renderer->isCurrentScreenCentered = FALSE;
    renderer->centerStartTime = 0;
    
    /* Use TPF (timePerFrame) for timer interval instead of hardcoded 50ms */
    SetTimer(renderer->hwnd, 1, renderer->config.timePerFrame, NULL);
}

void StopMarquee(MarqueeRenderer* renderer) {
    renderer->isRunning = FALSE;
    KillTimer(renderer->hwnd, 1);
}

void ResetMarquee(MarqueeRenderer* renderer) {
    StopMarquee(renderer);
    renderer->currentScreen = 0;
    renderer->scrollPosition = renderer->config.screenWidth;
    renderer->isCurrentScreenCentered = FALSE;
    renderer->centerStartTime = 0;
}

int GetTextWidth(MarqueeRenderer* renderer) {
    if (renderer->segmentCount == 0 || renderer->currentScreen >= renderer->segmentCount) {
        return 0;
    }
    
    HDC hdc = GetDC(renderer->hwnd);
    SelectObject(hdc, renderer->font);
    
    int maxWidth = 0;
    TextSegment* segment = &renderer->segments[renderer->currentScreen];
    
    for (int lineIndex = 0; lineIndex < segment->lineCount; lineIndex++) {
        int lineWidth = 0;
        TextLine* line = &segment->lines[lineIndex];
        
        for (int textIndex = 0; textIndex < line->textCount; textIndex++) {
            SIZE textSize;
            GetTextExtentPoint32W(hdc, line->texts[textIndex].text, 
                                (int)wcslen(line->texts[textIndex].text), &textSize);
            lineWidth += textSize.cx;
        }
        if (lineWidth > maxWidth) {
            maxWidth = lineWidth;
        }
    }
    
    ReleaseDC(renderer->hwnd, hdc);
    return maxWidth;
}

BOOL DoesTextFitInWindow(MarqueeRenderer* renderer) {
    if (renderer->segmentCount == 0 || renderer->currentScreen >= renderer->segmentCount) {
        return TRUE;
    }
    
    int textWidth = GetTextWidth(renderer);
    return textWidth <= renderer->config.screenWidth;
}

void UpdateMarquee(MarqueeRenderer* renderer) {
    if (!renderer->isRunning || renderer->segmentCount == 0) return;
    
    DWORD now = GetTickCount();
    
    /* Check if current screen should be centered */
    if (!renderer->isCurrentScreenCentered && DoesTextFitInWindow(renderer)) {
        renderer->isCurrentScreenCentered = TRUE;
        renderer->centerStartTime = now;
        InvalidateRect(renderer->hwnd, NULL, TRUE);
        return;
    }
    
    /* Handle centered text timing */
    if (renderer->isCurrentScreenCentered) {
        if (now - renderer->centerStartTime >= renderer->config.centerDelay) {
            /* Time to move to next screen */
            renderer->currentScreen = (renderer->currentScreen + 1) % renderer->segmentCount;
            renderer->scrollPosition = renderer->config.screenWidth;
            renderer->isCurrentScreenCentered = FALSE;
            renderer->centerStartTime = 0;
            InvalidateRect(renderer->hwnd, NULL, TRUE);
        }
        return;
    }
    
    /* Handle scrolling text using PM (pixelsPerFrame) instead of hardcoded 3 */
    if (now - renderer->lastUpdate >= renderer->config.timePerFrame) {
        renderer->scrollPosition -= renderer->config.pixelsPerFrame;
        
        if (renderer->scrollPosition < -GetTextWidth(renderer)) {
            renderer->currentScreen = (renderer->currentScreen + 1) % renderer->segmentCount;
            renderer->scrollPosition = renderer->config.screenWidth;
            Sleep(renderer->config.screenDelay);
        }
        
        renderer->lastUpdate = now;
        InvalidateRect(renderer->hwnd, NULL, TRUE);
    }
}

void RenderMarquee(MarqueeRenderer* renderer, HDC hdc) {
    if (renderer->segmentCount == 0 || renderer->currentScreen >= renderer->segmentCount) {
        return;
    }
    
    RECT rect;
    GetClientRect(renderer->hwnd, &rect);
    FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    
    SelectObject(hdc, renderer->font);
    SetBkMode(hdc, TRANSPARENT);
    
    TextSegment* segment = &renderer->segments[renderer->currentScreen];
    int lineHeight = renderer->config.screenHeight / renderer->config.linesPerScreen;
    
    int maxLines = renderer->config.linesPerScreen;
    if (segment->lineCount < maxLines) {
        maxLines = segment->lineCount;
    }
    
    for (int lineIndex = 0; lineIndex < maxLines; lineIndex++) {
        TextLine* line = &segment->lines[lineIndex];
        int y = lineIndex * lineHeight + 30;
        int x;
        
        if (renderer->isCurrentScreenCentered) {
            /* Calculate total width of this line for centering */
            int totalWidth = 0;
            for (int textIndex = 0; textIndex < line->textCount; textIndex++) {
                SIZE textSize;
                int textLen = (int)wcslen(line->texts[textIndex].text);
                GetTextExtentPoint32W(hdc, line->texts[textIndex].text, textLen, &textSize);
                totalWidth += textSize.cx;
            }
            /* Center the line */
            x = (renderer->config.screenWidth - totalWidth) / 2;
        } else {
            /* Use scrolling position */
            x = renderer->scrollPosition;
        }
        
        for (int textIndex = 0; textIndex < line->textCount; textIndex++) {
            ColoredText* coloredText = &line->texts[textIndex];
            SetTextColor(hdc, coloredText->color);
            
            SIZE textSize;
            int textLen = (int)wcslen(coloredText->text);
            GetTextExtentPoint32W(hdc, coloredText->text, textLen, &textSize);
            
            TextOutW(hdc, x, y, coloredText->text, textLen);
            x += textSize.cx;
        }
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        g_renderer = (MarqueeRenderer*)malloc(sizeof(MarqueeRenderer));
        InitRenderer(g_renderer, hwnd);
        break;
        
    case WM_DESTROY:
        if (g_renderer) {
            CleanupRenderer(g_renderer);
            free(g_renderer);
        }
        PostQuitMessage(0);
        break;
        
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (g_renderer) {
            RenderMarquee(g_renderer, hdc);
        }
        EndPaint(hwnd, &ps);
        break;
    }
    
    case WM_TIMER:
        if (g_renderer) {
            UpdateMarquee(g_renderer);
        }
        break;
        
    case WM_KEYDOWN:
        if (g_renderer) {
            switch (wParam) {
            case VK_SPACE:
                StartMarquee(g_renderer);
                break;
            case VK_ESCAPE:
                StopMarquee(g_renderer);
                break;
            case 'R':
                ResetMarquee(g_renderer);
                break;
            case 'L': {
                OPENFILENAMEW ofn;
                wchar_t filename[MAX_PATH] = {0};
                
                memset(&ofn, 0, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFile = filename;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Marquee Layout Files\0*.mly;*.txt\0All Files\0*.*\0";
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                
                if (GetOpenFileNameW(&ofn)) {
                    if (LoadLayoutFile(g_renderer, filename)) {
                        StartMarquee(g_renderer);
                        //MessageBoxW(hwnd, L"Layout loaded!", L"Success", MB_OK);
                    } else {
                        MessageBoxW(hwnd, L"Failed to load file!", L"Error", MB_OK | MB_ICONERROR);
                    }
                }
                
                break;
            }
            }
        }
        break;
    }
    
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; /* Suppress unused parameter warning */
    //(void)lpCmdLine;     /* Suppress unused parameter warning */
    
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MarqueeRenderer";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    /* Load icon from resources */
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    if (!wc.hIcon) {
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION); /* Fallback */
    }
    RegisterClassW(&wc);
    
    HWND hwnd = CreateWindowExW(
        0,
        L"MarqueeRenderer",
        L"Marquee Renderer - L:Load SPACE:Start ESC:Stop R:Reset",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 200,
        NULL, NULL, hInstance, NULL
    );
    
    if (!hwnd) return 0;
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    if(*lpCmdLine != 0)
    {
        if(LoadLayoutFile(g_renderer, lpCmdLine))
            StartMarquee(g_renderer);
        else MessageBoxW(hwnd, lpCmdLine, L"Could not open Marquee Layout!", MB_OK | MB_ICONERROR);
    }
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}