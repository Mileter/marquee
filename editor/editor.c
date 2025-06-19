/* editor.c - Marquee Layout Editor and Validator for Windows */
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define EDITOR
#include "../rc/resource.h"

#define MAX_ERROR_MSG 512
#define MAX_ERRORS 50
#define MAX_NESTING_DEPTH 255
#define GUTTER_WIDTH 50

typedef struct {
	int lineNumber;
	wchar_t message[MAX_ERROR_MSG];
	int severity;		/* 0=info, 1=warning, 2=error */
} ValidationError;

typedef struct {
	HWND hwndMain;
	HWND hwndEdit;
	HWND hwndErrorList;
	HWND hwndStatus;
	wchar_t currentFile[MAX_PATH];
	BOOL isModified;
	ValidationError errors[MAX_ERRORS];
	int errorCount;
	HFONT hFont;
	int lineHeight;
	int gutterWidth;
	WNDPROC originalEditProc;
	HINSTANCE hInstance;
} EditorState;

EditorState *g_editor = NULL;

void SetStatusText(const wchar_t *text)
{
	if (g_editor && g_editor->hwndStatus) {
		SetWindowTextW(g_editor->hwndStatus, text);
	}
}

void AddValidationError(int lineNum, const wchar_t *message, int severity)
{
	if (g_editor->errorCount < MAX_ERRORS) {
		g_editor->errors[g_editor->errorCount].lineNumber = lineNum;
		wcscpy(g_editor->errors[g_editor->errorCount].message, message);
		g_editor->errors[g_editor->errorCount].severity = severity;
		g_editor->errorCount++;
	}
}

void ClearErrors()
{
	g_editor->errorCount = 0;
	ListView_DeleteAllItems(g_editor->hwndErrorList);
}

void UpdateErrorList()
{
	ListView_DeleteAllItems(g_editor->hwndErrorList);

	for (int i = 0; i < g_editor->errorCount; i++) {
		LVITEMW item = { 0 };
		wchar_t lineStr[32];
		wchar_t severityStr[16];

		/* Line number */
		swprintf(lineStr, 32, L"%d", g_editor->errors[i].lineNumber);

		/* Severity */
		switch (g_editor->errors[i].severity) {
		case 0:
			wcscpy(severityStr, L"Info");
			break;
		case 1:
			wcscpy(severityStr, L"Warning");
			break;
		case 2:
			wcscpy(severityStr, L"Error");
			break;
		default:
			wcscpy(severityStr, L"Unknown");
			break;
		}

		item.mask = LVIF_TEXT;
		item.iItem = i;
		item.iSubItem = 0;
		item.pszText = lineStr;
		ListView_InsertItem(g_editor->hwndErrorList, &item);

		item.iSubItem = 1;
		item.pszText = severityStr;
		ListView_SetItem(g_editor->hwndErrorList, &item);

		item.iSubItem = 2;
		item.pszText = g_editor->errors[i].message;
		ListView_SetItem(g_editor->hwndErrorList, &item);
	}
}

void UpdateGutterAndRect()
{
	if (!g_editor->hwndEdit)
		return;

	/* Set the formatting rectangle to account for the gutter */
	RECT clientRect;
	GetClientRect(g_editor->hwndEdit, &clientRect);

	RECT formatRect = clientRect;
	formatRect.left += g_editor->gutterWidth;
	formatRect.right -= 4;	/* Small right margin */
	formatRect.top += 2;	/* Small top margin */
	formatRect.bottom -= 2;	/* Small bottom margin */

	SendMessageW(g_editor->hwndEdit, EM_SETRECTNP, 0,
		     (LPARAM) & formatRect);

	/* Force a redraw */
	InvalidateRect(g_editor->hwndEdit, NULL, TRUE);
}

TEXTMETRICW tm;

LRESULT CALLBACK
EditControlProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_PAINT:
		{
			/* Call the original paint first */
			LRESULT result =
			    CallWindowProcW(g_editor->originalEditProc, hwnd,
					    uMsg,
					    wParam,
					    lParam);

			/* Now paint the line numbers in the gutter */
			HDC hdc = GetDC(hwnd);
			if (hdc) {
				RECT clientRect;
				GetClientRect(hwnd, &clientRect);

				/* Set up drawing context for gutter */
				SetBkColor(hdc, RGB(245, 245, 245));
				SetTextColor(hdc, RGB(100, 100, 100));
				SelectObject(hdc, g_editor->hFont);

				/* Fill gutter background */
				RECT gutterRect = { 0, 0, g_editor->gutterWidth,
					clientRect.bottom
				};
				FillRect(hdc, &gutterRect,
					 CreateSolidBrush(RGB(245, 245, 245)));

				/* Get first visible line */
				int firstVisibleLine = (int)SendMessageW(hwnd,
									 EM_GETFIRSTVISIBLELINE,
									 0,
									 0);
				int totalLines =
				    (int)SendMessageW(hwnd, EM_GETLINECOUNT, 0,
						      0);

				/* Calculate visible lines */
				int visibleLines =
				    (clientRect.bottom -
				     clientRect.top) / g_editor->lineHeight + 2;

				/* Draw line numbers */
				/* Get the y position of the first visible line */
				int baseYPos = tm.tmExternalLeading;

				for (int i = 0;
				     i < visibleLines
				     && (firstVisibleLine + i) < totalLines;
				     i++) {
					int lineNum = firstVisibleLine + i + 1;
					wchar_t lineText[16];
					swprintf(lineText, 16, L"%d", lineNum);

					/* Calculate y position: base position + (line offset * line height) */
					int yPos =
					    baseYPos +
					    (i * g_editor->lineHeight);

					/* Only draw if the line is within the visible client area */
					if (yPos >= 0
					    && yPos + g_editor->lineHeight <=
					    clientRect.bottom) {
						RECT lineRect = {
							2,
							yPos,
							g_editor->gutterWidth -
							    4,
							yPos +
							    g_editor->lineHeight
						};

						DrawTextW(hdc, lineText, -1,
							  &lineRect,
							  DT_RIGHT | DT_VCENTER
							  | DT_SINGLELINE);
					}
				}

				/* Draw separator line */
				HPEN pen =
				    CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
				HPEN oldPen = SelectObject(hdc, pen);
				MoveToEx(hdc, g_editor->gutterWidth - 1, 0,
					 NULL);
				LineTo(hdc, g_editor->gutterWidth - 1,
				       clientRect.bottom);
				SelectObject(hdc, oldPen);
				DeleteObject(pen);

				ReleaseDC(hwnd, hdc);
			}

			return result;
		}

	case WM_LBUTTONDOWN:
		{
			/* Check if click is in the gutter */
			int xPos = GET_X_LPARAM(lParam);
			if (xPos < g_editor->gutterWidth) {
				/* Handle gutter click - could select entire line */
				int yPos = GET_Y_LPARAM(lParam);
				POINT pt = { g_editor->gutterWidth + 1, yPos };
				int charIndex =
				    (int)SendMessageW(hwnd, EM_CHARFROMPOS, 0,
						      MAKELPARAM(pt.x, pt.y));
				if (charIndex >= 0) {
					int lineIndex = (int)SendMessageW(hwnd,
									  EM_LINEFROMCHAR,
									  charIndex,
									  0);
					int lineStart = (int)SendMessageW(hwnd,
									  EM_LINEINDEX,
									  lineIndex,
									  0);
					int lineLength = (int)SendMessageW(hwnd,
									   EM_LINELENGTH,
									   lineStart,
									   0);

					/* Select the entire line */
					SendMessageW(hwnd, EM_SETSEL, lineStart,
						     lineStart + lineLength);
				}
				return 0;
			}
			break;
		}

	case WM_ERASEBKGND:
		/* Let the original control handle background, we'll paint gutter in WM_PAINT */
		break;

	case WM_VSCROLL:
	case WM_HSCROLL:
	case WM_MOUSEWHEEL:
		/* Call original handler first, then update display */
		CallWindowProcW(g_editor->originalEditProc, hwnd, uMsg, wParam,
				lParam);
		InvalidateRect(hwnd, NULL, FALSE);
		return 0;

	case WM_SIZE:
		/* Update formatting rect when control is resized */
		CallWindowProcW(g_editor->originalEditProc, hwnd, uMsg, wParam,
				lParam);
		UpdateGutterAndRect();
		return 0;
	}

	return CallWindowProcW(g_editor->originalEditProc, hwnd, uMsg, wParam,
			       lParam);
}

BOOL IsValidHexColor(const wchar_t *str, int len)
{
	if (len != 6)
		return FALSE;
	for (int i = 0; i < 6; i++) {
		wchar_t c = str[i];
		if (!((c >= L'0' && c <= L'9') ||
		      (c >= L'A' && c <= L'F') || (c >= L'a' && c <= L'f'))) {
			return FALSE;
		}
	}
	return TRUE;
}

typedef struct {
	int position;		/* Position in the line where backtick opened */
	BOOL hasColor;		/* Whether this backtick has a color specification */
} BacktickState;

typedef struct {
	BacktickState stack[MAX_NESTING_DEPTH];
	int depth;
} BacktickStack;

void InitBacktickStack(BacktickStack *stack)
{
	stack->depth = 0;
}

BOOL PushBacktick(BacktickStack *stack, int position, BOOL hasColor)
{
	if (stack->depth >= MAX_NESTING_DEPTH) {
		return FALSE;	/* Stack overflow */
	}
	stack->stack[stack->depth].position = position;
	stack->stack[stack->depth].hasColor = hasColor;
	stack->depth++;
	return TRUE;
}

BOOL PopBacktick(BacktickStack *stack, BacktickState *state)
{
	if (stack->depth <= 0) {
		return FALSE;	/* Stack underflow */
	}
	stack->depth--;
	if (state) {
		*state = stack->stack[stack->depth];
	}
	return TRUE;
}

BOOL IsStackEmpty(BacktickStack *stack)
{
	return stack->depth == 0;
}

void ValidateColorSyntax(const wchar_t *line, int lineNum)
{
	int len = (int)wcslen(line);
	BacktickStack stack;
	InitBacktickStack(&stack);

	for (int i = 0; i < len; i++) {
		if (line[i] == L'\\' && i + 1 < len) {
			i++;	/* Skip escaped character */
			continue;
		}

		if (line[i] == L'`') {
			/* Look for colon to determine if this has color specification */
			int colonPos = -1;
			int nextQuotePos = -1;

			/* Find the next colon and quote to understand the structure */
			for (int j = i + 1; j < len; j++) {
				if (line[j] == L'\\' && j + 1 < len) {
					j++;	/* Skip escaped character */
					continue;
				}
				if (line[j] == L':' && colonPos == -1) {
					colonPos = j;
				} else if (line[j] == L'\''
					   && nextQuotePos == -1) {
					nextQuotePos = j;
					break;
				} else if (line[j] == L'`') {
					/* Found another backtick before finding a quote */
					break;
				}
			}

			BOOL hasColorSpec = FALSE;

			if (colonPos != -1
			    && (nextQuotePos == -1 || colonPos < nextQuotePos))
			{
				/* Validate color specification between backtick and colon */
				int paramLen = colonPos - i - 1;
				if (paramLen == 6) {
					wchar_t colorSpec[7] = { 0 };
					wcsncpy(colorSpec, &line[i + 1], 6);
					if (IsValidHexColor(colorSpec, 6)) {
						hasColorSpec = TRUE;
					} else {
						AddValidationError(lineNum,
								   L"Invalid hex color specification",
								   2);
					}
				} else if (paramLen > 0) {
					AddValidationError(lineNum,
							   L"Color specification must be exactly 6 hex characters",
							   2);
				}
			}

			/* Push this backtick onto the stack */
			if (!PushBacktick(&stack, i, hasColorSpec)) {
				AddValidationError(lineNum,
						   L"Too many nested color specifications (maximum 10)",
						   2);
			}

		} else if (line[i] == L'\'') {
			/* Pop the most recent backtick from stack */
			BacktickState state;
			if (!PopBacktick(&stack, &state)) {
				AddValidationError(lineNum,
						   L"Closing quote without opening backtick",
						   2);
			} else {
				/* Successfully matched a backtick-quote pair */
				/* Could add additional validation here if needed */
			}
		}
	}

	/* Check for unmatched backticks */
	if (!IsStackEmpty(&stack)) {
		wchar_t errorMsg[256];
		swprintf(errorMsg, 256,
			 L"Unclosed color specification (%d unmatched backticks)",
			 stack.depth);
		AddValidationError(lineNum, errorMsg, 2);
	}
}

void ValidateFile()
{
	ClearErrors();

	int textLen = GetWindowTextLengthW(g_editor->hwndEdit);
	wchar_t *buffer = malloc((textLen + 1) * sizeof(wchar_t));
	if (!buffer)
		return;

	GetWindowTextW(g_editor->hwndEdit, buffer, textLen + 1);

	/* Parse line by line */
	wchar_t line[1024];
	int linePos = 0;
	int lineNum = 1;
	int segmentCount = 0;
	BOOL hasLPS = FALSE, hasSW = FALSE, hasSH = FALSE, hasSC =
	    FALSE, hasSD = FALSE;
	BOOL inSegment = FALSE;
	int expectedSegments = 0;

	/* Optional flags tracking */
	BOOL hasTPF = FALSE, hasPM = FALSE;

	for (int i = 0; i <= textLen; i++) {
		wchar_t c = (i < textLen) ? buffer[i] : L'\0';

		if (c == L'\r') {
			continue;
		} else if (c == L'\n' || c == L'\0') {
			line[linePos] = L'\0';

			int len = (int)wcslen(line);

			/* Skip empty lines and comments */
			if (line[0] != L'/' && len > 0) {
				/* Check metadata commands */
				if (wcsncmp(line, L"LPS", 3) == 0) {
					if (hasLPS)
						AddValidationError(lineNum,
								   L"Duplicate LPS command",
								   2);
					hasLPS = TRUE;
					int value = _wtoi(&line[4]);
					if (value <= 0)
						AddValidationError(lineNum,
								   L"LPS must be positive",
								   2);
				} else if (wcsncmp(line, L"SW", 2) == 0) {
					if (hasSW)
						AddValidationError(lineNum,
								   L"Duplicate SW command",
								   2);
					hasSW = TRUE;
					int value = _wtoi(&line[3]);
					if (value <= 0)
						AddValidationError(lineNum,
								   L"SW must be positive",
								   2);
				} else if (wcsncmp(line, L"SH", 2) == 0) {
					if (hasSH)
						AddValidationError(lineNum,
								   L"Duplicate SH command",
								   2);
					hasSH = TRUE;
					int value = _wtoi(&line[3]);
					if (value <= 0)
						AddValidationError(lineNum,
								   L"SH must be positive",
								   2);
				} else if (wcsncmp(line, L"SC", 2) == 0) {
					if (hasSC)
						AddValidationError(lineNum,
								   L"Duplicate SC command",
								   2);
					hasSC = TRUE;
					expectedSegments = _wtoi(&line[3]);
					if (expectedSegments <= 0)
						AddValidationError(lineNum,
								   L"SC must be positive",
								   2);
				} else if (wcsncmp(line, L"SD", 2) == 0) {
					if (hasSD)
						AddValidationError(lineNum,
								   L"Duplicate SD command",
								   2);
					hasSD = TRUE;
					int value = _wtoi(&line[3]);
					if (value < 0)
						AddValidationError(lineNum,
								   L"SD cannot be negative",
								   2);
				} else if (wcsncmp(line, L"CD", 2) == 0) {
					int value = _wtoi(&line[3]);
					if (value < 0)
						AddValidationError(lineNum,
								   L"CD cannot be negative",
								   2);

					/* OPTIONAL FLAGS */
				} else if (wcsncmp(line, L"TPF", 3) == 0) {
					if (hasTPF)
						AddValidationError(lineNum, L"Duplicate TPF command", 1);	/* Warning for optional */
					hasTPF = TRUE;
					int value = _wtoi(&line[4]);
					if (value <= 0)
						AddValidationError(lineNum,
								   L"TPF (millis per frame) must be positive",
								   2);
					if (value < 16)
						AddValidationError(lineNum, L"TPF below 16ms may cause performance issues", 1);	/* Warning */
				} else if (wcsncmp(line, L"PM", 2) == 0) {
					if (hasPM)
						AddValidationError(lineNum, L"Duplicate PM command", 1);	/* Warning for optional */
					hasPM = TRUE;
					int value = _wtoi(&line[3]);
					if (value <= 0)
						AddValidationError(lineNum,
								   L"PM (pixel movement per frame) must be positive",
								   2);
					if (value > 20)
						AddValidationError(lineNum, L"PM above 20 pixels may scroll too fast", 1);	/* Warning */

				} else if (wcscmp(line, L"START") == 0) {
					if (inSegment)
						AddValidationError(lineNum,
								   L"START inside another segment",
								   2);
					inSegment = TRUE;
				} else if (wcscmp(line, L"END") == 0) {
					if (!inSegment)
						AddValidationError(lineNum,
								   L"END without START",
								   2);
					inSegment = FALSE;
					segmentCount++;
				} else if (inSegment) {
					ValidateColorSyntax(line, lineNum);
				} else {
					AddValidationError(lineNum,
							   L"Text outside segment",
							   2);
				}
			}

			linePos = 0;
			lineNum++;

			if (c == L'\0')
				break;
		} else {
			if (linePos < 1023) {
				line[linePos++] = c;
			}
		}
	}

	/* Check required metadata */
	if (!hasLPS)
		AddValidationError(0, L"Missing LPS command", 2);
	if (!hasSW)
		AddValidationError(0, L"Missing SW command", 2);
	if (!hasSH)
		AddValidationError(0, L"Missing SH command", 2);
	if (!hasSC)
		AddValidationError(0, L"Missing SC command", 2);
	if (!hasSD)
		AddValidationError(0, L"Missing SD command", 2);

	/* Check segment count */
	if (expectedSegments != segmentCount) {
		wchar_t msg[256];
		swprintf(msg, 256, L"Expected %d segments, found %d",
			 expectedSegments, segmentCount);
		AddValidationError(0, msg, 2);
	}

	if (inSegment) {
		AddValidationError(lineNum - 1,
				   L"File ends with unclosed segment", 2);
	}

	free(buffer);
	UpdateErrorList();

	/* Update status */
	if (g_editor->errorCount == 0) {
		SetStatusText(L"Validation passed - No errors found");
	} else {
		wchar_t statusMsg[64];
		swprintf(statusMsg, 64, L"Validation failed - %d errors found",
			 g_editor->errorCount);
		SetStatusText(statusMsg);
	}
}

void NewFile()
{
	const wchar_t *template =
	    L"// Created with Marquee Layout Editor\r\n"
	    L"\r\n"
	    L"LPS 2\r\n"
	    L"//         Number of lines per screenfull\r\n"
	    L"\r\n"
	    L"SW  600\r\n"
	    L"//         Screen width, px\r\n"
	    L"SH  40\r\n"
	    L"//         Screen height, px\r\n"
	    L"\r\n"
	    L"SC  1\r\n"
	    L"//         Count of screens\r\n"
	    L"SD  500\r\n"
	    L"//         Delay after marquee stop scrolling to move to next screen (ms)\r\n"
	    L"CD  1500\r\n"
	    L"//         Delay after non-scrolling screen is displayed before moving to the next scene. (ms)\r\n"
	    L"TPF 50\r\n"
	    L"//         OPTIONAL, millis per frame\r\n"
	    L"PM  3\r\n"
	    L"//         OPTIONAL, pixel movement per frame\r\n"
	    L"\r\n"
	    L"//         Screen template\r\n"
	    L"START\r\n" L"`00FF00:Hello, `FF0000:world'!'\r\n" L"END\r\n";

	SetWindowTextW(g_editor->hwndEdit, template);
	wcscpy(g_editor->currentFile, L"");
	g_editor->isModified = FALSE;
	SetStatusText(L"New file created");
	UpdateGutterAndRect();
}

void LoadFile_impl(wchar_t *filename)
{
	FILE *file = _wfopen(filename, L"rb");
	if (file) {
		fseek(file, 0, SEEK_END);
		long size = ftell(file);
		fseek(file, 0, SEEK_SET);

		if (size >= 2) {
			/* Check for UTF-16LE BOM */
			unsigned char bom[2];
			fread(bom, 1, 2, file);

			if (bom[0] == 0xFF && bom[1] == 0xFE) {
				/* UTF-16LE file with BOM */
				long remainingSize = size - 2;
				if (remainingSize > 0) {
					wchar_t *wideBuffer =
					    malloc(remainingSize +
						   sizeof(wchar_t));
					if (wideBuffer) {
						size_t bytesRead =
						    fread(wideBuffer, 1,
							  remainingSize, file);
						/* Null terminate */
						wideBuffer[bytesRead /
							   sizeof(wchar_t)] =
						    L'\0';
						SetWindowTextW
						    (g_editor->hwndEdit,
						     wideBuffer);
						free(wideBuffer);
					}
				} else {
					/* Empty file with just BOM */
					SetWindowTextW(g_editor->hwndEdit, L"");
				}
			} else {
				/* Not UTF-16LE, rewind and read as multibyte */
				fseek(file, 0, SEEK_SET);

				char *buffer = malloc(size + 1);
				if (buffer) {
					fread(buffer, 1, size, file);
					buffer[size] = 0;

					/* Use IsTextUnicode to determine encoding */
					INT flags =
					    IS_TEXT_UNICODE_STATISTICS |
					    IS_TEXT_UNICODE_CONTROLS;
					if (IsTextUnicode(buffer, size, &flags)) {
						/* Treat as UTF-16 */
						SetWindowTextW
						    (g_editor->hwndEdit,
						     (wchar_t *)
						     buffer);
					} else {
						/* Convert from multibyte to wide chars */
						int wideSize =
						    MultiByteToWideChar(CP_UTF8,
									0,
									buffer,
									-1,
									NULL,
									0);
						wchar_t *wideBuffer =
						    malloc(wideSize *
							   sizeof(wchar_t));
						if (wideBuffer) {
							MultiByteToWideChar
							    (CP_UTF8, 0, buffer,
							     -1, wideBuffer,
							     wideSize);
							SetWindowTextW
							    (g_editor->hwndEdit,
							     wideBuffer);
							free(wideBuffer);
						}
					}
					free(buffer);
				}
			}
		} else {
			/* File too small or empty */
			SetWindowTextW(g_editor->hwndEdit, L"");
		}

		fclose(file);
		wcscpy(g_editor->currentFile, filename);
		g_editor->isModified = FALSE;
		SetStatusText(L"File opened successfully");
		UpdateGutterAndRect();
	} else {
		MessageBoxW(g_editor->hwndMain, L"Could not open file",
			    L"Error", MB_OK | MB_ICONERROR);
	}
}

void LoadFile()
{
	OPENFILENAMEW ofn;
	wchar_t filename[MAX_PATH] = { 0 };

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_editor->hwndMain;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter =
	    L"Marquee Layout Files\0*.mly\0Text Files\0*.txt\0All Files\0*.*\0";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

	if (GetOpenFileNameW(&ofn)) {
		LoadFile_impl(filename);
	}
}

void SaveFile(BOOL saveAs)
{
	wchar_t filename[MAX_PATH];
	wcscpy(filename, g_editor->currentFile);

	if (saveAs || wcslen(filename) == 0) {
		OPENFILENAMEW ofn;
		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = g_editor->hwndMain;
		ofn.lpstrFile = filename;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter =
		    L"Marquee Layout Files\0*.mly\0Text Files\0*.txt\0All Files\0*.*\0";
		ofn.lpstrDefExt = L"mly";
		ofn.Flags = OFN_OVERWRITEPROMPT;

		if (!GetSaveFileNameW(&ofn))
			return;
	}

	int textLen = GetWindowTextLengthW(g_editor->hwndEdit);
	wchar_t *wideBuffer = malloc((textLen + 1) * sizeof(wchar_t));
	if (wideBuffer) {
		GetWindowTextW(g_editor->hwndEdit, wideBuffer, textLen + 1);

		/* Use _wfopen with wide character filename */
		FILE *file = _wfopen(filename, L"wb");
		if (file) {
			unsigned char bom[2] = { 0xFF, 0xFE };	// WE ARE THE UTF-16 LE!!
			fwrite(bom, 1, 2, file);
			/* Write the wide character content directly as UTF-16LE */
			size_t textBytes = wcslen(wideBuffer) * sizeof(wchar_t);
			fwrite(wideBuffer, 1, textBytes, file);

			fclose(file);
			wcscpy(g_editor->currentFile, filename);
			g_editor->isModified = FALSE;
			SetStatusText(L"File saved successfully");
		} else {
			MessageBoxW(g_editor->hwndMain, L"Could not save file",
				    L"Error", MB_OK | MB_ICONERROR);
		}
		free(wideBuffer);
	}
}

void LaunchPreview()
{
	if (g_editor->isModified || wcslen(g_editor->currentFile) == 0) {
		int result = MessageBoxW(g_editor->hwndMain,
					 L"File must be saved before preview. Save now?",
					 L"Preview",
					 MB_YESNO | MB_ICONQUESTION);
		if (result == IDYES) {
			SaveFile(FALSE);
		} else {
			return;
		}
	}

	/* Launch the renderer with current file */
	STARTUPINFOW si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	si.cb = sizeof(si);

	wchar_t cmdLine[MAX_PATH + 50];
	wchar_t filename[MAX_PATH];
	wcscpy(filename, g_editor->currentFile);
	swprintf(cmdLine, MAX_PATH + 50, L"renderer.exe %ls", filename);

	if (!CreateProcessW
	    (NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
		MessageBoxW(g_editor->hwndMain,
			    L"Could not launch renderer. Make sure renderer.exe is in the same directory.",
			    L"Preview Error", MB_OK | MB_ICONERROR);
	} else {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
}

void ShowAbout()
{
	MessageBoxW(g_editor->hwndMain,
		    L"Marquee Layout Editor v1.1\n\nAn editor for marquee layout files.\n\nSupports syntax validation and preview functionality.\n\nCopyright (c) Mileter 2025, 3-Clause BSD License.",
		    L"About Marquee Editor", MB_OK | MB_ICONINFORMATION);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_CREATE:
		{
			/* Load and set the menu from resources */
			HMENU hMenu = LoadMenuW(g_editor->hInstance,
						MAKEINTRESOURCEW(IDR_MAINMENU));
			if (hMenu) {
				SetMenu(hwnd, hMenu);
			}

			/* Create font for editor */
			g_editor->hFont =
			    CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
					FALSE, DEFAULT_CHARSET,
					OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
					DEFAULT_QUALITY,
					FIXED_PITCH | FF_MODERN, L"MingLiU");
			if (!g_editor->hFont)
				g_editor->hFont =
				    CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE,
						FALSE, FALSE, DEFAULT_CHARSET,
						OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,
						DEFAULT_QUALITY,
						FIXED_PITCH | FF_MODERN,
						L"Courier New");

			/* Calculate line height and gutter width */
			HDC hdc = GetDC(hwnd);
			SelectObject(hdc, g_editor->hFont);
			GetTextMetricsW(hdc, &tm);
			g_editor->lineHeight = tm.tmHeight;

			/* Calculate width needed for line numbers (assume max 9999 lines) */
			SIZE size;
			GetTextExtentPoint32W(hdc, L"9999", 4, &size);
			g_editor->gutterWidth = size.cx + 10;	/* Add some padding */
			ReleaseDC(hwnd, hdc);

			/* Create edit control */
			g_editor->hwndEdit = CreateWindowExW(WS_EX_CLIENTEDGE,
							     L"EDIT",
							     L"",
							     WS_CHILD |
							     WS_VISIBLE |
							     WS_VSCROLL |
							     WS_HSCROLL |
							     ES_MULTILINE |
							     ES_AUTOVSCROLL |
							     ES_AUTOHSCROLL |
							     ES_NOHIDESEL, 10,
							     10, 800, 400, hwnd,
							     (HMENU)
							     IDC_EDIT_MAIN,
							     g_editor->hInstance,
							     NULL);

			/* Subclass the edit control to handle custom painting */
			g_editor->originalEditProc =
			    (WNDPROC) SetWindowLongPtrW(g_editor->hwndEdit,
							GWLP_WNDPROC, (LONG_PTR)
							EditControlProc);

			/* Create error list */
			g_editor->hwndErrorList =
			    CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
					    WS_CHILD | WS_VISIBLE | LVS_REPORT |
					    LVS_SINGLESEL, 10, 450, 800, 150,
					    hwnd, (HMENU)
					    IDC_LIST_ERRORS,
					    g_editor->hInstance, NULL);

			/* Setup error list columns */
			LVCOLUMNW col = { 0 };
			col.mask = LVCF_TEXT | LVCF_WIDTH;
			col.cx = 60;
			col.pszText = L"Line";
			ListView_InsertColumn(g_editor->hwndErrorList, 0, &col);

			col.cx = 80;
			col.pszText = L"Type";
			ListView_InsertColumn(g_editor->hwndErrorList, 1, &col);

			col.cx = 640;
			col.pszText = L"Message";
			ListView_InsertColumn(g_editor->hwndErrorList, 2, &col);

			SendMessageW(g_editor->hwndEdit, WM_SETFONT,
				     (WPARAM) g_editor->hFont, TRUE);
			SendMessageW(g_editor->hwndErrorList, WM_SETFONT,
				     (WPARAM) g_editor->hFont, TRUE);

			/* Create status bar */
			g_editor->hwndStatus =
			    CreateWindowW(STATUSCLASSNAMEW, L"Ready",
					  WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
					  hwnd, (HMENU) IDC_STATUS,
					  g_editor->hInstance, NULL);

			NewFile();
			break;
		}

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDM_FILE_NEW:
			NewFile();
			break;
		case IDM_FILE_OPEN:
			LoadFile();
			break;
		case IDM_FILE_SAVE:
			SaveFile(FALSE);
			break;
		case IDM_FILE_SAVEAS:
			SaveFile(TRUE);
			break;
		case IDM_FILE_EXIT:
			PostMessage(hwnd, WM_CLOSE, 0, 0);
			break;
		case IDM_TOOLS_VALIDATE:
			ValidateFile();
			break;
		case IDM_TOOLS_PREVIEW:
			LaunchPreview();
			break;
		case IDM_HELP_ABOUT:
			ShowAbout();
			break;
		case IDC_EDIT_MAIN:
			if (HIWORD(wParam) == EN_CHANGE) {
				g_editor->isModified = TRUE;
			}
			break;
		}
		break;

	case WM_SIZE:
		{
			RECT rect;
			GetClientRect(hwnd, &rect);

			/* Resize status bar */
			SendMessageW(g_editor->hwndStatus, WM_SIZE, 0, 0);

			/* Get status bar height */
			RECT statusRect;
			GetWindowRect(g_editor->hwndStatus, &statusRect);
			int statusHeight = statusRect.bottom - statusRect.top;

			/* Resize edit control - takes up top half */
			SetWindowPos(g_editor->hwndEdit, NULL, 10, 10,
				     rect.right - 20,
				     (rect.bottom - statusHeight) / 2 - 20,
				     SWP_NOZORDER);

			/* Resize error list - takes up bottom half */
			SetWindowPos(g_editor->hwndErrorList, NULL, 10,
				     (rect.bottom - statusHeight) / 2 + 10,
				     rect.right - 20,
				     (rect.bottom - statusHeight) / 2 - 20,
				     SWP_NOZORDER);

			/* Update gutter and formatting rect */
			UpdateGutterAndRect();

			break;
		}

	case WM_CLOSE:
		if (g_editor->isModified) {
			int result =
			    MessageBoxW(hwnd, L"Save changes before closing?",
					L"Marquee Editor",
					MB_YESNOCANCEL | MB_ICONQUESTION);
			if (result == IDYES) {
				SaveFile(FALSE);
			} else if (result == IDCANCEL) {
				return 0;
			}
		}
		DestroyWindow(hwnd);
		break;

	case WM_DESTROY:
		if (g_editor->hFont) {
			DeleteObject(g_editor->hFont);
		}
		PostQuitMessage(0);
		break;
	}

	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine,
	 int nCmdShow)
{
	(void)hPrevInstance;
	(void)lpCmdLine;

	/* Initialize common controls */
	InitCommonControls();

	/* Initialize editor state */
	g_editor = malloc(sizeof(EditorState));
	memset(g_editor, 0, sizeof(EditorState));
	g_editor->hInstance = hInstance;

	/* Register window class */
	WNDCLASSW wc;
	memset(&wc, 0, sizeof(wc));
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"MarqueeEditor";
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	/* Load icon from resources */
	wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
	if (!wc.hIcon) {
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);	/* Fallback */
	}

	RegisterClassW(&wc);

	/* Create window */
	g_editor->hwndMain = CreateWindowExW(0,
					     L"MarqueeEditor",
					     L"Marquee Layout Editor",
					     WS_OVERLAPPEDWINDOW,
					     CW_USEDEFAULT, CW_USEDEFAULT, 920,
					     700, NULL, NULL, hInstance, NULL);

	if (!g_editor->hwndMain) {
		free(g_editor);
		return 0;
	}

	/* Load file if in lpCmdLine */
	size_t lpCmdLineLen = wcslen(lpCmdLine);
	if (lpCmdLineLen > 0) {
		LPWSTR trimmed = malloc((lpCmdLineLen + 1) * sizeof(wchar_t));

		int offset = 0;

		for (size_t i = 0; i < lpCmdLineLen; i++) {
			if (lpCmdLine[i] == '"')
				continue;
			if (lpCmdLine[i] == 0)
				break;
			trimmed[offset] = lpCmdLine[i];
			offset++;
		}
		trimmed[offset] = 0;	// add null byte terminator

		LoadFile_impl(trimmed);

		free(trimmed);
		trimmed = 0;
	}

	/* Load accelerator table from resources */
	HACCEL hAccel =
	    LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDR_ACCELERATOR));

	ShowWindow(g_editor->hwndMain, nCmdShow);
	UpdateWindow(g_editor->hwndMain);

	/* Message loop with accelerator support */
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		/* Try to translate accelerator first */
		if (!hAccel
		    || !TranslateAcceleratorW(g_editor->hwndMain, hAccel, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	/* Cleanup */
	if (hAccel) {
		DestroyAcceleratorTable(hAccel);
	}

	free(g_editor);
	return (int)msg.wParam;
}
