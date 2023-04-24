#define MAX_CMD_LEN 512

#if !defined(__APPLE__) && !defined(__FreeBSD__)

#define CHANGEIFNAME_CMD "ip link set dev %S down && ip link set dev %S name %S && ip link set dev %S up"
#define CHANGEMAC_CMD "ip link set dev %S down && ip link set dev %S address %S && ip link set dev %S up"

#define SETDOWN_CMD "ip link set dev %S down"
#define SETUP_CMD "ip link set dev %S up"

#define SETMULTICASTON_CMD "ip link set dev %S multicast on"
#define SETMULTICASTOFF_CMD "ip link set dev %S multicast off"

#define SETALLMULTICASTON_CMD "ip link set dev %S allmulticast on"
#define SETALLMULTICASTOFF_CMD "ip link set dev %S allmulticast off"

#define SETARPON_CMD "ip link set dev %S arp off"
#define SETARPOFF_CMD "ip link set dev %S arp on"

#define SETPROMISCON_CMD "ip link set dev %S promisc on"
#define SETPROMISCOFF_CMD "ip link set dev %S promisc off"

#define SETMTU_CMD "ip link set dev %S mtu %u"

//#define SETTRAILERSON_CMD "ip link set dev %S trailers on"
//#define SETTRAILERSOFF_CMD "ip link set dev %S trailers off"

//#define SETDYNAMICON_CMD "ip link set dev %S dynamic on"
//#define SETDYNAMICOFF_CMD "ip link set dev %S dynamic off"

//#define SETCARRIERON_CMD "ip link set dev %S carrier on"
//#define SETCARRIEROFF_CMD "ip link set dev %S carrier off"

#define DELETEIP_CMD  "ip address del %S/%S dev %S"
#define ADDIP_CMD     "ip address add %S/%S broadcast %S dev %S"
#define CHANGEIP_CMD  "ip address del %S/%S dev %S && ip address add %S/%u broadcast %S dev %S"


#define DELETEIP6_CMD  "ip address del %S/%S dev %S"
#define ADDIP6_CMD     "ip address add %S/%S dev %S"
#define CHANGEIP6_CMD "ip address del %S/%S dev %S && ip address add %S/%u dev %S"

#else

#endif
