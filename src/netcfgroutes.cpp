#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include "netcfgplugin.h"
#include "netcfglng.h"
#include "fardialog.h"

#include <utils.h>

#include <assert.h>

#include <common/log.h>
#include <common/errname.h>
#include <common/netutils.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <memory>
#include <map>

extern const char * LOG_FILE;
#define LOG_SOURCE_FILE "netcfgroutes.cpp"

NetcfgIpRoute::NetcfgIpRoute(uint32_t index_, std::deque<IpRouteInfo> & inet_, std::map<uint32_t, std::wstring> & ifs_):
	FarPanel(index_),
	inet(inet_),
	ifs(ifs_)
{
	#if !defined(__APPLE__) && !defined(__FreeBSD__)
	tablemodes = std::make_unique<PanelMode[]>(PanelModeMax);
	memset(tablemodes.get(), 0, sizeof(PanelMode)*PanelModeMax);

	memset(&new_enc, 0, sizeof(new_enc));
	memset(&new_nexthop_enc, 0, sizeof(new_nexthop_enc));

	static const wchar_t * const columnTitles[2] = {L"Table:", L"Total rules:"};
	tablemodes[4].ColumnTypes = tablemodes[4].StatusColumnTypes = \
	tablemodes[5].ColumnTypes = tablemodes[5].StatusColumnTypes = L"N,C0";
	tablemodes[4].ColumnWidths = tablemodes[4].StatusColumnWidths = \
	tablemodes[5].ColumnWidths = tablemodes[5].StatusColumnWidths = L"10,0";
	tablemodes[4].ColumnTitles = tablemodes[5].ColumnTitles = (const wchar_t* const*)&columnTitles;
	tablemodes[5].FullScreen = TRUE;

	table = ROUTE_TABLE_DIRS;
	dirIndex = 0;
	#endif

	rt = nullptr;

	LOG_INFO("index_ %u this %p\n", index_, this);
}

NetcfgIpRoute::~NetcfgIpRoute()
{
	LOG_INFO("\n");
}

const int DIALOG_WIDTH = 80;

bool NetcfgIpRoute::DeleteIpRoute(void)
{
	LOG_INFO("\n");
	bool change = false;

	PanelInfo pi = {0};
	GetPanelInfo(pi);

	do {
		if( !pi.SelectedItemsNumber )
			break;

		FarDlgConstructor fdc(L"Delete route(s):", DIALOG_WIDTH);
		fdc.SetHelpTopic(L"DeleteRoute");

		std::vector<IpRouteInfo *> delroutes;

		static const DlgConstructorItem route = {DI_TEXT, true, 5, 0, 0, {0}};

		for( size_t i = 0; i < pi.SelectedItemsNumber; i++ ) {
			auto ppi = GetSelectedPanelItem(i);
			if( ppi && ppi->Flags & PPIF_USERDATA ) {

        			assert(ppi->UserData && ((PluginUserData *)ppi->UserData)->size == sizeof(PluginUserData));

				auto rt = ((PluginUserData *)ppi->UserData)->data.inet;
				delroutes.push_back(rt);

				auto off = fdc.Append(&route);
				std::wstring _rt(std::to_wstring(i+1)+L". ");
				_rt += rt->destIpandMask.empty() ? L"default":rt->destIpandMask;
				if( !rt->gateway.empty() )
					_rt += L" via " + rt->gateway;
				if( !rt->iface.empty() )
					_rt += L" dev " + rt->iface;

				#if !defined(__APPLE__) && !defined(__FreeBSD__)
				if( rt->osdep.table )
					_rt += L" table " + towstr(rtruletable(rt->osdep.table));
				#endif

				fdc.SetText(off, _rt.c_str(), true);
			}
			FreePanelItem(ppi);
		}

		if( delroutes.size() == 0 )
			break;

		auto offSufix = fdc.AppendOkCancel();

		fdc.SetDefaultButton(offSufix + WinSuffixOkIndex);
		fdc.SetFocus(offSufix + WinSuffixOkIndex);

		FarDialog dlg(&fdc);

		if( dlg.Run() != offSufix + WinSuffixOkIndex )
			break;

		for( auto rt : delroutes )
			change |= rt->DeleteIpRoute();

	} while(0);

	return change;
}

void NetcfgIpRoute::SelectInterface(HANDLE hDlg, uint32_t setIndex)
{
	int if_count = ifs.size()+1, index = 0;
	auto menuElements = std::make_unique<FarMenuItem[]>(if_count);
	memset(menuElements.get(), 0, sizeof(FarMenuItem)*if_count);
	for( const auto& [if_index, name] : ifs ) {
		menuElements[index].Text = name.c_str();
		index++;
	}

	menuElements[index].Text = L"";
	index++;

	menuElements[0].Selected = 1;
	index = NetCfgPlugin::psi.Menu(NetCfgPlugin::psi.ModuleNumber, -1, -1, 0, FMENU_WRAPMODE|FMENU_AUTOHIGHLIGHT, \
			 L"Select interface:", 0, L"hrd", nullptr, nullptr, menuElements.get(), if_count);

	if( index >= 0 && index < if_count )
		NetCfgPlugin::psi.SendDlgMessage(hDlg, DM_SETTEXTPTR, setIndex, (LONG_PTR)menuElements[index].Text);

	return;
}

void NetcfgIpRoute::SelectFamily(HANDLE hDlg, uint32_t setIndex)
{
	static const wchar_t * menuElements[] = {
		L"inet",
		L"inet6",
		L"link",
		L"mpls",
		L"bridge",
		L""
		};
	Select(hDlg, &menuElements[0], ARRAYSIZE(menuElements), setIndex);
}

const int EDIT_ENCAP_DIALOG_WIDTH = 80;

enum {
	WinEditIpEncapCaptionTextIndex,
	WinEditIpEncapTypeTextIndex,
	WinEditIpEncapTypeButtonIndex,

	WinEditIpEncapIpIdTextIndex,
	WinEditIpEncapIpIdEditIndex,
	WinEditIpEncapIpTTLTextIndex,
	WinEditIpEncapIpTTLEditIndex,
	WinEditIpEncapIpTosTextIndex,
	WinEditIpEncapIpTosEditIndex,
	WinEditIpEncapIpFlagsTextIndex,

	WinEditIpEncapMPLSLabelTextIndex,
	WinEditIpEncapMPLSLabelEditIndex,
	WinEditIpEncapMPLSTTLTextIndex,
	WinEditIpEncapMPLSTTLEditIndex,

	WinEditIpEncapSeparatorTextIndex,
	WinEditIpEncapIpSrcTextIndex,
	WinEditIpEncapIpSrcEditIndex,
	WinEditIpEncapIpDstTextIndex,
	WinEditIpEncapIpDstEditIndex,
	WinEditIpEncapIpCsumCheckBoxIndex,
	WinEditIpEncapIpKeyCheckBoxIndex,
	WinEditIpEncapIpSeqCheckBoxIndex,

	WinEditIpEncapSeparator2TextIndex,

	WinEditIpEncapIpOptsTextIndex,
	WinEditIpEncapIpOptsButtonIndex,

	WinEditIpEncapIpOptsGeneveEditIndex,
	WinEditIpEncapIpOptsVxlanEditIndex,
	WinEditIpEncapIpOptsErspanEditIndex,

	WinEditIpEncapMaxIndex
};

LONG_PTR WINAPI EditEncapDialogProc(HANDLE hDlg, int msg, int param1, LONG_PTR param2)
{
	static NetcfgIpRoute * rtCfg = 0;

	LOG_INFO("MSG: 0x%08X, param1 %u\n", msg, param1);

	switch( msg ) {

	case DN_INITDIALOG:
		rtCfg = (NetcfgIpRoute *)param2;
		break;
	case DM_CLOSE:
		rtCfg = 0;
		break;
	case DN_BTNCLICK:
		assert( rtCfg != 0 );
		switch( param1 ) {
		case WinEditIpEncapTypeButtonIndex:
		{
			static const wchar_t * menuElements[] = {  // enum lwtunnel_encap_types {    
				L"",		                   // 	LWTUNNEL_ENCAP_NONE,      
				L"mpls",	                   // 	LWTUNNEL_ENCAP_MPLS,      
				L"ip",		                   // 	LWTUNNEL_ENCAP_IP,        
				L"ila",		                   // 	LWTUNNEL_ENCAP_ILA,       
				L"ip6",		                   // 	LWTUNNEL_ENCAP_IP6,       
				L"seg6",	                   //   LWTUNNEL_ENCAP_SEG6,      
				L"bpf",                            //   LWTUNNEL_ENCAP_BPF,       
				L"seg6local",                      //   LWTUNNEL_ENCAP_SEG6_LOCAL,
				L"rpl",                            //   LWTUNNEL_ENCAP_RPL,       
				L"ioam6",                          //   LWTUNNEL_ENCAP_IOAM6,     
				L"xfrm",                           //   LWTUNNEL_ENCAP_XFRM,      
				L"Enter:"                          //   __LWTUNNEL_ENCAP_MAX,     
				};                                 //   };                           
			auto index = rtCfg->SelectNum(hDlg, &menuElements[0], ARRAYSIZE(menuElements), L"Encap type:", WinEditIpEncapTypeButtonIndex);
			if( index < 0 )
				return true;
			switch( index ) {
			case LWTUNNEL_ENCAP_MPLS:
				HideDialogItems(hDlg, WinEditIpEncapIpIdTextIndex, WinEditIpEncapIpFlagsTextIndex);
				HideDialogItems(hDlg, WinEditIpEncapSeparatorTextIndex, WinEditIpEncapIpOptsErspanEditIndex);
				UnhideDialogItems(hDlg, WinEditIpEncapMPLSLabelTextIndex, WinEditIpEncapMPLSTTLEditIndex);
				break;
			case LWTUNNEL_ENCAP_IP:
			case LWTUNNEL_ENCAP_IP6:
				HideDialogItems(hDlg, WinEditIpEncapMPLSLabelTextIndex, WinEditIpEncapMPLSTTLEditIndex);
				UnhideDialogItems(hDlg, WinEditIpEncapIpIdTextIndex, WinEditIpEncapIpFlagsTextIndex);
				UnhideDialogItems(hDlg, WinEditIpEncapSeparatorTextIndex, WinEditIpEncapIpOptsErspanEditIndex);
				break;
			default:
				HideDialogItems(hDlg, WinEditIpEncapIpIdTextIndex, WinEditIpEncapIpOptsErspanEditIndex);
				break;
			};
			return true;
		}
		break;
		case WinEditIpEncapIpOptsButtonIndex:
		{
			static const wchar_t * menuElements[] = {
				L"geneve_opts",
				L"vxlan_opts",
				L"erspan_opts",
				L""
				};

			auto index = rtCfg->Select(hDlg, &menuElements[0], ARRAYSIZE(menuElements), WinEditIpEncapIpOptsButtonIndex);
			if( index < 0 )
				return true;
			HideDialogItems(hDlg, WinEditIpEncapIpOptsGeneveEditIndex, WinEditIpEncapIpOptsErspanEditIndex);
			if( index < 3 )
				UnhideDialogItems(hDlg, index+WinEditIpEncapIpOptsGeneveEditIndex, index+WinEditIpEncapIpOptsGeneveEditIndex);
			return true;
		}
		};
	};

	return NetCfgPlugin::psi.DefDlgProc(hDlg, msg, param1, param2);
}

void NetcfgIpRoute::SelectEncap(HANDLE hDlg, Encap & enc, uint32_t setIndex)
{
	static const DlgConstructorItem dialog[] = {
		//  Type       NewLine X1              X2         Flags                PtrData
		{DI_DOUBLEBOX, false,  DEFAUL_X_START, EDIT_ENCAP_DIALOG_WIDTH-4,  0,     {.ptrData = L"Encap:"}},
		{DI_TEXT,      true,   5,              0,           0,                    {.ptrData = L"type:"}},
		{DI_BUTTON,    false,  11,             0,           0,                    {0}},

		{DI_TEXT,      false,  19,             0,           0,                    {.ptrData = L"id:"}},
		{DI_EDIT,      false,  23,            38,           DIF_LEFTTEXT,         {0}},
		{DI_TEXT,      false,  41,             0,           0,                    {.ptrData = L"ttl:"}},
		{DI_EDIT,      false,  46,            49,           DIF_LEFTTEXT,         {0}},
		{DI_TEXT,      false,  51,             0,           0,                    {.ptrData = L"tos:"}},
		{DI_EDIT,      false,  56,            59,           DIF_LEFTTEXT,         {0}},
		{DI_TEXT,      false,  61,             0,           0,                    {0}},

		{DI_TEXT,      false,  20,             0,           0,                    {.ptrData = L"label:"}},
		{DI_EDIT,      false,  27,            44,           DIF_LEFTTEXT,         {0}},
		{DI_TEXT,      false,  46,             0,           0,                    {.ptrData = L"ttl:"}},
		{DI_EDIT,      false,  51,            54,           DIF_LEFTTEXT,         {.ptrData = 0}},

		{DI_TEXT,      true,    5,             0,           0,                    {0}},
		{DI_TEXT,      true,    5,             0,           0,                    {.ptrData = L"src:"}},
		{DI_EDIT,      false,  10,            26,           DIF_LEFTTEXT,         {0}},
		{DI_TEXT,      false,  28,             0,           0,                    {.ptrData = L"dst:"}},
		{DI_EDIT,      false,  33,            49,           DIF_LEFTTEXT,         {0}},

		{DI_CHECKBOX,  false,  51,             0,            0,                   {.ptrData = L"csum"}},
		{DI_CHECKBOX,  false,  60,             0,            0,                   {.ptrData = L"key"}},
		{DI_CHECKBOX,  false,  68,             0,            0,                   {.ptrData = L"seq"}},

		{DI_TEXT,      true,    5,             0,           0,                    {0}},
		{DI_TEXT,      true,    5,             0,           0,                    {.ptrData = L"opts:"}},
		{DI_BUTTON,    false,  11,             0,           0,                    {0}},

		{DI_EDIT,      false,  27,             74,          DIF_LEFTTEXT,                    {0}},
		{DI_EDIT,      false,  27,             74,          DIF_LEFTTEXT,                    {0}},
		{DI_EDIT,      false,  27,             74,          DIF_LEFTTEXT,                    {0}},

		{DI_ENDDIALOG, 0}
	};

	FarDlgConstructor fdc(&dialog[0]);

	fdc.SetText(WinEditIpEncapTypeButtonIndex, towstr(lwtunnelencaptype(enc.type)).c_str(), true);
	switch( enc.type ) {
	case LWTUNNEL_ENCAP_MPLS:

		fdc.HideItems(WinEditIpEncapIpIdTextIndex, WinEditIpEncapIpFlagsTextIndex);
		fdc.HideItems(WinEditIpEncapSeparatorTextIndex, WinEditIpEncapIpOptsErspanEditIndex);

		if( enc.data.mpls.valid.dst )
			fdc.SetText(WinEditIpEncapMPLSLabelEditIndex, towstr(mpls_ntop((const struct mpls_label *)enc.data.mpls.dst)).c_str(), true);
		if( enc.data.mpls.valid.ttl )
			fdc.SetCountText(WinEditIpEncapMPLSTTLEditIndex, enc.data.mpls.ttl);
		break;

	// TODO: this is hack?
	case LWTUNNEL_ENCAP_IP6:
	case LWTUNNEL_ENCAP_IP:
	{
		const size_t maxlen = INET6_ADDRSTRLEN > INET_ADDRSTRLEN ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN;
		char s[maxlen+1] = {0};

		fdc.HideItems(WinEditIpEncapMPLSLabelTextIndex, WinEditIpEncapMPLSTTLEditIndex);

		if( enc.data.ip.valid.id )
			fdc.SetCountText(WinEditIpEncapIpIdEditIndex, ntohll(enc.data.ip.id));

		if( enc.data.ip.valid.ttl )
			fdc.SetCountText(WinEditIpEncapIpTTLEditIndex, enc.data.ip.ttl);

		if( enc.data.ip.valid.hoplimit )
			fdc.SetCountText(WinEditIpEncapIpTTLEditIndex, enc.data.ip.hoplimit);

		if( enc.data.ip.valid.tos )
			fdc.SetCountText(WinEditIpEncapIpTosEditIndex, enc.data.ip.tos);

		if( enc.data.ip.valid.tc )
			fdc.SetCountText(WinEditIpEncapIpTosEditIndex, enc.data.ip.tc);

		if( enc.data.ip.valid.src ) {
			if( inet_ntop(enc.data.ip.family, &enc.data.ip.src, s, maxlen) )
				fdc.SetText(WinEditIpEncapIpSrcEditIndex, towstr(s).c_str(), true);
		}
		if( enc.data.ip.valid.dst ) {
			if( inet_ntop(enc.data.ip.family, &enc.data.ip.dst, s, maxlen) )
				fdc.SetText(WinEditIpEncapIpDstEditIndex, towstr(s).c_str(), true);
		}
		if( enc.data.ip.valid.flags ) {
			std::wstring flags(L"flags:");
			wchar_t max16[sizeof("0xFFFF")] = {0};
			if( NetCfgPlugin::FSF.snprintf(max16, ARRAYSIZE(max16), L"0x%04X", enc.data.ip.flags) > 0 ) {
				flags += L"(";
				flags += max16;
				flags += L")";
			}
			fdc.SetText(WinEditIpEncapIpFlagsTextIndex, flags.c_str(), true);
			fdc.SetSelected(WinEditIpEncapIpCsumCheckBoxIndex, enc.data.ip.flags & TUNNEL_CSUM);
			fdc.SetSelected(WinEditIpEncapIpKeyCheckBoxIndex, enc.data.ip.flags & TUNNEL_KEY);
			fdc.SetSelected(WinEditIpEncapIpSeqCheckBoxIndex, enc.data.ip.flags & TUNNEL_SEQ);

			if( enc.data.ip.flags & TUNNEL_GENEVE_OPT && enc.data.ip.valid.geneve ) {
				fdc.OrFlags(WinEditIpEncapIpOptsVxlanEditIndex, DIF_HIDDEN);
				fdc.OrFlags(WinEditIpEncapIpOptsErspanEditIndex, DIF_HIDDEN);
				fdc.SetText(WinEditIpEncapIpOptsButtonIndex, L"geneve_opts");
				fdc.SetText(WinEditIpEncapIpOptsGeneveEditIndex, towstr(genevestring(enc.data.ip.total_geneves, enc.data.ip.geneve)).c_str(), true);
			} else if( enc.data.ip.flags & TUNNEL_VXLAN_OPT && enc.data.ip.valid.vxlan ) {
				fdc.OrFlags(WinEditIpEncapIpOptsGeneveEditIndex, DIF_HIDDEN);
				fdc.OrFlags(WinEditIpEncapIpOptsErspanEditIndex, DIF_HIDDEN);
				fdc.SetText(WinEditIpEncapIpOptsButtonIndex, L"vxlan_opts");
				if( enc.data.ip.valid.vxlan_gbp )
					fdc.SetCountText(WinEditIpEncapIpOptsVxlanEditIndex, enc.data.ip.vxlan_gbp);
			} else if( enc.data.ip.flags & TUNNEL_ERSPAN_OPT && enc.data.ip.valid.erspan ) {
				fdc.OrFlags(WinEditIpEncapIpOptsVxlanEditIndex, DIF_HIDDEN);
				fdc.OrFlags(WinEditIpEncapIpOptsGeneveEditIndex, DIF_HIDDEN);
				fdc.SetText(WinEditIpEncapIpOptsButtonIndex, L"erspan_opts");
				fdc.SetText(WinEditIpEncapIpOptsErspanEditIndex, towstr(erspanstring(&enc.data.ip.erspan)).c_str(), true);
			}
		}
	}
		break;
	default:
		fdc.HideItems(WinEditIpEncapIpIdTextIndex, WinEditIpEncapIpOptsErspanEditIndex);
		break;
	};

	auto offSufix = fdc.AppendOkCancel();

	fdc.SetDefaultButton(offSufix + WinSuffixOkIndex);
	fdc.SetFocus(offSufix + WinSuffixOkIndex);

	FarDialog dlg(&fdc, EditEncapDialogProc, (LONG_PTR)this);
	dlg.Run();

	return;
}

const int EDIT_IP_DIALOG_WIDTH = 84;
enum {
	WinEditIpCaptionIndex,
	WinEditIpFirstHeaderTextIndex,
	WinEditIpTypeButtonIndex,
	WinEditIpDestMaskEditIndex,
	WinEditIpFamilyButtonIndex,
	WinEditIpGatewayEditIndex,
	WinEditIpSeparator1TextIndex,
	WinEditIpDeviceTextIndex,
	WinEditIpDeviceButtonIndex,
	WinEditIpTableButtonIndex,
	WinEditIpScopeButtonIndex,
	WinEditIpProtoButtonIndex,
	WinEditIpPrefButtonIndex,
	WinEditIpMetricEditIndex,
	WinEditIpSeparator2TextIndex,
	WinEditIpSecondHeaderTextIndex,
	WinEditIpPrefsrcEditIndex,
	WinEditIpSrcMaskEditIndex,
	WinEditIpTOSEditIndex,
	WinEditIpOnlinkCheckBoxIndex,
	WinEditIpEncapCheckBoxIndex,
	WinEditIpEncapButtonIndex,
	WinEditIpPrefixMaxIndex
};

enum {
	WinEditIpNextHopeSeparatorIndex,
	WinEditIpNextHopeNextHopeCheckBoxIndex,
	WinEditIpNextHopeHeaderTextIndex,
	WinEditIpNextHopeOnlinkCheckBoxIndex,
	WinEditIpNextHopeEncapCheckBoxIndex,
	WinEditIpNextHopeEncapButtonIndex,
	WinEditIpNextHopeFamilyButtonIndex,
	WinEditIpNextHopeViaEditIndex,
	WinEditIpNextHopeDeviceButtonIndex,
	WinEditIpNextHopeWeightTextIndex,
	WinEditIpNextHopeWeightEditIndex,
	WinEditIpNextHopeMaxIndex
};

static void EditIpRouteAdjustFlags(HANDLE hDlg, uint32_t base)
{
	bool enabled = bool(NetCfgPlugin::psi.SendDlgMessage(hDlg, DM_GETCHECK, base+WinEditIpNextHopeNextHopeCheckBoxIndex, 0));
	ChangeDialogItemsView(hDlg, WinEditIpNextHopeHeaderTextIndex + base, WinEditIpNextHopeWeightEditIndex + base, false, !enabled);
	enabled = bool(NetCfgPlugin::psi.SendDlgMessage(hDlg, DM_GETCHECK, base + WinEditIpNextHopeEncapCheckBoxIndex, 0));
	ChangeDialogItemsView(hDlg, WinEditIpNextHopeEncapButtonIndex + base, WinEditIpNextHopeEncapButtonIndex + base, false, !enabled);
}

LONG_PTR WINAPI EditIpRouteDialogProc(HANDLE hDlg, int msg, int param1, LONG_PTR param2)
{
	static NetcfgIpRoute * rtCfg = 0;

	LOG_INFO("MSG: 0x%08X, param1 %u\n", msg, param1);

	switch( msg ) {

	case DN_INITDIALOG:
		rtCfg = (NetcfgIpRoute *)param2;
		break;
	case DM_CLOSE:
		rtCfg = 0;
		break;
	case DN_DRAWDLGITEM:
	{
		if( param1 > WinEditIpPrefixMaxIndex ) {
			uint32_t rindex = (param1 - WinEditIpPrefixMaxIndex)/WinEditIpNextHopeMaxIndex;
			uint32_t base = rindex*WinEditIpNextHopeMaxIndex+WinEditIpPrefixMaxIndex;
			uint32_t iindex = (param1 - WinEditIpPrefixMaxIndex)%WinEditIpNextHopeMaxIndex;
			assert( rtCfg->rt != nullptr );
			if( rindex <= rtCfg->rt->osdep.nhs.size() && iindex == WinEditIpNextHopeNextHopeCheckBoxIndex )
				EditIpRouteAdjustFlags(hDlg, base);
		} else if( param1 == WinEditIpEncapCheckBoxIndex ) {
			bool enabled = bool(NetCfgPlugin::psi.SendDlgMessage(hDlg, DM_GETCHECK, WinEditIpEncapCheckBoxIndex, 0));
			ChangeDialogItemsView(hDlg, WinEditIpEncapButtonIndex, WinEditIpEncapButtonIndex, false, !enabled);
		}

		break;
	}
	case DN_BTNCLICK:
		assert( rtCfg != 0 );

		if( param1 < WinEditIpPrefixMaxIndex ) {
		switch( param1 ) {
		case WinEditIpDeviceButtonIndex:
			rtCfg->SelectInterface(hDlg, WinEditIpDeviceButtonIndex);
			return true;
		case WinEditIpTypeButtonIndex:
		{
		static const wchar_t * menuElements[] = {
				L"unicast",
				L"local",
				L"broadcast",
				L"multicast",
				L"throw",
				L"unreachable",
				L"prohibit",
				L"blackhole",
				L"nat"
				};

			rtCfg->Select(hDlg, &menuElements[0], ARRAYSIZE(menuElements), WinEditIpTypeButtonIndex);
			return true;
		}
		case WinEditIpFamilyButtonIndex:
			rtCfg->SelectFamily(hDlg, WinEditIpFamilyButtonIndex);
			return true;
		#if !defined(__APPLE__) && !defined(__FreeBSD__)
		case WinEditIpTableButtonIndex:
		{
			static const wchar_t * menuElements[] = {
				L"main",
				L"local",
				L"default",
				L"Enter:"
				};

			rtCfg->SelectNum(hDlg, &menuElements[0], ARRAYSIZE(menuElements), L"Table:", WinEditIpTableButtonIndex);
			return true;
		}
		#endif
		case WinEditIpScopeButtonIndex:
		{
			static const wchar_t * menuElements[] = {
				L"host",
				L"link",
				L"global",
				L"Enter:"
				};
			rtCfg->SelectNum(hDlg, &menuElements[0], ARRAYSIZE(menuElements), L"Scope:", WinEditIpScopeButtonIndex);
			return true;
		}
		case WinEditIpProtoButtonIndex:
		{
			static const wchar_t * menuElements[] = {
				L"unspec",
				L"redirect",
				L"kernel",
				L"boot",
				L"static",
				L"dhcp",
				L"gated",
				L"ra",
				L"mrt",
				L"zebra",
				L"bird",
				L"dnrouted",
				L"xorp",
				L"ntk",
				L"keepalived",
				L"babel",
				L"bgp",
				L"isis",
				L"ospf",
				L"rip",
				L"eigrp",
				L"Enter:"
				};
			rtCfg->SelectNum(hDlg, &menuElements[0], ARRAYSIZE(menuElements), L"Proto:", WinEditIpProtoButtonIndex);
			return true;
		}
		case WinEditIpPrefButtonIndex:
		{
			static const wchar_t * menuElements[] = {
				L"medium",
				L"low",
				L"high"
				};
			rtCfg->Select(hDlg, &menuElements[0], ARRAYSIZE(menuElements), WinEditIpPrefButtonIndex);
			return true;
		}

		case WinEditIpEncapButtonIndex:
			rtCfg->SelectEncap(hDlg, rtCfg->rt->osdep.enc, WinEditIpEncapButtonIndex);
			return true;


		};
		} else {
			uint32_t rindex = (param1 - WinEditIpPrefixMaxIndex)/WinEditIpNextHopeMaxIndex;
			uint32_t base = rindex*WinEditIpNextHopeMaxIndex+WinEditIpPrefixMaxIndex;
			uint32_t iindex = (param1 - WinEditIpPrefixMaxIndex)%WinEditIpNextHopeMaxIndex;

			assert( rtCfg->rt != nullptr );

			// active if 0 or real items + 1
			if( rindex <= rtCfg->rt->osdep.nhs.size() )
			switch( iindex ) {
			case WinEditIpNextHopeEncapButtonIndex:
			{
				rtCfg->SelectEncap(hDlg, \
					rindex < rtCfg->rt->osdep.nhs.size() ? rtCfg->rt->osdep.nhs[rindex].enc:rtCfg->new_nexthop_enc, \
					base + WinEditIpNextHopeEncapButtonIndex);
				return true;
			}
			case WinEditIpNextHopeFamilyButtonIndex:
				rtCfg->SelectFamily(hDlg, base + WinEditIpNextHopeFamilyButtonIndex);
				return true;
			case WinEditIpNextHopeDeviceButtonIndex:
				rtCfg->SelectInterface(hDlg, base + WinEditIpNextHopeDeviceButtonIndex);
				return true;

			};
		}
		break;
	};

	return NetCfgPlugin::psi.DefDlgProc(hDlg, msg, param1, param2);
}

bool NetcfgIpRoute::EditIpRoute(void)
{
	LOG_INFO("\n");
	bool change = false;

	auto ppi = GetCurrentPanelItem();
	if( !ppi )
		return false;
	do {
		if( !(ppi->Flags & PPIF_USERDATA) )
			break;

		assert(ppi->UserData && ((PluginUserData *)ppi->UserData)->size == sizeof(PluginUserData));

		rt = ((PluginUserData *)ppi->UserData)->data.inet;
		static const DlgConstructorItem dialog[] = {
			//  Type       NewLine X1              X2         Flags                PtrData
			{DI_DOUBLEBOX, false,  DEFAUL_X_START, EDIT_IP_DIALOG_WIDTH-4,  0,     {.ptrData = L"Edit route:"}},
			{DI_TEXT,      true,   5,              0,           0,                 {.ptrData = L"type:           destination/mask:        via(fam):  via(gateway):"}},
			{DI_BUTTON,    true,   5,              0,           0,                 {.ptrData = 0}},
			{DI_EDIT,      false,  21,            44,           DIF_LEFTTEXT,      {.ptrData = 0}},
			{DI_BUTTON,    false,  46,             0,           0,                 {.ptrData = 0}},
			{DI_EDIT,      false,  57,            78,           DIF_LEFTTEXT,      {.ptrData = 0}},
			{DI_TEXT,      true,   5,              0,           0,                 {0}},
			{DI_TEXT,      true,   5,              0,           0,                 {.ptrData = L"device:             table:      scope:     proto:       pref:      metric:"}},
			{DI_BUTTON,    true,   5,              0,           DIF_CENTERTEXT,    {.ptrData = 0}},
			{DI_BUTTON,    false,  25,             0,           0,                 {.ptrData = 0}},
			{DI_BUTTON,    false,  37,             0,           0,                 {.ptrData = 0}},
			{DI_BUTTON,    false,  48,             0,           0,                 {.ptrData = 0}},
			{DI_BUTTON,    false,  61,             0,           0,                 {.ptrData = 0}},
			{DI_EDIT,      false,  72,            78,           DIF_LEFTTEXT,      {.ptrData = 0}},

			{DI_TEXT,      true,   5,              0,           0,                 {0}},
			{DI_TEXT,      true,   5,              0,           0,                 {.ptrData = L"prefsrc:            source/mask:             tos:"}},
			{DI_EDIT,      true,   5,             23,           DIF_LEFTTEXT,      {.ptrData = 0}},
			{DI_EDIT,      false,  25,            48,           DIF_LEFTTEXT,      {.ptrData = 0}},
			{DI_EDIT,      false,  50,            53,           DIF_LEFTTEXT,      {.ptrData = 0}},
			{DI_CHECKBOX,  false,  55,            0,            0,                 {.ptrData = L"onlink"}},
			{DI_CHECKBOX,  false,  66,            0,            0,                 {0}},
			{DI_BUTTON,    false,  69,            0,            0,                 {.ptrData = L"encap"}},
			{DI_ENDDIALOG, 0}
		};

		static const DlgConstructorItem nexthop[] = {
			//  Type       NewLine X1              X2         Flags                PtrData
			{DI_TEXT,      true,   5,             0,            DIF_BOXCOLOR|DIF_SEPARATOR, {0}},
			{DI_CHECKBOX,  true,   5,             0,            0,                 {.ptrData = L"nexthop"}},
			{DI_TEXT,      false,  17,            0,            0,                 {.ptrData = L"via(gateway):            device:"}},

			{DI_CHECKBOX,  false,  55,            0,            0,                 {.ptrData = L"onlink"}},
			{DI_CHECKBOX,  false,  66,            0,            0,                 {0}},
			{DI_BUTTON,    false,  69,            0,            0,                 {.ptrData = L"encap"}},

			{DI_BUTTON,    true,   5,             0,            0,                 {.ptrData = L"inet6"}},
			{DI_EDIT,      false,  17,           40,            DIF_LEFTTEXT,      {.ptrData = L"255.255.255.255"}},
			{DI_BUTTON,    false,  42,            0,            DIF_CENTERTEXT,    {.ptrData = L"wlxd123456789ab"}},

			{DI_TEXT,      false,  62,            0,            0,                 {.ptrData = L"weight:"}},
			{DI_EDIT,      false,  70,           77,            DIF_LEFTTEXT,      {.ptrData = L"1000"}},
			{DI_ENDDIALOG, 0}
		};

		FarDlgConstructor fdc(&dialog[0]);

		fdc.SetText(WinEditIpDestMaskEditIndex, rt->valid.destIpandMask ? rt->destIpandMask.c_str():L"default");

		if( rt->valid.iface )
			fdc.SetText(WinEditIpDeviceButtonIndex, rt->iface.c_str());

		if( rt->valid.prefsrc )
			fdc.SetText(WinEditIpPrefsrcEditIndex, rt->prefsrc.c_str());

		#if !defined(__APPLE__) && !defined(__FreeBSD__)
		if( rt->valid.fromsrcIpandMask )
			fdc.SetText(WinEditIpSrcMaskEditIndex, rt->osdep.fromsrcIpandMask.c_str());
		if( rt->valid.rtvia )
			fdc.SetText(WinEditIpGatewayEditIndex, rt->osdep.rtvia_addr.c_str());
		#endif
		else if( rt->valid.gateway )
			fdc.SetText(WinEditIpGatewayEditIndex, rt->gateway.c_str());

		fdc.SetText(WinEditIpFamilyButtonIndex, towstr(ipfamilyname(rt->sa_family)).c_str(), true);
		fdc.SetText(WinEditIpTypeButtonIndex, rt->valid.type ? towstr(rttype(rt->osdep.type)).c_str():0, true);
		fdc.SetText(WinEditIpTableButtonIndex, rt->valid.table ? towstr(rtruletable(rt->osdep.table)).c_str():0, true);
		fdc.SetText(WinEditIpScopeButtonIndex, rt->valid.scope ? towstr(rtscopetype(rt->osdep.scope)).c_str():0, true);
		fdc.SetText(WinEditIpProtoButtonIndex, rt->valid.protocol ? towstr(rtprotocoltype(rt->osdep.protocol)).c_str():0, true);

		if( rt->sa_family == AF_INET )
			fdc.OrFlags(WinEditIpPrefButtonIndex, DIF_DISABLE);
		fdc.SetText(WinEditIpPrefButtonIndex, rt->valid.icmp6pref ? towstr(rticmp6pref(rt->osdep.icmp6pref)).c_str():0, true);

		if( rt->valid.metric )
			fdc.SetCountText(WinEditIpMetricEditIndex, rt->osdep.metric);

		if( rt->valid.tos )
			fdc.SetCountText(WinEditIpTOSEditIndex, rt->osdep.tos);

		if( !rt->valid.encap )
			fdc.OrFlags(WinEditIpEncapButtonIndex, DIF_DISABLE);
		else
			fdc.SetSelected(WinEditIpEncapCheckBoxIndex, true);

		auto offNextHope = fdc.GetNumberOfItems();

		for( auto & item: rt->osdep.nhs ) {
			auto off = fdc.GetNumberOfItems();
			fdc.AppendItems(&nexthop[0]);
			fdc.SetSelected(off+WinEditIpNextHopeNextHopeCheckBoxIndex, true);
			fdc.SetSelected(off+WinEditIpNextHopeOnlinkCheckBoxIndex, item.flags & RTNH_F_ONLINK);
			fdc.SetText(off+WinEditIpNextHopeFamilyButtonIndex, item.valid.rtvia ? towstr(ipfamilyname(item.rtvia_family)).c_str():0, true);
			fdc.SetText(off+WinEditIpNextHopeViaEditIndex, item.valid.rtvia ? item.rtvia_addr.c_str():item.gateway.c_str());
			auto it = ifs.find(item.ifindex);
			if( it != ifs.end() )
				fdc.SetText(off+WinEditIpNextHopeDeviceButtonIndex, it->second.c_str());
			fdc.SetCountText(off+WinEditIpNextHopeWeightEditIndex, item.weight);

			if( !item.valid.encap )
				fdc.OrFlags(off+WinEditIpNextHopeEncapButtonIndex, DIF_DISABLE);
			else
				fdc.SetSelected(off+WinEditIpNextHopeEncapCheckBoxIndex, true);


		}

		auto offNewNextHope = fdc.GetNumberOfItems();
		fdc.AppendItems(&nexthop[0]);

		for(int i = WinEditIpNextHopeHeaderTextIndex; i < WinEditIpNextHopeMaxIndex; i++ )
			fdc.OrFlags(offNewNextHope+i, DIF_DISABLE);

		auto offSufix = fdc.AppendOkCancel();

		fdc.SetDefaultButton(offSufix + WinSuffixOkIndex);
		fdc.SetFocus(offSufix + WinSuffixOkIndex);

		FarDialog dlg(&fdc, EditIpRouteDialogProc, (LONG_PTR)this);
		dlg.Run();

	} while(0);

	rt == nullptr;
	FreePanelItem(ppi);

	return change;
}

int NetcfgIpRoute::ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change)
{
	LOG_INFO("NetcfgIpRoute::ProcessKey(key=0x%08X, controlState = 0x%08X)\n", key, controlState);

	if( controlState == 0 ) {
		switch( key ) {
		#if !defined(__APPLE__) && !defined(__FreeBSD__)
		case VK_RETURN:
			{
			PanelInfo pi = {0};
			if( auto ppi = GetCurrentPanelItem(&pi) ) {
				PanelRedrawInfo pri;

				if( NetCfgPlugin::FSF.LStricmp(ppi->FindData.lpwszFileName, L"..") == 0 ) {
					table = ROUTE_TABLE_DIRS;
					pri.TopPanelItem = topIndex;
					pri.CurrentItem = dirIndex;
				} else if( table == ROUTE_TABLE_DIRS ) {
					table = ppi->FindData.nFileSize;
					topIndex = pi.TopPanelItem;
					dirIndex = pi.CurrentItem;
					pri.TopPanelItem = 0;
					pri.CurrentItem = 0;
				} else {
					FreePanelItem(ppi);
					return TRUE;
				}

				FreePanelItem(ppi);
				NetCfgPlugin::psi.Control(hPlugin, FCTL_UPDATEPANEL, TRUE, 0);
				NetCfgPlugin::psi.Control(hPlugin, FCTL_REDRAWPANEL, 0, (LONG_PTR)&pri);
				return TRUE;
			}
			}
			break;
		#endif
		case VK_F8:
			#if !defined(__APPLE__) && !defined(__FreeBSD__)
			if( table != ROUTE_TABLE_DIRS )
			#endif
				change = DeleteIpRoute();
			return TRUE;
		case VK_F4:
			#if !defined(__APPLE__) && !defined(__FreeBSD__)
			if( table != ROUTE_TABLE_DIRS )
			#endif
				change |= EditIpRoute();
			return TRUE;
		}
	}

	return FALSE;
}

#if !defined(__APPLE__) && !defined(__FreeBSD__)

void NetcfgIpRoute::GetOpenPluginInfo(struct OpenPluginInfo * info)
{
	FarPanel::GetOpenPluginInfo(info);

	if( table != ROUTE_TABLE_DIRS ) {
		info->Flags |= OPIF_ADDDOTS;
		return;
	}

	info->PanelModesArray = tablemodes.get();
}

int NetcfgIpRoute::GetFindDataTables(struct PluginPanelItem **pPanelItem, int *pItemsNumber)
{
	LOG_INFO("\n");
	std::map<int, int> tables{{0,0},{253,0},{254,0},{255,0}};
	for( const auto & item : inet ) {
		auto [it, success] = tables.try_emplace(item.osdep.table, 0);
		tables[0]++;
		it->second++;
	}

	*pItemsNumber = tables.size();
	*pPanelItem = (struct PluginPanelItem *)malloc((*pItemsNumber) * sizeof(PluginPanelItem));
	memset(*pPanelItem, 0, (*pItemsNumber) * sizeof(PluginPanelItem));
	PluginPanelItem * pi = *pPanelItem;
	for( const auto & item : tables ) {
		pi->FindData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		pi->FindData.nFileSize = item.first;

		pi->FindData.dwFileAttributes |= FILE_FLAG_DELETE_ON_CLOSE;
		pi->FindData.lpwszFileName = wcsdup(towstr(rtruletable(item.first)).c_str());

		const wchar_t ** CustomColumnData = (const wchar_t **)malloc(1*sizeof(const wchar_t *));
		if( CustomColumnData ) {
			memset(CustomColumnData, 0, 1*sizeof(const wchar_t *));
			CustomColumnData[0] = DublicateCountString(item.second);
			pi->CustomColumnNumber = 1;
			pi->CustomColumnData = CustomColumnData;
		}
		pi++;
	}

	return int(true);
}

void NetcfgIpRoute::FreeFindData(struct PluginPanelItem * panelItem, int itemsNumber)
{
	FarPanel::FreeFindData(panelItem, itemsNumber);
	while( itemsNumber-- ) {
		if( (panelItem+itemsNumber)->FindData.dwFileAttributes & FILE_FLAG_DELETE_ON_CLOSE )
			free((void *)(panelItem+itemsNumber)->FindData.lpwszFileName);
	}
}
#endif

int NetcfgIpRoute::GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber)
{
	LOG_INFO("\n");

	const static wchar_t * default_route = L"default";
	const static wchar_t * empty_string = L"";
	*pItemsNumber = inet.size();

	#if !defined(__APPLE__) && !defined(__FreeBSD__)
	if( table == ROUTE_TABLE_DIRS )
		return GetFindDataTables(pPanelItem, pItemsNumber);

	if( table )
		for( const auto & item : inet ) {
			if( item.osdep.table != table )
				(*pItemsNumber)--;
		}
	#endif

	*pPanelItem = (struct PluginPanelItem *)malloc((*pItemsNumber) * sizeof(PluginPanelItem));
	memset(*pPanelItem, 0, (*pItemsNumber) * sizeof(PluginPanelItem));
	PluginPanelItem * pi = *pPanelItem;
	for( const auto & item : inet ) {

		#if !defined(__APPLE__) && !defined(__FreeBSD__)
		if( table && item.osdep.table != table )
			continue;
		#endif

		pi->FindData.lpwszFileName = item.destIpandMask.empty() ? default_route:item.destIpandMask.c_str();
		pi->FindData.dwFileAttributes = 0;
		pi->FindData.nFileSize = 0;

		PluginUserData * user_data = (PluginUserData *)malloc(sizeof(PluginUserData));
		if( user_data ) {
			user_data->size = sizeof(PluginUserData);
			user_data->data.inet = const_cast<IpRouteInfo*>(&item);
			pi->Flags = PPIF_USERDATA;
			pi->UserData = (DWORD_PTR)user_data;
		}

		const wchar_t ** CustomColumnData = (const wchar_t **)malloc(RoutesColumnDataMaxIndex*sizeof(const wchar_t *));
		if( CustomColumnData ) {
			memset(CustomColumnData, 0, RoutesColumnDataMaxIndex*sizeof(const wchar_t *));

			CustomColumnData[RoutesColumnViaIndex] = wcsdup(item.valid.gateway ? item.gateway.c_str(): (item.valid.rtvia ? item.osdep.rtvia_addr.c_str():empty_string));
			CustomColumnData[RoutesColumnDevIndex] = wcsdup(item.iface.c_str());
			CustomColumnData[RoutesColumnPrefsrcIndex] = wcsdup(item.prefsrc.c_str());
			#if !defined(__APPLE__) && !defined(__FreeBSD__)				
			CustomColumnData[RoutesColumnTypeIndex] = wcsdup(item.valid.protocol ? towstr(rtprotocoltype(item.osdep.protocol)).c_str():empty_string);
			CustomColumnData[RoutesColumnMetricIndex] = item.valid.metric ? DublicateCountString(item.osdep.metric):wcsdup(empty_string);
			#else
			CustomColumnData[RoutesColumnTypeIndex] = wcsdup(empty_string);
			CustomColumnData[RoutesColumnMetricIndex] = DublicateCountString(item.osdep.expire);
			#endif
			pi->CustomColumnNumber = RoutesColumnDataMaxIndex;
			pi->CustomColumnData = CustomColumnData;
		}
		pi++;
	}
	return int(true);
}

NetcfgArpRoute::NetcfgArpRoute(uint32_t index_, std::deque<ArpRouteInfo> & arp_):
	FarPanel(index_),
	arp(arp_)
{
	LOG_INFO("index_ %u this %p\n", index_, this);
}

NetcfgArpRoute::~NetcfgArpRoute()
{
	LOG_INFO("\n");
}

int NetcfgArpRoute::ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change)
{
	LOG_INFO("\n");
	return FALSE;
}

int NetcfgArpRoute::GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber)
{
	LOG_INFO("\n");
	*pItemsNumber = arp.size();
	*pPanelItem = (struct PluginPanelItem *)malloc((*pItemsNumber) * sizeof(PluginPanelItem));
	memset(*pPanelItem, 0, (*pItemsNumber) * sizeof(PluginPanelItem));
	PluginPanelItem * pi = *pPanelItem;
	for( const auto & item : arp ) {
		pi->FindData.lpwszFileName = item.ip.c_str();
		pi->FindData.dwFileAttributes = 0;
		pi->FindData.nFileSize = 0;

		const wchar_t ** CustomColumnData = (const wchar_t **)malloc(ArpRoutesColumnMaxIndex*sizeof(const wchar_t *));

		if( CustomColumnData ) {
			memset(CustomColumnData, 0, ArpRoutesColumnMaxIndex*sizeof(const wchar_t *));
			CustomColumnData[ArpRoutesColumnMacIndex] = wcsdup(item.mac.empty() ? L"00:00:00:00:00:00":item.mac.c_str());
			CustomColumnData[ArpRoutesColumnDevIndex] = wcsdup(item.iface.c_str());
			CustomColumnData[ArpRoutesColumnTypeIndex] = wcsdup(towstr(rttype(item.type)).c_str());
			CustomColumnData[ArpRoutesColumnStateIndex] = wcsdup(towstr(ndmsgstate(item.state)).c_str());
			pi->CustomColumnNumber = ArpRoutesColumnMaxIndex;
			pi->CustomColumnData = CustomColumnData;
		}
		pi++;
	}
	return int(true);
}


NetcfgRoutes::NetcfgRoutes():
	FarPanel()
{
	LOG_INFO("\n");

	nrts = std::make_unique<NetRoutes>();

	nrts->Update();

	panels.push_back(std::move(std::make_unique<NetcfgIpRoute>(RouteInetPanelIndex, nrts->inet, nrts->ifs)));
	panels.push_back(std::move(std::make_unique<NetcfgIpRoute>(RouteInet6PanelIndex, nrts->inet6, nrts->ifs)));
	panels.push_back(std::move(std::make_unique<NetcfgArpRoute>(RouteArpPanelIndex, nrts->arp)));

	autoAppendMcRoutes = false;
	autoAppendMc6Routes = false;

	if( nrts->mcinet.size() )
		panels.push_back(std::move(std::make_unique<NetcfgIpRoute>(RouteMcInetPanelIndex, nrts->mcinet, nrts->ifs)));
	else
		autoAppendMcRoutes = true;
		
	if( nrts->mcinet6.size() )
		panels.push_back(std::move(std::make_unique<NetcfgIpRoute>(RouteMcInet6PanelIndex, nrts->mcinet6, nrts->ifs)));
	else
		autoAppendMc6Routes = true;

	active = 0;
}

NetcfgRoutes::~NetcfgRoutes()
{
	LOG_INFO("\n");
}

int NetcfgRoutes::ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & redraw)
{
	if( controlState == 0 ) {
		switch( key ) {
		case VK_F6:
			if( GET_ACTIVE_ROUTE_PANEL() < panels.size() )
				active++;
			else
				active = 0;
			redraw = true;
			return TRUE;
		case VK_F2:
			nrts->Update();

			if( autoAppendMcRoutes && nrts->mcinet.size() ) {
				panels.push_back(std::move(std::make_unique<NetcfgIpRoute>(RouteMcInetPanelIndex, nrts->mcinet, nrts->ifs)));
				autoAppendMcRoutes = false;
			}
			if( autoAppendMc6Routes && nrts->mcinet6.size() ) {
				panels.push_back(std::move(std::make_unique<NetcfgIpRoute>(RouteMcInet6PanelIndex, nrts->mcinet6, nrts->ifs)));
				autoAppendMc6Routes = false;
			}

			redraw = true;
			return TRUE;
		}
	}

	auto res = panels[active]->ProcessKey(hPlugin, key, controlState, redraw);

	if( redraw )
		nrts->Update();

	return res;
}

int NetcfgRoutes::GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber)
{
	LOG_INFO("active %u\n", active);
	return panels[active]->GetFindData(pPanelItem, pItemsNumber);
}

void NetcfgRoutes::FreeFindData(struct PluginPanelItem * panelItem, int itemsNumber)
{
	LOG_INFO("\n");
	panels[active]->FreeFindData(panelItem, itemsNumber);
}

void NetcfgRoutes::GetOpenPluginInfo(struct OpenPluginInfo * info)
{
	LOG_INFO("NetcfgRoutes::GetOpenPluginInfo()\n");
	panels[active]->GetOpenPluginInfo(info);
}
