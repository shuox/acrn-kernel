/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/**
 * @file acrn_hsm.h
 *
 * ACRN HSM APIs for other modules in the Service VM.
 */

#ifndef _ACRN_HSM_H
#define _ACRN_HSM_H

#include <linux/types.h>
#include <linux/acrn.h>

#define ACRN_NAME_LEN		16
struct acrn_vm;
struct acrn_ioreq_client;

typedef	int (*ioreq_handler_t)(struct acrn_ioreq_client *client,
				struct acrn_io_request *req);
struct acrn_ioreq_client {
	/* Client name */
	char name[ACRN_NAME_LEN];
	/* The VM that the client belongs to */
	struct acrn_vm *vm;
	/* List node for this acrn_ioreq_client */
	struct list_head list;
	/* The default client? */
	bool is_default;

#define ACRN_IOREQ_CLIENT_DESTROYING	0U
	unsigned long flags;

	/* I/O ranges */
	struct list_head range_list;
	rwlock_t range_lock;

	/* The pending I/O requests */
	DECLARE_BITMAP(ioreqs_map, ACRN_IO_REQUEST_MAX);

	/* I/O requests handler of this client */
	ioreq_handler_t handler;
	/* The thread which runs the handler */
	struct task_struct *thread;
	/* The wait queue for the handler thread in idle */
	wait_queue_head_t wq;
	/* Data for the thread */
	void *priv;
};

struct acrn_ioreq_client *acrn_ioreq_create_client(struct acrn_vm *vm,
				ioreq_handler_t handler, void *data,
				bool is_default, const char *name);
void acrn_ioreq_destroy_client(struct acrn_ioreq_client *client);
int acrn_ioreq_add_range(struct acrn_ioreq_client *client, u32 type,
			   u64 start, u64 end);
int acrn_ioreq_del_range(struct acrn_ioreq_client *client, u32 type,
			   u64 start, u64 end);

void *acrn_mm_gpa2sva(struct acrn_vm *vm, u64 user_gpa, size_t size);
int acrn_mm_add_region(u16 vmid, u64 user_gpa, u64 service_gpa, u64 size,
			   u32 mem_type, u32 mem_access_right);
int acrn_mm_del_region(u16 vmid, u64 user_gpa, u64 size);

#endif
