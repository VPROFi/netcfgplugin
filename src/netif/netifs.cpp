#include "netifs.h"

#include <common/log.h>
#include <common/errname.h>

// Apple MacOS, FreeBSD and Linux has ifaddrs.h
extern "C" {
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
}

#ifndef MAIN_NETIF
extern const char * LOG_FILE;
#else
#define LOG_FILE ""
#endif

#define LOG_SOURCE_FILE "netifs.cpp"

#if !defined(__APPLE__) && !defined(__FreeBSD__)
#include <netpacket/packet.h>
#include <linux/sockios.h>
#include <linux/if_link.h>
#include <linux/ethtool.h>

#else
struct sockaddr_dl {
	u_char	sdl_len;	/* Total length of sockaddr */
	u_char	sdl_family;	/* AF_LINK */
	u_short	sdl_index;	/* if != 0, system given index for interface */
	u_char	sdl_type;	/* interface type */
	u_char	sdl_nlen;	/* interface name length, no trailing 0 reqd. */
	u_char	sdl_alen;	/* link level address length */
	u_char	sdl_slen;	/* link layer selector length */
	char	sdl_data[46];	/* minimum work area, can be larger;
				   contains both if name and ll address */
};
#include <net/if_var.h>
#include <net/if_types.h>
#include <sys/sockio.h>

#endif

static const char * get_sockaddr_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
	if( !sa )
		return "";

	switch( sa->sa_family ) {

	case AF_INET:
		return inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), s, maxlen);
	case AF_INET6:
            	return inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), s, maxlen);
#if !defined(__APPLE__) && !defined(__FreeBSD__)
	case AF_PACKET:
	{
		struct sockaddr_ll * sll = (struct sockaddr_ll *)sa;
		if( snprintf(s, maxlen, "%02x:%02x:%02x:%02x:%02x:%02x", \
			sll->sll_addr[0], sll->sll_addr[1], sll->sll_addr[2], \
			sll->sll_addr[3], sll->sll_addr[4], sll->sll_addr[5]) > 0 )
			return s;
		break;
	}
#else
	case AF_LINK:
	{
		struct sockaddr_dl * sdl = (struct sockaddr_dl *)sa;
		unsigned char * mac = (unsigned char *)sdl->sdl_data + sdl->sdl_nlen;
		if( snprintf(s, maxlen, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]) > 0 )
			return s;
		break;
	}
#endif
	default:
		LOG_ERROR("unsupported sa_family: %u\n", sa->sa_family);
		break;
    }
    return "";
}

NetInterface * NetInterfaces::Add(const char * name)
{
	std::string _name(name);
	std::wstring _wname(_name.begin(), _name.end());
	if( ifs.find(_wname) == ifs.end() ) {
		ifs[_wname] = new NetInterface();
		ifs[_wname]->size = sizeof(NetInterface);
		ifs[_wname]->name = _wname;
	}
	return ifs[_wname];
}

void NetInterfaces::Clear(void) {
	for( const auto& [name, net_if] : ifs )
		delete net_if;
	ifs.clear();
}

int NetInterfaces::Update(void)
{
	LOG_INFO("\n");

	struct ifaddrs *ifa_base, *ifa;
	if( getifaddrs(&ifa_base) != 0 ) {
		LOG_ERROR("getifaddrs(&ifa_base) ... error (%s) ", errorname(errno));
		return -1;
	}

	for (ifa = ifa_base; ifa; ifa = ifa->ifa_next) {
		if( !ifa->ifa_name )
			continue;

		NetInterface * net_if = Add(ifa->ifa_name);
	
		char s[INET6_ADDRSTRLEN > INET_ADDRSTRLEN ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN] = {0};

		IpAddressInfo ip;
		ip.ifa_flags = ifa->ifa_flags;
		std::string _name(get_sockaddr_str((const struct sockaddr *)ifa->ifa_addr, s, sizeof(s)));
		ip.ip = std::wstring(_name.begin(), _name.end());
		_name = get_sockaddr_str((const struct sockaddr *)ifa->ifa_netmask, s, sizeof(s));
		ip.netmask = std::wstring(_name.begin(), _name.end());
		if( ifa->ifa_flags & IFF_BROADCAST ) {
			_name = get_sockaddr_str((const struct sockaddr *)ifa->ifa_broadaddr, s, sizeof(s));
			ip.broadcast = std::wstring(_name.begin(), _name.end());
		} else if( ifa->ifa_flags & IFF_POINTOPOINT && ifa->ifa_dstaddr && ifa->ifa_dstaddr->sa_family != 0 ) {
			_name = get_sockaddr_str((const struct sockaddr *)ifa->ifa_dstaddr, s, sizeof(s));
			ip.point_to_point = std::wstring(_name.begin(), _name.end());
		}

		switch( ifa->ifa_addr->sa_family ) {

		case AF_INET:
			net_if->ip[ip.ip] = ip;
			break;
		case AF_INET6:
			
			if( snprintf(s, sizeof(s), "%u", net_if->Ip6MaskToBits(get_sockaddr_str((const struct sockaddr *)ifa->ifa_netmask, s, sizeof(s)))) > 0 ) {
				_name = s;
				ip.netmask = std::wstring(_name.begin(), _name.end());
			}
			net_if->ip6[ip.ip] = ip;
			break;
		#if !defined(__APPLE__) && !defined(__FreeBSD__)
		case AF_PACKET:
		#else
		case AF_LINK:
		#endif
		{
			MacAddressInfo mac;

			// get_sockaddr_str can output mac addresses - we can use some data
			mac.ifa_flags = ip.ifa_flags;
			net_if->ifa_flags = ip.ifa_flags;
			mac.mac = ip.ip;
			mac.broadcast = ip.broadcast;
			net_if->mac[mac.mac] = mac;

			net_if->permanent_mac = mac.mac;
			break;
		}
		default:
			LOG_ERROR("unsupported sa_family %u\n", ifa->ifa_addr->sa_family);
			break;
		}

		if( ifa->ifa_data ) {
			#if !defined(__APPLE__) && !defined(__FreeBSD__)
			struct rtnl_link_stats *stats = (struct rtnl_link_stats *)ifa->ifa_data;
			net_if->recv_bytes = stats->rx_bytes;
			net_if->send_bytes = stats->tx_bytes;
			net_if->recv_packets = stats->rx_packets;
			net_if->send_packets = stats->tx_packets;
			net_if->recv_errors = stats->rx_errors;
			net_if->send_errors = stats->tx_errors;
			net_if->multicast = stats->multicast;
			net_if->collisions = stats->collisions;

			int isocket = socket(AF_INET, SOCK_DGRAM, 0);
			if( isocket < 0 ) {
				LOG_ERROR("socket(AF_INET, SOCK_STREAM, 0) ... error (%s) ", errorname(errno));
				continue;
			}

			struct ifreq paifr;
			memset(&paifr, 0, sizeof(paifr));
			strncpy(paifr.ifr_name, ifa->ifa_name, sizeof(paifr.ifr_name));
			if( ioctl(isocket, SIOCGIFMTU, &paifr) != -1 )
				net_if->mtu = paifr.ifr_mtu;
			else
				LOG_ERROR("ioctl(SIOCGIFMTU) ... error (%s) ", errorname(errno));

			struct ethtool_perm_addr * epa = (struct ethtool_perm_addr*)malloc(sizeof(struct ethtool_perm_addr) + IFHWADDRLEN);
			if( epa ) {
				epa->cmd = ETHTOOL_GPERMADDR;
				epa->size = IFHWADDRLEN;
				paifr.ifr_data = (caddr_t)epa;
				if( ioctl(isocket, SIOCETHTOOL, &paifr) >= 0 ) {
					if( snprintf(s, sizeof(s), "%02x:%02x:%02x:%02x:%02x:%02x", \
							epa->data[0], epa->data[1], epa->data[2], \
							epa->data[3],epa->data[4],epa->data[5]) > 0 ) {
						//std::string _name(s);
						_name = s;
						net_if->permanent_mac = std::wstring(_name.begin(), _name.end());
					}
				} else
					LOG_ERROR("ioctl(SIOCETHTOOL) ... error (%s) ", errorname(errno));

				free(epa);
			}
			#else
			struct if_data *stats = (struct if_data *)ifa->ifa_data;
			net_if->mtu = stats->ifi_mtu;
			// TODO: permanent mac on apple
			net_if->recv_bytes = stats->ifi_ibytes;
			net_if->send_bytes = stats->ifi_obytes;
			net_if->recv_packets = stats->ifi_ipackets;
			net_if->send_packets = stats->ifi_opackets;
			net_if->recv_errors = stats->ifi_ierrors;
			net_if->send_errors = stats->ifi_oerrors;
			net_if->multicast = (uint64_t)stats->ifi_imcasts + stats->ifi_omcasts;
			net_if->collisions = stats->ifi_collisions;


			int isocket = socket(AF_INET, SOCK_DGRAM, 0);
			if( isocket < 0 ) {
				LOG_ERROR("socket(AF_INET, SOCK_STREAM, 0) ... error (%s) ", errorname(errno));
				continue;
			}

			struct ifreq paifr;
			memset(&paifr, 0, sizeof(paifr));
			strncpy(paifr.ifr_name, ifa->ifa_name, sizeof(paifr.ifr_name));
			if( ioctl(isocket, SIOCGIFEFLAGS, &paifr) != -1 ) {
				//net_if->mtu = paifr.ifr_mtu;
				LOG_INFO("paifr.ifr_eflags: 0x%08X\n", paifr.ifr_eflags);
			} else
				LOG_ERROR("ioctl(SIOCGIFEFLAGS) ... error (%s) ", errorname(errno));

			if( ioctl(isocket, SIOCGIFXFLAGS, &paifr) != -1 ) {
				//net_if->mtu = paifr.ifr_mtu;
				LOG_INFO("paifr.ifr_xflags: 0x%08X\n", paifr.ifr_xflags);
			} else
				LOG_ERROR("ioctl(SIOCGIFXFLAGS) ... error (%s) ", errorname(errno));

			#endif
		}
	}

	freeifaddrs(ifa_base);

	return 0;
}

void NetInterfaces::Log(void) {
	for( const auto& [name, net_if] : ifs ) {
		LOG_INFO("%S:\n", net_if->name.c_str());
		for( const auto& [ip, ipinfo] : net_if->ip ) {
			LOG_INFO("   flags=0x%08X ip: %S netmask: %S %s %S\n", ipinfo.ifa_flags, ip.c_str(), ipinfo.netmask.c_str(), \
				ipinfo.ifa_flags & IFF_BROADCAST ? "broadcast:":ipinfo.ifa_flags & IFF_POINTOPOINT ? "pint-to-point:":"", \
				ipinfo.ifa_flags & IFF_BROADCAST ? ipinfo.broadcast.c_str():ipinfo.ifa_flags & IFF_POINTOPOINT ? ipinfo.point_to_point.c_str():L"");
		}
		for( const auto& [ip, ipinfo] : net_if->ip6 ) {
			LOG_INFO("   flags=0x%08X ip6: %S netmask: %S %s %S\n", ipinfo.ifa_flags, ip.c_str(), ipinfo.netmask.c_str(), \
				ipinfo.ifa_flags & IFF_BROADCAST ? "broadcast:":ipinfo.ifa_flags & IFF_POINTOPOINT ? "pint-to-point:":"", \
				ipinfo.ifa_flags & IFF_BROADCAST ? ipinfo.broadcast.c_str():ipinfo.ifa_flags & IFF_POINTOPOINT ? ipinfo.point_to_point.c_str():L"");
		}
		for( const auto& [mac, macinfo] : net_if->mac ) {
			LOG_INFO("   flags=0x%08X mac: %S %s %S\n", macinfo.ifa_flags, mac.c_str(), \
				macinfo.ifa_flags & IFF_BROADCAST ? "broadcast:":"", \
				macinfo.ifa_flags & IFF_BROADCAST ? macinfo.broadcast.c_str():L"");
		}
		LOG_INFO("   permanent mac: %S\n", net_if->permanent_mac.c_str());
		LOG_INFO("   mtu: %u\n", net_if->mtu);
		LOG_INFO("   recv_bytes: %lu\n", net_if->recv_bytes);
		LOG_INFO("   send_bytes: %lu\n", net_if->send_bytes);
		LOG_INFO("   recv_packets: %lu\n", net_if->recv_packets);
		LOG_INFO("   send_packets: %lu\n", net_if->send_packets);
		LOG_INFO("   recv_errors: %lu\n", net_if->recv_errors);
		LOG_INFO("   send_errors: %lu\n", net_if->send_errors);
		LOG_INFO("   multicast: %lu\n", net_if->multicast);
		LOG_INFO("   collisions: %lu\n", net_if->collisions);
	}
}

NetInterfaces::NetInterfaces()
{
}

NetInterfaces::~NetInterfaces()
{
	Clear();
}

#ifdef MAIN_NETIF
int RootExec(const char * cmd)
{
	std::string _cmd("sudo /bin/sh -c \'");
	_cmd += cmd;
	_cmd += "'";
	LOG_INFO("Try exec \"%s\"\n", _cmd.c_str());
	return system(_cmd.c_str());
}

int main(int argc, char * argv[])
{
	{
		NetInterfaces net = NetInterfaces();
		net.Update();
		net.Log();
		for( const auto& [name, net_if] : net ) {
			LOG_INFO("%S:\n", net_if->name.c_str());
			net_if->SetInterfaceName(L"eth0");
			net_if->SetMac(L"88:88:88:88:88:88");
			net.Log();
			break;
		}
		net.Clear();
	}
	{
		NetInterface net_if = NetInterface();
		char buf[sizeof("255.255.255.255")] = {0};
		LOG_INFO("net_if.Ip6MaskToBits(\"ffff:ffff:ffff:ffff::\") = %u\n", net_if.Ip6MaskToBits("ffff:ffff:ffff:ffff::"));
		LOG_INFO("net_if.Ip6MaskToBits(\"ffff:ffff:ffff:ffff:ffff::\") = %u\n", net_if.Ip6MaskToBits("ffff:ffff:ffff:ffff:ffff::"));
		LOG_INFO("net_if.Ip6MaskToBits(\"ffff:ffff:ffff:ffff:ffff:ffff::\") = %u\n", net_if.Ip6MaskToBits("ffff:ffff:ffff:ffff:ffff:ffff::"));
		LOG_INFO("net_if.Ip6MaskToBits(\"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff\") = %u\n", net_if.Ip6MaskToBits("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
		LOG_INFO("net_if.IpBitsToMask(16) = %s\n", net_if.IpBitsToMask(16, buf, sizeof(buf)));
		LOG_INFO("net_if.IpBitsToMask(24) = %s\n", net_if.IpBitsToMask(24, buf, sizeof(buf)));
		LOG_INFO("net_if.IpBitsToMask(32) = %s\n", net_if.IpBitsToMask(32, buf, sizeof(buf)));
		LOG_INFO("net_if.IpBitsToMask(1) = %s\n", net_if.IpBitsToMask(1, buf, sizeof(buf)));
		LOG_INFO("net_if.IpBitsToMask(9) = %s\n", net_if.IpBitsToMask(9, buf, sizeof(buf)));
		LOG_INFO("net_if.IpBitsToMask(33333) = %s\n", net_if.IpBitsToMask(33333, buf, sizeof(buf)));
		LOG_INFO("net_if.IpBitsToMask(\"255.255.255.0\") = %u\n", net_if.IpMaskToBits("255.255.255.0"));
		LOG_INFO("net_if.IpBitsToMask(\"255.255.255.255\") = %u\n", net_if.IpMaskToBits("255.255.255.255"));
		LOG_INFO("net_if.IpBitsToMask(\"255.255.255.128\") = %u\n", net_if.IpMaskToBits("255.255.255.128"));
		LOG_INFO("net_if.IpBitsToMask(\"24\") = %u\n", net_if.IpMaskToBits("24"));
		LOG_INFO("net_if.IpBitsToMask(\"/24\") = %u\n", net_if.IpMaskToBits("/24"));
		LOG_INFO("net_if.IpBitsToMask(\"abc\") = %u\n", net_if.IpMaskToBits("abc"));
		LOG_INFO("net_if.IpBitsToMask(\"124\") = %u\n", net_if.IpMaskToBits("124"));
		LOG_INFO("net_if.IpBitsToMask(\"64\") = %u\n", net_if.IpMaskToBits("64"));
		LOG_INFO("net_if.IpBitsToMask(\"/64\") = %u\n", net_if.IpMaskToBits("/64"));

	}

	system("mv /tmp/far2l.netcfg.log .");
	return 0;
}

#endif //MAIN_NETIF
