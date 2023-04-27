#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include "netcfgplugin.h"
#include "netcfglng.h"

#include <utils.h>

#include <assert.h>

#include <common/log.h>
#include <common/errname.h>

extern const char * LOG_FILE;
#define LOG_SOURCE_FILE "netcfgplugin.cpp"

struct PluginStartupInfo NetCfgPlugin::psi = {0};
struct FarStandardFunctions NetCfgPlugin::FSF = {0};

NetCfgPlugin::NetCfgPlugin(const PluginStartupInfo * info)
{
	assert( info->StructSize >= sizeof(PluginStartupInfo) );
	assert( psi.StructSize == 0 );
	psi=*info;

	assert( info->FSF->StructSize >= sizeof(FarStandardFunctions) );
	assert( FSF.StructSize == 0 );
	FSF=*info->FSF;

	psi.FSF=&FSF;

	cfg = new PluginCfg(&psi);
	nifs = new NetInterfaces();

	LOG_INFO("\n");
}

NetCfgPlugin::~NetCfgPlugin()
{
	LOG_INFO("\n");

	delete nifs;
	nifs = 0;

	delete cfg;
	cfg = 0;

	memset(&psi, 0, sizeof(psi));
	memset(&FSF, 0, sizeof(FSF));
}

int NetCfgPlugin::Configure(int itemNumber)
{
	assert( cfg != 0 );
	return cfg->Configure(itemNumber);
}

HANDLE NetCfgPlugin::OpenPlugin(int openFrom, INT_PTR item)
{
	LOG_INFO("\n");

	int res = nifs->Update();
	cfg->CgfPluginData();

	return (HANDLE)(res == 0);
}

void NetCfgPlugin::ClosePlugin(HANDLE hPlugin)
{
	LOG_INFO("\n");
	nifs->Clear();
}

void NetCfgPlugin::Log(void)
{
	LOG_INFO("\n");
	nifs->Log();
}

const wchar_t * NetCfgPlugin::GetMsg(int msgId)
{
	assert( psi.GetMsg != 0 );
	return psi.GetMsg(psi.ModuleNumber, msgId);
}

void NetCfgPlugin::GetOpenPluginInfo(HANDLE hPlugin, struct OpenPluginInfo *info)
{
	cfg->GetOpenPluginInfo(info);
}

void NetCfgPlugin::GetPluginInfo(struct PluginInfo *info)
{
	cfg->GetPluginInfo(info);
}

const wchar_t * NetCfgPlugin::DublicateCountString(unsigned long long value)
{
	wchar_t max64[sizeof("18446744073709551615")] = {0};
	if( FSF.snprintf(max64, ARRAYSIZE(max64), L"%llu", value) > 0 )
		return wcsdup(max64);
	return wcsdup(L"");
}

const wchar_t * NetCfgPlugin::DublicateFileSizeString(unsigned long long value)
{
	if( value > 100 * 1024 ) {
		std::wstring _tmp = FileSizeString(value);
		return wcsdup(_tmp.c_str());
	}
	return DublicateCountString(value);
}

struct PluginUserData {
	DWORD size;
	NetInterface * net_if;
};

int NetCfgPlugin::GetFindData(struct PluginPanelItem **pPanelItem,int *pItemsNumber)
{

	LOG_INFO("\n");
	nifs->Update();
	nifs->Log();

	*pItemsNumber = nifs->size();
	*pPanelItem = (struct PluginPanelItem *)malloc((*pItemsNumber) * sizeof(PluginPanelItem));

	memset(*pPanelItem, 0, (*pItemsNumber) * sizeof(PluginPanelItem));
	PluginPanelItem * pi = *pPanelItem;

	std::wstring _tmp;

	for( const auto& [name, net_if] : *nifs ) {
		
		pi->FindData.lpwszFileName = name.c_str();

		pi->FindData.dwFileAttributes = net_if->IsCarrierOn() ? FILE_ATTRIBUTE_EXECUTABLE:(FILE_ATTRIBUTE_OFFLINE|FILE_ATTRIBUTE_HIDDEN);
		pi->FindData.nFileSize = net_if->recv_bytes+net_if->send_bytes;

		pi->Flags = PPIF_USERDATA;
		PluginUserData * user_data = (PluginUserData *)malloc(sizeof(PluginUserData));
		if( user_data ) {
			user_data->size = sizeof(PluginUserData);
			assert( net_if->size == sizeof(NetInterface) );
			user_data->net_if = net_if;
			pi->UserData = (DWORD_PTR)user_data;
		}

		const wchar_t ** CustomColumnData = (const wchar_t **)malloc(CColumnDataMaxIndex*sizeof(const wchar_t *));
		memset(CustomColumnData, 0, CColumnDataMaxIndex*sizeof(const wchar_t *));


		_tmp.clear();
		for( const auto& [ip, ipinfo] : net_if->ip ) {
			if( !_tmp.empty() )
				_tmp += L",";
			_tmp += ip;
		}

		CustomColumnData[CColumnDataIpIndex] = wcsdup(_tmp.c_str());


		for( const auto& [mac, macinfo] : net_if->mac ) {
			CustomColumnData[CColumnDataMacIndex] = wcsdup(mac.c_str());
			break;
		}
		if( CustomColumnData[CColumnDataMacIndex] == 0 )
			CustomColumnData[CColumnDataMacIndex] = wcsdup(L"");

		_tmp.clear();
		for( const auto& [ip, ipinfo] : net_if->ip6 ) {
			if( !_tmp.empty() )
				_tmp += L",";
			_tmp += ip;
		}
		CustomColumnData[CColumnDataIp6Index] = wcsdup(_tmp.c_str());

		CustomColumnData[CColumnDataMtuIndex] = DublicateCountString(net_if->mtu);

		CustomColumnData[CColumnDataRecvBytesIndex] = DublicateFileSizeString(net_if->recv_bytes);
		CustomColumnData[CColumnDataSendBytesIndex] = DublicateFileSizeString(net_if->send_bytes);

		CustomColumnData[CColumnDataRecvPktsIndex] = DublicateCountString(net_if->recv_packets);
		CustomColumnData[CColumnDataSendPktsIndex] = DublicateCountString(net_if->send_packets);
		CustomColumnData[CColumnDataRecvErrsIndex] = DublicateCountString(net_if->recv_errors);
		CustomColumnData[CColumnDataSendErrsIndex] = DublicateCountString(net_if->send_errors);
		CustomColumnData[CColumnDataMulticastIndex] = DublicateCountString(net_if->multicast);
		CustomColumnData[CColumnDataCollisionsIndex] = DublicateCountString(net_if->collisions);

		CustomColumnData[CColumnDataPermanentMacIndex] = wcsdup(net_if->permanent_mac.c_str());

		pi->CustomColumnData = CustomColumnData;
		pi->CustomColumnNumber = CColumnDataMaxIndex;
		pi++;
	}
	return int(true);
}

void NetCfgPlugin::FreeFindData(struct PluginPanelItem * panelItem, int itemsNumber)
{
	LOG_INFO("\n");
	while( itemsNumber-- ) {
		while( (panelItem+itemsNumber)->CustomColumnNumber-- )
			free((void *)(panelItem+itemsNumber)->CustomColumnData[(panelItem+itemsNumber)->CustomColumnNumber]);
		free((void *)(panelItem+itemsNumber)->CustomColumnData);

		if( (panelItem+itemsNumber)->Flags & PPIF_USERDATA && (panelItem+itemsNumber)->UserData )
			free((void *)(panelItem+itemsNumber)->UserData);

	}
	free((void *)panelItem);
}


const int DIALOG_WIDTH = 78;

enum {
	WinCaptionIndex,
	WinNameTextIndex,
	WinIfNameEditIndex,
	WinMACNameTextIndex,
	WinIfMACEditIndex,
	WinPermMACTextIndex,
	WinUpFlagIndex,
	WinMulticastFlagIndex,
	WinAllMulticastFlagIndex,
	WinPromiscFlagIndex,
	WinNoARPFlagIndex,
	WinDebugFlagIndex,
	WinIfFlagsIndex,
	WinMtuTextIndex,
	WinMtuEditIndex,
	WinCarrierTextIndex,
	WinCarrierOnOffTextIndex,
};

#define WIN_IP_INITIAL_Y 7

enum {
	WinIpTextIndex,
	WinIpEditIndex,
	WinIpMaskTextIndex,
	WinIpMaskEditIndex,
	WinIpBroadcastTextIndex,
	WinIpBroadcastEditIndex,
	WinIpPointToPointTextIndex = WinIpBroadcastTextIndex,
	WinIpPointToPointEditIndex = WinIpBroadcastEditIndex,
	WinIpMaxIndex
};

enum {
	WinIp6TextIndex,
	WinIp6EditIndex,
	WinIp6MaskTextIndex,
	WinIp6MaskEditIndex,
	WinIp6MaxIndex
};

enum {
	WinStatRecvSendCaptionTextIndex,
	WinStatRecvTextIndex,
	WinStatRecvBytesIndex,
	WinStatRecvPacketsIndex,
	WinStatRecvErrorsIndex,
	WinStatSendTextIndex,
	WinStatSendBytesIndex,
	WinStatSendPacketsIndex,
	WinStatSendErrorsIndex,
	WinStatMulticastTextIndex,
	WinStatMulticastIndex,
	WinStatCollisionsTextIndex,
	WinStatCollisionsIndex,
	WinStatMaxIndex
};

enum {
	WinFooterSeparatorIndex,
	WinFooterOkIndex,
	WinFooterCancelIndex,
};

enum {
	WinTcpCaptionIndex,
	WinTcpInterfaceTextIndex,
	WinTcpInterfaceButtonIndex,
	WinTcpPromiscCheckboxIndex,
	WinTcpFileTextIndex,
	WinTcpFileEditIndex,
	WinTcpSeparatorIndex,
	WinTcpOkButtonIndex,
	WinTcpCancelButtonIndex,
	WinTcpDumpMaxIndex
};

void NetCfgPlugin::SetCapFilePath(HANDLE hDlg, int item, const wchar_t * capname)
{
	int nameSize = psi.Control(PANEL_PASSIVE, FCTL_GETPANELDIR, 0, (LONG_PTR)0);
	wchar_t * path = new wchar_t[nameSize];
	psi.Control(PANEL_PASSIVE, FCTL_GETPANELDIR, nameSize, (LONG_PTR)path);
	std::wstring _file(path);
	delete[] path;
	_file += L"/";
	_file += capname;
	_file += L".cap";
	psi.SendDlgMessage(hDlg, DM_SETTEXTPTR, item, (LONG_PTR)_file.c_str());
	return;
}

LONG_PTR WINAPI SettingDialogProc(HANDLE hDlg, int msg, int param1, LONG_PTR param2)
{
	static NetCfgPlugin * netCfg = 0;
	if( msg == DN_INITDIALOG ) {
		netCfg = (NetCfgPlugin *)param2;
	}

	switch(msg){
		case DN_BTNCLICK:
		switch (param1){
			case WinTcpInterfaceButtonIndex:
			{
				int if_count = netCfg->nifs->size(), index = 0;
				FarMenuItem *menuElements = new FarMenuItem[if_count];
				memset(menuElements, 0, sizeof(FarMenuItem)*if_count);

				for( const auto& [name, net_if] : *(netCfg->nifs) ) {
					menuElements[index].Text = name.c_str();
					index++;
				}
				menuElements[0].Selected = 1;
				index = netCfg->psi.Menu(netCfg->psi.ModuleNumber, -1, -1, 0, FMENU_WRAPMODE|FMENU_AUTOHIGHLIGHT, \
						 L"Select interface:", 0, L"hrd", nullptr, nullptr, menuElements, if_count);
				if( index >= 0 && index < if_count ) {
					netCfg->psi.SendDlgMessage(hDlg, DM_SETTEXTPTR, WinTcpInterfaceButtonIndex, (LONG_PTR)menuElements[index].Text);
					netCfg->SetCapFilePath(hDlg, WinTcpFileEditIndex, menuElements[index].Text);
				}
				return true;
			};
		};
		break;
	};

	LOG_INFO("MSG: %u\n", msg);

	assert( netCfg != 0 );
	auto res = netCfg->psi.DefDlgProc(hDlg, msg, param1, param2);
	if( msg == DM_CLOSE )
		netCfg = 0;
	return res;
}

int NetCfgPlugin::ProcessKey(HANDLE hPlugin,int key,unsigned int controlState)
{
	bool change = false;

	if( controlState == 0 && key == VK_F3 )
		change = true;

	if( controlState == 0 && key == VK_F6 ) {

		int if_count = nifs->size(), index = 0;
		FarMenuItem *menuElements = new FarMenuItem[if_count];
		memset(menuElements, 0, sizeof(FarMenuItem)*if_count);

		for( const auto& [name, net_if] : *nifs ) {
			if( net_if->IsTcpdumpOn() ) {
				menuElements[index].Text = name.c_str();
				index++;
			}
		}

		if( index ) {
			menuElements[0].Selected = 1;
			index = psi.Menu(psi.ModuleNumber, -1, -1, 0, FMENU_WRAPMODE|FMENU_AUTOHIGHLIGHT, \
							 L"Stop tcpdump on interface:", 0, L"hrd", nullptr, nullptr, menuElements, index);
			if( index >= 0 && index < if_count ) {
				auto net_if = nifs->find(menuElements[index].Text);
				if( net_if != nifs->end() ) {
						LOG_INFO("stop tcpdump interface %S\n", menuElements[index].Text);
						net_if->second->TcpDumpStop();
				}
			}
		} else {
			const wchar_t * msgItems[] = { L"Tcpdump: ", GetMsg(MNoAnyTcpdumpTask), GetMsg(MOk) };
			psi.Message( psi.ModuleNumber, 0, NULL, msgItems, ARRAYSIZE(msgItems), 1);
		}
		return true;
	}

	if( controlState == 0 && key == VK_F5 ) {
		struct PanelInfo pi;
		psi.Control(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, (LONG_PTR)&pi);
		PluginPanelItem* ppi = (PluginPanelItem*)malloc(psi.Control(PANEL_ACTIVE, FCTL_GETPANELITEM, pi.CurrentItem, 0));
		if( ppi ) {
			psi.Control(PANEL_ACTIVE, FCTL_GETPANELITEM, pi.CurrentItem, (LONG_PTR)ppi);
			LOG_INFO("ppi->FindData.lpwszFileName %S\n", ppi->FindData.lpwszFileName);
		}
		do {

			if( FSF.LStricmp(ppi->FindData.lpwszFileName, L"..") == 0 )
				break;

			if( !(ppi->Flags & PPIF_USERDATA) || !ppi->UserData )
				break;

			assert( ((PluginUserData *)ppi->UserData)->size == sizeof(PluginUserData) );
			NetInterface * net_if = ((PluginUserData *)ppi->UserData)->net_if;
			assert( net_if->size == sizeof(NetInterface) );
			static struct FarDialogItem fdi_template[] = {
  //        Type        X1   Y1       X2              Y2          Focus    union    Flags             DefaultButton    PtrData             MaxLen
  /* 0*/ {DI_DOUBLEBOX, 3,   1,  DIALOG_WIDTH-4,       0,           0,      {0},       0,                    0,      L"Tcpdump:",                    0},
  /* 1*/ {DI_TEXT,      5,   2,       0,               0,           0,      {0},       0,                    0,      L"Interface:",                  0},
  /* 2*/ {DI_BUTTON,    16,  2,      37,               2,           0,      {0},       0,                    0,      L"",            MAX_INTERFACE_NAME_LEN},
  /* 3*/ {DI_CHECKBOX,  62,  2,       0,               0,           0,      {0},       0,                    0,      L"PROMISC",                     0},
  /* 4*/ {DI_TEXT,      5,   4,       0,               0,           0,      {0},       0,                    0,      L"File:",                       0},
  /* 5*/ {DI_EDIT,     11,   4,      72,               3,           0,      {0},       0,                    0,      L"",                            0},
  /* 6*/ {DI_TEXT,      5,   5,       0,               0,           0,      {0}, DIF_BOXCOLOR|DIF_SEPARATOR, 0,      L"",                            0},
  /* 7*/ {DI_BUTTON,    0,   6,       0,               0,           0,      {0}, DIF_CENTERGROUP,            0,      GetMsg(MOk),                    0},
  /* 8*/ {DI_BUTTON,    0,   6,       0,               0,           0,      {0}, DIF_CENTERGROUP,            0,      GetMsg(MCancel),                0}
                };
			unsigned int fdi_cnt = ARRAYSIZE(fdi_template);
			struct FarDialogItem * fdi = (struct FarDialogItem *)malloc(fdi_cnt * sizeof(FarDialogItem));
			if( !fdi )
				break;

			memmove(fdi, fdi_template, sizeof(fdi_template));

			fdi[WinTcpInterfaceButtonIndex].PtrData = ppi->FindData.lpwszFileName;

			fdi[WinTcpCaptionIndex].Y2 = ARRAYSIZE(fdi_template)-2;
			fdi[WinTcpOkButtonIndex].DefaultButton = 1;
			fdi[WinTcpOkButtonIndex].Focus = 1;

			HANDLE hDlg = psi.DialogInit(psi.ModuleNumber, -1, -1, DIALOG_WIDTH, ARRAYSIZE(fdi_template), L"Tcpdump", fdi, fdi_cnt, 0, 0, SettingDialogProc, (LONG_PTR)this);
			if (hDlg == INVALID_HANDLE_VALUE) {
				LOG_ERROR("DialogInit()\n");
				free(fdi);
				break;
			}

			SetCapFilePath(hDlg, WinTcpFileEditIndex, ppi->FindData.lpwszFileName);

			auto res = psi.DialogRun(hDlg);
			LOG_INFO("0. psi.DialogRun(hDlg) ret %u %u\n", res, WinTcpOkButtonIndex);
			if( res == WinTcpOkButtonIndex /*(fdi_cnt-2)*/ ) {
					const wchar_t interface[MAX_INTERFACE_NAME_LEN+1] = {0};
					psi.SendDlgMessage(hDlg, DM_GETTEXTPTR, WinTcpInterfaceButtonIndex, (ULONG_PTR)interface);
					auto net_if = nifs->find(interface);
					LOG_INFO("tcpdump interface %S net_if != nifs->end() %s\n", interface, net_if != nifs->end() ? "true":"false");
					if( net_if != nifs->end() ) {
						net_if->second->TcpDumpStart((const wchar_t *)psi.SendDlgMessage(hDlg, DM_GETCONSTTEXTPTR, WinTcpFileEditIndex, 0),
											bool(psi.SendDlgMessage(hDlg, DM_GETCHECK, WinTcpPromiscCheckboxIndex, 0)));
					}
			}

			free(fdi);

		} while(0);

		if( ppi )
			free(ppi);

		return TRUE;
	}

	if( controlState == 0 && key == VK_F4 ) {

		struct PanelInfo pi;
		psi.Control(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, (LONG_PTR)&pi);
		PluginPanelItem* ppi = (PluginPanelItem*)malloc(psi.Control(PANEL_ACTIVE, FCTL_GETPANELITEM, pi.CurrentItem, 0));
		if( ppi ) {
			psi.Control(PANEL_ACTIVE, FCTL_GETPANELITEM, pi.CurrentItem, (LONG_PTR)ppi);
			LOG_INFO("ppi->FindData.lpwszFileName %S\n", ppi->FindData.lpwszFileName);
		}

		do {

			if( FSF.LStricmp(ppi->FindData.lpwszFileName, L"..") == 0 )
				break;

			if( !(ppi->Flags & PPIF_USERDATA) || !ppi->UserData )
				break;

			assert( ((PluginUserData *)ppi->UserData)->size == sizeof(PluginUserData) );
			NetInterface * net_if = ((PluginUserData *)ppi->UserData)->net_if;
			assert( net_if->size == sizeof(NetInterface) );

		static struct FarDialogItem fdi_template_prefix[] = {
  //        Type        X1   Y1       X2              Y2          Focus    union    Flags             DefaultButton            PtrData             MaxLen
  /* 0*/ {DI_DOUBLEBOX, 3,   1,  DIALOG_WIDTH-4,       0,           0,      {0},       0,                    0,      0,                              0},
  /* 1*/ {DI_TEXT,      5,   2,       0,               0,           0,      {0},       0,                    0,      0,                              0},
  /* 2*/ {DI_EDIT,     11,   2,      27,               2,           0,      {0},       0,                    0,      0,                              0},

  /* 3*/ {DI_TEXT,     29,   2,       0,               0,           0,      {0},       0,                    0,      L"MAC:",                        0},
  /* 4*/ {DI_EDIT,     34,   2,      51,               2,           0,      {0},       0,                    0,      0,                              0},
  /* 5*/ {DI_TEXT,     53,   2,       0,               0,           0,      {0},       0,                    0,      0,                              0},


  /* 6*/ {DI_CHECKBOX,  6,   3,       0,               0,           0,      {0},       0,                    0,      L"UP",                          0},
  /* 7*/ {DI_CHECKBOX, 31,   3,       0,               0,           0,      {0},       0,                    0,      L"MULTICAST",                   0},
  /* 8*/ {DI_CHECKBOX, 56,   3,       0,               0,           0,      {0},       0,                    0,      L"ALLMULTICAST",                0},

  /* 9*/ {DI_CHECKBOX,  6,   4,       0,               0,           0,      {0},       0,                    0,      L"PROMISC",                     0},
  /*10*/ {DI_CHECKBOX, 31,   4,       0,               0,           0,      {0},       0,                    0,      L"NOARP(no MAC)",               0},
  /*11*/ {DI_CHECKBOX, 56,   4,       0,               0,           0,      {0},       DIF_HIDDEN,           0,      L"DEBUG",                       0},

  /*12*/ {DI_TEXT,      5,   5,       0,               0,           0,      {0},       0,                    0,      L"",                            0},
  /*13*/ {DI_TEXT,      55,  5,       0,               0,           0,      {0},       0,                    0,      L"mtu:",                        0},
  /*14*/ {DI_EDIT,      60,  5,      72,               5,           0,      {0},       0,                    0,      L"",                            0},

  /*15*/ {DI_TEXT,      13,  3,       0,               0,           0,      {0},       0,                    0,      L"carrier:",                    0},
  /*16*/ {DI_TEXT,      22,  3,       0,               0,           0,      {0},       0,                    0,      L"ON",                          0},

  /*17*/ {DI_TEXT,      5,   6,       0,               0,           0,      {0}, DIF_BOXCOLOR|DIF_SEPARATOR, 0,      L"",                            0},

};

	static struct FarDialogItem fdi_ip_template[] = {
  /*16*/ {DI_TEXT,      5,   7,       0,               0,           0,      {0},       0,                    0,      L"ip:",                         0},
  /*17*/ {DI_EDIT,      9,   7,      24,               7,           0,      {0},       0,                    0,      L"",                            0},

  /*18*/ {DI_TEXT,     26,   7,       0,               0,           0,      {0},       0,                    0,      L"mask:",                       0},
  /*19*/ {DI_EDIT,     32,   7,      47,               7,           0,      {0},       0,                    0,      L"",                            0},

  /*20*/ {DI_TEXT,     49,   7,       0,               0,           0,      {0},       0,                    0,      L"brcast:",                     0},
  /*21*/ {DI_EDIT,     57,   7,      72,               7,           0,      {0},       0,                    0,      L"",                            0},
	};

	static struct  FarDialogItem separator_template = {DI_TEXT, 5,   6, 0, 0, 0, {0}, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0};
	static struct  FarDialogItem stats_template[] = {
  /*20*/ {DI_TEXT,     13,   7,       0,               0,           0,      {0},       0,                    0,      L"bytes:       packets:      errors:", 0},
  /*20*/ {DI_TEXT,     5,    8,       0,               0,           0,      {0},       0,                    0,      L"recv:",                     0},
  /*20*/ {DI_TEXT,     11,   8,      20,               8,           0,      {0}, DIF_CENTERTEXT,             0,      L"",                          0},
  /*20*/ {DI_TEXT,     24,   8,      35,               8,           0,      {0}, DIF_CENTERTEXT,             0,      L"",                          0},
  /*20*/ {DI_TEXT,     37,   8,      48,               8,           0,      {0}, DIF_CENTERTEXT,             0,      L"",                          0},
  /*20*/ {DI_TEXT,     5,    8,       0,               0,           0,      {0},       0,                    0,      L"send:",                     0},
  /*20*/ {DI_TEXT,     11,   8,      20,               8,           0,      {0}, DIF_CENTERTEXT,             0,      L"",                          0},
  /*20*/ {DI_TEXT,     24,   8,      35,               8,           0,      {0}, DIF_CENTERTEXT,             0,      L"",                          0},
  /*20*/ {DI_TEXT,     37,   8,      48,               8,           0,      {0}, DIF_CENTERTEXT,             0,      L"",                          0},
  /*20*/ {DI_TEXT,     50,   8,       0,               0,           0,      {0},       0,                    0,      L"multicast:",                0},
  /*20*/ {DI_TEXT,     62,   8,      72,               8,           0,      {0}, DIF_CENTERTEXT,             0,      L"",                          0},
  /*20*/ {DI_TEXT,     50,   8,       0,               0,           0,      {0},       0,                    0,      L"collisions:",               0},
  /*20*/ {DI_TEXT,     62,   8,      72,               8,           0,      {0}, DIF_CENTERTEXT,             0,      L"",                          0},
	};

	static struct FarDialogItem fdi_ip6_template[] = {
  /*16*/ {DI_TEXT,      5,   7,       0,               0,           0,      {0},       0,                    0,      L"ip6:",                        0},
  /*17*/ {DI_EDIT,     10,   7,      39,               7,           0,      {0},       0,                    0,      L"",                            0},

  /*18*/ {DI_TEXT,     41,   7,       0,               0,           0,      {0},       0,                    0,      L"mask6(use /xx):",             0},
  /*19*/ {DI_EDIT,     57,   7,      62,               7,           0,      {0},       0,                    0,      L"",                            0},
	};

	static struct FarDialogItem fdi_template_suffix[] = {
  /*28*/ {DI_TEXT,      5,  18,       0,               0,           0,      {0}, DIF_BOXCOLOR|DIF_SEPARATOR, 0,      L"",                            0},
  /*29*/ {DI_BUTTON,    0,  19,       0,               0,           0,      {0}, DIF_CENTERGROUP,            0,      GetMsg(MOk),                    0},
  /*30*/ {DI_BUTTON,    0,  19,       0,               0,           0,      {0}, DIF_CENTERGROUP,            0,      GetMsg(MCancel),                0}
                };

		unsigned int ipcnt = (net_if->ip.size()+1) * ARRAYSIZE(fdi_ip_template);
		unsigned int ip6cnt = (net_if->ip6.size()+1) * ARRAYSIZE(fdi_ip6_template);
		unsigned int fdi_cnt = ARRAYSIZE(fdi_template_prefix) + ARRAYSIZE(stats_template) + ARRAYSIZE(fdi_template_suffix) + \
				ipcnt + ip6cnt + 2;

		struct FarDialogItem * fdi = (struct FarDialogItem *)malloc(fdi_cnt * sizeof(FarDialogItem));
		if( !fdi )
			break;

		memmove(fdi, fdi_template_prefix, sizeof(fdi_template_prefix));

		std::wstring caption(GetMsg(MIfConfigTitle));
		caption += ppi->FindData.lpwszFileName;
		caption += L" ";
		/* 0 */ fdi[WinCaptionIndex].PtrData = caption.c_str();

		/* 1 */ fdi[WinNameTextIndex].PtrData = GetMsg(MIfConfigName);

		/* 2 */ fdi[WinIfNameEditIndex].PtrData = ppi->FindData.lpwszFileName;
#if defined(__APPLE__)
		fdi[WinIfNameEditIndex].Flags |= DIF_DISABLE;

		fdi[WinMulticastFlagIndex].Flags |= DIF_DISABLE;
		fdi[WinAllMulticastFlagIndex].Flags |= DIF_DISABLE;
		fdi[WinNoARPFlagIndex].Flags |= DIF_DISABLE;
		fdi[WinDebugFlagIndex].Flags |= DIF_DISABLE;
#endif
		/* 4 */ fdi[WinIfMACEditIndex].PtrData = ppi->CustomColumnData[CColumnDataMacIndex];

		std::wstring permanent_mac(L"(");
		permanent_mac += net_if->permanent_mac;
		permanent_mac += L")";
		/* 5 */ fdi[WinPermMACTextIndex].PtrData = permanent_mac.c_str();

		std::wstring flags(L"Flags: ");
		if( ppi->Flags & PPIF_USERDATA && ppi->UserData ) {

			fdi[WinUpFlagIndex].Selected = net_if->IsUp();
			fdi[WinUpFlagIndex].Flags |= DIF_SETCOLOR | BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | (net_if->IsUp() ? FOREGROUND_GREEN:FOREGROUND_RED);

			fdi[WinCarrierOnOffTextIndex].PtrData = net_if->IsCarrierOn() ? L"ON":L"OFF";
			fdi[WinCarrierOnOffTextIndex].Flags |= DIF_SETCOLOR | BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | (net_if->IsCarrierOn() ? FOREGROUND_GREEN:FOREGROUND_RED);

			fdi[WinMulticastFlagIndex].Selected = net_if->IsMulticast();
			fdi[WinAllMulticastFlagIndex].Selected = net_if->IsAllmulticast();
			fdi[WinPromiscFlagIndex].Selected = net_if->IsPromisc();
			fdi[WinNoARPFlagIndex].Selected = net_if->IsNoARP();
			fdi[WinDebugFlagIndex].Selected = net_if->IsDebug();

			flags += net_if->IsLoopback() ? L"LOOPBACK, ":L"";
			flags += net_if->IsBroadcast() ? L"BROADCAST, ":L"";
			flags += net_if->IsPointToPoint() ? L"POINTOPOINT, ":L"";
			flags += net_if->IsNotrailers() ? L"NOTRAILERS, ":L"";
			flags += net_if->IsDormant() ? L"DORMANT, ":L"";
			flags += net_if->IsEcho() ? L"ECHO, ":L"";

			wchar_t max32[sizeof("0xFFFFFFFF")] = {0};
			if( FSF.snprintf(max32, ARRAYSIZE(max32), L"0x%08X", net_if->ifa_flags) > 0 ) {
				flags += L"(";
				flags += max32;
				flags += L")";
			}
			LOG_INFO("FLAGS: %S\n", flags.c_str());
			/* 12 */ fdi[WinIfFlagsIndex].PtrData = flags.c_str();
		}

		fdi[WinMtuEditIndex].PtrData = ppi->CustomColumnData[CColumnDataMtuIndex];

		std::wstring _tmp;

		struct FarDialogItem * fdi_ip = fdi + ARRAYSIZE(fdi_template_prefix);
		int Y = WIN_IP_INITIAL_Y;

		for( const auto& [ip, ipinfo] : net_if->ip ) {

			for( auto& fdi_position : fdi_ip_template ) {
				fdi_position.Y1 = Y;
				fdi_position.Y2 = Y;
			}

			memmove(fdi_ip, fdi_ip_template, sizeof(fdi_ip_template));

			fdi_ip[WinIpEditIndex].PtrData = ip.c_str();
			fdi_ip[WinIpMaskEditIndex].PtrData = ipinfo.netmask.c_str();

			if( net_if->IsBroadcast() || net_if->IsPointToPoint() ) {
				if( net_if->IsBroadcast() ) {
					fdi_ip[WinIpBroadcastEditIndex].PtrData = ipinfo.broadcast.c_str();
				} else {
					fdi_ip[WinIpPointToPointTextIndex].PtrData = L"pt2pt: ";
					fdi_ip[WinIpBroadcastEditIndex].PtrData = ipinfo.point_to_point.c_str();
				}
			} else {
				fdi_ip[WinIpBroadcastTextIndex].Flags |= DIF_HIDDEN;
				fdi_ip[WinIpBroadcastEditIndex].Flags |= DIF_HIDDEN;
			}

			fdi_ip += ARRAYSIZE(fdi_ip_template);
			Y++;
		}

		for( auto& fdi_position : fdi_ip_template ) {
			fdi_position.Y1 = Y;
			fdi_position.Y2 = Y;
		}

		memmove(fdi_ip, fdi_ip_template, sizeof(fdi_ip_template));
		if( net_if->IsPointToPoint() )
			fdi_ip[WinIpPointToPointTextIndex].PtrData = L"pt2pt: ";
		else if( net_if->IsLoopback() ) {
			fdi_ip[WinIpBroadcastTextIndex].Flags |= DIF_HIDDEN;
			fdi_ip[WinIpBroadcastEditIndex].Flags |= DIF_HIDDEN;
		}
		fdi_ip += ARRAYSIZE(fdi_ip_template);
		Y++;

		memmove(fdi_ip, &separator_template, sizeof(separator_template));
		fdi_ip->Y1 = Y;
		Y++;
		fdi_ip++;

		for( const auto& [ip, ipinfo] : net_if->ip6 ) {

			memmove(fdi_ip, fdi_ip6_template, sizeof(fdi_ip6_template));

			fdi_ip[WinIp6TextIndex].Y1 = Y;
			fdi_ip[WinIp6EditIndex].Y1 = Y;
			fdi_ip[WinIp6EditIndex].Y2 = Y;
			//Y++;
			fdi_ip[WinIp6MaskTextIndex].Y1 = Y;
			fdi_ip[WinIp6MaskEditIndex].Y1 = Y;
			fdi_ip[WinIp6MaskEditIndex].Y2 = Y;

			fdi_ip[WinIp6EditIndex].PtrData = ip.c_str();
			fdi_ip[WinIp6MaskEditIndex].PtrData = ipinfo.netmask.c_str();

			fdi_ip += ARRAYSIZE(fdi_ip6_template);
			Y++;
		}

		memmove(fdi_ip, fdi_ip6_template, sizeof(fdi_ip6_template));
		fdi_ip[WinIp6TextIndex].Y1 = Y;
		fdi_ip[WinIp6EditIndex].Y1 = Y;
		fdi_ip[WinIp6EditIndex].Y2 = Y;
		//Y++;
		fdi_ip[WinIp6MaskTextIndex].Y1 = Y;
		fdi_ip[WinIp6MaskEditIndex].Y1 = Y;
		fdi_ip[WinIp6MaskEditIndex].Y2 = Y;

		fdi_ip += ARRAYSIZE(fdi_ip6_template);
		Y++;

		//separator_template.Y1 = Y;
		memmove(fdi_ip, &separator_template, sizeof(separator_template));
		fdi_ip->Y1 = Y;
		Y++;
		fdi_ip++;

		memmove(fdi_ip, stats_template, sizeof(stats_template));
		fdi_ip[WinStatRecvSendCaptionTextIndex].Y1 = Y;
		Y++;
		fdi_ip[WinStatRecvTextIndex].Y1 = Y;
		fdi_ip[WinStatRecvBytesIndex].Y1 = Y;
		fdi_ip[WinStatRecvBytesIndex].Y2 = Y;
		fdi_ip[WinStatRecvPacketsIndex].Y1 = Y;
		fdi_ip[WinStatRecvPacketsIndex].Y2 = Y;
		fdi_ip[WinStatRecvErrorsIndex].Y1 = Y;
		fdi_ip[WinStatRecvErrorsIndex].Y2 = Y;


		fdi_ip[WinStatMulticastTextIndex].Y1 = Y;
		fdi_ip[WinStatMulticastIndex].Y1 = Y;
		fdi_ip[WinStatMulticastIndex].Y2 = Y;

		Y++;
		fdi_ip[WinStatSendTextIndex].Y1 = Y;
		fdi_ip[WinStatSendBytesIndex].Y1 = Y;
		fdi_ip[WinStatSendBytesIndex].Y2 = Y;
		fdi_ip[WinStatSendPacketsIndex].Y1 = Y;
		fdi_ip[WinStatSendPacketsIndex].Y2 = Y;
		fdi_ip[WinStatSendErrorsIndex].Y1 = Y;
		fdi_ip[WinStatSendErrorsIndex].Y2 = Y;

		fdi_ip[WinStatCollisionsTextIndex].Y1 = Y;
		fdi_ip[WinStatCollisionsIndex].Y1 = Y;
		fdi_ip[WinStatCollisionsIndex].Y2 = Y;

		fdi_ip[WinStatRecvBytesIndex].PtrData = ppi->CustomColumnData[CColumnDataRecvBytesIndex];

		fdi_ip[WinStatRecvPacketsIndex].PtrData = ppi->CustomColumnData[CColumnDataRecvPktsIndex];
		fdi_ip[WinStatRecvErrorsIndex].PtrData = ppi->CustomColumnData[CColumnDataRecvErrsIndex];

		fdi_ip[WinStatSendBytesIndex].PtrData = ppi->CustomColumnData[CColumnDataSendBytesIndex];
		fdi_ip[WinStatSendPacketsIndex].PtrData = ppi->CustomColumnData[CColumnDataSendPktsIndex];
		fdi_ip[WinStatSendErrorsIndex].PtrData = ppi->CustomColumnData[CColumnDataSendErrsIndex];

		fdi_ip[WinStatMulticastIndex].PtrData = ppi->CustomColumnData[CColumnDataMulticastIndex];
		fdi_ip[WinStatCollisionsIndex].PtrData = ppi->CustomColumnData[CColumnDataCollisionsIndex];


		fdi_ip += ARRAYSIZE(stats_template);


		assert( fdi + (fdi_cnt - ARRAYSIZE(fdi_template_suffix)) == fdi_ip );
		memmove(fdi_ip, fdi_template_suffix, sizeof(fdi_template_suffix));

		Y++;
		fdi_ip[WinFooterSeparatorIndex].Y1 = Y;
		Y++;
		fdi_ip[WinFooterOkIndex].Y1 = Y;
		fdi_ip[WinFooterCancelIndex].Y1 = Y;
	
		Y++;
		fdi[WinCaptionIndex].Y2 = Y;

		fdi[fdi_cnt-2].DefaultButton = 1;
		fdi[2].Focus = 1;

		LOG_INFO("INIT DIALOG ...\n");

		Y++;
		Y++;
		HANDLE hDlg = psi.DialogInit(psi.ModuleNumber, -1, -1, DIALOG_WIDTH, Y, L"Config", fdi, fdi_cnt, 0, 0, NULL, 0);
		if (hDlg == INVALID_HANDLE_VALUE) {
			LOG_ERROR("DialogInit()\n");
			free((void *)fdi);
			break;
		}

		if( psi.DialogRun(hDlg) == (fdi_cnt-2) ) {

			// ifname
			const wchar_t * ifname = (const wchar_t *)psi.SendDlgMessage(hDlg, DM_GETCONSTTEXTPTR, WinIfNameEditIndex, 0);
			change |= net_if->SetInterfaceName(ifname);

			// mac address
			const wchar_t * mac = (const wchar_t *)psi.SendDlgMessage(hDlg, DM_GETCONSTTEXTPTR, WinIfMACEditIndex, 0);
			change |= net_if->SetMac(mac);

			// flags
			change |= net_if->SetUp(bool(psi.SendDlgMessage(hDlg, DM_GETCHECK, WinUpFlagIndex, 0)));
			change |= net_if->SetMulticast(bool(psi.SendDlgMessage(hDlg, DM_GETCHECK, WinMulticastFlagIndex, 0)));
			change |= net_if->SetAllmulticast(bool(psi.SendDlgMessage(hDlg, DM_GETCHECK, WinAllMulticastFlagIndex, 0)));
			change |= net_if->SetPromisc(bool(psi.SendDlgMessage(hDlg, DM_GETCHECK, WinPromiscFlagIndex, 0)));
			change |= net_if->SetNoARP(bool(psi.SendDlgMessage(hDlg, DM_GETCHECK, WinNoARPFlagIndex, 0)));

			// mtu
			const wchar_t * mtu = (const wchar_t *)psi.SendDlgMessage(hDlg, DM_GETCONSTTEXTPTR, WinMtuEditIndex, 0);
			change |= net_if->SetMtu(FSF.atoi(mtu));

			// ip
			int indexOffset = ARRAYSIZE(fdi_template_prefix);
			int indexOffsetLast = indexOffset + ipcnt;
			fdi_ip = fdi + ARRAYSIZE(fdi_template_prefix);

			while( indexOffset < indexOffsetLast ) {

				const wchar_t * ip = (const wchar_t *)psi.SendDlgMessage(hDlg, DM_GETCONSTTEXTPTR, indexOffset+WinIpEditIndex, 0);
				const wchar_t * mask = (const wchar_t *)psi.SendDlgMessage(hDlg, DM_GETCONSTTEXTPTR, indexOffset+WinIpMaskEditIndex, 0);
				const wchar_t * bcip = (const wchar_t *)psi.SendDlgMessage(hDlg, DM_GETCONSTTEXTPTR, indexOffset+WinIpBroadcastEditIndex, 0);

				LOG_INFO("%S: ip %S to %S\n", ifname, fdi_ip[WinIpEditIndex].PtrData, ip);
				LOG_INFO("%S: ip mask %S to %S\n", ifname, fdi_ip[WinIpMaskEditIndex].PtrData, mask);
				LOG_INFO("%S: ip broadcast %S to %S\n", ifname, fdi_ip[WinIpBroadcastEditIndex].PtrData, bcip);

				if( *ip == 0 ) {
					if( *(const wchar_t *)fdi_ip[WinIpEditIndex].PtrData != 0 )
						change |= net_if->DeleteIp(fdi_ip[WinIpEditIndex].PtrData);
				} else	if( *(const wchar_t *)fdi_ip[WinIpEditIndex].PtrData == 0 ) {
        				change |= net_if->AddIp(ip, mask, bcip);
        			} else	if( FSF.LStricmp((const wchar_t *)fdi_ip[WinIpEditIndex].PtrData, ip) != 0 ||
				    FSF.LStricmp((const wchar_t *)fdi_ip[WinIpMaskEditIndex].PtrData, mask) != 0 ||
				    FSF.LStricmp((const wchar_t *)fdi_ip[WinIpBroadcastEditIndex].PtrData, bcip) != 0 ) {
					change |= net_if->ChangeIp(fdi_ip[WinIpEditIndex].PtrData, ip, mask, bcip);
				}

				indexOffset += WinIpMaxIndex;
				fdi_ip += ARRAYSIZE(fdi_ip_template);
			}

			fdi_ip++;
			indexOffset++;

			// ip6
			indexOffsetLast = indexOffset + ip6cnt;
			while( indexOffset < indexOffsetLast ) {
				const wchar_t * ip = (const wchar_t *)psi.SendDlgMessage(hDlg, DM_GETCONSTTEXTPTR, indexOffset+WinIp6EditIndex, 0);
				const wchar_t * mask = (const wchar_t *)psi.SendDlgMessage(hDlg, DM_GETCONSTTEXTPTR, indexOffset+WinIp6MaskEditIndex, 0);

				assert(mask != 0);

				if( *mask == 0x2F )
					mask++;

				LOG_INFO("%S: ip6 %S to %S\n", ifname, fdi_ip[WinIp6EditIndex].PtrData, ip);
				LOG_INFO("%S: ip6 mask %S to %S\n", ifname, fdi_ip[WinIp6MaskEditIndex].PtrData, mask);

				if( *ip == 0 ) {
					if( *(const wchar_t *)fdi_ip[WinIp6EditIndex].PtrData != 0 )
						change |= net_if->DeleteIp6(fdi_ip[WinIp6EditIndex].PtrData);
				} else	if( *(const wchar_t *)fdi_ip[WinIp6EditIndex].PtrData == 0 ) {
        				change |= net_if->AddIp6(ip, (*mask == 0) ? L"64":mask);
				} else	if( FSF.LStricmp((const wchar_t *)fdi_ip[WinIp6EditIndex].PtrData, ip) != 0 ||
				    	    FSF.LStricmp((const wchar_t *)fdi_ip[WinIp6MaskEditIndex].PtrData, mask) != 0 ) {
					change |= net_if->ChangeIp6(fdi_ip[WinIp6EditIndex].PtrData, ip, mask);
				}
				indexOffset += WinIp6MaxIndex;
				fdi_ip += ARRAYSIZE(fdi_ip6_template);
			}
		}

		psi.DialogFree(hDlg);

		free(fdi);

		} while(0);

		if( ppi )
			free(ppi);

	}

	if( change ) {
		nifs->Clear();
		psi.Control(hPlugin, FCTL_UPDATEPANEL, TRUE, 0);
		psi.Control(hPlugin, FCTL_REDRAWPANEL, 0, 0);
		return TRUE;
	}

	if( controlState == 0 && (key > VK_F1 && key < VK_F10) )
		return TRUE;

	return FALSE;
}
