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
#else
#include <net/if_dl.h>
#endif

static std::wstring get_sockaddr_str(const struct sockaddr *sa, bool ismask = false)
{

	const size_t maxlen = INET6_ADDRSTRLEN > INET_ADDRSTRLEN ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN;
	char s[maxlen+1] = {0};
	std::string addr;

	if( !sa )
		return std::wstring();

	switch( sa->sa_family ) {
	case AF_INET:
	{
		if( inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), s, maxlen) )
			addr = s;
		break;
	}
	case AF_INET6:
	{
		struct sockaddr_in6 sa6 = *((const struct sockaddr_in6 *)sa);
		if( inet_ntop(AF_INET6, &sa6.sin6_addr, s, maxlen) ) {
			if( ismask ) {
				int bits = 0;
				for( int i = 0; i < 16; i++ ) {
					while (sa6.sin6_addr.s6_addr[i]) {
						bits += sa6.sin6_addr.s6_addr[i] & 1;
						sa6.sin6_addr.s6_addr[i] >>= 1;
					}
				}
				if( snprintf(s, maxlen, "%u", bits) <= 0 )
					s[0] = 0;
			}
			addr = s;
		}
		break;
	}
#if !defined(__APPLE__) && !defined(__FreeBSD__)
	case AF_PACKET:
	{
		struct sockaddr_ll * sll = (struct sockaddr_ll *)sa;
		unsigned char * mac = (unsigned char *)sll->sll_addr;
#else
	case AF_LINK:
	{
		struct sockaddr_dl * sdl = (struct sockaddr_dl *)sa;
		unsigned char * mac = (unsigned char *)sdl->sdl_data + sdl->sdl_nlen;
#endif
		if( snprintf(s, maxlen, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]) > 0 )
			addr = s;
		break;
	}
	default:
		LOG_ERROR("unsupported sa_family: %u\n", sa->sa_family);
		break;
	}
	return std::wstring(addr.begin(), addr.end());
}

NetInterface * NetInterfaces::Add(const char * name)
{
	std::string _name(name);
	std::wstring _wname(_name.begin(), _name.end());
	if( ifs.find(_wname) == ifs.end() ) {
		ifs[_wname] = new NetInterface(_wname);
		ifs[_wname]->UpdateStats();
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
		if( !ifa->ifa_name || !ifa->ifa_addr )
			continue;

		NetInterface * net_if = Add(ifa->ifa_name);
	
		char s[INET6_ADDRSTRLEN > INET_ADDRSTRLEN ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN] = {0};

		IpAddressInfo ip;
		ip.ifa_flags = ifa->ifa_flags;
		ip.ip = get_sockaddr_str((const struct sockaddr *)ifa->ifa_addr);
		if( ifa->ifa_netmask )
			ifa->ifa_netmask->sa_family = ifa->ifa_addr->sa_family;
		ip.netmask = get_sockaddr_str((const struct sockaddr *)ifa->ifa_netmask, true);
		if( ifa->ifa_flags & IFF_BROADCAST && ifa->ifa_broadaddr ) {
			ifa->ifa_broadaddr->sa_family = ifa->ifa_addr->sa_family;
			ip.broadcast = get_sockaddr_str((const struct sockaddr *)ifa->ifa_broadaddr);
		} else if( ifa->ifa_flags & IFF_POINTOPOINT && ifa->ifa_dstaddr ) {
			ifa->ifa_dstaddr->sa_family = ifa->ifa_addr->sa_family;
			ip.point_to_point = get_sockaddr_str((const struct sockaddr *)ifa->ifa_dstaddr);
		}

		switch( ifa->ifa_addr->sa_family ) {

		case AF_INET:
			net_if->ip[ip.ip] = ip;
			break;
		case AF_INET6:
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
			//net_if->SetInterfaceName(L"eth0");
			//net_if->SetMac(L"88:88:88:88:88:88");
			net.Log();
			break;
		}
		net.Clear();
	}
	{
		NetInterface net_if = NetInterface(L"lo");
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
