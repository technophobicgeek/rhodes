// Rhodes.cpp : Implementation of WinMain.
#include "stdafx.h"
#include "MainWindow.h"

#include "common/RhodesApp.h"
#include "common/StringConverter.h"
#include "common/rhoparams.h"
#include "rho/rubyext/GeoLocationImpl.h"
#include "ruby/ext/rho/rhoruby.h"
#include "net/NetRequestImpl.h"
#include "sync/ClientRegister.h"

using namespace rho;
using namespace rho::common;
using namespace std;
using namespace stdext;

#ifndef RUBY_RUBY_H
typedef unsigned long VALUE;
#endif //!RUBY_RUBY_H

#if defined(OS_WINDOWS)
char* parseToken( const char* start, int len );
#endif

extern "C" char* wce_wctomb(const wchar_t* w);
extern "C" wchar_t* wce_mbtowc(const char* a);
extern "C" void rho_ringtone_manager_stop();

#if defined(_WIN32_WCE)
#include <regext.h>

// Global Notification Handle
HREGNOTIFY g_hNotify = NULL;

// ConnectionsNetworkCount
// Gets a value indicating the number of network connections that are currently connected.
#define SN_CONNECTIONSNETWORKCOUNT_ROOT HKEY_LOCAL_MACHINE
#define SN_CONNECTIONSNETWORKCOUNT_PATH TEXT("System\\State\\Connections\\Network")
#define SN_CONNECTIONSNETWORKCOUNT_VALUE TEXT("Count")

#endif

#ifdef OS_WINDOWS
String httpProxy;
#endif

static String g_strCmdLine;

class CRhodesModule : public CAtlExeModuleT< CRhodesModule >
{
private:

public :
	bool ParseCommandLine(LPCTSTR lpCmdLine, HRESULT* pnRetCode ) throw( ) 
    {
		m_nRestarting = 1;
		TCHAR szTokens[] = _T("-/");
		LPCTSTR lpszToken = FindOneOf(lpCmdLine, szTokens);
        getRhoRootPath();

		while (lpszToken != NULL)
		{
			if (WordCmpI(lpszToken, _T("Restarting"))==0) {
				m_nRestarting = 10;
			}

#if defined(OS_WINDOWS)
			if (wcsncmp(lpszToken, _T("http_proxy_url"),14)==0) {
				char *token = wce_wctomb(lpszToken);
				char *proxy =parseToken(token,strlen(token));
				
				if (proxy)
					httpProxy = proxy;
				else 
					LOG(WARNING) + "invalid value for \"http_proxy_url\" cmd parameter";

				if (proxy) free(proxy);
				if (token) free(token);

			} else if (wcsncmp(lpszToken, _T("approot"),7)==0) {
				char* token = wce_wctomb(lpszToken);
				//parseToken will allocate extra byte at the end of the returned token value
				char* path = parseToken( token, strlen(token) );
				if (path) {
					int len = strlen(path);
					if (!(path[len]=='\\' || path[len]=='/')) {
						path[len] = '\\';
						path[len+1]  = 0;
					}
					m_strRootPath = path;
					free(path);
				}
				free(token);
			}
#endif
			lpszToken = FindOneOf(lpszToken, szTokens);
		}

		return __super::ParseCommandLine(lpCmdLine, pnRetCode);
	}

    // This method is called immediately before entering the message loop.
    // It contains initialization code for the application.
    // Returns:
    // S_OK => Success. Continue with RunMessageLoop() and PostMessageLoop().
    // S_FALSE => Skip RunMessageLoop(), call PostMessageLoop().
    // error code => Failure. Skip both RunMessageLoop() and PostMessageLoop().
    HRESULT PreMessageLoop(int nShowCmd) throw()
    {
        HRESULT hr = __super::PreMessageLoop(nShowCmd);
        if (FAILED(hr))
        {
            return hr;
        }
        // Note: In this sample, we don't respond differently to different hr success codes.

        // Allow only one instance of the application.
        // the "| 0x01" activates the correct owned window of the previous instance's main window
		HWND hWnd = NULL;
		for (int wait = 0; wait < m_nRestarting; wait++) {
			hWnd = FindWindow(CMainWindow::GetWndClassInfo().m_wc.lpszClassName, NULL);
			if (hWnd && m_nRestarting > 1) {
				Sleep(1000);
			} else {
				break;
			}
		}
		//EnumWindows(EnumRhodesWindowsProc, (LPARAM)&hWnd);

		if (hWnd)
		{
			SetForegroundWindow( HWND( DWORD(hWnd) | 0x01 ) );
			return S_FALSE;
		}

		rho_logconf_Init(m_strRootPath.c_str());

        if ( !rho_rhodesapp_canstartapp(g_strCmdLine.c_str(), " /-,") )
        {
			LOG(INFO) + "This is hidden app and can be started only with security key.";
			return S_FALSE;
        }

		LOG(INFO) + "Rhodes started";
#ifdef OS_WINDOWS
		if (httpProxy.length() > 0) {
			parseHttpProxyURI(httpProxy);
		} else {
			if (RHOCONF().isExist("http_proxy_url")) {
				parseHttpProxyURI(RHOCONF().getString("http_proxy_url"));
			}
		}
#endif
        //::SetThreadPriority(GetCurrentThread(),10);

		//Check for bundle directory is exists.
		HANDLE hFind;
		WIN32_FIND_DATA wfd;
		
		// rootpath + "rho/"
		if (m_strRootPath.at(m_strRootPath.length()-1) == '/') {
			hFind = FindFirstFile(convertToStringW(m_strRootPath.substr(0, m_strRootPath.find_last_of('/'))).c_str(), &wfd);
		} else if (m_strRootPath.at(m_strRootPath.length()-1) == '\\') {
			//delete all '\' from the end of the pathname
			int i = m_strRootPath.length();
			for ( ; i != 1; i--) {
				if (m_strRootPath.at(i-1) != '\\')
					break;
			}
			hFind = FindFirstFile(convertToStringW(m_strRootPath.substr(0, i)).c_str(), &wfd);
		}

		if (INVALID_HANDLE_VALUE == hFind || !(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			int last = 0, pre_last = 0;
			last = getRhoRootPath().find_last_of('\\');
			pre_last = getRhoRootPath().substr(0, last).find_last_of('\\');
			String appName = getRhoRootPath().substr(pre_last + 1, last - pre_last - 1);

			String messageText = "Bundle directory \"" + 
									m_strRootPath.substr(0, m_strRootPath.find_last_of('/')) + 
									"\" is  missing\n";

			LOG(INFO) + messageText;
			int msgboxID = MessageBox(NULL,
										convertToStringW(messageText).c_str(),
										convertToStringW(appName).c_str(),
										MB_ICONERROR | MB_OK);


			return S_FALSE;
	    }

        rho::common::CRhodesApp::Create(m_strRootPath );
        String strTitle = RHODESAPP().getAppTitle();

        DWORD dwStyle = WS_VISIBLE;

#if !defined(_WIN32_WCE)
        dwStyle |= WS_OVERLAPPEDWINDOW;
#endif
        // Create the main application window
        m_appWindow.Create(NULL, CWindow::rcDefault, convertToStringW(strTitle).c_str(), dwStyle);
        if (NULL == m_appWindow.m_hWnd)
        {
            return S_FALSE;
        }

        RHODESAPP().startApp();

        // Navigate to the "loading..." page
		m_appWindow.Navigate2(_T("about:blank"));
        // Show the main application window
        m_appWindow.ShowWindow(nShowCmd);

#if defined(_WIN32_WCE)
		// Register for changes in the number of network connections
		hr = RegistryNotifyWindow(SN_CONNECTIONSNETWORKCOUNT_ROOT,
			SN_CONNECTIONSNETWORKCOUNT_PATH, 
			SN_CONNECTIONSNETWORKCOUNT_VALUE, 
			m_appWindow.m_hWnd, 
			WM_CONNECTIONSNETWORKCOUNT, 
			0, 
			NULL, 
			&g_hNotify);
#else
		rho_clientregister_create("win32_client");
#endif

        return S_OK;
    }

	HWND GetMainWindow() {
		return m_appWindow.m_hWnd;
	}

	CMainWindow* GetMainWindowObject() {
		return &m_appWindow;
	}

	CMainWindow& GetAppWindow() {
		return m_appWindow;
	}

	HWND GetWebViewWindow() {
		return m_appWindow.getWebViewHWND();
	}

    void RunMessageLoop( ) throw( )
    {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            if (!m_appWindow.TranslateAccelerator(&msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

#if defined(OS_WINCE)
        CGPSController* pGPS = CGPSController::Instance();
        pGPS->DeleteInstance();
#endif
        rho_ringtone_manager_stop();

#if !defined(_WIN32_WCE)
        rho_clientregister_destroy();
#endif

        rho::common::CRhodesApp::Destroy();

        net::CNetRequestImpl::deinitConnection();
    }

    const rho::String& getRhoRootPath()
    {
        if ( m_strRootPath.length() == 0 )
        {
            char rootpath[MAX_PATH];
            int len;
            if ( (len = GetModuleFileNameA(NULL,rootpath,MAX_PATH)) == 0 )
                strcpy(rootpath,".");
            else
            {
                while( !(rootpath[len] == '\\'  || rootpath[len] == '/') )
                  len--;
                rootpath[len+1]=0;
            }

            m_strRootPath = rootpath;
            m_strRootPath += "rho/";
        }

        return m_strRootPath; 
    }

	void parseHttpProxyURI(const rho::String &http_proxy)
	{
		// http://<login>:<passwod>@<host>:<port>
		const char *default_port = "8080";

		if (http_proxy.length() < 8) {
			LOG(ERROR) + "invalid http proxy url";
			return;
		}

		int index = http_proxy.find("http://", 0, 7);
		if (index == string::npos) {
			LOG(ERROR) + "http proxy url should starts with \"http://\"";
			return;
		}
		index = 7;

		enum {
			ST_START,
			ST_LOGIN,
			ST_PASSWORD,
			ST_HOST,
			ST_PORT,
			ST_FINISH
		};

		String token, login, password, host, port;
		char c, state = ST_START, prev_state = state;
		int length = http_proxy.length();

		for (int i = index; i < length; i++) {
			c = http_proxy[i];

			switch (state) {
			case ST_START:
				if (c == '@') {
					prev_state = state; state = ST_HOST;
				} else if (c == ':') {
					prev_state = state; state = ST_PASSWORD;
				} else {
					token +=c;
					state = ST_HOST;
				}
				break;
			case ST_HOST:
				if (c == ':') {
					host = token; token.clear();			
					prev_state = state; state = ST_PORT;
				} else if (c == '@') {
					host = token; token.clear();		
					prev_state = state;	state = ST_LOGIN;					
				} else {
					token += c;
					if (i == (length - 1)) {
						host = token; token.clear();								
					}
				}
				break;
			case ST_PORT:
				if (c == '@') {
					port = token; token.clear();			
					prev_state = state; state = ST_LOGIN;
				} else {
					token += c;
					if (i == (length - 1)) {
						port = token; token.clear();
					}
				}
				break;
			case ST_LOGIN:
				if (prev_state == ST_PORT || prev_state == ST_HOST) {
					login    = host; host.clear();
					password = port; port.clear();
					prev_state = state; state = ST_HOST;
					token += c;
				} else {
					token += c;
					if (i == (length - 1)) {
						login = token; token.clear();								
					}
				}
				break;
			case ST_PASSWORD:
				if (c == '@') {
					password = token; token.clear();			
					prev_state = state; state = ST_HOST;
				} else {
					token += c;
					if (i == (length - 1)) {
						password = token; token.clear();								
					}
				}
				break;
			default:
				;
			}
		}

		LOG(INFO) + "Setting up HTTP proxy:";
		LOG(INFO) + "URI: " + http_proxy;
		LOG(INFO) + "HTTP proxy login    = " + login;
		LOG(INFO) + "HTTP proxy password = " + password;
		LOG(INFO) + "HTTP proxy host     = " + host;
		LOG(INFO) + "HTTP proxy port     = " + port;
	
		if (host.length()) {
			RHOCONF().setString ("http_proxy_host", host, false);

			if (port.length()){
				RHOCONF().setString ("http_proxy_port", port, false);
			} else {
				LOG(WARNING) + "there is no proxy port defined";
			}

			if (login.length())
				RHOCONF().setString ("http_proxy_login", login, false);

			if (password.length())
				RHOCONF().setString ("http_proxy_password", password, false);

		} else {
			LOG(ERROR) + "empty host name in HTTP-proxy URL";
		}

	}

private:
    CMainWindow m_appWindow;
    rho::String m_strRootPath;
	int m_nRestarting;
};

CRhodesModule _AtlModule;
HINSTANCE rhoApplicationHINSTANCE = 0;
//
bool g_restartOnExit = false;
//
extern "C" int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
                                LPTSTR lpCmdLine, int nShowCmd)
{
	INITCOMMONCONTROLSEX ctrl;

	rhoApplicationHINSTANCE = hInstance;
	
	//Required to use datetime picker controls.
	ctrl.dwSize = sizeof(ctrl);
	ctrl.dwICC = ICC_DATE_CLASSES|ICC_BAR_CLASSES;
	InitCommonControlsEx(&ctrl);

    g_strCmdLine = convertToStringA(lpCmdLine);

	return _AtlModule.WinMain(nShowCmd);
}

extern "C" HWND getMainWnd() {
	return _AtlModule.GetMainWindow();
}

CMainWindow& getAppWindow() 
{
	return _AtlModule.GetAppWindow();
}

extern "C" HWND getWebViewWnd() {
	return _AtlModule.GetWebViewWindow();
}

CMainWindow* Rhodes_getMainWindow() {
	return _AtlModule.GetMainWindowObject();
}


extern "C" const char* rho_native_rhopath() 
{
    return _AtlModule.getRhoRootPath().c_str();
}

extern "C" void rho_conf_show_log()
{
    ::PostMessage(getMainWnd(),WM_COMMAND,IDM_LOG,0);
}

//Hook for ruby call to refresh web view

extern "C" void rho_net_impl_network_indicator(int active)
{
    //TODO: rho_net_impl_network_indicator
}
/*
extern "C" void mapview_create(rho_param *p) {
    //TODO: mapview_create
}

extern "C" void mapview_close() {
    //TODO: mapview_close
}

extern "C" VALUE mapview_state_started() {
    //TODO: mapview_state_started
    return 0;
}

extern "C" double mapview_state_center_lat() {
    //TODO:
    return 0;
}

extern "C" double mapview_state_center_lon() {
    //TODO:
    return 0;
}
*/
extern "C" void rho_map_location(char* query)
{
}

extern "C" void rho_appmanager_load( void* httpContext, char* szQuery)
{
}

extern "C" void Init_openssl(void)
{
}

//extern "C" void Init_digest(void)
//{
//}

extern "C" void Init_fcntl(void)
{
}

//extern "C" void Init_RhoEvent()
//{
//}

//extern "C" void Init_Calendar()
//{
//}

/*BOOL EnumRhodesWindowsProc(HWND hwnd,LPARAM lParam)
{
	char buf[255] = {0};
	static char current_path[255] = {0};
	if ( strlen(current_path) == 0 )
		GetWindowModuleFileName(getMainWnd(), current_path, 255);
	
	GetWindowModuleFileName(hwnd, buf, 255);

	if ( strncmp( buf, current_path, 255 ) == 0 )
	{
		HWND* pWnd = (HWND*)lParam;
		*pWnd = hwnd;
		return FALSE;
	}

	return TRUE;
}*/

#if defined(OS_WINDOWS)
//parseToken will allocate extra byte at the end of the 
//returned token value
char* parseToken( const char* start, int len ) {
    int nNameLen = 0;
    while(*start==' '){ start++; len--;}

    int i = 0;
    for( i = 0; i < len; i++ ){
        if ( start[i] == '=' ){
            if ( i > 0 ){
                int s = i-1;
                for(; s >= 0 && start[s]==' '; s-- );

                nNameLen = s+1;
                break;
            }else 
                break;
        }
    }

    if ( nNameLen == 0 )
        return NULL;

    const char* szValue = start + i+1;
    int nValueLen = len - (i+1);

    while(*szValue==' ' || *szValue=='\'' || *szValue=='"' && nValueLen >= 0 ){ szValue++; nValueLen--;}
    while(nValueLen > 0 && (szValue[nValueLen-1]==' ' || szValue[nValueLen-1]=='\'' || szValue[nValueLen-1]=='"')) nValueLen--;

	char* value = (char*) malloc(nValueLen+2);
	strncpy(value, szValue, nValueLen);
	value[nValueLen] = '\0';

	return value;
}

// char -> wchar_t 
wchar_t* wce_mbtowc(const char* a)
{
	int length;
	wchar_t *wbuf;

	length = MultiByteToWideChar(CP_ACP, 0, 
		a, -1, NULL, 0);
	wbuf = (wchar_t*)malloc( (length+1)*sizeof(wchar_t) );
	MultiByteToWideChar(CP_ACP, 0,
		a, -1, wbuf, length);

	return wbuf;
}

// wchar_t -> char
char* wce_wctomb(const wchar_t* w)
{
	DWORD charlength;
	char* pChar;

	charlength = WideCharToMultiByte(CP_ACP, 0, w,
					-1, NULL, 0, NULL, NULL);
	pChar = (char*)malloc(charlength+1);
	WideCharToMultiByte(CP_ACP, 0, w,
		-1, pChar, charlength, NULL, NULL);

	return pChar;
}

#endif


#if !defined(_WIN32_WCE)
#include <gdiplus.h>
#include <Gdiplusinit.h>
using namespace Gdiplus;

#define   SelectBitmap(hdc, hbm)  ((HBITMAP)SelectObject((hdc), (HGDIOBJ)(HBITMAP)(hbm)))
HBITMAP SHLoadImageFile(  LPCTSTR pszFileName )
{
    if ( !pszFileName || !*pszFileName )
        return 0;

    String strFileName = convertToStringA(pszFileName);
    if ( String_endsWith(strFileName, ".bmp") )
    {
        return (HBITMAP)::LoadImage(NULL, pszFileName, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    }

    if ( !String_endsWith(strFileName, ".png") )
        return 0;

    static bool s_GDIInit = false;
    if ( !s_GDIInit)
    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
        s_GDIInit = true;
    }

    Gdiplus::Image* image = new Gdiplus::Image(convertToStringW(strFileName).c_str());
    SizeF sizePng;
    Status res = image->GetPhysicalDimension(&sizePng);

    HDC hDC = GetDC(getMainWnd());

    HDC hdcMem = CreateCompatibleDC(hDC);
    HBITMAP hBitmap  = ::CreateCompatibleBitmap(hDC, (int)sizePng.Width, (int)sizePng.Height);
    HBITMAP hbmOld = SelectBitmap(hdcMem, hBitmap);

    CRect rc(0,0,(int)sizePng.Width, (int)sizePng.Height);
	COLORREF clrOld = ::SetBkColor(hdcMem, RGB(255,255,255));
	::ExtTextOut(hdcMem, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
	::SetBkColor(hdcMem, clrOld);

    Gdiplus::Graphics grpx(hdcMem);
    res = grpx.DrawImage(image, 0, 0, (int)sizePng.Width, (int)sizePng.Height);

    SelectBitmap(hdcMem, hbmOld);
    DeleteDC(hdcMem);
    DeleteDC(hDC);

    return hBitmap;
}

#endif