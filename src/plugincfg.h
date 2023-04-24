#ifndef __CONFIGPLUGIN_H__
#define __CONFIGPLUGIN_H__

#include <farplug-wide.h>

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
	CColumnDataIpIndex,
	CColumnDataMacIndex,
	CColumnDataIp6Index,
	CColumnDataMtuIndex,
	CColumnDataRecvBytesIndex,
	CColumnDataSendBytesIndex,
	CColumnDataRecvPktsIndex,
	CColumnDataSendPktsIndex,
	CColumnDataRecvErrsIndex,
	CColumnDataSendErrsIndex,
	CColumnDataMulticastIndex,
	CColumnDataCollisionsIndex,
	CColumnDataPermanentMacIndex,
	CColumnDataMaxIndex
};

class PluginCfg {

	private:
		const PluginStartupInfo * startupInfo;

		struct PanelMode PanelModesArray[PanelModeMax];
		struct KeyBarTitles keyBar;
		struct OpenPluginInfo openInfo;

		void CfgPanelModes(void);
		void CfgKeyBarTitles(void);

		bool addToDisksMenu;
		bool addToPluginsMenu;
		bool logEnable;

		const wchar_t * GetMsg(int msgId);

	public:
		explicit PluginCfg(const PluginStartupInfo * startupInfo);
		~PluginCfg();

		void CgfPluginData(void);
		void GetOpenPluginInfo(struct OpenPluginInfo *info);
		void GetPluginInfo(struct PluginInfo *info);
		int Configure(int itemNumber);
		bool LogEnable(void) const {return logEnable;};

};

#endif /* __CONFIGPLUGIN_H__ */
