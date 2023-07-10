#include "netroute.h"
#include "netrouteset.h"

#include <common/log.h>
#include <common/errname.h>
#include <common/netutils.h>
#include <common/sizestr.h>

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
}

#define LOG_SOURCE_FILE "netroute.cpp"
extern const char * LOG_FILE;

IpRouteInfo::IpRouteInfo()
{
	hoplimit = (uint32_t)-1;
	memset(&valid, 0, sizeof(valid));
#if !defined(__APPLE__) && !defined(__FreeBSD__)
	osdep.metric = 0;
	osdep.table = 0;
	osdep.tos = 0;
	osdep.protocol = 0;
	osdep.scope = 0;
	osdep.type = 0;
	osdep.icmp6pref = 0;
	memset(&osdep.enc, 0, sizeof(osdep.enc));
	memset(&osdep.rtcache, 0, sizeof(osdep.rtcache));
	memset(&osdep.rtmetrics, 0, sizeof(osdep.rtmetrics));
#else
	osdep.expire = 0;
#endif
}
IpRouteInfo::~IpRouteInfo()
{
//	LOG_INFO("\n");
}
ArpRouteInfo::ArpRouteInfo()
{
	flags = 0;
	#if !defined(__APPLE__) && !defined(__FreeBSD__)
	sa_family = AF_PACKET;
	hwType = 0;
	type = 0;
	state = 0;
	ci = {0};
	valid = {0};
	#else
	sa_family = AF_LINK;
	#endif
//	LOG_INFO("\n");
}
ArpRouteInfo::~ArpRouteInfo()
{
//	LOG_INFO("\n");
}


void IpRouteInfo::Log(void) const
{
#if !defined(__APPLE__) && !defined(__FreeBSD__)
	const char * fmt = sa_family == AF_INET ? \
		"from: %15S to: %18S via: %15S dev: %17S prefsrc: %15S table: %5s type: %11s proto: %6s scope: %7s metric: %4d perf: %6s tos: %3u flags: 0x%08X(%s)\n":
		"from: %15S to: %29S via: %15S dev: %17S prefsrc: %29S table: %5s type: %11s proto: %6s scope: %7s metric: %4d perf: %6s tos: %3u flags: 0x%08X(%s)\n";
	LOG_INFO(fmt,
		osdep.fromsrcIpandMask.empty() ? L"any":osdep.fromsrcIpandMask.c_str(),
		!destIpandMask.empty() ? destIpandMask.c_str():L"default",
		gateway.empty() ? L"default":gateway.c_str(),
		iface.c_str(),
		prefsrc.empty() ?	L"any":prefsrc.c_str(),
		rtruletable(osdep.table),
		rttype(osdep.type),
		rtprotocoltype(osdep.protocol),
		rtscopetype(osdep.scope),
		osdep.metric,
		rticmp6pref(osdep.icmp6pref),
		osdep.tos,
		flags, RouteFlagsToString(flags, sa_family == AF_INET6));
	//LogRtCache();
#else
	const char * fmt = sa_family == AF_INET ? \
		"to: %18S via: %15S dev: %17S prefsrc: %15S expire: %4d flags: 0x%08X(%s)\n":
		"to: %29S via: %15S dev: %17S prefsrc: %29S expire: %4d flags: 0x%08X(%s)\n";
	LOG_INFO(fmt,
		!destIpandMask.empty() ? destIpandMask.c_str():L"default",
		gateway.empty() ? L"default":gateway.c_str(),
		iface.c_str(),
		prefsrc.empty() ?	L"any":prefsrc.c_str(),
		osdep.expire,
		flags, RouteFlagsToString(flags, sa_family == AF_INET6));

#endif
}

#if !defined(__APPLE__) && !defined(__FreeBSD__)
void RuleRouteInfo::Log(void) const
{
	LOG_INFO("%s) %5d: %S\n", familyname(family), priority, rule.c_str());
}

// towstr can use only for ASCII symbols
static std::wstring towstr(const char * name)
{
	std::string _s(name);
	return std::wstring(_s.begin(), _s.end());
}

void RuleRouteInfo::ToRuleString(void)
{
	const size_t maxlen = sizeof("18446744073709551615");
	char s[maxlen] = {0};

	if( !rule.empty() )
		return;

	if( type == RTM_DELRULE )
		rule += L"Deleted ";

	if( flags & FIB_RULE_INVERT )
		rule += L"not ";

	if( valid.fromIpandMask )
		rule += L"from " + fromIpandMask;

	if( tos ) {
		if( snprintf(s, maxlen, "0x%02x", tos) > 0 )
			rule += L" tos " + towstr(s);
	}
	if( valid.fwmark || valid.fwmask ) {
		rule += L" fwmark ";
		if( snprintf(s, maxlen, "0x%x", fwmark) > 0 )
			rule += towstr(s);
		if( valid.fwmask ) {
			if( snprintf(s, maxlen, "0x%x", fwmask) > 0 )
				rule += L"/" + towstr(s);
		}
	}
	if( valid.iiface ) {
		rule += L" iif ";
		rule += iiface;
		if( flags & FIB_RULE_IIF_DETACHED )
			rule += L" [detached]";
	}
	if( valid.oiface ) {
		rule += L" oif " + oiface;
		if( flags & FIB_RULE_OIF_DETACHED )
			rule += L" [detached]";
	}

	if( valid.l3mdev )
		rule += L" l3mdev";

	if( valid.uid_range )
		rule += L" uidrange "+ std::to_wstring(uid_range.start) + L"-" + std::to_wstring(uid_range.end);

	if( valid.ip_protocol )
		rule += L" ipproto "+ towstr(iprotocolname(ip_protocol));

	if( valid.sport_range ) {
		if( sport_range.start == sport_range.end )
			rule += L" sport "+ std::to_wstring(sport_range.start);
		else
			rule += L" sport "+ std::to_wstring(sport_range.start) + L"-" + std::to_wstring(sport_range.end);
	}
	if( valid.dport_range ) {
		if( dport_range.start == dport_range.end )
			rule += L" dport "+ std::to_wstring(dport_range.start);
		else
			rule += L" dport "+ std::to_wstring(dport_range.start) + L"-" + std::to_wstring(dport_range.end);
	}

	if( valid.tun_id )
		rule += L" tun_id " + std::to_wstring(ntohll(tun_id)); 

	if( table ) {
		rule += L" table " + towstr(rtruletable(table));
		if( valid.suppress_prefixlength )
			rule += L" suppress_prefixlength " + std::to_wstring(suppress_prefixlength);
		if( valid.suppress_ifgroup )
			rule += L" suppress_ifgroup " + std::to_wstring(suppress_ifgroup);
	}

	if( valid.flowto ) {
		rule += L" realms ";
		if( valid.flowfrom )
			rule += std::to_wstring(flowfrom) + L"/";
		rule += std::to_wstring(flowto);
	}

	switch( action ) {
		case FR_ACT_TO_TBL:
			break;
		case FR_ACT_GOTO:
			if( valid.goto_priority )
				rule += L" goto " + std::to_wstring(goto_priority);
			else
				rule += L" goto none";
			if( flags & FIB_RULE_UNRESOLVED )
				rule += L" [unresolved]";
			break;
		case RTN_NAT:
			if( valid.gateway )
				rule += L" map-to "+ gateway;
			else
				rule += L" masquerade";
			break;
		default:
			rule += L" " + towstr(fractionrule(action));
			break;
	};

	if( valid.protocol )
		rule += L" proto " + towstr(rtprotocoltype(protocol));
}

void IpRouteInfo::LogRtCache(void) const
{
	static int hz = 0;

	if( !hz )
		hz = sysconf(_SC_CLK_TCK);

	if( !hz )
		hz = 100;

	if( osdep.rtcache.rta_clntref ||
	    osdep.rtcache.rta_lastuse ||
	    osdep.rtcache.rta_expires ||
	    osdep.rtcache.rta_error ||
	    osdep.rtcache.rta_used ||
	    osdep.rtcache.rta_id ||
	    osdep.rtcache.rta_ts ||
	    osdep.rtcache.rta_tsage ) {
	LOG_INFO("    rta_clntref: %u\n", osdep.rtcache.rta_clntref);
	LOG_INFO("    rta_lastuse: %u ( %s)\n", osdep.rtcache.rta_lastuse, msec_to_str(SEC_TO_MS(osdep.rtcache.rta_lastuse/hz)));
	LOG_INFO("    rta_expires: %d ( %s)\n", osdep.rtcache.rta_expires, msec_to_str(SEC_TO_MS(osdep.rtcache.rta_expires/hz)));
	LOG_INFO("    rta_error:   %d (%s)\n", osdep.rtcache.rta_error, errorname(osdep.rtcache.rta_error < 0 ? (-osdep.rtcache.rta_error):osdep.rtcache.rta_error));
	LOG_INFO("    rta_used:    %u\n", osdep.rtcache.rta_used);
	LOG_INFO("    rta_id:      0x%08X\n", osdep.rtcache.rta_id);
	LOG_INFO("    rta_ts:      %u\n", osdep.rtcache.rta_ts);
	LOG_INFO("    rta_tsage:   %u ( %s)\n", osdep.rtcache.rta_tsage, msec_to_str(SEC_TO_MS(osdep.rtcache.rta_tsage)));
	}
}

bool IpRouteInfo::ExecuteCommand(const char * cmd)
{
	assert( cmd != 0 );

	if( destIpandMask.empty() || ( iface.empty() && gateway.empty() ) )
		return false;

	char buffer[MAX_CMD_LEN], * ptr = buffer;
	size_t size = sizeof(buffer);
	int res = -1;

	if( iface == L"blackhole" || iface == L"unreachable" || iface == L"prohibit" || iface == L"throw" ) {
		if( snprintf(buffer, sizeof(buffer), "ip -%s route %s %S %S", sa_family == AF_INET ? "4":"6", cmd, iface.c_str(), destIpandMask.c_str()) > 0 )
			return RootExec(buffer) == 0;
	} else
		res = snprintf(buffer, sizeof(buffer), "ip -%s route %s %S", sa_family == AF_INET ? "4":"6", cmd, destIpandMask.c_str());

	if( res < 0 )
		return false;		
	ptr += res;
	size -= res;
	res = 0;
	if( !gateway.empty() && (res = snprintf(ptr, size, " via %S", gateway.c_str())) < 0 )
		return false;
	ptr += res;
	size -= res;
	res = 0;
	if( !iface.empty() && (res = snprintf(ptr, size, " dev %S", iface.c_str())) < 0 )
		return false;
	ptr += res;
	size -= res;
	res = 0;
	if( osdep.metric && (res = snprintf(ptr, size, " metric %u", osdep.metric)) < 0 )
		return false;
	ptr += res;
	size -= res;
	res = 0;
	if( osdep.table && (res = snprintf(ptr, size, " table %u", osdep.table)) < 0 )
		return false;

	return RootExec(buffer) == 0;
}
#else
bool IpRouteInfo::ExecuteCommand(const char * cmd)
{
	LOG_INFO("unsuported %s yet\n", cmd);
	return false;
}
#endif

bool IpRouteInfo::CreateIpRoute(void)
{
	LOG_INFO("\n");
	return ExecuteCommand("add");
}

bool IpRouteInfo::DeleteIpRoute(void)
{
	return ExecuteCommand("del");
}

bool IpRouteInfo::ChangeIpRoute(IpRouteInfo & ipr)
{
	if( DeleteIpRoute() ) {
		if( ipr.CreateIpRoute() ) {
			*this = ipr;
			return true;
		}
	}
	return false;
}

// TODO:
bool ArpRouteInfo::CreateArpRoute(void)
{
	LOG_ERROR("unsuported yet\n");
	return false;
}
bool ArpRouteInfo::DeleteArpRoute(void)
{
	LOG_ERROR("unsuported yet\n");
	return false;
}
bool ArpRouteInfo::ChangeArpRoute(ArpRouteInfo & arpr)
{
	LOG_ERROR("unsuported yet\n");
	return false;
}

void ArpRouteInfo::Log(void) const
{
#if !defined(__APPLE__) && !defined(__FreeBSD__)
	LOG_INFO("%8S: %28S      -> %26S       0x%08X(%s) 0x%02X\n", iface.c_str(), ip.c_str(), mac.c_str(), flags, RouteFlagsToString(flags, 0), hwType);
#else
	LOG_INFO("%8S: %28S      -> %26S       0x%08X(%s) %d\n", iface.c_str(), ip.c_str(), mac.c_str(), flags, RouteFlagsToString(flags, 0), ifnameIndex);
#endif
}

#if !defined(__APPLE__) && !defined(__FreeBSD__)
void ArpRouteInfo::LogCacheInfo(void) const
{
	static int hz = 0;

	if( !hz )
		hz = sysconf(_SC_CLK_TCK);

	if( !hz )
		hz = 100;

	if( valid.ci ) {
		LOG_INFO("    ndm_confirmed: %u ( %s)\n", ci.ndm_confirmed, msec_to_str(SEC_TO_MS(ci.ndm_confirmed/hz)));
		LOG_INFO("    ndm_used:      %u ( %s)\n", ci.ndm_used, msec_to_str(SEC_TO_MS(ci.ndm_used/hz)));
		LOG_INFO("    ndm_updated:   %u ( %s)\n", ci.ndm_updated, msec_to_str(SEC_TO_MS(ci.ndm_updated/hz)));
		LOG_INFO("    ndm_refcnt:    %u\n", ci.ndm_refcnt);
	}
}
#endif
