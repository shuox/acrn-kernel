/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * hypercall ID definition
 *
 */

#ifndef _ACRN_HV_DEFS_H
#define _ACRN_HV_DEFS_H

/*
 * Common structures for HV/HSM
 */

#define _HC_ID(x, y) (((x) << 24) | (y))

#define HC_ID 0x80UL

/* general */
#define HC_ID_GEN_BASE               0x0UL
#define HC_GET_API_VERSION          _HC_ID(HC_ID, HC_ID_GEN_BASE + 0x00)
#define HC_SOS_OFFLINE_CPU          _HC_ID(HC_ID, HC_ID_GEN_BASE + 0x01)
#define HC_GET_PLATFORM_INFO        _HC_ID(HC_ID, HC_ID_GEN_BASE + 0x03)

/* VM management */
#define HC_ID_VM_BASE               0x10UL
#define HC_CREATE_VM                _HC_ID(HC_ID, HC_ID_VM_BASE + 0x00)
#define HC_DESTROY_VM               _HC_ID(HC_ID, HC_ID_VM_BASE + 0x01)
#define HC_START_VM                 _HC_ID(HC_ID, HC_ID_VM_BASE + 0x02)
#define HC_PAUSE_VM                 _HC_ID(HC_ID, HC_ID_VM_BASE + 0x03)
#define HC_CREATE_VCPU              _HC_ID(HC_ID, HC_ID_VM_BASE + 0x04)
#define HC_RESET_VM                 _HC_ID(HC_ID, HC_ID_VM_BASE + 0x05)
#define HC_SET_VCPU_REGS            _HC_ID(HC_ID, HC_ID_VM_BASE + 0x06)

/* IRQ and Interrupts */
#define HC_ID_IRQ_BASE              0x20UL
#define HC_INJECT_MSI               _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x03)
#define HC_VM_INTR_MONITOR          _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x04)
#define HC_SET_IRQLINE              _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x05)

/* DM ioreq management */
#define HC_ID_IOREQ_BASE            0x30UL
#define HC_SET_IOREQ_BUFFER         _HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x00)
#define HC_NOTIFY_REQUEST_FINISH    _HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x01)

/* Guest memory management */
#define HC_ID_MEM_BASE              0x40UL
#define HC_VM_SET_MEMORY_REGIONS    _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x02)
#define HC_VM_WRITE_PROTECT_PAGE    _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x03)

/* PCI assignment*/
#define HC_ID_PCI_BASE              0x50UL
#define HC_ASSIGN_PTDEV             _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x00)
#define HC_DEASSIGN_PTDEV           _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x01)
#define HC_SET_PTDEV_INTR_INFO      _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x03)
#define HC_RESET_PTDEV_INTR_INFO    _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x04)

/* DEBUG */
#define HC_ID_DBG_BASE              0x60UL

/* Power management */
#define HC_ID_PM_BASE               0x80UL
#define HC_PM_GET_CPU_STATE         _HC_ID(HC_ID, HC_ID_PM_BASE + 0x00)
#define HC_PM_SET_SSTATE_DATA       _HC_ID(HC_ID, HC_ID_PM_BASE + 0x01)

#endif /* __ACRN_HV_DEFS_H */
