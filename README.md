# netcfgplugin

Плагин позволяет (linux и macos):
 * просматривать, редактировать информацию о сетевых интерфейсах, а так же сохранять трафик на интерфейсе в файл (tcpdump)
 * просматривать, создавать, редактировать и удалять информацию о маршрутизации arp, ipv4(ipv6) + multicast
   * в linux (получение информации по netlink, изменение - используя команду ip):
      * маршруты разбиты по таблицам маршрутизации
      * можно просматривать, создавать, редактировать и удалять сетевые правила
      * просмотр multicast маршрутизации
   * в macos (получение информации ioctl, изменение route, ifconfig, arp, npd)

The plugin allows (linux and macos):
  * view and edit information about network interfaces (linux and macos), and use tcpdump on interface
  * view, create, edit and delete routing information arp, ipv4(ipv6) + multicast
    * in linux (getting information via netlink, changing - using the ip command):
       * routes broken down by routing tables
       * you can view, create, edit and delete network rules
       * view multicast routing
    * on macos (getting ioctl information, changing route, ifconfig, arp, npd)

far2l net interfaces configuration plugin.

Build instruction like far2l (https://github.com/VPROFi/far2l)

If your want build inside other version far2l - put content ./src into ./far2l/netcfg and add to ./far2l/CMakeLists.txt add_subdirectory (netcfg)

![](img/ifs.png)
![](img/iproutes.png)
![](img/arp.png)
![](img/rule.png)
![](img/config.png)
![](img/ipv6route.png)
![](img/del.png)
![](img/macos.png)
![](img/tcpdump.png)
![](img/tcpdumpstop.png)
![](img/main.png)
![](img/mainwide.png)
![](img/cfg.png)
