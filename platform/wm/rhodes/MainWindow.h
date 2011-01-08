// MainWindow.h: Defines main window for this application.

#pragma once

#if !defined(_WIN32_WCE)
#include <exdispid.h>
#include <exdisp.h>
#endif
#include "resource.h"
#include "logging/RhoLog.h"
#include "common/RhoConf.h"
#include "common/RhodesApp.h"
#include "Alert.h"
#include "RhoNativeViewManagerWM.h"
#include "SyncStatusDlg.h"

#if defined(OS_WINDOWS)
#include "menubar.h"
#endif
#include "LogView.h"

#define ID_CUSTOM_MENU_ITEM_FIRST (WM_APP+3)
#define ID_CUSTOM_MENU_ITEM_LAST  (ID_CUSTOM_MENU_ITEM_FIRST + (APP_MENU_ITEMS_MAX) - 1)

static const UINT ID_BROWSER = 1;

static UINT WM_TAKEPICTURE             = ::RegisterWindowMessage(L"RHODES_WM_TAKEPICTURE");
static UINT WM_SELECTPICTURE           = ::RegisterWindowMessage(L"RHODES_WM_SELECTPICTURE");
static UINT WM_CONNECTIONSNETWORKCOUNT = ::RegisterWindowMessage(L"RHODES_WM_CONNECTIONSNETWORKCOUNT");
static UINT WM_ALERT_SHOW_POPUP        = ::RegisterWindowMessage(L"RHODES_WM_ALERT_SHOW_POPUP");
static UINT WM_ALERT_HIDE_POPUP        = ::RegisterWindowMessage(L"RHODES_WM_ALERT_HIDE_POPUP");
static UINT WM_DATETIME_PICKER         = ::RegisterWindowMessage(L"RHODES_WM_DATETIME_PICKER");
static UINT WM_BLUETOOTH_DISCOVER      = ::RegisterWindowMessage(L"RHODES_WM_BLUETOOTH_DISCOVER");
static UINT WM_BLUETOOTH_DISCOVERED    = ::RegisterWindowMessage(L"RHODES_WM_BLUETOOTH_DISCOVERED");
static UINT WM_BLUETOOTH_CALLBACK	   = ::RegisterWindowMessage(L"RHODES_WM_BLUETOOTH_CALLBACK");
static UINT WM_EXECUTE_COMMAND		   = ::RegisterWindowMessage(L"RHODES_WM_EXECUTE_COMMAND");


class CMainWindow :
#if defined(_WIN32_WCE)
	public CFrameWindowImpl<CMainWindow>, 
	public CFullScreenFrame<CMainWindow>,
#else
    public CWindowImpl<CMainWindow, CWindow, CWinTraits<WS_BORDER | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS> >,
#endif
    public IDispEventImpl<ID_BROWSER, CMainWindow>
{
    DEFINE_LOGCLASS;
public:
    CMainWindow();
    ~CMainWindow();
    //
	void Navigate2(BSTR URL);
    void Navigate(BSTR URL);

	HWND getWebViewHWND();
	
	//char* GetCurrentLocation() { return m_current_url; }

    // Required to forward messages to the PIEWebBrowser control
    BOOL TranslateAccelerator(MSG* pMsg);

#if defined(OS_WINDOWS)
    DECLARE_WND_CLASS(TEXT("Rhodes.MainWindow"))
#else
	static ATL::CWndClassInfo& GetWndClassInfo() 
	{ 
        static rho::StringW strAppName = RHODESAPP().getAppNameW() + L".MainWindow";
		static ATL::CWndClassInfo wc = 
		{ 
			{ CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, StartWindowProc, 
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_WINDOW + 1), NULL, strAppName.c_str() }, 
			NULL, NULL, IDC_ARROW, TRUE, 0, _T("") 
		}; 
		return wc; 
	}
#endif
    
	BEGIN_MSG_MAP(CMainWindow)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_ACTIVATE, OnActivate)
        MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        //MESSAGE_HANDLER(WM_SETTEXT, OnSetText)
		//MESSAGE_HANDLER(WM_PAINT, OnPaint)
        COMMAND_ID_HANDLER(IDM_EXIT, OnExitCommand)
        COMMAND_ID_HANDLER(IDM_NAVIGATE_BACK, OnNavigateBackCommand)
        COMMAND_ID_HANDLER(IDM_SK1_EXIT, OnBackCommand)
        COMMAND_ID_HANDLER(IDM_REFRESH, OnRefreshCommand)
		COMMAND_ID_HANDLER(IDM_NAVIGATE, OnNavigateCommand)
        COMMAND_ID_HANDLER(IDM_LOG,OnLogCommand)
		COMMAND_ID_HANDLER(ID_FULLSCREEN, OnFullscreenCommand)
		COMMAND_RANGE_HANDLER(ID_CUSTOM_MENU_ITEM_FIRST, ID_CUSTOM_MENU_ITEM_LAST, OnCustomMenuItemCommand)
#if defined(OS_WINDOWS)
		COMMAND_ID_HANDLER(IDM_POPUP_MENU, OnPopupMenuCommand)
		MESSAGE_HANDLER(WM_WINDOWPOSCHANGED, OnPosChanged)
#endif
		MESSAGE_HANDLER(WM_TAKEPICTURE, OnTakePicture)
		MESSAGE_HANDLER(WM_SELECTPICTURE, OnSelectPicture)
		MESSAGE_HANDLER(WM_CONNECTIONSNETWORKCOUNT, OnConnectionsNetworkCount)
        MESSAGE_HANDLER(WM_ALERT_SHOW_POPUP, OnAlertShowPopup)
		MESSAGE_HANDLER(WM_ALERT_HIDE_POPUP, OnAlertHidePopup);
		MESSAGE_HANDLER(WM_DATETIME_PICKER, OnDateTimePicker);
		MESSAGE_HANDLER(WM_BLUETOOTH_DISCOVER, OnBluetoothDiscover);
		MESSAGE_HANDLER(WM_BLUETOOTH_DISCOVERED, OnBluetoothDiscovered);
		MESSAGE_HANDLER(WM_BLUETOOTH_CALLBACK, OnBluetoothCallback);
		MESSAGE_HANDLER(WM_EXECUTE_COMMAND, OnExecuteCommand);
    END_MSG_MAP()
	
private:
    // WM_xxx handlers
    LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnActivate(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
    LRESULT OnSettingChange(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
    LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnSetText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);

    // WM_COMMAND handlers
    LRESULT OnExitCommand(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnBackCommand(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnNavigateBackCommand(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnRefreshCommand(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnNavigateCommand(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLogCommand(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFullscreenCommand (WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCustomMenuItemCommand (WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

#if defined(OS_WINDOWS)
	LRESULT OnPopupMenuCommand(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPosChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
#endif

	LRESULT OnTakePicture(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnSelectPicture(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnConnectionsNetworkCount(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
    LRESULT OnAlertShowPopup (UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnAlertHidePopup (UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnDateTimePicker (UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnBluetoothDiscover (UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnBluetoothDiscovered (UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnBluetoothCallback (UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnExecuteCommand (UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	
public:
    BEGIN_SINK_MAP(CMainWindow)
        SINK_ENTRY(ID_BROWSER, DISPID_BEFORENAVIGATE2, &CMainWindow::OnBeforeNavigate2)
        SINK_ENTRY(ID_BROWSER, DISPID_TITLECHANGE, &CMainWindow::OnBrowserTitleChange)
        SINK_ENTRY(ID_BROWSER, DISPID_NAVIGATECOMPLETE2, &CMainWindow::OnNavigateComplete2)
        SINK_ENTRY(ID_BROWSER, DISPID_DOCUMENTCOMPLETE, &CMainWindow::OnDocumentComplete)
        SINK_ENTRY(ID_BROWSER, DISPID_COMMANDSTATECHANGE, &CMainWindow::OnCommandStateChange)
    END_SINK_MAP()

private:
    // event handlers
    void __stdcall OnBeforeNavigate2(IDispatch* pDisp, VARIANT * pvtURL, 
                                     VARIANT * /*pvtFlags*/, VARIANT * pvtTargetFrameName,
                                     VARIANT * /*pvtPostData*/, VARIANT * /*pvtHeaders*/, 
                                     VARIANT_BOOL * /*pvbCancel*/);
    void __stdcall OnBrowserTitleChange(BSTR bstrTitleText);
    void __stdcall OnNavigateComplete2(IDispatch* pDisp, VARIANT * pvtURL);
    void __stdcall OnDocumentComplete(IDispatch* pDisp, VARIANT * pvtURL);
    void __stdcall OnCommandStateChange(long lCommand, BOOL bEnable);

    // utility functions
    BOOL SetMenuItemEnabled      (UINT uMenuItemID, BOOL bEnable);
	BOOL SetToolbarButtonEnabled (UINT uTbbID, BOOL bEnable);
	
	void ShowLoadingPage(LPDISPATCH pDisp, VARIANT* URL);

	void createCustomMenu(void);

	// return cleared URL or empty string
	String processForNativeView(String url);
	void restoreWebView();
	void hideWebView();
	void showWebView();

private:
	NativeViewFactory* mNativeViewFactory;
	NativeView* mNativeView;
	String mNativeViewType;


private:
    // Represents the PIEWebBrowser control contained in the main application.
    // window. m_browser is used to manage the control and its associated 
    // "AtlAxWin" window. (AtlAxWin is a window class that ATL uses to support 
    // containment of controls in windows.)
    CAxWindow m_browser;
	bool mIsBrowserViewHided;

    // cached copy of hosted control's IWebBrowser2 interface pointer
    CComPtr<IWebBrowser2> m_spIWebBrowser2;

#if defined(_WIN32_WCE)
    // main menu bar for application
    CWindow m_menuBar;
#elif defined (OS_WINDOWS)
	CMenuBar m_menuBar;
	int m_menuBarHeight;
	CLogView m_logView;
#endif //_WIN32_WCE

#if defined(_WIN32_WCE)
    // Used to manage SIP state. Also used to adjust window for SIP.
    SHACTIVATEINFO m_sai;
#endif

	bool m_bLoading;

#if !defined(_WIN32_WCE)
private:
	static int m_screenWidth;
	static int m_screenHeight;
	
public:
	static int getScreenWidth() {return m_screenWidth;}
	static int getScreenHeight() {return m_screenHeight;}
#endif
private:
	int m_pageCounter;

    rho::Vector<rho::common::CAppMenuItem> m_arAppMenuItems;
	CAlertDialog *m_alertDialog;

    CSyncStatusDlg m_SyncStatusDlg;
};
