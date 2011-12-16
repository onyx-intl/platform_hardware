/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* Copyright 2010-2011 Freescale Semiconductor Inc. */

#ifndef _ETHERNET_H
#define _ETHERNET_H

#if __cplusplus
extern "C" {
#endif

/**
 * Load the Ethernet driver.
 *
 * @return 0 on success, < 0 on failure.
 */
int ethernet_load_driver();

/**
 * Unload the Ethernet driver.
 *
 * @return 0 on success, < 0 on failure.
 */
int ethernet_unload_driver();


/**
 * ethernet_wait_for_event() performs a blocking call to 
 * get a Ethernet event and returns a string representing 
 * a Ethernet event when it occurs.
 *
 * @param buf is the buffer that receives the event
 * @param len is the maximum length of the buffer
 *
 * @returns number of bytes in buffer, 0 if no
 * event (for instance, no connection), and less than 0
 * if there is an error.
 */
int ethernet_wait_for_event(char *buf, size_t len);

/**
 * ethernet_command() issues a command to the Ethernet driver.
 *
 *
 * @param command is the string command
 * @param reply is a buffer to receive a reply string
 * @param reply_len on entry, this is the maximum length of
 *        the reply buffer. On exit, the number of
 *        bytes in the reply buffer.
 *
 * @return 0 if successful, < 0 if an error.
 */
int ethernet_command(const char *command, char *reply, size_t *reply_len);

/**
 * do_dhcp_request() issues a dhcp request and returns the acquired
 * information. 
 * 
 * All IPV4 addresses/mask are in network byte order.
 *
 * @param ipaddr return the assigned IPV4 address
 * @param gateway return the gateway being used
 * @param mask return the IPV4 mask
 * @param dns1 return the IPV4 address of a DNS server
 * @param dns2 return the IPV4 address of a DNS server
 * @param server return the IPV4 address of DHCP server
 * @param lease return the length of lease in seconds.
 *
 * @return 0 if successful, < 0 if error.
 */
int do_dhcp_request(int *ipaddr, int *gateway, int *mask,
                   int *dns1, int *dns2, int *server, int *lease);

/**
 * Return the error string of the last do_dhcp_request().
 */
const char *get_dhcp_error_string();

#if __cplusplus
};  // extern "C"
#endif

#endif  // _WIFI_H
