#include "farpanel.h"
#include "netcfglng.h"
#include "netcfgplugin.h"

#include <string>
#include <assert.h>
#include <utils.h>

#include <common/log.h>

extern const char * LOG_FILE;
#define LOG_SOURCE_FILE "farpanel.cpp"

FarPanel::FarPanel():
	index(NO_PANEL_INDEX),
	data(nullptr)
{
	LOG_INFO("index NO_PANEL_INDEX\n");
}

FarPanel::FarPanel(uint32_t index_):
	index(static_cast<PanelIndex>(index_)),
	data(std::make_unique<PanelData>())
{
	LOG_INFO("index %u\n", index);
	FillPanelData(data.get(), index);
}

FarPanel::~FarPanel()
{
	LOG_INFO("\n");
}

void FarPanel::GetPanelInfo(PanelInfo & pi)
{
	NetCfgPlugin::psi.Control(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, (LONG_PTR)&pi);
}


PluginPanelItem * FarPanel::GetPanelItem(intptr_t itemNum) const
{
	auto size = NetCfgPlugin::psi.Control(PANEL_ACTIVE, FCTL_GETPANELITEM, itemNum, 0);
	PluginPanelItem * ppi = (PluginPanelItem *)malloc(size);
	if( ppi != 0 )
		NetCfgPlugin::psi.Control(PANEL_ACTIVE, FCTL_GETPANELITEM, itemNum, (LONG_PTR)ppi);
	return ppi;
}

PluginPanelItem * FarPanel::GetCurrentPanelItem(PanelInfo * piret) const
{
	struct PanelInfo pi = {0};
	NetCfgPlugin::psi.Control(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, (LONG_PTR)&pi);
	if( piret )
		*piret = pi;
	return GetPanelItem(pi.CurrentItem);
}

PluginPanelItem * FarPanel::GetSelectedPanelItem(intptr_t selectedItemNum)
{
	auto size = NetCfgPlugin::psi.Control(PANEL_ACTIVE, FCTL_GETSELECTEDPANELITEM, selectedItemNum, 0);
	PluginPanelItem * ppi = (PluginPanelItem *)malloc(size);
	if( ppi != 0 )
		NetCfgPlugin::psi.Control(PANEL_ACTIVE, FCTL_GETSELECTEDPANELITEM, selectedItemNum, (LONG_PTR)ppi);
	return ppi;
}

void FarPanel::FreePanelItem(PluginPanelItem * ppi) const
{
	free((void *)ppi);
}

const wchar_t * FarPanel::GetMsg(int msgId) const
{
	assert( NetCfgPlugin::psi.GetMsg != 0 );
	return NetCfgPlugin::psi.GetMsg(NetCfgPlugin::psi.ModuleNumber, msgId);
}

int FarPanel::Select(HANDLE hDlg, const wchar_t ** elements, int count, uint32_t setIndex)
{
	int index = 0;
	auto menuElements = std::make_unique<FarMenuItem[]>(count);
	memset(menuElements.get(), 0, sizeof(FarMenuItem)*count);
	do {
		menuElements[index].Text = elements[index];
	} while( ++index < count );

	menuElements[0].Selected = 1;

	index = NetCfgPlugin::psi.Menu(NetCfgPlugin::psi.ModuleNumber, -1, -1, 0, FMENU_WRAPMODE|FMENU_AUTOHIGHLIGHT, \
			 L"Select:", 0, L"", nullptr, nullptr, menuElements.get(), count);
	if( index >= 0 && index < count )
		NetCfgPlugin::psi.SendDlgMessage(hDlg, DM_SETTEXTPTR, setIndex, (LONG_PTR)menuElements[index].Text);
	return index;
}

int FarPanel::SelectNum(HANDLE hDlg, const wchar_t ** elements, int count, const wchar_t * subTitle, uint32_t setIndex)
{
	auto index = Select(hDlg, elements, count, setIndex);
	if( index == (count-1) ) {
		static wchar_t num[sizeof("4294967296")] = {0};
		NetCfgPlugin::psi.InputBox(L"Enter number:", subTitle, 0, 0, num, ARRAYSIZE(num), 0, FIB_NOUSELASTHISTORY);
		NetCfgPlugin::psi.SendDlgMessage(hDlg, DM_SETTEXTPTR, setIndex, (LONG_PTR)num);
		NetCfgPlugin::psi.SendDlgMessage(hDlg, DM_REDRAW, 0, 0);
	}
	return index;
}

void FarPanel::GetOpenPluginInfo(struct OpenPluginInfo * info)
{
	LOG_INFO("index %u\n", index);
	ReloadPanelString(data.get(), index);
	*info = data->openInfo;
}

const wchar_t * FarPanel::GetPanelTitle(void) const
{
	return data->openInfo.PanelTitle;
}

void FarPanel::FreeFindData(struct PluginPanelItem * panelItem, int itemsNumber)
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

std::wstring FarPanel::towstr(const char * name) const
{
	std::string _s(name);
	return std::wstring(_s.begin(), _s.end());
}

std::string FarPanel::tostr(const wchar_t * name) const
{
	std::wstring _s(name);
	return std::string(_s.begin(), _s.end());
}

const wchar_t * FarPanel::DublicateCountString(int64_t value) const
{
	wchar_t max64[sizeof("18446744073709551615")] = {0};

#if INTPTR_MAX == INT32_MAX
	if( NetCfgPlugin::FSF.snprintf(max64, ARRAYSIZE(max64), L"%lld", value) > 0 )
#elif INTPTR_MAX == INT64_MAX
	if( NetCfgPlugin::FSF.snprintf(max64, ARRAYSIZE(max64), L"%ld", value) > 0 )
#else
    #error "Environment not 32 or 64-bit."
#endif
		return wcsdup(max64);

	return wcsdup(L"");
}

const wchar_t * FarPanel::DublicateFileSizeString(uint64_t value) const
{
	if( value > 100 * 1024 ) {
		std::wstring _tmp = FileSizeString(value);
		return wcsdup(_tmp.c_str());
	}
	return DublicateCountString((int64_t)value);
}
