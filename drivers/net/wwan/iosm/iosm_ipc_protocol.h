/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020-21 Intel Corporation.
 */

#ifndef IOSM_IPC_PROTOCOL_H
#define IOSM_IPC_PROTOCOL_H

#include "iosm_ipc_imem.h"
#include "iosm_ipc_pm.h"
#include "iosm_ipc_protocol_ops.h"

/* Trigger the doorbell interrupt on CP. */
#define IPC_DOORBELL_IRQ_HPDA 0
#define IPC_DOORBELL_IRQ_IPC 1
#define IPC_DOORBELL_IRQ_SLEEP 2

/* IRQ vector number. */
#define IPC_DEVICE_IRQ_VECTOR 0
#define IPC_MSG_IRQ_VECTOR 0
#define IPC_UL_PIPE_IRQ_VECTOR 0
#define IPC_DL_PIPE_IRQ_VECTOR 0

#define IPC_MEM_MSG_ENTRIES 128

/* Default time out for sending IPC messages like open pipe, close pipe etc.
 * during run mode.
 *
 * If the message interface lock to CP times out, the link to CP is broken.
 * mode : run mode (IPC_MEM_EXEC_STAGE_RUN)
 * unit : milliseconds
 */
#define IPC_MSG_COMPLETE_RUN_DEFAULT_TIMEOUT 500 /* 0.5 seconds */

/* Default time out for sending IPC messages like open pipe, close pipe etc.
 * during boot mode.
 *
 * If the message interface lock to CP times out, the link to CP is broken.
 * mode : boot mode
 * (IPC_MEM_EXEC_STAGE_BOOT | IPC_MEM_EXEC_STAGE_PSI | IPC_MEM_EXEC_STAGE_EBL)
 * unit : milliseconds
 */
#define IPC_MSG_COMPLETE_BOOT_DEFAULT_TIMEOUT 500 /* 0.5 seconds */

/**
 * struct ipc_protocol_context_info - Structure of the context info
 * @device_info_addr:		64 bit address to device info
 * @head_array:			64 bit address to head pointer arr for the pipes
 * @tail_array:			64 bit address to tail pointer arr for the pipes
 * @msg_head:			64 bit address to message head pointer
 * @msg_tail:			64 bit address to message tail pointer
 * @msg_ring_addr:		64 bit pointer to the message ring buffer
 * @msg_ring_entries:		This field provides the number of entries which
 *				the MR can hold
 * @msg_irq_vector:		This field provides the IRQ which shall be
 *				generated by the EP device when generating
 *				completion for Messages.
 * @device_info_irq_vector:	This field provides the IRQ which shall be
 *				generated by the EP dev after updating Dev. Info
 */
struct ipc_protocol_context_info {
	phys_addr_t device_info_addr;
	phys_addr_t head_array;
	phys_addr_t tail_array;
	phys_addr_t msg_head;
	phys_addr_t msg_tail;
	phys_addr_t msg_ring_addr;
	__le16 msg_ring_entries;
	u8 msg_irq_vector;
	u8 device_info_irq_vector;
};

/**
 * struct ipc_protocol_device_info - Structure for the device information
 * @execution_stage:		CP execution stage
 * @ipc_status:			IPC states
 * @device_sleep_notification:	Requested device pm states
 */
struct ipc_protocol_device_info {
	__le32 execution_stage;
	__le32 ipc_status;
	__le32 device_sleep_notification;
};

/**
 * struct ipc_protocol_ap_shm - Protocol Shared Memory Structure
 * @ci:			Context information struct
 * @device_info:	Device information struct
 * @msg_head:		Point to msg head
 * @head_array:		Array of head pointer
 * @msg_tail:		Point to msg tail
 * @tail_array:		Array of tail pointer
 * @msg_ring:		Circular buffers for the read/tail and write/head
 *			indeces.
 */
struct ipc_protocol_ap_shm {
	struct ipc_protocol_context_info ci;
	struct ipc_protocol_device_info device_info;
	__le32 msg_head;
	__le32 head_array[IPC_MEM_MAX_PIPES];
	__le32 msg_tail;
	__le32 tail_array[IPC_MEM_MAX_PIPES];
	union ipc_mem_msg_entry msg_ring[IPC_MEM_MSG_ENTRIES];
};

/**
 * struct iosm_protocol - Structure for IPC protocol.
 * @p_ap_shm:		Pointer to Protocol Shared Memory Structure
 * @pm:			Instance to struct iosm_pm
 * @pcie:		Pointer to struct iosm_pcie
 * @imem:		Pointer to struct iosm_imem
 * @rsp_ring:		Array of OS completion objects to be triggered once CP
 *			acknowledges a request in the message ring
 * @dev:		Pointer to device structure
 * @phy_ap_shm:		Physical/Mapped representation of the shared memory info
 * @old_msg_tail:	Old msg tail ptr, until AP has handled ACK's from CP
 */
struct iosm_protocol {
	struct ipc_protocol_ap_shm *p_ap_shm;
	struct iosm_pm pm;
	struct iosm_pcie *pcie;
	struct iosm_imem *imem;
	struct ipc_rsp *rsp_ring[IPC_MEM_MSG_ENTRIES];
	struct device *dev;
	dma_addr_t phy_ap_shm;
	u32 old_msg_tail;
};

/**
 * struct ipc_call_msg_send_args - Structure for message argument for
 *				   tasklet function.
 * @prep_args:		Arguments for message preparation function
 * @response:		Can be NULL if result can be ignored
 * @msg_type:		Message Type
 */
struct ipc_call_msg_send_args {
	union ipc_msg_prep_args *prep_args;
	struct ipc_rsp *response;
	enum ipc_msg_prep_type msg_type;
};

/**
 * ipc_protocol_tq_msg_send - prepare the msg and send to CP
 * @ipc_protocol:	Pointer to ipc_protocol instance
 * @msg_type:		Message type
 * @prep_args:		Message arguments
 * @response:		Pointer to a response object which has a
 *			completion object and return code.
 *
 * Returns: 0 on success and failure value on error
 */
int ipc_protocol_tq_msg_send(struct iosm_protocol *ipc_protocol,
			     enum ipc_msg_prep_type msg_type,
			     union ipc_msg_prep_args *prep_args,
			     struct ipc_rsp *response);

/**
 * ipc_protocol_msg_send - Send ipc control message to CP and wait for response
 * @ipc_protocol:	Pointer to ipc_protocol instance
 * @prep:		Message type
 * @prep_args:		Message arguments
 *
 * Returns: 0 on success and failure value on error
 */
int ipc_protocol_msg_send(struct iosm_protocol *ipc_protocol,
			  enum ipc_msg_prep_type prep,
			  union ipc_msg_prep_args *prep_args);

/**
 * ipc_protocol_suspend - Signal to CP that host wants to go to sleep (suspend).
 * @ipc_protocol:	Pointer to ipc_protocol instance
 *
 * Returns: true if host can suspend, false if suspend must be aborted.
 */
bool ipc_protocol_suspend(struct iosm_protocol *ipc_protocol);

/**
 * ipc_protocol_s2idle_sleep - Call PM function to set PM variables in s2idle
 *			       sleep/active case
 * @ipc_protocol:	Pointer to ipc_protocol instance
 * @sleep:		True for sleep/False for active
 */
void ipc_protocol_s2idle_sleep(struct iosm_protocol *ipc_protocol, bool sleep);

/**
 * ipc_protocol_resume - Signal to CP that host wants to resume operation.
 * @ipc_protocol:	Pointer to ipc_protocol instance
 *
 * Returns: true if host can resume, false if there is a problem.
 */
bool ipc_protocol_resume(struct iosm_protocol *ipc_protocol);

/**
 * ipc_protocol_pm_dev_sleep_handle - Handles the Device Sleep state change
 *				      notification.
 * @ipc_protocol:	Pointer to ipc_protocol instance.
 *
 * Returns: true if sleep notification handled, false otherwise.
 */
bool ipc_protocol_pm_dev_sleep_handle(struct iosm_protocol *ipc_protocol);

/**
 * ipc_protocol_doorbell_trigger - Wrapper for PM function which wake up the
 *				   device if it is in low power mode
 *				   and trigger a head pointer update interrupt.
 * @ipc_protocol:	Pointer to ipc_protocol instance.
 * @identifier:		Specifies what component triggered hpda
 *			update irq
 */
void ipc_protocol_doorbell_trigger(struct iosm_protocol *ipc_protocol,
				   u32 identifier);

/**
 * ipc_protocol_sleep_notification_string - Returns last Sleep Notification as
 *					    string.
 * @ipc_protocol:	Instance pointer of Protocol module.
 *
 * Returns: Pointer to string.
 */
const char *
ipc_protocol_sleep_notification_string(struct iosm_protocol *ipc_protocol);

/**
 * ipc_protocol_init - Allocates IPC protocol instance
 * @ipc_imem:		Pointer to iosm_imem structure
 *
 * Returns: Address of IPC  protocol instance on success & NULL on failure.
 */
struct iosm_protocol *ipc_protocol_init(struct iosm_imem *ipc_imem);

/**
 * ipc_protocol_deinit - Deallocates IPC protocol instance
 * @ipc_protocol:	pointer to the IPC protocol instance
 */
void ipc_protocol_deinit(struct iosm_protocol *ipc_protocol);

#endif
