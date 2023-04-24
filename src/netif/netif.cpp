#include "netif.h"
#include "netifset.h"

#include <common/log.h>
#include <common/errname.h>

#define LOG_SOURCE_FILE "netif.cpp"
#ifndef MAIN_NETIF
extern const char * LOG_FILE;
#else
#define LOG_FILE ""
#endif

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
}

#ifndef IFF_LOWER_UP
#define IFF_LOWER_UP 0x10000
#endif //IFF_LOWER_UP

#ifndef IFF_DORMANT
#define IFF_DORMANT 0x20000
#endif //IFF_DORMANT

#ifndef IFF_ECHO
#define IFF_ECHO 0x40000
#endif //IFF_ECHO

bool NetInterface::IsUp() { return ifa_flags & IFF_UP; };
bool NetInterface::IsBroadcast() { return ifa_flags & IFF_BROADCAST; };
bool NetInterface::IsDebug() { return ifa_flags & IFF_DEBUG; };
//bool NetInterface::SetDebug(bool on) { return ifa_flags & IFF_DEBUG; };
bool NetInterface::IsLoopback() { return ifa_flags & IFF_LOOPBACK; };
bool NetInterface::IsPointToPoint() { return ifa_flags & IFF_POINTOPOINT; };
bool NetInterface::IsNotrailers() { return ifa_flags & IFF_NOTRAILERS; };
//bool NetInterface::SetNotrailers() { return ifa_flags & IFF_NOTRAILERS; };
bool NetInterface::IsRunning() { return ifa_flags & IFF_RUNNING; };
bool NetInterface::IsNoARP() { return ifa_flags & IFF_NOARP; };
bool NetInterface::IsPromisc() { return ifa_flags & IFF_PROMISC; };
bool NetInterface::IsMulticast() { return ifa_flags & IFF_MULTICAST; };
bool NetInterface::IsAllmulticast() { return ifa_flags & IFF_ALLMULTI; };
bool NetInterface::IsCarrierOn() { return (ifa_flags & IFF_RUNNING) && (ifa_flags & IFF_LOWER_UP); };
bool NetInterface::IsDormant() { return ifa_flags & IFF_DORMANT; };
bool NetInterface::IsEcho() { return ifa_flags & IFF_ECHO; };
//bool NetInterface::SetEcho(bool on) { return ifa_flags & IFF_ECHO; };

int NetInterface::Ip6MaskToBits(const char * mask)
{
	int masklen = strlen(mask), bits = 0;
	if( masklen < sizeof(char) )
		return 0;

	if( masklen < sizeof("/128") && *mask != ':' ) {
		bits = (*mask == 0x2F) ? atoi(mask+1):atoi(mask);
		return bits <= 128 ? bits:0;
	}

	struct in6_addr addr;
	inet_pton(AF_INET6, mask, &addr);
	for (int i = 0; i < 16; i++) {
		while (addr.s6_addr[i]) {
			bits += addr.s6_addr[i] & 1;
			addr.s6_addr[i] >>= 1;
		}
	}
	return bits;
}

int NetInterface::IpMaskToBits(const char * mask)
{
	int masklen = strlen(mask), bits = 0;
	if( masklen < sizeof(char) )
		return 0;

	if( masklen < sizeof("/32") ) {
		bits = (*mask == 0x2F) ? atoi(mask+1):atoi(mask);
		return bits <= 32 ? bits:0;
	}

	struct in_addr addr;
	inet_pton(AF_INET, mask, &addr);
	while (addr.s_addr) {
		bits += addr.s_addr & 1;
		addr.s_addr >>= 1;
	}
	return bits;
}


const char * NetInterface::IpBitsToMask(uint32_t bits, char * buffer, size_t maxlen)
{
    int mask = 0xffffffff << (32 - bits);
    if( bits < 32 && snprintf(buffer, maxlen, "%d.%d.%d.%d", (mask >> 24) & 0xff, (mask >> 16) & 0xff, (mask >> 8) & 0xff, mask & 0xff) > 0 )
	return buffer;
    return "0.0.0.0";
}

int RootExec(const char * cmd);

bool NetInterface::SetInterfaceName(const wchar_t * newname)
{
	if( name == newname )
		return false;

	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, CHANGEIFNAME_CMD, name.c_str(), name.c_str(), newname, newname) > 0 )
		if( RootExec(buffer) == 0 ) {
			name = newname; // we need update ifname for next commands
			return true;
		}
	return false;
}

bool NetInterface::SetMac(const wchar_t * newmac)
{
	if( mac.find(newmac) != mac.end() )
		return false;
	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, CHANGEMAC_CMD, name.c_str(), name.c_str(), newmac, name.c_str()) > 0 )
		if( RootExec(buffer) == 0 )
			return true;
	return false;
}

bool NetInterface::SetUp(bool on)
{
	if( (IsUp() && on) || (!IsUp() && !on) )
		return false;

	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, on ? SETUP_CMD:SETDOWN_CMD, name.c_str()) > 0 )
		if( RootExec(buffer) == 0 )
			return true;
	return false;
}

bool NetInterface::SetMulticast(bool on)
{
	if( (IsMulticast() && on) || (!IsMulticast() && !on) )
		return false;
	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, on ? SETMULTICASTON_CMD:SETMULTICASTOFF_CMD, name.c_str()) > 0 )
		if( RootExec(buffer) == 0 )
			return true;
	return false;
}

bool NetInterface::SetAllmulticast(bool on)
{
	if( (IsAllmulticast() && on) || (!IsAllmulticast() && !on) )
		return false;
	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, on ? SETALLMULTICASTON_CMD:SETALLMULTICASTOFF_CMD, name.c_str()) > 0 )
		if( RootExec(buffer) == 0 )
			return true;
	return false;
}

bool NetInterface::SetNoARP(bool on)
{
	if( (IsNoARP() && on) || (!IsNoARP() && !on) )
		return false;
	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, on ? SETARPON_CMD:SETARPOFF_CMD, name.c_str()) > 0 )
		if( RootExec(buffer) == 0 )
			return true;
	return false;
}

bool NetInterface::SetPromisc(bool on)
{
	if( (IsPromisc() && on) || (!IsPromisc() && !on) )
		return false;
	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, on ? SETPROMISCON_CMD:SETPROMISCOFF_CMD, name.c_str()) > 0 )
		if( RootExec(buffer) == 0 )
			return true;
	return false;
}

bool NetInterface::SetMtu(uint32_t newmtu)
{
	if( mtu == newmtu )
		return false;
	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, SETMTU_CMD, name.c_str(), newmtu) > 0 )
		if( RootExec(buffer) == 0 )
			return true;
	return false;
}

bool NetInterface::DeleteIp(const wchar_t * delip)
{
	auto it = ip.find(delip);
	if( it == ip.end() )
		return false;
	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, DELETEIP_CMD, delip, it->second.netmask.c_str(), name.c_str()) > 0 )
		if( RootExec(buffer) == 0 ) {
			ip.erase(it);
			return true;
		}
	return false;
}

bool NetInterface::DeleteIp6(const wchar_t * delip)
{
	auto it = ip6.find(delip);
	if( it == ip6.end() )
		return false;
	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, DELETEIP6_CMD, delip, it->second.netmask.c_str(), name.c_str()) > 0 )
		if( RootExec(buffer) == 0 ) {
			ip6.erase(it);
			return true;
		}
	return false;
}

bool NetInterface::AddIp(const wchar_t * newip, const wchar_t * mask, const wchar_t * bcip)
{
	if( ip.find(newip) != ip.end() )
		return false;
	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, ADDIP_CMD, newip, mask, bcip, name.c_str()) > 0 )
		if( RootExec(buffer) == 0 ) {
			// we need update ip and mask for next commands
			IpAddressInfo _ip;
			_ip.ifa_flags = IFF_BROADCAST;
			_ip.ip = newip;
			_ip.netmask = mask;
			ip[newip] = _ip;
			return true;
		}
	return false;
}

bool NetInterface::AddIp6(const wchar_t * newip, const wchar_t * mask)
{
	if( ip6.find(newip) != ip6.end() )
		return false;
	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, ADDIP6_CMD, newip, mask, name.c_str()) > 0 )
		if( RootExec(buffer) == 0 ) {
			// we need update ip and mask for next commands
			IpAddressInfo _ip;
			_ip.ifa_flags = 0;
			_ip.ip = newip;
			_ip.netmask = mask;
			ip6[newip] = _ip;
			return true;
		}
	return false;
}


bool NetInterface::ChangeIp(const wchar_t * oldip, const wchar_t * newip, const wchar_t * mask, const wchar_t * bcip)
{
	auto it = ip.find(oldip);
	if( it == ip.end() )
		return false;

	std::string _mask(mask, mask+wcslen(mask));
	int maskbits = IpMaskToBits(_mask.c_str());

	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, CHANGEIP_CMD, oldip, it->second.netmask.c_str(), name.c_str(), newip, maskbits, bcip, name.c_str()) > 0 )
		if( RootExec(buffer) == 0 ) {
			// we need update ip and mask for next commands
			IpAddressInfo _ip;
			_ip.ifa_flags = it->second.ifa_flags;
			_ip.ip = newip;
			std::string _ipmask(IpBitsToMask((uint32_t)maskbits, buffer, sizeof(buffer)));
			_ip.netmask = std::wstring(_ipmask.begin(),_ipmask.end());
			ip.erase(it);
			ip[newip] = _ip;
			return true;
		}
	return false;
}

bool NetInterface::ChangeIp6(const wchar_t * oldip, const wchar_t * newip, const wchar_t * mask)
{
	auto it = ip6.find(oldip);
	if( it == ip6.end() )
		return false;

	std::string _mask(mask, mask+wcslen(mask));
	int maskbits = Ip6MaskToBits(_mask.c_str());

	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, CHANGEIP6_CMD, oldip, it->second.netmask.c_str(), name.c_str(), newip, maskbits, name.c_str()) > 0 )
		if( RootExec(buffer) == 0 ) {
			// we need update ip and mask for next commands
			IpAddressInfo _ip;
			_ip.ifa_flags = it->second.ifa_flags;
			_ip.ip = newip;
			_ip.netmask = mask;
			// delete old ip
			ip6.erase(it);
			ip6[newip] = _ip;
			return true;
		}
	return false;
}
