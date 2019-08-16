/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/**
 * @file acrn_common_def.h
 *
 * Common structure/definitions for acrn_ioctl/acrn_drv
 */

#ifndef _ACRN_COMMON_DEF_H
#define _ACRN_COMMON_DEF_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

/* Generic memory attributes */
#define	MEM_ACCESS_READ                 0x00000001
#define	MEM_ACCESS_WRITE                0x00000002
#define	MEM_ACCESS_EXEC	                0x00000004
#define	MEM_ACCESS_RWX			(MEM_ACCESS_READ | MEM_ACCESS_WRITE | \
						MEM_ACCESS_EXEC)
#define MEM_ACCESS_RIGHT_MASK           0x00000007
#define	MEM_TYPE_WB                     0x00000040
#define	MEM_TYPE_WT                     0x00000080
#define	MEM_TYPE_UC                     0x00000100
#define	MEM_TYPE_WC                     0x00000200
#define	MEM_TYPE_WP                     0x00000400
#define MEM_TYPE_MASK                   0x000007C0

/*
 * IO request
 */
#define ACRN_REQUEST_MAX 16

#define REQ_STATE_PENDING	0
#define REQ_STATE_COMPLETE	1
#define REQ_STATE_PROCESSING	2
#define REQ_STATE_FREE		3

#define REQ_PORTIO	0
#define REQ_MMIO	1
#define REQ_PCICFG	2
#define REQ_WP		3

#define REQUEST_READ	0
#define REQUEST_WRITE	1

/**
 * @brief Hypercall
 *
 * @addtogroup acrn_hypercall ACRN Hypercall
 * @{
 */

struct mmio_request {
	uint32_t direction;
	uint32_t reserved;
	uint64_t address;
	uint64_t size;
	uint64_t value;
};

struct pio_request {
	uint32_t direction;
	uint32_t reserved;
	uint64_t address;
	uint64_t size;
	uint32_t value;
};

struct pci_request {
	uint32_t direction;
	uint32_t reserved[3];/* need keep same header fields with pio_request */
	int64_t size;
	int32_t value;
	int32_t bus;
	int32_t dev;
	int32_t func;
	int32_t reg;
};

/**
 * struct acrn_request - 256-byte ACRN request
 *
 * The state transitions of a ACRN request are:
 *
 *    FREE -> PENDING -> PROCESSING -> COMPLETE -> FREE -> ...
 *                                \              /
 *                                 +--> FAILED -+
 *
 * When a request is in COMPLETE or FREE state, the request is owned by the
 * hypervisor. SOS (HSM or DM) shall not read or write the internals of the
 * request except the state.
 *
 * When a request is in PENDING or PROCESSING state, the request is owned by
 * SOS. The hypervisor shall not read or write the request other than the state.
 *
 * Based on the rules above, a typical ACRN request lifecycle should looks like
 * the following.
 *
 *                     (assume the initial state is FREE)
 *
 *       SOS vCPU 0                SOS vCPU x                    UOS vCPU y
 *
 *                                                 hypervisor:
 *                                                     fill in type, addr, etc.
 *                                                     pause UOS vcpu y
 *                                                     set state to PENDING (a)
 *                                                     fire upcall to SOS vCPU 0
 *
 *  HSM:
 *      scan for pending requests
 *      set state to PROCESSING (b)
 *      assign requests to clients (c)
 *
 *                            client:
 *                                scan for assigned requests
 *                                handle the requests (d)
 *                                set state to COMPLETE
 *                                notify the hypervisor
 *
 *                            hypervisor:
 *                                resume UOS vcpu y (e)
 *
 *                                                 hypervisor:
 *                                                     post-work (f)
 *                                                     set state to FREE
 *
 * Note that the following shall hold.
 *
 *   1. (a) happens before (b)
 *   2. (c) happens before (d)
 *   3. (e) happens before (f)
 *   4. One vCPU cannot trigger another I/O request before the previous one has
 *      completed (i.e. the state switched to FREE)
 *
 * Accesses to the state of a acrn_request shall be atomic and proper barriers
 * are needed to ensure that:
 *
 *   1. Setting state to PENDING is the last operation when issuing a request in
 *      the hypervisor, as the hypervisor shall not access the request any more.
 *
 *   2. Due to similar reasons, setting state to COMPLETE is the last operation
 *      of request handling in HSM or clients in SOS.
 */
struct acrn_request {
	/**
	 * @type: Type of this request. Byte offset: 0.
	 */
	uint32_t type;

	/**
	 * @completion_polling: Hypervisor will poll completion if set.
	 *
	 * Byte offset: 4.
	 */
	uint32_t completion_polling;


	/**
	 * @reserved0: Reserved fields. Byte offset: 4.
	 */
	uint32_t reserved0[14];

	/**
	 * @reqs: Details about this request.
	 *
	 * For REQ_PORTIO, this has type pio_request. For REQ_MMIO and REQ_WP,
	 * this has type mmio_request. For REQ_PCICFG, this has type
	 * pci_request. Byte offset: 64.
	 */
	union {
		struct pio_request pio_request;
		struct pci_request pci_request;
		struct mmio_request mmio_request;
		uint64_t reserved1[8];
	} reqs;

	/**
	 * @reserved1: Reserved fields. Byte offset: 128.
	 */
	uint32_t reserved1;

	/**
	 * @client: The client which is distributed to handle this request.
	 *
	 * Accessed by ACRN_HSM only. Byte offset: 132.
	 */
	int32_t client;

	/**
	 * @processed: The status of this request.
	 *
	 * Take REQ_STATE_xxx as values. Byte offset: 136.
	 */
	atomic_t processed;
} __aligned(256);

struct acrn_request_buffer {
	union {
		struct acrn_request req_queue[ACRN_REQUEST_MAX];
		uint8_t reserved[4096];
	};
};

#endif /* _ACRN_COMMON_DEF_H */
