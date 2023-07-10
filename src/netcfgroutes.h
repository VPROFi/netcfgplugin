#ifndef __NETCFGROUTES_H__
#define __NETCFGROUTES_H__

#include "farpanel.h"
#include "netroute/netroutes.h"
#include <memory>

#define ROUTE_TABLE_DIRS static_cast<uint32_t>(-1)

class NetcfgIpRoute : public FarPanel
{
private:
	std::deque<IpRouteInfo> & inet;
	std::map<uint32_t, std::wstring> & ifs;

	#if !defined(__APPLE__) && !defined(__FreeBSD__)
	std::unique_ptr<PanelMode[]> tablemodes;
	uint32_t table;
	uint32_t dirIndex;
	uint32_t topIndex;
	int GetFindDataTables(struct PluginPanelItem **pPanelItem, int *pItemsNumber);
	#endif
	bool DeleteIpRoute(void);

	IpRouteInfo * rt;
	Encap new_enc;
	Encap new_nexthop_enc;
	bool EditIpRoute(void);

	friend LONG_PTR WINAPI EditIpRouteDialogProc(HANDLE hDlg, int msg, int param1, LONG_PTR param2);
	friend LONG_PTR WINAPI EditEncapDialogProc(HANDLE hDlg, int msg, int param1, LONG_PTR param2);
	void SelectInterface(HANDLE hDlg, uint32_t setIndex);
	void SelectFamily(HANDLE hDlg, uint32_t setIndex);
	void SelectEncap(HANDLE hDlg, Encap & enc, uint32_t setIndex);
public:
	int ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change) override;
	int GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber) override;
	void GetOpenPluginInfo(struct OpenPluginInfo * info) override;
	void FreeFindData(struct PluginPanelItem * panelItem, int itemsNumber) override;
	explicit NetcfgIpRoute(uint32_t index, std::deque<IpRouteInfo> & inet, std::map<uint32_t, std::wstring> & ifs);
	~NetcfgIpRoute();
};

class NetcfgArpRoute : public FarPanel
{
private:
	std::deque<ArpRouteInfo> & arp;
public:
	int ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change) override;
	int GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber) override;
	explicit NetcfgArpRoute(uint32_t index, std::deque<ArpRouteInfo> & arp);
	~NetcfgArpRoute();
};

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
	void FreeFindData(struct PluginPanelItem * panelItem, int itemsNumber);
	void GetOpenPluginInfo(struct OpenPluginInfo * info);
	explicit NetcfgRoutes();
	~NetcfgRoutes();
};

#endif // __NETCFGROUTES_H__
