/*
 *   Copyright (C) 2011 Freescale Semiconductor, Inc. All Rights Reserved.
 */
/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#define LOG_TAG "RIL"
#include <utils/Log.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <linux/netlink.h>

#include <signal.h>
#include <unistd.h>
#include "runtime.h"

int current_modem_type = UNKNOWN_MODEM;

#define FAKE_PORT "/dev/ttyFAKEPort"
/* Rild need a fake port to pass continue init job,
 * return a fake port make it runable.
 * Or the system will enter 15s in early suspend.
 */

struct modem_3g_device {
	const char *idVendor;
	const char *idProduct;
	const char *deviceport;	/* sending AT command */
	const char *dataport;	/* sending 3g data */
	const char *name;
	const int   type;
};

#define PATH_SIZE 1024
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
static const char *USB_DIR_BASE = "/sys/class/usb_device/";

static struct modem_3g_device modem_3g_device_table[] = {
	{
		.name		= "Huawei-EM770",
		.idVendor	= "12d1",
		.idProduct	= "1001",
		.deviceport	= "/dev/ttyUSB0",
		.dataport	= "/dev/ttyUSB0",
		.type		= HUAWEI_MODEM,
	},
	{
		.name		= "Huawei-EM770W",
		.idVendor	= "12d1",
		.idProduct	= "1404",
		.deviceport	= "/dev/ttyUSB2",
		.dataport	= "/dev/ttyUSB0",
		.type		= HUAWEI_MODEM,
	},
	{
		.name		= "Huawei-E180",
		.idVendor	= "12d1",
		.idProduct	= "1003",
		.deviceport	= "/dev/ttyUSB1",
		.dataport	= "/dev/ttyUSB0",
		.type		= HUAWEI_MODEM,
	},
	{
		.name		= "Huawei-EM750",
		.idVendor	= "12d1",
		.idProduct	= "1413",
		.deviceport	= "/dev/ttyUSB3",
		.dataport	= "/dev/ttyUSB0",
		.type		= HUAWEI_MODEM,
	},
	{
		.name		= "InnoComm-Amazon1",
		.idVendor	= "1519",
		.idProduct	= "1001",
		.deviceport	= "/dev/ttyACM3",
		.dataport	= "/dev/ttyACM0",
		.type		= AMAZON_MODEM,
	},
	{
		.name		= "InnoComm-Amazon1",
		.idVendor	= "1519",
		.idProduct	= "0020",
		.deviceport	= "/dev/ttyACM3",
		.dataport	= "/dev/ttyACM0",
		.type		= AMAZON_MODEM,
	}
};

/* -------------------------------------------------------------- */

#define DEBUG_UEVENT 0
#define UEVENT_PARAMS_MAX 32

enum uevent_action { action_add, action_remove, action_change };

struct uevent {
    char *path;
    enum uevent_action action;
    char *subsystem;
    char *param[UEVENT_PARAMS_MAX];
    unsigned int seqnum;
};

static void dump_uevent(struct uevent *event);

int readfile(char *path, char *content, size_t size)
{
	int ret;
	FILE *f;
	f = fopen(path, "r");
	if (f == NULL)
		return -1;

	ret = fread(content, 1, size, f);
	fclose(f);
	return ret;
}

int is_device_equal(struct modem_3g_device *device,
		     const char *idv, const char *idp)
{
	long pvid = 0xffff, ppid = 0xffff;
	long t_vid, t_pid;
	if (device == NULL)
		return 0;
	t_vid = strtol(device->idVendor, NULL, 16);
	t_pid = strtol(device->idProduct, NULL, 16);
	pvid = strtol(idv, NULL, 16);
	ppid = strtol(idp, NULL, 16);

	return (t_vid == pvid && t_pid == ppid);
}

struct modem_3g_device *
find_devices_in_table(const char *idvendor, const char *idproduct)
{
	int i;
	int size = ARRAY_SIZE(modem_3g_device_table);
	struct modem_3g_device *device;

	for (i = 0; i < size; i++) {
		device = &modem_3g_device_table[i];

		if (is_device_equal(device, idvendor, idproduct)) {
			LOGI("Runtime 3G port found matched device with "
			     "Name:%s idVendor:%s idProduct:%s",
			     device->name, device->idVendor, device->idProduct);

			return device;
		}
	}

	return NULL;
}

struct modem_3g_device *find_matched_device(void)
{
	struct dirent *dent;
	DIR *usbdir;
	struct modem_3g_device *device = NULL;
	char *path, *path2;
	char idvendor[64];
	char idproduct[64];
	int ret, i;

	path = malloc(PATH_SIZE);
	path2 = malloc(PATH_SIZE);
	if (!path || !path2)
		return NULL;

	usbdir = opendir(USB_DIR_BASE);
	if (usbdir == NULL) {
		free(path);
		free(path2);
		return NULL;
	}

	memset(path, 0, PATH_SIZE);
	memset(path2, 0, PATH_SIZE);

	while ((dent = readdir(usbdir)) != NULL) {
		if (strcmp(dent->d_name, ".") == 0
		    || strcmp(dent->d_name, "..") == 0)
			continue;
		memset(idvendor, 0, sizeof(idvendor));
		memset(idproduct, 0, sizeof(idproduct));
		path = strcpy(path, USB_DIR_BASE);
		path = strcat(path, dent->d_name);
		strcpy(path2, path);
		path = strcat(path, "/device/idVendor");
		path2 = strcat(path2, "/device/idProduct");

		ret = readfile(path, idvendor, 4);
		if (ret <= 0)
			continue;
		ret = readfile(path2, idproduct, 4);
		if (ret <= 0)
			continue;
		device = find_devices_in_table(idvendor, idproduct);
		if (device != NULL)
			goto out;
	}

	if (device == NULL)
		LOGI("Runtime 3G can't find supported modem");
out:
	closedir(usbdir);
	free(path);
	free(path2);

	return device;
}


const char *runtime_3g_port_device(void)
{
	struct modem_3g_device *device;

	device = find_matched_device();
	if (device == NULL)
		return FAKE_PORT;

	/* Set gobal modem type. */
	current_modem_type = device->type;

	LOGI("Current modem type = %d", current_modem_type);

	return device->deviceport;
}

const char *runtime_3g_port_data(void)
{
	struct modem_3g_device *device;

	device = find_matched_device();
	if (device == NULL)
		return FAKE_PORT;
	return device->dataport;
}

static void free_uevent(struct uevent *event)
{
    int i;
    free(event->path);
    free(event->subsystem);
    for (i = 0; i < UEVENT_PARAMS_MAX; i++) {
	    if (!event->param[i])
		    break;
	    free(event->param[i]);
    }
    free(event);
}

static int dispatch_uevent(struct uevent *event)
{
	/* if it's a usb tty event in our table. make the rild reboot. */
	int i;
	int ret;
	for (i = 0; i < UEVENT_PARAMS_MAX; i++) {
		if (!event->param[i])
			break;
		if (strncmp(event->param[i], "PRODUCT=", 8) == 0) {
			char vbuf[5], pbuf[5];
			ret = sscanf(event->param[i],
				     "PRODUCT=%4s/%4s/", vbuf, pbuf);
			if (ret < 0)
				return -1;
			if (find_devices_in_table(vbuf, pbuf))
				alarm(1);
			/* Restart in 1 second, since USB usually have
			 * many devices, this avoid rild restart too
			 * many times. */
		}
	}
	return 0;
}

int process_uevent_message(int sock)
{
	char buffer[64 * 1024];
	char *s = buffer, *p;
	char *end;
	int count, param_idx = 0, ret;
	struct uevent *event;
	count = recv(sock, buffer, sizeof(buffer), 0);
	if (count < 0) {
		LOGE("Error receiving uevent (%s)", strerror(errno));
		return -errno;
	}
	event = malloc(sizeof(struct uevent));
	if (!event) {
		LOGE("Error allcating memroy (%s)", strerror(errno));
		return -errno;
	}
	memset(event, 0, sizeof(struct uevent));

	end = s + count;

	for (p = s; *p != '@'; p++)
		;
	p++;
	event->path = strdup(p);
	s += strlen(s) + 1;

	while (s < end) {
		if (!strncmp(s, "ACTION=", strlen("ACTION="))) {
			char *a = s + strlen("ACTION=");
			if (!strcmp(a, "add"))
				event->action = action_add;
			else if (!strcmp(a, "change"))
				event->action = action_change;
			else if (!strcmp(a, "remove"))
				event->action = action_remove;
		} else if (!strncmp(s, "SEQNUM=", strlen("SEQNUM=")))
			event->seqnum = atoi(s + strlen("SEQNUM="));
		else if (!strncmp(s, "SUBSYSTEM=", strlen("SUBSYSTEM=")))
			event->subsystem = strdup(s + strlen("SUBSYSTEM="));
		else
			event->param[param_idx++] = strdup(s);
		s += strlen(s) + 1;
	}

	ret = dispatch_uevent(event);
#if DEBUG_UEVENT
	dump_uevent(event);
#endif
	free_uevent(event);
	return ret;
}

static void dump_uevent(struct uevent *event)
{
    int i;

    LOGD("[UEVENT] Sq: %u S: %s A: %d P: %s",
	      event->seqnum, event->subsystem, event->action, event->path);
    for (i = 0; i < UEVENT_PARAMS_MAX; i++) {
	    if (!event->param[i])
		    break;
	    LOGD("%s", event->param[i]);
    }
}

void restart_rild(int p)
{
	LOGI("3G Modem changed,RILD will restart...");
	exit(-1);
}

void *usb_tty_monitor_thread(void *arg)
{
	struct sockaddr_nl nladdr;
	struct pollfd pollfds[2];
	int uevent_sock;
	int ret, max = 0;
	int uevent_sz = 64 * 1024;
	int timeout = -1;
	struct sigaction timeoutsigact;

	LOGI("3G modem monitor thread is start");

	timeoutsigact.sa_handler = restart_rild;
	sigemptyset(&timeoutsigact.sa_mask);
	sigaddset(&timeoutsigact.sa_mask, SIGALRM);
	sigaction(SIGALRM, &timeoutsigact, 0);

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_pid = getpid();
	nladdr.nl_groups = 0xffffffff;

	uevent_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (uevent_sock < 0) {
		LOGE(" Netlink socket faild, usb monitor exiting...");
		return NULL;
	}

	if (setsockopt(uevent_sock, SOL_SOCKET, SO_RCVBUFFORCE, &uevent_sz,
		       sizeof(uevent_sz)) < 0) {
		LOGE("Unable to set uevent socket options: %s", strerror(errno));
		return NULL;
	}

	if (bind(uevent_sock, (struct sockaddr *) &nladdr, sizeof(nladdr)) < 0) {
		   LOGE("Unable to bind uevent socket: %s", strerror(errno));
		   return NULL;
	}
	pollfds[0].fd = uevent_sock;
	pollfds[0].events = POLLIN;

	ret = fcntl(uevent_sock,F_SETFL, O_NONBLOCK);
	if (ret < 0)
		LOGE("Error on fcntl:%s", strerror(errno));

	while (1) {
		ret = poll(pollfds, 1, timeout);

		switch (ret) {
		case 0:
			LOGD("poll timeout");
			continue;
		case -1:
			LOGD("poll error:%s", strerror(errno));
			break;

		default:
			if (pollfds[0].revents & POLLIN)
				process_uevent_message(uevent_sock);
		}
	}

	close(uevent_sock);
}

int start_uevent_monitor(void)
{
	pthread_t pth_uevent_monitor;
	return pthread_create(&pth_uevent_monitor, NULL,
			      usb_tty_monitor_thread, NULL);
}


