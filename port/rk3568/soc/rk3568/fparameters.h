/*
 * Copyright (C) 2026, Phytium Technology Co., Ltd.   All Rights Reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * FilePath: fparameters.h
 * Date: 2026-08-06
 * Description: RK3568 soc parameters for standalone bring-up
 */

#ifndef RK3568_FPARAMETERS_H
#define RK3568_FPARAMETERS_H

#ifdef __cplusplus
extern "C"
{
#endif

#if !defined(__ASSEMBLER__)
#include "ftypes.h"
#endif

#define SOC_TARGET_RK3568

#define CORE0_AFF                     0x0
#define CORE1_AFF                     0x1
#define CORE2_AFF                     0x2
#define CORE3_AFF                     0x3
#define CORE_AFF_MASK                 0xF
#define FCORE_NUM                     4
#define FCPU_CONFIG_ARM64_PA_BITS     40
/* cache */
#define CACHE_LINE_ADDR_MASK          0x3FUL
#define CACHE_LINE                    64U

/* UART (DW-APB 16550 compatible, 24MHz baud clock) */
#define FUART0_ID                     0
#define FUART1_ID                     1
#define FUART2_ID                     2
#define FUART_NUM                     3

#define FUART0_IRQ_NUM                148
#define FUART0_BASE_ADDR              0xFDD50000
#define FUART0_CLK_FREQ_HZ            24000000

#define FUART1_IRQ_NUM                149
#define FUART1_BASE_ADDR              0xFE650000
#define FUART1_CLK_FREQ_HZ            24000000

#define FUART2_IRQ_NUM                150
#define FUART2_BASE_ADDR              0xFE660000
#define FUART2_CLK_FREQ_HZ            24000000

/* GIC-500 (GICv3), distributor 0xFD400000, redistributor 0xFD460000 (0xC0000, 4 cores) */
#define FGIC_NUM                      1

#define GICV3_BASE_ADDR               0xFD400000
#define GICV3_DISTRIBUTOR_BASE_ADDR   0xFD400000
#define GICV3_RD_BASE_ADDR            0xFD460000
#define GICV3_RD_OFFSET               (2U << 16)
#define GICV3_RD_SIZE                 (12U << 16)

#define GICV3_ITS_BASE_ADDR           0xFD440000

/* generic timer (24MHz; rk3568.dtsi arch timer PPI 13/14/10/11
 * = INTID 29(sec-phys)/30(ns-phys)/26(hyp)/27(virt), 与 rk3568_virt 机器
 * GTIMER 接线一致; 旧值整体错位一位(29/30/27 实为 sec-phys/ns-phys/virt) */
#define GENERIC_PTIMER_EL1_IRQ_NUM    30U
#define GENERIC_VTIMER_EL1_IRQ_NUM    27U

#define GENERIC_VTIMER_EL2_NS_IRQ_NUM 28U
#define GENERIC_PTIMER_EL2_NS_IRQ_NUM 26U

#define GENERIC_TIMER_ID0             0 /* non-secure physical timer */
#define GENERIC_TIMER_ID1             1 /* virtual timer */
#define GENERIC_TIMER_NUM             2

/* PMU (PPI 7) */
#define FPMU_IRQ_NUM                  23

/* RK3568 system controllers (u-boot rk3568.c / dts confirmed) */
#define RK3568_GRF_BASE_ADDR          0xFDC60000
#define RK3568_CRU_BASE_ADDR          0xFDD20000
#define RK3568_PMUCRU_BASE_ADDR       0xFDD00000
#define RK3568_PMUGRF_BASE_ADDR       0xFDC20000
#define RK3568_PIPE_GRF_BASE_ADDR     0xFDC50000
#define RK3568_PIPE_PHY_GRF2_BASE_ADDR 0xFDC90000
#define RK3568_PCIE30_PHY_GRF_BASE_ADDR 0xFDCB8000

/* GPIO (DW-APB gpio, 0x100 per bank) */
#define FGPIO0_BASE_ADDR              0xFDD60000
#define FGPIO1_BASE_ADDR              0xFE740000
#define FGPIO2_BASE_ADDR              0xFE750000
#define FGPIO3_BASE_ADDR              0xFE760000
#define FGPIO4_BASE_ADDR              0xFE770000
#define FGPIO_NUM                     5U

/* USB3 OTG (DWC3 device mode) + USB2 PHY0, see SKILL §7.7. 中断号 = dts SPI + 32
 * (SDK 统一用 GIC INTID 约定, 同 uart2=150)。
 * 注意: 0xFD000000 是另一个 DWC3(USB3 Host, IRQ 170), 别混。
 * FreeRTOS host 模式下两个 DWC3 都可用作 host (OTG0 强制 host, 见
 * usb_rk3568_host_platform.c), 故 FUSB3_NUM = 2 (usbhost30=bus0, usbdrd30=bus1)。 */
#define FUSB3_ID                      0
#define FUSB3_NUM                     2

#define FUSB3_OTG_BASE_ADDR           0xFCC00000ULL /* usbdrd_dwc3, DWC3 core regs 绝对偏移 */
#define FUSB3_OTG_IRQ_NUM             201U         /* dts GIC_SPI 169 -> INTID 201 */

#define RK3568_USB3HOST_BASE_ADDR     0xFD000000U  /* usb3 host dwc3 (xHCI @ base + 0) */
#define RK3568_USB3HOST_IRQ_NUM       202U         /* dts GIC_SPI 170 -> INTID 202 */

#define RK3568_USB2PHY0_BASE_ADDR     0xFE8A0000U  /* usb2phy0 */
#define RK3568_USB2PHY0_GRF_BASE_ADDR 0xFDCA0000U  /* usb2phy0_grf (SYSCON, 写掩码) */
#define RK3568_USB2PHY0_IRQ_NUM       167U         /* dts GIC_SPI 135 -> INTID 167 */

/* USB2 host 控制器对 (EHCI+OHCI), 面板 USB2.0 Type-A 各经一个板载 CH334P hub。
 * usb_host0_* 挂 usb2phy1_otg 线, usb_host1_* 挂 usb2phy1_host 线。
 * NetBSD rk_usb2phy 实测: EHCI/OHCI 无专属时钟 ID(HCLK_USB2HOST0/1 ARB 门默认开),
 * UTMI 480m 来自 usb2phy1 GRF+0x008 bit4 输出使能。 */
#define RK3568_USB2HOST0_EHCI_BASE_ADDR 0xFD800000U /* dts GIC_SPI 130 -> INTID 162 */
#define RK3568_USB2HOST0_EHCI_IRQ_NUM   162U
#define RK3568_USB2HOST0_OHCI_BASE_ADDR 0xFD840000U /* dts GIC_SPI 131 -> INTID 163 */
#define RK3568_USB2HOST0_OHCI_IRQ_NUM   163U
#define RK3568_USB2HOST1_EHCI_BASE_ADDR 0xFD880000U /* dts GIC_SPI 133 -> INTID 165 */
#define RK3568_USB2HOST1_EHCI_IRQ_NUM   165U
#define RK3568_USB2HOST1_OHCI_BASE_ADDR 0xFD8C0000U /* dts GIC_SPI 134 -> INTID 166 */
#define RK3568_USB2HOST1_OHCI_IRQ_NUM   166U

#define RK3568_USB2PHY1_BASE_ADDR     0xFE8B0000U  /* usb2phy1 */
#define RK3568_USB2PHY1_GRF_BASE_ADDR 0xFDCA8000U  /* usb2phy1_grf (SYSCON, 写掩码) */

/* CRU 时钟门(值为 1 = 关, 写 0 = 开; 高 16 位 = write-enable)。
 * CRU 0xFDD20328 = clkgate_con[10]: bit0 ACLK_PIPE bit1 PCLK_PIPE
 *                                   bit8 ACLK_USB3OTG0 bit9 REF bit10 SUSPEND
 * PMUCRU 0xFDD00188 = pmu_clkgate_con[2]: bit0 clk_ref24m bit1 xin_osc0_usbphy0_g
 * (linux clk-rk3568.c clk.h 宏对齐) */
#define RK3568_CRU_CLKGATE_CON_USB3OTG0 0x328U
#define RK3568_CRU_CLKGATE_MASK_USB3OTG0 0x00070300U
#define RK3568_PMUCRU_CLKGATE_CON_USBPHY0 0x188U
#define RK3568_PMUCRU_CLKGATE_MASK_USBPHY0 0x00000003U

/* GMAC (Synopsys dwmac-4.20a) */
#define FDGMAC0_ID                    0
#define FDGMAC1_ID                    1
#define FDGMAC_NUM                    2

#define FDGMAC0_BASE_ADDR             0xFE2A0000
#define FDGMAC1_BASE_ADDR             0xFE010000

/* GIC INTID = SPI index + 32: gmac0 GIC_SPI 27 -> 59, gmac1 GIC_SPI 32 -> 64 */
#define FDGMAC0_IRQ_NUM               59
#define FDGMAC1_IRQ_NUM               64

#define FDGMAC_DMA_MIN_ALIGN          128
#define FDGMAC_MAX_PACKET_SIZE        1600

/* PCIe (DWC pcie3x2 via pcie30phy + pcie2x1 via combphy2), verified in u-boot bring-up.
 * INSTANCE0 = pcie3x2 (PCIe x4 槽), INSTANCE1 = pcie2x1 (M.2 22110 槽, combphy2). */
#define FPCIE_ECAM_INSTANCE_NUM       2
#define FPCIE_ECAM_INSTANCE0          0
#define FPCIE_ECAM_INSTANCE1          1

/* PCIe3x2 INTx legacy 中断: dts GIC_SPI 162(SPI 索引) = GIC INTID 194(TRM
 * Table1-3 pcie30x2_legacy)。SDK 中断号统一用 GIC INTID(uart2=150、PPI=29 同
 * 约定), 不能直接抄 dts 的 SPI 索引(差 32)。
 * 注: 2026-08-07 实测本板 Hikvision NVMe 不发送 Assert_INTA 链路消息
 * (端点 PCI_STATUS.INTERRUPT=1 但 DWC PCIE_CLIENT_INTR_STATUS_LEGACY bit0=0),
 * 即该盘不支持 legacy INTx 中断递送, INTx 模式暂不可用。 */
#define FPCIE_ECAM_INTA_IRQ_NUM       194
#define FPCIE_ECAM_INTB_IRQ_NUM       194
#define FPCIE_ECAM_INTC_IRQ_NUM       194
#define FPCIE_ECAM_INTD_IRQ_NUM       194

#define FPCI_ECAM_CONFIG_BASE_ADDR    0xF0000000
#define FPCI_ECAM_IO_CONFIG_BASE_ADDR 0xF0100000
#define FPCI_ECAM_IO_CONFIG_REG_LENGTH 0x100000
#define FPCI_ECAM_MEM32_BASE_ADDR     0xF0200000
#define FPCI_ECAM_MEM32_REG_LENGTH    0x1E00000
#define FPCI_ECAM_MEM64_BASE_ADDR     0x380000000ULL
#define FPCI_ECAM_MEM64_REG_LENGTH    0x40000000

/* pcie2x1 (INSTANCE1) outbound windows: cfg 0xF4000000/1M + io 0xF4100000/1M +
 * mem32 0xF4200000/30M + mem64 0x3_00000000/1G (rk3568.dtsi pcie2x1 ranges) */
#define FPCI_ECAM2_CONFIG_BASE_ADDR   0xF4000000
#define FPCI_ECAM2_IO_CONFIG_BASE_ADDR 0xF4100000
#define FPCI_ECAM2_IO_CONFIG_REG_LENGTH 0x100000
#define FPCI_ECAM2_MEM32_BASE_ADDR    0xF4200000
#define FPCI_ECAM2_MEM32_REG_LENGTH   0x1E00000
#define FPCI_ECAM2_MEM64_BASE_ADDR    0x300000000ULL
#define FPCI_ECAM2_MEM64_REG_LENGTH   0x40000000

#define FPCIE_ECAM_CFG_MAX_NUM_OF_BUS 0x20
#define FPCIE_ECAM_CFG_MAX_NUM_OF_DEV 32
#define FPCIE_ECAM_CFG_MAX_NUM_OF_FUN 8

/* DWC controller / phy registers (fdwpcie) */
#define RK3568_PCIE30X2_APB_BASE_ADDR    0xFE280000
#define RK3568_PCIE30X2_DBI_BASE_ADDR    0x3C0800000ULL
#define RK3568_PCIE30X2_LEGACY_IRQ_NUM   194
#define RK3568_PCIE30X2_PERST_GPIO_BASE  0xFE750000 /* GPIO2 */
#define RK3568_PCIE30X2_PERST_GPIO_BANK  3U         /* D */
#define RK3568_PCIE30X2_PERST_GPIO_PIN   (1U << 6)  /* GPIO2_PD6 */
#define RK3568_PCIE30PHY_BASE_ADDR       0xFE8C0000
#define RK3568_PCIE30PHY_SRAM_SIZE       0x20000

/* DWC pcie2x1 (M.2 22110 槽): APB 0xFE260000、DBI 0x3C0000000、PHY=combphy2
 * (0xFE840000)、PHY_GRF 0xFDC90000。PERST# = GPIO3_PC1 (active-high, linux dts)。 */
#define RK3568_PCIE20X1_APB_BASE_ADDR    0xFE260000
#define RK3568_PCIE20X1_DBI_BASE_ADDR    0x3C0000000ULL
#define RK3568_PCIE20X1_LEGACY_IRQ_NUM   104U /* dts GIC_SPI 72 (legacy) -> INTID 104 */
#define RK3568_PCIE20X1_PERST_GPIO_BASE  0xFE760000 /* GPIO3 */
#define RK3568_PCIE20X1_PERST_GPIO_BANK  2U         /* C */
#define RK3568_PCIE20X1_PERST_GPIO_PIN   (1U << 1)  /* GPIO3_PC1 */

/* CRU clock gates (clkgate_con) for pcie3x2 */
#define RK3568_CRU_CLKGATE_CON_PIPE      10U /* ACLK_PIPE bit0, PCLK_PIPE bit1 */
#define RK3568_CRU_CLKGATE_CON_PCIE30X2  13U /* MST bit0 SLV bit1 DBI bit2 PCLK bit3 AUX bit4 */
/* pcie2x1: clkgate_con[12] MST bit0 SLV bit1 DBI bit2 PCLK bit3 AUX bit4 */
#define RK3568_CRU_CLKGATE_CON_PCIE20    12U

/* SDMMC (DesignWare Mobile Storage Host, DW-MMC "dw-mshc"), TRM Part2 Ch6.
 * sdmmc0 = TF 卡槽, sdmmc1 = SDIO(板级未启用). eMMC(fe310000)是 SDHCI 接口,
 * 属另一个控制器(dwcmshc), 见 SKILL §11. */
#define FDWMMC0_ID                    0
#define FDWMMC1_ID                    1
#define FDWMMC_NUM                    2

#define FDWMMC0_BASE_ADDR             0xFE2B0000U
#define FDWMMC1_BASE_ADDR             0xFE2C0000U

/* GIC INTID = dts SPI 索引 + 32 (SDK 统一用 INTID 约定, 同 uart2=150) */
#define FDWMMC0_IRQ_NUM               130U /* dts GIC_SPI 98 */
#define FDWMMC1_IRQ_NUM               131U /* dts GIC_SPI 99 */

#define FDWMMC_CLK_FREQ_HZ            150000000U /* ciu max-frequency */

/* eMMC (Synopsys DWC MSHC, SDHCI 4.20a 接口, "dwcmshc"), TRM Part2 Ch7.
 * 与 sdmmc0/1(DW-MMC)是两类控制器, 由 fdwmshc 驱动. 板级 8bit/52MHz 限速. */
#define FDWMSHC0_ID                   0
#define FDWMSHC_NUM                   1

#define FDWMSHC0_BASE_ADDR            0xFE310000U

/* GIC INTID = dts SPI 索引 + 32 (SDK 统一用 INTID 约定, 同 uart2=150) */
#define FDWMSHC0_IRQ_NUM              51U /* dts GIC_SPI 19 */

#define FDWMSHC_CLK_FREQ_HZ           200000000U /* CCLK_EMMC 最大源 (HS200 200M 用) */

/* SFC/FSPI (SPI NOR flash 控制器), dts GIC_SPI 101 -> INTID 133 (TRM Part1 Ch27) */
#define FDSFC0_BASE_ADDR              0xFE300000UL
#define FDSFC0_IRQ_NUM                133U

/* I2C (Rockchip I2C v5, TRM Part1 Ch22). i2c0=0xFDD40000(PMU 域, SPI46->INTID78,
 * 板载 RK809 PMIC@0x20); i2c1=0xFE5A0000(CRU 域, SPI47->INTID79, 板载 RX8025
 * RTC@0x32 + INA3221 功率计@0x40)。clk_i2cx 输入标称 100MHz。 */
#define FDWI2C0_BASE_ADDR             0xFDD40000U
#define FDWI2C0_IRQ_NUM               78U /* dts GIC_SPI 46 */
#define FDWI2C1_BASE_ADDR             0xFE5A0000U
#define FDWI2C1_IRQ_NUM               79U /* dts GIC_SPI 47 */
#define FDWI2C_REF_CLK_HZ             100000000U

/* SATA (DWC AHCI, snps,dwc-ahci). SATA0=fc000000(立式座/原 bring-up 目标),
 * SATA1=fc400000(M.2 2280 槽, 2026-08-09 实测盘在此)。中断号 = dts GIC SPI + 32
 * (SATA0 SPI94→INTID126, SATA1 SPI95→INTID127; SDK 统一用 GIC INTID 约定)。 */
#define FSATA0_ID                       0
#define FSATA0_BASE_ADDR                0xFC000000U
#define FSATA0_IRQ_NUM                  126U
#define FSATA1_ID                       1
#define FSATA1_BASE_ADDR                0xFC400000U
#define FSATA1_IRQ_NUM                  127U
#define FSATA_NUM                       2

/* SATA combphy0/1 (naneng) + PHY_GRF0/1. 基址按 idx 递增 0x10000 */
#define RK3568_COMBPHY0_BASE_ADDR       0xFE820000U
#define RK3568_COMBPHY1_BASE_ADDR       0xFE830000U
#define RK3568_COMBPHY2_BASE_ADDR       0xFE840000U
#define RK3568_PIPE_PHY_GRF0_BASE_ADDR  0xFDC70000U
#define RK3568_PIPE_PHY_GRF1_BASE_ADDR  0xFDC80000U

/* PMU (power domain) */
#define RK3568_PMU_BASE_ADDR            0xFDD90000U

/* CRU SATA 相关 (hiword 写使能; gate 1=关 0=开; softrst assert=1 release=0) */
#define RK3568_CRU_CLKGATE_CON_PIPE     10U   /* CRU+0x328: bit0 ACLK_PIPE bit1 PCLK_PIPE bit2 ACLK_PIPE_BIU bit3 PCLK_PIPE_BIU bit7 PCLK_PIPE_GRF */
#define RK3568_CRU_CLKGATE_CON_SATA0    11U   /* CRU+0x32C: bit0 ACLK_SATA0 bit1 CLK_SATA0_PMALIVE bit2 CLK_SATA0_RXOOB bit3 CLK_SATA0_PIPE */
#define RK3568_CRU_CLKGATE_CON_PIPEPHY  34U   /* CRU+0x388: bit4 PCLK_PIPEPHY0 */
#define RK3568_CRU_SOFTRST_CON_SATA0    8U    /* CRU+0x420: bit6-9 (aresetn_sata0/resetn_sata0_pipe/pmalive/rxoob) */
#define RK3568_CRU_SOFTRST_CON_PIPEPHY  28U   /* CRU+0x470: bit4 presetn_pipephy0(apb) bit5 resetn_pipephy0 */
#define RK3568_CRU_CLKSEL_CON29         0x174U /* aclk_pipe mux [1:0]: 0=gpll_400m 1=gpll_300m 2=gpll_200m 3=xin24m */

/* PMU PD_PIPE 电源域 (RK3568_PMU_BASE_ADDR + 偏移) */
#define RK3568_PMU_PWR_GATE_SFTCON      0xA0U  /* bit8=pd_pipe_dwn_ena: 0=上电 1=下电 (hiword 写使能) */
#define RK3568_PMU_PWR_DWN_ST           0x98U  /* bit8=pd_pipe_dwn_stat: 0=上电完成 1=断电中 */
#define RK3568_PMU_PD_PIPE_BIT          (1U << 8)

#ifdef __cplusplus
}
#endif

#endif // RK3568_FPARAMETERS_H
