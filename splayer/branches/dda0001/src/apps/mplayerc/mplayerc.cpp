﻿/* 
 *	Copyright (C) 2003-2006 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

// mplayerc.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include <imagehlp.h>

#include "mplayerc.h"
#include <atlsync.h>
#include <Tlhelp32.h>
#include "MainFrm.h"
#include "..\..\DSUtil\DSUtil.h"
#include "revision.h"
#include "ChkDefPlayer.h"
#include <locale.h> 
#include <d3d9.h>
#include <d3dx9.h>
#include "DlgChkUpdater.h"
#include <dsound.h>

#include "..\..\..\Updater\cupdatenetlib.h"

#include "DisplaySettingDetector.h"

#include "../../filters/transform/mpcvideodec/CpuId.h"

//#define  SPI_GETDESKWALLPAPER 115

#include "..\..\filters\transform\mpadecfilter\MpaDecFilter.h"

//Update URL
char* szUrl = "http://svplayer.shooter.cn/api/updater.php";

/////////
typedef BOOL (WINAPI* MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
										 CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
										 CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
										 CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);
static LONG WINAPI  DebugMiniDumpFilter( struct _EXCEPTION_POINTERS *pExceptionInfo )
{
	LONG retval = EXCEPTION_CONTINUE_SEARCH;
	HWND hParent = NULL;                        // find a better value for your app

	// firstly see if dbghelp.dll is around and has the function we need
	// look next to the EXE first, as the one in System32 might be old
	// (e.g. Windows 2000)
	HMODULE hDll = NULL;
	TCHAR szDbgHelpPath[_MAX_PATH];

	if (GetModuleFileName( NULL, szDbgHelpPath, _MAX_PATH ))
	{
		TCHAR *pSlash = _tcsrchr( szDbgHelpPath, _T('\\') );
		if (pSlash)
		{
			_tcscpy(pSlash+1, _T("DBGHELP.DLL"));
			hDll = ::LoadLibrary( szDbgHelpPath );
		}
	}

	if (hDll==NULL)
	{
		// load any version we can
		hDll = ::LoadLibrary(_T("DBGHELP.DLL"));
	}

	LPCTSTR szResult = NULL;

	if (hDll)
	{
		MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)::GetProcAddress(hDll,"MiniDumpWriteDump");
		if (pDump)
		{
			TCHAR szDumpPath[_MAX_PATH];
			TCHAR szDumpName[_MAX_PATH] = {0};
			TCHAR szScratch [_MAX_PATH];


			int itimestamp = (int)time(NULL)/5;
			TCHAR szTimestamp[_MAX_PATH];
			_itow_s(itimestamp, szTimestamp,_MAX_PATH, 36);

			// work out a good place for the dump file
			//_tgetcwd(szDumpPath,_MAX_PATH);
			//_tcscat( szDumpPath, _T("\\"));

			_tcscat( szDumpName, _T("splayer_") );
			_tcscat( szDumpName, SVP_REV_STR );
			_tcscat( szDumpName, _T("_"));
			_tcscat( szDumpName, szTimestamp );
			_tcscat( szDumpName, _T(".dmp"));

			GetModuleFileName( NULL, szDumpPath, _MAX_PATH );
			wcscpy( PathFindFileName(szDumpPath), szDumpName);

			// ask the user if they want to save a dump file
			//if (::MessageBox(NULL,_T("程序发生意外,是否保存一个文件用于诊断?"), ResStr(IDR_MAINFRAME) ,MB_YESNO)==IDYES)
			{
				// create the file
                HANDLE hFile = ::CreateFile( szDumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW,
					FILE_ATTRIBUTE_NORMAL, NULL );

				if (hFile!=INVALID_HANDLE_VALUE)
				{
					_MINIDUMP_EXCEPTION_INFORMATION ExInfo;
					ExInfo.ThreadId = ::GetCurrentThreadId();
					ExInfo.ExceptionPointers = pExceptionInfo;
					ExInfo.ClientPointers = NULL;

					// write the dump
					BOOL bOK = pDump( GetCurrentProcess(),GetCurrentProcessId(),hFile,MiniDumpNormal,&ExInfo,NULL,NULL);
					if (bOK)
					{
						_stprintf( szScratch, ResStr(IDS_MSG_CRASH_DUMPED), szDumpPath );
						szResult = szScratch;
						retval = EXCEPTION_EXECUTE_HANDLER;
						{
							TCHAR sUpdaterPath[_MAX_PATH];
							TCHAR sExePath[_MAX_PATH];
							TCHAR sUpPerm[_MAX_PATH];
							if (GetModuleFileName( NULL, sExePath, _MAX_PATH ))
							{
								GetModuleFileName( NULL, sUpdaterPath, _MAX_PATH );

								wcscpy( PathFindFileName(sUpdaterPath), _T("Updater.exe"));
								_stprintf( sUpPerm, _T(" /dmp splayer_%s_%s.dmp "), SVP_REV_STR ,szTimestamp);
								//(int)::ShellExecute(NULL, _T("open"), sUpdaterPath, sUpPerm, NULL, SW_HIDE);

								//(int)::ShellExecute(NULL, _T("open"), sExePath, L" /fromdmp", NULL, SW_SHOW);
								
								SVP_LogMsg5(L"crash dumped %s %s %s", sUpdaterPath , sUpPerm , sExePath);

							}
						}
					}
					else
					{
						_stprintf( szScratch, ResStr(IDS_MSG_SAVE_CRASH_DUMP_FAIL), szDumpPath, GetLastError() );
						szResult = szScratch;
					}
					::CloseHandle(hFile);
				}
				else
				{
					_stprintf( szScratch, ResStr(IDS_MSG_CRASH_DUMP_CREATION_FAILED), szDumpPath, GetLastError() );
					szResult = szScratch;
				}
			}
		}
		else
		{
			szResult = ResStr(IDS_MSG_DBGHELP_DLL_IS_TOO_OLD);
		}
	}
	else
	{
		szResult = ResStr(IDS_MSG_DBGHELP_DLL_NOT_EXIST);
	}

	if (szResult)
	{
		//::MessageBox( NULL, szResult, ResStr(IDR_MAINFRAME), MB_OK );
	}

	
	return retval;
}
static LPCTSTR DebugMiniDumpProcess( HANDLE pProcess, DWORD pId ,DWORD dwTid)
{
	LONG retval = EXCEPTION_CONTINUE_SEARCH;
	HWND hParent = NULL;                        // find a better value for your app

	// firstly see if dbghelp.dll is around and has the function we need
	// look next to the EXE first, as the one in System32 might be old
	// (e.g. Windows 2000)
	HMODULE hDll = NULL;
	TCHAR szDbgHelpPath[_MAX_PATH];

	if (GetModuleFileName( NULL, szDbgHelpPath, _MAX_PATH ))
	{
		TCHAR *pSlash = _tcsrchr( szDbgHelpPath, _T('\\') );
		if (pSlash)
		{
			_tcscpy(pSlash+1, _T("DBGHELP.DLL"));
			hDll = ::LoadLibrary( szDbgHelpPath );
		}
	}

	if (hDll==NULL)
	{
		// load any version we can
		hDll = ::LoadLibrary(_T("DBGHELP.DLL"));
	}

	LPCTSTR szResult = NULL;

	if (hDll)
	{
		MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)::GetProcAddress(hDll,"MiniDumpWriteDump");
		if (pDump)
		{
			TCHAR szDumpPath[_MAX_PATH];
			TCHAR szScratch [_MAX_PATH];

			// work out a good place for the dump file
			_tgetcwd(szDumpPath,_MAX_PATH);
			_tcscat( szDumpPath, _T("\\"));
			int itimestamp = (int)time(NULL)/5;
			TCHAR szTimestamp[_MAX_PATH];
			_itow_s(itimestamp, szTimestamp,_MAX_PATH, 36);

			_tcscat( szDumpPath, _T("splayer_hanged_") );
			_tcscat( szDumpPath, SVP_REV_STR );
			_tcscat( szDumpPath, _T("_"));
			_tcscat( szDumpPath, szTimestamp);
			_tcscat( szDumpPath, _T(".dmp"));

			// ask the user if they want to save a dump file
			//if (::MessageBox(NULL,_T("程序发生意外,是否保存一个文件用于诊断?"), ResStr(IDR_MAINFRAME) ,MB_YESNO)==IDYES)
			{
				// create the file
                HANDLE hFile = ::CreateFile( szDumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW,
					FILE_ATTRIBUTE_NORMAL, NULL );

				if (hFile!=INVALID_HANDLE_VALUE)
				{
					/*
_EXCEPTION_POINTERS ExceptionInfo;
					_MINIDUMP_EXCEPTION_INFORMATION ExInfo;
					ExInfo.ThreadId = dwTid;
					ExInfo.ExceptionPointers = &ExceptionInfo;
					ExInfo.ClientPointers = NULL;
*/

					// write the dump
					BOOL bOK = pDump( pProcess, pId,hFile,
						 MINIDUMP_TYPE(MiniDumpWithDataSegs  | MiniDumpWithIndirectlyReferencedMemory | MiniDumpFilterModulePaths)
						 ,NULL,NULL,NULL);
					if (bOK)
					{
						_stprintf( szScratch, ResStr(IDS_MSG_HANGED_DUMP_CREATED), szDumpPath );
						szResult = szScratch;
						retval = EXCEPTION_EXECUTE_HANDLER;
						{
							TCHAR sUpdaterPath[_MAX_PATH];
							TCHAR sUpPerm[_MAX_PATH];
							if (GetModuleFileName( NULL, sUpdaterPath, _MAX_PATH ))
							{
								
								wcscpy( PathFindFileName(sUpdaterPath), _T("Updater.exe"));
								_stprintf( sUpPerm, _T(" /dmp splayer_hanged_%s_%s.dmp "), SVP_REV_STR ,szTimestamp);
								(int)::ShellExecute(NULL, _T("open"), sUpdaterPath, sUpPerm, NULL, SW_HIDE);

								
							}
						}
					}
					else
					{
						_stprintf( szScratch, ResStr(IDS_MSG_HANG_DUMP_SAVE_FAILED), szDumpPath, GetLastError() );
						szResult = szScratch;
					}
					::CloseHandle(hFile);
				}
				else
				{
					_stprintf( szScratch, ResStr(IDS_MSG_HANG_DUMP_CREATE_FAILED), szDumpPath, GetLastError() );
					szResult = szScratch;
				}
			}
		}
		else
		{
			szResult = ResStr(IDS_MSG_DBGHELP_DLL_IS_TOO_OLD);
		}
	}
	else
	{
		szResult = ResStr(IDS_MSG_DBGHELP_DLL_NOT_EXIST);
	}

	if (szResult)
	{
		//::MessageBox( NULL, szResult, ResStr(IDR_MAINFRAME), MB_OK );
	}


	return szResult;
}

int IsInsideVM(){
	//这个方法似乎是检测虚拟机的一个简单有效的方法，虽然还不能确定它是否是100%有效。名字很有意思，红色药丸（为什么不是bluepill,哈哈）。我在网上找到了个ppt专门介绍这个方法，可惜现在翻不到了。记忆中原理是这样的，主要检测IDT的数  值，如果这个数值超过了某个数值，我们就可以认为应用程序处于虚拟环境中，似乎这个方法在多CPU的机器中并不可靠。据称ScoobyDoo方法是RedPill的升级版。代码也是在网上找的，做了点小改动。有四种返回结果，可以确认是VMWare，还是  VirtualPC，还是其它VME，或是没有处于VME中。
	//return value: 0:none,1:vmvare;2:vpc;3:others
	unsigned char matrix[6];

	unsigned char redpill[] = 
		"\x0f\x01\x0d\x00\x00\x00\x00\xc3";

	HANDLE hProcess = GetCurrentProcess();

	LPVOID lpAddress = NULL;
	PDWORD lpflOldProtect = NULL;

	__try
	{
		*((unsigned*)&redpill[3]) = (unsigned)matrix;

		lpAddress = VirtualAllocEx(hProcess, NULL, 6, MEM_RESERVE|MEM_COMMIT , PAGE_EXECUTE_READWRITE);

		if(lpAddress == NULL)
			return 0;

		BOOL success = VirtualProtectEx(hProcess, lpAddress, 6, PAGE_EXECUTE_READWRITE , lpflOldProtect);

		if(success != 0)
			return 0;

		memcpy(lpAddress, redpill, 8);

		((void(*)())lpAddress)();

		if (matrix[5]>0xd0) 
		{
			if(matrix[5]==0xff)//vmvare
				return 1;
			else if(matrix[5]==0xe8)//vitualpc
				return 2;
			else
				return 3;
		}
		else 
			return 0;
	}
	__finally
	{
		VirtualFreeEx(hProcess, lpAddress, 0, MEM_RELEASE);
	} 
}

void CorrectComboListWidth(CComboBox& box, CFont* pWndFont)
{
	int cnt = box.GetCount();
	if(cnt <= 0) return;

	CDC* pDC = box.GetDC();
	pDC->SelectObject(pWndFont);

	int maxw = box.GetDroppedWidth();

	for(int i = 0; i < cnt; i++)
	{
		CString str;
		box.GetLBText(i, str);
		int w = pDC->GetTextExtent(str).cx + 22;
		if(maxw < w) maxw = w;
	}

	box.ReleaseDC(pDC);

	box.SetDroppedWidth(maxw);
}

HICON LoadIcon(CString fn, bool fSmall)
{
	if(fn.IsEmpty()) return(NULL);

	CString ext = fn.Left(fn.Find(_T("://"))+1).TrimRight(':');
	if(ext.IsEmpty() || !ext.CompareNoCase(_T("file")))
		ext = _T(".") + fn.Mid(fn.ReverseFind('.')+1);

	CSize size(fSmall?16:32,fSmall?16:32);

	if(!ext.CompareNoCase(_T(".ifo")))
	{
		if(HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_DVD), IMAGE_ICON, size.cx, size.cy, 0))
			return(hIcon);
	}

	if(!ext.CompareNoCase(_T(".cda")))
	{
		if(HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_AUDIOCD), IMAGE_ICON, size.cx, size.cy, 0))
			return(hIcon);
	}

	do
	{
		CRegKey key;

		TCHAR buff[256];
		ULONG len;

		if(ERROR_SUCCESS != key.Open(HKEY_CLASSES_ROOT, ext + _T("\\DefaultIcon"), KEY_READ))
		{
			if(ERROR_SUCCESS != key.Open(HKEY_CLASSES_ROOT, ext, KEY_READ))
				break;

			len = sizeof(buff);
			memset(buff, 0, len);
			if(ERROR_SUCCESS != key.QueryStringValue(NULL, buff, &len) || (ext = buff).Trim().IsEmpty())
				break;

			if(ERROR_SUCCESS != key.Open(HKEY_CLASSES_ROOT, ext + _T("\\DefaultIcon"), KEY_READ))
				break;
		}

		CString icon;

		len = sizeof(buff);
		memset(buff, 0, len);
		if(ERROR_SUCCESS != key.QueryStringValue(NULL, buff, &len) || (icon = buff).Trim().IsEmpty())
			break;

		int i = icon.ReverseFind(',');
		if(i < 0) break;
		
		int id = 0;
		if(_stscanf(icon.Mid(i+1), _T("%d"), &id) != 1)
			break;

		icon = icon.Left(i);

		HICON hIcon = NULL;
		UINT cnt = fSmall 
			? ExtractIconEx(icon, id, NULL, &hIcon, 1)
			: ExtractIconEx(icon, id, &hIcon, NULL, 1);
		if(hIcon) return hIcon;
	}
	while(0);

	return((HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_UNKNOWN), IMAGE_ICON, size.cx, size.cy, 0));
}

bool LoadType(CString fn, CString& type)
{
	CRegKey key;

	TCHAR buff[256];
	ULONG len;

	if(fn.IsEmpty()) return(NULL);

	CString ext = fn.Left(fn.Find(_T("://"))+1).TrimRight(':');
	if(ext.IsEmpty() || !ext.CompareNoCase(_T("file")))
		ext = _T(".") + fn.Mid(fn.ReverseFind('.')+1);

	if(ERROR_SUCCESS != key.Open(HKEY_CLASSES_ROOT, ext))
		return(false);

	CString tmp = ext;

    while(ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, tmp))
	{
		len = sizeof(buff);
		memset(buff, 0, len);
		if(ERROR_SUCCESS != key.QueryStringValue(NULL, buff, &len))
			break;

		CString str(buff);
		str.Trim();

		if(str.IsEmpty() || str == tmp)
			break;

		tmp = str;
	}

	type = tmp;

	return(true);
}


bool LoadResource(UINT resid, CStringA& str, LPCTSTR restype)
{
	HINSTANCE iInstance = AfxGetResourceHandle();

	str.Empty();
	HRSRC hrsrc = FindResource(iInstance, MAKEINTRESOURCE(resid), restype);
	if(!hrsrc) {
		iInstance = AfxGetApp()->m_hInstance;
		hrsrc = FindResource(iInstance, MAKEINTRESOURCE(resid), restype);
		if(!hrsrc)
			return(false);
	}
	HGLOBAL hGlobal = LoadResource(iInstance, hrsrc);
	if(!hGlobal) return(false);
	DWORD size = SizeofResource(iInstance, hrsrc);
	if(!size) return(false);
	memcpy(str.GetBufferSetLength(size), LockResource(hGlobal), size);
	return(true);
}

bool UnRegSvr32Real(LPCTSTR szDllPath){
	//LoadLibrary(path))
	HMODULE h = NULL;
	__try{
		h = LoadLibraryEx(  szDllPath , 0, LOAD_WITH_ALTERED_SEARCH_PATH);
	}__except(EXCEPTION_EXECUTE_HANDLER) {  }
	if(h)
	{

		typedef HRESULT (__stdcall * PDllRegisterServer)();
		if(PDllRegisterServer p = (PDllRegisterServer)GetProcAddress(h, "DllUnRegisterServer"))
		{
			__try{
				p();
			}__except(EXCEPTION_EXECUTE_HANDLER) {  }
		}
		__try{
			FreeLibrary(h);
		}__except(EXCEPTION_EXECUTE_HANDLER) {  }
	}
	return true;
}
bool UnRegSvr32(CString szDllPath){
	bool ret = UnRegSvr32Real(szDllPath.GetBuffer());
	szDllPath.ReleaseBuffer();
	return ret;
}
bool RegSvr32Real(LPCTSTR szDllPath){
	//LoadLibrary(path))
	HMODULE h = NULL;
	__try{
		h = LoadLibraryEx(  szDllPath , 0, LOAD_WITH_ALTERED_SEARCH_PATH);
	}__except(EXCEPTION_EXECUTE_HANDLER) {  }
	if(h)
	{
		
			typedef HRESULT (__stdcall * PDllRegisterServer)();
			if(PDllRegisterServer p = (PDllRegisterServer)GetProcAddress(h, "DllRegisterServer"))
			{
				__try{
					p();
				}__except(EXCEPTION_EXECUTE_HANDLER) {  }
			}
		__try{
			FreeLibrary(h);
		}__except(EXCEPTION_EXECUTE_HANDLER) {  }
	}
	return true;
}
bool RegSvr32(CString szDllPath){
	bool ret = RegSvr32Real(szDllPath.GetBuffer());
	szDllPath.ReleaseBuffer();
	return ret;
}
/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	CString m_appname;
	CString m_strRevision;
	virtual BOOL OnInitDialog()
	{
		UpdateData();
//#ifdef UNICODE
	//	m_appname += _T(" (unicode build)");
//#endif
		//cs_version.GetWindowText(m_strRevision);
		CString path;
		GetModuleFileName(NULL, path.GetBuffer(MAX_PATH), MAX_PATH);
		path.ReleaseBuffer();
		
			DWORD             dwHandle;
			UINT              dwLen;
//			UINT              uLen;
			UINT              cbTranslate = sizeof(VS_FIXEDFILEINFO);
//			LPVOID            lpBuffer;

			dwLen  = GetFileVersionInfoSize(path, &dwHandle);

			TCHAR * lpData = (TCHAR*) malloc(dwLen);
			if(!lpData)
				return TRUE;
			memset((char*)lpData, 0 , dwLen);
			VS_FIXEDFILEINFO* lpVerinfo;

			/* GetFileVersionInfo() requires a char *, but the api doesn't
			* indicate that it will modify it */
			if(GetFileVersionInfo(path, dwHandle, dwLen, lpData) != 0)
			{
				VerQueryValue(lpData, 
					TEXT("\\"),
					(LPVOID*)&lpVerinfo,
					&cbTranslate);

				// Read the file description for each language and code page.

				
			}
		
			m_strRevision.Format(L"%s: %d.%d (Build %s)",ResStr( IDS_ABOUT_DIALOG_VERSION_LABEL ) , HIWORD(lpVerinfo->dwProductVersionMS),
				LOWORD(lpVerinfo->dwProductVersionMS) ,  SVP_REV_STR );
		UpdateData(FALSE);
		return TRUE;
	}
	CStatic cs_version;
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD), m_appname(_T("")), m_strRevision(_T(""))
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
	DDX_Text(pDX, IDC_STATIC1, m_appname);
	DDX_Text(pDX, IDC_VERSION, m_strRevision);
	DDX_Control(pDX, IDC_VERSION, cs_version);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMPlayerCApp

BEGIN_MESSAGE_MAP(CMPlayerCApp, CWinApp)
	//{{AFX_MSG_MAP(CMPlayerCApp)
	ON_COMMAND(ID_HELP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_FILE_EXIT, OnFileExit)
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_HELP_SHOWCOMMANDLINESWITCHES, OnHelpShowcommandlineswitches)
END_MESSAGE_MAP()
void myInvalidParameterHandler(const wchar_t* expression,
							   const wchar_t* function, 
							   const wchar_t* file, 
							   unsigned int line, 
							   uintptr_t pReserved)
{
	SVP_LogMsg5(L"Invalid parameter detected in function %s."
		L" File: %s Line: %d\n", function, file, line);
	SVP_LogMsg5(L"Expression: %s\n", expression);
}



/////////////////////////////////////////////////////////////////////////////
// CMPlayerCApp construction
const UINT WM_MOUSEMOVEIN = ::RegisterWindowMessage(_T("WM_MOUSEMOVEIN"));
const UINT WM_MOUSEMOVEOUT = ::RegisterWindowMessage(_T("WM_MOUSEMOVEOUT"));
int CMPlayerCApp::m_isVista = -1;
int CMPlayerCApp::m_bCanUseCUDA = -1;
int CMPlayerCApp::m_bHasEVRSupport = -1;
HMODULE	CMPlayerCApp::m_hResDll = NULL;

CMPlayerCApp::CMPlayerCApp()
//	: m_hMutexOneInstance(NULL)
:  m_bMouseIn(FALSE)      // doesn't matter because don't know yet
, m_bMouseInOutUnknown(TRUE)      // don't know whether in or out yet
, m_bGenerateMouseInOutMessages(TRUE) 
, m_cnetupdater(NULL)
, m_fDisplayStats(FALSE)
, m_fTearingTest(0)
, m_fResetStats(0)
, sqlite_setting(NULL)
,sqlite_local_record(NULL)
{
	m_pMainWnd = NULL;
	_invalid_parameter_handler oldHandler, newHandler;
	newHandler = myInvalidParameterHandler;
	oldHandler = _set_invalid_parameter_handler(newHandler);
	
	// Disable the message box for assertions.
	_CrtSetReportMode(_CRT_ASSERT, 0);

	::SetUnhandledExceptionFilter(DebugMiniDumpFilter);

	_set_SSE2_enable(1);

	QueryPerformanceFrequency ((LARGE_INTEGER*)&m_PerfFrequency);

}

BOOL CMPlayerCApp::PumpMessage() {
	BOOL ok = CWinApp::PumpMessage();
	
	if (ok && m_bGenerateMouseInOutMessages) {
			//If mouse is in then check if it has gone out. If
			//mouse is not in then check if it has come in.
			MSG m_msgCur;
			CWnd* pMainWnd = ::AfxGetMainWnd();
			CMainFrame* pFrame = (CMainFrame*)pMainWnd;
			if (pMainWnd) {
				//As long as there is no message for this application
				//track the mouse cursor position.
				while(!PeekMessage(&m_msgCur, 0, 0, 0, PM_NOREMOVE)) {
					
						CPoint pt; GetCursorPos(&pt);
						CWnd* pMsgWnd = CWnd::WindowFromPoint(pt);
						//If window at mouse cursor position is not this
						//app's window and not any of its child windows
						//then it means mouse has left the app area.
						m_bMouseInOutUnknown = FALSE;
						if ( pMsgWnd != pMainWnd && !pMainWnd->IsChild(pMsgWnd)
							&& pMsgWnd != &(pFrame->m_wndToolTopBar) ) {

								if(m_s.bUserAeroUI() ){
									if( pMsgWnd == (pFrame->m_wndFloatToolBar) || pFrame->m_wndFloatToolBar->IsChild(pMsgWnd) )
										break;
								}
							if( (m_bMouseIn || m_bMouseInOutUnknown) ){
								m_bMouseIn = FALSE;
								pMainWnd->PostMessage(WM_MOUSEMOVEOUT, 0, 0L);
							}
							break;
						}else if( !m_bMouseIn|| m_bMouseInOutUnknown){
							m_bMouseIn = TRUE;
							pMainWnd->PostMessage(WM_MOUSEMOVEIN, 0, 0L);
							break;
						}
						break;
					
				}
			}
		}
	
	return ok;
}
void CMPlayerCApp::ShowCmdlnSwitches()
{
	CString s;

	if(m_s.nCLSwitches&CLSW_UNRECOGNIZEDSWITCH)
	{
		CAtlList<CString> sl;
		for(int i = 0; i < __argc; i++) sl.AddTail(__targv[i]);
		s += "Unrecognized switch(es) found in command line string: \n\n" + Implode(sl, ' ') + "\n\n";
	}

	s +=
		_T("Usage: splayer.exe \"pathname\" [switches]\n\n")
		_T("\"pathname\"\tThe main file or directory to be loaded. (wildcards allowed)\n")
		_T("/dub \"dubname\"\tLoad an additional audio file.\n")
		_T("/sub \"subname\"\tLoad an additional subtitle file.\n")
		_T("/filter \"filtername\"\tLoad DirectShow filters from a dynamic link library. (wildcards allowed)\n")
		_T("/dvd\t\tRun in dvd mode, \"pathname\" means the dvd folder (optional).\n")
		_T("/cd\t\tLoad all the tracks of an audio cd or (s)vcd, \"pathname\" means the drive path (optional).\n")
		_T("/open\t\tOpen the file, don't automatically start playing.\n")
		_T("/play\t\tStart playing the file as soon the player is launched.\n")
		_T("/close\t\tClose the player after playback (only works when used with /play).\n")
		_T("/shutdown\tShutdown the operating system after playback\n")
		_T("/fullscreen\tStart in full-screen mode.\n")
		_T("/minimized\tStart in minimized mode.\n")
		_T("/new\t\tUse a new instance of the player.\n")
		_T("/add\t\tAdd \"pathname\" to playlist, can be combined with /open and /play.\n")
		_T("/regvid\t\tCreate file associations for video files.\n")
		_T("/regaud\t\tCreate file associations for audio files.\n")
		_T("/unregall\t\tRemove all file associations.\n")
		_T("/start ms\t\tStart playing at \"ms\" (= milliseconds)\n")
		_T("/fixedsize w,h\tSet fixed window size.\n")
		_T("/monitor N\tStart on monitor N, where N starts from 1.\n")
		_T("/help /h /?\tShow help about command line switches. (this message box)\n");

	AfxMessageBox(s);
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CMPlayerCApp object

CMPlayerCApp theApp;

HWND g_hWnd = NULL;

bool CMPlayerCApp::StoreSettingsToIni()
{

	CString ini = GetIniPath();
	CSVPToolBox svpTool;
    
	if( !sqlite_setting){ 
		
		int iDescLen;
		char * buff = svpTool.CStringToUTF8(ini,&iDescLen) ;
		sqlite_setting = new SQLITE3( buff );
		free(buff);
		if(!sqlite_setting->db_open){
			delete sqlite_setting;
			sqlite_setting = NULL;
		}
        //AfxMessageBox(L"3");
	}
	
	if(sqlite_setting){
		sqlite_setting->exec_sql("CREATE TABLE IF NOT EXISTS \"settingint\" ( \"hkey\" TEXT,  \"sect\" TEXT,  \"sval\" INTEGER)");
		sqlite_setting->exec_sql("CREATE TABLE IF NOT EXISTS \"settingstring\" (  \"hkey\" TEXT,   \"sect\" TEXT,   \"vstring\" TEXT)");
		sqlite_setting->exec_sql("CREATE TABLE IF NOT EXISTS \"settingbin2\" (   \"skey\" TEXT,   \"sect\" TEXT,   \"vdata\" BLOB)");
		sqlite_setting->exec_sql("CREATE UNIQUE INDEX IF NOT EXISTS \"pkey\" on settingint (hkey ASC, sect ASC)");
		sqlite_setting->exec_sql("CREATE UNIQUE INDEX IF NOT EXISTS \"pkeystring\" on settingstring (hkey ASC, sect ASC)");
		sqlite_setting->exec_sql("CREATE UNIQUE INDEX IF NOT EXISTS \"pkeybin\" on settingbin2 (skey ASC, sect ASC)");
		sqlite_setting->exec_sql("PRAGMA synchronous=OFF");
		sqlite_setting->exec_sql("DROP TABLE  IF EXISTS  \"settingbin\""); //old wrong one
        //AfxMessageBox(L"4");
		/*
		int iwriteorg = sqlite_setting->GetProfileInt(ResStr(IDS_R_SETTINGS), L"writedetect", 0, false);
		sqlite_setting->WriteProfileInt(ResStr(IDS_R_SETTINGS), L"writedetect", iwriteorg+1,false);
		int iwritenew = sqlite_setting->GetProfileInt(ResStr(IDS_R_SETTINGS), L"writedetect", 0,false);

		if(iwritenew != (iwriteorg+1)){
			delete sqlite_setting;
			sqlite_setting = NULL;
			return false;
		}
		*/
		return(true);
	}

	return StoreSettingsToRegistry();
/*
	
		CString ini = GetIniPath();
	
		FILE* f;
		if(!(f = _tfopen(ini, _T("r+"))) && !(f = _tfopen(ini, _T("w"))))
			return StoreSettingsToRegistry();
		fclose(f);
	
		if(m_pszRegistryKey) free((void*)m_pszRegistryKey);
		m_pszRegistryKey = NULL;
		if(m_pszProfileName) free((void*)m_pszProfileName);
		m_pszProfileName = _tcsdup(ini);
	
		return(true);*/
	
}

bool CMPlayerCApp::StoreSettingsToRegistry()
{
	//_tremove(GetIniPath());

	if(m_pszRegistryKey) free((void*)m_pszRegistryKey);
	m_pszRegistryKey = NULL;


	CRegKey key, key1;
	if(ERROR_SUCCESS == key.Open(HKEY_CURRENT_USER, _T("Software\\SVPlayer"), KEY_READ) && ERROR_SUCCESS != key.Open(HKEY_CURRENT_USER, _T("Software\\SPlayer"), KEY_READ))
	{

		SetRegistryKey(_T("SVPlayer"));
	}else{
		SetRegistryKey(_T("SPlayer"));
	}
	

	return(true);
}

CString CMPlayerCApp::GetIniPath()
{
	//CString path;
	//GetModuleFileName(AfxGetInstanceHandle(), path.GetBuffer(MAX_PATH), MAX_PATH);
	//path.ReleaseBuffer();
	//path = path.Left(path.ReverseFind('.')+1) + _T("ini");
	CSVPToolBox svpTool;
	
	return(svpTool.GetPlayerPath(L"settings.db"));
}

bool CMPlayerCApp::IsIniValid()
{
	//return FALSE;
	CFileStatus fs;
	return CFileGetStatus(GetIniPath(), fs) && fs.m_size > 0;
}
#include <strsafe.h>
#pragma comment(lib , "strsafe.lib")
//*************************************************************
//
//  RegDelnodeRecurse()
//
//  Purpose:    Deletes a registry key and all its subkeys / values.
//
//  Parameters: hKeyRoot    -   Root key
//              lpSubKey    -   SubKey to delete
//
//  Return:     TRUE if successful.
//              FALSE if an error occurs.
//
//*************************************************************

BOOL CMPlayerCApp::RegDelnodeRecurse (HKEY hKeyRoot, LPTSTR lpSubKey)
{
	LPTSTR lpEnd;
	LONG lResult;
	DWORD dwSize;
	TCHAR szName[MAX_PATH];
	HKEY hKey;
	FILETIME ftWrite;

	// First, see if we can delete the key without having
	// to recurse.

	lResult = RegDeleteKey(hKeyRoot, lpSubKey);

	if (lResult == ERROR_SUCCESS) 
		return TRUE;

	lResult = RegOpenKeyEx (hKeyRoot, lpSubKey, 0, KEY_READ, &hKey);

	if (lResult != ERROR_SUCCESS) 
	{
		if (lResult == ERROR_FILE_NOT_FOUND) {
			printf("Key not found.\n");
			return TRUE;
		} 
		else {
			printf("Error opening key.\n");
			return FALSE;
		}
	}

	// Check for an ending slash and add one if it is missing.

	lpEnd = lpSubKey + lstrlen(lpSubKey);

	if (*(lpEnd - 1) != TEXT('\\')) 
	{
		*lpEnd =  TEXT('\\');
		lpEnd++;
		*lpEnd =  TEXT('\0');
	}

	// Enumerate the keys

	dwSize = MAX_PATH;
	lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
		NULL, NULL, &ftWrite);

	if (lResult == ERROR_SUCCESS) 
	{
		do {

			StringCchCopy (lpEnd, MAX_PATH*2, szName);

			if (!RegDelnodeRecurse(hKeyRoot, lpSubKey)) {
				break;
			}

			dwSize = MAX_PATH;

			lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
				NULL, NULL, &ftWrite);

		} while (lResult == ERROR_SUCCESS);
	}

	lpEnd--;
	*lpEnd = TEXT('\0');

	RegCloseKey (hKey);

	// Try again to delete the key.

	lResult = RegDeleteKey(hKeyRoot, lpSubKey);

	if (lResult == ERROR_SUCCESS) 
		return TRUE;

	return FALSE;
}

//*************************************************************
//
//  RegDelnode()
//
//  Purpose:    Deletes a registry key and all its subkeys / values.
//
//  Parameters: hKeyRoot    -   Root key
//              lpSubKey    -   SubKey to delete
//
//  Return:     TRUE if successful.
//              FALSE if an error occurs.
//
//*************************************************************

BOOL CMPlayerCApp::RegDelnode (HKEY hKeyRoot, LPTSTR lpSubKey)
{
	TCHAR szDelKey[2 * MAX_PATH];

	StringCchCopy (szDelKey, MAX_PATH*2, lpSubKey);
	return RegDelnodeRecurse(hKeyRoot, szDelKey);

}



void CMPlayerCApp::RemoveAllSetting(){
	_tremove(GetIniPath());
	
//	HKEY hKey;
	RegDelnode(HKEY_CURRENT_USER, L"Software\\SPlayer");
	AppSettings& s = AfxGetAppSettings();
	s.fInitialized = FALSE;
	s.UpdateData(FALSE);

}
bool CMPlayerCApp::GetAppDataPath(CString& path)
{
	CSVPToolBox svpTool;
	return svpTool.GetAppDataPath(path);
}

void CMPlayerCApp::PreProcessCommandLine()
{
	m_cmdln.RemoveAll();
	for(int i = 1; i < __argc; i++)
	{
		CString str = CString(__targv[i]).Trim(_T(" \""));

		if(str[0] != '/' && str[0] != '-' && str.Find(_T(":")) < 0)
		{
			LPTSTR p = NULL;
			CString str2;
			str2.ReleaseBuffer(GetFullPathName(str, MAX_PATH, str2.GetBuffer(MAX_PATH), &p));
			CFileStatus fs;
			if(!str2.IsEmpty() && CFileGetStatus(str2, fs)) str = str2;
		}

		m_cmdln.AddTail(str);
	}
}
 void CALLBACK HungWindowResponseCallback(HWND target_window,
												UINT message,
												ULONG_PTR data,
												LRESULT result){

	CMPlayerCApp* instance = (CMPlayerCApp*)(data);
	if (NULL != instance) {
		instance->m_bGotResponse = true;
		
		//AfxMessageBox(_T("m_bGotResponse true"));
	}
}

BOOL CMPlayerCApp::SendCommandLine(HWND hWnd, BOOL bPostMessage )
{
	if(m_cmdln.IsEmpty())
		return false;

	int bufflen = sizeof(DWORD);

	POSITION pos = m_cmdln.GetHeadPosition();
	while(pos) bufflen += (m_cmdln.GetNext(pos).GetLength()+1)*sizeof(TCHAR);

	CAutoVectorPtr<BYTE> buff;
	if(!buff.Allocate(bufflen))
		return false;

	BYTE* p = buff;

	*(DWORD*)p = m_cmdln.GetCount(); 
	p += sizeof(DWORD);

	pos = m_cmdln.GetHeadPosition();
	while(pos)
	{
		CString s = m_cmdln.GetNext(pos); 
		int len = (s.GetLength()+1)*sizeof(TCHAR);
		memcpy(p, s, len);
		p += len;
	}

	COPYDATASTRUCT cds;
	cds.dwData = 0x6ABE51;
	cds.cbData = bufflen;
	cds.lpData = (void*)(BYTE*)buff;
	if(bPostMessage){

		m_bGotResponse = FALSE;
		if(SendMessageCallback(hWnd,WM_NULL,(WPARAM) NULL, NULL, 
			HungWindowResponseCallback,	(ULONG_PTR)(this))){
		
			MSG tMsg;
			for(int i =0;i< 7; i++){
				PeekMessage(&tMsg, hWnd, 0,0,0);
				if(m_bGotResponse){
					//AfxMessageBox(_T("m_bGotResponse true 2"));
					break;
				}
				Sleep(320);
			}


			if(m_bGotResponse != true){
				//AfxMessageBox(_T("m_bGotResponse false"));
				return false;
			}
		}else{
			//AfxMessageBox(_T("SendMessageCallback false"));
			return FALSE;
		}

		
	}
	//AfxMessageBox(_T("SendMsg"));
	SendMessage(hWnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
	return true;
}
HRESULT SVPRegDeleteValueEx(HKEY hKeyRoot,
					  LPCTSTR lpKey,LPCTSTR lpValue)
{
	HRESULT bErrRet = S_OK;

	HKEY hKey;

	if (ERROR_SUCCESS == RegOpenKeyEx(hKeyRoot,
		lpKey,
		0,
		KEY_SET_VALUE,
		&hKey))
	{

		if (ERROR_SUCCESS != RegDeleteValue(hKey,lpValue))
			bErrRet = E_FAIL;

		RegCloseKey(hKey);

	}

	else
		bErrRet = E_FAIL;

	return bErrRet;
}


/////////////////////////////////////////////////////////////////////////////
// CMPlayerCApp initialization

#include "..\..\..\include\detours\detours.h"
#include "..\..\..\include\winddk\ntddcdvd.h"

BOOL (__stdcall * Real_IsDebuggerPresent)(void)
= IsDebuggerPresent;

LONG (__stdcall * Real_ChangeDisplaySettingsExA)(LPCSTR a0,
												 LPDEVMODEA a1,
												 HWND a2,
												 DWORD a3,
												 LPVOID a4)
												 = ChangeDisplaySettingsExA;

LONG (__stdcall * Real_ChangeDisplaySettingsExW)(LPCWSTR a0,
												 LPDEVMODEW a1,
												 HWND a2,
												 DWORD a3,
												 LPVOID a4)
												 = ChangeDisplaySettingsExW;

HANDLE (__stdcall * Real_CreateFileA)(LPCSTR a0,
									  DWORD a1,
									  DWORD a2,
									  LPSECURITY_ATTRIBUTES a3,
									  DWORD a4,
									  DWORD a5,
									  HANDLE a6)
									  = CreateFileA;

HANDLE (__stdcall * Real_CreateFileW)(LPCWSTR a0,
									  DWORD a1,
									  DWORD a2,
									  LPSECURITY_ATTRIBUTES a3,
									  DWORD a4,
									  DWORD a5,
									  HANDLE a6)
									  = CreateFileW;

DWORD (__stdcall * Real_GetModuleFileNameA)(HMODULE hModule, LPCH lpFilename,  DWORD nSize )
												 =  GetModuleFileNameA;
BOOL (__stdcall * Real_DeviceIoControl)(HANDLE a0,
										DWORD a1,
										LPVOID a2,
										DWORD a3,
										LPVOID a4,
										DWORD a5,
										LPDWORD a6,
										LPOVERLAPPED a7)
										= DeviceIoControl;

MMRESULT  (__stdcall * Real_mixerSetControlDetails)( HMIXEROBJ hmxobj, 
													LPMIXERCONTROLDETAILS pmxcd, 
													DWORD fdwDetails)
													= mixerSetControlDetails;

#include <Winternl.h>
typedef NTSTATUS (WINAPI *FUNC_NTQUERYINFORMATIONPROCESS)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);
static FUNC_NTQUERYINFORMATIONPROCESS		Real_NtQueryInformationProcess = NULL;
/*
NTSTATUS (* Real_NtQueryInformationProcess) (HANDLE				ProcessHandle, 
PROCESSINFOCLASS	ProcessInformationClass, 
PVOID				ProcessInformation, 
ULONG				ProcessInformationLength, 
PULONG				ReturnLength)
= NULL;*/

BOOL WINAPI Mine_IsDebuggerPresent()
{
	TRACE(_T("Oops, somebody was trying to be naughty! (called IsDebuggerPresent)\n")); 
	return FALSE;
}
#include "Struct.h"
NTSTATUS WINAPI Mine_NtQueryInformationProcess(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength)
{
	NTSTATUS		nRet;

	nRet = Real_NtQueryInformationProcess(ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);

	if (ProcessInformationClass == ProcessBasicInformation)
	{
		PROCESS_BASIC_INFORMATION*		pbi = (PROCESS_BASIC_INFORMATION*)ProcessInformation;
		PEB_NT*							pPEB;
		PEB_NT							PEB;

		pPEB = (PEB_NT*)pbi->PebBaseAddress;
		ReadProcessMemory(ProcessHandle, pPEB, &PEB, sizeof(PEB), NULL);
		PEB.BeingDebugged = 0;
		WriteProcessMemory(ProcessHandle, pPEB, &PEB, sizeof(PEB), NULL);
	}
	else if (ProcessInformationClass == 7) // ProcessDebugPort
	{
		BOOL*		pDebugPort = (BOOL*)ProcessInformation;
		*pDebugPort = FALSE;
	}

	return nRet;
}

LONG WINAPI Mine_ChangeDisplaySettingsEx(LONG ret, DWORD dwFlags, LPVOID lParam)
{
	if(dwFlags&CDS_VIDEOPARAMETERS)
	{
		VIDEOPARAMETERS* vp = (VIDEOPARAMETERS*)lParam;

		if(vp->Guid == GUIDFromCString(_T("{02C62061-1097-11d1-920F-00A024DF156E}"))
			&& (vp->dwFlags&VP_FLAGS_COPYPROTECT))
		{
			if(vp->dwCommand == VP_COMMAND_GET)
			{
				if((vp->dwTVStandard&VP_TV_STANDARD_WIN_VGA)
					&& vp->dwTVStandard != VP_TV_STANDARD_WIN_VGA)
				{
					TRACE(_T("Ooops, tv-out enabled? macrovision checks suck..."));
					vp->dwTVStandard = VP_TV_STANDARD_WIN_VGA;
				}
			}
			else if(vp->dwCommand == VP_COMMAND_SET)
			{
				TRACE(_T("Ooops, as I already told ya, no need for any macrovision bs here"));
				return 0;
			}
		}
	}

	return ret;
}
LONG WINAPI Mine_ChangeDisplaySettingsExA(LPCSTR lpszDeviceName, LPDEVMODEA lpDevMode, HWND hwnd, DWORD dwFlags, LPVOID lParam)
{
	return Mine_ChangeDisplaySettingsEx(
		Real_ChangeDisplaySettingsExA(lpszDeviceName, lpDevMode, hwnd, dwFlags, lParam), 
		dwFlags, 
		lParam);
}

DWORD WINAPI Mine_GetModuleFileNameA( HMODULE hModule, LPCH lpFilename,  DWORD nSize )
{
	DWORD ret = Real_GetModuleFileNameA(hModule, lpFilename, nSize);
	//SVP_LogMsg3(" Mine_GetModuleFileNameA %s" , lpFilename);
	return ret;
}
				   
LONG WINAPI Mine_ChangeDisplaySettingsExW(LPCWSTR lpszDeviceName, LPDEVMODEW lpDevMode, HWND hwnd, DWORD dwFlags, LPVOID lParam)
{
	return Mine_ChangeDisplaySettingsEx(
		Real_ChangeDisplaySettingsExW(lpszDeviceName, lpDevMode, hwnd, dwFlags, lParam), 
		dwFlags,
		lParam);
}

HANDLE WINAPI Mine_CreateFileA(LPCSTR p1, DWORD p2, DWORD p3, LPSECURITY_ATTRIBUTES p4, DWORD p5, DWORD p6, HANDLE p7)
{
	//CStringA fn(p1);
	//fn.MakeLower();
	//int i = fn.Find(".part");
	//if(i > 0 && i == fn.GetLength() - 5)
	p3 |= FILE_SHARE_WRITE;
	//if(strstr(p1, ("SVPDebug")) == 0)  SVP_LogMsg6(("Mine_CreateFileA %s") , p1);
    //if( strcmp (p1 + nLen-4, ".ini") == 0)
        //SVP_LogMsg5(L"Mine_CreateFileW %s", p1);

	return Real_CreateFileA(p1, p2, p3, p4, p5, p6, p7);
}
#include "Ifo.h"
BOOL CreateFakeVideoTS(LPCWSTR strIFOPath, LPWSTR strFakeFile, size_t nFakeFileSize)
{
	BOOL		bRet = FALSE;
	WCHAR		szTempPath[MAX_PATH];
	WCHAR		strFileName[MAX_PATH];
	WCHAR		strExt[10];
	CIfo		Ifo;

	if (!GetTempPathW(MAX_PATH, szTempPath)) return FALSE;

	_wsplitpath_s (strIFOPath, NULL, 0, NULL, 0, strFileName, countof(strFileName), strExt, countof(strExt));
	_snwprintf_s  (strFakeFile, nFakeFileSize, _TRUNCATE, L"%sMPC%s%s", szTempPath, strFileName, strExt);

	if (Ifo.OpenFile (strIFOPath) &&
		Ifo.RemoveUOPs()  &&
		Ifo.SaveFile (strFakeFile))
	{
		bRet = TRUE;
	}

	return bRet;
}

DWORD (__stdcall * Real_GetPrivateProfileStringA)(LPCSTR lpAppName,
                                                  LPCSTR lpKeyName,
                                                  LPCSTR lpDefault,
                                                  LPSTR lpReturnedString,
                                                  DWORD nSize,
                                                  LPCSTR lpFileName)
                                      = GetPrivateProfileStringA;


DWORD WINAPI Mine_GetPrivateProfileStringA(
                          LPCSTR lpAppName,
                          LPCSTR lpKeyName,
                          LPCSTR lpDefault,
                          LPSTR lpReturnedString,
                          DWORD nSize,
                          LPCSTR lpFileName
                         )
{

     
    if(strcmp(lpAppName, "CoreAVC") == 0 && strcmp(lpKeyName, "Settings") == 0 )
    {
        char defaultsettings[] = "use_cuda=1 use_tray=0";
        char defaultsettings_nocuda[] = "use_cuda=0 use_tray=0";
        AppSettings& s = AfxGetAppSettings();
        if( s.useGPUCUDA )
            strcpy_s( lpReturnedString ,  nSize , defaultsettings );
        else
            strcpy_s( lpReturnedString ,  nSize , defaultsettings_nocuda );
        return sizeof(defaultsettings);
    }
     

     //SVP_LogMsg6(("Mine_GetPrivateProfileStringA %s %s %s  %s") , lpAppName, lpKeyName, lpDefault, lpReturnedString);
     return  Real_GetPrivateProfileStringA(lpAppName, lpKeyName, lpDefault, lpReturnedString, nSize, lpFileName);
}

BOOL (__stdcall * Real_WritePrivateProfileStringA)( LPCSTR lpAppName,
                                                    LPCSTR lpKeyName,
                                                    LPCSTR lpString,
                                                    LPCSTR lpFileName)
                                                  = WritePrivateProfileStringA;

BOOL WINAPI Mine_WritePrivateProfileStringA(
                            LPCSTR lpAppName,
                            LPCSTR lpKeyName,
                            LPCSTR lpString,
                            LPCSTR lpFileName
                            )
{


    if(strcmp(lpAppName, "CoreAVC") == 0 && strcmp(lpKeyName, "Settings") == 0 )
    {
        return true;
    }
    //SVP_LogMsg6(("Mine_WritePrivateProfileStringA %s %s %s  %s") , lpAppName, lpKeyName, lpString, lpFileName);
    return Real_WritePrivateProfileStringA( lpAppName,
                                     lpKeyName,
                                     lpString,
                                     lpFileName);
}




HANDLE WINAPI Mine_CreateFileW(LPCWSTR p1, DWORD p2, DWORD p3, LPSECURITY_ATTRIBUTES p4, DWORD p5, DWORD p6, HANDLE p7)
{
	HANDLE	hFile = INVALID_HANDLE_VALUE;
	WCHAR	strFakeFile[MAX_PATH];
	int		nLen  = wcslen(p1);

    //if(CString(p1).Find( L"SVPDebug") < 0)
	 //   SVP_LogMsg5(L"Mine_CreateFileW %s", p1);

	p3 |= FILE_SHARE_WRITE;

	if (nLen>=4 && _wcsicmp (p1 + nLen-4, L".ifo") == 0)
	{
		if (CreateFakeVideoTS(p1, strFakeFile, countof(strFakeFile)))
		{
			hFile = Real_CreateFileW(strFakeFile, p2, p3, p4, p5, p6, p7);
		}
	}

	if (hFile == INVALID_HANDLE_VALUE)
		hFile = Real_CreateFileW(p1, p2, p3, p4, p5, p6, p7);

	//if(wcsstr(p1, _T("SVPDebug")) == 0)  SVP_LogMsg5(_T("Mine_CreateFileW %s") , p1);

	return hFile;
}


MMRESULT WINAPI Mine_mixerSetControlDetails(HMIXEROBJ hmxobj, LPMIXERCONTROLDETAILS pmxcd, DWORD fdwDetails)
{
	if(fdwDetails == (MIXER_OBJECTF_HMIXER|MIXER_SETCONTROLDETAILSF_VALUE)) 
		return MMSYSERR_NOERROR; // don't touch the mixer, kthx
	return Real_mixerSetControlDetails(hmxobj, pmxcd, fdwDetails);
}

BOOL WINAPI Mine_DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
	BOOL ret = Real_DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped);

	if(IOCTL_DVD_GET_REGION == dwIoControlCode && lpOutBuffer
		&& lpBytesReturned && *lpBytesReturned == sizeof(DVD_REGION))
	{
		DVD_REGION* pDVDRegion = (DVD_REGION*)lpOutBuffer;
		pDVDRegion->SystemRegion = ~pDVDRegion->RegionData;
	}

	return ret;
}

#include "../../subtitles/SSF.h"
#include "../../subtitles/RTS.h"
#include "../../subpic/MemSubPic.h"

class ssftest
{
public:
	ssftest()
	{
		Sleep(10000);

		MessageBeep(-1);
// 8; //
		SubPicDesc spd;
		spd.w = 640;
		spd.h = 480;
		spd.bpp = 32;
		spd.pitch = spd.w*spd.bpp>>3;
		spd.type = MSP_RGB32;
		spd.vidrect = CRect(0, 0, spd.w, spd.h);
		spd.bits = new BYTE[spd.pitch*spd.h];

		CCritSec csLock;
/*
		CRenderedTextSubtitle s(&csLock);
		s.Open(_T("../../subtitles/libssf/demo/demo.ssa"), 1);

		for(int i = 2*60*1000+2000; i < 2*60*1000+17000; i += 10)
		{
			memsetd(spd.bits, 0xff000000, spd.pitch*spd.h);
			CRect bbox;
			bbox.SetRectEmpty();
			s.Render(spd, 10000i64*i, 25, bbox);
		}
*/
		try
		{
			ssf::CRenderer s(&csLock);
			s.Open(_T("../../subtitles/libssf/demo/demo.ssf"));

			for(int i = 2*60*1000+2000; i < 2*60*1000+17000; i += 40)
			// for(int i = 2*60*1000+2000; i < 2*60*1000+17000; i += 1000)
			//for(int i = 0; i < 5000; i += 40)
			{
				memsetd(spd.bits, 0xff000000, spd.pitch*spd.h);
				CRect bbox;
				bbox.SetRectEmpty();
				s.Render(spd, 10000i64*i, 25, bbox);
			}
		}
		catch(ssf::Exception& e)
		{
			TRACE(_T("%s\n"), e.ToString());
			ASSERT(0);
		}

		delete [] spd.bits;

		::ExitProcess(0);
	}
};
void SVPRegWriteDWORD(HKEY key_root, CString szKey, TCHAR* valKey, DWORD val){
	HKEY reg;
	DWORD s;
	if(!RegCreateKeyEx(key_root,szKey,0,NULL,0,KEY_ALL_ACCESS,NULL,&reg,&s)) {
		RegSetValueEx(reg,valKey,0,REG_DWORD, (BYTE*)&val, 4) ;
		RegCloseKey(reg);
	}

}
void SVPRegWriteStr(HKEY key_root, CString szKey, TCHAR* valKey, TCHAR* val){
	HKEY reg;
	DWORD s;
	if(!RegCreateKeyEx(key_root,szKey,0,NULL,0,KEY_ALL_ACCESS,NULL,&reg,&s)) {
		RegSetValueEx(reg,valKey,0,REG_SZ, (BYTE*)val, (DWORD) (lstrlen(val)+1)*sizeof(TCHAR)) ;
		RegCloseKey(reg);
	}

}
void CMPlayerCApp::InitInstanceThreaded(INT64 CLS64){
	
	CSVPToolBox svpToolBox;
	CStringArray csaDll;

    SVP_LogMsg5(L"Settings::InitInstanceThreaded");
	//SVP_LogMsg5(L"%x %d xx ",CLS64 ,(CLS64&CLSW_STARTFROMDMP) );
	if(!(CLS64&CLSW_STARTFROMDMP)){
		_wremove(svpToolBox.GetPlayerPath(_T("SVPDebug.log")));
		_wremove(svpToolBox.GetPlayerPath(_T("SVPDebug2.log")));
	}

	/*
for(int i = 0; i <= 30; i++){
		SVP_LogMsg5(_T("COLOR_GRAYTEXT %x %d"), GetSysColor(i), i);

	}
	SVP_LogMsg5(_T("COLOR_GRAYTEXT %x"), GetSysColor(COLOR_GRAYTEXT));
*/

    SVP_LogMsg5(L"Settings::InitInstanceThreaded 1");

	//avoid crash by lame acm
	RegDelnode(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\MediaResources\\msacm\\msacm.lameacm");
	SVPRegDeleteValueEx( HKEY_LOCAL_MACHINE , L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\drivers.desc",L"LameACM.acm");
	SVPRegDeleteValueEx( HKEY_LOCAL_MACHINE , L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\drivers32",L"msacm.lameacm");

    SVP_LogMsg5(L"Settings::InitInstanceThreaded 2");
	bool regReal = false;
	{
		CString prefs(_T("Software\\RealNetworks\\Preferences"));

		CRegKey key;

		if(ERROR_SUCCESS != key.Open(HKEY_CLASSES_ROOT, prefs + _T("\\DT_Common"), KEY_READ)){
			regReal = true;
		}else{
			TCHAR buff[MAX_PATH];
			ULONG len = sizeof(buff);
			if(ERROR_SUCCESS != key.QueryStringValue(NULL, buff, &len))
			{
				regReal = true;
			}else{
				key.Close();

				if(!svpToolBox.ifFileExist(CString(buff) + _T("pnen3260.dll"))) {
					regReal = true;
				}
			}
		}
			
	}
    SVP_LogMsg5(L"Settings::InitInstanceThreaded 3");
	if(regReal && svpToolBox.ifFileExist(svpToolBox.GetPlayerPath(L"Real\\Common\\pnen3260.dll"))){
		CString szLPath;
		szLPath = CString(svpToolBox.GetPlayerPath(L"\\"));
		SVPRegWriteStr(HKEY_CLASSES_ROOT,L"Software\\RealNetworks\\Preferences\\DT_Codecs",0,szLPath.GetBuffer());
		szLPath.ReleaseBuffer();

		szLPath = CString(svpToolBox.GetPlayerPath(L"Real\\Common\\"));
		SVPRegWriteStr(HKEY_CLASSES_ROOT,L"Software\\RealNetworks\\Preferences\\DT_Common",0,szLPath.GetBuffer());
		SVPRegWriteStr(HKEY_CLASSES_ROOT,L"Software\\RealNetworks\\Preferences\\DT_Objbrokr",0,szLPath.GetBuffer());
		szLPath.ReleaseBuffer();

		szLPath = CString(svpToolBox.GetPlayerPath(L"Real\\Plugins\\"));
		SVPRegWriteStr(HKEY_CLASSES_ROOT,L"Software\\RealNetworks\\Preferences\\DT_Plugins",0,szLPath.GetBuffer());
		szLPath.ReleaseBuffer();

		SVPRegWriteDWORD(HKEY_CURRENT_USER,L"Software\\RealNetworks\\RealMediaSDK\\6.0\\Preferences\\UseOverlay",0, 0);

		/*

		WriteRegStr HKCR "Software\RealNetworks\Preferences\DT_Codecs" "" "$INSTDIR"
		WriteRegStr HKCR "Software\RealNetworks\Preferences\DT_Common" "" "$INSTDIR\Real\Common\"
		WriteRegStr HKCR "Software\RealNetworks\Preferences\DT_Objbrokr" "" "$INSTDIR\Real\Common\"
		WriteRegStr HKCR "Software\RealNetworks\Preferences\DT_Plugins" "" "$INSTDIR\Real\\"
		WriteRegDWORD HKCR "Software\RealNetworks\RealMediaSDK\6.0\Preferences\UseOverlay"  "" 0
		*/
	}
    SVP_LogMsg5(L"Settings::InitInstanceThreaded 4");
	m_bSystemParametersInfo[0] = FALSE;
	if(!IsVista()){
		BOOL bDropShadow = FALSE;

		if (SystemParametersInfo(SPI_GETDROPSHADOW , 0,    &bDropShadow,	0)){
			if(bDropShadow){
				if( SystemParametersInfo(SPI_SETDROPSHADOW , 0,    FALSE,	0) )
					m_bSystemParametersInfo[0] = TRUE;
			}
		}

/* useless, not effect to tooltip shadow

		if (SystemParametersInfo(SPI_GETTOOLTIPFADE , 0,    &bDropShadow,	0)){
			if(bDropShadow){
				if( SystemParametersInfo(SPI_SETTOOLTIPFADE , 0,    FALSE,	0) )
					m_bSystemParametersInfo[1] = TRUE;
			}
		}


		if (SystemParametersInfo(SPI_GETTOOLTIPANIMATION , 0,    &bDropShadow,	0)){
			if(bDropShadow){
				if( SystemParametersInfo(SPI_SETTOOLTIPANIMATION , 0,    FALSE,	0) )
					m_bSystemParametersInfo[2] = TRUE;
			}
		}

*/


	}
    SVP_LogMsg5(L"Settings::InitInstanceThreaded 5");
	//csaDll.Add( _T("codecs\\CoreAVCDecoder.ax")); //avoid missing reg key problem
	//CFilterMapper2 fm2(false);
	if(1){
		HKEY fKey;
		CFilterMapper2 fmx(false);
		if(RegOpenKey(HKEY_CLASSES_ROOT , _T("CLSID\\{55DA30FC-F16B-49FC-BAA5-AE59FC65F82D}") , &fKey ) != ERROR_SUCCESS ){
				
			SVP_LogMsg5(L"Reg haalis.ax");
			RegSvr32( _T("haalis.ax"));
			
		}
		if(RegOpenKey(HKEY_CLASSES_ROOT , _T("CLSID\\{31345649-0000-0010-8000-00AA00389B71}") , &fKey ) != ERROR_SUCCESS ){
			SVP_LogMsg5(L"reg iv41.ax");
			RegSvr32( _T("ir41_32.ax"));
		}

		if(RegOpenKey(HKEY_CLASSES_ROOT , _T("CLSID\\{30355649-0000-0010-8000-00AA00389B71}") , &fKey ) != ERROR_SUCCESS ){
			SVP_LogMsg5(L"reg ir50_32.dll");
			RegSvr32( _T("ir50_32.dll"));
		}
		

		if(RegOpenKey(HKEY_CLASSES_ROOT , _T("CLSID\\{DB43B405-43AA-4f01-82D8-D84D47E6019C}") , &fKey ) != ERROR_SUCCESS ){
			SVP_LogMsg5(L"Reg ogm.dll");
			RegSvr32( _T("ogm.dll"));
		}

		if(RegOpenKey(HKEY_CLASSES_ROOT , _T("CLSID\\{B841F346-4835-4de8-AA5E-2E7CD2D4C435}") , &fKey ) != ERROR_SUCCESS ){
			SVP_LogMsg5(L"Reg ts.dll");
			RegSvr32( _T("ts.dll"));
		}
        SVP_LogMsg5(L"Settings::InitInstanceThreaded 6");
		if( RegOpenKey(HKEY_CLASSES_ROOT , _T("CLSID\\{ACD23F8C-B37E-4B2D-BA08-86CB6E621D6A}") , &fKey) != ERROR_SUCCESS){
			SVP_LogMsg5(L"mpc_mtcontain go");
			RegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mtcontain.dll")) );
			SVP_LogMsg5(L"mpc_mtcontain done");
		}
		if( RegOpenKey(HKEY_CLASSES_ROOT , _T("CLSID\\{B4DAEDB7-7F0E-434F-9AA3-B82B549A3680}") , &fKey) != ERROR_SUCCESS){
			RegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mtcontrol.dll")) );
			SVP_LogMsg5(L"mpc_mtcontrol done");
		}

		/*[267851681.982100] SET RegW (null)
		[267851685.151100] SET RegW (null)
		[267851702.360300] SET Mine_RegSetValueExW ThreadingModel
		[267851706.226100] Mine_CoCreateInstance {CDA42200-BD88-11D0-BD4E-00A0C911CE86} 0
		[267851706.454700] Mine_RegOpenKeyExW  CLSID\{083863F1-70DE-11D0-BD40-00A0C911CE86}\Instance 0 
		[267851706.604200] Mine_RegOpenKeyExW 80000000 CLSID\{083863F1-70DE-11D0-BD40-00A0C911CE86}\Instance 0
		[267851706.816000] Mine_RegOpenKeyExW  {FF5DCC7A-7147-41E1-86E8-DD05ABD588BF} 0 
		[267851707.029300] Mine_RegOpenKeyExW 2ea {FF5DCC7A-7147-41E1-86E8-DD05ABD588BF} 2
		[267851725.508600] Mine_CoCreateInstance {4315D437-5B8C-11D0-BD3B-00A0C911CE86} 0
		[267851725.720400] Mine_RegCreateKeyExW CLSID\{083863F1-70DE-11D0-BD40-00A0C911CE86}\Instance\{FF5DCC7A-7147-41E1-86E8-DD05ABD588BF}
		[267851730.858000] SET Mine_RegSetValueExW FriendlyName
		[267851733.711300] Mine_RegCreateKeyExW CLSID\{083863F1-70DE-11D0-BD40-00A0C911CE86}\Instance\{FF5DCC7A-7147-41E1-86E8-DD05ABD588BF}
		[267851733.959600] SET Mine_RegSetValueExW CLSID
		[267851743.117000] Mine_RegCreateKeyExW CLSID\{083863F1-70DE-11D0-BD40-00A0C911CE86}\Instance\{FF5DCC7A-7147-41E1-86E8-DD05ABD588BF}
		[267851743.406500] SET Mine_RegSetValueExW FilterData*/
		//RegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mxrender.dll")) );
		//SVP_LogMsg5(L"mpc_mxrender done");
SVP_LogMsg5(L"Settings::InitInstanceThreaded 7");
		if( RegOpenKey(HKEY_CLASSES_ROOT , _T("MpcMxVideo.XvidDecoder.1") , &fKey ) != ERROR_SUCCESS){
			RegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mxvideo.dll")) );
		}
SVP_LogMsg5(L"Settings::InitInstanceThreaded 8");
		if( RegOpenKey(HKEY_CLASSES_ROOT , _T("Mpcwtlvcl.VideoFrame") , &fKey ) != ERROR_SUCCESS){
			
				RegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mdssockc.dll")) );
				RegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mxaudio.dll")) );
				
				RegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mxscreen.dll")) );
				RegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mxshbasu.dll")) );
				RegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mxshmaiu.dll")) );
				RegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mxshsour.dll")) );
				RegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mcucltu.dll")) );
				RegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mcufilecu.dll")) );
		
				RegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_wtlvcl.dll")) );
				SVP_LogMsg5(L"reg csf dlls");
		
			
		}
		SVP_LogMsg5(L"Reg End");
	}

		//csaDll.Add( _T("tsccvid.dll"));
		//csaDll.Add( _T("wvc1dmod.dll"));
		//csaDll.Add( _T("RadGtSplitter.ax"));

	{
		HKEY fKey;
		if(RegOpenKey(HKEY_CLASSES_ROOT , _T("CLSID\\{2eeb4adf-4578-4d10-bca7-bb955f56320a}") , &fKey ) != ERROR_SUCCESS ){
			csaDll.Add( _T("wmadmod.dll"));
		}else{
			//RegCloseKey(fKey);
		}
	}
    SVP_LogMsg5(L"Settings::InitInstanceThreaded 10");
		
		for(int i = 0; i < csaDll.GetCount(); i++){
			CString szDllPath = svpToolBox.GetPlayerPath( csaDll.GetAt(i) );
			if(svpToolBox.ifFileExist(szDllPath)){
				//fm2.Register(szDllPath);
				RegSvr32( szDllPath );
			}
		}
SVP_LogMsg5(L"Settings::InitInstanceThreaded 11");
        if(!sqlite_local_record){ // TODO: save play record to local sql db
            //AfxMessageBox(L"0");
            int iDescLen;
            CString recordPath;
            svpToolBox.GetAppDataPath(recordPath);
            CPath tmPath(recordPath);
            tmPath.RemoveBackslash();
            tmPath.AddBackslash();
            tmPath.Append( _T("local.db"));
            //AfxMessageBox(tmPath);
            char * buff = svpToolBox.CStringToUTF8(CString(tmPath),&iDescLen) ;
            sqlite_local_record = new SQLITE3( buff );
            free(buff);
            if(!sqlite_local_record->db_open){
                delete sqlite_local_record;
                sqlite_local_record = NULL;
            }
            //AfxMessageBox(L"1");
            if(sqlite_local_record){
                // 			sqlite_local_record->exec_sql("CREATE TABLE  IF NOT EXISTS favrec (\"favtype\" INTEGER, \"favpath\" TEXT, \"favtime\" TEXT, \"addtime\" INTEGER, \"favrecent\" INTEGER )");
                // 			sqlite_local_record->exec_sql("CREATE UNIQUE INDEX  IF NOT EXISTS \"favpk\" on favrec (favtype ASC, favpath ASC, favrecent ASC)");
                // 			sqlite_local_record->exec_sql("CREATE INDEX  IF NOT EXISTS \"favord\" on favrec (addtime ASC)");
                sqlite_local_record->exec_sql("CREATE TABLE  IF NOT EXISTS histories (\"fpath\" TEXT, \"subid\" INTEGER, \"subid2\" INTEGER, \"audioid\" INTEGER, \"stoptime\" INTEGER, \"modtime\" INTEGER )");
                sqlite_local_record->exec_sql("CREATE UNIQUE INDEX  IF NOT EXISTS \"hispk\" on histories (fpath ASC)");
                sqlite_local_record->exec_sql("CREATE INDEX  IF NOT EXISTS \"modtime\" on histories (modtime ASC)");
                sqlite_local_record->exec_sql("PRAGMA synchronous=OFF");
                //sqlite_local_record->end_transaction();
            }
            //AfxMessageBox(L"2");
        }
SVP_LogMsg5(L"Settings::InitInstanceThreaded 12");
		CMainFrame* pFrame = (CMainFrame*)m_pMainWnd;
		

		if(pFrame){
			if(CLS64&CLSW_STARTFROMDMP){
				
				pFrame->SendStatusMessage(ResStr(IDS_OSD_MSG_JUST_RECOVER_FROM_CRASH), 6000);
			}
				
            SVP_LogMsg5(L"Settings::InitInstanceThreaded 15");
			//检查文件关联
			if ( m_s.fCheckFileAsscOnStartup ){
				CChkDefPlayer dlg_chkdefplayer;
				if( ! dlg_chkdefplayer.b_isDefaultPlayer() ){
					if(m_s.fPopupStartUpExtCheck || (IsVista() && !IsUserAnAdmin())){
						dlg_chkdefplayer.DoModal();
					}else{
						dlg_chkdefplayer.setDefaultPlayer();
					}
				}
				//	dlg_chkdefplayer.setDefaultPlayer();

			}
SVP_LogMsg5(L"Settings::InitInstanceThreaded 16");
			if ( time(NULL) > (m_s.tLastCheckUpdater + m_s.tCheckUpdaterInterleave) || m_s.tLastCheckUpdater == 0){
			

				if(m_s.tLastCheckUpdater == 0 && !svpToolBox.FindSystemFile( _T("wmvcore.dll") )){
					pFrame->SendStatusMessage(_T("您的系统中缺少必要的wmv/asf媒体组件，正在下载（约2MB）..."), 4000);
				}
				//if(m_s.tLastCheckUpdater == 0 &&  !svpToolBox.bFontExist(_T("微软雅黑")) && !svpToolBox.bFontExist(_T("Microsoft YaHei")) ){ 
				//	pFrame->SendStatusMessage(_T("您的系统中缺少字体组件，正在下载（约8MB）..."), 4000);
				//}
				m_s.tLastCheckUpdater = (UINT)time(NULL); 
				m_s.UpdateData(true);

				if(!m_cnetupdater)
					m_cnetupdater = new cupdatenetlib();

				if(m_cnetupdater->downloadList()){
					m_cnetupdater->downloadFiles();
					m_cnetupdater->tryRealUpdate(TRUE);
				}
				if(!pFrame->m_bCheckingUpdater){
					pFrame->m_bCheckingUpdater = true;
					SVP_RealCheckUpdaterExe( &(pFrame->m_bCheckingUpdater) );

				}
				SVP_LogMsg5(L"Settings::InitInstanceThreaded 17");
				m_cnetupdater->bSVPCU_DONE = TRUE;
			}
			
			//AfxMessageBox(_T("GO"));
			HWND hWnd = NULL;
			while(1)
			{
				Sleep(1000);
				hWnd = ::FindWindowEx(NULL, hWnd, MPC_WND_CLASS_NAME, NULL);
				if(!hWnd)
					break;

				//AfxMessageBox(_T("GO1"));
				//Sleep(3000);
				if(hWnd != pFrame->m_hWnd) {
					CString dumpMsg;

					Sleep(1000);
				//					AfxMessageBox(_T("GO1.5"));
					DWORD pId = NULL;
					DWORD dwTid = GetWindowThreadProcessId(hWnd, &pId);
					if(pId == GetCurrentProcessId())
						continue;

					m_bGotResponse = FALSE;
					if(SendMessageCallback(hWnd,WM_NULL,(WPARAM) NULL, NULL, 
						HungWindowResponseCallback,	(ULONG_PTR)(this))){

							MSG tMsg;
							for(int i =0;i< 10; i++){
								PeekMessage(&tMsg, hWnd, 0,0,0);
								if(m_bGotResponse){
									//AfxMessageBox(_T("m_bGotResponse true 2"));
									break;
								}
								Sleep(320);
							}


							if(m_bGotResponse != true){
								//		AfxMessageBox(_T("GO3"));
								HANDLE deadProcess = OpenProcess( PROCESS_TERMINATE  , false , pId);
								if(deadProcess){
									dumpMsg = DebugMiniDumpProcess(deadProcess, pId, dwTid);
									TerminateProcess(deadProcess, 0);
									CloseHandle(deadProcess);
								}
								hWnd = NULL;

								if(!dumpMsg.IsEmpty())
									pFrame->SendStatusMessage(dumpMsg, 4000);
							}
					}
					
				}
			}
            SVP_LogMsg5(L"Settings::InitInstanceThreaded 18");
            if(!IsVista()){
			    CDisplaySettingDetector cdsd;
                cdsd.init();
                int valevel = cdsd.GetVideoAccelLevel();
                SVP_LogMsg6("Video %s valevel %d", cdsd.Video0Name, valevel);
                
                if(valevel != 0)
                {
                    pFrame->SendStatusMessage(ResStr(IDS_OSD_MSG_VIDEO_CARD_ACCELERATION_LEVEL_TOO_LOW), 6000);
                    cdsd.SetVideoAccelLevel(0);
                    SVP_LogMsg5(L"Settings::InitInstanceThreaded 19");
                }
            }
            
		}
        SVP_LogMsg5(L"Settings::InitInstanceThreaded 22");
}

UINT __cdecl Thread_InitInstance( LPVOID lpParam ) 
{ 
	
	CMPlayerCApp * ma =(CMPlayerCApp*) lpParam;
	CMainFrame* pFrame = (CMainFrame*)ma->m_pMainWnd;
	while(pFrame->m_WndSizeInited < 2){
		Sleep(1000);
	}
	INT64 CLS64 = ma->m_s.nCLSwitches;
	
	CoInitialize(NULL);
	
	ma->InitInstanceThreaded(CLS64);

	CoUninitialize();
	return 0; 
}

HGLOBAL (__stdcall * Real_LoadResource)( HMODULE hModule, HRSRC hResInfo ) =  ::LoadResource;

HGLOBAL WINAPI Mine_LoadResource( HMODULE hModule, HRSRC hResInfo )
{
	//SVP_LogMsg5(L"Mine_LoadResource");
	if(CMPlayerCApp::m_hResDll){
		HGLOBAL hGTmp = Real_LoadResource(hModule, hResInfo);
		if(hGTmp)
			return hGTmp;
		else{
			return Real_LoadResource( AfxGetApp()->m_hInstance, hResInfo);
		}
	}else{
		return Real_LoadResource(hModule, hResInfo);
	}

}
HANDLE  (__stdcall * Real_LoadImageA)( HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad) = ::LoadImageA;
HANDLE  (__stdcall * Real_LoadImageW)( HINSTANCE hInst, LPCWSTR name, UINT type, int cx, int cy, UINT fuLoad) = ::LoadImageW;
HANDLE WINAPI Mine_LoadImageA( HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad)
{
	//SVP_LogMsg5(L"Mine_LoadImageA");
	if(CMPlayerCApp::m_hResDll){
		HANDLE hGTmp = Real_LoadImageA(hInst,  name,  type,  cx,  cy,  fuLoad);
		if(hGTmp)
			return hGTmp;
		else{
			return Real_LoadImageA(AfxGetApp()->m_hInstance,  name,  type,  cx,  cy,  fuLoad);
		}
	}else{
		return Real_LoadImageA(hInst,  name,  type,  cx,  cy,  fuLoad);
	}
}
HANDLE WINAPI Mine_LoadImageW( HINSTANCE hInst, LPCWSTR name, UINT type, int cx, int cy, UINT fuLoad)
{
	//SVP_LogMsg5(L"Mine_LoadImageW");
	if(CMPlayerCApp::m_hResDll){
		HANDLE hGTmp = Real_LoadImageW(hInst,  name,  type,  cx,  cy,  fuLoad);
		if(hGTmp)
			return hGTmp;
		else{
			return Real_LoadImageW(AfxGetApp()->m_hInstance,  name,  type,  cx,  cy,  fuLoad);
		}
	}else{
		return Real_LoadImageW(hInst,  name,  type,  cx,  cy,  fuLoad);
	}
}

int  (__stdcall * Real_LoadStringA)(  HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax) = ::LoadStringA;
int  (__stdcall * Real_LoadStringW)( HINSTANCE hInstance,UINT uID, LPWSTR lpBuffer, int cchBufferMax) = ::LoadStringW;

int WINAPI Mine_LoadStringA( HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax){
	//SVP_LogMsg5(L"Mine_LoadStringA");
	if(CMPlayerCApp::m_hResDll){
		int hGTmp = Real_LoadStringA(hInstance,  uID,  lpBuffer,  cchBufferMax);
		if(hGTmp)
			return hGTmp;
		else{
	//		SVP_LogMsg5(L"Mine_LoadStringA2");
			return Real_LoadStringA(AfxGetApp()->m_hInstance,  uID,  lpBuffer,  cchBufferMax);
		}
	}else{
		return Real_LoadStringA(hInstance,  uID,  lpBuffer,  cchBufferMax);
	}
}
int WINAPI Mine_LoadStringW( HINSTANCE hInstance,UINT uID, LPWSTR lpBuffer, int cchBufferMax){
	//SVP_LogMsg5(L"Mine_LoadStringW");
	if(CMPlayerCApp::m_hResDll){
		int hGTmp = Real_LoadStringW(hInstance,  uID,  lpBuffer,  cchBufferMax);
		if(hGTmp)
			return hGTmp;
		else{
	//		SVP_LogMsg5(L"Mine_LoadStringW2");
			return Real_LoadStringW(AfxGetApp()->m_hInstance,  uID,  lpBuffer,  cchBufferMax);
		}
	}else{
		return Real_LoadStringW(hInstance,  uID,  lpBuffer,  cchBufferMax);
	}
}
HICON  (__stdcall * Real_LoadIconA)(  HINSTANCE  hInstance, LPCSTR lpIconName) = ::LoadIconA;
HICON  (__stdcall * Real_LoadIconW)( HINSTANCE  hInstance, LPCWSTR lpIconName) = ::LoadIconW;

HICON WINAPI Mine_LoadIconA( HINSTANCE hInstance, LPCSTR lpIconName){
	if(CMPlayerCApp::m_hResDll){
		HICON hGTmp = Real_LoadIconA(hInstance,lpIconName);
		if(hGTmp)
			return hGTmp;
		else{
			return Real_LoadIconA(AfxGetApp()->m_hInstance,  lpIconName);
		}
	}else{
		return Real_LoadIconA(hInstance, lpIconName);
	}
}
HICON WINAPI Mine_LoadIconW( HINSTANCE hInstance, LPCWSTR lpIconName){
	if(CMPlayerCApp::m_hResDll){
		HICON hGTmp = Real_LoadIconW(hInstance,lpIconName);
		if(hGTmp)
			return hGTmp;
		else{
			return Real_LoadIconW(AfxGetApp()->m_hInstance,  lpIconName);
		}
	}else{
		return Real_LoadIconW(hInstance, lpIconName);
	}
}
HBITMAP  (__stdcall * Real_LoadBitmapA)(  HINSTANCE  hInstance, LPCSTR lpBitmapName) = ::LoadBitmapA;
HBITMAP  (__stdcall * Real_LoadBitmapW)( HINSTANCE  hInstance, LPCWSTR lpBitmapName) = ::LoadBitmapW;

HBITMAP WINAPI Mine_LoadBitmapA( HINSTANCE hInstance, LPCSTR lpBitmapName){
	if(CMPlayerCApp::m_hResDll){
		HBITMAP hGTmp = Real_LoadBitmapA(hInstance,lpBitmapName);
		if(hGTmp)
			return hGTmp;
		else{
			return Real_LoadBitmapA(AfxGetApp()->m_hInstance,  lpBitmapName);
		}
	}else{
		return Real_LoadBitmapA(hInstance, lpBitmapName);
	}
}
HBITMAP WINAPI Mine_LoadBitmapW( HINSTANCE hInstance, LPCWSTR lpBitmapName){
	if(CMPlayerCApp::m_hResDll){
		HBITMAP hGTmp = Real_LoadBitmapW(hInstance,lpBitmapName);
		if(hGTmp)
			return hGTmp;
		else{
			return Real_LoadBitmapW(AfxGetApp()->m_hInstance,  lpBitmapName);
		}
	}else{
		return Real_LoadBitmapW(hInstance, lpBitmapName);
	}
}

LPTOP_LEVEL_EXCEPTION_FILTER  (__stdcall * Real_SetUnhandledExceptionFilter)( LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter) = ::SetUnhandledExceptionFilter;
LPTOP_LEVEL_EXCEPTION_FILTER WINAPI Mine_SetUnhandledExceptionFilter( LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter)
{
	//SVP_LogMsg5(L"Someone is calling SetUnhandledExceptionFilter");
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
// WTL/ATL supporting logic
WTL::CAppModule _Module;
//////////////////////////////////////////////////////////////////////////


BOOL CMPlayerCApp::InitInstance()
{
	//ssftest s;

	//_CrtSetBreakAlloc(12143);

	long		lError;
	DetourRestoreAfterWith();
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	DetourAttach(&(PVOID&)Real_IsDebuggerPresent, (PVOID)Mine_IsDebuggerPresent);
	DetourAttach(&(PVOID&)Real_ChangeDisplaySettingsExA, (PVOID)Mine_ChangeDisplaySettingsExA);
	DetourAttach(&(PVOID&)Real_ChangeDisplaySettingsExW, (PVOID)Mine_ChangeDisplaySettingsExW);
	DetourAttach(&(PVOID&)Real_CreateFileA, (PVOID)Mine_CreateFileA);
	DetourAttach(&(PVOID&)Real_CreateFileW, (PVOID)Mine_CreateFileW);
	DetourAttach(&(PVOID&)Real_mixerSetControlDetails, (PVOID)Mine_mixerSetControlDetails);
	DetourAttach(&(PVOID&)Real_DeviceIoControl, (PVOID)Mine_DeviceIoControl);
	DetourAttach(&(PVOID&)Real_GetModuleFileNameA, (PVOID)Mine_GetModuleFileNameA);

    
    DetourAttach(&(PVOID&)Real_WritePrivateProfileStringA, (PVOID)Mine_WritePrivateProfileStringA);
    DetourAttach(&(PVOID&)Real_GetPrivateProfileStringA, (PVOID)Mine_GetPrivateProfileStringA);
    
	DetourAttach(&(PVOID&)Real_LoadResource, (PVOID)Mine_LoadResource);
	DetourAttach(&(PVOID&)Real_LoadImageW, (PVOID)Mine_LoadImageW);
	DetourAttach(&(PVOID&)Real_LoadImageA, (PVOID)Mine_LoadImageA);
	DetourAttach(&(PVOID&)Real_LoadStringW, (PVOID)Mine_LoadStringW);
	DetourAttach(&(PVOID&)Real_LoadStringA, (PVOID)Mine_LoadStringA);
	DetourAttach(&(PVOID&)Real_LoadIconW, (PVOID)Mine_LoadIconW);
	DetourAttach(&(PVOID&)Real_LoadIconA, (PVOID)Mine_LoadIconA);
	DetourAttach(&(PVOID&)Real_LoadBitmapW, (PVOID)Mine_LoadBitmapW);
	DetourAttach(&(PVOID&)Real_LoadBitmapA, (PVOID)Mine_LoadBitmapA);

	DetourAttach(&(PVOID&)Real_SetUnhandledExceptionFilter, (PVOID)Mine_SetUnhandledExceptionFilter);

#ifndef _DEBUG
	HMODULE hNTDLL	=	LoadLibrary (_T("ntdll.dll"));
	if (hNTDLL)
	{
		Real_NtQueryInformationProcess = (FUNC_NTQUERYINFORMATIONPROCESS)GetProcAddress (hNTDLL, "NtQueryInformationProcess");

		if (Real_NtQueryInformationProcess)
			DetourAttach(&(PVOID&)Real_NtQueryInformationProcess, (PVOID)Mine_NtQueryInformationProcess);
	}
#endif
	CFilterMapper2::Init();

#if !defined(_DEBUG) || !defined(_WIN64)
	lError = DetourTransactionCommit();
	ASSERT (lError == NOERROR);
#endif
	HRESULT hr;
    if(FAILED(hr = OleInitialize(0)))
	{
        AfxMessageBox(_T("OleInitialize failed!"));
		return FALSE;
	}

  //////////////////////////////////////////////////////////////////////////
  // WTL/ATL supporting logic
  ::CoInitialize(NULL);
  ::DefWindowProc(NULL, 0, 0, 0L);
  WTL::AtlInitCommonControls(ICC_BAR_CLASSES);
  _Module.Init(NULL, AfxGetInstanceHandle());
  //////////////////////////////////////////////////////////////////////////
	
	SetLanguage(-1);

    WNDCLASS wndcls;
    memset(&wndcls, 0, sizeof(WNDCLASS));
    wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wndcls.lpfnWndProc = ::DefWindowProc; 
    wndcls.hInstance = AfxGetInstanceHandle();
    wndcls.hIcon = LoadIcon(IDR_MAINFRAME);
    wndcls.hCursor = LoadCursor(IDC_ARROW);
    wndcls.hbrBackground = 0;//(HBRUSH)(COLOR_WINDOW + 1); // no bkg brush, the view and the bars should always fill the whole client area
    wndcls.lpszMenuName = NULL;
    wndcls.lpszClassName = MPC_WND_CLASS_NAME;

	if(!AfxRegisterClass(&wndcls))
    {
		AfxMessageBox(_T("MainFrm class registration failed!"));
		return FALSE;
    }

	if(!AfxSocketInit(NULL))
	{
        AfxMessageBox(_T("AfxSocketInit failed!"));
		return FALSE;
	}


	PreProcessCommandLine();

	//if(IsIniValid()) 
	StoreSettingsToRegistry();
	StoreSettingsToIni();
	// else StoreSettingsToRegistry();
	//SwapMouseButton (true);
	CString AppDataPath;
	if(GetAppDataPath(AppDataPath))
		CreateDirectory(AppDataPath, NULL);

	m_s.ParseCommandLine(m_cmdln);

	if(m_s.nCLSwitches&(CLSW_HELP|CLSW_UNRECOGNIZEDSWITCH))
	{
		//if(m_s.nCLSwitches&CLSW_HELP)
			//ShowCmdlnSwitches();

		return FALSE;
	}

	if((m_s.nCLSwitches&CLSW_CLOSE) && m_s.slFiles.IsEmpty())
	{
		return FALSE;
	}
	m_pDwmIsCompositionEnabled = NULL;
	m_pDwmEnableComposition = NULL;
	m_pDwmExtendFrameIntoClientArea  = NULL;
	m_pDwmDefWindowProc = NULL;
	m_pOpenThemeData = NULL;
	m_pGetLayeredWindowAttributes = NULL;
	m_pGetThemeSysFont = NULL;
	m_pDrawThemeTextEx = NULL;
	m_pCloseThemeData = NULL;

	HMODULE hDWMAPI = LoadLibrary(L"dwmapi.dll");
	if (hDWMAPI)
	{
		(FARPROC &)m_pDwmIsCompositionEnabled = GetProcAddress(hDWMAPI, "DwmIsCompositionEnabled");
		(FARPROC &)m_pDwmEnableComposition = GetProcAddress(hDWMAPI, "DwmEnableComposition");
		(FARPROC &)m_pDwmExtendFrameIntoClientArea = GetProcAddress(hDWMAPI, "DwmExtendFrameIntoClientArea");
		(FARPROC &)m_pDwmDefWindowProc = GetProcAddress(hDWMAPI, "DwmDefWindowProc");

		BOOL bCompositionEnabled =  true;
		if(m_pDwmIsCompositionEnabled)
			m_pDwmIsCompositionEnabled(&bCompositionEnabled);

		m_s.bAeroGlassAvalibility = bCompositionEnabled;

	}else
		m_s.bAeroGlassAvalibility = false;

	HMODULE hUSER32 = LoadLibrary(L"user32.dll");
	if(hUSER32){
		(FARPROC &)m_pGetLayeredWindowAttributes =  GetProcAddress(hUSER32, "GetLayeredWindowAttributes");
	}

	HMODULE hUXAPI = LoadLibrary(L"uxtheme.dll");
	if (hUXAPI)
	{
		(FARPROC &)m_pOpenThemeData = GetProcAddress(hUXAPI, "OpenThemeData");
		(FARPROC &)m_pGetThemeSysFont = GetProcAddress(hUXAPI, "GetThemeSysFont");
		(FARPROC &)m_pDrawThemeTextEx = GetProcAddress(hUXAPI, "DrawThemeTextEx");
		(FARPROC &)m_pCloseThemeData = GetProcAddress(hUXAPI, "CloseThemeData");

	}else
		m_s.bAeroGlassAvalibility = false;

	HMODULE hD3D9 = LoadLibrary(L"d3d9.dll");
	if (hD3D9)
		(FARPROC &)m_pDirect3DCreate9Ex = GetProcAddress(hD3D9, "Direct3DCreate9Ex");
	else
		m_pDirect3DCreate9Ex = NULL;

	m_s.UpdateData(false);
	if (m_s.nCLSwitches & CLSW_ADMINOPTION)
	{
		switch (m_s.iAdminOption)
		{
		case 1:
			{
				CPPageSheet options(ResStr(IDS_OPTIONS_CAPTION), NULL, NULL, CPPageFormats::IDD );
				options.DoModal();
				return FALSE;
			}
			break;
		case 2:
			{

				CDlgChkUpdater dlgChkUpdater;
				dlgChkUpdater.DoModal();
					
				return FALSE;
			}
			break;
		default :
			CChkDefPlayer dlg_chkdefplayer;
			dlg_chkdefplayer.b_isDefaultPlayer();
			dlg_chkdefplayer.setDefaultPlayer();
			return FALSE;
		}
		
	}

	if((m_s.nCLSwitches&CLSW_REGEXTVID) || (m_s.nCLSwitches&CLSW_REGEXTAUD))
	{
		CMediaFormats& mf = m_s.Formats;

		for(size_t i = 0; i < mf.GetCount(); i++)
		{
			if(!mf[i].GetLabel().CompareNoCase(_T("Image file"))) continue;
			if(!mf[i].GetLabel().CompareNoCase(_T("Subtitle file"))) continue;
			if(mf[i].GetLabel().Find(_T("Playlist file")) == -1) continue;
			if(mf[i].IsAudioOnly() < 0) continue;
				
			int fAudioOnly = mf[i].IsAudioOnly();

			int j = 0;
			CString str = mf[i].GetExtsWithPeriod();
            CString szPerceivedType = mf[i].getPerceivedType();
			for(CString ext = str.Tokenize(_T(" "), j); !ext.IsEmpty(); ext = str.Tokenize(_T(" "), j))
			{
				if(((m_s.nCLSwitches&CLSW_REGEXTVID) && fAudioOnly != 1) || ((m_s.nCLSwitches&CLSW_REGEXTAUD) && fAudioOnly == 0 )) {
                    
					CPPageFormats::RegisterExt(ext, true, szPerceivedType);
				}
			}
		}

		return FALSE;
	}
	
	if((m_s.nCLSwitches&CLSW_UNREGEXT))
	{
		CMediaFormats& mf = m_s.Formats;

		for(size_t i = 0; i < mf.GetCount(); i++)
		{
			int j = 0;
			CString str = mf[i].GetExtsWithPeriod();
            CString pType = mf[i].getPerceivedType();
			for(CString ext = str.Tokenize(_T(" "), j); !ext.IsEmpty(); ext = str.Tokenize(_T(" "), j))
			{
				CPPageFormats::RegisterExt(ext, false , pType);
			}
		}

        CPPageFormats cpf;
        cpf.AddAutoPlayToRegistry(cpf.AP_VIDEO, false);
        cpf.AddAutoPlayToRegistry(cpf.AP_DVDMOVIE, false);
        cpf.AddAutoPlayToRegistry(cpf.AP_AUDIOCD, false);
        cpf.AddAutoPlayToRegistry(cpf.AP_MUSIC, false);
        cpf.AddAutoPlayToRegistry(cpf.AP_SVCDMOVIE, false);
        cpf.AddAutoPlayToRegistry(cpf.AP_VCDMOVIE, false);
        cpf.AddAutoPlayToRegistry(cpf.AP_BDMOVIE, false);
        cpf.AddAutoPlayToRegistry(cpf.AP_DVDAUDIO, false);
        cpf.AddAutoPlayToRegistry(cpf.AP_CAPTURECAMERA, false);


		return FALSE;
	}

	m_mutexOneInstance.Create(NULL, TRUE, MPC_WND_CLASS_NAME);

	CString dumpMsg;
	if(GetLastError() == ERROR_ALREADY_EXISTS
	&& (!(m_s.fAllowMultipleInst || (m_s.nCLSwitches&CLSW_NEW) || m_cmdln.IsEmpty())
		|| (m_s.nCLSwitches&CLSW_ADD)))
	{
		if(HWND hWnd = ::FindWindow(MPC_WND_CLASS_NAME, NULL))
		{
			{

				SetForegroundWindow(hWnd);
				//AfxMessageBox(_T("5")); 

				if(!(m_s.nCLSwitches&CLSW_MINIMIZED) && IsIconic(hWnd))
					ShowWindow(hWnd, SW_RESTORE);

				if(SendCommandLine(hWnd, true)){

					//AfxMessageBox(_T("6"));

					return FALSE;
				}else{
					//AfxMessageBox(_T("should create dump for that hwnd"));
					DWORD pId = NULL;
					DWORD dwTid = GetWindowThreadProcessId(hWnd, &pId);
					HANDLE deadProcess = OpenProcess( PROCESS_TERMINATE  , false , pId);
					if(deadProcess){
						dumpMsg = DebugMiniDumpProcess(deadProcess, pId, dwTid);
						TerminateProcess(deadProcess, 0);
						CloseHandle(deadProcess);
					}
				}
				
			}
		}
	}


	
	if(!__super::InitInstance())
	{
		AfxMessageBox(_T("InitInstance failed!"));
		return FALSE;
	}

	//int aElements[3] = {COLOR_BTNHIGHLIGHT, COLOR_BTNSHADOW, COLOR_WINDOW};

	//DWORD aNewColors[3] = {0xeeeeee, 0xb2b2b2, 0xdddddd};
	//SetSysColors(3, aElements, aNewColors); 


	
	CRegKey key;
	if(ERROR_SUCCESS == key.Create(HKEY_LOCAL_MACHINE, CString(L"Software\\SPlayer\\")+ResStr(IDR_MAINFRAME)))
	{
		CString path;
		GetModuleFileName(AfxGetInstanceHandle(), path.GetBuffer(MAX_PATH), MAX_PATH);
		path.ReleaseBuffer();
		key.SetStringValue(_T("ExePath"), path);
	}

	AfxEnableControlContainer();
	CMainFrame* pFrame = new CMainFrame;
	m_pMainWnd = pFrame;
	pFrame->LoadFrame(IDR_MAINFRAME, WS_OVERLAPPEDWINDOW|FWS_ADDTOTITLE, NULL, NULL);
	pFrame->SetDefaultWindowRect((m_s.nCLSwitches&CLSW_MONITOR)?m_s.iMonitor:0);
	pFrame->RestoreFloatingControlBars();
	pFrame->SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), TRUE);
	pFrame->DragAcceptFiles();
	pFrame->ShowWindow((m_s.nCLSwitches&CLSW_MINIMIZED)?SW_SHOWMINIMIZED:SW_SHOW);
	pFrame->UpdateWindow();
	pFrame->m_hAccelTable = m_s.hAccel;

	CWinThread* th_InitInstance = AfxBeginThread(Thread_InitInstance , this,  THREAD_PRIORITY_LOWEST, 0, CREATE_SUSPENDED);
	th_InitInstance->m_pMainWnd = AfxGetMainWnd();
	th_InitInstance->ResumeThread();

	m_s.WinLircClient.SetHWND(m_pMainWnd->m_hWnd);
	if(m_s.fWinLirc) m_s.WinLircClient.Connect(m_s.WinLircAddr);
	m_s.UIceClient.SetHWND(m_pMainWnd->m_hWnd);
	if(m_s.fUIce) m_s.UIceClient.Connect(m_s.UIceAddr);

	SendCommandLine(m_pMainWnd->m_hWnd);

	pFrame->SetFocus();

	if(!dumpMsg.IsEmpty())
		pFrame->SendStatusMessage(dumpMsg, 5000);

	return TRUE;
}

int CMPlayerCApp::ExitInstance()
{

	if(sqlite_setting)
		sqlite_setting->exec_sql("PRAGMA synchronous=ON");
    if(sqlite_local_record){
        CString szSQL;
        szSQL.Format(L"DELETE FROM histories WHERE modtime < '%d' ", time(NULL)-3600*24*30);
        // SVP_LogMsg5(szSQL);
        sqlite_local_record->exec_sql_u(szSQL);

        sqlite_local_record->exec_sql("PRAGMA synchronous=ON");
    }
	m_s.UpdateData(true);


	if(!IsVista()){
		BOOL bDropShadow = TRUE;
		if(m_bSystemParametersInfo[0])
			SystemParametersInfo(SPI_SETDROPSHADOW , 0,    &bDropShadow,	0) ;
			
		
	}

	CSVPToolBox svpToolBox;
	UnRegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mtcontain.dll")) );
	UnRegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mxrender.dll")) );
	UnRegSvr32( svpToolBox.GetPlayerPath(_T("csfcodec\\mpc_mxvideo.dll")) );
	OleUninitialize();

  //////////////////////////////////////////////////////////////////////////
  // WTL/ATL supporting logic
  _Module.Term();
  ::CoUninitialize();
  //////////////////////////////////////////////////////////////////////////

	int ret = CWinApp::ExitInstance();

	/*if ( m_s.fCheckFileAsscOnStartup ){
		if( IsVista() && IsUserAnAdmin() ){
			CChkDefPlayer dlg_chkdefplayer;
			dlg_chkdefplayer.setDefaultPlayer(2);
		}

	}*/

	if(m_hResDll)
		FreeLibrary(m_hResDll);
	
	if (sqlite_setting)
		delete sqlite_setting;
    if (sqlite_local_record)
        delete sqlite_local_record;
    

	return ret;
}

/////////////////////////////////////////////////////////////////////////////
// CMPlayerCApp message handlers
// App command to run the dialog

void CMPlayerCApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

void CMPlayerCApp::OnFileExit()
{
	OnAppExit();
}

// CRemoteCtrlClient

CRemoteCtrlClient::CRemoteCtrlClient() 
	: m_pWnd(NULL)
	, m_nStatus(DISCONNECTED)
{
}

void CRemoteCtrlClient::SetHWND(HWND hWnd)
{
	CAutoLock cAutoLock(&m_csLock);

	m_pWnd = CWnd::FromHandle(hWnd);
}

void CRemoteCtrlClient::Connect(CString addr)
{
	CAutoLock cAutoLock(&m_csLock);

	if(m_nStatus == CONNECTING && m_addr == addr)
	{
		TRACE(_T("CRemoteCtrlClient (Connect): already connecting to %s\n"), addr);
		return;
	}

	if(m_nStatus == CONNECTED && m_addr == addr)
	{
		TRACE(_T("CRemoteCtrlClient (Connect): already connected to %s\n"), addr);
		return;
	}

	m_nStatus = CONNECTING;

	TRACE(_T("CRemoteCtrlClient (Connect): connecting to %s\n"), addr);

	Close();

	Create();

	CString ip = addr.Left(addr.Find(':')+1).TrimRight(':');
	int port = _tcstol(addr.Mid(addr.Find(':')+1), NULL, 10);

	__super::Connect(ip, port);

	m_addr = addr;
}

void CRemoteCtrlClient::OnConnect(int nErrorCode)
{
	CAutoLock cAutoLock(&m_csLock);

	m_nStatus = (nErrorCode == 0 ? CONNECTED : DISCONNECTED);

	TRACE(_T("CRemoteCtrlClient (OnConnect): %d\n"), nErrorCode);
}

void CRemoteCtrlClient::OnClose(int nErrorCode)
{
	CAutoLock cAutoLock(&m_csLock);

	if(m_hSocket != INVALID_SOCKET && m_nStatus == CONNECTED)
	{
		TRACE(_T("CRemoteCtrlClient (OnClose): connection lost\n"));
	}

	m_nStatus = DISCONNECTED;

	TRACE(_T("CRemoteCtrlClient (OnClose): %d\n"), nErrorCode);
}

void CRemoteCtrlClient::OnReceive(int nErrorCode)
{
	if(nErrorCode != 0 || !m_pWnd) return;

	CStringA str;
	int ret = Receive(str.GetBuffer(256), 255, 0);
	if(ret <= 0) return;
	str.ReleaseBuffer(ret);

	TRACE(_T("CRemoteCtrlClient (OnReceive): %s\n"), CString(str));

	OnCommand(str);

	__super::OnReceive(nErrorCode);
}

void CRemoteCtrlClient::ExecuteCommand(CStringA cmd, int repcnt)
{
	cmd.Trim();
	if(cmd.IsEmpty()) return;
	cmd.Replace(' ', '_');

	AppSettings& s = AfxGetAppSettings();

	POSITION pos = s.wmcmds.GetHeadPosition();
	while(pos)
	{
		wmcmd wc = s.wmcmds.GetNext(pos);
		CStringA name = CString(wc.name);
		name.Replace(' ', '_');
		if((repcnt == 0 && wc.rmrepcnt == 0 || wc.rmrepcnt > 0 && (repcnt%wc.rmrepcnt) == 0)
		&& (!name.CompareNoCase(cmd) || !wc.rmcmd.CompareNoCase(cmd) || wc.cmd == (WORD)strtol(cmd, NULL, 10)))
		{
			CAutoLock cAutoLock(&m_csLock);
			TRACE(_T("CRemoteCtrlClient (calling command): %s\n"), wc.name);
			m_pWnd->SendMessage(WM_COMMAND, wc.cmd);
			break;
		}
	}
}

// CWinLircClient

CWinLircClient::CWinLircClient()
{
}

void CWinLircClient::OnCommand(CStringA str)
{
	TRACE(_T("CWinLircClient (OnCommand): %s\n"), CString(str));

	int i = 0, j = 0, repcnt = 0;
	for(CStringA token = str.Tokenize(" ", i); 
		!token.IsEmpty();
		token = str.Tokenize(" ", i), j++)
	{
		if(j == 1)
			repcnt = strtol(token, NULL, 16);
		else if(j == 2)
			ExecuteCommand(token, repcnt);
	}
}

// CUIceClient

CUIceClient::CUIceClient()
{
}

void CUIceClient::OnCommand(CStringA str)
{
	TRACE(_T("CUIceClient (OnCommand): %s\n"), CString(str));

	CStringA cmd;
	int i = 0, j = 0;
	for(CStringA token = str.Tokenize("|", i); 
		!token.IsEmpty(); 
		token = str.Tokenize("|", i), j++)
	{
		if(j == 0)
			cmd = token;
		else if(j == 1)
			ExecuteCommand(cmd, strtol(token, NULL, 16));
	}
}

// CMPlayerCApp::Settings

CMPlayerCApp::Settings::Settings() 
	: fInitialized(false)
	, MRU(0, _T("Recent File List"), _T("File%d"), 20)
	, MRUDub(0, _T("Recent Dub List"), _T("Dub%d"), 20)
	, MRUUrl(0, _T("Recent Url List"), _T("Url%d"), 20)
	, hAccel(NULL)
	, bSetTempChannelMaping(0)
	, htpcmode(0)
	, bNoMoreDXVAForThisFile(0)
	, bDisableSoftCAVC(false)
    , bUsePowerDVD()
{

}

CMPlayerCApp::Settings::~Settings()
{
	if(hAccel)
		DestroyAcceleratorTable(hAccel);
}
void CMPlayerCApp::Settings::RegGlobalAccelKey(HWND hWnd){
	if(!hWnd) {
		if(AfxGetMyApp()->m_pMainWnd)
			hWnd = AfxGetMyApp()->m_pMainWnd->m_hWnd;
		if(!hWnd) 
			return;
	}


	POSITION pos = wmcmds.GetHeadPosition();
	while(pos){
		wmcmd& wc = wmcmds.GetNext(pos);
		if(wc.name == ResStr(IDS_HOTKEY_BOSS_KEY)){
			UINT modKey = 0;
			if( wc.fVirt & FCONTROL) {modKey |= MOD_CONTROL;}
			if( wc.fVirt & FALT) {modKey |= MOD_ALT;}
			if( wc.fVirt & FSHIFT) {modKey |= MOD_SHIFT;}
			UnregisterHotKey(hWnd, ID_BOSS);
			RegisterHotKey(hWnd, ID_BOSS, modKey, wc.key); 
		}
	}
}
void CMPlayerCApp::Settings::ThreadedLoading(){
	
	CMPlayerCApp * pApp  = AfxGetMyApp();
	SVP_LogMsg5(L"Settings::ThreadedLoading");
	CMainFrame* pFrame = (CMainFrame*)pApp->m_pMainWnd;
	while(!pFrame || pFrame->m_WndSizeInited < 2){
		Sleep(1000);
		pFrame = (CMainFrame*)pApp->m_pMainWnd;
	}
	CSVPToolBox svptoolbox;
	SVP_LogMsg5(L"Settings::ThreadedLoading 1");
	BOOL bHadYaheiDownloaded = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS),  _T("HasYaheiDownloaded"), 0); //默认检查是否使用旧字体

	CString szTTFPath = svptoolbox.GetPlayerPath( _T("msyh.ttf") );
	if( svptoolbox.ifFileExist(szTTFPath)) {
		if( AddFontResourceEx( szTTFPath , FR_PRIVATE, 0) ){

			if(bHadYaheiDownloaded != 1)  //首次成功调入了外部字体，下次不再检查是否使用旧字体
				pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), _T("HasYaheiDownloaded"), 1 );	
		}else{
			if(bHadYaheiDownloaded != 0)  //没有成功调入外部字体，下次检查是否使用旧字体
				pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), _T("HasYaheiDownloaded"), 0 );	
		}
	}
    SVP_LogMsg5(L"Settings::ThreadedLoading 2");
	if( bIsChineseUIUser() ){
		if(!svptoolbox.bFontExist(_T("微软雅黑")) && svptoolbox.bFontExist(_T("文泉驿微米黑"))){ 
			if(subdefstyle.fontName.CompareNoCase(_T("黑体") ) == 0 )
				subdefstyle.fontName = _T("文泉驿微米黑");
			if(subdefstyle2.fontName.CompareNoCase(_T("黑体") ) == 0 )
				subdefstyle2.fontName = _T("文泉驿微米黑");
			if(subdefstyle.fontName.CompareNoCase(_T("微软雅黑") ) == 0 )
				subdefstyle.fontName = _T("文泉驿微米黑");
			if(subdefstyle2.fontName.CompareNoCase(_T("微软雅黑") ) == 0 )
				subdefstyle2.fontName = _T("文泉驿微米黑");
		}
		if(!svptoolbox.bFontExist(_T("Microsoft YaHei")) && svptoolbox.bFontExist(_T("WenQuanYi Micro Hei"))){ 
			if(subdefstyle.fontName.CompareNoCase(_T("SimHei") ) == 0 )
				subdefstyle.fontName = _T("WenQuanYi Micro Hei");
			if(subdefstyle2.fontName.CompareNoCase(_T("SimHei") ) == 0 )
				subdefstyle2.fontName = _T("WenQuanYi Micro Hei");

			if(subdefstyle.fontName.CompareNoCase(_T("Microsoft YaHei") ) == 0 )
				subdefstyle.fontName = _T("WenQuanYi Micro Hei");
			if(subdefstyle2.fontName.CompareNoCase(_T("Microsoft YaHei") ) == 0 )
				subdefstyle2.fontName = _T("WenQuanYi Micro Hei");
		}
		if(!bNotChangeFontToYH){
			if(!svptoolbox.bFontExist(_T("文泉驿微米黑")) && !svptoolbox.bFontExist(_T("Microsoft YaHei")) 
				 && !svptoolbox.bFontExist(_T("Microsoft YaHei"))  && !svptoolbox.bFontExist(_T("WenQuanYi Micro Hei"))){ 
				
			}
			if(!bHadYaheiDownloaded ){
				if(svptoolbox.bFontExist(_T("微软雅黑"))){ //
					if(subdefstyle.fontName.CompareNoCase(_T("黑体") ) == 0 )
						subdefstyle.fontName = _T("微软雅黑");
					if(subdefstyle2.fontName.CompareNoCase(_T("黑体") ) == 0 )
						subdefstyle2.fontName = _T("微软雅黑");

				}else if(svptoolbox.bFontExist(_T("文泉驿微米黑"))){ //
					if(subdefstyle.fontName.CompareNoCase(_T("黑体") ) == 0 )
						subdefstyle.fontName = _T("文泉驿微米黑");
					if(subdefstyle2.fontName.CompareNoCase(_T("黑体") ) == 0 )
						subdefstyle2.fontName = _T("文泉驿微米黑");
					if(subdefstyle.fontName.CompareNoCase(_T("微软雅黑") ) == 0 )
						subdefstyle.fontName = _T("文泉驿微米黑");
					if(subdefstyle2.fontName.CompareNoCase(_T("微软雅黑") ) == 0 )
						subdefstyle2.fontName = _T("文泉驿微米黑");

				}else if( !svptoolbox.bFontExist(_T("黑体")) ){
					if(subdefstyle.fontName.CompareNoCase(_T("黑体") ) == 0 )
						subdefstyle.fontName = _T("SimHei");
					if(subdefstyle2.fontName.CompareNoCase(_T("黑体") ) == 0 )
						subdefstyle2.fontName = _T("SimHei");
				}
				if(svptoolbox.bFontExist(_T("Microsoft YaHei"))){ //Microsoft YaHei
					if(subdefstyle.fontName.CompareNoCase(_T("SimHei") ) == 0 )
						subdefstyle.fontName = _T("Microsoft YaHei");
					if(subdefstyle2.fontName.CompareNoCase(_T("SimHei") ) == 0 )
						subdefstyle2.fontName = _T("Microsoft YaHei");
				}else if(svptoolbox.bFontExist(_T("WenQuanYi Micro Hei"))){ //WenQuanYi Micro Hei
					if(subdefstyle.fontName.CompareNoCase(_T("SimHei") ) == 0 )
						subdefstyle.fontName = _T("WenQuanYi Micro Hei");
					if(subdefstyle2.fontName.CompareNoCase(_T("SimHei") ) == 0 )
						subdefstyle2.fontName = _T("WenQuanYi Micro Hei");

					if(subdefstyle.fontName.CompareNoCase(_T("Microsoft YaHei") ) == 0 )
						subdefstyle.fontName = _T("WenQuanYi Micro Hei");
					if(subdefstyle2.fontName.CompareNoCase(_T("Microsoft YaHei") ) == 0 )
						subdefstyle2.fontName = _T("WenQuanYi Micro Hei");
				}
			}
			
		}
	}
    SVP_LogMsg5(L"Settings::ThreadedLoading 3");
/*

	CCpuId m_CPU;
	if( m_CPU.GetFeatures() & (m_CPU.MPC_MM_SSE4|m_CPU.MPC_MM_SSE42| m_CPU.MPC_MM_SSE4A)){
		bDisableSoftCAVC = true;
	}else{
		bDisableSoftCAVC = false;
	}
    */
}
UINT __cdecl Thread_AppSettingLoadding( LPVOID lpParam ) 
{ 
	CMPlayerCApp::Settings * ms =(CMPlayerCApp::Settings*) lpParam;
	ms->ThreadedLoading();
	return 0; 
}
int CMPlayerCApp::Settings::FindWmcmdsIDXofCmdid(UINT cmdid, POSITION pos){
	int cmdIndex = 0;
	POSITION posc = 0;
	while(posc = wmcmds.Find(cmdid, posc)){
// 		CString szLog;
// 		szLog.Format(_T("%ul %ul"), posc , pos );
// 		SVP_LogMsg(szLog);
		if(posc == pos){
			return cmdIndex;
		}
		cmdIndex++;
	}
	return -1;
}
POSITION CMPlayerCApp::Settings::FindWmcmdsPosofCmdidByIdx(INT cmdid, int idx){
	int cmdIndex = 0;
	POSITION posc = 0;
	while(posc = wmcmds.Find(cmdid, posc)){
		if(cmdIndex == idx || idx < 0){
			return posc;
		}
		cmdIndex++;
	}
	return 0;
}

void CMPlayerCApp::Settings::SetNumberOfSpeakers( int iSS , int iNumberOfSpeakers){

	if(iNumberOfSpeakers == -1){
		iNumberOfSpeakers = GetNumberOfSpeakers();
	}
	if(!iSS){
		switch( iNumberOfSpeakers ){
			case 1: iDecSpeakers = 100;	break;
			case 2: iDecSpeakers = 200;	break;
			case 3: iDecSpeakers = 210;	break;
			case 4: iDecSpeakers = 220;	break;
			case 5: iDecSpeakers = 221;	break;
			case 6: iDecSpeakers = 321;	break;
			case 7: iDecSpeakers = 341;	break;
			case 8: iDecSpeakers = 341;	break;
		}
		iSS = iDecSpeakers;
	}else{
		iSS = abs(iSS);
		iDecSpeakers = iSS;
	}
}
/*
#define SPEAKER_FRONT_LEFT              0x1
#define SPEAKER_FRONT_RIGHT             0x2
#define SPEAKER_FRONT_CENTER            0x4
#define SPEAKER_LOW_FREQUENCY           0x8
#define SPEAKER_BACK_LEFT               0x10
#define SPEAKER_BACK_RIGHT              0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER    0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER   0x80
#define SPEAKER_BACK_CENTER             0x100
#define SPEAKER_SIDE_LEFT               0x200
#define SPEAKER_SIDE_RIGHT              0x400
#define SPEAKER_TOP_CENTER              0x800
#define SPEAKER_TOP_FRONT_LEFT          0x1000
#define SPEAKER_TOP_FRONT_CENTER        0x2000
#define SPEAKER_TOP_FRONT_RIGHT         0x4000
#define SPEAKER_TOP_BACK_LEFT           0x8000
#define SPEAKER_TOP_BACK_CENTER         0x10000
#define SPEAKER_TOP_BACK_RIGHT          0x20000


void CMPlayerCApp::Settings::SetChannelMapByNumberOfSpeakers( int iSS , int iNumberOfSpeakers){

	if(bSetTempChannelMaping)
		return;
//SVP_LogMsg5(L"iNumberOfSpeakers2 %d", iNumberOfSpeakers);
	if(iNumberOfSpeakers == -1){
		iNumberOfSpeakers = GetNumberOfSpeakers();
	}
	/*
	memset(pSpeakerToChannelMap, 0, sizeof(pSpeakerToChannelMap));
 	//for(int j = 0; j < 18; j++)
 	//	for(int i = 0; i <= j; i++)
 	//		pSpeakerToChannelMap[j][i] = 1<<i;
	if(!iSS){
		switch( iNumberOfSpeakers ){
				case 1: iDecSpeakers = 100;	break;
				case 2: iDecSpeakers = 200;	break;
				case 3: iDecSpeakers = 210;	break;
				case 4: iDecSpeakers = 220;	break;
				case 5: iDecSpeakers = 221;	break;
				case 6: iDecSpeakers = 321;	break;
				case 7: iDecSpeakers = 341;	break;
				case 8: iDecSpeakers = 341;	break;
		}
		iSS = iDecSpeakers;
	}else{
		iSS = abs(iSS);
		iDecSpeakers = iSS;
		//iNumberOfSpeakers =  (iSS % 10) + ( (int)(iSS/10) % 10 ) + ( (int)(iSS/100) % 10 ) ;
	}
//	SVP_LogMsg5(L"iNumberOfSpeakers %d", iNumberOfSpeakers);
	switch(iNumberOfSpeakers){
		case 1:
			//2 Channel
			pSpeakerToChannelMap[1][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT;
			//pSpeakerToChannelMap[1][1] = 0;

			//3 Channel
			pSpeakerToChannelMap[2][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER;
			//pSpeakerToChannelMap[2][1] = pSpeakerToChannelMap[2][2] = 0;

			//4 Channel
			pSpeakerToChannelMap[3][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT;
			//pSpeakerToChannelMap[3][1] = pSpeakerToChannelMap[3][2] = pSpeakerToChannelMap[3][3] = 0;

			//5 Channel
			pSpeakerToChannelMap[4][0] = SPEAKER_FRONT_LEFT|SPEAKER_BACK_LEFT|SPEAKER_LOW_FREQUENCY|SPEAKER_FRONT_RIGHT|SPEAKER_BACK_RIGHT;
			//pSpeakerToChannelMap[4][1] = pSpeakerToChannelMap[4][2] = pSpeakerToChannelMap[4][3] = pSpeakerToChannelMap[4][4]= 0;

			//6 Channel
			pSpeakerToChannelMap[5][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER|SPEAKER_BACK_LEFT|SPEAKER_LOW_FREQUENCY|SPEAKER_FRONT_RIGHT|SPEAKER_BACK_RIGHT ;
			//pSpeakerToChannelMap[5][1] = pSpeakerToChannelMap[5][2] = pSpeakerToChannelMap[5][3] = pSpeakerToChannelMap[5][4] = pSpeakerToChannelMap[5][5] = 0;

			//7 Channel
			pSpeakerToChannelMap[6][0] = 0xff ;
			//pSpeakerToChannelMap[6][1] = pSpeakerToChannelMap[6][2] = pSpeakerToChannelMap[6][3] = pSpeakerToChannelMap[6][4] = pSpeakerToChannelMap[6][5] =  pSpeakerToChannelMap[6][6] = pSpeakerToChannelMap[6][7] =0;

			//8 Channel
			pSpeakerToChannelMap[7][0] = 0xff;
			//pSpeakerToChannelMap[7][1] = pSpeakerToChannelMap[7][2] = pSpeakerToChannelMap[7][3] = pSpeakerToChannelMap[7][4] = pSpeakerToChannelMap[7][5] = pSpeakerToChannelMap[7][6] = pSpeakerToChannelMap[7][7] = 0;
			break;
		default: //2 
			pSpeakerToChannelMap[0][0] = pSpeakerToChannelMap[0][1] = 1;

			//pSpeakerToChannelMap[1][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER|SPEAKER_BACK_LEFT|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_LEFT_OF_CENTER|SPEAKER_SIDE_LEFT;
			//pSpeakerToChannelMap[1][1] = SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_BACK_RIGHT|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_RIGHT_OF_CENTER|SPEAKER_SIDE_RIGHT ;

			//3 Channel
			pSpeakerToChannelMap[2][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[2][1] = SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER;
			//pSpeakerToChannelMap[2][2] = 0;

			//4 Channel
			pSpeakerToChannelMap[3][0] = SPEAKER_FRONT_LEFT|SPEAKER_BACK_LEFT|SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[3][1] =  SPEAKER_FRONT_RIGHT|SPEAKER_BACK_RIGHT|SPEAKER_FRONT_CENTER;
			//pSpeakerToChannelMap[3][2] = pSpeakerToChannelMap[3][3] = 0;

			//5 Channel
			pSpeakerToChannelMap[4][0] = SPEAKER_FRONT_LEFT|SPEAKER_BACK_LEFT|SPEAKER_LOW_FREQUENCY|SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[4][1] =  SPEAKER_FRONT_RIGHT|SPEAKER_BACK_RIGHT|SPEAKER_LOW_FREQUENCY|SPEAKER_FRONT_CENTER;
			//pSpeakerToChannelMap[4][2] = pSpeakerToChannelMap[4][3] = pSpeakerToChannelMap[4][4]= 0;

			//6 Channel
			pSpeakerToChannelMap[5][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER|SPEAKER_BACK_LEFT|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[5][1] = SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_BACK_RIGHT|SPEAKER_LOW_FREQUENCY ;
			//pSpeakerToChannelMap[5][2] = pSpeakerToChannelMap[5][3] = pSpeakerToChannelMap[5][4] = pSpeakerToChannelMap[5][5] = 0;

			//7 Channel
			pSpeakerToChannelMap[6][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER|SPEAKER_BACK_LEFT|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_LEFT_OF_CENTER|SPEAKER_SIDE_LEFT;
			pSpeakerToChannelMap[6][1] = SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_BACK_RIGHT|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_RIGHT_OF_CENTER|SPEAKER_SIDE_RIGHT ;
			//pSpeakerToChannelMap[6][2] = pSpeakerToChannelMap[6][3] = pSpeakerToChannelMap[6][4] = pSpeakerToChannelMap[6][5] =  pSpeakerToChannelMap[6][6] = pSpeakerToChannelMap[6][7] =0;

			//8 Channel
			pSpeakerToChannelMap[7][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER|SPEAKER_BACK_LEFT|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_LEFT_OF_CENTER|SPEAKER_SIDE_LEFT;
			pSpeakerToChannelMap[7][1] = SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_BACK_RIGHT|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_RIGHT_OF_CENTER|SPEAKER_SIDE_RIGHT ;
			//pSpeakerToChannelMap[7][2] = pSpeakerToChannelMap[7][3] = pSpeakerToChannelMap[7][4] = pSpeakerToChannelMap[7][5] = pSpeakerToChannelMap[7][6] = pSpeakerToChannelMap[7][7] = 0;
			break;
		case 3:
			pSpeakerToChannelMap[0][0] = pSpeakerToChannelMap[0][1]  = pSpeakerToChannelMap[0][2] = 1;

			pSpeakerToChannelMap[1][0] = SPEAKER_FRONT_LEFT;
			pSpeakerToChannelMap[1][1] = SPEAKER_FRONT_RIGHT;
			pSpeakerToChannelMap[1][2] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT;

			//3 Channel
			pSpeakerToChannelMap[2][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[2][1] = SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[2][2] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT;

			//4 Channel
			pSpeakerToChannelMap[3][0] = SPEAKER_FRONT_LEFT|SPEAKER_BACK_LEFT|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[3][1] =  SPEAKER_FRONT_RIGHT|SPEAKER_BACK_RIGHT|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[3][2] = SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY;
			//pSpeakerToChannelMap[3][3] = 0;

			//5 Channel
			pSpeakerToChannelMap[4][0] = SPEAKER_FRONT_LEFT|SPEAKER_BACK_LEFT|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[4][1] =  SPEAKER_FRONT_RIGHT|SPEAKER_BACK_RIGHT|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[4][2] = SPEAKER_FRONT_CENTER;
			//pSpeakerToChannelMap[4][3] = pSpeakerToChannelMap[4][4]= 0;

			//6 Channel
			pSpeakerToChannelMap[5][0] = SPEAKER_FRONT_LEFT|SPEAKER_BACK_LEFT|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[5][1] = SPEAKER_FRONT_RIGHT|SPEAKER_BACK_RIGHT|SPEAKER_LOW_FREQUENCY ;
			pSpeakerToChannelMap[5][2] = SPEAKER_FRONT_CENTER;
			//pSpeakerToChannelMap[5][3] = pSpeakerToChannelMap[5][4] = pSpeakerToChannelMap[5][5] = 0;

			//7 Channel
			pSpeakerToChannelMap[6][0] = SPEAKER_FRONT_LEFT|SPEAKER_BACK_LEFT|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_LEFT_OF_CENTER|SPEAKER_SIDE_LEFT;
			pSpeakerToChannelMap[6][1] = SPEAKER_FRONT_RIGHT|SPEAKER_BACK_RIGHT|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_RIGHT_OF_CENTER|SPEAKER_SIDE_RIGHT ;
			pSpeakerToChannelMap[6][2] = SPEAKER_FRONT_CENTER|SPEAKER_FRONT_RIGHT_OF_CENTER|SPEAKER_FRONT_LEFT_OF_CENTER|SPEAKER_SIDE_RIGHT|SPEAKER_SIDE_LEFT ;
			//pSpeakerToChannelMap[6][2] = pSpeakerToChannelMap[6][3] = pSpeakerToChannelMap[6][4] = pSpeakerToChannelMap[6][5] =  pSpeakerToChannelMap[6][6] = pSpeakerToChannelMap[6][7] =0;

			//8 Channel
			pSpeakerToChannelMap[7][0] = SPEAKER_FRONT_LEFT|SPEAKER_BACK_LEFT|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_LEFT_OF_CENTER|SPEAKER_SIDE_LEFT;
			pSpeakerToChannelMap[7][1] = SPEAKER_FRONT_RIGHT|SPEAKER_BACK_RIGHT|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_RIGHT_OF_CENTER|SPEAKER_SIDE_RIGHT ;
			pSpeakerToChannelMap[7][2] = SPEAKER_FRONT_CENTER|SPEAKER_FRONT_RIGHT_OF_CENTER|SPEAKER_FRONT_LEFT_OF_CENTER|SPEAKER_SIDE_RIGHT|SPEAKER_SIDE_LEFT ;
			//pSpeakerToChannelMap[7][2] = pSpeakerToChannelMap[7][3] = pSpeakerToChannelMap[7][4] = pSpeakerToChannelMap[7][5] = pSpeakerToChannelMap[7][6] = pSpeakerToChannelMap[7][7] = 0;
			break;
		case 4:
			pSpeakerToChannelMap[0][0] = pSpeakerToChannelMap[0][1]  = pSpeakerToChannelMap[0][2] = pSpeakerToChannelMap[0][3] = 1;

			pSpeakerToChannelMap[1][0] = SPEAKER_FRONT_LEFT;
			pSpeakerToChannelMap[1][1] = SPEAKER_FRONT_RIGHT;
			pSpeakerToChannelMap[1][2] = SPEAKER_FRONT_LEFT;
			pSpeakerToChannelMap[1][3] = SPEAKER_FRONT_RIGHT;

			//3 Channel
			pSpeakerToChannelMap[2][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[2][1] = SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[2][2] = SPEAKER_FRONT_LEFT;
			pSpeakerToChannelMap[2][3] = SPEAKER_FRONT_RIGHT;
			//pSpeakerToChannelMap[2][2] = 0;

			//4 Channel
			pSpeakerToChannelMap[3][0] = SPEAKER_FRONT_LEFT;
			pSpeakerToChannelMap[3][1] =  SPEAKER_FRONT_RIGHT;//SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[3][2] = SPEAKER_FRONT_CENTER|SPEAKER_BACK_LEFT;
			pSpeakerToChannelMap[3][3] = SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_RIGHT;

			pSpeakerToChannelMap[3][4] = SPEAKER_BACK_LEFT;
			pSpeakerToChannelMap[3][5] = SPEAKER_BACK_RIGHT;

			//5 Channel
			pSpeakerToChannelMap[4][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[4][1] =  SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[4][2] = SPEAKER_BACK_LEFT;
			pSpeakerToChannelMap[4][3] = SPEAKER_BACK_RIGHT;
			
			//pSpeakerToChannelMap[4][3] = pSpeakerToChannelMap[4][4]= 0;

			//6 Channel
			pSpeakerToChannelMap[5][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[5][1] = SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY ;
			pSpeakerToChannelMap[5][2] = SPEAKER_BACK_LEFT;
			pSpeakerToChannelMap[5][3] = SPEAKER_BACK_RIGHT;

			//pSpeakerToChannelMap[5][3] = pSpeakerToChannelMap[5][4] = pSpeakerToChannelMap[5][5] = 0;

			//7 Channel
			pSpeakerToChannelMap[6][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_LEFT_OF_CENTER;
			pSpeakerToChannelMap[6][1] = SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_RIGHT_OF_CENTER ;
			pSpeakerToChannelMap[6][2] = SPEAKER_BACK_LEFT|SPEAKER_SIDE_LEFT ;
			pSpeakerToChannelMap[6][3] = SPEAKER_BACK_RIGHT|SPEAKER_SIDE_RIGHT ;
			//pSpeakerToChannelMap[6][2] = pSpeakerToChannelMap[6][3] = pSpeakerToChannelMap[6][4] = pSpeakerToChannelMap[6][5] =  pSpeakerToChannelMap[6][6] = pSpeakerToChannelMap[6][7] =0;

			//8 Channel
			pSpeakerToChannelMap[7][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_LEFT_OF_CENTER;
			pSpeakerToChannelMap[7][1] = SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_RIGHT_OF_CENTER ;
			pSpeakerToChannelMap[7][2] = SPEAKER_BACK_LEFT|SPEAKER_SIDE_LEFT ;
			pSpeakerToChannelMap[7][3] = SPEAKER_BACK_RIGHT|SPEAKER_SIDE_RIGHT ;
			//pSpeakerToChannelMap[7][2] = pSpeakerToChannelMap[7][3] = pSpeakerToChannelMap[7][4] = pSpeakerToChannelMap[7][5] = pSpeakerToChannelMap[7][6] = pSpeakerToChannelMap[7][7] = 0;
			break;
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
			pSpeakerToChannelMap[0][0] = pSpeakerToChannelMap[0][1]  = pSpeakerToChannelMap[0][2] = pSpeakerToChannelMap[0][3] =  pSpeakerToChannelMap[0][4] =  pSpeakerToChannelMap[0][5] =  pSpeakerToChannelMap[0][6] = 1;

			pSpeakerToChannelMap[1][0] = SPEAKER_FRONT_LEFT;
			pSpeakerToChannelMap[1][1] = SPEAKER_FRONT_RIGHT;
			pSpeakerToChannelMap[1][2] = SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[1][3] = SPEAKER_FRONT_LEFT;
			pSpeakerToChannelMap[1][4] = SPEAKER_FRONT_RIGHT;

			//3 Channel
			pSpeakerToChannelMap[2][0] = SPEAKER_FRONT_LEFT;
			pSpeakerToChannelMap[2][1] = SPEAKER_FRONT_RIGHT;
			pSpeakerToChannelMap[2][2] = SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[2][3] = SPEAKER_FRONT_LEFT;
			pSpeakerToChannelMap[2][4] = SPEAKER_FRONT_RIGHT;
			//pSpeakerToChannelMap[2][2] = 0;

			//4 Channel
			pSpeakerToChannelMap[3][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[3][1] =  SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[3][2] = SPEAKER_BACK_LEFT;//;
			pSpeakerToChannelMap[3][3] = SPEAKER_BACK_RIGHT;
			pSpeakerToChannelMap[3][4] = SPEAKER_BACK_LEFT;//;
			pSpeakerToChannelMap[3][5] = SPEAKER_BACK_RIGHT;
			
			//pSpeakerToChannelMap[3][4] = ;

			//5 Channel
			
			pSpeakerToChannelMap[4][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[4][1] =  SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[4][2] = SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[4][3] = SPEAKER_BACK_LEFT;
			pSpeakerToChannelMap[4][4] = SPEAKER_BACK_RIGHT;
			

			//pSpeakerToChannelMap[4][3] = pSpeakerToChannelMap[4][4]= 0;

			//6 Channel
			pSpeakerToChannelMap[5][0] = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[5][1] = SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY ;
			pSpeakerToChannelMap[5][2] = SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[5][3] = SPEAKER_LOW_FREQUENCY;
			pSpeakerToChannelMap[5][4] = SPEAKER_BACK_LEFT;
			pSpeakerToChannelMap[5][5] = SPEAKER_BACK_RIGHT;

			//pSpeakerToChannelMap[5][3] = pSpeakerToChannelMap[5][4] = pSpeakerToChannelMap[5][5] = 0;

			//7 Channel
			pSpeakerToChannelMap[6][0] = SPEAKER_FRONT_LEFT|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_LEFT_OF_CENTER;
			pSpeakerToChannelMap[6][1] = SPEAKER_FRONT_RIGHT|SPEAKER_LOW_FREQUENCY |SPEAKER_BACK_CENTER|SPEAKER_FRONT_RIGHT_OF_CENTER;
			pSpeakerToChannelMap[6][2] = SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[6][3] = SPEAKER_BACK_LEFT|SPEAKER_SIDE_LEFT;
			pSpeakerToChannelMap[6][4] = SPEAKER_BACK_RIGHT|SPEAKER_SIDE_RIGHT;

			//pSpeakerToChannelMap[6][2] = pSpeakerToChannelMap[6][3] = pSpeakerToChannelMap[6][4] = pSpeakerToChannelMap[6][5] =  pSpeakerToChannelMap[6][6] = pSpeakerToChannelMap[6][7] =0;

			//8 Channel
			pSpeakerToChannelMap[7][0] = SPEAKER_FRONT_LEFT|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_FRONT_LEFT_OF_CENTER;
			pSpeakerToChannelMap[7][1] = SPEAKER_FRONT_RIGHT|SPEAKER_LOW_FREQUENCY |SPEAKER_BACK_CENTER|SPEAKER_FRONT_RIGHT_OF_CENTER;
			pSpeakerToChannelMap[7][2] = SPEAKER_FRONT_CENTER;
			pSpeakerToChannelMap[7][3] = SPEAKER_BACK_LEFT|SPEAKER_SIDE_LEFT;
			pSpeakerToChannelMap[7][4] = SPEAKER_BACK_RIGHT|SPEAKER_SIDE_RIGHT;
			//pSpeakerToChannelMap[7][2] = pSpeakerToChannelMap[7][3] = pSpeakerToChannelMap[7][4] = pSpeakerToChannelMap[7][5] = pSpeakerToChannelMap[7][6] = pSpeakerToChannelMap[7][7] = 0;
			break;
		
	}
	//for(int j = 0; j < 18; j++)
	//	for(int i = 0; i < 18; i++)
	//		if(i>j)
	//			pSpeakerToChannelMap[j][i] = 0;

	{
		int iFRS = iSS/10;
		BOOL iLFE = iSS % 10;
		int m_ac3spkcfg;
		int m_dtsspkcfg;
		bool m_aacdownmix;
		m_aacdownmix = FALSE;

		switch(iFRS){
			case 20:
				m_ac3spkcfg = A52_STEREO ;
				m_dtsspkcfg = DTS_STEREO ;
				m_aacdownmix = TRUE;
				break;
			case 21:
				m_ac3spkcfg = A52_2F1R  ;
				m_dtsspkcfg = DTS_2F1R  ;
				m_aacdownmix = TRUE;
				break;
			case 22:
				m_ac3spkcfg = A52_2F2R  ;
				m_dtsspkcfg = DTS_2F2R  ;
				break;
			case 30:
				m_ac3spkcfg = A52_3F  ;
				m_dtsspkcfg = DTS_3F  ;
				m_aacdownmix = TRUE;
				break;
			case 31:
				m_ac3spkcfg = A52_3F1R  ;
				m_dtsspkcfg = DTS_3F1R  ;
				m_aacdownmix = TRUE;
				break;
			case 32:
				m_ac3spkcfg = A52_3F2R  ;
				m_dtsspkcfg = DTS_3F2R  ;
				break;
			default:
				m_ac3spkcfg = A52_STEREO ;
				m_dtsspkcfg = DTS_STEREO ;
				m_aacdownmix = TRUE;
				break;

		}

		if(iLFE){
			m_ac3spkcfg |= A52_LFE;
			m_dtsspkcfg |= DTS_LFE;
		}
		
		CComQIPtr<IMpaDecFilter> m_pMDF;
		if(m_pMDF){
			//m_pMDF->SetSampleFormat((MPCSampleFormat)m_outputformat);
			//m_pMDF->SetSpeakerConfig(IMpaDecFilter::ac3, m_ac3spkcfg);
			//m_pMDF->SetDynamicRangeControl(IMpaDecFilter::ac3, m_ac3drc);
			//m_pMDF->SetSpeakerConfig(IMpaDecFilter::dts, m_dtsspkcfg);
			//m_pMDF->SetDynamicRangeControl(IMpaDecFilter::dts, m_dtsdrc);
			//m_pMDF->SetSpeakerConfig(IMpaDecFilter::aac, m_aacdownmix);
		}

		CRegKey key;
		if(ERROR_SUCCESS == key.Create(HKEY_CURRENT_USER, _T("Software\\SPlayer\\Filters\\MPEG Audio Decoder")))
		{
			//key.SetDWORDValue(_T("Ac3SpeakerConfig"), m_ac3spkcfg);
			//key.SetDWORDValue(_T("DtsSpeakerConfig"), m_dtsspkcfg);
			key.SetDWORDValue(_T("AacSpeakerConfig"), m_aacdownmix);

		}
	}
	
	//fCustomChannelMapping = true;//!IsVista();
	
}*/
#define INITDBARR(d,...) { float x[] = { __VA_ARGS__ };memcpy(d,x,sizeof(x));delete x; }
#define DEBUGVARFCMAP(x) 0
//x
#define LEVEL_PLUS6DB 2.0
#define LEVEL_PLUS3DB 1.4142135623730951
#define LEVEL_3DB 0.7071067811865476
#define LEVEL_45DB 0.5946035575013605
#define LEVEL_6DB 0.5


static inline float EqzConvertdBRevert( float db )
{
	/* Map it to gain,
	* (we do as if the input of iir is /EQZ_IN_FACTOR, but in fact it's the non iir data that is *EQZ_IN_FACTOR)
	* db = 20*log( out / in ) with out = in + amp*iir(i/EQZ_IN_FACTOR)
	* or iir(i) == i for the center freq so
	* db = 20*log( 1 + amp/EQZ_IN_FACTOR )
	* -> amp = EQZ_IN_FACTOR*(10^(db/20) - 1)
	**/

 	if( db < -20.0 )
 		db = -20.0;
 	else if(  db > 20.0 )
 		db = 20.0;
// 	return EQZ_IN_FACTOR * ( pow( 10, db / 20.0 ) - 1.0 );

	return db / 20;
	//EqzConvertdB( pEQBandControlCurrent[i] * 20);
}

#define INITEQPERSET(d,n,x1,x2,...) { eq_perset_setting t = { n ,x1,x2, { __VA_ARGS__ } }; d = t; }
void CMPlayerCApp::Settings::InitEQPerset(){

	//init eq perset

	INITEQPERSET(eqPerset[1] , ResStr(IDS_EQ_PERSET_NAME_CLASSICAL), 10, 12.0,
		-1.11022e-15f, -1.11022e-15f, -1.11022e-15f, -1.11022e-15f, -1.11022e-15f, -1.11022e-15f, -7.2f, -7.2f, -7.2f, -9.6f  );

	INITEQPERSET(eqPerset[2] , ResStr(IDS_EQ_PERSET_NAME_CLUB), 10, 6.0,
		-1.11022e-15f, -1.11022e-15f, 8.0f, 5.6f, 5.6f, 5.6f, 3.2f, -1.11022e-15f, -1.11022e-15f, -1.11022e-15f  );

	INITEQPERSET(eqPerset[3] , ResStr(IDS_EQ_PERSET_NAME_DANCE), 10, 5.0,
		9.6f, 7.2f, 2.4f, -1.11022e-15f, -1.11022e-15f, -5.6f, -7.2f, -7.2f, -1.11022e-15f, -1.11022e-15f  );

	INITEQPERSET(eqPerset[4] , ResStr(IDS_EQ_PERSET_NAME_FULLBASS), 10, 5.0,
		-8.0f, 9.6f, 9.6f, 5.6f, 1.6f, -4.0f, -8.0f, -10.4f, -11.2f, -11.2f   );

	INITEQPERSET(eqPerset[5] , ResStr(IDS_EQ_PERSET_NAME_FULLBASSTREBLE), 10, 4.0,
		7.2f, 5.6f, -1.11022e-15f, -7.2f, -4.8f, 1.6f, 8.0f, 11.2f, 12.0f, 12.0f  );

	INITEQPERSET(eqPerset[6] , ResStr(IDS_EQ_PERSET_NAME_FULLTREBLE), 10, 3.0,
		-9.6f, -9.6f, -9.6f, -4.0f, 2.4f, 11.2f, 16.0f, 16.0f, 16.0f, 16.8f  );

	INITEQPERSET(eqPerset[7] , ResStr(IDS_EQ_PERSET_NAME_HEADPHONES), 10, 4.0,
		4.8f, 11.2f, 5.6f, -3.2f, -2.4f, 1.6f, 4.8f, 9.6f, 12.8f, 14.4f  );

	INITEQPERSET(eqPerset[8] , ResStr(IDS_EQ_PERSET_NAME_LARGEHALL), 10, 5.0,
		10.4f, 10.4f, 5.6f, 5.6f, -1.11022e-15f, -4.8f, -4.8f, -4.8f, -1.11022e-15f, -1.11022e-15f  );

	INITEQPERSET(eqPerset[9] , ResStr(IDS_EQ_PERSET_NAME_LIVE), 10, 7.0,
		-4.8f, -1.11022e-15f, 4.0f, 5.6f, 5.6f, 5.6f, 4.0f, 2.4f, 2.4f, 2.4f  );

	INITEQPERSET(eqPerset[10] , ResStr(IDS_EQ_PERSET_NAME_PARTY), 10, 6.0,
		7.2f, 7.2f, -1.11022e-15f, -1.11022e-15f, -1.11022e-15f, -1.11022e-15f, -1.11022e-15f, -1.11022e-15f, 7.2f, 7.2f  );

	INITEQPERSET(eqPerset[11] , ResStr(IDS_EQ_PERSET_NAME_POP), 10, 6.0,
		-1.6f, 4.8f, 7.2f, 8.0f, 5.6f, -1.11022e-15f, -2.4f, -2.4f, -1.6f, -1.6f  );

	INITEQPERSET(eqPerset[12] , ResStr(IDS_EQ_PERSET_NAME_REGGAE), 10, 8.0,
		-1.11022e-15f, -1.11022e-15f, -1.11022e-15f, -5.6f, -1.11022e-15f, 6.4f, 6.4f, -1.11022e-15f, -1.11022e-15f, -1.11022e-15f  );

	INITEQPERSET(eqPerset[13] , ResStr(IDS_EQ_PERSET_NAME_ROCK), 10, 5.0,
		8.0f, 4.8f, -5.6f, -8.0f, -3.2f, 4.0f, 8.8f, 11.2f, 11.2f, 11.2f  );

	INITEQPERSET(eqPerset[14] , ResStr(IDS_EQ_PERSET_NAME_SKA), 10, 6.0,
		-2.4f, -4.8f, -4.0f, -1.11022e-15f, 4.0f, 5.6f, 8.8f, 9.6f, 11.2f, 9.6f  );

	INITEQPERSET(eqPerset[15] , ResStr(IDS_EQ_PERSET_NAME_SOFT), 10, 5.0,
		4.8f, 1.6f, -1.11022e-15f, -2.4f, -1.11022e-15f, 4.0f, 8.0f, 9.6f, 11.2f, 12.0f  );

	INITEQPERSET(eqPerset[16] , ResStr(IDS_EQ_PERSET_NAME_SOFTROCK), 10, 7.0,
		4.0f, 4.0f, 2.4f, -1.11022e-15f, -4.0f, -5.6f, -3.2f, -1.11022e-15f, 2.4f, 8.8f  );

	INITEQPERSET(eqPerset[17] , ResStr(IDS_EQ_PERSET_NAME_TECHNO), 10, 5.0,
		8.0f, 5.6f, -1.11022e-15f, -5.6f, -4.8f, -1.11022e-15f, 8.0f, 9.6f, 9.6f, 8.8f  );

	POSITION pos = eqPerset.GetStartPosition();
	while(pos)
	{
		eq_perset_setting cVal;
		DWORD cKey;
		eqPerset.GetNextAssoc(pos, cKey, cVal);

		for(int i = 0; i < MAX_EQ_BAND; i++){
			eqPerset[cKey].f_amp[i] = EqzConvertdBRevert(cVal.f_preamp-12+cVal.f_amp[i]);
		}


	}

}
void CMPlayerCApp::Settings::InitChannelMap()
{
	memset(pSpeakerToChannelMap2, 0 , sizeof(pSpeakerToChannelMap2));
	///大于等于6声道时 第3、4声源声道分别是中置和重低音 
	//下面的算法把最后一个声源声道当作重低音了
	//CString szOut;
	for(int iInputChannelCount = 1; iInputChannelCount <= MAX_INPUT_CHANNELS; iInputChannelCount++){
		for(int iOutputChannelCount = 1; iOutputChannelCount <= MAX_OUTPUT_CHANNELS; iOutputChannelCount++){
			
			if(iOutputChannelCount >= iInputChannelCount )
				break;

			//szOut.AppendFormat(_T("[%d] -> [%d] \r\n") , iInputChannelCount , iOutputChannelCount);
			
			for(int iSpeakerID = 0; iSpeakerID < iOutputChannelCount; iSpeakerID++){
				//szOut.AppendFormat(_T("[%d] -> [%d] @ %d : { ") , iInputChannelCount , iOutputChannelCount , iSpeakerID);
				CString szFloatList;
				for(int iChannelID = 0; iChannelID < iInputChannelCount; iChannelID++){
					float fTmpVal = 1.0;
					if( iInputChannelCount > 2 && iChannelID < 2){	 // 前置左右声道
						fTmpVal = 1.0;
					}else if( iInputChannelCount > 4 && iChannelID == 2){ //中置声道
						fTmpVal = 2.4;
					}else if( iInputChannelCount > 5 && iChannelID == (iInputChannelCount - 1) ){ //重低音 降低
						fTmpVal = 0.9f;
					}else if(iInputChannelCount > 2 && iChannelID >= 2){ //除中置 重低音外的声道
						fTmpVal = 0.9f;
					}
					if(iOutputChannelCount == 1){  //输出至单声道
						fTmpVal = fTmpVal;
					}else{  //输出双单声道
						// iOutputChannelCount >= 2
						int iRearOutputCount2 = iOutputChannelCount;
						if(iOutputChannelCount > 5){
							iRearOutputCount2--;
						}
						if(iRearOutputCount2 > 3 && (iRearOutputCount2%2)==0 ){
							iRearOutputCount2--;
						}

						if( iInputChannelCount > 4 && iChannelID == 2){ //中置声道
							
							if( iOutputChannelCount <= 4  ){
								if(iSpeakerID >= 2)
									fTmpVal = DEBUGVARFCMAP(0.00118);

							}else if(iOutputChannelCount > 4 ){
								if(iSpeakerID != 2)
									fTmpVal = DEBUGVARFCMAP(0.00116);
							}
						}else if( iChannelID < iRearOutputCount2 || (iInputChannelCount > 4 &&  iChannelID < iRearOutputCount2) ) { //输出声道足够时仅 1 1 映射
							if( iInputChannelCount > 4 && iOutputChannelCount <= 4 && iSpeakerID >= 2){ //有中置
								if(iChannelID != (iSpeakerID+1)){
									fTmpVal = DEBUGVARFCMAP(0.00115);
								}
							}else if(iChannelID != iSpeakerID){
								fTmpVal = DEBUGVARFCMAP(0.00112);
							}
						}else {//音源声道多于输出声道 
							if(iOutputChannelCount > 5 && iSpeakerID < (iOutputChannelCount-1) && 
								iInputChannelCount > 5 && iChannelID == (iInputChannelCount - 1)){ //输出有重低音且输入有重低音时
								fTmpVal = DEBUGVARFCMAP(0.0004); //不映射重低音至非重低音音箱
							}else if( iInputChannelCount > 4 && iChannelID == 2){ //有中置声道音源
								if( iOutputChannelCount == 4 ){ // 没有中置音箱时不映射到后置音箱
									if(iSpeakerID >= 2)
										fTmpVal = DEBUGVARFCMAP(0.0002);
								}else if( iOutputChannelCount > 4 && iSpeakerID >= 3 ){ // 有中置音箱时不映射到后置音箱
									fTmpVal = DEBUGVARFCMAP(0.0001);
								}
								
							}else if( iInputChannelCount > 5 && iChannelID == (iInputChannelCount-1) ){  //有重低音音源
								if(iOutputChannelCount > 5 && iSpeakerID == (iOutputChannelCount-1)){ //以1:1能量输出至重低音音箱
									fTmpVal = 1.0;
								}
							}else{
								int iRearInputCount = iInputChannelCount; //这段是取最后2个非中置非重低音音箱和声源声道ID的部分
								int iRearOutputCount = iOutputChannelCount;
								if(iInputChannelCount >= 5){
									iRearInputCount--;
								}
								if(iInputChannelCount > 5){
									iRearInputCount--;
								}
								if(iOutputChannelCount >= 5){
									iRearOutputCount--;
								}
								if(iOutputChannelCount > 5){
									iRearOutputCount--;
								}
								int iRearInputEnd = iInputChannelCount - 1;
								if(iInputChannelCount > 5){
									iRearInputEnd--;
								}
								if(iInputChannelCount >= 5){
									if(iRearInputEnd == 2){
										iRearInputEnd--;
									}
								}
								int iRearInputEnd2 = iRearInputEnd-1;
								if(iInputChannelCount >= 5){
									if(iRearInputEnd2 == 2){
										iRearInputEnd2--;
									}
								}

								int iRearOuputEnd = iOutputChannelCount - 1;
								if(iOutputChannelCount > 5){
									iRearOuputEnd--;
								}
								if(iOutputChannelCount >= 5){
									if(iRearOuputEnd == 2){
										iRearOuputEnd--;
									}
								}
								int iRearOuputEnd2 = iRearOuputEnd - 1;
								if(iOutputChannelCount >= 5){
									if(iRearOuputEnd2 == 2){
										iRearOuputEnd2--;
									}
								}

								BOOL bPass = false;
								
								if( (iRearOutputCount%2) && (iRearInputCount%2) == 0 ){ //如果后置音箱有单数
									//将最后2个音源映射到 最后一个音箱, 而且不映射到别的地方
									
									if(  iChannelID == iRearInputEnd || iChannelID == iRearInputEnd2) {
										bPass = true;
										if(iSpeakerID == iRearOuputEnd){
											//pass
											//fTmpVal = 0.576;
										}else{
											fTmpVal = DEBUGVARFCMAP(0.00224);
										}
										
									}
									
								}else if( (iRearOutputCount%2) == 0 && (iRearInputCount%2)  ){//如果后置音源有单数，
									//将最后1个音源映射到 最后2个音箱, 而且不映射到别的地方
									
									if( iChannelID == iRearInputEnd  ){
										bPass = true;
										if(( iSpeakerID == iRearOuputEnd || iSpeakerID == iRearOuputEnd2)){
											//pass
											//fTmpVal = 0.576;
										}else{
											fTmpVal = DEBUGVARFCMAP(0.00223);
										}
									}
									
								}
								
								if(!bPass){
									if(iInputChannelCount >= 5){ //音源有中置音源
										if(iOutputChannelCount >= 5){//音箱有中置音箱
												int iWhereitShoulbe = iChannelID;
												while(iWhereitShoulbe > iRearOuputEnd){
													iWhereitShoulbe-=2;
												}
											
												if(iWhereitShoulbe != (iSpeakerID)){  //channel 3 => 0
													fTmpVal = DEBUGVARFCMAP(0.0011);
												}
											
										}else{//音箱没有中置音箱输出
											int iWhereitShoulbe = -1;
											if(iOutputChannelCount == 3){ //单数
												iWhereitShoulbe = 2;
											}else if(iOutputChannelCount == 4){
												if(iChannelID > 2){
													iWhereitShoulbe = (iChannelID-3)%2 + 2;
												}else{
													iWhereitShoulbe = iChannelID;
												}

											}else if(iOutputChannelCount == 2){
												if(iChannelID > 2){
													iWhereitShoulbe = (iChannelID-3)%2 ;
												}else{
													iWhereitShoulbe = iChannelID;
												}
											}
											

											if( iWhereitShoulbe != iSpeakerID){  //channel 3 => 0
												fTmpVal = DEBUGVARFCMAP(0.0023);
											}
										}
									}else{
										if( (iChannelID%iOutputChannelCount) != iSpeakerID){  // channel 2 => 0
											fTmpVal = DEBUGVARFCMAP(0.0022);
										}
									}
								}

							}
							
						}

						//default
					}
					
					pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iChannelID] = fTmpVal;
				}
				//for(int iChannelID = 0; iChannelID < iInputChannelCount; iChannelID++){
				//	szFloatList.AppendFormat(_T(" %f ,"), pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iChannelID]);
				//}
				//szOut += szFloatList.TrimRight(_T(",")) + _T(" } \r\n");
			}
			//szOut += _T("\r\n");
		}
	}

	for(int iInputChannelCount = 5; iInputChannelCount <= MAX_INPUT_CHANNELS; iInputChannelCount++){
		for(int iOutputChannelCount = 3; iOutputChannelCount <= MAX_OUTPUT_CHANNELS; iOutputChannelCount++){
			if(iInputChannelCount <= iOutputChannelCount){
				for(int iSpeakerID = 0; iSpeakerID < iInputChannelCount; iSpeakerID++){
					pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iSpeakerID] = 1.0;
				}
			}
			
			if(pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][0][2] <= 0)
				pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][0][2] = 1.0;

			if(pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][1][2] <= 0)
				pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][1][2] = 1.0;

		}
	}
	float tmp;
	for(int iInputChannelCount = 5; iInputChannelCount <= MAX_INPUT_CHANNELS; iInputChannelCount++){
		for(int iOutputChannelCount = 2; iOutputChannelCount <= MAX_OUTPUT_CHANNELS; iOutputChannelCount++){

			for(int iSpeakerID = 0; iSpeakerID < iOutputChannelCount; iSpeakerID++){
				int iLEFChann = max(iInputChannelCount-1, 5);
				tmp = pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iLEFChann] ;

				for(int iChannelID = iLEFChann; iChannelID > 3; iChannelID--){
					pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iChannelID]
					= pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iChannelID-1];
				}
				pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][3] = tmp;
			}

			if(iOutputChannelCount > 4){
				for(int iChannelID = 0; iChannelID < iInputChannelCount; iChannelID++){
					int iLEFChann = max(iOutputChannelCount-1, 5);
					tmp = pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][iLEFChann][iChannelID] ;
					for(int iSpeakerID = iLEFChann; iSpeakerID > 3; iSpeakerID--){
						pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iChannelID]
						= pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID-1][iChannelID];
					}
					pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][3][iChannelID] = tmp;
				}
			}

		}
	}
	
	//CSVPToolBox svpTool;
	//svpTool.filePutContent( svpTool.GetPlayerPath(_T("ChannelSetting.txt")),  szOut);
		
	//5.1 => 2.0
	//INITDBARR( pSpeakerToChannelMap2[5][1][0] , LEVEL_3DB,0,LEVEL_PLUS3DB,LEVEL_45DB,0,LEVEL_6DB );
	//INITDBARR( pSpeakerToChannelMap2[5][1][1] , 0,LEVEL_3DB,LEVEL_PLUS3DB,0,LEVEL_45DB,LEVEL_6DB );
}
void CMPlayerCApp::Settings::ChangeChannelMapByCustomSetting()
{
	for(int iInputChannelCount = 1; iInputChannelCount <= MAX_INPUT_CHANNELS; iInputChannelCount++){
		for(int iOutputChannelCount = 1; iOutputChannelCount <= MAX_OUTPUT_CHANNELS; iOutputChannelCount++){
			bool bHasCustomSetting = false;
			for(int iSpeakerID = 0; iSpeakerID < iOutputChannelCount; iSpeakerID++){
				for(int iChannelID = 0; iChannelID < iInputChannelCount; iChannelID++){
					if(pSpeakerToChannelMap2Custom[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iChannelID] != 0){
						bHasCustomSetting = true;
						break;
					}
				}
				if(bHasCustomSetting)
					break;
			}
			if(bHasCustomSetting){
				for(int iSpeakerID = 0; iSpeakerID < iOutputChannelCount; iSpeakerID++){
					for(int iChannelID = 0; iChannelID < iInputChannelCount; iChannelID++){
						pSpeakerToChannelMap2[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iChannelID] =
							pSpeakerToChannelMap2Custom[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iChannelID] ;
							
					}
				}
			}
		}
	}
	//此处修正之前我的犯下的错误，大于等于第4声源的顺移，并将最后一个声源与第4声源的数据交换 （重低音）
	
	return;	
}
CString CMPlayerCApp::Settings::GetSVPSubStorePath(){
	CString StoreDir = SVPSubStoreDir;
	CSVPToolBox svpTool;
	if(StoreDir.IsEmpty() || !svpTool.ifDirExist(StoreDir) || !svpTool.ifDirWritable(StoreDir)){
		svpTool.GetAppDataPath(StoreDir);
		CPath tmPath(StoreDir);
		tmPath.RemoveBackslash();
		tmPath.AddBackslash();
		tmPath.Append( _T("SVPSub"));
		StoreDir = (CString)tmPath;
		_wmkdir(StoreDir);
		if(StoreDir.IsEmpty() || !svpTool.ifDirExist(StoreDir) || !svpTool.ifDirWritable(StoreDir)){
			StoreDir =  svpTool.GetPlayerPath(_T("SVPSub"));
			_wmkdir(StoreDir);
			if(StoreDir.IsEmpty() || !svpTool.ifDirExist(StoreDir) || !svpTool.ifDirWritable(StoreDir)){

				//WTF cant create fordler ?
				StoreDir = _T("%temp%");
				
			}else{
				SVPSubStoreDir = StoreDir;
			}
		}else{
			SVPSubStoreDir = StoreDir;
		}
	}

	return StoreDir;
}
BOOL CMPlayerCApp::Settings::bShouldUseGPUAcel(){
	return  useGPUAcel && !bNoMoreDXVAForThisFile ;
}
BOOL CMPlayerCApp::Settings::bIsChineseUIUser()
{
    return  (iLanguage == 0 || iLanguage == 2);
}
BOOL CMPlayerCApp::Settings::bUserAeroUI(){
	return  (bAeroGlassAvalibility && bAeroGlass) || bTransControl;
}
BOOL CMPlayerCApp::Settings::bShouldUseEVR(){
	//return 1;
	//Vista下使用EVR
	//XP下 不用GPU加速时使用EVR
	//XP下 用CoreAVC+CUDA时使用EVR

	return ( ( IsVista() )  && !bDisableEVR ) || ( !IsVista() && bDisableEVR);//|| ( !IsVista() && fVMRGothSyncFix &&  ( !useGPUAcel || useGPUCUDA ) && HasEVRSupport()) 
}
BOOL CMPlayerCApp::Settings::bUserAeroTitle(){
	return  bAeroGlassAvalibility && bAeroGlass;
}
void CMPlayerCApp::Settings::UpdateData(bool fSave)
{
	CMPlayerCApp* pApp = AfxGetMyApp();
	ASSERT(pApp);

	UINT len;
	BYTE* ptr = NULL;

	if(!fSave && wmcmds.IsEmpty()){
#define ADDCMD(cmd) wmcmds.AddTail(wmcmd##cmd)
		ADDCMD((ID_BOSS, VK_OEM_3, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_BOSS_KEY)));
		ADDCMD((ID_PLAY_PLAYPAUSE, VK_SPACE, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_PLAY_PAUSE), APPCOMMAND_MEDIA_PLAY_PAUSE, wmcmd::LDOWN));
		ADDCMD((ID_PLAY_SEEKFORWARDMED, VK_RIGHT, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_SEEKFORWARDMED)));//
		ADDCMD((ID_PLAY_SEEKBACKWARDMED, VK_LEFT, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_SEEKBACKFORD)));//

		ADDCMD((ID_FILE_OPENQUICK, 'Q', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_FILEOPEN_QUICK)));
		ADDCMD((ID_FILE_OPENURLSTREAM, 'U', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_FILEOPEN_URL)));
		ADDCMD((ID_FILE_OPENMEDIA, 'O', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_FILEOPEN_NORMAL)));
		ADDCMD((ID_FILE_OPENFOLDER, 'F', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_FILEOPEN_FOLDER)));

		ADDCMD((ID_SUBMOVEUP,  VK_OEM_4 /* [ */, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_MOVE_UP)));
		ADDCMD((ID_SUBMOVEDOWN,  VK_OEM_6  /* ] */, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_MOVE_DOWN)));
		ADDCMD((ID_SUB2MOVEUP,  VK_OEM_4, FVIRTKEY|FALT|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_2ND_MOVE_UP)));
		ADDCMD((ID_SUB2MOVEDOWN,  VK_OEM_6, FVIRTKEY|FALT|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_2ND_MOVE_DOWN)));
		ADDCMD((ID_SUBFONTDOWNBOTH,  VK_F1, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_FONT_SHRINK)));
		ADDCMD((ID_SUBFONTUPBOTH,  VK_F2, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_FONT_ENLARGE)));
		ADDCMD((ID_SUB1FONTDOWN,  VK_F3, FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_1ST_FONT_SHRINK)));
		ADDCMD((ID_SUB1FONTUP,  VK_F4, FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_1ST_FONT_ENLARGE)));
		ADDCMD((ID_SUB2FONTDOWN,  VK_F5, FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_2ND_FONT_SHRINK)));
		ADDCMD((ID_SUB2FONTUP,  VK_F6, FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_2ND_FONT_ENLARGE)));

		ADDCMD((ID_SUB_DELAY_DOWN, VK_F1, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_1ST_DELAY_REDUCE)));
		ADDCMD((ID_SUB_DELAY_UP, VK_F2,   FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_1ST_DELAY_PLUS)));
		ADDCMD((ID_SUB_DELAY_DOWN2, VK_F1, FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_2ND_DELAY_REDUCE)));
		ADDCMD((ID_SUB_DELAY_UP2, VK_F2,   FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_2ND_DELAY_PLUS)));

        ADDCMD((ID_SHOW_VIDEO_STAT_OSD,  VK_TAB, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_SHOW_VIDEO_STAT_OSD)));

		ADDCMD((ID_BRIGHTINC, VK_HOME, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_BRIGHT_MORE)));
		ADDCMD((ID_BRIGHTDEC, VK_END, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_BRIGHT_LESS)));

		ADDCMD((ID_ABCONTROL_TOGGLE,  VK_F7, FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_ABCONTROL_TOGGEL)));
		ADDCMD((ID_ABCONTROL_SETA,  VK_F8, FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_ABCONTROL_SETA)));
		ADDCMD((ID_ABCONTROL_SETB,  VK_F9, FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_ABCONTROL_SETB)));
        
		ADDCMD((ID_FILE_OPENDVD, 'D', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_FILEOPEN_DVD)));
		ADDCMD((ID_FILE_OPENBDVD, 'B', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_FILEOPEN_BD)));
		ADDCMD((ID_FILE_OPENDEVICE, 'V', FVIRTKEY|FCONTROL|FNOINVERT, _T("Open Device")));
		ADDCMD((ID_FILE_SAVE_COPY, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_FILE_SAVEAS)));
		ADDCMD((ID_FILE_SAVE_IMAGE, 'I', FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_IMAGE_SAVEAS)));
		ADDCMD((ID_FILE_SAVE_IMAGE_AUTO, VK_F5, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_IMAGE_SAVE_AUTO)));
		ADDCMD((ID_FILE_LOAD_SUBTITLE, 'L', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_LOAD_SUBTITLE)));
		ADDCMD((ID_FILE_SAVE_SUBTITLE, 'S', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_SAVE_SUBTITLE)));
		//ADDCMD((ID_FILE_CLOSEPLAYLIST, 'C', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_COPY_IMAGE_TO_CLIPBOARD)));
		ADDCMD((ID_FILE_COPYTOCLIPBOARD, 'C', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_COPY_IMAGE_TO_CLIPBOARD)));
		ADDCMD((ID_FILE_PROPERTIES, VK_F10, FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_FILE_PROPERTIES)));
		ADDCMD((ID_FILE_EXIT, 'X', FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_FILE_EXIT)));
		ADDCMD((ID_TOGGLE_SUBTITLE, 'H', FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_TOGGLE)));
		ADDCMD((ID_VIEW_VF_FROMINSIDE, 'C', FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_RESET)));
		ADDCMD((ID_PLAY_PLAY, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_PLAY_PLAY),APPCOMMAND_MEDIA_PLAY));
		ADDCMD((ID_PLAY_PAUSE, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_PLAY_PAUSE),APPCOMMAND_MEDIA_PAUSE));
		ADDCMD((ID_PLAY_MANUAL_STOP, VK_OEM_PERIOD, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_PLAY_STOP), APPCOMMAND_MEDIA_STOP));
		ADDCMD((ID_PLAY_FRAMESTEP, VK_RIGHT, FVIRTKEY|FCONTROL|FALT|FNOINVERT, ResStr(IDS_HOTKEY_PLAY_FRAMESTEP)));
		ADDCMD((ID_PLAY_FRAMESTEPCANCEL, VK_LEFT, FVIRTKEY|FCONTROL|FALT|FNOINVERT, ResStr(IDS_HOTKEY_PLAY_FRAMESTEP_BACK)));
		ADDCMD((ID_PLAY_INCRATE, VK_UP, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PLAY_INCREASE_RATE)));
		ADDCMD((ID_PLAY_DECRATE, VK_DOWN, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PLAY_DECREASE_RATE)));
		ADDCMD((ID_VIEW_FULLSCREEN, VK_RETURN, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_FULLSCREEN), 0, wmcmd::LDBLCLK));
		ADDCMD((ID_VIEW_FULLSCREEN, VK_RETURN, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_FULLSCREEN), 0, wmcmd::MUP));
		ADDCMD((ID_PLAY_INCAUDDELAY, VK_ADD, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_AUDIO_DELAY_PLUS)));
		ADDCMD((ID_PLAY_DECAUDDELAY, VK_SUBTRACT, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_AUDIO_DELAY_REDUCE)));
		//ADDCMD((ID_VIEW_FULLSCREEN_SECONDARY, VK_ESCAPE, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_FULLSCREEN_SECONDARY)));
		ADDCMD((ID_VIEW_PLAYLIST, 'P', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_PLAYLIST)));
		ADDCMD((ID_VIEW_PLAYLIST, '7', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_PLAYLIST)));
		ADDCMD((ID_PLAY_RESETRATE, 'R', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PLAY_RESET_PLAYRATE)));
		ADDCMD((ID_PLAY_SEEKFORWARDSMALL, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_SEEK_FORWARD_SMALL),APPCOMMAND_MEDIA_FAST_FORWARD));
		ADDCMD((ID_PLAY_SEEKBACKWARDSMALL, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_SEEK_BACKWARD_SMALL),APPCOMMAND_MEDIA_REWIND));
		ADDCMD((ID_PLAY_SEEKFORWARDMED, VK_RIGHT, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_SEEK_FORWARD_MED)));
		ADDCMD((ID_PLAY_SEEKBACKWARDMED, VK_LEFT, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_SEEK_BACKWARD_MED)));
		ADDCMD((ID_PLAY_SEEKFORWARDLARGE, VK_RIGHT, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_SEEK_FORWARD_BIG)));
		ADDCMD((ID_PLAY_SEEKBACKWARDLARGE, VK_LEFT, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_SEEK_BACKWARD_BIG)));
		ADDCMD((ID_PLAY_SEEKKEYFORWARD, VK_RIGHT, FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_SEEK_FORWARD_KEYFRAME)));
		ADDCMD((ID_PLAY_SEEKKEYBACKWARD, VK_LEFT, FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_SEEK_BACKWARD_KEYFRAME)));
		ADDCMD((ID_NAVIGATE_SKIPFORWARD, VK_NEXT, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_NAV_SKIP_FORWARD), APPCOMMAND_MEDIA_NEXTTRACK, wmcmd::X2DOWN));
		ADDCMD((ID_NAVIGATE_SKIPBACK, VK_PRIOR, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_NAV_SKIP_BACKWARD), APPCOMMAND_MEDIA_PREVIOUSTRACK, wmcmd::X1DOWN));
		ADDCMD((ID_NAVIGATE_SKIPFORWARDPLITEM, VK_NEXT, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_NAV_SKIP_FORWARD_PLITEM)));
		ADDCMD((ID_NAVIGATE_SKIPBACKPLITEM, VK_PRIOR, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_NAV_SKIP_BACKWARD_PLITEM)));
		ADDCMD((ID_VIEW_CAPTIONMENU, '0', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_CAPTION_MENU)));
		ADDCMD((ID_VIEW_SEEKER, '1', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_SEEK_BAR)));
		ADDCMD((ID_VIEW_CONTROLS, '2', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_TOOL_BAR)));
		ADDCMD((ID_VIEW_INFORMATION, '3', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_PLAY_INFORMATION)));
		ADDCMD((ID_VIEW_STATISTICS, '4', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_PLAY_STATUS)));
		//ADDCMD((ID_VIEW_STATUS, '5', FVIRTKEY|FCONTROL|FNOINVERT, _T("Toggle Status")));
		ADDCMD((ID_SHOWTRANSPRANTBAR, '5', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_TRANSPARENT_CONTROL_DIALOG)));
		ADDCMD((ID_VIEW_SUBRESYNC, '6', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_SUBRESYNC)));
		ADDCMD((ID_VIEW_CAPTURE, '8', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_CAPTURE_PANEL)));
		ADDCMD((ID_VIEW_SHADEREDITOR, '9', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_SHADER_EDITOR_PANEL)));
		ADDCMD((ID_VIEW_PRESETS_MINIMAL, '1', FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_UI_PRESET_MINIMAL)));
		ADDCMD((ID_VIEW_PRESETS_COMPACT, '2', FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_UI_PRESET_COMPACT)));
		ADDCMD((ID_VIEW_PRESETS_NORMAL, '3', FVIRTKEY|FNOINVERT, _T("View Normal")));
		ADDCMD((ID_VIEW_FULLSCREEN_SECONDARY, VK_F11, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_FULLSCREEN_SECONDARY)));
		ADDCMD((ID_VIEW_ZOOM_50, '1', FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_ZOOM_50)));
		ADDCMD((ID_VIEW_ZOOM_100, '2', FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_ZOOM_100)));
		ADDCMD((ID_VIEW_ZOOM_200, '3', FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_ZOOM_200)));
		ADDCMD((ID_VIEW_ZOOM_AUTOFIT, '4', FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_ZOOM_AUTOFIT)));	
		ADDCMD((ID_ASPECTRATIO_NEXT, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_ASPECT_RATIO_NEXT)));
		ADDCMD((ID_VIEW_VF_HALF, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_VIDEO_FRAME_50)));
		ADDCMD((ID_VIEW_VF_NORMAL, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_VIDEO_FRAME_100)));
		ADDCMD((ID_VIEW_VF_DOUBLE, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_VIDEO_FRAME_200)));
		ADDCMD((ID_VIEW_VF_STRETCH, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_VIDEO_FRAME_STRETCH)));
		ADDCMD((ID_VIEW_VF_FROMINSIDE, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_VIDEO_FRAME_REMOVE_BLACK_BAR)));
		ADDCMD((ID_VIEW_VF_FROMOUTSIDE, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_VIDEO_FRAME_STANDARD)));
		ADDCMD((ID_SWITCH_AUDIO_DEVICE, 'A', FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_SWITCH_AUDIO_DEVICE)));
		ADDCMD((ID_ONTOP_ALWAYS, 'T', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PIN_ONTOP_ALWAYS)));
		ADDCMD((ID_PLAY_GOTO, 'G', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PLAY_GOTO)));
		
		ADDCMD((ID_VIEW_RESET, VK_NUMPAD5, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_RESET)));
		ADDCMD((ID_VIEW_INCSIZE, VK_NUMPAD9, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_SIZE_PLUS)));
		ADDCMD((ID_VIEW_INCWIDTH, VK_NUMPAD6, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_WIDTH_PLUS)));
		ADDCMD((ID_VIEW_INCHEIGHT, VK_NUMPAD8, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_HEIGHT_PLUS)));
		ADDCMD((ID_VIEW_DECSIZE, VK_NUMPAD1, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_SIZE_REDUCE)));
		ADDCMD((ID_VIEW_DECWIDTH, VK_NUMPAD4, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_WIDTH_REDUCE)));
		ADDCMD((ID_VIEW_DECHEIGHT, VK_NUMPAD2, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_HEIGHT_REDUCE)));
		ADDCMD((ID_PANSCAN_CENTER, VK_NUMPAD5, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_CENTER)));
		ADDCMD((ID_PANSCAN_MOVELEFT, VK_NUMPAD4, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_MOVE_LEFT)));
		ADDCMD((ID_PANSCAN_MOVERIGHT, VK_NUMPAD6, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_MOVE_RIGHT)));
		ADDCMD((ID_PANSCAN_MOVEUP, VK_NUMPAD8, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_MOVE_UP)));
		ADDCMD((ID_PANSCAN_MOVEDOWN, VK_NUMPAD2, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_MOVE_DOWN)));
		ADDCMD((ID_PANSCAN_MOVEUPLEFT, VK_NUMPAD7, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_MOVE_UPLEFT)));
		ADDCMD((ID_PANSCAN_MOVEUPRIGHT, VK_NUMPAD9, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_MOVE_UPRIGHT)));
		ADDCMD((ID_PANSCAN_MOVEDOWNLEFT, VK_NUMPAD1, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_MOVE_BOTTOMLEFT)));
		ADDCMD((ID_PANSCAN_MOVEDOWNRIGHT, VK_NUMPAD3, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_MOVE_BOTTOMRIGHT)));
		ADDCMD((ID_PANSCAN_ROTATEXP, VK_NUMPAD8, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_ROTATE_X_PLUS)));
		ADDCMD((ID_PANSCAN_ROTATEXM, VK_NUMPAD2, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_ROTATE_X_REDUCE)));
		ADDCMD((ID_PANSCAN_ROTATEYP, VK_NUMPAD4, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_ROTATE_Y_PLUS)));
		ADDCMD((ID_PANSCAN_ROTATEYM, VK_NUMPAD6, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_ROTATE_Y_REDUCE)));
		ADDCMD((ID_PANSCAN_ROTATEZP, VK_NUMPAD1, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_ROTATE_Z_PLUS)));
		ADDCMD((ID_PANSCAN_ROTATEZM, VK_NUMPAD3, FVIRTKEY|FALT|FNOINVERT, ResStr(IDS_HOTKEY_PANSCAN_ROTATE_Z_REDUCE)));
		ADDCMD((ID_VOLUME_UP, VK_UP, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_VOLUME_UP), APPCOMMAND_VOLUME_UP, wmcmd::WUP));//APPCOMMAND_VOLUME_UP
		ADDCMD((ID_VOLUME_DOWN, VK_DOWN, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_VOLUME_DOWN), APPCOMMAND_VOLUME_DOWN, wmcmd::WDOWN));//APPCOMMAND_VOLUME_DOWN
		ADDCMD((ID_VOLUME_MUTE, 'M', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_VOLUME_MUTE),APPCOMMAND_VOLUME_MUTE));//, APPCOMMAND_VOLUME_MUTE
		//ADDCMD((ID_VOLUME_BOOST_INC, 0, FVIRTKEY|FNOINVERT, _T("Volume Boost Increase")));
		//ADDCMD((ID_VOLUME_BOOST_DEC, 0, FVIRTKEY|FNOINVERT, _T("Volume Boost Decrease")));
		//ADDCMD((ID_VOLUME_BOOST_MIN, 0, FVIRTKEY|FNOINVERT, _T("Volume Boost Min")));
		//ADDCMD((ID_VOLUME_BOOST_MAX, 0, FVIRTKEY|FNOINVERT, _T("Volume Boost Max")));
		ADDCMD((ID_NAVIGATE_TITLEMENU, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_TITLEMENU)));
		ADDCMD((ID_NAVIGATE_ROOTMENU, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_ROOTMENU)));
		ADDCMD((ID_NAVIGATE_SUBPICTUREMENU, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_SUBTITLE_MENU)));
		ADDCMD((ID_NAVIGATE_AUDIOMENU, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_AUDIO_MENU)));
		ADDCMD((ID_NAVIGATE_ANGLEMENU, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_ANGLE_MENU)));
		ADDCMD((ID_NAVIGATE_CHAPTERMENU, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_CHAP_MENU)));
		ADDCMD((ID_NAVIGATE_MENU_LEFT, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_LEFT)));
		ADDCMD((ID_NAVIGATE_MENU_RIGHT, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_RIGHT)));
		ADDCMD((ID_NAVIGATE_MENU_UP, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_UP)));
		ADDCMD((ID_NAVIGATE_MENU_DOWN, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_DOWN)));
		ADDCMD((ID_NAVIGATE_MENU_ACTIVATE, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_OK)));
		ADDCMD((ID_NAVIGATE_MENU_BACK, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_RETURN)));
		ADDCMD((ID_NAVIGATE_MENU_LEAVE, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_EXIT)));
		ADDCMD((ID_DVD_SUB_ONOFF, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_TOGGLE_SUBTITLE)));
		ADDCMD((ID_DVD_ANGLE_NEXT, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_ANGLE_NEXT)));
		ADDCMD((ID_DVD_ANGLE_PREV, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_ANGLE_PREV)));
		ADDCMD((ID_DVD_AUDIO_NEXT, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_AUDIO_NEXT)));
		ADDCMD((ID_DVD_AUDIO_PREV, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_AUDIO_PREV)));
		ADDCMD((ID_DVD_SUB_NEXT, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_SUBTITLE_NEXT)));
		ADDCMD((ID_DVD_SUB_PREV, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_DVD_MENU_CONTROL_SUBTITLE_PREV)));

		ADDCMD((ID_MENU_PLAYER_SHORT, 0, FVIRTKEY|FNOINVERT, _T("Player Menu (short)"), 0, wmcmd::RUP));
		//ADDCMD((ID_MENU_PLAYER_LONG, 0, FVIRTKEY|FNOINVERT, _T("Player Menu (long)")));
		//ADDCMD((ID_MENU_FILTERS, 0, FVIRTKEY|FNOINVERT, _T("Filters Menu")));
		ADDCMD((ID_VIEW_OPTIONS, 'O', FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_SETTING_PANEL)));
		ADDCMD((ID_STREAM_AUDIO_NEXT, 'A', FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_AUDIO_STREAM_NEXT)));
		ADDCMD((ID_STREAM_AUDIO_PREV, 'A', FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_AUDIO_STREAM_PREV)));
		ADDCMD((ID_STREAM_SUB_NEXT, 'S', FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_NEXT)));
		ADDCMD((ID_STREAM_SUB_PREV, 'S', FVIRTKEY|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_PREV)));
		ADDCMD((ID_STREAM_SUB_ONOFF, 'W', FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_STEAM_SUBTITLE_TOGGLE)));
		ADDCMD((ID_SUBTITLES_SUBITEM_START+2, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_SUBTITLE_RELOAD)));
		ADDCMD((ID_OGM_AUDIO_NEXT, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_OGM_AUDIO_STREAM_NEXT)));
		ADDCMD((ID_OGM_AUDIO_PREV, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_OGM_AUDIO_STREAM_PREV)));
		ADDCMD((ID_OGM_SUB_NEXT, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_OGM_SUBTITLE_NEXT)));
		ADDCMD((ID_OGM_SUB_PREV, 0, FVIRTKEY|FNOINVERT, ResStr(IDS_HOTKEY_OGM_SUBTITLE_PREV)));
		ADDCMD((ID_VSYNC_OFFSET_MORE, VK_UP, FVIRTKEY|FALT|FCONTROL|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_VSYNC_OFFSET_MORE)));
		ADDCMD((ID_VSYNC_OFFSET_LESS, VK_DOWN, FVIRTKEY|FALT|FCONTROL|FSHIFT|FNOINVERT, ResStr(IDS_HOTKEY_VSYNC_OFFSET_LESS)));
		ADDCMD((ID_SHOWDRAWSTAT, 'J', FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_TOGGLE_RENDER_STATUS)));

        ADDCMD((ID_PLAYLIST_DELETEITEM, VK_DELETE, FVIRTKEY|FNOINVERT, ResStr(IDS_PLAYLIST_DELETEITEM)));

//		ADDCMD((ID_SHOWSUBVOTEUI, 'V', FVIRTKEY|FCONTROL|FALT|FNOINVERT, L"Testing"));

        ADDCMD((ID_SEEK_TO_BEGINNING,VK_HOME, FVIRTKEY|FCONTROL|FNOINVERT,  ResStr(IDS_HOTKEY_SEEK_TO_BEGINNING)));
        ADDCMD((ID_SEEK_TO_MIDDLE, VK_DELETE, FVIRTKEY|FCONTROL|FNOINVERT,  ResStr(IDS_HOTKEY_SEEK_TO_MIDDLE)));
        ADDCMD((ID_SEEK_TO_END, VK_END, FVIRTKEY|FCONTROL|FNOINVERT, ResStr(IDS_HOTKEY_SEEK_TO_END)));
#undef ADDCMD
	}
	if(fSave)
	{
		if(!fInitialized) return;

		if(!AfxGetMyApp()->IsIniValid())
			AfxGetMyApp()->SetRegistryKey(_T("SPlayer"));

		if( bGenUIINIOnExit){ //save color theme sample ini
			CSVPToolBox svpTool;
			POSITION pos = colorsTheme.GetStartPosition();
			CString szCDATA;
			while(pos)
			{
				DWORD cVal;
				CString cKey;
				colorsTheme.GetNextAssoc(pos, cKey, cVal);

				CString szBuf;
				if(cVal > 10 || cVal == 0){
					DWORD result = cVal;
					cVal = (result&0xff000000 | ((result&0x000000ff) << 16) | ((result&0x0000ff00)) | ((result&0x00ff0000) >> 16)) ;
					szBuf.Format(_T("%s=#%06x\r\n"), cKey , cVal);
				}else{
					szBuf.Format(_T("%s=%d\r\n"), cKey , cVal);
				}
				szCDATA += szBuf;
				
			}
			svpTool.filePutContent(svpTool.GetPlayerPath(_T("uisample.ini")), szCDATA);
		}

		if(pApp->sqlite_setting){
			pApp->sqlite_setting->begin_transaction();
		}

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_CHECKFILEASSCONSTARTUP), fCheckFileAsscOnStartup);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_POPSTARTUPEXTCHECK), fPopupStartUpExtCheck);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_CHECKFILEEXTSASSCONSTARTUP), szStartUPCheckExts);
		
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_CHECKUPDATERINTERLEAVE), tCheckUpdaterInterleave);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LASTCHECKUPDATER), tLastCheckUpdater);


		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTORESUMEPLAY), autoResumePlay);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_HIDECAPTIONMENU), fHideCaptionMenu);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_CONTROLSTATE), nCS);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DEFAULTVIDEOFRAME), iDefaultVideoSize);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_KEEPASPECTRATIO), fKeepAspectRatio);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_COMPMONDESKARDIFF), fCompMonDeskARDiff);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VOLUME), nVolume);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_BALANCE), nBalance);
		//pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_MUTE), fMute);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LOOPNUM), nLoops);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LOOP), fLoopForever);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_REWIND), fRewind);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ZOOM), iZoomLevel);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_MULTIINST), fAllowMultipleInst);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_TITLEBARTEXTSTYLE), iTitleBarTextStyle);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_TITLEBARTEXTTITLE), fTitleBarTextTitle);		
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ONTOP), iOnTop);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_TRAYICON), fTrayIcon);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOZOOM), fRememberZoomLevel);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_FULLSCREENCTRLS), fShowBarsWhenFullScreen);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_FULLSCREENCTRLSTIMEOUT), nShowBarsWhenFullScreenTimeOut);
		pApp->WriteProfileBinary(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_FULLSCREENRES), (BYTE*)&dmFullscreenRes, sizeof(dmFullscreenRes));
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_EXITFULLSCREENATTHEEND), fExitFullScreenAtTheEnd);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_REMEMBERWINDOWPOS), fRememberWindowPos);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_REMEMBERWINDOWSIZE), fRememberWindowSize);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SNAPTODESKTOPEDGES), fSnapToDesktopEdges);		
		pApp->WriteProfileBinary(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LASTWINDOWRECT), (BYTE*)&rcLastWindowPos, sizeof(rcLastWindowPos));
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LASTWINDOWTYPE), lastWindowType);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ASPECTRATIO_X), AspectRatio.cx);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ASPECTRATIO_Y), AspectRatio.cy);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_KEEPHISTORY), fKeepHistory);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEGPUACEL), useGPUAcel);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_OPTIONDECODER), optionDecoder);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEGPUCUDA), useGPUCUDA);

		
        pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOCONVSUB)+L"BIG2GB", autoIconvSubBig2GB);
        pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOCONVSUB)+L"GB2BIG", autoIconvSubGB2BIG);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTODOWNLAODSVPSUB), autoDownloadSVPSub);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_NOTAUTOCHECKSPEAKER), bNotAutoCheckSpeaker);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_FCUSTOMSPEAKERS), fCustomSpeakers);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DSVIDEORENDERERTYPE), iDSVideoRendererType);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_RMVIDEORENDERERTYPE), iRMVideoRendererType);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_QTVIDEORENDERERTYPE), iQTVideoRendererType);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_APSURACEFUSAGE), iAPSurfaceUsage);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VMRSYNCFIX), fVMRSyncFix);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VMRSYNCFIX) +_T("Goth"), fVMRGothSyncFix);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DX9_RESIZER), iDX9Resizer);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VMR9MIXERMODE), fVMR9MixerMode);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VMR9MIXERYUV), fVMR9MixerYUV);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUDIORENDERERTYPE), CString(AudioRendererDisplayName));
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOLOADAUDIO), fAutoloadAudio);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOLOADSUBTITLES), fAutoloadSubtitles);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOLOADSUBTITLES)+_T("2"), fAutoloadSubtitles2);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_BLOCKVSFILTER), fBlockVSFilter);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ENABLEWORKERTHREADFOROPENING), fEnableWorkerThreadForOpening);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_REPORTFAILEDPINS), fReportFailedPins);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_UPLOADFAILEDPINS), fUploadFailedPinsInfo);
		
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DVDPATH), sDVDPath);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEDVDPATH), fUseDVDPath);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_MENULANG), idMenuLang);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUDIOLANG), idAudioLang);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SUBTITLESLANG), idSubtitlesLang);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOSPEAKERCONF), fAutoSpeakerConf);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USESPDIF), fbUseSPDIF);
		

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ONLYUSEINTERNALDEC), onlyUseInternalDec);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USESMARTDRAG), useSmartDrag);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DECSPEAKERS), iDecSpeakers);
		
		
		CString style;
		CString style2;
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPLOGFONT), style <<= subdefstyle);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPLOGFONT2), style2 <<= subdefstyle2);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPOVERRIDEPLACEMENT), fOverridePlacement);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPOVERRIDEPLACEMENT)+_T("2"), fOverridePlacement2);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPHORPOS), nHorPos);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPVERPOS), nVerPos);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPHORPOS2), nHorPos2);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPVERPOS2), nVerPos2);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPCSIZE), nSPCSize);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPCMAXRES), nSPCMaxRes);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SUBDELAYINTERVAL), nSubDelayInterval);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_POW2TEX), fSPCPow2Tex);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ENABLESUBTITLES), fEnableSubtitles);		
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ENABLESUBTITLES2), fEnableSubtitles2);		
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ENABLEAUDIOSWITCHER), fEnableAudioSwitcher);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ENABLEAUDIOTIMESHIFT), fAudioTimeShift);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUDIOTIMESHIFT), tAudioTimeShift);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DOWNSAMPLETO441), fDownSampleTo441);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_CUSTOMCHANNELMAPPING), fCustomChannelMapping);
		pApp->WriteProfileBinary(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPEAKERTOCHANNELMAPPING)+_T("Offset"), (BYTE*)pSpeakerToChannelMapOffset, sizeof(pSpeakerToChannelMapOffset));
		pApp->WriteProfileBinary(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPEAKERTOCHANNELMAPPING)+_T("Custom"), (BYTE*)pSpeakerToChannelMap2Custom, sizeof(pSpeakerToChannelMap2Custom));
		
		pApp->WriteProfileBinary(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_EQCONTROL), (BYTE*)pEQBandControlCustom, sizeof(pEQBandControlCustom));
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_EQCONTROLPERSET), pEQBandControlPerset);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUDIONORMALIZE), fAudioNormalize);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUDIONORMALIZERECOVER), fAudioNormalizeRecover);		
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUDIOBOOST), min((int)AudioBoost, 56)); // not more than 415%

		//pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DISABLE_EVR), bDisableEVR);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_FASTERSEEKING), fFasterSeeking);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEINTERNALTSSPLITER), fUseInternalTSSpliter);
		
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEAEROGLASS), bAeroGlass);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USETRANSCONTROL), bTransControl);

		//pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DISABLESMARTDRAG),  disableSmartDrag );
		
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SHADERLIST), strShaderList);

		{
			for(int i = 0; ; i++)
			{
				CString key;
				key.Format(_T("%s\\%04d"), ResStr(IDS_R_FILTERS), i);
				int j = pApp->GetProfileInt(key, _T("Enabled"), -1); 
				pApp->WriteProfileString(key, NULL, NULL);
				if(j < 0) break;
			}
			pApp->WriteProfileString(ResStr(IDS_R_FILTERS), NULL, NULL);

			POSITION pos = filters.GetHeadPosition();
			for(int i = 0; pos; i++)
			{
				FilterOverride* f = filters.GetNext(pos);

				if(f->fTemporary)
					continue;

				CString key;
				key.Format(_T("%s\\%04d"), ResStr(IDS_R_FILTERS), i);

				pApp->WriteProfileInt(key, _T("SourceType"), (int)f->type);
				pApp->WriteProfileInt(key, _T("Enabled"), (int)!f->fDisabled);
				if(f->type == FilterOverride::REGISTERED)
				{
					pApp->WriteProfileString(key, _T("DisplayName"), CString(f->dispname));
					pApp->WriteProfileString(key, _T("Name"), f->name);
				}
				else if(f->type == FilterOverride::EXTERNAL)
				{
					pApp->WriteProfileString(key, _T("Path"), f->path);
					pApp->WriteProfileString(key, _T("Name"), f->name);
					pApp->WriteProfileString(key, _T("CLSID"), CStringFromGUID(f->clsid));
				}
				POSITION pos2 = f->backup.GetHeadPosition();
				for(int i = 0; pos2; i++)
				{
					CString val;
					val.Format(_T("org%04d"), i);
					pApp->WriteProfileString(key, val, CStringFromGUID(f->backup.GetNext(pos2)));
				}
				pos2 = f->guids.GetHeadPosition();
				for(int i = 0; pos2; i++)
				{
					CString val;
					val.Format(_T("mod%04d"), i);
					pApp->WriteProfileString(key, val, CStringFromGUID(f->guids.GetNext(pos2)));
				}
				pApp->WriteProfileInt(key, _T("LoadType"), f->iLoadType);
				pApp->WriteProfileInt(key, _T("Merit"), f->dwMerit);
			}
		}

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_INTREALMEDIA), fIntRealMedia);
		// pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_REALMEDIARENDERLESS), fRealMediaRenderless);
		// pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_QUICKTIMERENDERER), iQuickTimeRenderer);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_REALMEDIAFPS), *((DWORD*)&RealMediaQuickTimeFPS));

		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS) + _T("\\") + ResStr(IDS_RS_PNSPRESETS), NULL, NULL);
		for(int i = 0, j = m_pnspresets.GetCount(); i < j; i++)
		{
			CString str;
			str.Format(_T("Preset%d"), i);
			pApp->WriteProfileString(ResStr(IDS_R_SETTINGS) + _T("\\") + ResStr(IDS_RS_PNSPRESETS), str, m_pnspresets[i]);
		}

		pApp->WriteProfileString(ResStr(IDS_R_COMMANDS), NULL, NULL);
		POSITION pos = wmcmds.GetHeadPosition();
		
		for(int i = 0; pos; )
		{
			POSITION posc = pos;
			wmcmd& wc = wmcmds.GetNext(pos);
			if(wc.IsModified())
			{
				CString str;
				str.Format(_T("CommandMod%d"), i);
				int cmdidx = FindWmcmdsIDXofCmdid( wc.cmd , posc);
				CString str2;
				str2.Format(_T("%d %x %x %s %d %d %d %d"), 
					wc.cmd, wc.fVirt, wc.key, 
					_T("\"") + CString(wc.rmcmd) +  _T("\""), wc.rmrepcnt,
					wc.mouse, wc.appcmd, cmdidx);
				//SVP_LogMsg(str2);
				pApp->WriteProfileString(ResStr(IDS_R_COMMANDS), str, str2);
				i++;
			}
			
		}
		CString		strTemp;
		strTemp.Format (_T("%f"), m_RenderSettings.fTargetSyncOffset);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VSYNC_TARGETOFFSET), strTemp);


		strTemp.Format (_T("%f"), dBrightness);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_COLOR_BRIGHTNESS), strTemp);
		strTemp.Format (_T("%f"), dContrast);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_COLOR_CONTRAST), strTemp);
		strTemp.Format (_T("%f"), dHue);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_COLOR_HUE), strTemp);
		strTemp.Format (_T("%f"), dSaturation);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_COLOR_SATURATION), strTemp);

		
		strTemp.Format (_T("%f"), dGSubFontRatio);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_GLOBAL_SUBFONTRATIO), strTemp);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DSSVPRENDERTYE), iSVPRenderType);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DISABLECENTERBIGOPENBMP), bDisableCenterBigOpenBmp);
		 
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WINLIRC), fWinLirc);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WINLIRCADDR), WinLircAddr);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_UICE), fUIce);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_UICEADDR), UIceAddr);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DISABLEXPTOOLBARS), fDisabeXPToolbars);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEWMASFREADER), fUseWMASFReader);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_JUMPDISTS), nJumpDistS);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_JUMPDISTM), nJumpDistM);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_JUMPDISTL), nJumpDistL);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_FREEWINDOWRESIZING), fFreeWindowResizing);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_NOTIFYMSN), fNotifyMSN);		
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_NOTIFYGTSDLL), fNotifyGTSdll);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VMDETECTED), fVMDetected);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SAVESVPSUBWITHVIDEO), bSaveSVPSubWithVideo);
		  
		Formats.UpdateData(true);

		pApp->WriteProfileInt(ResStr(IDS_R_INTERNAL_FILTERS), ResStr(IDS_RS_SRCFILTERS), SrcFilters|~(SRC_LAST-1));
		pApp->WriteProfileInt(ResStr(IDS_R_INTERNAL_FILTERS), ResStr(IDS_RS_TRAFILTERS), TraFilters|~(TRA_LAST-1));

		pApp->WriteProfileInt(ResStr(IDS_R_INTERNAL_FILTERS), ResStr(IDS_RS_DXVACOMPAT), bDVXACompat);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USERFFMPEGWMV), useFFMPEGWMV);
		
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SMOOTHMUTILMONITOR), fbSmoothMutilMonitor);

		//pApp->WriteProfileInt(ResStr(IDS_R_INTERNAL_FILTERS), ResStr(IDS_RS_DXVAFILTERS), DXVAFilters|~(DXVA_LAST-1));
		//pApp->WriteProfileInt(ResStr(IDS_R_INTERNAL_FILTERS), ResStr(IDS_RS_FFMPEGFILTERS), FFmpegFilters|~(FFM_LAST-1));

		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LOGOFILE), logofn);

		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_UELASTPANEL), szUELastPanel);
		
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LOGOID), logoid);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LOGOEXT), logoext);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LOGOSTRETCH), logostretch);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_HIDECDROMSSUBMENU), fHideCDROMsSubMenu);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DONTNEEDSVPSUBFILTER), bDontNeedSVPSubFilter);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_PRIORITY), priority);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LAUNCHFULLSCREEN), launchfullscreen);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ENABLEWEBSERVER), fEnableWebServer);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBSERVERPORT), nWebServerPort);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBSERVERPRINTDEBUGINFO), fWebServerPrintDebugInfo);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBSERVERUSECOMPRESSION), fWebServerUseCompression);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBSERVERLOCALHOSTONLY), fWebServerLocalhostOnly);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBROOT), WebRoot);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBDEFINDEX), WebDefIndex);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBSERVERCGI), WebServerCGI);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USERGBONLY), bRGBOnly);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_HARDWAREDECODERFAILCOUNT), lHardwareDecoderFailCount);
		
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SNAPSHOTPATH), SnapShotPath);
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SNAPSHOTEXT), SnapShotExt);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_THUMBROWS), ThumbRows);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_THUMBCOLS), ThumbCols);
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_THUMBWIDTH), ThumbWidth);		

		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ISDB), ISDb);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SHOWCONTROLBAR),bShowControlBar);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEWAVEOUTDEVICEBYDEFAULT), bUseWaveOutDeviceByDefault);

		
		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SVPSUBSTOREDIR), SVPSubStoreDir);

        pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS),  ResStr(IDS_RS_SVPSUBSTOREDIR) + L"_DONT_DEL", bDontDeleteOldSubFileAutomaticly );

		pApp->WriteProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SVPPERFSUB), szSVPSubPerf);


		pApp->WriteProfileString(_T("Shaders"), NULL, NULL);
		pApp->WriteProfileInt(_T("Shaders"), _T("Initialized"), 1);
		pApp->WriteProfileString(_T("Shaders"), _T("Combine"), m_shadercombine);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_NOTCHANGEFONTTOYH), bNotChangeFontToYH);

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ULTRAFAST), fBUltraFastMode);


		pos = m_shaders.GetHeadPosition();
		for(int i = 0; pos; i++)
		{
			const Shader& s = m_shaders.GetNext(pos);

			if(!s.label.IsEmpty())
			{
				CString index;
				index.Format(_T("%d"), i);
				CString srcdata = s.srcdata;
				srcdata.Replace(_T("\r"), _T(""));
				srcdata.Replace(_T("\n"), _T("\\n"));
				srcdata.Replace(_T("\t"), _T("\\t"));
				AfxGetMyApp()->WriteProfileString(_T("Shaders"), index, s.label + _T("|") + s.target + _T("|") + srcdata);
			}
		}

		if(pApp->m_pszRegistryKey)
		{
			// WINBUG: on win2k this would crash WritePrivateProfileString
			pApp->WriteProfileInt(_T(""), _T(""), pApp->GetProfileInt(_T(""), _T(""), 0)?0:1);
		}

		if(bUserAeroUI()){
			//pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_TRANSPARENTTOOLBARPOSOFFSET)+_T("2"), m_lTransparentToolbarPosOffset);		
		}
		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), _T("LastVersion"), 968);		
		if(pApp->sqlite_setting){
			pApp->sqlite_setting->end_transaction();
		}

	}
	else
	{
		if(fInitialized) return;

		OSVERSIONINFO vi;
		vi.dwOSVersionInfoSize = sizeof(vi);
		GetVersionEx(&vi);
		fXpOrBetter = (vi.dwMajorVersion >= 5 && vi.dwMinorVersion >= 1 || vi.dwMajorVersion >= 6);
		int iUpgradeReset =  pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), _T("LastVersion"), 1) ;
		/*/	RegQueryStringValue( HKLM, 'SOFTWARE\Microsoft\DirectX', 'Version', sVersion );
		DirectX 8.0 is 4.8.0
		DirectX 8.1 is 4.8.1
		DirectX 9.0 is 4.9.0
		//*/
		iDXVer = 0;
		CRegKey dxver;
		if(ERROR_SUCCESS == dxver.Open(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\DirectX"), KEY_READ))
		{
			CString str;
			ULONG len = 64;
			if(ERROR_SUCCESS == dxver.QueryStringValue(_T("Version"), str.GetBuffer(len), &len))
			{
				str.ReleaseBuffer(len);
				int ver[4];
				_stscanf(str, _T("%d.%d.%d.%d"), ver+0, ver+1, ver+2, ver+3);
				iDXVer = ver[1];
			}
		}

		CString szOEMSub;
		CRegKey oem;
		if(ERROR_SUCCESS == oem.Open(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\SPlayer"), KEY_READ))
		{
			CString str;
			ULONG len = 640;
			if(ERROR_SUCCESS == oem.QueryStringValue(_T("OEM"), str.GetBuffer(len), &len))
			{
				str.ReleaseBuffer(len);
				szOEMTitle = str;
			}

			if(ERROR_SUCCESS == oem.QueryStringValue(_T("OEMSUB"), str.GetBuffer(len), &len))
			{
				str.ReleaseBuffer(len);
				szOEMSub = str;
			}
		}
		
		szSVPSubPerf = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SVPPERFSUB), szOEMSub);
		if(!szOEMSub.IsEmpty() && szSVPSubPerf.IsEmpty()){
			szSVPSubPerf = szOEMSub;
		}
		
		fVMDetected = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VMDETECTED), -1);
		if(fVMDetected == -1 || (fVMDetected == 0 && iUpgradeReset < 390)){
			if(IsInsideVM() ){
				fVMDetected = 1;
			}else{
				fVMDetected = 0;
			}
		}
		bOldLumaControl = 0;
		
		bIsIVM = false;
		szCurrentExtension.Empty();


		fCheckFileAsscOnStartup = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_CHECKFILEASSCONSTARTUP), 1);
		szStartUPCheckExts = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_CHECKFILEEXTSASSCONSTARTUP), _T(".mkv .avi .rmvb .rm .wmv .asf .mov .mp4 .mpeg .mpg .3gp"));
		fPopupStartUpExtCheck = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_POPSTARTUPEXTCHECK), 1);
		
		tCheckUpdaterInterleave = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_CHECKUPDATERINTERLEAVE),  86400);
		tLastCheckUpdater = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LASTCHECKUPDATER),  0);
		

		autoResumePlay = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTORESUMEPLAY), 1);
		fHideCaptionMenu = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_HIDECAPTIONMENU), 0);
		

		iDefaultVideoSize = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DEFAULTVIDEOFRAME), DVS_FROMINSIDE);
		fKeepAspectRatio = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_KEEPASPECTRATIO), TRUE);
		fCompMonDeskARDiff = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_COMPMONDESKARDIFF), FALSE);
		nVolume = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VOLUME), 100);
		nBalance = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_BALANCE), 0);
		fMute = 0;//!!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_MUTE), 0);
		nLoops = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LOOPNUM), 2);
		fLoopForever = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LOOP), 0);
		fRewind = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_REWIND), FALSE);
		iZoomLevel = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ZOOM), 1);

		lHardwareDecoderFailCount = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_HARDWAREDECODERFAILCOUNT), 0);

		bDisableCenterBigOpenBmp = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DISABLECENTERBIGOPENBMP), 0) > 0;

		CSVPToolBox svptoolbox;
		fForceRGBrender = 0;
		useGPUAcel = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEGPUACEL), -1);
		if(useGPUAcel < 0 ){
			try{
				useGPUAcel = !!svptoolbox.GetGPUString(&szaGPUStrings);
			}
			catch(...){
				useGPUAcel = 0;
			}
		}


		bool bDefaultVSync = false;
		bool bDefaultGothSync = false;
		if(!bAeroGlassAvalibility ){//&& !useGPUAcel
			//HINSTANCE hEVR = LoadLibrary(_T("evr.dll"));
			//if(hEVR){
			bDefaultGothSync = true;
			//	FreeLibrary(hEVR);
			//}else
			//	bDefaultVSync = true;
		}

		//fVMRSyncFix = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VMRSYNCFIX), 0);//bDefaultVSync
		fVMRSyncFix = 0;
		fVMRGothSyncFix = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VMRSYNCFIX) + _T("Goth"), bDefaultGothSync);
		if(iUpgradeReset < 652){
			fVMRSyncFix = bDefaultVSync;
			fVMRGothSyncFix = bDefaultGothSync;
		}
		m_RenderSettings.bSynchronizeNearest = fVMRGothSyncFix;
		m_RenderSettings.bSynchronizeVideo = 0;//fVMRSyncFix;

		szUELastPanel = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_UELASTPANEL), _T(""));

		optionDecoder = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_OPTIONDECODER), _T("CoreAVCdec"));
		//iDXVer = 7;
		if(useGPUAcel){
		//	iDXVer = 9;
		}
		bRGBOnly = 0;//!!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USERGBONLY), 0);

		useFFMPEGWMV = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USERFFMPEGWMV), 1);
 		if(fVMDetected ){
 			//fForceRGBrender = 1;
			//bRGBOnly = 1;
 			iDXVer = 7;
 		}
		int iDefaultSVPRenderType =  (IsVista() || iDXVer >= 9);
		
		if(!IsVista()){
			BOOL noDX93D = false;
			for(int i = 0; i < szaGPUStrings.GetCount();i++){
				if(szaGPUStrings.GetAt(i).Find(_T("(0x8086::0x2a42)")) >= 0
					|| szaGPUStrings.GetAt(i).Find(_T("(0x1039::0x6351)")) >= 0){ // SiS Mirage 3 Graphics
					noDX93D = true;
				}
				
			}
			if(AfxGetMyApp()->GetD3X9Dll()==NULL){
				noDX93D = true;
			}
			if( noDX93D ){
				iDefaultSVPRenderType = 0;
				useGPUAcel = 0;
			}
		}
		iSVPRenderType =  pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DSSVPRENDERTYE), iDefaultSVPRenderType);
		iDSVideoRendererType = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DSVIDEORENDERERTYPE), ( (IsVista() || iDXVer >= 9) ? VIDRNDT_DS_VMR9RENDERLESS : VIDRNDT_DS_VMR7RENDERLESS) );
		if((iDSVideoRendererType != VIDRNDT_DS_VMR7RENDERLESS && iDSVideoRendererType != VIDRNDT_DS_VMR9RENDERLESS) || fVMDetected){//|| iDSVideoRendererType != VIDRNDT_DS_OVERLAYMIXER
			iDSVideoRendererType = VIDRNDT_DS_VMR7RENDERLESS;
		}
		iRMVideoRendererType = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_RMVIDEORENDERERTYPE), ( (IsVista() || iDXVer >= 9) ? VIDRNDT_RM_DX9 : VIDRNDT_RM_DX7 ) );
		iQTVideoRendererType = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_QTVIDEORENDERERTYPE),  ( (IsVista() || iDXVer >= 9) ? VIDRNDT_QT_DX9 : VIDRNDT_QT_DX7 ) );
		if(iSVPRenderType ){
			iDSVideoRendererType = VIDRNDT_DS_VMR9RENDERLESS;
			iRMVideoRendererType = VIDRNDT_RM_DX9;
			iQTVideoRendererType = VIDRNDT_QT_DX9;
			iAPSurfaceUsage = VIDRNDT_AP_TEXTURE3D;
		}else{// if(m_sgs_videorender == _T("DX7"))
			iSVPRenderType = 0; 
			if(IsVista())
				iDSVideoRendererType = VIDRNDT_DS_OLDRENDERER;
			else
				iDSVideoRendererType = VIDRNDT_DS_OVERLAYMIXER;

			iRMVideoRendererType = VIDRNDT_RM_DEFAULT;
			iQTVideoRendererType = VIDRNDT_QT_DEFAULT;
		}

		iAPSurfaceUsage = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_APSURACEFUSAGE), VIDRNDT_AP_TEXTURE3D);
		iAPSurfaceUsage = VIDRNDT_AP_TEXTURE3D;
		useGPUCUDA = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEGPUCUDA), 0);
		useGPUCUDA = SVP_CanUseCoreAvcCUDA(useGPUCUDA);

		bNotAutoCheckSpeaker = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_NOTAUTOCHECKSPEAKER), 0);
		fCustomSpeakers = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_FCUSTOMSPEAKERS), 0) >0;
		
		
		SVPSubStoreDir = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SVPSUBSTOREDIR), _T(""));

        bDontDeleteOldSubFileAutomaticly =pApp->GetProfileInt(ResStr(IDS_R_SETTINGS) , ResStr(IDS_RS_SVPSUBSTOREDIR) + L"_DONT_DEL", 0 );

		bDontNeedSVPSubFilter = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DONTNEEDSVPSUBFILTER), 0) >0;

		fBUltraFastMode = 0;// !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ULTRAFAST), 1);
		autoDownloadSVPSub = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTODOWNLAODSVPSUB), 1);

        autoIconvSubBig2GB = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOCONVSUB)+L"BIG2GB", 1);
        autoIconvSubGB2BIG = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOCONVSUB)+L"GB2BIG", 1);

        
		//fVMRSyncFix = 0;
		iDX9Resizer = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DX9_RESIZER), 1);
		fVMR9MixerMode = FALSE;//!!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VMR9MIXERMODE), FALSE);
		fVMR9MixerYUV = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VMR9MIXERYUV), FALSE);
		AudioRendererDisplayName = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUDIORENDERERTYPE), _T(""));
		fAutoloadAudio = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOLOADAUDIO), TRUE);
		fAutoloadSubtitles = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOLOADSUBTITLES), TRUE );//!IsVSFilterInstalled()
		fAutoloadSubtitles2 = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOLOADSUBTITLES)+_T("2"), FALSE ); 
		fBlockVSFilter = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_BLOCKVSFILTER), TRUE);
		fEnableWorkerThreadForOpening = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ENABLEWORKERTHREADFOROPENING), TRUE);
		fReportFailedPins = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_REPORTFAILEDPINS), FALSE);
		fUploadFailedPinsInfo = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_UPLOADFAILEDPINS), TRUE );
		fAllowMultipleInst = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_MULTIINST), 0);
		iTitleBarTextStyle = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_TITLEBARTEXTSTYLE), 1);
		fTitleBarTextTitle = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_TITLEBARTEXTTITLE), FALSE);
		iOnTop = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ONTOP), 0);
		fTrayIcon = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_TRAYICON), 0);
		fRememberZoomLevel = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOZOOM), 1);
		fShowBarsWhenFullScreen = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_FULLSCREENCTRLS), 1);
		nShowBarsWhenFullScreenTimeOut = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_FULLSCREENCTRLSTIMEOUT), 0);

		fFasterSeeking = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_FASTERSEEKING), 1) >0;
		
		if(pApp->GetProfileBinary(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_FULLSCREENRES), &ptr, &len))
		{
			memcpy(&dmFullscreenRes, ptr, sizeof(dmFullscreenRes));
			delete [] ptr;
		}
		else
		{
			dmFullscreenRes.fValid = false;
		}
		fExitFullScreenAtTheEnd = 0;//!!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_EXITFULLSCREENATTHEEND), 1);
		//if(iUpgradeReset < 720){
		//	fExitFullScreenAtTheEnd = 1;
		//}
		fRememberWindowPos = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_REMEMBERWINDOWPOS), 1);
		fRememberWindowSize = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_REMEMBERWINDOWSIZE), 0);
		fSnapToDesktopEdges = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SNAPTODESKTOPEDGES), 1);
		if(iUpgradeReset < 341){
			fRememberWindowSize = 0;
		}
		if(iUpgradeReset < 51){
			fSnapToDesktopEdges = 1;
		}
		AspectRatio.cx = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ASPECTRATIO_X), 0);
		AspectRatio.cy = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ASPECTRATIO_Y), 0);
		fKeepHistory = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_KEEPHISTORY), 1);
		if(pApp->GetProfileBinary(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LASTWINDOWRECT), &ptr, &len))
		{
			memcpy(&rcLastWindowPos, ptr, sizeof(rcLastWindowPos));
			delete [] ptr;
			
		}
		else
		{
			if ( rcLastWindowPos.Height() < 200){
				rcLastWindowPos.bottom = rcLastWindowPos.top + 360;
			}
			fRememberWindowPos = false;
		}
		lastWindowType = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LASTWINDOWTYPE), SIZE_RESTORED);
		sDVDPath = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DVDPATH), _T(""));
		fUseDVDPath = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEDVDPATH), 0);
		idMenuLang = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_MENULANG), ::GetUserDefaultLCID());
		idAudioLang = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUDIOLANG), ::GetUserDefaultLCID());
		idSubtitlesLang = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SUBTITLESLANG), ::GetUserDefaultLCID());
		fAutoSpeakerConf = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUTOSPEAKERCONF), 1);
		
		// TODO: rename subdefstyle -> defStyle, IDS_RS_SPLOGFONT -> IDS_RS_SPSTYLE
		subdefstyle <<= pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPLOGFONT), ResStr(IDS_SETTING_DEFAULT_MAIN_SUBTITLE_FONT_STYLE));
		subdefstyle2 <<= pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPLOGFONT2), ResStr(IDS_SETTING_DEFAULT_2ND_SUBTITLE_FONT_STYLE));

		CheckSVPSubExts = _T(" .ts; .avi; .mkv;");
		fbSmoothMutilMonitor = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SMOOTHMUTILMONITOR), 1) >0;
		bShowControlBar = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SHOWCONTROLBAR), 0) >0;

		m_RenderSettings.fTargetSyncOffset = (float)_tstof(pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_VSYNC_TARGETOFFSET), _T("1.85")));


		dBrightness		= (float)_tstof(pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_COLOR_BRIGHTNESS),	_T("100")));
		if(iUpgradeReset < 580){
			dBrightness = 100;
		}
		dContrast		= (float)_tstof(pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_COLOR_CONTRAST),		_T("1.0")));
		dHue			= (float)_tstof(pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_COLOR_HUE),			_T("0")));
		dSaturation		= (float)_tstof(pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_COLOR_SATURATION),	_T("1")));

		dGSubFontRatio		= (float)_tstof(pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_GLOBAL_SUBFONTRATIO),	_T("1.0")));

		
		bNotChangeFontToYH = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_NOTCHANGEFONTTOYH), 0) >0;
//		disableSmartDrag = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DISABLESMARTDRAG),  -1 );
		bDisableEVR = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DISABLE_EVR), 0);
		bUseWaveOutDeviceByDefault = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEWAVEOUTDEVICEBYDEFAULT), 0);
		fCustomChannelMapping = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_CUSTOMCHANNELMAPPING), 1);
		iDecSpeakers = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DECSPEAKERS), -1);
		iSS = abs(iDecSpeakers) % 1000;
		int iNumberOfSpeakers = -1;
		if(iDecSpeakers == -1 || (iSS == 200 && iUpgradeReset < 400) ){
			iDecSpeakers = 200;
			iNumberOfSpeakers = GetNumberOfSpeakers();
			switch( iNumberOfSpeakers ){
				case 1: iDecSpeakers = 100;	break;
				case 2: iDecSpeakers = 200;	break;
				case 3: iDecSpeakers = 210;	break;
				case 4: iDecSpeakers = 220;	break;
				case 5: iDecSpeakers = 221;	break;
				case 6: iDecSpeakers = 321;	break;
				case 7: iDecSpeakers = 321;	break;
				case 8: iDecSpeakers = 321;	break;
			}
		}

		InitChannelMap();

		

		memset(pSpeakerToChannelMap2Custom, 0 , sizeof(pSpeakerToChannelMap2Custom));
		if( pApp->GetProfileBinary(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPEAKERTOCHANNELMAPPING)+_T("Custom"), &ptr, &len)  )
		{
			memcpy(pSpeakerToChannelMap2Custom, ptr, sizeof(pSpeakerToChannelMap2Custom));
			delete [] ptr;

			if(iUpgradeReset < 968){
				//FixOffsetSetting
				float tmp;
				for(int iInputChannelCount = 5; iInputChannelCount <= MAX_INPUT_CHANNELS; iInputChannelCount++){
					for(int iOutputChannelCount = 2; iOutputChannelCount <= MAX_OUTPUT_CHANNELS; iOutputChannelCount++){

						for(int iSpeakerID = 0; iSpeakerID < iOutputChannelCount; iSpeakerID++){
							int iLEFChann = max(iInputChannelCount-1, 5);
							tmp = pSpeakerToChannelMap2Custom[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iLEFChann] ;

							for(int iChannelID = iLEFChann; iChannelID > 3; iChannelID--){
								pSpeakerToChannelMap2Custom[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iChannelID]
								= pSpeakerToChannelMap2Custom[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iChannelID-1];
							}
							pSpeakerToChannelMap2Custom[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][3] = tmp;
						}

						if(iOutputChannelCount > 4){
							for(int iChannelID = 0; iChannelID < iInputChannelCount; iChannelID++){
								int iLEFChann = max(iOutputChannelCount-1, 5);
								tmp = pSpeakerToChannelMap2Custom[iInputChannelCount-1][iOutputChannelCount-1][iLEFChann][iChannelID] ;
								for(int iSpeakerID = iLEFChann; iSpeakerID > 3; iSpeakerID--){
									pSpeakerToChannelMap2Custom[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID][iChannelID]
									= pSpeakerToChannelMap2Custom[iInputChannelCount-1][iOutputChannelCount-1][iSpeakerID-1][iChannelID];
								}
								pSpeakerToChannelMap2Custom[iInputChannelCount-1][iOutputChannelCount-1][3][iChannelID] = tmp;
							}
						}

					}
				}
			}
			ChangeChannelMapByCustomSetting();

		}
		memset(pSpeakerToChannelMapOffset, 0 , sizeof(pSpeakerToChannelMapOffset));
		if( pApp->GetProfileBinary(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPEAKERTOCHANNELMAPPING)+_T("Offset"), &ptr, &len)  )
		{
			memcpy(pSpeakerToChannelMapOffset, ptr, sizeof(pSpeakerToChannelMapOffset));
			delete [] ptr;

		}
		pEQBandControlPerset = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_EQCONTROLPERSET), 0);

		memset(pEQBandControlCustom, 0 , sizeof(pEQBandControlCustom));
		if(pApp->GetProfileBinary(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_EQCONTROL), &ptr, &len)  )
		{
			memcpy(pEQBandControlCustom, ptr, sizeof(pEQBandControlCustom));
			delete [] ptr;
		}

		InitEQPerset();
		
		//SVP_LogMsg5(L"Init ChannelMap %f", pSpeakerToChannelMap2[5][1][0][1]);

		fbUseSPDIF = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USESPDIF), 0);

		fUseInternalTSSpliter = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEINTERNALTSSPLITER), 0);


		bHasCUDAforCoreAVC = (svptoolbox.CanUseCUDAforCoreAVC() == TRUE);
		//bSupportFFGPU = svptoolbox.SupportFFGP

		CWinThread* th_InitSettingInstance = AfxBeginThread( Thread_AppSettingLoadding, this, THREAD_PRIORITY_LOWEST , 0, CREATE_SUSPENDED);
		th_InitSettingInstance->m_pMainWnd = AfxGetMainWnd();
		th_InitSettingInstance->ResumeThread();
		
		fOverridePlacement = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPOVERRIDEPLACEMENT), 0);
		fOverridePlacement2 = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPOVERRIDEPLACEMENT)+_T("2"), TRUE);
		nHorPos = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPHORPOS), 50);
		nVerPos = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPVERPOS), 90);
		nHorPos2 = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPHORPOS2), 50);
		nVerPos2 = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPVERPOS2), 95);
		nSPCSize = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPCSIZE), 3);
		nSPCMaxRes = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SPCMAXRES), 2);
		nSubDelayInterval = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SUBDELAYINTERVAL), 500);
		fSPCPow2Tex = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_POW2TEX), TRUE);
		fEnableSubtitles = TRUE;
		if(!autoDownloadSVPSub){
		  fEnableSubtitles = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ENABLESUBTITLES), TRUE);
		}
		fEnableSubtitles2 = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ENABLESUBTITLES2), TRUE);
		fEnableAudioSwitcher = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ENABLEAUDIOSWITCHER), TRUE);
		fAudioTimeShift = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ENABLEAUDIOTIMESHIFT), 0);
		tAudioTimeShift = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUDIOTIMESHIFT), 0);
		fDownSampleTo441 = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DOWNSAMPLETO441), 0);
		
		onlyUseInternalDec = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ONLYUSEINTERNALDEC), FALSE);
		useSmartDrag = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USESMARTDRAG), FALSE);


		
		if(iUpgradeReset < 49){
			subdefstyle.relativeTo = 0;	
		}
		if(iUpgradeReset < 95){
			nCS |= CS_STATUSBAR;
			fCustomChannelMapping = FALSE;
		}
		if(iUpgradeReset < 122){
			fDownSampleTo441 = 0;
		}
		if(iUpgradeReset < 670){
			bDisableEVR = 0;
		}
		
		fAudioNormalize = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUDIONORMALIZE), FALSE);
		fAudioNormalizeRecover = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUDIONORMALIZERECOVER), TRUE);
		AudioBoost = (float)pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_AUDIOBOOST), 1);


		{
			for(int i = 0; ; i++)
			{
				CString key;
				key.Format(_T("%s\\%04d"), ResStr(IDS_R_FILTERS), i);

				CAutoPtr<FilterOverride> f(new FilterOverride);

				f->fDisabled = !pApp->GetProfileInt(key, _T("Enabled"), 0);

				UINT j = pApp->GetProfileInt(key, _T("SourceType"), -1);
				if(j == 0)
				{
					f->type = FilterOverride::REGISTERED;
					f->dispname = CStringW(pApp->GetProfileString(key, _T("DisplayName"), _T("")));
					f->name = pApp->GetProfileString(key, _T("Name"), _T(""));
				}
				else if(j == 1)
				{
					f->type = FilterOverride::EXTERNAL;
					f->path = pApp->GetProfileString(key, _T("Path"), _T(""));
					f->name = pApp->GetProfileString(key, _T("Name"), _T(""));
					f->clsid = GUIDFromCString(pApp->GetProfileString(key, _T("CLSID"), _T("")));
				}
				else
				{
					pApp->WriteProfileString(key, NULL, 0);
					break;
				}

				f->backup.RemoveAll();
				for(int i = 0; ; i++)
				{
					CString val;
					val.Format(_T("org%04d"), i);
					CString guid = pApp->GetProfileString(key, val, _T(""));
					if(guid.IsEmpty()) break;
					f->backup.AddTail(GUIDFromCString(guid));
				}

				f->guids.RemoveAll();
				for(int i = 0; ; i++)
				{
					CString val;
					val.Format(_T("mod%04d"), i);
					CString guid = pApp->GetProfileString(key, val, _T(""));
					if(guid.IsEmpty()) break;
					f->guids.AddTail(GUIDFromCString(guid));
				}

				f->iLoadType = (int)pApp->GetProfileInt(key, _T("LoadType"), -1);
				if(f->iLoadType < 0) break;

				f->dwMerit = pApp->GetProfileInt(key, _T("Merit"), MERIT_DO_NOT_USE+1);

				filters.AddTail(f);
			}
		}

		fIntRealMedia = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_INTREALMEDIA), 0);
		//fRealMediaRenderless = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_REALMEDIARENDERLESS), 0);
		//iQuickTimeRenderer = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_QUICKTIMERENDERER), 2);
		RealMediaQuickTimeFPS = 25.0;
		*((DWORD*)&RealMediaQuickTimeFPS) = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_REALMEDIAFPS), *((DWORD*)&RealMediaQuickTimeFPS));

		m_pnspresets.RemoveAll();
		for(int i = 0; i < (ID_PANNSCAN_PRESETS_END - ID_PANNSCAN_PRESETS_START); i++)
		{
			CString str;
			str.Format(_T("Preset%d"), i);
			str = pApp->GetProfileString(ResStr(IDS_R_SETTINGS) + _T("\\") + ResStr(IDS_RS_PNSPRESETS), str, _T(""));
			if(str.IsEmpty()) break;
			m_pnspresets.Add(str);
		}
		if(m_pnspresets.IsEmpty())
		{
			double _4p3 = 4.0/3.0;
			double _16p9 = 16.0/9.0;
			double _185p1 = 1.85/1.0;
			double _235p1 = 2.35/1.0;

			CString str;
			str.Format(_T("16:9,%.3f,%.3f,%.3f,%.3f"), 0.5, 0.5, _4p3/_4p3, _16p9/_4p3);
			m_pnspresets.Add(str);
			str.Format(ResStr(IDS_PANSCAN_PERSET_STRING_FORMAT_WIDE), 0.5, 0.5, _16p9/_4p3, _16p9/_4p3);
			m_pnspresets.Add(str);
			str.Format(_T("2.35:1,%.3f,%.3f,%.3f,%.3f"), 0.5, 0.5, _235p1/_4p3, _235p1/_4p3);
			m_pnspresets.Add(str);
		}

		for(int i = 0; i < wmcmds.GetCount(); i++)
		{
			CString str;
			str.Format(_T("CommandMod%d"), i);
			str = pApp->GetProfileString(ResStr(IDS_R_COMMANDS), str, _T(""));
			if(str.IsEmpty()) break;
			int cmd, fVirt, key, repcnt, mouse, appcmd;
			TCHAR buff[128];
			int n;
			int cmdidx = 0;
			if(5 > (n = _stscanf(str, _T("%d %x %x %s %d %d %d %d"), &cmd, &fVirt, &key, buff, &repcnt, &mouse, &appcmd, &cmdidx)))
				break;
			//CString szLog;
			//szLog.Format(_T("got cmd idx %d while reading "),cmdidx);
			//SVP_LogMsg(szLog);
			if(POSITION pos = FindWmcmdsPosofCmdidByIdx(cmd, cmdidx))
			{
				wmcmd& wc = wmcmds.GetAt(pos);
                wc.cmd = cmd;
				wc.fVirt = fVirt;
				wc.key = key;
				if(n >= 6) wc.mouse = (UINT)mouse;
				if(n >= 7) wc.appcmd = (UINT)appcmd;
				wc.rmcmd = CStringA(buff).Trim('\"');
				wc.rmrepcnt = repcnt;
			}
		}

		CAtlArray<ACCEL> pAccel;
		pAccel.SetCount(wmcmds.GetCount());
		POSITION pos = wmcmds.GetHeadPosition();
		for(int i = 0; pos; i++) pAccel[i] = wmcmds.GetNext(pos);
		hAccel = CreateAcceleratorTable(pAccel.GetData(), pAccel.GetCount());

		WinLircAddr = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WINLIRCADDR), _T("127.0.0.1:8765"));
		fWinLirc = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WINLIRC), 0);
		UIceAddr = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_UICEADDR), _T("127.0.0.1:1234"));
		fUIce = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_UICE), 0);

		fDisabeXPToolbars = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_DISABLEXPTOOLBARS), 0);
		fUseWMASFReader = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEWMASFREADER), TRUE);
		nJumpDistS = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_JUMPDISTS), 5000);
		nJumpDistM = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_JUMPDISTM), 30000);
		nJumpDistL = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_JUMPDISTL), 60000);
		fFreeWindowResizing = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_FREEWINDOWRESIZING), TRUE);
		fNotifyMSN = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_NOTIFYMSN), FALSE);
		fNotifyGTSdll = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_NOTIFYGTSDLL), FALSE);

		Formats.UpdateData(false);
		if(iUpgradeReset < 50){
			for(size_t i = 0; i < Formats.GetCount(); i++){
				if( Formats[i].GetEngineType() == RealMedia || Formats[i].GetEngineType() == QuickTime){
					Formats[i].SetEngineType(DirectShow);
				}
			}
			Formats.UpdateData(true);
		}


		SrcFilters = pApp->GetProfileInt(ResStr(IDS_R_INTERNAL_FILTERS), ResStr(IDS_RS_SRCFILTERS), ~0);//^SRC_MATROSKA^SRC_MP4^SRC_MPEG^SRC_OGG
		TraFilters = pApp->GetProfileInt(ResStr(IDS_R_INTERNAL_FILTERS), ResStr(IDS_RS_TRAFILTERS), ~0);//^TRA_MPEG1^TRA_AAC^TRA_AC3^TRA_DTS^TRA_LPCM^TRA_MPEG2^TRA_VORBIS
		DXVAFilters = pApp->GetProfileInt(ResStr(IDS_R_INTERNAL_FILTERS), ResStr(IDS_RS_DXVAFILTERS), ~0);
		FFmpegFilters = pApp->GetProfileInt(ResStr(IDS_R_INTERNAL_FILTERS), ResStr(IDS_RS_FFMPEGFILTERS), ~0);
		bDVXACompat = pApp->GetProfileInt(ResStr(IDS_R_INTERNAL_FILTERS), ResStr(IDS_RS_DXVACOMPAT), 0);
		
		
		int iDefaultExtLogo = 0;
		CString szExtLogoFn = _T("");
		int iDefaultNCS = CS_SEEKBAR|CS_TOOLBAR;
		if(bAeroGlassAvalibility && szOEMTitle.IsEmpty()){

			LPWSTR sWallpaper = new WCHAR[MAX_PATH];
			if( SystemParametersInfo( SPI_GETDESKWALLPAPER,MAX_PATH-1, sWallpaper,	0) ){
				szExtLogoFn = CString(sWallpaper);
				iDefaultExtLogo = 1;
				iDefaultNCS = 0;
			}

		}
		
		

		lAeroTransparent = 0xaf;
		logostretch = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LOGOSTRETCH), 1);
		logofn = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LOGOFILE), szExtLogoFn);
		logoid = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LOGOID), IDF_LOGO7);
		logoext = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LOGOEXT), iDefaultExtLogo);

		nCS = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_CONTROLSTATE), iDefaultNCS) & ~CS_COLORCONTROLBAR; //CS_STATUSBAR

		fHideCDROMsSubMenu = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_HIDECDROMSSUBMENU), 0);		

		priority = HIGH_PRIORITY_CLASS;//pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_PRIORITY), NORMAL_PRIORITY_CLASS);
		::SetPriorityClass(::GetCurrentProcess(), priority);
		launchfullscreen = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_LAUNCHFULLSCREEN), FALSE);

		fEnableWebServer = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ENABLEWEBSERVER), FALSE);
		nWebServerPort = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBSERVERPORT), 13579);
		fWebServerPrintDebugInfo = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBSERVERPRINTDEBUGINFO), FALSE);
		fWebServerUseCompression = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBSERVERUSECOMPRESSION), TRUE);
		fWebServerLocalhostOnly = !!pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBSERVERLOCALHOSTONLY), TRUE);
		WebRoot = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBROOT), _T("*./webroot"));
		WebDefIndex = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBDEFINDEX), _T("index.html;index.php"));
		WebServerCGI = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_WEBSERVERCGI), _T(""));

		iEvrBuffers		= pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_EVR_BUFFERS), 5);

		bAeroGlass		= pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USEAEROGLASS), bAeroGlassAvalibility);//bAeroGlassAvalibility
		bTransControl = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_USETRANSCONTROL), 0);
		//if(!bAeroGlassAvalibility)
		//	bAeroGlass = false;

		bSaveSVPSubWithVideo  = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SAVESVPSUBWITHVIDEO), 0);

		CString MyPictures;

		CRegKey key;
		// grrrrr
		// if(!SHGetSpecialFolderPath(NULL, MyPictures.GetBufferSetLength(MAX_PATH), CSIDL_MYPICTURES, TRUE)) MyPictures.Empty();
		// else MyPictures.ReleaseBuffer();
		if(ERROR_SUCCESS == key.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"), KEY_READ))
		{
			ULONG len = MAX_PATH;
			if(ERROR_SUCCESS == key.QueryStringValue(_T("My Pictures"), MyPictures.GetBuffer(MAX_PATH), &len)) MyPictures.ReleaseBufferSetLength(len);
			else MyPictures.Empty();
		}
		SnapShotPath = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SNAPSHOTPATH), MyPictures);
		SnapShotExt = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SNAPSHOTEXT), _T(".jpg"));

		ThumbRows = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_THUMBROWS), 4);
		ThumbCols = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_THUMBCOLS), 4);
		ThumbWidth = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_THUMBWIDTH), 1024);

		ISDb = pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_ISDB), _T("www.opensubtitles.org/isdb"));

		pApp->WriteProfileInt(ResStr(IDS_R_SETTINGS), _T("LastUsedPage"), 0);

		
		//

		m_shaders.RemoveAll();

		CAtlList<UINT> shader_ids;
		shader_ids.AddTail(IDF_SHADER_SHARPEN);
		shader_ids.AddTail(IDF_SHADER_DENOISE);
		shader_ids.AddTail(IDF_SHADER_LEVELS);
		shader_ids.AddTail(IDF_SHADER_LEVELS2);
		shader_ids.AddTail(IDF_SHADER_LEVELS3);
		//shader_ids.AddTail(IDF_SHADER_BT601_BT701);
		shader_ids.AddTail(IDF_SHADER_YV12CHROMAUP);
		shader_ids.AddTail(IDF_SHADER_DEINTERLACE);
		shader_ids.AddTail(IDF_SHADER_EDGE_SHARPEN);
		shader_ids.AddTail(IDF_SHADER_SHARPEN_COMPLEX);
		shader_ids.AddTail(IDF_SHADER_SHARPEN_COMPLEX3);
		shader_ids.AddTail(IDF_SHADER_BT601_BT709);
		/*
		shader_ids.AddTail(IDF_SHADER_GRAYSCALE);
		shader_ids.AddTail(IDF_SHADER_INVERT);
		shader_ids.AddTail(IDF_SHADER_CONTOUR);
		shader_ids.AddTail(IDF_SHADER_EMBOSS);
		shader_ids.AddTail(IDF_SHADER_LETTERBOX);
		shader_ids.AddTail(IDF_SHADER_NIGHTVISION);
		shader_ids.AddTail(IDF_SHADER_PROCAMP);
		shader_ids.AddTail(IDF_SHADER_SPHERE);
		shader_ids.AddTail(IDF_SHADER_SPOTLIGHT);
		shader_ids.AddTail(IDF_SHADER_WAVE);
		*/
		

		CAtlStringMap<UINT> shaders;
		pos = shader_ids.GetHeadPosition();
		while(pos){
			UINT idf = shader_ids.GetNext(pos);
			shaders[ResStr(idf)] = idf;
		}
		
		int iShader = 0;

		pos = shader_ids.GetHeadPosition();
		while(pos)
		{
			UINT idf = shader_ids.GetNext(pos);
			
			CStringA srcdata;
			if(LoadResource( idf, srcdata, _T("FILE")))
			{
				Shader s;
				s.label = ResStr(idf);
				switch(idf){
					case IDF_SHADER_SHARPEN_COMPLEX3:
						s.target = _T("ps_3_0");
						break;
					default:
						s.target = _T("ps_2_0");
						break;

				}
				
				s.srcdata = CString(srcdata);
				m_shaders.AddTail(s);
			}
		}
/*
		for(; ; iShader++)
		{
			
			CString str;
			str.Format(_T("%d"), iShader);
			str = pApp->GetProfileString(_T("Shaders"), str);

			CAtlList<CString> sl;
			CString label = Explode(str, sl, '|');
			if(label.IsEmpty()) break;
			if(sl.GetCount() < 3) continue;

			Shader s;
			s.label = sl.RemoveHead();
			s.target = sl.RemoveHead();
			s.srcdata = sl.RemoveHead();
			s.srcdata.Replace(_T("\\n"), _T("\n"));
			s.srcdata.Replace(_T("\\t"), _T("\t"));
			UINT iTmp;
			if( !shaders.Lookup( s.label, iTmp )){
				m_shaders.AddTail(s);
				shaders.RemoveKey(s.label);
			}
			
		}

*/
		CString szDefaultShaders = _T("");
		if(useGPUAcel){
			//szDefaultShaders = ResStr(IDF_SHADER_LEVELS);
		}
		strShaderList	= pApp->GetProfileString(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_SHADERLIST), szDefaultShaders);

		// TODO: sort shaders by label

		m_shadercombine = pApp->GetProfileString(_T("Shaders"), _T("Combine"), _T(""));

		if(bUserAeroUI()){
			m_lTransparentToolbarPosOffset = pApp->GetProfileInt(ResStr(IDS_R_SETTINGS), ResStr(IDS_RS_TRANSPARENTTOOLBARPOSOFFSET)+_T("2"), 0);
			m_lTransparentToolbarPosSavedOffset = m_lTransparentToolbarPosOffset;
		}

		if(nCLSwitches & CLSW_HTPCMODE){
			launchfullscreen = 1;
			htpcmode = 1;
		
		}else{
			//launchfullscreen = 0;
			htpcmode = 0;
		}

        if(nCLSwitches & CLSW_STARTFULL){
            startAsFullscreen = 1;
        }else{
            startAsFullscreen = 0;
        }

		fInitialized = true;
	}
}

void CMPlayerCApp::Settings::ParseCommandLine(CAtlList<CString>& cmdln)
{
	nCLSwitches = 0;
	//SVP_LogMsg5(L"cls reset");
	slFiles.RemoveAll();
	slDubs.RemoveAll();
	slSubs.RemoveAll();
	slFilters.RemoveAll();
	rtStart = 0;
	fixedWindowSize.SetSize(0, 0);
	iMonitor = 0;
	bGenUIINIOnExit = false;
	if(launchfullscreen) nCLSwitches |= CLSW_FULLSCREEN;

	POSITION pos = cmdln.GetHeadPosition();
	while(pos)
	{
		CString param = cmdln.GetNext(pos);
		if(param.IsEmpty()) continue;

		if((param[0] == '-' || param[0] == '/') && param.GetLength() > 1)
		{
			CString sw = param.Mid(1).MakeLower();
			if(sw == _T("open")) nCLSwitches |= CLSW_OPEN;
			else if(sw == _T("play")) nCLSwitches |= CLSW_PLAY;
			else if(sw == _T("fullscreen")) nCLSwitches |= CLSW_FULLSCREEN;
            else if(sw == _T("startfull")) nCLSwitches |= CLSW_STARTFULL;
			else if(sw == _T("minimized")) nCLSwitches |= CLSW_MINIMIZED;
			else if(sw == _T("new")) nCLSwitches |= CLSW_NEW;
			else if(sw == _T("help") || sw == _T("h") || sw == _T("?")) nCLSwitches |= CLSW_HELP;
			else if(sw == _T("dub") && pos) slDubs.AddTail(cmdln.GetNext(pos));
			else if(sw == _T("sub") && pos) slSubs.AddTail(cmdln.GetNext(pos));
			else if(sw == _T("filter") && pos) slFilters.AddTail(cmdln.GetNext(pos));
			else if(sw == _T("dvd")) nCLSwitches |= CLSW_DVD;
			else if(sw == _T("cd")) nCLSwitches |= CLSW_CD;
			else if(sw == _T("add")) nCLSwitches |= CLSW_ADD;
			else if(sw == _T("cap")) nCLSwitches |= CLSW_CAP;
			else if(sw == _T("regvid")) nCLSwitches |= CLSW_REGEXTVID;
			else if(sw == _T("regaud")) nCLSwitches |= CLSW_REGEXTAUD;
			else if(sw == _T("unregall")) nCLSwitches |= CLSW_UNREGEXT;
			else if(sw == _T("unregvid")) nCLSwitches |= CLSW_UNREGEXT; /* keep for compatibility with old versions */
			else if(sw == _T("unregaud")) nCLSwitches |= CLSW_UNREGEXT; /* keep for compatibility with old versions */
			else if(sw == _T("start") && pos) {rtStart = 10000i64*_tcstol(cmdln.GetNext(pos), NULL, 10); nCLSwitches |= CLSW_STARTVALID;}
			else if(sw == _T("startpos") && pos) {/* TODO: mm:ss. */;}
			else if(sw == _T("nofocus")) nCLSwitches |= CLSW_NOFOCUS;
			else if(sw == _T("close")) nCLSwitches |= CLSW_CLOSE;
			else if(sw == _T("standby")) nCLSwitches |= CLSW_STANDBY;
			else if(sw == _T("hibernate")) nCLSwitches |= CLSW_HIBERNATE;
			else if(sw == _T("shutdown")) nCLSwitches |= CLSW_SHUTDOWN;
			else if(sw == _T("fromdmp")) { nCLSwitches |= CLSW_STARTFROMDMP; /*SVP_LogMsg5(L"dmpfrom %x", nCLSwitches);*/}
			else if(sw == _T("htpc")) { nCLSwitches |= CLSW_HTPCMODE|CLSW_FULLSCREEN; }
			else if(sw == _T("logoff")) nCLSwitches |= CLSW_LOGOFF;
			else if(sw == _T("genui")) {nCLSwitches |= CLSW_GENUIINI;bGenUIINIOnExit = true; }
			else if(sw == _T("adminoption")) { nCLSwitches |= CLSW_ADMINOPTION; iAdminOption = _ttoi (cmdln.GetNext(pos)); }
			else if(sw == _T("fixedsize") && pos)
			{
				CAtlList<CString> sl;
				Explode(cmdln.GetNext(pos), sl, ',', 2);
				if(sl.GetCount() == 2)
				{
					fixedWindowSize.SetSize(_ttol(sl.GetHead()), _ttol(sl.GetTail()));
					if(fixedWindowSize.cx > 0 && fixedWindowSize.cy > 0)
						nCLSwitches |= CLSW_FIXEDSIZE;
				}
			}
			else if(sw == _T("monitor") && pos) {iMonitor = _tcstol(cmdln.GetNext(pos), NULL, 10); nCLSwitches |= CLSW_MONITOR;}
			else nCLSwitches |= CLSW_HELP|CLSW_UNRECOGNIZEDSWITCH;
		}
		else
		{
			slFiles.AddTail(param);
		}
	}
	//SVP_LogMsg5(L"cls end %x", nCLSwitches);
}

// Retrieve an integer value from INI file or registry.
UINT  CMPlayerCApp::GetProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault)
{
	if(sqlite_setting){
		return sqlite_setting->GetProfileInt( lpszSection,  lpszEntry,  nDefault);
	}else{
		return __super::GetProfileInt( lpszSection,  lpszEntry,  nDefault);
	}
}

// Sets an integer value to INI file or registry.
BOOL  CMPlayerCApp::WriteProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue)
{
	if(sqlite_setting){
		return sqlite_setting->WriteProfileInt( lpszSection,  lpszEntry,  nValue);
	}else{
		SVP_LogMsg6("dwqdwq");
		return __super::WriteProfileInt( lpszSection,  lpszEntry,  nValue);
	}
}

// Retrieve a string value from INI file or registry.
CString  CMPlayerCApp::GetProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry,
						 LPCTSTR lpszDefault )
{
	if(sqlite_setting){
		return sqlite_setting->GetProfileString( lpszSection,  lpszEntry,
		 lpszDefault );
	}else{
		return __super::GetProfileString( lpszSection,  lpszEntry,
			lpszDefault );
	}
}

// Sets a string value to INI file or registry.
BOOL  CMPlayerCApp::WriteProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry,
						LPCTSTR lpszValue)
{
	if(sqlite_setting){
	return sqlite_setting->WriteProfileString( lpszSection,  lpszEntry,
		 lpszValue);
	}else{
		return __super::WriteProfileString( lpszSection,  lpszEntry,
			lpszValue);
	}
}

// Retrieve an arbitrary binary value from INI file or registry.
BOOL  CMPlayerCApp::GetProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry,
					  LPBYTE* ppData, UINT* pBytes)
{
	if(sqlite_setting){
	return sqlite_setting->GetProfileBinary( lpszSection,  lpszEntry,
		 ppData,  pBytes);
	}else{
		return __super::GetProfileBinary( lpszSection,  lpszEntry,
			ppData,  pBytes);
	}
}

// Sets an arbitrary binary value to INI file or registry.
BOOL  CMPlayerCApp::WriteProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry,
						LPBYTE pData, UINT nBytes)
{
	if(sqlite_setting){
		return sqlite_setting->WriteProfileBinary( lpszSection,  lpszEntry,
		 pData,  nBytes);
	}else{
		return __super::WriteProfileBinary( lpszSection,  lpszEntry,
			pData,  nBytes);
	}
}

COLORREF CMPlayerCApp::Settings::GetColorFromTheme(CString clrName, COLORREF clrDefault){
	if( colorsTheme.IsEmpty() ){
		//check skins file
		CSVPToolBox svpTool;
		CPath skinsColorPath( svpTool.GetPlayerPath(_T("skins")));
		skinsColorPath.AddBackslash();
		skinsColorPath.Append(_T("ui.ini"));
		CWebTextFile f;
		CString str;
		if(f.Open(skinsColorPath) ){
			

			if(f.GetEncoding() == CTextFile::ASCII) 
				f.SetEncoding(CTextFile::ANSI);

			int idxCurrent = -1;

			while(f.ReadString(str))
			{
				str.Trim();
				int pos_of_comm = str.Find(_T("//"));
				if(pos_of_comm >= 0){
					if(pos_of_comm <=3){
						continue;
					}
					str = str.Left(pos_of_comm);
				}
				CAtlList<CString> sl;
				Explode(str, sl, '=', 2);
				if(sl.GetCount() != 2) continue;

				CString key = sl.RemoveHead();
				key.Trim(_T("#\t\r\n "));
				CString cvalue = sl.RemoveHead();
				cvalue.Trim();
				bool isHex = false;
				if(cvalue.Find('#') == 0){
					isHex = true;
				}
				if(cvalue.GetLength() >= 6){
					isHex = true;
				}
				cvalue.Trim(_T("#\t\r\n "));
				if(isHex)
					colorsTheme[key] = svpTool._httoi( cvalue );	
				else
					colorsTheme[key] = _ttoi( cvalue );	
				
				SVP_LogMsg5(_T("theme %s %d") , key, cvalue);
			}
		}
		colorsTheme[_T("NULL")] = 0x000000;
	}
	COLORREF cTmp;
	if( colorsTheme.Lookup(clrName , cTmp) ){
		return cTmp;
	}else{
		colorsTheme[clrName] = clrDefault;
		return clrDefault;
	}
}
void CMPlayerCApp::Settings::GetFav(favtype ft, CAtlList<CString>& sl, BOOL bRecent)
{
	sl.RemoveAll();

	//SVP_LogMsg5(L"GetFav start");
	CString root;

	switch(ft)
	{
	case FAV_FILE: root = ResStr(IDS_R_FAVFILES); break;
	case FAV_DVD: root = ResStr(IDS_R_FAVDVDS); break;
	case FAV_DEVICE: root = ResStr(IDS_R_FAVDEVICES); break;
	default: return;
	}

	if (bRecent){ root += _T("_Recent"); }

	for(int i = 0; ; i++)
	{
		CString s;
		s.Format(_T("Name%d"), i);
		s = AfxGetMyApp()->GetProfileString(root, s, NULL);
		if(s.IsEmpty()) break;
		sl.AddTail(s);
	}
}

void CMPlayerCApp::Settings::SetFav(favtype ft, CAtlList<CString>& sl, BOOL bRecent)
{
	CString root;

	switch(ft)
	{
	case FAV_FILE: root = ResStr(IDS_R_FAVFILES); break;
	case FAV_DVD: root = ResStr(IDS_R_FAVDVDS); break;
	case FAV_DEVICE: root = ResStr(IDS_R_FAVDEVICES); break;
	default: return;
	}

	if (bRecent){ root += _T("_Recent"); }
	AfxGetMyApp()->WriteProfileString(root, NULL, NULL);

	int i = 0;
	POSITION pos = sl.GetHeadPosition();
	while(pos)
	{
		CString s;
		s.Format(_T("Name%d"), i++);
		AfxGetMyApp()->WriteProfileString(root, s, sl.GetNext(pos));
	}
	for(int j = 0 ; j < 10;j++){
		CString s;
		s.Format(_T("Name%d"), i++);
		if( AfxGetMyApp()->WriteProfileString(root, s, NULL) == 0){
			break;
		}
	}
}
void CMPlayerCApp::Settings::DelFavByFn(favtype ft, BOOL bRecent, CString szMatch){
	CAtlList<CString> sl;
	GetFav(ft, sl, bRecent);
	//SVP_LogMsg5(L" DelFavByFn  ");
	if(bRecent){
		CMD5Checksum cmd5;
		CStringA szMD5data(szMatch);
		CString szMatchmd5 = cmd5.GetMD5((BYTE*)szMD5data.GetBuffer() , szMD5data.GetLength() );
		POSITION pos = sl.GetHeadPosition();
		while(pos){
			if( sl.GetAt(pos).Find(szMatchmd5) >= 0 ){
	//			SVP_LogMsg5(L" DelFavByFn RemoveAt ");
				sl.RemoveAt(pos);
				break;
			}
			sl.GetNext(pos);
		}
		if(sl.GetCount() > 20){
			sl.RemoveHead();
		}
	}else{
		//if(sl.Find(s)) return;
	}
	
	SetFav(ft, sl , bRecent);
}
void CMPlayerCApp::Settings::AddFav(favtype ft, CString s, BOOL bRecent, CString szMatch)
{
	SVP_LogMsg5(L"bRecent Start %s", s);
	//if(AfxGetMyApp()->sqlite_local_record){
		//CString szSQL;
		//szSQL.Format(_T("REPLACE INTO favrec  ( favtype, favpath, favtime, addtime, favrecent ) VALUES (\"%d\" , \"%s\" ,\"%d\",\"%d\" )"), ft, s ,time(NULL),bRecent);
	//}
	CAtlList<CString> sl;
	GetFav(ft, sl, bRecent);
	if(bRecent){
		CMD5Checksum cmd5;
		CStringA szMD5data(szMatch);
		CString szMatchmd5 = cmd5.GetMD5((BYTE*)szMD5data.GetBuffer() , szMD5data.GetLength() );
		szMD5data.ReleaseBuffer();
		POSITION pos = sl.GetHeadPosition();
		while(pos){
			if( sl.GetAt(pos).Find(szMatchmd5) >= 0 ){
				sl.RemoveAt(pos);
				break;
			}
			sl.GetNext(pos);
		}
		if(sl.GetCount() > 20){
			sl.RemoveHead();
		}
		s.Replace( szMatch , szMatchmd5);
	}else{
		if(sl.Find(s)) return;
	}
	sl.AddTail(s);
	SetFav(ft, sl , bRecent);

	SVP_LogMsg5(L"bRecent Add Fav Done %s " , s);
}

// CMPlayerCApp::Settings::CRecentFileAndURLList

CMPlayerCApp::Settings::CRecentFileAndURLList::CRecentFileAndURLList(UINT nStart, LPCTSTR lpszSection,
															LPCTSTR lpszEntryFormat, int nSize,	
															int nMaxDispLen) 
	: CRecentFileList(nStart, lpszSection, lpszEntryFormat, nSize, nMaxDispLen)	
{
}

//#include <afximpl.h>
extern BOOL AFXAPI AfxFullPath(LPTSTR lpszPathOut, LPCTSTR lpszFileIn);
extern BOOL AFXAPI AfxComparePath(LPCTSTR lpszPath1, LPCTSTR lpszPath2);

void CMPlayerCApp::Settings::CRecentFileAndURLList::Add(LPCTSTR lpszPathName)
{
	ASSERT(m_arrNames != NULL);
	ASSERT(lpszPathName != NULL);
	ASSERT(AfxIsValidString(lpszPathName));

	if(CString(lpszPathName).MakeLower().Find(_T("@device:")) >= 0)
		return;

	bool fURL = (CString(lpszPathName).Find(_T("://")) >= 0);

	// fully qualify the path name
	TCHAR szTemp[1024];
	if(fURL) _tcscpy_s(szTemp, countof(szTemp), lpszPathName);
	else AfxFullPath(szTemp, lpszPathName);

	// update the MRU list, if an existing MRU string matches file name
	int iMRU;
	for (iMRU = 0; iMRU < m_nSize-1; iMRU++)
	{
		if((fURL && !_tcscmp(m_arrNames[iMRU], szTemp))
		|| AfxComparePath(m_arrNames[iMRU], szTemp))
			break;      // iMRU will point to matching entry
	}
	// move MRU strings before this one down
	for (; iMRU > 0; iMRU--)
	{
		ASSERT(iMRU > 0);
		ASSERT(iMRU < m_nSize);
		m_arrNames[iMRU] = m_arrNames[iMRU-1];
	}
	// place this one at the beginning
	m_arrNames[0] = szTemp;
}


void CMPlayerCApp::OnHelpShowcommandlineswitches()
{
	ShowCmdlnSwitches();
}
UINT CMPlayerCApp::GetBottomSubOffset(){
	CMainFrame* pFrame = (CMainFrame*)m_pMainWnd;
	return pFrame->GetBottomSubOffset();
}
//

void GetCurDispMode(dispmode& dm)
{
	if(HDC hDC = ::GetDC(0))
	{
		dm.fValid = true;
		dm.size = CSize(GetDeviceCaps(hDC, HORZRES), GetDeviceCaps(hDC, VERTRES));
		dm.bpp = GetDeviceCaps(hDC, BITSPIXEL);
		dm.freq = GetDeviceCaps(hDC, VREFRESH);
		::ReleaseDC(0, hDC);
	}
}

bool GetDispMode(int i, dispmode& dm)
{
	DEVMODE devmode;
	devmode.dmSize = sizeof(DEVMODE);
	if(!EnumDisplaySettings(0, i, &devmode))
		return(false);

	dm.fValid = true;
	dm.size = CSize(devmode.dmPelsWidth, devmode.dmPelsHeight);
	dm.bpp = devmode.dmBitsPerPel;
	dm.freq = devmode.dmDisplayFrequency;

	return(true);
}

void SetDispMode(dispmode& dm)
{
	if(!dm.fValid) return;

	DEVMODE dmScreenSettings;
	memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
	dmScreenSettings.dmSize = sizeof(dmScreenSettings);
	dmScreenSettings.dmPelsWidth = dm.size.cx;
	dmScreenSettings.dmPelsHeight = dm.size.cy;
	dmScreenSettings.dmBitsPerPel = dm.bpp;
	dmScreenSettings.dmDisplayFrequency = dm.freq;
	dmScreenSettings.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
	ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);
}

#include <afxsock.h>
#include <atlsync.h>
#include <atlutil.h> // put this before the first detours macro above to see an ICE with vc71 :)
#include "..\..\..\lib\ATL Server\include\atlrx.h" // http://www.codeplex.com/AtlServer/
#include "afxwin.h"

typedef CAtlRegExp<CAtlRECharTraits> CAtlRegExpT;
typedef CAtlREMatchContext<CAtlRECharTraits> CAtlREMatchContextT;

bool FindRedir(CUrl& src, CString ct, CString& body, CAtlList<CString>& urls, CAutoPtrList<CAtlRegExpT>& res)
{
	POSITION pos = res.GetHeadPosition();
	while(pos)
	{
		CAtlRegExpT* re = res.GetNext(pos);

		CAtlREMatchContextT mc;
		const CAtlREMatchContextT::RECHAR* s = (LPCTSTR)body;
		const CAtlREMatchContextT::RECHAR* e = NULL;
		for(; s && re->Match(s, &mc, &e); s = e)
		{
			const CAtlREMatchContextT::RECHAR* szStart = 0;
			const CAtlREMatchContextT::RECHAR* szEnd = 0;
			mc.GetMatch(0, &szStart, &szEnd);

			CString url;
			url.Format(_T("%.*s"), szEnd - szStart, szStart);
			url.Trim();

			if(url.CompareNoCase(_T("asf path")) == 0) continue;

			CUrl dst;
			dst.CrackUrl(CString(url));
			if(_tcsicmp(src.GetSchemeName(), dst.GetSchemeName())
			|| _tcsicmp(src.GetHostName(), dst.GetHostName())
			|| _tcsicmp(src.GetUrlPath(), dst.GetUrlPath()))
			{
				urls.AddTail(url);
			}
			else
			{
				// recursive
				urls.RemoveAll();
				break;
			}
		}
	}

	return urls.GetCount() > 0;
}

bool FindRedir(CString& fn, CString ct, CAtlList<CString>& fns, CAutoPtrList<CAtlRegExpT>& res)
{
	CString body;

	CTextFile f(CTextFile::ANSI);
	if(f.Open(fn)) for(CString tmp; f.ReadString(tmp); body += tmp + '\n');

	CString dir = fn.Left(max(fn.ReverseFind('/'), fn.ReverseFind('\\'))+1); // "ReverseFindOneOf"

	POSITION pos = res.GetHeadPosition();
	while(pos)
	{
		CAtlRegExpT* re = res.GetNext(pos);

		CAtlREMatchContextT mc;
		const CAtlREMatchContextT::RECHAR* s = (LPCTSTR)body;
		const CAtlREMatchContextT::RECHAR* e = NULL;
		for(; s && re->Match(s, &mc, &e); s = e)
		{
			const CAtlREMatchContextT::RECHAR* szStart = 0;
			const CAtlREMatchContextT::RECHAR* szEnd = 0;
			mc.GetMatch(0, &szStart, &szEnd);

			CString fn2;
			fn2.Format(_T("%.*s"), szEnd - szStart, szStart);
			fn2.Trim();

			if(!fn2.CompareNoCase(_T("asf path"))) continue;
			if(fn2.Find(_T("EXTM3U")) == 0 || fn2.Find(_T("#EXTINF")) == 0) continue;

			if(fn2.Find(_T(":")) < 0 && fn2.Find(_T("\\\\")) != 0 && fn2.Find(_T("//")) != 0)
			{
				CPath p;
				p.Combine(dir, fn2);
				fn2 = (LPCTSTR)p;
			}

			if(!fn2.CompareNoCase(fn))
				continue;

			fns.AddTail(fn2);
		}
	}

	return fns.GetCount() > 0;
}
void GetSystemFontWithScale(CFont* pFont, double dDefaultSize, int iWeight, CString szTryFontName){
	HDC hdc = ::GetDC(NULL);
	double scale = 1.0*GetDeviceCaps(hdc, LOGPIXELSY) / 96.0;
	::ReleaseDC(0, hdc);

	pFont->m_hObject = NULL;

    if(!szTryFontName.IsEmpty()){
        pFont->CreateFont(int(dDefaultSize * scale), 0, 0, 0, iWeight, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, 
            szTryFontName);
    }
	if(!pFont->m_hObject && !(::GetVersion()&0x80000000))
		pFont->CreateFont(int(dDefaultSize * scale), 0, 0, 0, iWeight, 0, 0, 0, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, 
		_T("Microsoft Sans Serif"));
	if(!pFont->m_hObject)
		pFont->CreateFont(int(dDefaultSize * scale), 0, 0, 0, iWeight, 0, 0, 0, DEFAULT_CHARSET, 
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, 
		_T("MS Sans Serif"));

	
}
CString GetContentType(CString fn, CAtlList<CString>* redir)
{
	CUrl url;
	CString ct, body;

	if(fn.Find(_T("://")) >= 0)
	{
		url.CrackUrl(fn);

		if(_tcsicmp(url.GetSchemeName(), _T("pnm")) == 0)
			return "audio/x-pn-realaudio";

		if(_tcsicmp(url.GetSchemeName(), _T("mms")) == 0)
			return "video/x-ms-asf";
		
		if(_tcsicmp(url.GetSchemeName(), _T("rtsp")) == 0)
			return "application/vnd.rn-realvideo";

		if(_tcsicmp(url.GetSchemeName(), _T("http")) != 0)
			return "";

		DWORD ProxyEnable = 0;
		CString ProxyServer;
		DWORD ProxyPort = 0;

		ULONG len = 256+1;
		CRegKey key;
		if(ERROR_SUCCESS == key.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"), KEY_READ)
		&& ERROR_SUCCESS == key.QueryDWORDValue(_T("ProxyEnable"), ProxyEnable) && ProxyEnable
		&& ERROR_SUCCESS == key.QueryStringValue(_T("ProxyServer"), ProxyServer.GetBufferSetLength(256), &len))
		{
			ProxyServer.ReleaseBufferSetLength(len);

			CAtlList<CString> sl;
			ProxyServer = Explode(ProxyServer, sl, ';');
			if(sl.GetCount() > 1)
			{
				POSITION pos = sl.GetHeadPosition();
				while(pos)
				{
					CAtlList<CString> sl2;
					if(!Explode(sl.GetNext(pos), sl2, '=', 2).CompareNoCase(_T("http"))
					&& sl2.GetCount() == 2)
					{
						ProxyServer = sl2.GetTail();
						break;
					}
				}
			}

			ProxyServer = Explode(ProxyServer, sl, ':');
			if(sl.GetCount() > 1) ProxyPort = _tcstol(sl.GetTail(), NULL, 10);
		}

		CSocket s;
		s.Create();
		CString szHostName = url.GetHostName();
		UINT iPort = url.GetPortNumber();
		if(s.Connect(
			ProxyEnable ? ProxyServer  : szHostName, 
			ProxyEnable ? ProxyPort : iPort))
		{
			CStringA host = CStringA(url.GetHostName());
			CStringA path = CStringA(url.GetUrlPath()) + CStringA(url.GetExtraInfo());

			if(ProxyEnable) path = "http://" + host + path;

			CStringA hdr;
			hdr.Format(
				"GET %s HTTP/1.0\r\n"
				"User-Agent: Media Player Classic\r\n"
				"Host: %s\r\n"
				"Accept: */*\r\n"
				"\r\n", path, host);

// MessageBox(NULL, CString(hdr), _T("Sending..."), MB_OK);

			if(s.Send((LPCSTR)hdr, hdr.GetLength()) < hdr.GetLength()) return "";

			hdr.Empty();
			while(1)
			{
				CStringA str;
				str.ReleaseBuffer(s.Receive(str.GetBuffer(256), 256)); // SOCKET_ERROR == -1, also suitable for ReleaseBuffer
				if(str.IsEmpty()) break;
				hdr += str;
				int hdrend = hdr.Find("\r\n\r\n");
				if(hdrend >= 0) {body = hdr.Mid(hdrend+4); hdr = hdr.Left(hdrend); break;}
			}

// MessageBox(NULL, CString(hdr), _T("Received..."), MB_OK);

			CAtlList<CStringA> sl;
			Explode(hdr, sl, '\n');
			POSITION pos = sl.GetHeadPosition();
			while(pos)
			{
				CStringA& hdrline = sl.GetNext(pos);
				CAtlList<CStringA> sl2;
				Explode(hdrline, sl2, ':', 2);
				CStringA field = sl2.RemoveHead().MakeLower();
				if(field == "location" && !sl2.IsEmpty())
					return GetContentType(CString(sl2.GetHead()), redir);
				if(field == "content-type" && !sl2.IsEmpty())
					ct = sl2.GetHead();
			}

			while(body.GetLength() < 256)
			{
				CStringA str;
				str.ReleaseBuffer(s.Receive(str.GetBuffer(256), 256)); // SOCKET_ERROR == -1, also suitable for ReleaseBuffer
				if(str.IsEmpty()) break;
				body += str;
			}

			if(body.GetLength() >= 8)
			{
				CStringA str = TToA(body);
				if(!strncmp((LPCSTR)str, ".ra", 3))
					return "audio/x-pn-realaudio";
				if(!strncmp((LPCSTR)str, ".RMF", 4))
					return "audio/x-pn-realaudio";
				if(*(DWORD*)(LPCSTR)str == 0x75b22630)
					return "video/x-ms-wmv";
				if(!strncmp((LPCSTR)str+4, "moov", 4))
					return "video/quicktime";
			}

			if(redir && (ct == _T("audio/x-scpls") || ct == _T("audio/x-mpegurl")))
			{
				while(body.GetLength() < 4*1024) // should be enough for a playlist...
				{
					CStringA str;
					str.ReleaseBuffer(s.Receive(str.GetBuffer(256), 256)); // SOCKET_ERROR == -1, also suitable for ReleaseBuffer
					if(str.IsEmpty()) break;
					body += str;
				}
			}
		}
	}
	else if(!fn.IsEmpty())
	{
		CPath p(fn);
		CString ext = p.GetExtension().MakeLower();
		if(ext == _T(".asx")) ct = _T("video/x-ms-asf");
		else if(ext == _T(".pls")) ct = _T("audio/x-scpls");
		else if(ext == _T(".m3u")) ct = _T("audio/x-mpegurl");
		else if(ext == _T(".qtl")) ct = _T("application/x-quicktimeplayer");
		else if(ext == _T(".mpcpl")) ct = _T("application/x-mpc-playlist");
		else if(ext == _T(".cue")) ct = _T("application/x-cue-playlist");

		if(FILE* f = _tfopen(fn, _T("rb")))
		{
			CStringA str;
			str.ReleaseBufferSetLength(fread(str.GetBuffer(10240), 1, 10240, f));
			body = AToT(str);
			fclose(f);
		}
	}

	if(body.GetLength() >= 4) // here only those which cannot be opened through dshow
	{
		CStringA str = TToA(body);
		if(!strncmp((LPCSTR)str, ".ra", 3))
			return "audio/x-pn-realaudio";
		if(!strncmp((LPCSTR)str, "FWS", 3))
			return "application/x-shockwave-flash";
	}

	if(redir && !ct.IsEmpty())
	{
		CAutoPtrList<CAtlRegExpT> res;
		CAutoPtr<CAtlRegExpT> re;

		if(ct == _T("video/x-ms-asf"))
		{
			// ...://..."/>
			re.Attach(new CAtlRegExpT());
			if(re && REPARSE_ERROR_OK == re->Parse(_T("{[a-zA-Z]+://[^\n\">]*}"), FALSE))
				res.AddTail(re);
			// Ref#n= ...://...\n
			re.Attach(new CAtlRegExpT());
			if(re && REPARSE_ERROR_OK == re->Parse(_T("Ref\\z\\b*=\\b*[\"]*{([a-zA-Z]+://[^\n\"]+}"), FALSE))
				res.AddTail(re);
		}
		else if(ct == _T("audio/x-scpls"))
		{
			// File1=...\n
			re.Attach(new CAtlRegExp<>());
			if(re && REPARSE_ERROR_OK == re->Parse(_T("file\\z\\b*=\\b*[\"]*{[^\n\"]+}"), FALSE))
				res.AddTail(re);
		}
		else if(ct == _T("audio/x-mpegurl"))
		{
			// #comment
			// ...
			re.Attach(new CAtlRegExp<>());
			if(re && REPARSE_ERROR_OK == re->Parse(_T("{[^#][^\n]+}"), FALSE))
				res.AddTail(re);
		}
		else if(ct == _T("audio/x-pn-realaudio"))
		{
			// rtsp://...
			re.Attach(new CAtlRegExp<>());
			if(re && REPARSE_ERROR_OK == re->Parse(_T("{rtsp://[^\n]+}"), FALSE))
				res.AddTail(re);
		}

		if(!body.IsEmpty())
		{
			if(fn.Find(_T("://")) >= 0) FindRedir(url, ct, body, *redir, res);
			else FindRedir(fn, ct, *redir, res);
		}
	}



	return ct;
}
void CMPlayerCApp::GainAdminPrivileges(UINT idd, BOOL bWait){
	CString			strCmd;
	CString			strApp;

	strCmd.Format (_T("/adminoption %d"), idd);

	CSVPToolBox svpTool;
	strApp = svpTool.GetPlayerPath();


	SHELLEXECUTEINFO execinfo;
	memset(&execinfo, 0, sizeof(execinfo));
	execinfo.lpFile			= strApp;
	execinfo.cbSize			= sizeof(execinfo);
	execinfo.lpVerb			= _T("runas");
	execinfo.fMask			= SEE_MASK_NOCLOSEPROCESS;
	execinfo.nShow			= SW_SHOWDEFAULT;
	execinfo.lpParameters	= strCmd;

	ShellExecuteEx(&execinfo);

	if(bWait)
		WaitForSingleObject(execinfo.hProcess, INFINITE);
}
BOOL CALLBACK DS_GetAudioDeviceGUID(LPGUID lpGuid,
								  LPCTSTR lpstrDescription,
								  LPCTSTR lpstrModule,
								  LPGUID* pGuid) {
								CString m_DisplayName = AfxGetAppSettings().AudioRendererDisplayName ;

								//SVP_LogMsg3("%s vs %s %s" , CStringA(lpstrDescription) , CStringA(m_DisplayName) , CStringA(CStringFromGUID( *lpGuid )) );
								//|| m_DisplayName.Find( CStringFromGUID( *lpGuid ) ) >= 0
								if(m_DisplayName.Find( lpstrDescription) >=0 ){
								//	SVP_LogMsg3("Got match audio");
									if (lpGuid == NULL) {
								//		SVP_LogMsg3("Got match audio NULL");
										memset(*(pGuid), 0, sizeof(GUID));
									} else {
										memcpy(*(pGuid), lpGuid, sizeof(GUID));
									}

								}
								
									  /* continue enumeration */
									  return TRUE;
}

int  CMPlayerCApp::GetNumberOfSpeakers(LPCGUID lpcGUID, HWND hWnd){
	AppSettings& s = AfxGetAppSettings();
	if(s.bNotAutoCheckSpeaker > 1 && s.fCustomSpeakers){
		return s.bNotAutoCheckSpeaker;
	}
	CComPtr<IDirectSound> pDS;
	DWORD spc = 0, defchnum = 2;
	CString m_DisplayName = s.AudioRendererDisplayName ;
	GUID GUID_Audio;
	
	if(!lpcGUID && !m_DisplayName.IsEmpty()){
		if(!lpcGUID)
			lpcGUID = &GUID_Audio;
		//SVP_LogMsg3("DirectSoundEnumerate");
		DirectSoundEnumerate((LPDSENUMCALLBACK) DS_GetAudioDeviceGUID, &lpcGUID);

	}
	

	if(SUCCEEDED(DirectSoundCreate(lpcGUID, &pDS, NULL)))// && SUCCEEDED(pDS->SetCooperativeLevel(AfxGetMainWnd()->m_hWnd, DSSCL_NORMAL) )
	{
		//AfxMessageBox( CStringFromGUID(m_clsid ));
		if(SUCCEEDED(pDS->GetSpeakerConfig(&spc)))
		{
			spc = DSSPEAKER_CONFIG(spc);
			switch(spc)
			{
			case DSSPEAKER_DIRECTOUT: defchnum = 6; break;
			case DSSPEAKER_HEADPHONE: defchnum = 2; break;
			case DSSPEAKER_MONO: defchnum = 1; break;
			case DSSPEAKER_QUAD: defchnum = 4; break;
			
			case DSSPEAKER_STEREO: defchnum = 2; break;
			case DSSPEAKER_SURROUND: defchnum = 3; break;
			case DSSPEAKER_5POINT1: defchnum = 6; break;
			case DSSPEAKER_7POINT1: defchnum = 7; break;
			default: defchnum = spc; break;
			}
			AfxGetAppSettings().szFGMLog.AppendFormat(_T("\r\nGotNumberOfSpeakers %d for %s"), defchnum, m_DisplayName);

		//	SVP_LogMsg3("nGotNumberOfSpeakers %d for %s", defchnum , CStringA(m_DisplayName));

		}
		//SVP_LogMsg3("nGotNumberOfSpeakers %d for %s", defchnum , CStringA(m_DisplayName));
	}else{
		//SVP_LogMsg3("DirectSoundCreate fail");
	}

	return defchnum;
}
bool CMPlayerCApp::HasEVRSupport(){
	if(m_bHasEVRSupport < 0 ){
		m_bHasEVRSupport = (bool)( LoadLibrary (L"dxva2.dll") &&  LoadLibrary (L"evr.dll") );
	}
	return m_bHasEVRSupport > 0;
}
bool CMPlayerCApp::CanUseCUDA(){
	if(m_bCanUseCUDA < 0 ){
		CSVPToolBox svpTool;
		m_bCanUseCUDA = svpTool.CanUseCUDAforCoreAVC() && svpTool.ifFileExist( svpTool.GetPlayerPath(_T("codecs\\CoreAVCDecoder.ax")) );
	}
	return m_bCanUseCUDA > 0;
}

LPCTSTR CMPlayerCApp::GetSatelliteDll(int nLanguage)
{
	switch (nLanguage)
	{
	case 1:		// English
		return _T("lang\\splayer.en.dll");
    case 2:		// English
        return _T("lang\\splayer.cht.dll");
    case 3:		// Russian
		return _T("lang\\splayer.ru.dll");
	}
	return NULL;
}


void CMPlayerCApp::SetLanguage (int nLanguage)
{
	AppSettings&	s = AfxGetAppSettings();
	HMODULE		hMod = NULL;
	LPCTSTR		strSatellite = NULL;
	bool		bNoChange = false;

	CSVPToolBox svpTool;
	CString szLangDefault = svpTool.GetPlayerPath(  _T("lang\\default") );
	CString szLangSeting;
	if(nLanguage < 0)
	{
		BOOL langSeted = false;
		//get default lang setting
		
		if(svpTool.ifFileExist(szLangDefault)){
			szLangSeting = svpTool.fileGetContent(szLangDefault);
			if(!szLangSeting.IsEmpty()){
				nLanguage = _wtoi( szLangSeting );
				if(nLanguage > 0)
					strSatellite = GetSatelliteDll(nLanguage) ;
				langSeted = true;
			}
		}
		if(!langSeted){
				 //LC_ALL
				//SVP_LogMsg5(L"Loc %x" , GetSystemDefaultLCID());
				//SVP_LogMsg5(L"Loc %x" ,GetSystemDefaultLangID());
				switch(GetSystemDefaultLangID()){ //http://www.science.co.il/Language/Locale-Codes.asp?s=codepage
					case 0x0804:
					case 0x1004:
						nLanguage = 0;
						break;
                    case 0x1404:
                    case 0x0c04:
                    case 0x0404: //Chinese 
                        nLanguage = 2;
                        break;
					case 0x0419:
						nLanguage = 3;
						break;
					default:
						nLanguage = 1;
						break;
				}
				strSatellite = GetSatelliteDll(nLanguage) ;
		}
	}else{
		strSatellite = GetSatelliteDll(nLanguage) ;
	}
	
	if (strSatellite)
	{
		hMod = LoadLibrary ( svpTool.GetPlayerPath( strSatellite ) );		
		
	}

	m_hResDll = hMod;
	

	if (!hMod) 
	{
		//AfxMessageBox(L"Load Lang Fail");
		m_hResDll = NULL;
		hMod = AfxGetApp()->m_hInstance;
		s.iLanguage = 0;
	}else{
		s.iLanguage  = nLanguage;
	}
	
	szLangSeting.Format(L"%d" , s.iLanguage );
	svpTool.filePutContent(szLangDefault,szLangSeting );
	AfxSetResourceHandle(hMod);
}
bool CMPlayerCApp::IsWin7(){
	if(m_isVista < 0 )
		IsVista();
	return (m_isVista >= 2);
}
bool CMPlayerCApp::IsVista()
{
	if(m_isVista < 0 ){
		m_isVista = 0;
		OSVERSIONINFO osver;

		osver.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );

		if (	::GetVersionEx( &osver ) && 
			osver.dwPlatformId == VER_PLATFORM_WIN32_NT ){
			//SVP_LogMsg5(L"OS %d %d", osver.dwMajorVersion, osver.dwMinorVersion);
			if(osver.dwMajorVersion >= 6 ) {
				m_isVista = 1;
				if(osver.dwMinorVersion >= 1){
					m_isVista = 2;
				}
			}
			if(osver.dwMajorVersion >= 7 ) 
				m_isVista = 2;
		}

	}
	
	return m_isVista>0;
}

bool CMPlayerCApp::IsVSFilterInstalled()
{
	bool result = false;
	CRegKey key;
	if(ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, _T("CLSID\\{083863F1-70DE-11d0-BD40-00A0C911CE86}\\Instance\\{9852A670-F845-491B-9BE6-EBD841B8A613}"), KEY_READ)) {
		result = true;
	}
	
	return result;
}
/*
if (m_hD3DX9Dll == NULL)
{
int min_ver = D3DX_SDK_VERSION;
int max_ver = D3DX_SDK_VERSION;

m_nDXSdkRelease = 0;

if(D3DX_SDK_VERSION >= 42) {
// August 2009 SDK (v42) is not compatible with older versions
min_ver = 42;			
} else {
if(D3DX_SDK_VERSION > 33) {
// versions between 34 and 41 have no known compatibility issues
min_ver = 34;
}	else {		
// The minimum version that supports the functionality required by MPC is 24
min_ver = 24;

if(D3DX_SDK_VERSION == 33) {
// The April 2007 SDK (v33) should not be used (crash sometimes during shader compilation)
max_ver = 32;		
}				
}
}

// load latest compatible version of the DLL that is available
for (int i=max_ver; i>=min_ver; i--)
{
m_strD3DX9Version.Format(_T("d3dx9_%d.dll"), i);
m_hD3DX9Dll = LoadLibrary (m_strD3DX9Version);
if (m_hD3DX9Dll) 
{
m_nDXSdkRelease = i;
break;
}
}

*/

void  CMPlayerCApp::ClearRecentFileListForWin7()
{


    IApplicationDestinations* pIAD;
    HRESULT hr  = ::CoCreateInstance(CLSID_ApplicationDestinations, NULL, CLSCTX_INPROC_SERVER,
        IID_IApplicationDestinations, reinterpret_cast<void**>(&pIAD) );

    if (SUCCEEDED(hr))
    {
        pIAD->RemoveAllDestinations();
    }

}

#define D3D_MAX_SDKVERSION 45
HINSTANCE CMPlayerCApp::GetD3X9Dll()
{
	if (m_hD3DX9Dll == NULL)
	{
		m_nDXSdkRelease = 0;
		// Try to load latest DX9 available
		for (int i= D3D_MAX_SDKVERSION ; i>23; i--)
		{
			if (i != 33)	// Prevent using DXSDK April 2007 (crash sometimes during shader compilation)
			{
				m_strD3DX9Version.Format(_T("d3dx9_%d.dll"), i);
				m_hD3DX9Dll = LoadLibrary (m_strD3DX9Version);
				if (m_hD3DX9Dll) 
				{
					m_nDXSdkRelease = i;
					break;
				}
			}
		}
	}

	return m_hD3DX9Dll;
}

LONGLONG CMPlayerCApp::GetPerfCounter()
{
	LONGLONG		i64Ticks100ns;
	if (m_PerfFrequency != 0)
	{
		QueryPerformanceCounter ((LARGE_INTEGER*)&i64Ticks100ns);
		i64Ticks100ns	= LONGLONG((double(i64Ticks100ns) * 10000000) / double(m_PerfFrequency) + 0.5);

		return i64Ticks100ns;
	}else{
		SVP_LogMsg(_T("No m_PerfFrequency!!"));
	}
	return 0;
}
