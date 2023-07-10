#ifndef __CONFIGPLUGIN_H__
#define __CONFIGPLUGIN_H__

#include <farplug-wide.h>
#include <farkeys.h>
#include <map>

enum {
	PanelModeBrief,
	PanelModeMedium,
	PanelModeFull,
	PanelModeWide,
	PanelModeDetailed,
	PanelModeDiz,
	PanelModeLongDiz,
	PanelModeOwners,
	PanelModeLinks,
	PanelModeAlternative,
	PanelModeMax,
};

enum {
	InterfaceColumnIpIndex,
	InterfaceColumnMacIndex,
	InterfaceColumnIp6Index,
	InterfaceColumnMtuIndex,
	InterfaceColumnRecvBytesIndex,
	InterfaceColumnSendBytesIndex,
	InterfaceColumnRecvPktsIndex,
	InterfaceColumnSendPktsIndex,
	InterfaceColumnRecvErrsIndex,
	InterfaceColumnSendErrsIndex,
	InterfaceColumnMulticastIndex,
	InterfaceColumnCollisionsIndex,
	InterfaceColumnPermanentMacIndex,
	InterfaceColumnMaxIndex
};

enum {
	RoutesColumnViaIndex,
	RoutesColumnDevIndex,
	RoutesColumnPrefsrcIndex,
	RoutesColumnTypeIndex,
	RoutesColumnMetricIndex,
	RoutesColumnDataMaxIndex
};


enum {
	ArpRoutesColumnMacIndex,
	ArpRoutesColumnDevIndex,
	ArpRoutesColumnTypeIndex,
	ArpRoutesColumnStateIndex,
	ArpRoutesColumnMaxIndex
};

typedef struct {
	const wchar_t * statusColumnTypes;
	const wchar_t * statusColumnWidths;
	const wchar_t * columnTypes[2];
	const wchar_t * columnWidths[2];
	const wchar_t * columnTitles[2][15];
	uint32_t keyBarTitles[12];
	uint32_t panelTitle;
	uint32_t format;
	uint32_t flags;
} CfgDefaults;

typedef enum {
	IfcsPanelIndex,
	RouteInetPanelIndex,
	RouteInet6PanelIndex,
	RouteArpPanelIndex,
	RouteMcInetPanelIndex,
	RouteMcInet6PanelIndex,
	MaxPanelIndex
} PanelIndex;

class PluginCfg {

	private:
		static std::map<PanelIndex, CfgDefaults> def;
		static size_t init;

		const char * GetPanelName(PanelIndex index);

		bool addToDisksMenu;
		bool addToPluginsMenu;
		bool logEnable;

		const wchar_t * GetMsg(int msgId);
	public:
		explicit PluginCfg();
		~PluginCfg();

		void FillPanelData(struct PanelData * data, PanelIndex index);
		void ReloadPanelString(struct PanelData * data, PanelIndex index);

		void GetPluginInfo(struct PluginInfo *info);
		int Configure(int itemNumber);
		bool LogEnable(void) const {return logEnable;};
};

#endif /* __CONFIGPLUGIN_H__ */
