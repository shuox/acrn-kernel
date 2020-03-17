/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/**
 * @file acrn.h
 *
 * Userspace interface for /dev/acrn_hsm
 * Common structure/definitions for acrn ioctl
 */

#ifndef _ACRN_H
#define _ACRN_H

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

/**
 * struct api_version - data structure to track ACRN_SRV API version
 *
 * @major_version: major version of ACRN_SRV API
 * @minor_version: minor version of ACRN_SRV API
 */
struct api_version {
	uint32_t major_version;
	uint32_t minor_version;
};

/**
 * @brief Info to create a VM, the parameter for HC_CREATE_VM hypercall
 */
struct acrn_create_vm {
	/** created vmid return to ACRN. Keep it first field */
	uint16_t vmid;

	/** Reserved */
	uint16_t reserved0;

	/** VCPU numbers this VM want to create */
	uint16_t vcpu_num;

	/** Reserved */
	uint16_t reserved1;

	/** the GUID of this VM */
	uint8_t	 GUID[16];

	/* VM flag bits from Guest OS. */
	uint64_t vm_flag;

	/** guest physical address of VM request_buffer */
	uint64_t req_buf;

	/** Reserved for future use*/
	uint8_t  reserved2[16];
};

/**
 * @brief Info to create a VCPU
 *
 * the parameter for HC_CREATE_VCPU hypercall
 */
struct acrn_create_vcpu {
	/** the virtual CPU ID for the VCPU created */
	uint16_t vcpu_id;

	/** the physical CPU ID for the VCPU created */
	uint16_t pcpu_id;

	/** Reserved for future use*/
	uint8_t reserved[4];
} __aligned(8);

struct acrn_gp_regs {
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rbx;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
};

struct acrn_descriptor_ptr {
	uint16_t limit;
	uint64_t base;
	uint16_t reserved[3];
} __packed;

struct acrn_vcpu_regs {
	struct acrn_gp_regs gprs;
	struct acrn_descriptor_ptr gdt;
	struct acrn_descriptor_ptr idt;

	uint64_t        rip;
	uint64_t        cs_base;
	uint64_t        cr0;
	uint64_t        cr4;
	uint64_t        cr3;
	uint64_t        ia32_efer;
	uint64_t        rflags;
	uint64_t        reserved_64[4];

	uint32_t        cs_ar;
	uint32_t        reserved_32[4];

	/* don't change the order of following sel */
	uint16_t        cs_sel;
	uint16_t        ss_sel;
	uint16_t        ds_sel;
	uint16_t        es_sel;
	uint16_t        fs_sel;
	uint16_t        gs_sel;
	uint16_t        ldt_sel;
	uint16_t        tr_sel;

	uint16_t        reserved_16[4];
};

/**
 * @brief Info to set vcpu state
 *
 * the pamameter for HC_SET_VCPU_REGS
 */
struct acrn_set_vcpu_regs {
	/** the virtual CPU ID for the VCPU */
	uint16_t vcpu_id;

	/** reserved space to make cpu_state aligned to 8 bytes */
	uint16_t reserved0[3];

	/** the structure to hold vcpu state */
	struct acrn_vcpu_regs vcpu_regs;
};

/**
 * struct ic_ptdev_irq - pass thru device irq data structure
 */
struct ic_ptdev_irq {
#define IRQ_INTX 0
#define IRQ_MSI 1
#define IRQ_MSIX 2
	/** @type: irq type */
	uint32_t type;
	/** @virt_bdf: virtual bdf description of pass thru device */
	uint16_t virt_bdf;	/* IN: Device virtual BDF# */
	/** @phys_bdf: physical bdf description of pass thru device */
	uint16_t phys_bdf;	/* IN: Device physical BDF# */
	/** union */
	union {
		/** struct intx - info of IOAPIC/PIC interrupt */
		struct {
			/** @virt_pin: virtual IOAPIC pin */
			uint32_t virt_pin;
			/** @phys_pin: physical IOAPIC pin */
			uint32_t phys_pin;
			/** @is_pic_pin: PIC pin */
			uint32_t is_pic_pin;
		} intx;

		/** struct msix - info of MSI/MSIX interrupt */
		struct {
			/* Keep this filed on top of msix */
			/** @vector_cnt: vector count of MSI/MSIX */
			uint32_t vector_cnt;

			/** @table_size: size of MSIX table(round up to 4K) */
			uint32_t table_size;

			/** @table_paddr: physical address of MSIX table */
			uint64_t table_paddr;
		} msix;
	};
};

#define VM_MEMMAP_SYSMEM       0
#define VM_MEMMAP_MMIO         1

/**
 * struct vm_memmap - EPT memory mapping info for guest
 */
struct vm_memmap {
	/** @type: memory mapping type */
	uint32_t type;
	uint32_t reserved;
	/** @guest_vm_pa: guest VM's physical address of memory mapping */
	uint64_t guest_vm_pa;
	union {
		/** @hpa: host VM's physical address of memory mapping */
		uint64_t host_vm_pa;
		/** @vma_base: host VM's vma of memory mapping */
		uint64_t vma_base;
	};
	/** @len: length of memory mapping */
	uint64_t len;
	/** @attr: attribut of memory mapping */
	uint32_t attr;	/* RWX */
};

/**
 * @brief Info to inject a MSI interrupt to VM
 *
 * the parameter for HC_INJECT_MSI hypercall
 */
struct acrn_msi_entry {
	/** MSI addr[19:12] with dest VCPU ID */
	uint64_t msi_addr;

	/** MSI data[7:0] with vector */
	uint64_t msi_data;
};

/**
 * struct ioreq_notify - data structure to notify hypervisor ioreq is handled
 *
 * @client_id: client id to identify ioreq client
 * @vcpu: identify the ioreq submitter
 */
struct ioreq_notify {
	int32_t client_id;
	uint32_t vcpu;
};

struct acrn_generic_address {
	uint8_t		space_id;
	uint8_t		bit_width;
	uint8_t		bit_offset;
	uint8_t		access_size;
	uint64_t	address;
};

struct cpu_cx_data {
	struct acrn_generic_address cx_reg;
	uint8_t		type;
	uint32_t	latency;
	uint64_t	power;
};

struct cpu_px_data {
	uint64_t core_frequency;	/* megahertz */
	uint64_t power;			/* milliWatts */
	uint64_t transition_latency;	/* microseconds */
	uint64_t bus_master_latency;	/* microseconds */
	uint64_t control;		/* control value */
	uint64_t status;		/* success indicator */
};

#define PMCMD_TYPE_MASK		0x000000ff

enum pm_cmd_type {
	PMCMD_GET_PX_CNT,
	PMCMD_GET_PX_DATA,
	PMCMD_GET_CX_CNT,
	PMCMD_GET_CX_DATA,
};

#define ACRN_IOEVENTFD_FLAG_PIO		0x01
#define ACRN_IOEVENTFD_FLAG_DATAMATCH	0x02
#define ACRN_IOEVENTFD_FLAG_DEASSIGN	0x04
struct acrn_ioeventfd {
	int32_t fd;
	uint32_t flags;
	uint64_t addr;
	uint32_t len;
	uint32_t reserved;
	uint64_t data;
};

#define ACRN_IRQFD_FLAG_DEASSIGN	0x01
struct acrn_irqfd {
	int32_t fd;
	uint32_t flags;
	struct acrn_msi_entry msi;
};

/*
 * Common IOCTL ID definition for DM
 */
#define _IC_ID(x, y) (((x) << 24) | (y))
#define IC_ID 0x43UL

/* General */
#define IC_ID_GEN_BASE                  0x0UL
#define IC_GET_API_VERSION             _IC_ID(IC_ID, IC_ID_GEN_BASE + 0x00)

/* VM management */
#define IC_ID_VM_BASE                  0x10UL
#define IC_CREATE_VM                   _IC_ID(IC_ID, IC_ID_VM_BASE + 0x00)
#define IC_DESTROY_VM                  _IC_ID(IC_ID, IC_ID_VM_BASE + 0x01)
#define IC_START_VM                    _IC_ID(IC_ID, IC_ID_VM_BASE + 0x02)
#define IC_PAUSE_VM                    _IC_ID(IC_ID, IC_ID_VM_BASE + 0x03)
#define IC_CREATE_VCPU                 _IC_ID(IC_ID, IC_ID_VM_BASE + 0x04)
#define IC_RESET_VM                    _IC_ID(IC_ID, IC_ID_VM_BASE + 0x05)
#define IC_SET_VCPU_REGS               _IC_ID(IC_ID, IC_ID_VM_BASE + 0x06)

/* IRQ and Interrupts */
#define IC_ID_IRQ_BASE                 0x20UL
#define IC_INJECT_MSI                  _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x03)
#define IC_VM_INTR_MONITOR             _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x04)
#define IC_SET_IRQLINE                 _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x05)

/* DM ioreq management */
#define IC_ID_IOREQ_BASE                0x30UL
#define IC_SET_IOREQ_BUFFER             _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x00)
#define IC_NOTIFY_REQUEST_FINISH        _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x01)
#define IC_CREATE_IOREQ_CLIENT          _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x02)
#define IC_ATTACH_IOREQ_CLIENT          _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x03)
#define IC_DESTROY_IOREQ_CLIENT         _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x04)
#define IC_CLEAR_VM_IOREQ               _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x05)

/* Guest memory management */
#define IC_ID_MEM_BASE                  0x40UL
#define IC_SET_MEMSEG                   _IC_ID(IC_ID, IC_ID_MEM_BASE + 0x01)
#define IC_UNSET_MEMSEG                 _IC_ID(IC_ID, IC_ID_MEM_BASE + 0x02)

/* PCI assignment*/
#define IC_ID_PCI_BASE                  0x50UL
#define IC_ASSIGN_PTDEV                _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x00)
#define IC_DEASSIGN_PTDEV              _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x01)
#define IC_SET_PTDEV_INTR_INFO         _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x03)
#define IC_RESET_PTDEV_INTR_INFO       _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x04)

/* Power management */
#define IC_ID_PM_BASE                   0x60UL
#define IC_PM_GET_CPU_STATE            _IC_ID(IC_ID, IC_ID_PM_BASE + 0x00)

/* VHM eventfd */
#define IC_ID_EVENT_BASE		0x70UL
#define IC_EVENT_IOEVENTFD		_IC_ID(IC_ID, IC_ID_EVENT_BASE + 0x00)
#define IC_EVENT_IRQFD			_IC_ID(IC_ID, IC_ID_EVENT_BASE + 0x01)

#endif /* _ACRN_H */
