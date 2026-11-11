// SNKHD - shim tchar (FBNeo usa TCHAR = char fuori da Windows UNICODE)
#ifndef SNKHD_TCHAR_H
#define SNKHD_TCHAR_H
#include <stdio.h>
#include <string.h>
typedef char TCHAR;
#ifndef _T
#define _T(x) x
#endif
#define _stprintf sprintf
#define _tfopen fopen
#define _tcslen strlen
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcscmp strcmp
#define _tcsicmp strcasecmp
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#ifndef __fastcall
#define __fastcall
#endif
#endif
