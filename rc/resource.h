#ifndef RESOURCE_H
#define RESOURCE_H
#include <windows.h>

#define IDI_APPICON 1000

#ifdef EDITOR

// Control IDs
#define IDC_EDIT_MAIN 1001
#define IDC_LIST_ERRORS 1002
#define IDC_STATUS 1009

// Menu IDs
#define IDM_FILE_NEW 2001
#define IDM_FILE_OPEN 2002
#define IDM_FILE_SAVE 2003
#define IDM_FILE_SAVEAS 2004
#define IDM_FILE_EXIT 2005
#define IDM_TOOLS_VALIDATE 2006
#define IDM_TOOLS_PREVIEW 2007
#define IDM_HELP_ABOUT 2008

// Accelerator IDs (same as menu IDs for simplicity)
#define IDA_FILE_NEW IDM_FILE_NEW
#define IDA_FILE_OPEN IDM_FILE_OPEN
#define IDA_FILE_SAVE IDM_FILE_SAVE
#define IDA_TOOLS_VALIDATE IDM_TOOLS_VALIDATE
#define IDA_TOOLS_PREVIEW IDM_TOOLS_PREVIEW

// Resource IDs
#define IDR_MAINMENU 3001
#define IDR_ACCELERATOR 3002
//#define IDI_APPICON 3003

#endif

#ifdef RENDERER

#endif

#ifdef VALIDATE

#endif

#endif
