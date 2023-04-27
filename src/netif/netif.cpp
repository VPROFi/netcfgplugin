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
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
}

#if !defined(__APPLE__) && !defined(__FreeBSD__)

#include <linux/sysctl.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <unistd.h>

#else

#include <sys/sysctl.h>
#include <net/route.h>
#include <net/if_var.h>
#include <net/if_dl.h>

#endif

#ifndef IFF_LOWER_UP
#define IFF_LOWER_UP 0x10000
#endif //IFF_LOWER_UP

#ifndef IFF_DORMANT
#define IFF_DORMANT 0x20000
#endif //IFF_DORMANT

#ifndef IFF_ECHO
#define IFF_ECHO 0x40000
#endif //IFF_ECHO

NetInterface::NetInterface(std::wstring name):
name(name),
size(sizeof(NetInterface)),
tcpdumpOn(false)
{
}

NetInterface::~NetInterface()
{
}


bool NetInterface::UpdateStats(void)
{
	std::string iface(name.begin(), name.end());
	bool res = false;

	#if !defined(__APPLE__) && !defined(__FreeBSD__)

	FILE * netdev = fopen("/proc/net/dev", "r");
	if( !netdev ) {
		LOG_ERROR("can`t open \"/proc/net/dev\" ... error (%s) ", errorname(errno));
		return false;
	}

	uint64_t unused;
	char line[512];
	char ifname[MAX_INTERFACE_NAME_LEN + 1];
	if( fgets(line, sizeof(line), netdev) != 0 ) {
		while( fgets(line, sizeof(line), netdev) != 0 && \
			fscanf(netdev, "%s", ifname) > 0 ) {
			if( strlen(ifname) == (iface.size() + 1) && \
			    strncmp(iface.c_str(), ifname, iface.size()) == 0 ) {
				res = (fscanf(netdev, "%lu %lu %lu %lu %lu %lu %lu %lu "
						"%lu %lu %lu %lu %lu %lu %lu %lu\n",
						   &recv_bytes, &recv_packets,
						   &recv_errors, &unused, &unused,
						   &unused, &unused, &multicast,
						   &send_bytes, &send_packets,
						   &send_errors, &unused,
						   &unused, &collisions,
						   &unused,
						   &unused) == 16);
				break;
			}
		}
	}

	fclose(netdev);

	if( !res )
		LOG_ERROR("stats for %S not found\n", iface.c_str());
	else
		LOG_INFO("%16s: stats update ... success\n", iface.c_str());

	// add mtu mac permanent etc

	int isocket = socket(AF_INET, SOCK_DGRAM, 0);
	if( isocket < 0 ) {
		LOG_ERROR("socket(AF_INET, SOCK_STREAM, 0) ... error (%s)\n", errorname(errno));
		return false;
	}

	struct ifreq paifr;
	memset(&paifr, 0, sizeof(paifr));
	strncpy(paifr.ifr_name, iface.c_str(), iface.size());
	if( ioctl(isocket, SIOCGIFMTU, &paifr) != -1 )
		mtu = paifr.ifr_mtu;
	else
		LOG_ERROR("ioctl(SIOCGIFMTU) ... error (%s)\n", errorname(errno));

	struct ethtool_perm_addr * epa = (struct ethtool_perm_addr*)malloc(sizeof(struct ethtool_perm_addr) + IFHWADDRLEN);
	if( epa ) {
		epa->cmd = ETHTOOL_GPERMADDR;
		epa->size = IFHWADDRLEN;
		paifr.ifr_data = (caddr_t)epa;
		if( ioctl(isocket, SIOCETHTOOL, &paifr) >= 0 ) {
			char s[INET6_ADDRSTRLEN > INET_ADDRSTRLEN ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN] = {0};
			if( snprintf(s, sizeof(s), "%02x:%02x:%02x:%02x:%02x:%02x", \
					epa->data[0], epa->data[1], epa->data[2], \
					epa->data[3],epa->data[4],epa->data[5]) > 0 ) {
				std::string _name(s);
				permanent_mac = std::wstring(_name.begin(), _name.end());
			}
		} else
			LOG_ERROR("ioctl(SIOCETHTOOL) ... error (%s)\n", errorname(errno));

		free(epa);
	}

	close(isocket);

	#else

	int mib[6] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0};
	char * buf = 0;
	size_t size = 0;

	if( sysctl(mib, sizeof(mib)/sizeof(mib[0]), 0, &size, 0, 0) < 0 ) {
		LOG_ERROR("sysctl(CTL_NET, PF_ROUTE, NET_RT_IFLIST2) ... error (%s)\n", errorname(errno));
		return false;
	}

	buf = (char *)malloc(size);
	if( !buf ) {
		LOG_ERROR("can`t allocate %lu bytes\n", size);
	}
	do {
		if( sysctl(mib, sizeof(mib)/sizeof(mib[0]), buf, &size, 0, 0) < 0 ) {
			LOG_ERROR("sysctl(CTL_NET, PF_ROUTE, NET_RT_IFLIST2) ... error (%s)\n", errorname(errno));
			break;
		}
		char *lim, *next;
		lim = buf + size;
		for (next = buf; next < lim; ) {
			struct if_msghdr * ifm = (struct if_msghdr *)next;
			next += ifm->ifm_msglen;
			if( ifm->ifm_type == RTM_IFINFO2 ) {
				struct if_msghdr2 *if2m = (struct if_msghdr2 *)ifm;
				struct sockaddr_dl *sdl = (struct sockaddr_dl *)(if2m + 1);
				if( sdl->sdl_nlen == iface.size() && strncmp(iface.c_str(), sdl->sdl_data, iface.size()) == 0 ) {
					LOG_INFO("%16s: interface ... found\n", iface.c_str());
					ifa_flags = if2m->ifm_flags;
					send_packets = if2m->ifm_data.ifi_opackets;
					recv_packets = if2m->ifm_data.ifi_ipackets;
					send_bytes = if2m->ifm_data.ifi_obytes;
					recv_bytes = if2m->ifm_data.ifi_ibytes;
					send_errors =if2m->ifm_data.ifi_oerrors;
					recv_errors = if2m->ifm_data.ifi_ierrors;
					collisions = if2m->ifm_data.ifi_collisions;
					multicast = if2m->ifm_data.ifi_imcasts + if2m->ifm_data.ifi_omcasts;
					mtu = if2m->ifm_data.ifi_mtu;
				}

			}
		}

	} while(0);

	free(buf);
	
	#endif

	return res;
}

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
bool NetInterface::IsCarrierOn()
{
	#if !defined(__APPLE__) && !defined(__FreeBSD__)	
	return (ifa_flags & IFF_RUNNING) && (ifa_flags & IFF_LOWER_UP);
	#else
	return (ifa_flags & IFF_RUNNING);
	#endif
}
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

#if !defined(__APPLE__) && !defined(__FreeBSD__)
	if( snprintf(buffer, MAX_CMD_LEN, CHANGEMAC_CMD, name.c_str(), name.c_str(), newmac, name.c_str()) > 0 )
#else
	if( snprintf(buffer, MAX_CMD_LEN, CHANGEMAC_CMD, name.c_str(), newmac, name.c_str(), name.c_str()) > 0 )
#endif
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

	std::string _mask(it->second.netmask.begin(), it->second.netmask.end());
	int maskbits = IpMaskToBits(_mask.c_str());

	char buffer[MAX_CMD_LEN];
#if !defined(__APPLE__) && !defined(__FreeBSD__)
	if( snprintf(buffer, MAX_CMD_LEN, DELETEIP_CMD, delip, maskbits, name.c_str()) > 0 )
#else
	if( snprintf(buffer, MAX_CMD_LEN, DELETEIP_CMD, name.c_str(), delip, maskbits) > 0 )
#endif
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

	std::string _mask(it->second.netmask.begin(), it->second.netmask.end());
	int maskbits = Ip6MaskToBits(_mask.c_str());

#if !defined(__APPLE__) && !defined(__FreeBSD__)
	if( snprintf(buffer, MAX_CMD_LEN, DELETEIP6_CMD, delip, maskbits, name.c_str()) > 0 )
#else
	if( snprintf(buffer, MAX_CMD_LEN, DELETEIP6_CMD, name.c_str(), delip, maskbits) > 0 )
#endif
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

	if( *bcip == 0 || *bcip == 0x20 )
		bcip = L"255.255.255.255";

	if( *mask == 0 || *mask == 0x20 )
		mask = L"255.255.255.0";

	std::string _mask(mask, mask+wcslen(mask));
	int maskbits = IpMaskToBits(_mask.c_str());

	char buffer[MAX_CMD_LEN];
#if !defined(__APPLE__) && !defined(__FreeBSD__)
	if( snprintf(buffer, MAX_CMD_LEN, ADDIP_CMD, newip, maskbits, bcip, name.c_str()) > 0 )
#else
	if( snprintf(buffer, MAX_CMD_LEN, ADDIP_CMD, name.c_str(), newip, maskbits, bcip) > 0 )
#endif
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

	std::string _mask(mask, mask+wcslen(mask));
	int maskbits = Ip6MaskToBits(_mask.c_str());

#if !defined(__APPLE__) && !defined(__FreeBSD__)
	if( snprintf(buffer, MAX_CMD_LEN, ADDIP6_CMD, newip, maskbits, name.c_str()) > 0 )
#else
	if( snprintf(buffer, MAX_CMD_LEN, ADDIP6_CMD, name.c_str(), newip, maskbits) > 0 )
#endif
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

	if( *mask == 0 || *mask == 0x20 )
		mask = L"255.255.255.0";

	std::string _mask(mask, mask+wcslen(mask));
	int maskbits = IpMaskToBits(_mask.c_str());
	_mask = std::string(it->second.netmask.begin(), it->second.netmask.end());
	int oldmaskbits = IpMaskToBits(_mask.c_str());

	if( *bcip == 0 || *bcip == 0x20 )
		bcip = L"255.255.255.255";

	char buffer[MAX_CMD_LEN];
#if !defined(__APPLE__) && !defined(__FreeBSD__)
	if( snprintf(buffer, MAX_CMD_LEN, CHANGEIP_CMD, oldip, oldmaskbits, name.c_str(), newip, maskbits, bcip, name.c_str()) > 0 )
#else
	if( snprintf(buffer, MAX_CMD_LEN, CHANGEIP_CMD, name.c_str(), oldip, oldmaskbits, name.c_str(), newip, maskbits, bcip) > 0 )
#endif
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
	_mask = std::string(it->second.netmask.begin(), it->second.netmask.end());
	int oldmaskbits = Ip6MaskToBits(_mask.c_str());

	char buffer[MAX_CMD_LEN];
#if !defined(__APPLE__) && !defined(__FreeBSD__)
	if( snprintf(buffer, MAX_CMD_LEN, CHANGEIP6_CMD, oldip, oldmaskbits, name.c_str(), newip, maskbits, name.c_str()) > 0 )
#else
	if( snprintf(buffer, MAX_CMD_LEN, CHANGEIP6_CMD, name.c_str(), oldip, oldmaskbits, name.c_str(), newip, maskbits) > 0 )
#endif
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

bool NetInterface::TcpDumpStart(const wchar_t * file, bool promisc)
{
	LOG_INFO("file: %S promisc: %s\n", file, promisc ? "true":"false");
	SetPromisc(promisc);
	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, TCPDUMP_CMD, name.c_str(), file, name.c_str(), file) > 0 )
		tcpdumpOn = (RootExec(buffer) == 0);
	return tcpdumpOn;
}

void NetInterface::TcpDumpStop(void)
{
	LOG_INFO("\n");
	if( !tcpdumpOn )
		return;
	char buffer[MAX_CMD_LEN];
	if( snprintf(buffer, MAX_CMD_LEN, TCPDUMPKILL_CMD, name.c_str()) > 0 )
		tcpdumpOn = !(RootExec(buffer) == 0);
	return;
}
