/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* Copyright 2010-2011 Freescale Semiconductor Inc. */

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/ethtool.h>

#include "cutils/log.h"
#include "hardware_legacy/ethernet.h"

#define LOG_TAG "EthernetHW"

static int check_driver_loaded() {
/*
	struct ifconf ifc;
	struct ifreq *ifr;
    char   buf[1024];
    int    nInterfaces;
    int    i;
    int    sock ;
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);

// get all interfaces. 
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if(ioctl(sock, SIOCGIFCONF, &ifc) < 0)
	{
		LOGW("ioctl(SIOCGIFCONF)");
		close(sock);
		return 0;
	}
    close(sock);
    
	ifr         = ifc.ifc_req;
	nInterfaces = ifc.ifc_len / sizeof(struct ifreq);
	for(i = 0; i < nInterfaces; i++)
	{
		struct ifreq *item = &ifr[i];

	// Show the device name and IP address 
		LOGW("%s: IP %s",
		       item->ifr_name,
		       inet_ntoa(((struct sockaddr_in *)&item->ifr_addr)->sin_addr));
		       
		if(strcmp(item->ifr_name,"eth0") == 0)
		{
		    return 1;
        }
    }
    return 0;
*/
    return 1;
}

int ethernet_load_driver()
{

    LOGW("ethernet_load_driver\n");
    /* currently the driver is loaded default, can't be unloaded*/ 

    if (check_driver_loaded()) {
        return 0;
    }

    return -1;
}

int ethernet_unload_driver()
{
    /* currently the driver is loaded default, can't be unloaded*/ 
    return 0;
}


int ethernet_wait_for_event(char *buf, size_t buflen)
{
    return 0;
}


int ethernet_command(const char *command, char *reply, size_t *reply_len)
{
    struct ifreq ifr;
    struct ethtool_value edata;
    int    sock ;
    char   *updown;
    int    ret = 0;
    unsigned int addr,flags;
    int    command_num;
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ-1] = 0;

    if(strcmp(command,"DRIVER UP")==0) command_num=1;
    else if(strcmp(command,"DRIVER DOWN")==0) command_num=2;
    else if(strcmp(command,"DRIVER IP")==0) command_num=3;
    else if(strcmp(command,"STATUS")==0) command_num=4;
    else command_num=0;

    switch(command_num)
    {
        case 1: //command up
            LOGW("command up ethernet\n");
            if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
                LOGW("command up failed");
                return -1;
            } else
                flags = ifr.ifr_flags;
            updown =  (flags & IFF_UP)           ? "up" : "down";        
            if(strcmp(updown,"up")==0) 
            {
                ret = 0;
            }
            else
            {
                if(ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) ret = -1;
                ifr.ifr_flags = ifr.ifr_flags | IFF_UP;
                if(ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) ret = -1;

            }
            if(ret == 0)
            {
                strcpy(reply,"OK");     
                *reply_len = 2;
            }else
            {
                strcpy(reply,"FAILED");               
                *reply_len = 6;        
            }            
            
        break;
        case 2: //command down
            LOGW("command down ethernet\n");
            if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
                LOGW("command down failed");
                return -1;
            } else
                flags = ifr.ifr_flags;
                
            updown =  (flags & IFF_UP)           ? "up" : "down";            
            if(strcmp(updown,"down")==0) 
            {
                ret = 0;
            }
            else
            {
                if(ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) ret = -1;
                ifr.ifr_flags = ifr.ifr_flags & (~IFF_UP);
                if(ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) ret = -1;
            }
            if(ret == 0)
            {
                strcpy(reply,"OK");     
                *reply_len = 2;
            }else
            {
                strcpy(reply,"FAILED");               
                *reply_len = 6;        
            }            
                
        break;
        case 3: //ip_addr
            LOGW("command ip ethernet\n");
            if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
                LOGW("command ip failed");
                return -1;
            } else
            {
                addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
                LOGW("ip addr %d",addr);
            }
            
            if(ret == 0)
            {
                strcpy(reply,"OK");     
                *reply_len = 2;
            }else
            {
                strcpy(reply,"FAILED");               
                *reply_len = 6;        
            }
        break;
        case 4: //status
            //LOGW("command status ethernet\n");
            edata.cmd = ETHTOOL_GLINK;
            edata.data = 0;
            ifr.ifr_data = (char *) &edata;

            if(ioctl(sock, SIOCETHTOOL, &ifr ) < 0)
            {
                close(sock);
                LOGW("command status failed");
                strcpy(reply,"CTRL-EVENT-ERROR");
                *reply_len = 16; 
                return ret;
            }
            
            if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) 
            {
                close(sock);
                LOGW("command up/down failed");
                strcpy(reply,"CTRL-EVENT-ERROR");
                *reply_len = 16; 
                return ret;
            } else
                flags = ifr.ifr_flags;
            updown =  (flags & IFF_UP)           ? "up" : "down";   

            if((edata.data ==1) && (strcmp(updown,"up")==0)) 
            {   
                strcpy(reply,"CTRL-EVENT-PLUGGED_AND_UP");
                *reply_len = 25;  
            }else if((edata.data ==1) && (strcmp(updown,"down")==0))
            {
                strcpy(reply,"CTRL-EVENT-PLUGGED_AND_DOWN");
                *reply_len = 27;     
            }else if((edata.data ==0) && (strcmp(updown,"down")==0))                
            {   
                strcpy(reply,"CTRL-EVENT-UNPLUGGED_AND_DOWN");
                *reply_len = 29;  
            }else if((edata.data ==0) && (strcmp(updown,"up")==0))        
            {   
                strcpy(reply,"CTRL-EVENT-UNPLUGGED_AND_UP");
                *reply_len = 27;              
            }else
            {
                strcpy(reply,"CTRL-EVENT-UNKNOWN");
                *reply_len = 18;                  
            }
            
        break;
        default:
        break;
    }

    //LOGW("reply =%s",reply);
    close(sock);
    return ret;
}
