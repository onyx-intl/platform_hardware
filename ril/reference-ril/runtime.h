/*
 *   Copyright (C) 2011 Freescale Semiconductor, Inc. All Rights Reserved.
 */

#ifndef RUNTIME_H
#define RUNTIME_H

extern int start_uevent_monitor(void);
extern const char *runtime_3g_port_device(void);
extern const char *runtime_3g_port_data(void);

enum {
	HUAWEI_MODEM = 0,
	AMAZON_MODEM,
	UNKNOWN_MODEM,
};

extern int current_modem_type;

#endif
