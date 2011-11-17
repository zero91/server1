/*
 * Copyright (c) 2009, Xiliu Tang (xiliu.tang@gmail.com)
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above 
 *       copyright notice, this list of conditions and the following 
 *       disclaimer in the documentation and/or other materials provided 
 *       with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Project Website http://code.google.com/p/server1/
 */
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
