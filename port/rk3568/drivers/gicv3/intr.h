/*
 * @file
 * @brief Interrupt layer for the FreeRTOS test system: the GICv3/ITS
 *        driver (see gicv3.c, gicv3_its.c, both NetBSD-derived) plus
 *        the flat Interrupt* API the port layer and the bus backends
 *        consume.
 *
 * The API mirrors the surface the FreeRTOS portable layer expects:
 * priority values are the 16-step nibble scale (register value is
 * value << 4), the priority mask is programmed in raw register scale,
 * and a runtime RPR probe decides whether the CPU interface needs a
 * translated view of both.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#ifndef _DRIVERS_GICV3_INTR_H_
#define _DRIVERS_GICV3_INTR_H_

#include <stdint.h>

#include "sdkconfig.h"
#include "gicv3.h"
#include "gicv3_its.h"

/* one interrupt controller instance: the NetBSD-derived core plus
 * its ITS attachment */
struct intr_softc {
	struct gicv3_softc gic;
	struct gicv3_its its;
};

/* API handle: a single interrupt controller instance */
typedef struct intr_softc InterruptDrvType;

#define INTERRUPT_DRV_INTS_ID 0

typedef enum {
	INTERRUPT_ROLE_MASTER = 0,
	INTERRUPT_ROLE_SLAVE,
	INTERRUPT_ROLE_NONE,
} INTERRUPT_ROLE_SELECT;

#define IRQ_MODE_TRIG_LEVEL	0x00
#define IRQ_MODE_TRIG_EDGE	0x01

#define IRQ_PRIORITY_OFFSET	4	/* implemented priority bit offset */

typedef void (*IrqHandler)(int32_t vector, void *param);

struct IrqDesc {
	IrqHandler handler;
	void *param;
};

/* set when the CPU interface implements fewer priority bits than the
 * registers: raw mask values need translation before programming
 * (the FreeRTOS port layer calls these as functions) */
uint32_t intr_priority_translate_set(uint32_t value);
uint32_t intr_priority_translate_get(uint32_t value);

#define PRIORITY_TRANSLATE_SET(x)	intr_priority_translate_set((x))
#define PRIORITY_TRANSLATE_GET(x)	intr_priority_translate_get((x))

#define INTR_MAX_HANDLERS 1024
#define INTR_MAX_LPI_HANDLERS 8192

void InterruptInit(InterruptDrvType *int_driver_p, uint32_t instance_id,
	    INTERRUPT_ROLE_SELECT role_select);
void InterruptEarlyInit(void);

void InterruptMask(int int_id);
void InterruptUmask(int int_id);

void InterruptDeactivation(int int_id);
int InterruptGetAck(void);

void InterruptSetTrigerMode(int int_id, unsigned int mode);
unsigned int InterruptGetTrigerMode(int int_id);

void InterruptSetPriority(int int_id, unsigned int priority);
unsigned int InterruptGetPriority(int int_id);

void InterruptSetPriorityMask(unsigned int priority);
unsigned int InterruptGetPriorityMask(void);
uint32_t InterruptGetCurrentPriority(void);
uint8_t InterruptGetPriorityConfig(void);

void InterruptSetPriorityGroupBits(unsigned int bits);

/* raw ICC_BPR1_EL1 view for the port's priority-group assertion */
uint32_t intr_icc_bpr1_read(void);

void InterruptInstall(int int_id, IrqHandler handler, void *param,
	    const char *name);

/* diagnostics: LPI property/pending view (driver/iwm attach path) */
void intr_dump_lpi(uint32_t lpi, uint8_t *propp, uint8_t *pendp);

#endif	/* _DRIVERS_GICV3_INTR_H_ */
