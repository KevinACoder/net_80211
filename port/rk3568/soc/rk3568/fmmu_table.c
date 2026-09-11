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
 * FilePath: fmmu_table.c
 * Date: 2026-08-06
 * Description: RK3568 mmu table: map device space for gic/uart/cru/grf
 */
#include "fmmu.h"
#include "ftypes.h"
#include "fparameters.h"

const struct ArmMmuRegion mmu_regions[] = {

    /* SATA0/1 (DWC AHCI) @ 0xFC000000/0xFC400000, 共 8MB (SATA1 = M.2 2280 槽,
     * 2026-08-09 实测盘在此)。0xFE820000 combphy0 已被 DEVICE_REGION 覆盖。 */
    MMU_REGION_FLAT_ENTRY("DEVICE_SATA", 0xFC000000, (8 * 1024 * 1024),
                          MT_DEVICE_NGNRNE | MT_P_RW_U_NA | MT_NS),

    /* DWC3 USB3 OTG (usbdrd_dwc3) @ 0xFCC00000/4M, IRQ 201. 在默认 DEVICE_REGION
     * (0xFD000000 起)之外, 必须单独映射, 否则访问挂死 (SKILL §7.7) */
    MMU_REGION_FLAT_ENTRY("DEVICE_USB_DWC3", 0xFCC00000, (4 * 1024 * 1024),
                          MT_DEVICE_NGNRNE | MT_P_RW_U_NA | MT_NS),

    MMU_REGION_FLAT_ENTRY("DEVICE_REGION", 0xFD000000, (48 * 1024 * 1024),
                          MT_DEVICE_NGNRNE | MT_P_RW_U_NA | MT_NS),

    /* PCIe pcie3x2 windows: cfg 0xF0000000/1MB + io 0xF0100000/1MB + mem32 0xF0200000/30MB */
    MMU_REGION_FLAT_ENTRY("DEVICE_PCIE_WINDOW", 0xF0000000, (32 * 1024 * 1024),
                          MT_DEVICE_NGNRNE | MT_P_RW_U_NA | MT_NS),

    /* PCIe pcie2x1 windows: cfg 0xF4000000/1MB + io 0xF4100000/1MB + mem32 0xF4200000/30MB */
    MMU_REGION_FLAT_ENTRY("DEVICE_PCIE2_WINDOW", 0xF4000000, (32 * 1024 * 1024),
                          MT_DEVICE_NGNRNE | MT_P_RW_U_NA | MT_NS),

    /* PCIe pcie3x2 DBI (AXI slave window above 4GB) */
    MMU_REGION_FLAT_ENTRY("DEVICE_PCIE_DBI", 0x3C0800000, (4 * 1024 * 1024),
                          MT_DEVICE_NGNRNE | MT_P_RW_U_NA | MT_NS),

    /* PCIe pcie2x1 DBI (AXI slave window above 4GB) */
    MMU_REGION_FLAT_ENTRY("DEVICE_PCIE2_DBI", 0x3C0000000, (4 * 1024 * 1024),
                          MT_DEVICE_NGNRNE | MT_P_RW_U_NA | MT_NS),

};

const struct ArmMmuConfig mmu_config = {
    .num_regions = ARRAY_SIZE(mmu_regions),
    .mmu_regions = mmu_regions,
};
