#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <glog/logging.h>
#include "base/basictypes.hpp"
#include "base/stringprintf.hpp"
#include <errno.h>

string GetMacAddress() {
  int fd; // Socket descriptor
  struct ifreq sIfReq; // Interface request
  struct if_nameindex *pIfList; // Ptr to interface name index
  struct if_nameindex *pListSave; // Ptr to interface name index
  //// Initialize this function
  pIfList = (struct if_nameindex *)NULL;
  pListSave = (struct if_nameindex *)NULL;
  // Create a socket that we can use for all of our ioctls
  fd = socket( PF_INET, SOCK_STREAM, 0 );
  if ( fd < 0 ) {
    // Socket creation failed, this is a fatal error
    LOG(WARNING) << "Create socket fail";
    return "";
  }
  // Obtain a list of dynamically allocated structures
  pIfList = pListSave = if_nameindex();
  //
  // Walk thru the array returned and query for each interface's
  // address
  //
  for (pIfList; *(char *)pIfList != 0; pIfList++ ) {
    strncpy( sIfReq.ifr_name, pIfList->if_name, IF_NAMESIZE );
    // Get the MAC address for this interface
    if (ioctl(fd, SIOCGIFHWADDR, &sIfReq) != 0) {
      // We failed to get the MAC address for the interface
      LOG(WARNING) << pIfList->if_name << " Ioctl fail:" << strerror(errno);
      continue;
    }
    uint8 *mac_addr = reinterpret_cast<uint8*>(sIfReq.ifr_ifru.ifru_hwaddr.sa_data);
    if (mac_addr[0] == 0 && mac_addr[1] == 0 && mac_addr[2] == 0 &&
        mac_addr[3] == 0 && mac_addr[4] == 0 && mac_addr[5] == 0) {
      VLOG(2) << pIfList->if_name << " haven't mac address";
      continue;
    }
    string str_mac_address = StringPrintf("%02x:%02x:%02x:%02x:%02x:%02x",
                  mac_addr[0], mac_addr[1], mac_addr[2],
                  mac_addr[3], mac_addr[4], mac_addr[5]);
    VLOG(2) << pIfList->if_name << " mac address : " << str_mac_address;
    if_freenameindex(pListSave);
    close(fd);
    return str_mac_address;
  }
  if_freenameindex(pListSave);
  close(fd);
  return "";
}
