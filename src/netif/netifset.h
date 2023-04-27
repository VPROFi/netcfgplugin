#define MAX_CMD_LEN 512

#define SETMULTICASTON_CMD     "ip link set dev %S multicast on"
#define SETMULTICASTOFF_CMD    "ip link set dev %S multicast off"

#define SETALLMULTICASTON_CMD  "ip link set dev %S allmulticast on"
#define SETALLMULTICASTOFF_CMD "ip link set dev %S allmulticast off"

#define SETARPON_CMD           "ip link set dev %S arp off"
#define SETARPOFF_CMD          "ip link set dev %S arp on"

//#define SETTRAILERSON_CMD "ip link set dev %S trailers on"
//#define SETTRAILERSOFF_CMD "ip link set dev %S trailers off"

//#define SETDYNAMICON_CMD "ip link set dev %S dynamic on"
//#define SETDYNAMICOFF_CMD "ip link set dev %S dynamic off"

//#define SETCARRIERON_CMD "ip link set dev %S carrier on"
//#define SETCARRIEROFF_CMD "ip link set dev %S carrier off"

//#define TCPDUMP_CMD   "tcpdump --interface=%S --buffer-size=8192 -w %S -C 10000000 -Z root -W 10 &"

#define TCPDUMP_CMD     "tcpdump --interface=%S --buffer-size=8192 --monitor-mode -w %S || tcpdump --interface=%S --buffer-size=8192 -w %S &"
#define TCPDUMPKILL_CMD "pkill -f '^tcpdump --interface=%S'"

#if !defined(__APPLE__) && !defined(__FreeBSD__)

#define CHANGEIFNAME_CMD  "ip link set dev %S down && ip link set dev %S name %S && ip link set dev %S up"
#define SETDOWN_CMD       "ip link set dev %S down"
#define SETUP_CMD         "ip link set dev %S up"
#define CHANGEMAC_CMD     "ip link set dev %S down && ip link set dev %S address %S && ip link set dev %S up"
#define SETMTU_CMD        "ip link set dev %S mtu %u"
#define ADDIP_CMD         "ip address add %S/%u broadcast %S dev %S"
#define DELETEIP_CMD      "ip address del %S/%u dev %S"
#define CHANGEIP_CMD      "ip address del %S/%u dev %S && ip address add %S/%u broadcast %S dev %S"
#define DELETEIP6_CMD     "ip address del %S/%u dev %S"
#define ADDIP6_CMD        "ip address add %S/%u dev %S"
#define CHANGEIP6_CMD     "ip address del %S/%u dev %S && ip address add %S/%u dev %S"
#define SETPROMISCON_CMD  "ip link set dev %S promisc on"
#define SETPROMISCOFF_CMD "ip link set dev %S promisc off"

#else

#define CHANGEIFNAME_CMD  "change ifname unsupported on macos: %S %S %S %S"

#define SETDOWN_CMD       "ifconfig %S down"
#define SETUP_CMD         "ifconfig %S up"

#define CHANGEMAC_CMD     "ifconfig %S ether %S && ifconfig %S down && ifconfig %S up"
#define SETMTU_CMD        "ifconfig %S mtu %u"
#define ADDIP_CMD         "ifconfig %S inet %S/%u broadcast %S add"
#define DELETEIP_CMD      "ifconfig %S inet %S/%u delete"
#define CHANGEIP_CMD      "ifconfig %S inet %S/%u delete && ifconfig %S inet %S/%u broadcast %S add"

#define ADDIP6_CMD        "ifconfig %S inet6 %S/%u add"
#define DELETEIP6_CMD     "ifconfig %S inet6 %S/%u delete"
#define CHANGEIP6_CMD     "ifconfig %S inet6 %S/%u delete && ifconfig %S inet6 %S/%u add"

#define SETPROMISCON_CMD  "ifconfig %S promisc"
#define SETPROMISCOFF_CMD "ifconfig %S -promisc"

#endif
