#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include "netcfgplugin.h"
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

NetcfgRoutes::NetcfgRoutes():
	FarPanel()
{
	LOG_INFO("\n");

	nrts = std::make_unique<NetRoutes>();

	nrts->Update();

	#if !defined(__APPLE__) && !defined(__FreeBSD__)
	panels.push_back(std::make_unique<NetcfgIpRoute>(RouteInetPanelIndex, AF_INET, nrts->inet, nrts->rule, nrts->ifs));
	panels.push_back(std::make_unique<NetcfgIpRoute>(RouteInet6PanelIndex, AF_INET6, nrts->inet6, nrts->rule6, nrts->ifs));
	#else
	panels.push_back(std::make_unique<NetcfgIpRoute>(RouteInetPanelIndex, AF_INET, nrts->inet, nrts->ifs));
	panels.push_back(std::make_unique<NetcfgIpRoute>(RouteInet6PanelIndex, AF_INET6, nrts->inet6, nrts->ifs));
	#endif

	panels.push_back(std::make_unique<NetcfgArpRoute>(RouteArpPanelIndex, nrts->arp, nrts->ifs));

	autoAppendMcRoutes = false;
	autoAppendMc6Routes = false;

	#if !defined(__APPLE__) && !defined(__FreeBSD__)
	panels.push_back(std::make_unique<NetcfgIpRoute>(RouteMcInetPanelIndex, RTNL_FAMILY_IPMR, nrts->mcinet, nrts->mcrule, nrts->ifs));
	panels.push_back(std::make_unique<NetcfgIpRoute>(RouteMcInet6PanelIndex, RTNL_FAMILY_IP6MR, nrts->mcinet6, nrts->mcrule6, nrts->ifs));
/*	if( nrts->mcinet.size() )
		panels.push_back(std::make_unique<NetcfgIpRoute>(RouteMcInetPanelIndex, RTNL_FAMILY_IPMR, nrts->mcinet, nrts->mcrule, nrts->ifs));
	else
		autoAppendMcRoutes = true;
		
	if( nrts->mcinet6.size() )
		panels.push_back(std::make_unique<NetcfgIpRoute>(RouteMcInet6PanelIndex, RTNL_FAMILY_IP6MR, nrts->mcinet6, nrts->mcrule6, nrts->ifs));
	else
		autoAppendMc6Routes = true;*/
	#endif

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

			#if !defined(__APPLE__) && !defined(__FreeBSD__)
			if( autoAppendMcRoutes && nrts->mcinet.size() ) {
				panels.push_back(std::make_unique<NetcfgIpRoute>(RouteMcInetPanelIndex, RTNL_FAMILY_IPMR, nrts->mcinet, nrts->mcrule, nrts->ifs));
				autoAppendMcRoutes = false;
			}
			if( autoAppendMc6Routes && nrts->mcinet6.size() ) {
				panels.push_back(std::make_unique<NetcfgIpRoute>(RouteMcInet6PanelIndex, RTNL_FAMILY_IP6MR, nrts->mcinet6, nrts->mcrule6, nrts->ifs));
				autoAppendMc6Routes = false;
			}
			#endif

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
