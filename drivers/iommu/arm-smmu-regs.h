/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * IOMMU API for ARM architected SMMU implementations.
 *
 * Copyright (C) 2013 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#ifndef _ARM_SMMU_REGS_H
#define _ARM_SMMU_REGS_H

#include <linux/bits.h>

/* Configuration registers */
#define ARM_SMMU_GR0_sCR0		0x0
#define sCR0_SHCFG_SHIFT		22
#define sCR0_SHCFG_MASK			0x3
#define sCR0_SHCFG_NSH			3
#define sCR0_VMID16EN			BIT(31)
#define sCR0_BSU			GENMASK(15, 14)
#define sCR0_FB				BIT(13)
#define sCR0_PTM			BIT(12)
#define sCR0_VMIDPNE			BIT(11)
#define sCR0_USFCFG			BIT(10)
#define sCR0_GCFGFIE			BIT(5)
#define sCR0_GCFGFRE			BIT(4)
#define sCR0_EXIDENABLE			BIT(3)
#define sCR0_GFIE			BIT(2)
#define sCR0_GFRE			BIT(1)
#define sCR0_CLIENTPD			BIT(0)

/* Auxiliary Configuration register */
#define ARM_SMMU_GR0_sACR		0x10

/* Identification registers */
#define ARM_SMMU_GR0_ID0		0x20
#define ID0_S1TS			BIT(30)
#define ID0_S2TS			BIT(29)
#define ID0_NTS				BIT(28)
#define ID0_SMS				BIT(27)
#define ID0_ATOSNS			BIT(26)
#define ID0_PTFS_NO_AARCH32		BIT(25)
#define ID0_PTFS_NO_AARCH32S		BIT(24)
#define ID0_NUMIRPT			GENMASK(23, 16)
#define ID0_CTTW			BIT(14)
#define ID0_NUMSIDB			GENMASK(12, 9)
#define ID0_EXIDS			BIT(8)
#define ID0_NUMSMRG			GENMASK(7, 0)

#define ARM_SMMU_GR0_ID1		0x24
#define ID1_PAGESIZE			BIT(31)
#define ID1_NUMPAGENDXB			GENMASK(30, 28)
#define ID1_NUMS2CB			GENMASK(23, 16)
#define ID1_NUMCB			GENMASK(7, 0)

#define ARM_SMMU_GR0_ID2		0x28
#define ID2_VMID16			BIT(15)
#define ID2_PTFS_64K			BIT(14)
#define ID2_PTFS_16K			BIT(13)
#define ID2_PTFS_4K			BIT(12)
#define ID2_UBS				GENMASK(11, 8)
#define ID2_OAS				GENMASK(7, 4)
#define ID2_IAS				GENMASK(3, 0)

#define ARM_SMMU_GR0_ID3		0x2c
#define ARM_SMMU_GR0_ID4		0x30
#define ARM_SMMU_GR0_ID5		0x34
#define ARM_SMMU_GR0_ID6		0x38

#define ARM_SMMU_GR0_ID7		0x3c
#define ID7_MAJOR			GENMASK(7, 4)
#define ID7_MINOR			GENMASK(3, 0)

#define ARM_SMMU_GR0_sGFSR		0x48
#define ARM_SMMU_GR0_sGFSYNR0		0x50
#define ARM_SMMU_GR0_sGFSYNR1		0x54
#define ARM_SMMU_GR0_sGFSYNR2		0x58

/* Global TLB invalidation */
#define ARM_SMMU_GR0_TLBIVMID		0x64
#define ARM_SMMU_GR0_TLBIALLNSNH	0x68
#define ARM_SMMU_GR0_TLBIALLH		0x6c
#define ARM_SMMU_GR0_sTLBGSYNC		0x70

#define ARM_SMMU_GR0_sTLBGSTATUS	0x74
#define sTLBGSTATUS_GSACTIVE		BIT(0)

/* Stream mapping registers */
#define ARM_SMMU_GR0_SMR(n)		(0x800 + ((n) << 2))
#define SMR_MASK_MASK			0x7FFF
#define SID_MASK			0x7FFF
#define SMR_VALID			BIT(31)
#define SMR_MASK			GENMASK(31, 16)
#define SMR_ID				GENMASK(15, 0)

#define ARM_SMMU_GR0_S2CR(n)		(0xc00 + ((n) << 2))
#define S2CR_SHCFG_SHIFT		8
#define S2CR_SHCFG_MASK			0x3
#define S2CR_SHCFG_NSH			0x3

#define S2CR_PRIVCFG			GENMASK(25, 24)
enum arm_smmu_s2cr_privcfg {
	S2CR_PRIVCFG_DEFAULT,
	S2CR_PRIVCFG_DIPAN,
	S2CR_PRIVCFG_UNPRIV,
	S2CR_PRIVCFG_PRIV,
};
#define S2CR_TYPE			GENMASK(17, 16)
enum arm_smmu_s2cr_type {
	S2CR_TYPE_TRANS,
	S2CR_TYPE_BYPASS,
	S2CR_TYPE_FAULT,
};
#define S2CR_EXIDVALID			BIT(10)
#define S2CR_CBNDX			GENMASK(7, 0)

/* Context bank attribute registers */
#define ARM_SMMU_GR1_CBAR(n)		(0x0 + ((n) << 2))
#define CBAR_VMID_SHIFT			0
#define CBAR_VMID_MASK			0xff
#define CBAR_S1_BPSHCFG_SHIFT		8
#define CBAR_S1_BPSHCFG_MASK		3
#define CBAR_S1_BPSHCFG_NSH		3
#define CBAR_S1_MEMATTR_SHIFT		12
#define CBAR_S1_MEMATTR_MASK		0xf
#define CBAR_S1_MEMATTR_WB		0xf
#define CBAR_TYPE_SHIFT			16
#define CBAR_TYPE_MASK			0x3
#define CBAR_TYPE_S2_TRANS		(0 << CBAR_TYPE_SHIFT)
#define CBAR_TYPE_S1_TRANS_S2_BYPASS	(1 << CBAR_TYPE_SHIFT)
#define CBAR_TYPE_S1_TRANS_S2_FAULT	(2 << CBAR_TYPE_SHIFT)
#define CBAR_TYPE_S1_TRANS_S2_TRANS	(3 << CBAR_TYPE_SHIFT)
#define CBAR_IRPTNDX_SHIFT		24
#define CBAR_IRPTNDX_MASK		0xff

#define ARM_SMMU_GR1_CBFRSYNRA(n)	(0x400 + ((n) << 2))
#define CBFRSYNRA_SID_MASK		(0xffff)

#define ARM_SMMU_GR1_CBA2R(n)		(0x800 + ((n) << 2))
#define CBA2R_RW64_32BIT		(0 << 0)
#define CBA2R_RW64_64BIT		(1 << 0)
#define CBA2R_VMID_SHIFT		16
#define CBA2R_VMID_MASK			0xffff

#define ARM_SMMU_CB_SCTLR		0x0
#define ARM_SMMU_CB_ACTLR		0x4
#define ARM_SMMU_CB_RESUME		0x8
#define ARM_SMMU_CB_TTBCR2		0x10
#define ARM_SMMU_CB_TTBR0		0x20
#define ARM_SMMU_CB_TTBR1		0x28
#define ARM_SMMU_CB_TTBCR		0x30
#define ARM_SMMU_CB_CONTEXTIDR		0x34
#define ARM_SMMU_CB_S1_MAIR0		0x38
#define ARM_SMMU_CB_S1_MAIR1		0x3c
#define ARM_SMMU_CB_PAR			0x50
#define ARM_SMMU_CB_FSR			0x58
#define ARM_SMMU_CB_FSRRESTORE		0x5c
#define ARM_SMMU_CB_FAR			0x60
#define ARM_SMMU_CB_FSYNR0		0x68
#define ARM_SMMU_CB_FSYNR1		0x6c
#define ARM_SMMU_CB_S1_TLBIVA		0x600
#define ARM_SMMU_CB_S1_TLBIASID		0x610
#define ARM_SMMU_CB_S1_TLBIALL		0x618
#define ARM_SMMU_CB_S1_TLBIVAL		0x620
#define ARM_SMMU_CB_S2_TLBIIPAS2	0x630
#define ARM_SMMU_CB_S2_TLBIIPAS2L	0x638
#define ARM_SMMU_CB_TLBSYNC		0x7f0
#define ARM_SMMU_CB_TLBSTATUS		0x7f4
#define TLBSTATUS_SACTIVE		(1 << 0)
#define ARM_SMMU_CB_ATS1PR		0x800
#define ARM_SMMU_CB_ATSR		0x8f0
#define ARM_SMMU_STATS_SYNC_INV_TBU_ACK 0x25dc
#define ARM_SMMU_TBU_PWR_STATUS         0x2204
#define ARM_SMMU_MMU2QSS_AND_SAFE_WAIT_CNTR 0x2670

#define SCTLR_MEM_ATTR_SHIFT		16
#define SCTLR_SHCFG_SHIFT		22
#define SCTLR_RACFG_SHIFT		24
#define SCTLR_WACFG_SHIFT		26
#define SCTLR_SHCFG_MASK		0x3
#define SCTLR_SHCFG_NSH			0x3
#define SCTLR_RACFG_RA			0x2
#define SCTLR_WACFG_WA			0x2
#define SCTLR_MEM_ATTR_OISH_WB_CACHE	0xf
#define SCTLR_MTCFG			(1 << 20)
#define SCTLR_S1_ASIDPNE		(1 << 12)
#define SCTLR_CFCFG			(1 << 7)
#define SCTLR_HUPCF			(1 << 8)
#define SCTLR_CFIE			(1 << 6)
#define SCTLR_CFRE			(1 << 5)
#define SCTLR_E				(1 << 4)
#define SCTLR_AFE			(1 << 2)
#define SCTLR_TRE			(1 << 1)
#define SCTLR_M				(1 << 0)

#define CB_PAR_F			(1 << 0)

#define ATSR_ACTIVE			(1 << 0)

#define RESUME_RETRY			(0 << 0)
#define RESUME_TERMINATE		(1 << 0)

#define TTBCR2_SEP_SHIFT		15
#define TTBCR2_SEP_UPSTREAM		(0x7 << TTBCR2_SEP_SHIFT)
#define TTBCR2_AS			(1 << 4)

#define TTBRn_ASID_SHIFT		48

#define FSR_MULTI			(1 << 31)
#define FSR_SS				(1 << 30)
#define FSR_UUT				(1 << 8)
#define FSR_ASF				(1 << 7)
#define FSR_TLBLKF			(1 << 6)
#define FSR_TLBMCF			(1 << 5)
#define FSR_EF				(1 << 4)
#define FSR_PF				(1 << 3)
#define FSR_AFF				(1 << 2)
#define FSR_TF				(1 << 1)

#define FSR_IGN				(FSR_AFF | FSR_ASF | \
					 FSR_TLBMCF | FSR_TLBLKF)
#define FSR_FAULT			(FSR_MULTI | FSR_SS | FSR_UUT | \
					 FSR_EF | FSR_PF | FSR_TF | FSR_IGN)

#define FSYNR0_WNR			(1 << 4)

#endif /* _ARM_SMMU_REGS_H */
