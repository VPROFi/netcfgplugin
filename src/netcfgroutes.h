#ifndef __NETCFGROUTES_H__
#define __NETCFGROUTES_H__

#include "farpanel.h"
#include "netroute/netroutes.h"
#include "netcfgiproutes.h"
#include "netcfgrules.h"
#include "netcfgarp.h"
#include <memory>

#define SET_ACTIVE_ROUTE_PANEL(active_) active = (active_ - RouteInetPanelIndex)
#define GET_ACTIVE_ROUTE_PANEL() (active + RouteInetPanelIndex)

class NetcfgRoutes : public FarPanel
{
private:
	std::unique_ptr<NetRoutes> nrts;

	uint32_t active;
	std::vector<std::unique_ptr<FarPanel>> panels;

	bool autoAppendMcRoutes;
	bool autoAppendMc6Routes;
	
	// copy and assignment not allowed
	NetcfgRoutes(const NetcfgRoutes&) = delete;
	void operator=(const NetcfgRoutes&) = delete;

public:
	int ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change) override;
	int GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber) override;
	void FreeFindData(struct PluginPanelItem * panelItem, int itemsNumber) override;
	void GetOpenPluginInfo(struct OpenPluginInfo * info) override;
	explicit NetcfgRoutes();
	virtual ~NetcfgRoutes();
};

#endif // __NETCFGROUTES_H__
