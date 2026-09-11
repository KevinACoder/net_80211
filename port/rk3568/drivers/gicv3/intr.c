/*
 * @file
 * @brief Bare-metal interrupt layer over the GICv3/ITS driver: the
 *        flat Interrupt* API consumed by the FreeRTOS port, the tick
 *        setup and the bus backends.
 *
 * Hardware coordinates for this board (GIC-600): distributor
 * 0xfd400000, redistributor frames from 0xfd460000 with a 128 KB
 * stride, ITS at 0xfd440000. The SPI carrying the PCIe INTx
 * aggregation is configured by the PCIe backend through this API.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#include <stdint.h>
#include <string.h>

#include "fkernel.h"
#include "faarch.h"
#include "fsleep.h"
#include "fgeneric_timer.h"

#include "intr.h"
#include "gicv3.h"
#include "gicv3_its.h"

#ifdef CONFIG_USE_VIRTUAL_GTIMER
#define RPR_TEST_TIMER_ID		GENERIC_TIMER_ID1
#define RPR_TEST_TIMER_IRQ_NUM		GENERIC_VTIMER_IRQ_NUM
#else
#define RPR_TEST_TIMER_ID		GENERIC_TIMER_ID0
#define RPR_TEST_TIMER_IRQ_NUM		GENERIC_PTIMER_IRQ_NUM
#endif

#define TIMER_PRIORITY 8

/* board GIC-600 coordinates */
#define INTR_GICD_BASE		0xFD400000u
#define INTR_GICR_BASE		0xFD460000u
#define INTR_GICR_STRIDE	0x20000u
#define INTR_GICR_COUNT		4u
#define INTR_ITS_BASE		0xFD440000u

/* exception and interrupt handler tables (dispatched by the board
 * exception path) */
struct IrqDesc isr_table[INTR_MAX_HANDLERS];
struct IrqDesc lpi_isr_table[INTR_MAX_LPI_HANDLERS];

static struct intr_softc intr_instance;
static InterruptDrvType *intr_softc_p;

/* set when the CPU interface implements fewer priority bits than the
 * registers: raw mask values need translation before programming */
static uint8_t need_translate;

uint32_t
intr_priority_translate_set(uint32_t value)
{

	return ((value >> 1) | 0x80) & 0xff;
}

uint32_t
intr_priority_translate_get(uint32_t value)
{

	return (value << 1) & 0xff;
}

#define PRIORITY_TRANSLATE_SET(x)	intr_priority_translate_set(x)
#define PRIORITY_TRANSLATE_GET(x)	intr_priority_translate_get(x)

#define INTR_LPI_BASE	GIC_LPI_BASE

static struct gicv3_softc *
intr_gic(void)
{

	if (intr_softc_p == NULL) {
		return NULL;
	}
	return &((struct intr_softc *) intr_softc_p)->gic;
}

static struct gicv3_its *
intr_its(void)
{

#ifdef CONFIG_ENABLE_GIC_ITS
	if (intr_softc_p == NULL) {
		return NULL;
	}
	return &((struct intr_softc *) intr_softc_p)->its;
#else
	return NULL;
#endif
}

void
InterruptMask(int int_id)
{
	struct gicv3_softc *sc = intr_gic();

	if (sc == NULL)
		return;

	if (int_id >= INTR_LPI_BASE) {
#ifdef CONFIG_ENABLE_GIC_ITS
		gicv3_its_lpi_disable(intr_its(), (u_int) int_id);
#endif
		return;
	}
	gicv3_block_irq(sc, (u_int) int_id);
}

void
InterruptUmask(int int_id)
{
	struct gicv3_softc *sc = intr_gic();

	if (sc == NULL)
		return;

	if (int_id >= INTR_LPI_BASE) {
#ifdef CONFIG_ENABLE_GIC_ITS
		gicv3_its_lpi_enable(intr_its(), (u_int) int_id);
#endif
		return;
	}
	gicv3_unblock_irq(sc, (u_int) int_id);
}

int
InterruptGetAck(void)
{
	uint32_t iar;

	__asm volatile("mrs %0, ICC_IAR1_EL1" : "=r"(iar));
	return (int) iar;
}

void
InterruptDeactivation(int int_id)
{
	uint32_t val = (uint32_t) int_id;

	__asm volatile("msr ICC_DIR_EL1, %0" :: "r"(val));
}

void
InterruptSetTrigerMode(int int_id, unsigned int mode)
{
	struct gicv3_softc *sc = intr_gic();

	if (sc == NULL || int_id >= INTR_LPI_BASE)
		return;

	gicv3_establish_irq(sc, (u_int) int_id,
	    mode == IRQ_MODE_TRIG_EDGE);
}

unsigned int
InterruptGetTrigerMode(int int_id)
{
	/* trigger state lives in the establish-only configuration; the
	 * callers only re-assert, so report the reset default */
	(void) int_id;
	return IRQ_MODE_TRIG_LEVEL;
}

void
InterruptSetPriority(int int_id, unsigned int priority)
{
	struct gicv3_softc *sc = intr_gic();
	const uint8_t prio8 = (uint8_t) ((priority << IRQ_PRIORITY_OFFSET) &
	    0xff);

	if (sc == NULL)
		return;

	if (int_id >= INTR_LPI_BASE) {
#ifdef CONFIG_ENABLE_GIC_ITS
		gicv3_its_lpi_set_priority(intr_its(), (u_int) int_id,
		    prio8);
#endif
		return;
	}
	gicv3_set_irq_priority(sc, (u_int) int_id, prio8);
}

unsigned int
InterruptGetPriority(int int_id)
{
	/* untracked: consumers only program priorities */
	(void) int_id;
	return 0;
}

void
InterruptSetPriorityMask(unsigned int priority)
{

	if (need_translate) {
		priority = PRIORITY_TRANSLATE_SET(priority);
	}
	icc_pmr_write(priority);
}

unsigned int
InterruptGetPriorityMask(void)
{
	uint32_t priority = icc_pmr_read();

	if (need_translate) {
		priority = PRIORITY_TRANSLATE_GET(priority);
	}
	return priority;
}

uint32_t
InterruptGetCurrentPriority(void)
{
	uint32_t icc_rpr = icc_rpr_read();

	if (need_translate) {
		return PRIORITY_TRANSLATE_GET(icc_rpr);
	}
	return icc_rpr;
}

uint8_t
InterruptGetPriorityConfig(void)
{

	return need_translate;
}

void
InterruptSetPriorityGroupBits(unsigned int bits)
{

	icc_bpr1_write(bits & 0x7);
}

uint32_t
intr_icc_bpr1_read(void)
{

	return icc_bpr1_read();
}

void
InterruptInstall(int int_id, IrqHandler handler, void *param,
    const char *name)
{

	(void) name;
	if (int_id >= 0 && int_id < INTR_MAX_HANDLERS) {
		if (handler != NULL) {
			isr_table[int_id].handler = handler;
			isr_table[int_id].param = param;
		}
	} else if (int_id >= INTR_LPI_BASE &&
	    int_id < INTR_LPI_BASE + INTR_MAX_LPI_HANDLERS) {
		lpi_isr_table[int_id - INTR_LPI_BASE].handler = handler;
		lpi_isr_table[int_id - INTR_LPI_BASE].param = param;
	}
}

void
intr_dump_lpi(uint32_t lpi, uint8_t *propp, uint8_t *pendp)
{
	struct gicv3_its *its = intr_its();

	if (its == NULL || lpi < INTR_LPI_BASE) {
		if (propp != NULL)
			*propp = 0xff;
		if (pendp != NULL)
			*pendp = 0xff;
		return;
	}
	gicv3_its_dump_lpi(its, lpi, propp, pendp);
}

/*
 * Measure the running-priority step while lowering the priority of a
 * timer interrupt by one nibble: a step of 8 means the CPU interface
 * renders priorities with fewer bits and every raw mask value needs
 * the translate shift.
 */
static uint32_t rpr_array[2];

static void intr_rpr_probe_handler(int32_t vector, void *param)
{
	static uint16_t interrupt_count;

	(void) param;

	if (vector != RPR_TEST_TIMER_IRQ_NUM) {
		return;
	}

	rpr_array[interrupt_count] = icc_rpr_read();

	interrupt_count++;
	InterruptSetPriority(RPR_TEST_TIMER_IRQ_NUM, TIMER_PRIORITY + 1);
	GenericTimerSetTimerValue(RPR_TEST_TIMER_ID, 1);
	if (interrupt_count == 2) {
		GenericTimerStop(RPR_TEST_TIMER_ID);
		GenericTimerInterruptDisable(RPR_TEST_TIMER_ID);
		InterruptMask(RPR_TEST_TIMER_IRQ_NUM);
		interrupt_count = 0;
	}
}

void
InterruptInit(InterruptDrvType *int_driver_p, uint32_t instance_id,
    INTERRUPT_ROLE_SELECT role_select)
{
	struct intr_softc *sc = (struct intr_softc *) int_driver_p;

	(void) instance_id;
	if (role_select == INTERRUPT_ROLE_NONE) {
		return;
	}

	memset(isr_table, 0, sizeof(isr_table));
	memset(lpi_isr_table, 0, sizeof(lpi_isr_table));

	intr_softc_p = int_driver_p;
	sc->gic.sc_gicd_base = INTR_GICD_BASE;
	sc->gic.sc_gicr_base = INTR_GICR_BASE;
	sc->gic.sc_gicr_stride = INTR_GICR_STRIDE;
	sc->gic.sc_gicr_count = INTR_GICR_COUNT;

	if (gicv3_init(&sc->gic) != 0) {
		/* no redistributor for this PE: leave the controller as
		 * firmware programmed it */
		intr_softc_p = NULL;
		return;
	}

	/* open the priority filter: the callers manage the mask from
	 * here on */
	InterruptSetPriorityMask(0xff);

	/* ICC_BPR1_EL1 uses the gggg.ssss group field split */
	InterruptSetPriorityGroupBits(0);

	/* calibrate the priority-view translation */
	{
		InterruptSetPriority(RPR_TEST_TIMER_IRQ_NUM, TIMER_PRIORITY);
		InterruptInstall(RPR_TEST_TIMER_IRQ_NUM,
		    intr_rpr_probe_handler, NULL, NULL);
		InterruptUmask(RPR_TEST_TIMER_IRQ_NUM);

		GenericTimerStop(RPR_TEST_TIMER_ID);
		GenericTimerSetTimerValue(RPR_TEST_TIMER_ID, 1000);
		GenericTimerInterruptEnable(RPR_TEST_TIMER_ID);
		GenericTimerStart(RPR_TEST_TIMER_ID);
		INTERRUPT_ENABLE();
		fsleep_microsec(100);
		INTERRUPT_DISABLE();
		GenericTimerStop(RPR_TEST_TIMER_ID);
		GenericTimerInterruptDisable(RPR_TEST_TIMER_ID);
		InterruptMask(RPR_TEST_TIMER_IRQ_NUM);

		const uint32_t step = rpr_array[1] - rpr_array[0];

		if (step == 8) {
			need_translate = 1;
		} else {
			need_translate = 0;
		}
	}

#ifdef CONFIG_ENABLE_GIC_ITS
	/* ITS/LPI init */
	gicv3_its_init(&sc->gic, INTR_ITS_BASE, &sc->its);
#endif
}

void
InterruptEarlyInit(void)
{

#if defined(CONFIG_USE_DEFAULT_INTERRUPT_CONFIG)
	static InterruptDrvType early_instance;

#if defined(CONFIG_INTERRUPT_ROLE_SLAVE)
	InterruptInit(&early_instance, INTERRUPT_DRV_INTS_ID,
	    INTERRUPT_ROLE_SLAVE);
#elif defined(CONFIG_INTERRUPT_ROLE_MASTER)
	InterruptInit(&early_instance, INTERRUPT_DRV_INTS_ID,
	    INTERRUPT_ROLE_MASTER);
#endif
#endif
}
