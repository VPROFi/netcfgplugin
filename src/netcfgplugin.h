#ifndef __NETCFGPLUGIN_H__
#define __NETCFGPLUGIN_H__

#include "netif/netifs.h"
#include "plugincfg.h"

#include <farplug-wide.h>

class NetCfgPlugin {
	private:
		// Configuration
		PluginCfg * cfg;

		// Main functional
		NetInterfaces * nifs;

		// copy and assignment not allowed
		NetCfgPlugin(const NetCfgPlugin&) = delete;
		void operator=(const NetCfgPlugin&) = delete;

		// internal api
		const wchar_t * GetMsg(int msgId);

		const wchar_t * DublicateFileSizeString(unsigned long long value);
		const wchar_t * DublicateCountString(unsigned long long value);

	public:

		explicit NetCfgPlugin(const PluginStartupInfo * info);
		~NetCfgPlugin();

		// far2l backconnect
		static struct PluginStartupInfo psi;
		static struct FarStandardFunctions FSF;

		void Log(void);
		
		// far2l api
		int GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber);
		void FreeFindData(struct PluginPanelItem * PanelItem, int ItemsNumber);
		void GetPluginInfo(struct PluginInfo *info);

		HANDLE OpenPlugin(int openFrom, INT_PTR item);
		void ClosePlugin(HANDLE hPlugin);
		void GetOpenPluginInfo(HANDLE hPlugin, struct OpenPluginInfo *info);
		int ProcessKey(HANDLE hPlugin,int key,unsigned int controlState);
		int Configure(int itemNumber);
};

#endif /* __NETCFGPLUGIN_H__ */
