#include "plugincfg.h"
#include "netcfglng.h"

#include <string>
#include <assert.h>


#include <utils.h>
#include <KeyFileHelper.h>

#include <common/log.h>

#define LOG_SOURCE_FILE "plugincfg.cpp"
#define LOG_MAX_PATH 256

static char initial_log[LOG_MAX_PATH] = {'/','t','m','p','/','f','a','r','2','l','.','n','e','t','c','f','g','.','l','o','g',0};
const char * LOG_FILE = initial_log;
static_assert( sizeof("/dev/null") < LOG_MAX_PATH );

#define INI_LOCATION InMyConfig("plugins/netcfg/config.ini")
#define INI_SECTION "Settings"

PluginCfg::PluginCfg(const PluginStartupInfo * info) :
	startupInfo(info)
{
	KeyFileReadSection kfr(INI_LOCATION, INI_SECTION);
	addToDisksMenu = (bool)kfr.GetInt("addToDisksMenu", true);
	addToPluginsMenu = (bool)kfr.GetInt("addToPluginsMenu", true);
	logEnable = (bool)kfr.GetInt("logEnable", true);
        if( logEnable ) {
		std::string logfile = kfr.GetString("logfile", initial_log);
		if( logfile.size() < (LOG_MAX_PATH-1) && logfile.size() >= sizeof("/a") )
			memmove(initial_log, logfile.c_str(), logfile.size()+1);
	} else
		memmove(initial_log, "/dev/null", sizeof("/dev/null"));

	LOG_INFO("\n");
}

PluginCfg::~PluginCfg()
{
	LOG_INFO("\n");
	startupInfo = 0;
}

const wchar_t * PluginCfg::GetMsg(int msgId)
{
	assert( startupInfo != 0 );
	return startupInfo->GetMsg(startupInfo->ModuleNumber, msgId);
}

void PluginCfg::GetPluginInfo(struct PluginInfo *info)
{
	info->StructSize = sizeof(PluginInfo);
	info->Flags = 0;

	static const wchar_t *diskMenuStrings[1];
	static const wchar_t *pluginMenuStrings[1];
	static const wchar_t *pluginConfigStrings[1];

	diskMenuStrings[0] = GetMsg(MDiskMenuString);
	pluginMenuStrings[0] = GetMsg(MPluginMenuString);
	pluginConfigStrings[0] = GetMsg(MPluginConfigString);

	info->DiskMenuStrings = diskMenuStrings;
	info->DiskMenuStringsNumber = addToDisksMenu ? ARRAYSIZE(diskMenuStrings):0;

	info->PluginMenuStrings = pluginMenuStrings;
	info->PluginMenuStringsNumber = addToPluginsMenu ? ARRAYSIZE(pluginMenuStrings):0;

	info->PluginConfigStrings = pluginConfigStrings;
	info->PluginConfigStringsNumber = ARRAYSIZE(pluginConfigStrings);

	info->CommandPrefix = 0;
}

void PluginCfg::CfgPanelModes(void)
{
	memset(PanelModesArray,0,sizeof(PanelModesArray));

	static const wchar_t * statusColumnTypes = L"N,C0,SF,C1,C2";
	static const wchar_t * statusColumnWidths = L"7,16,7,17,0";

	for( int i =0; i < ARRAYSIZE(PanelModesArray); i++ ) {
		PanelModesArray[i].FullScreen = 0;
		PanelModesArray[i].DetailedStatus = 0;
		PanelModesArray[i].AlignExtensions = 0;
		PanelModesArray[i].CaseConversion = TRUE;
		PanelModesArray[i].StatusColumnTypes = statusColumnTypes;
		PanelModesArray[i].StatusColumnWidths = statusColumnWidths;
	}

	// ifname ip ipv6 mac bytes
	PanelModesArray[4].ColumnTypes = L"N,C0,SF,C1,C2";
	PanelModesArray[4].ColumnWidths = L"7,15,7,17,0";
	static const wchar_t * ColumnTitles[5] = {L"ifc",L"ip",L"snd+rcv", L"mac", L"ipv6"};
	PanelModesArray[4].ColumnTitles = (const wchar_t* const*)&ColumnTitles;

	// name                       N
	// ip                         C0
	// recv_bytes + send_bytes    SF
	// mac                        C1
	// ipv6                       C2
	// mtu                        C3
	// recv_bytes                 C4
	// send_bytes                 C5
	// recv_packets               C6
	// send_packets               C7
	// recv_errors                C8
	// send_errors                C9
	// multicast                  C10
	// collisions                 C11
	// permanent_mac              C12

	PanelModesArray[5].ColumnTypes =  L"N,C0,C1,C2,C3,C4,C5,C6,C7,C8,C9,C10,C11,C12";
	PanelModesArray[5].ColumnWidths = L"7,15,17,25,0,0,0,0,0,0,0,0,0,17";
	static const wchar_t * ColumnTitles5[15] = {L"ifc",L"ip",L"mac", L"ipv6", L"mtu", L"rcv", L"snd", L"rpkts", L"spkts",L"rerr",L"serr",L"mcast",L"colls",L"permmac"};

	PanelModesArray[5].ColumnTitles = (const wchar_t* const*)&ColumnTitles5;
	PanelModesArray[5].FullScreen = TRUE;
}

void PluginCfg::CfgKeyBarTitles(void)
{
	memset(&keyBar,0,sizeof(keyBar));
	keyBar.Titles[3-1]=(TCHAR*)GetMsg(MF3);
	keyBar.Titles[4-1]=(TCHAR*)GetMsg(MF4);
	keyBar.Titles[5-1]=(TCHAR*)GetMsg(MF5);
	keyBar.Titles[6-1]=(TCHAR*)GetMsg(MF6);

	keyBar.Titles[1]=\
	keyBar.Titles[6]=keyBar.Titles[7]=
	keyBar.Titles[8]=keyBar.Titles[10]=keyBar.Titles[11]=(wchar_t *)L"";
}

void PluginCfg::CgfPluginData(void)
{
	memset(&openInfo,0,sizeof(openInfo));
	openInfo.StructSize = sizeof(OpenPluginInfo);
	openInfo.Flags = OPIF_USEFILTER|OPIF_USEHIGHLIGHTING|OPIF_ADDDOTS|OPIF_SHOWPRESERVECASE;

	openInfo.HostFile = 0;
	openInfo.CurDir=_T(""); // If plugin returns empty string here, FAR will close plugin automatically if ENTER is pressed on ".." item
	openInfo.Format=(TCHAR*)GetMsg(MFormatNetcfgPanel); // see copy menu
	openInfo.InfoLines = 0;
	openInfo.InfoLinesNumber = 0;
	openInfo.DescrFiles = 0;
	openInfo.DescrFilesNumber = 0;

	openInfo.PanelTitle=(TCHAR*)GetMsg(MPanelTitle);

	CfgPanelModes();
	openInfo.PanelModesArray = PanelModesArray;
	openInfo.PanelModesNumber = ARRAYSIZE(PanelModesArray);
	openInfo.StartPanelMode=_T('4');

	openInfo.StartSortMode = SM_NAME;
	openInfo.StartSortOrder = 0; // 0 for ascending, 1 - for descedning

	CfgKeyBarTitles();
	openInfo.KeyBar = &keyBar;

	openInfo.ShortcutData = 0;
}

void PluginCfg::GetOpenPluginInfo(struct OpenPluginInfo *info)
{
	// reload string
	keyBar.Titles[3-1]=(TCHAR*)GetMsg(MF3);
	keyBar.Titles[4-1]=(TCHAR*)GetMsg(MF4);
	keyBar.Titles[5-1]=(TCHAR*)GetMsg(MF5);
	keyBar.Titles[6-1]=(TCHAR*)GetMsg(MF6);
	openInfo.Format=(TCHAR*)GetMsg(MFormatNetcfgPanel); // see copy menu
	openInfo.PanelTitle=(TCHAR*)GetMsg(MPanelTitle);

	*info = openInfo;
}

static const int DIALOG_WIDTH = 78;

enum {
	WinCfgCaptionIndex,
	WinCfgAddDiskMenuIndex,
	WinCfgAddPluginsMenuIndex,
	WinCfgEanbleLogIndex,
	WinCfgEanbleLogEditIndex,
	WinCfgSeparatorIndex,
	WinCfgOkIndex,
	WinCfgCancelIndex,
	WinCfgMaxIndex
};

int PluginCfg::Configure(int itemNumber)
{
	LOG_INFO("\n");
	bool change = false;

	static struct FarDialogItem fdi_template[] = {
  //        Type        X1   Y1       X2              Y2          Focus    union    Flags             DefaultButton            PtrData             MaxLen
  /* 0*/ {DI_DOUBLEBOX, 3,   1,  DIALOG_WIDTH-4,       0,           0,      {0},       0,                    0,      L"Netconfig",                   0},
  /* 1*/ {DI_CHECKBOX,  5,   2,       0,               0,           0,      {0},       0,                    0,      L"Add to Disks menu",           0},
  /* 2*/ {DI_CHECKBOX, 40,   2,       0,               0,           0,      {0},       0,                    0,      L"Add to Plugins menu",         0},
  /* 3*/ {DI_CHECKBOX,  5,   3,       0,               0,           0,      {0},       0,                    0,      L"Enable log:",                 0},
  /* 4*/ {DI_EDIT,     21,   3,      72,               4,           0,      {0},       0,                    0,      0,                              0},
  /* 5*/ {DI_TEXT,      5,   4,       0,               0,           0,      {0}, DIF_BOXCOLOR|DIF_SEPARATOR, 0,      L"",                            0},
  /* 6*/ {DI_BUTTON,    0,   5,       0,               0,           0,      {0}, DIF_CENTERGROUP,            0,      GetMsg(MOk),                    0},
  /* 7*/ {DI_BUTTON,    0,   5,       0,               0,           0,      {0}, DIF_CENTERGROUP,            0,      GetMsg(MCancel),                0}
	};

	unsigned int fdi_cnt = ARRAYSIZE(fdi_template);
	struct FarDialogItem * fdi = (struct FarDialogItem *)malloc(fdi_cnt * sizeof(FarDialogItem));
	if( !fdi )
		return FALSE;

	do {
		memmove(fdi, fdi_template, sizeof(fdi_template));
		fdi[WinCfgCaptionIndex].PtrData = GetMsg(MConfigPluginTitle);
		fdi[WinCfgAddDiskMenuIndex].PtrData = GetMsg(MAddDisksMenu);
		fdi[WinCfgAddDiskMenuIndex].Selected = addToDisksMenu;
		fdi[WinCfgAddPluginsMenuIndex].PtrData = GetMsg(MAddToPluginsMenu);
		fdi[WinCfgAddPluginsMenuIndex].Selected = addToPluginsMenu;

		fdi[WinCfgEanbleLogIndex].PtrData = GetMsg(MEnableLog);
		fdi[WinCfgEanbleLogIndex].Selected = logEnable;

		std::string _logfile(LOG_FILE);
		std::wstring logfile(_logfile.begin(), _logfile.end());

		fdi[WinCfgEanbleLogEditIndex].PtrData = logfile.c_str();

		fdi[WinCfgCaptionIndex].Y2 = ARRAYSIZE(fdi_template)-2;

		fdi[fdi_cnt-2].DefaultButton = 1;
		fdi[3].Focus = 1;

		assert( startupInfo != 0 );
		HANDLE hDlg = startupInfo->DialogInit(startupInfo->ModuleNumber, -1, -1, DIALOG_WIDTH, ARRAYSIZE(fdi_template), L"Config plugin", fdi, fdi_cnt, 0, 0, NULL, 0);
		if (hDlg == INVALID_HANDLE_VALUE) {
			LOG_ERROR("DialogInit()\n");
			break;
		}

		if( startupInfo->DialogRun(hDlg) == (fdi_cnt-2) ) {

			KeyFileHelper kfh(INI_LOCATION);

			change |= bool(startupInfo->SendDlgMessage(hDlg, DM_GETCHECK, WinCfgAddDiskMenuIndex, 0)) != addToDisksMenu;
			addToDisksMenu = bool(startupInfo->SendDlgMessage(hDlg, DM_GETCHECK, WinCfgAddDiskMenuIndex, 0));
			kfh.SetInt(INI_SECTION, "addToDisksMenu", addToDisksMenu);

			change |= bool(startupInfo->SendDlgMessage(hDlg, DM_GETCHECK, WinCfgAddPluginsMenuIndex, 0)) != addToPluginsMenu;
			addToPluginsMenu = bool(startupInfo->SendDlgMessage(hDlg, DM_GETCHECK, WinCfgAddPluginsMenuIndex, 0));
			kfh.SetInt(INI_SECTION, "addToPluginsMenu", addToPluginsMenu);

			logfile = (const wchar_t *)startupInfo->SendDlgMessage(hDlg, DM_GETCONSTTEXTPTR, WinCfgEanbleLogEditIndex, 0);
			if( startupInfo->FSF->LStricmp(logfile.c_str(), fdi[WinCfgEanbleLogEditIndex].PtrData) != 0 && \
				logfile.size() < (LOG_MAX_PATH-1) &&
				logfile.size() >= sizeof("/a") ) {
				change = true;
				_logfile = Wide2MB( logfile.c_str() );
				memmove(initial_log, _logfile.c_str(), _logfile.size()+1);
				kfh.SetString(INI_SECTION, "logfile", _logfile);
			}

			change |= bool(startupInfo->SendDlgMessage(hDlg, DM_GETCHECK, WinCfgEanbleLogIndex, 0)) != logEnable;
			logEnable = bool(startupInfo->SendDlgMessage(hDlg, DM_GETCHECK, WinCfgEanbleLogIndex, 0));
			if( change && !logEnable )
				memmove(initial_log, "/dev/null", sizeof("/dev/null"));
			kfh.SetInt(INI_SECTION, "logEnable", logEnable);

			if( change )
				kfh.Save();
		}

	} while(0);

	free((void *)fdi);
	return change;
}
