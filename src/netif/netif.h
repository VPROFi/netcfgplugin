#ifndef __NETIF_H__
#define __NETIF_H__

#include <string>
#include <map>
#include <stdint.h>

struct IpAddressInfo {
	unsigned int ifa_flags;
	std::wstring ip;
	std::wstring netmask;
	std::wstring point_to_point;
	std::wstring broadcast;
};

struct MacAddressInfo {
	unsigned int ifa_flags;
	std::wstring mac;
	std::wstring broadcast;
};

struct NetInterface {
	uint32_t size;
	std::wstring name;
	std::map<std::wstring, MacAddressInfo> mac;
	std::map<std::wstring, IpAddressInfo> ip;
	std::map<std::wstring, IpAddressInfo> ip6;
	std::wstring permanent_mac;
	uint32_t mtu;
	uint32_t ifa_flags;
	uint64_t recv_bytes;
	uint64_t send_bytes;
	uint64_t recv_packets;
	uint64_t send_packets;
	uint64_t recv_errors;
	uint64_t send_errors;
	uint64_t multicast;
	uint64_t collisions;

	bool IsUp();
	bool SetUp(bool on);
	bool IsBroadcast();
	bool IsDebug();
	bool SetDebug(bool on);
	bool IsLoopback();
	bool IsPointToPoint();
	bool IsNotrailers();
	bool SetNotrailers(bool on);
	bool IsRunning();
	bool IsNoARP();
	bool SetNoARP(bool on);
	bool IsPromisc();
	bool SetPromisc(bool on);
	bool IsMulticast();
	bool SetMulticast(bool on);
	bool IsAllmulticast();
	bool SetAllmulticast(bool on);

	bool IsCarrierOn();
	bool IsDormant(); // can`t use - setup, or sleep
	bool IsEcho();  // wifi IEEE80211_TX_CTL_REQ_TX_STATUS receiving TX status frames for frames sent on an interface in managed mode
			// see /sys/class/net/<dev>/flags

	const char * IpBitsToMask(uint32_t bits, char * buffer, size_t maxlen);
	int Ip6MaskToBits(const char * mask);
	int IpMaskToBits(const char * mask);

	bool SetInterfaceName(const wchar_t * name);
	bool SetMac(const wchar_t * mac);
	bool SetMtu(uint32_t newmtu);
	bool DeleteIp(const wchar_t * ip);
	bool DeleteIp6(const wchar_t * ip);
	bool AddIp(const wchar_t * newip, const wchar_t * mask, const wchar_t * bcip);
	bool AddIp6(const wchar_t * newip, const wchar_t * mask);
	bool ChangeIp(const wchar_t * oldip, const wchar_t * newip, const wchar_t * mask, const wchar_t * bcip);
	bool ChangeIp6(const wchar_t * oldip, const wchar_t * newip, const wchar_t * mask);
};

#endif /* __NETIF_H__ */
