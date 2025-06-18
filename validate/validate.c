/* validate.c - Standalone Marquee Layout File Validator */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#define VALIDATE
#include "../rc/resource.h"

#define MAX_ERROR_MSG 512
#define MAX_ERRORS 1000
#define MAX_NESTING_DEPTH 255

typedef struct {
    int lineNumber;
    wchar_t message[MAX_ERROR_MSG];
    int severity; /* 0=info, 1=warning, 2=error */
} ValidationError;

ValidationError errors[MAX_ERRORS];
int errorCount = 0;

void AddValidationError(int lineNum, const wchar_t* message, int severity) {
    if (errorCount < MAX_ERRORS) {
	errors[errorCount].lineNumber = lineNum;
        wcscpy(errors[errorCount].message, message);
        errors[errorCount].severity = severity;
        errorCount++;
    }
}

int IsValidHexColor(const wchar_t* str, int len) {
    if (len != 6) return 0;
    for (int i = 0; i < 6; i++) {
        wchar_t c = str[i];
        if (!((c >= L'0' && c <= L'9') || 
              (c >= L'A' && c <= L'F') || 
              (c >= L'a' && c <= L'f'))) {
            return 0;
        }
    }
    return 1;
}

typedef struct {
    int position;     /* Position in the line where backtick opened */
    int hasColor;     /* Whether this backtick has a color specification */
} BacktickState;

typedef struct {
    BacktickState stack[MAX_NESTING_DEPTH];
    int depth;
} BacktickStack;

void InitBacktickStack(BacktickStack* stack) {
    stack->depth = 0;
}

int PushBacktick(BacktickStack* stack, int position, int hasColor) {
    if (stack->depth >= MAX_NESTING_DEPTH) {
        return 0; /* Stack overflow */
    }
    stack->stack[stack->depth].position = position;
    stack->stack[stack->depth].hasColor = hasColor;
    stack->depth++;
    return 1;
}

int PopBacktick(BacktickStack* stack, BacktickState* state) {
    if (stack->depth <= 0) {
        return 0; /* Stack underflow */
    }
    stack->depth--;
    if (state) {
        *state = stack->stack[stack->depth];
    }
    return 1;
}

int IsStackEmpty(BacktickStack* stack) {
    return stack->depth == 0;
}

void ValidateColorSyntax(const wchar_t* line, int lineNum) {
    int len = (int)wcslen(line);
    BacktickStack stack;
    InitBacktickStack(&stack);
    
    for (int i = 0; i < len; i++) {
        if (line[i] == L'\\' && i + 1 < len) {
            i++; /* Skip escaped character */
            continue;
        }
        
        if (line[i] == L'`') {
            /* Look for colon to determine if this has color specification */
            int colonPos = -1;
            int nextQuotePos = -1;
            
            /* Find the next colon and quote to understand the structure */
            for (int j = i + 1; j < len; j++) {
                if (line[j] == L'\\' && j + 1 < len) {
                    j++; /* Skip escaped character */
                    continue;
                }
                if (line[j] == L':' && colonPos == -1) {
                    colonPos = j;
                } else if (line[j] == L'\'' && nextQuotePos == -1) {
                    nextQuotePos = j;
                    break;
                } else if (line[j] == L'`') {
                    /* Found another backtick before finding a quote */
                    break;
                }
            }
            
            int hasColorSpec = 0;
            
            if (colonPos != -1 && (nextQuotePos == -1 || colonPos < nextQuotePos)) {
                /* Validate color specification between backtick and colon */
                int paramLen = colonPos - i - 1;
                if (paramLen == 6) {
                    wchar_t colorSpec[7] = {0};
                    wcsncpy(colorSpec, &line[i + 1], 6);
                    if (IsValidHexColor(colorSpec, 6)) {
                        hasColorSpec = 1;
                    } else {
                        AddValidationError(lineNum, L"Invalid hex color specification", 2);
                    }
                } else if (paramLen > 0) {
                    AddValidationError(lineNum, L"Color specification must be exactly 6 hex characters", 2);
                }
            }
            
            /* Push this backtick onto the stack */
            if (!PushBacktick(&stack, i, hasColorSpec)) {
                AddValidationError(lineNum, L"Too many nested color specifications (maximum 255)", 2);
            }
            
        } else if (line[i] == L'\'') {
            /* Pop the most recent backtick from stack */
            BacktickState state;
            if (!PopBacktick(&stack, &state)) {
                AddValidationError(lineNum, L"Closing quote without opening backtick", 2);
            } else {
                /* Successfully matched a backtick-quote pair */
                /* Could add additional validation here if needed */
            }
        }
    }
    
    /* Check for unmatched backticks */
    if (!IsStackEmpty(&stack)) {
        wchar_t errorMsg[256];
        swprintf(errorMsg, 256, L"Unclosed color specification (%d unmatched backticks)", stack.depth);
        AddValidationError(lineNum, errorMsg, 2);
    }
}

int ValidateFile(const char* filename) {
    errorCount = 0;
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        wprintf(L"Error: Could not open file '%s'\n", filename);
        return 0;
    }
    
    /* Get file size and read content (existing logic) */
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    wchar_t* buffer = NULL;
    
    /* File reading logic remains the same... */
    if (size >= 2) {
        unsigned char bom[2]; 
        fread(bom, 1, 2, file);
        
        if (bom[0] == 0xFF && bom[1] == 0xFE) {
            long remainingSize = size - 2;
            if (remainingSize > 0) {
                buffer = malloc(remainingSize + sizeof(wchar_t));
                if (buffer) {
                    size_t bytesRead = fread(buffer, 1, remainingSize, file);
                    buffer[bytesRead / sizeof(wchar_t)] = L'\0';
                }
            } else {
                buffer = malloc(sizeof(wchar_t));
                if (buffer) buffer[0] = L'\0';
            }
        } else {
            fseek(file, 0, SEEK_SET);
            char* mbBuffer = malloc(size + 1);
            if (mbBuffer) {
                fread(mbBuffer, 1, size, file);
                mbBuffer[size] = 0;
                
                size_t wideSize = mbstowcs(NULL, mbBuffer, 0) + 1;
                buffer = malloc(wideSize * sizeof(wchar_t));
                if (buffer) {
                    mbstowcs(buffer, mbBuffer, wideSize);
                }
                free(mbBuffer);
            }
        }
    } else {
        buffer = malloc(sizeof(wchar_t));
        if (buffer) buffer[0] = L'\0';
    }
    
    fclose(file);
    
    if (!buffer) {
        wprintf(L"Error: Could not allocate memory to read file\n");
        return 0;
    }
    
    /* Parse line by line with TPF and PM support */
    wchar_t line[1024];
    int linePos = 0;
    int lineNum = 1;
    int segmentCount = 0;
    int hasLPS = 0, hasSW = 0, hasSH = 0, hasSC = 0, hasSD = 0;
    int inSegment = 0;
    int expectedSegments = 0;
    int textLen = (int)wcslen(buffer);
    
    /* Optional flags tracking */
    int hasTPF = 0, hasPM = 0;
    
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
                    if (hasLPS) AddValidationError(lineNum, L"Duplicate LPS command", 2);
                    hasLPS = 1;
                    if (wcslen(line) > 4) {
                        int value = wcstol(&line[4], NULL, 10);
                        if (value <= 0) AddValidationError(lineNum, L"LPS must be positive", 2);
                    }
                } else if (wcsncmp(line, L"SW", 2) == 0) {
                    if (hasSW) AddValidationError(lineNum, L"Duplicate SW command", 2);
                    hasSW = 1;
                    if (wcslen(line) > 3) {
                        int value = wcstol(&line[3], NULL, 10);
                        if (value <= 0) AddValidationError(lineNum, L"SW must be positive", 2);
                    }
                } else if (wcsncmp(line, L"SH", 2) == 0) {
                    if (hasSH) AddValidationError(lineNum, L"Duplicate SH command", 2);
                    hasSH = 1;
                    if (wcslen(line) > 3) {
                        int value = wcstol(&line[3], NULL, 10);
                        if (value <= 0) AddValidationError(lineNum, L"SH must be positive", 2);
                    }
                } else if (wcsncmp(line, L"SC", 2) == 0) {
                    if (hasSC) AddValidationError(lineNum, L"Duplicate SC command", 2);
                    hasSC = 1;
                    if (wcslen(line) > 3) {
                        expectedSegments = wcstol(&line[3], NULL, 10);
                        if (expectedSegments <= 0) AddValidationError(lineNum, L"SC must be positive", 2);
                    }
                } else if (wcsncmp(line, L"SD", 2) == 0) {
                    if (hasSD) AddValidationError(lineNum, L"Duplicate SD command", 2);
                    hasSD = 1;
                    if (wcslen(line) > 3) {
                        int value = wcstol(&line[3], NULL, 10);
                        if (value < 0) AddValidationError(lineNum, L"SD cannot be negative", 2);
                    }
                } else if (wcsncmp(line, L"CD", 2) == 0) {
                    if (wcslen(line) > 3) {
                        int value = wcstol(&line[3], NULL, 10);
                        if (value < 0) AddValidationError(lineNum, L"CD cannot be negative", 2);
                    }
                
                /* OPTIONAL FLAGS */
                } else if (wcsncmp(line, L"TPF", 3) == 0) {
                    if (hasTPF) AddValidationError(lineNum, L"Duplicate TPF command", 1);
                    hasTPF = 1;
                    if (wcslen(line) > 4) {
                        int value = wcstol(&line[4], NULL, 10);
                        if (value <= 0) AddValidationError(lineNum, L"TPF (millis per frame) must be positive", 2);
                        if (value < 16) AddValidationError(lineNum, L"TPF below 16ms may cause performance issues", 1);
                    }
                } else if (wcsncmp(line, L"PM", 2) == 0) {
                    if (hasPM) AddValidationError(lineNum, L"Duplicate PM command", 1);
                    hasPM = 1;
                    if (wcslen(line) > 3) {
                        int value = wcstol(&line[3], NULL, 10);
                        if (value <= 0) AddValidationError(lineNum, L"PM (pixel movement per frame) must be positive", 2);
                        if (value > 20) AddValidationError(lineNum, L"PM above 20 pixels may scroll too fast", 1);
                    }
                
                } else if (wcscmp(line, L"START") == 0) {
                    if (inSegment) AddValidationError(lineNum, L"START inside another segment", 2);
                    inSegment = 1;
                } else if (wcscmp(line, L"END") == 0) {
                    if (!inSegment) AddValidationError(lineNum, L"END without START", 2);
                    inSegment = 0;
                    segmentCount++;
                } else if (inSegment) {
                    ValidateColorSyntax(line, lineNum);
                } else {
                    AddValidationError(lineNum, L"Text outside segment", 2);
                }
            }
            
            linePos = 0;
            lineNum++;
            
            if (c == L'\0') break;
        } else {
            if (linePos < 1023) {
                line[linePos++] = c;
            }
        }
    }
    
    /* Check required metadata */
    if (!hasLPS) AddValidationError(0, L"Missing LPS command", 2);
    if (!hasSW) AddValidationError(0, L"Missing SW command", 2);
    if (!hasSH) AddValidationError(0, L"Missing SH command", 2);
    if (!hasSC) AddValidationError(0, L"Missing SC command", 2);
    if (!hasSD) AddValidationError(0, L"Missing SD command", 2);
    
    /* Check segment count */
    if (expectedSegments != segmentCount) {
        wchar_t msg[256];
        swprintf(msg, 256, L"Expected %d segments, found %d", expectedSegments, segmentCount);
        AddValidationError(0, msg, 2);
    }
    
    if (inSegment) {
        AddValidationError(lineNum - 1, L"File ends with unclosed segment", 2);
    }
    
    free(buffer);
    return 1;
}

void PrintResults(void) {
    if (errorCount == 0) {
        wprintf(L"[i] Validation passed - No errors found\n");
        return;
    }
    
    /* Count errors by severity */
    int errors_count = 0, warnings_count = 0, info_count = 0;
    for (int i = 0; i < errorCount; i++) {
        switch (errors[i].severity) {
            case 0: info_count++; break;
            case 1: warnings_count++; break;
            case 2: errors_count++; break;
        }
    }
    
    wprintf(L"[x] Validation failed - %d issues found:\n", errorCount);
    wprintf(L"  Errors: %d, Warnings: %d, Info: %d\n\n", 
            errors_count, warnings_count, info_count);
    wprintf(L" +-----+------+------+---\n");
    wprintf(L" | Severity   | Line | Message\n");
    wprintf(L" +-----+------+------+---------\n");
    
    /* Print all errors */
    for (int i = 0; i < errorCount; i++) {
        const wchar_t* severityStr;
        const wchar_t* icon;
        switch (errors[i].severity) {
            case 0: severityStr = L"INFO"; icon = L"[i]"; break;
            case 1: severityStr = L"WARN"; icon = L"[!]"; break;
            case 2: severityStr = L"ERR "; icon = L"[x]"; break;
            default: severityStr = L" ?? "; icon = L"[?]"; break;
        }
        
        if (errors[i].lineNumber > 0) {
            wprintf(L" | %3ls | %4ls | %4d | %ls\n", 
                   icon, severityStr, errors[i].lineNumber, errors[i].message);
        } else {
            wprintf(L" | %3ls | %4ls |      | %ls\n", 
                   icon, severityStr, errors[i].message);
        }
    }
    
    wprintf(L" +-----+------+------+---------\n");
}

int main(int argc, char* argv[]) {
    /* Set locale for wide character output */
    setlocale(LC_ALL, "");
    
#ifdef _WIN32
    /* Enable Unicode output on Windows console */
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);
#endif
    
    if (argc != 2) {
        wprintf(L"Usage: %s <filename.mly>\n", argv[0]);
        wprintf(L"Validates a Marquee Layout file and reports any syntax errors.\n");
        return 1;
    }
    
    wprintf(L"Validating file: %s\n\n", argv[1]);
    
    if (!ValidateFile(argv[1])) {
        return 1;
    }
    
    PrintResults();
    
    /* Return exit code based on validation results */
    int exit_code = 0;
    for (int i = 0; i < errorCount; i++) {
        if (errors[i].severity == 2) { /* Error */
            exit_code = 1;
            break;
        }
    }
    
    return exit_code;
}