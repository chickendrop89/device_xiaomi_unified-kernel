/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _UAPI_SAILMB_H_
#define _UAPI_SAILMB_H_

#include <linux/ioctl.h>

#define MAILBOX_SIZE	0x100000	// size of sail mailbox region.
#define OTA_SIZE	0x300000	// size of sail mailbox ota region.
#define SAIL_MAILBOX_MAGIC	0x5A	// Unique magic number to make IOCTL calls.

/**
 * struct sailmb_client_data - Specifies client data passed from userspace while
 * making IOCTL calls.
 *
 * @event_fd: Eventfd initialized in the userspace for an RX client. This eventfd is mapped to the
 * corresponding irq. The irq handler signals an incoming interrupt using this eventfd.
 *
 * @signal_num: IPCC signal number used for this client.
 *
 * @tx_mode: Indicates if the TX channel is used by the client. Set to 0 if a TX channel is not
 * used.
 *
 * @rx_mode: Indicates if the RX channel is used by the client. Set to 0 if a RX channel is not
 * used.
 */
struct sailmb_client_data {
	int event_fd;
	unsigned int signal_num;
	unsigned int tx_mode;
	unsigned int rx_mode;
};

/**
 * struct sailmb_data - Structure to hold required data while making IOCTL calls.
 *
 * @ioctl_magic: Magic number to authenticate IOCTL calls.
 *
 * @sailmb_client: Structure to hold client data passed from the userspace to service IOCTLs.
 */
struct sailmb_data {
	int ioctl_magic;
	struct sailmb_client_data sailmb_client;
};

/*
 * This IOCTL is used to send an IPCC interrupt to sail after writing data into the mailbox.
 * A pointer to sailmb_data struct is passed as an argument which is used to identify and trigger
 * the correct irq. It also checks if tx_mode is set for this operation to be valid.
 *
 * @return: Return 0 on success or corresponding error code on failure.
 */
#define SEND_INTERRUPT _IOWR(SAIL_MAILBOX_MAGIC, 0x1, struct sailmb_data *)

/*
 * This IOCTL is made after initializing an eventfd in the userspace and to register
 * it in the kernel. The call also enables the corresponding irq to support incoming interrupts.
 * A pointer sailmb_data struct is passed as an argument which is used to identify the right irq
 * and associate the eventfd to it. It also checks if rx_mode is set for this operation to be valid.
 *
 * @return: Return 0 on success or corresponding error code on failure.
 */
#define SET_EVENT_FD _IOWR(SAIL_MAILBOX_MAGIC, 0x2, struct sailmb_data *)

/*
 * This IOCTL is used to disable an irq once the client application completes execution.
 * A pointer to sailmb_data struct is passed as an argument which is used to identify the right irq
 * and disable it. It also check if rx_mode is set for this operation to be valid.
 *
 * @return: Return 0 on success or corresponding error code on failure.
 */
#define DISABLE_INTERRUPT _IOWR(SAIL_MAILBOX_MAGIC, 0x3, struct sailmb_data *)

/*
 * This IOCTL is used to internally initialize parameters to facilitate the memory mapping of the
 * sail mailbox region to the userspace.
 *
 * @return: Return 0 on success or corresponding error code on failure.
 */
#define SET_MAILBOX_ADDR _IOWR(SAIL_MAILBOX_MAGIC, 0x4, struct sailmb_data *)

/*
 * This IOCTL is used to internally initialize parameters to facilitate the memory mapping of the
 * sail mailbox ota region to the userspace.
 *
 * @return: Return 0 on success or corresponding error code on failure.
 */
#define SET_OTA_ADDR _IOWR(SAIL_MAILBOX_MAGIC, 0x5, struct sailmb_data *)

#endif /* _UAPI_SAILMB_H_ */
