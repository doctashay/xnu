/*
 * Copyright (c) 2004-2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#include <ppc/machine_routines.h>
#include <ppc/machine_cpu.h>
#include <ppc/exception.h>
#include <ppc/misc_protos.h>
#include <ppc/Firmware.h>
#include <ppc/pmap.h>
#include <ppc/asm.h>
#include <ppc/proc_reg.h>
#include <kern/pms.h>
#include <ppc/savearea.h>
#include <ppc/Diagnostics.h>
#include <kern/processor.h>
#include <string.h>


static void pmsCPURemote(uint32_t nstep);
static void pmsSetPowerBook(uint32_t sel, uint32_t cpu, uint32_t platformData);
static uint32_t pmsQueryPowerBook(uint32_t cpu, uint32_t platformData);
static void pmsSetM23(uint32_t sel, uint32_t cpu, uint32_t platformData);
static uint32_t pmsQueryM23(uint32_t cpu, uint32_t platformData);

extern unsigned char sysInfo[0x58];

#define SYSINFO_U32(offset) (*(uint32_t *)(void *)(sysInfo + (offset)))

pmsDef hwM23Step[] = {
	{ .pmsLimit = century, .pmsStepID = 0x0, .pmsSetCmd = 0x00810000, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x0, .pmsNext = 0x1, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x1388, .pmsStepID = 0x1, .pmsSetCmd = 0x00000001, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x0, .pmsNext = 0x2, .pmsTDelay = 0x0 },
	{ .pmsLimit = century, .pmsStepID = 0x2, .pmsSetCmd = 0x00800003, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x0, .pmsNext = 0x2, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x03E8, .pmsStepID = 0x3, .pmsSetCmd = 0x00800004, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x0, .pmsNext = 0x2, .pmsTDelay = 0x0 },
	{ .pmsLimit = century, .pmsStepID = 0x4, .pmsSetCmd = 0x00810006, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x4, .pmsNext = 0x4, .pmsTDelay = 0x0 },
	{ .pmsLimit = century, .pmsStepID = 0x5, .pmsSetCmd = 0x00800007, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x5, .pmsNext = 0x5, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x0, .pmsStepID = 0x6, .pmsSetCmd = 0x00C00007, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0xFFFFFFFFU, .pmsNext = 0xFFFFFFFFU, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x0, .pmsStepID = 0x7, .pmsSetCmd = 0x00C00007, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0xFFFFFFFFU, .pmsNext = 0xFFFFFFFFU, .pmsTDelay = 0x0 },
	{ .pmsLimit = century, .pmsStepID = 0x8, .pmsSetCmd = 0x00810006, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x8, .pmsNext = 0x8, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x0, .pmsStepID = 0x9, .pmsSetCmd = 0x00810000, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x0, .pmsNext = 0x1, .pmsTDelay = 0x0 },
};

pmsDef hwpmsStep[] = {
	{ .pmsLimit = 0x0, .pmsStepID = 0x0, .pmsSetCmd = 0x81400000, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x15, .pmsNext = 0x15, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x11F8, .pmsStepID = 0x1, .pmsSetCmd = 0x00000001, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x0, .pmsNext = 0x0A, .pmsTDelay = 0x0 },
	{ .pmsLimit = century, .pmsStepID = 0x2, .pmsSetCmd = 0x00800003, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x1A, .pmsNext = 0x2, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x03E8, .pmsStepID = 0x3, .pmsSetCmd = 0x00800004, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x0, .pmsNext = 0x2, .pmsTDelay = 0x0 },
	{ .pmsLimit = century, .pmsStepID = 0x4, .pmsSetCmd = 0x00818006, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x4, .pmsNext = 0x4, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x0190, .pmsStepID = 0x5, .pmsSetCmd = 0x408107, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x0D, .pmsNext = 0x0D, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x0, .pmsStepID = 0x6, .pmsSetCmd = 0x408107, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x0E, .pmsNext = 0x0E, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x0, .pmsStepID = 0x7, .pmsSetCmd = 0x408107, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x0F, .pmsNext = 0x0F, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x11F8, .pmsStepID = 0x8, .pmsSetCmd = 0x00828000, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x10, .pmsNext = 0x13, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x0, .pmsStepID = 0x9, .pmsSetCmd = 0x81400000, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x16, .pmsNext = 0x16, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x0190, .pmsStepID = 0x0A, .pmsSetCmd = 0x00008102, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x17, .pmsNext = 0x0B, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x2580, .pmsStepID = 0x0B, .pmsSetCmd = 0x80810002, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x18, .pmsNext = 0x0C, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x0190, .pmsStepID = 0x0C, .pmsSetCmd = 0x00008102, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x19, .pmsNext = 0x02, .pmsTDelay = 0x0 },
	{ .pmsLimit = century, .pmsStepID = 0x0D, .pmsSetCmd = 0x80800007, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x0D, .pmsNext = 0x0D, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x0, .pmsStepID = 0x0E, .pmsSetCmd = 0x80C00007, .sf.pmsSetFuncInd = 0x0, .pmsDown = 0xFFFFFFFFU, .pmsNext = 0xFFFFFFFFU, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x0, .pmsStepID = 0x0F, .pmsSetCmd = 0x80C00007, .sf.pmsSetFuncInd = 0x0, .pmsDown = 0xFFFFFFFFU, .pmsNext = 0xFFFFFFFFU, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x0, .pmsStepID = 0x10, .pmsSetCmd = 0x81400000, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x11, .pmsNext = 0x11, .pmsTDelay = 0x0 },
	{ .pmsLimit = century, .pmsStepID = 0x11, .pmsSetCmd = 0x00828000, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x11, .pmsNext = 0x12, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x11F8, .pmsStepID = 0x12, .pmsSetCmd = 0x00000001, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x11, .pmsNext = 0x13, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x0190, .pmsStepID = 0x13, .pmsSetCmd = 0x00008102, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x11, .pmsNext = 0x14, .pmsTDelay = 0x0 },
	{ .pmsLimit = century, .pmsStepID = 0x14, .pmsSetCmd = 0x80810003, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x10, .pmsNext = 0x14, .pmsTDelay = 0x0 },
	{ .pmsLimit = century, .pmsStepID = 0x15, .pmsSetCmd = 0x00828000, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x0, .pmsNext = 0x1, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x11F8, .pmsStepID = 0x16, .pmsSetCmd = 0x00828000, .sf.pmsSetFuncInd = 0x1, .pmsDown = 0x0, .pmsNext = 0x0A, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x01F4, .pmsStepID = 0x17, .pmsSetCmd = pmsDelay, .sf.pmsSetFuncInd = 0x0, .pmsDown = 0x0, .pmsNext = 0x0A, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x01F4, .pmsStepID = 0x18, .pmsSetCmd = pmsDelay, .sf.pmsSetFuncInd = 0x0, .pmsDown = 0x0, .pmsNext = 0x0B, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x01F4, .pmsStepID = 0x19, .pmsSetCmd = pmsDelay, .sf.pmsSetFuncInd = 0x0, .pmsDown = 0x0, .pmsNext = 0x0C, .pmsTDelay = 0x0 },
	{ .pmsLimit = 0x01F4, .pmsStepID = 0x1A, .pmsSetCmd = pmsDelay, .sf.pmsSetFuncInd = 0x0, .pmsDown = 0x0, .pmsNext = 0x02, .pmsTDelay = 0x0 },
};

static uint32_t pmsM23State;

static void
pmsSetPowerBook(uint32_t sel, uint32_t cpu, uint32_t platformData)
{
	volatile uint32_t *pb_ctrl;
	volatile uint8_t *pb_regs;
	uint32_t lo;

	(void)cpu;
	(void)platformData;

	if ((int32_t)sel < 0) {
		pb_ctrl = (volatile uint32_t *)(uintptr_t)SYSINFO_U32(0x24);
		if (pb_ctrl != NULL) {
			pb_ctrl[0x100 / sizeof(*pb_ctrl)] = ((sel << 8) & 0x7F);
			__asm__ volatile("eieio");
		}
	}

	pb_regs = (volatile uint8_t *)(uintptr_t)SYSINFO_U32(0x2C);
	if (pb_regs == NULL) {
		return;
	}

	if (sel & 0x00008000U) {
		pb_regs[0x6B] = (uint8_t)((((sel >> 8) & 1U)) | 4U);
		__asm__ volatile("eieio");
	}

	lo = sel & 0xFFU;
	pb_regs[0x63] = (uint8_t)((((lo >> 1) & 1U)) | 4U);
	pb_regs[0x64] = (uint8_t)(((lo & 1U)) | 4U);
	pb_regs[0x65] = (uint8_t)(((sel & 1U)) | 4U);
	__asm__ volatile("eieio");
}

static uint32_t
pmsQueryPowerBook(uint32_t cpu, uint32_t platformData)
{
	volatile uint8_t *pb_regs;
	uint32_t bit0;
	uint32_t result;

	(void)cpu;
	(void)platformData;

	pb_regs = (volatile uint8_t *)(uintptr_t)SYSINFO_U32(0x2C);
	if (pb_regs == NULL) {
		return 0;
	}

	bit0 = pb_regs[0x64] & 1U;
	result = ((pb_regs[0x63] & 1U) << 2);
	result |= (bit0 << 1);
	result |= bit0;
	result |= ((pb_regs[0x6B] & 1U) << 8);
	return result;
}

static void
pmsSetM23(uint32_t sel, uint32_t cpu, uint32_t platformData)
{
	(void)cpu;
	(void)platformData;
	pmsM23State = sel & 0xFFU;
}

static uint32_t
pmsQueryM23(uint32_t cpu, uint32_t platformData)
{
	(void)cpu;
	(void)platformData;
	return pmsM23State;
}


pmsDef pmsDefault[] = {
	{
		.pmsLimit = century,							/* We can normally stay here for 100 years */
		.pmsStepID = pmsIdle,							/* Unique identifier to this step */
		.pmsSetCmd = 0,									/* Dummy platform power level */
		.sf.pmsSetFuncInd = 0,							/* Dummy platform set function */
		.pmsDown = pmsIdle,								/* We stay here */
		.pmsNext = pmsNorm								/* Next step */
	},
	{
		.pmsLimit = century,							/* We can normally stay here for 100 years */
		.pmsStepID = pmsNorm,							/* Unique identifier to this step */
		.pmsSetCmd = 0,									/* Dummy platform power level */
		.sf.pmsSetFuncInd = 0,							/* Dummy platform set function */
		.pmsDown = pmsIdle,								/* Down to idle */
		.pmsNext = pmsNorm								/* Next step */
	},
	{
		.pmsLimit = century,							/* We can normally stay here for 100 years */
		.pmsStepID = pmsNormHigh,						/* Unique identifier to this step */
		.pmsSetCmd = 0,									/* Dummy platform power level */
		.sf.pmsSetFuncInd = 0,							/* Dummy platform set function */
		.pmsDown = pmsIdle,								/* Down to idle */
		.pmsNext = pmsNormHigh							/* Next step */
	},
	{
		.pmsLimit = century,							/* We can normally stay here for 100 years */
		.pmsStepID = pmsBoost,							/* Unique identifier to this step */
		.pmsSetCmd = 0,									/* Dummy platform power level */
		.sf.pmsSetFuncInd = 0,							/* Dummy platform set function */
		.pmsDown = pmsIdle,								/* Step down */
		.pmsNext = pmsBoost								/* Next step */
	},	
	{	
		.pmsLimit = century,							/* We can normally stay here for 100 years */
		.pmsStepID = pmsLow,							/* Unique identifier to this step */
		.pmsSetCmd = 0,									/* Dummy platform power level */
		.sf.pmsSetFuncInd = 0,							/* Dummy platform set function */
		.pmsDown = pmsLow,								/* We always stay here */
		.pmsNext = pmsLow								/* We always stay here */
	},	
	{	
		.pmsLimit = century,							/* We can normally stay here for 100 years */
		.pmsStepID = pmsHigh,							/* Unique identifier to this step */
		.pmsSetCmd = 0,									/* Dummy platform power level */
		.sf.pmsSetFuncInd = 0,							/* Dummy platform set function */
		.pmsDown = pmsHigh,								/* We always stay here */
		.pmsNext = pmsHigh								/* We always stay here */
	},	
	{	
		.pmsLimit = 0,									/* Time doesn't matter for a prepare for change */
		.pmsStepID = pmsPrepCng,						/* Unique identifier to this step */
		.pmsSetCmd = pmsParkIt,							/* Force us to be parked */
		.sf.pmsSetFuncInd = 0,							/* Dummy platform set function */
		.pmsDown = pmsPrepCng,							/* We always stay here */
		.pmsNext = pmsPrepCng							/* We always stay here */
	},	
	{	
		.pmsLimit = 0,									/* Time doesn't matter for a prepare for sleep */
		.pmsStepID = pmsPrepSleep,						/* Unique identifier to this step */
		.pmsSetCmd = pmsParkIt,							/* Force us to be parked */
		.sf.pmsSetFuncInd = 0,							/* Dummy platform set function */
		.pmsDown = pmsPrepSleep,						/* We always stay here */
		.pmsNext = pmsPrepSleep							/* We always stay here */
	},	
	{	
		.pmsLimit = 0,									/* Time doesn't matter for a prepare for sleep */
		.pmsStepID = pmsOverTemp,						/* Unique identifier to this step */
		.pmsSetCmd = 0,									/* Dummy platform power level */
		.sf.pmsSetFuncInd = 0,							/* Dummy platform set function */
		.pmsDown = pmsOverTemp,							/* We always stay here */
		.pmsNext = pmsOverTemp							/* We always stay here */
	}	
};



/*
 *	This is where the CPU part of the stepper code lives.   
 *
 *	It also contains the "hacked kext" experimental code.  This is/was used for
 *	experimentation and bringup.  It should neither live long nor prosper.
 *
 */

/*
 *	Set the processor frequency and stuff
 */

void pmsCPUSet(uint32_t sel) {
	int nfreq;
	struct per_proc_info *pp;

	pp = getPerProc();									/* Get our per_proc */

	if(!((sel ^ pp->pms.pmsCSetCmd) & pmsCPU)) return;	/* If there aren't any changes, bail now... */

	nfreq = (sel & pmsCPU) >> 16;						/* Isolate the new frequency */
	
	switch(pp->pf.pfPowerModes & pmType) {				/* Figure out what type to do */
	
		case pmDFS:										/* This is a DFS machine */
			ml_set_processor_speed_dfs(nfreq);			/* Yes, set it */
			break;
	
		case pmDualPLL:
			ml_set_processor_speed_dpll(nfreq);			/* THIS IS COMPLETELY UNTESTED!!! */
			break;

		case pmPowerTune:								/* This is a PowerTune machine */
			ml_set_processor_speed_powertune(nfreq);	/* Diddle the deal */
			break;
			
		default:										/* Not this time dolt!!! */
			panic("pmsCPUSet: unsupported power manager type: %08X\n", pp->pf.pfPowerModes);
			break;
	
	}
	
}

/*
 *	This code configures the initial step tables.  It should be called after the timebase frequency is initialized.
 */

void pmsCPUConf(void) {

	int i;
	kern_return_t ret;
	pmsSetFunc_t pmsDfltFunc[pmsSetFuncMax];			/* List of functions for the external power control to use */

	for(i = 0; i < pmsSetFuncMax; i++) pmsDfltFunc[i] = NULL;	/* Clear this */


	if ((memcmp(sysInfo, "PowerBook", 9) == 0) && (SYSINFO_U32(0x20) == 0x000000D2U)) {
		pmsDfltFunc[1] = pmsSetPowerBook;
		ret = pmsBuild((pmsDef *)&hwpmsStep, sizeof(hwpmsStep), pmsDfltFunc, 0, pmsQueryPowerBook);
		if (ret != KERN_SUCCESS) {
			panic("pmsCPUConf: hwpmsStep build failed, ret = %08X\n", ret);
		}
	} else if ((memcmp(sysInfo, "PowerMac8,2", 11) == 0) && (SYSINFO_U32(0x20) == 0x00000039U)) {
		pmsDfltFunc[1] = pmsSetM23;
		ret = pmsBuild((pmsDef *)&hwM23Step, sizeof(hwM23Step), pmsDfltFunc, 0, pmsQueryM23);
		if (ret != KERN_SUCCESS) {
			panic("pmsCPUConf: hwM23Step build failed, ret = %08X\n", ret);
		}
	} else {
		ret = pmsBuild((pmsDef *)&pmsDefault, sizeof(pmsDefault), pmsDfltFunc, 0, (pmsQueryFunc_t)0);	/* Configure the default stepper */
		if(ret != KERN_SUCCESS) {							/* Some screw up? */
			panic("pmsCPUConf: initial stepper table build failed, ret = %08X\n", ret);	/* Squeal */
		}
	}
	
	pmsSetStep(pmsHigh, 1);								/* Slew to high speed */
	pmsPark();											/* Then park */
	return;
}

/*
 * Machine-dependent initialization
 */
void
pmsCPUMachineInit(void)
{
	return;
}

/*
 *	This function should be called once for each processor to force the
 *	processor to the correct voltage and frequency.
 */
 
void pmsCPUInit(void) {

	int cpu;

	cpu = cpu_number();									/* Who are we? */
	
	kprintf("************ Initializing stepper hardware, cpu %d ******************\n", cpu);	/* (BRINGUP) */
	
	pmsSetStep(pmsHigh, 1);								/* Slew to high speed */
	pmsPark();											/* Then park */

	kprintf("************ Stepper hardware initialized, cpu %d ******************\n", cpu);	/* (BRINGUP) */
}

extern uint32_t hid1get(void);

uint32_t
pmsCPUQuery(void)
{
	uint32_t result;
	struct per_proc_info *pp;
	uint64_t scdata;

	pp = getPerProc();									/* Get our per_proc */

	switch(pp->pf.pfPowerModes & pmType) {				/* Figure out what type to do */
	
		case pmDFS:										/* This is a DFS machine */
			result = hid1get();							/* Get HID1 */
			result = (result >> 6) & 0x00030000;		/* Isolate the DFS bits */
			break;
			
		case pmPowerTune:								/* This is a PowerTune machine */		
			(void)ml_scom_read(PowerTuneStatusReg, &scdata);	/* Get the current power level */
			result = (scdata >> (32 + 8)) & 0x00030000;	/* Shift the data to align with the set command */
			break;
			
		default:										/* Query not supported for this kind */
			result = 0;									/* Return highest if not supported */
			break;
	
	}

	return result;
}

/*
 *	These are not implemented for PPC.
 */
void pmsCPUYellowFlag(void) {
}

void pmsCPUGreenFlag(void) {
}

uint32_t pmsCPUPackageQuery(void)
{
    	/* multi-core CPUs are not supported. */
    	return(~(uint32_t)0);
}

/*
 *	Broadcast a change to all processors including ourselves.
 *	This must transition before broadcasting because we may block and end up on a different processor.
 *
 *	This will block until all processors have transitioned, so
 *	obviously, this can block.
 *
 *	Called with interruptions disabled.
 *
 */
 
void pmsCPURun(uint32_t nstep) {

	pmsRunLocal(nstep);								/* If we aren't parking (we are already parked), transition ourselves */
	(void)cpu_broadcast(&pmsBroadcastWait, pmsCPURemote, nstep);	/* Tell everyone else to do it too */

	return;
	
}

/*
 *	Receive a broadcast and react.
 *	This is called from the interprocessor signal handler.
 *	We wake up the initiator after we are finished.
 *
 */
	
static void pmsCPURemote(uint32_t nstep) {

	pmsRunLocal(nstep);								/* Go set the step */
	if(!hw_atomic_sub(&pmsBroadcastWait, 1)) {		/* Drop the wait count */
		thread_wakeup((event_t)&pmsBroadcastWait);	/* If we were the last, wake up the signaller */
	}
	return;
}	

/*
 *	Control the Power Management Stepper.
 *	Called from user state by the superuser via a ppc system call.
 *	Interruptions disabled.
 *
 */
int pmsCntrl(struct savearea *save) {
	save->save_r3 = pmsControl(save->save_r3, (user_addr_t)(uintptr_t)save->save_r4, save->save_r5);
	return 1;
}



