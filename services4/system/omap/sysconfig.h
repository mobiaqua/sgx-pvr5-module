/*************************************************************************/ /*!
@Title          System Description Header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    This header provides system-specific declarations and macros
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#if !defined(__SOCCONFIG_H__)
#define __SOCCONFIG_H__

#if defined(__linux__)

#if (SGXCORE == 544) && (SGX_CORE_REV == 117) // AM65xx

/* Information not coming from DT files */
#define SYS_SGX_PDS_TIMER_FREQ		(1000)	// 1ms (1000hz)
#define SYS_SGX_HWRECOVERY_TIMEOUT_FREQ	(100)	// 10ms (100hz)
#define SYS_SGX_CLOCK_SPEED		450000000

#elif (SGXCORE == 530) && (SGX_CORE_REV == 125) // OMAP3630, AM335x, AM43xx

/* Information not coming from DT files */
#define SYS_SGX_PDS_TIMER_FREQ		(1000)	// 1ms (1000hz)
#define SYS_SGX_HWRECOVERY_TIMEOUT_FREQ	(50)	// 20ms (50hz)
#define SYS_SGX_CLOCK_SPEED		200000000

/* Timer info for debug and timing builds
 * AM335x uses GP7 TIMER
 */
#define SYS_OMAP_GPTIMER_ENABLE_SYS_PHYS_BASE	0x4804A038
#define SYS_OMAP_GPTIMER_REGS_SYS_PHYS_BASE	0x4804A03C
#define SYS_OMAP_GPTIMER_TSICR_SYS_PHYS_BASE	0x4804A054

#elif (SGXCORE == 544) && (SGX_CORE_REV == 116) // AM57xx, DRA7x

/* Information not coming from DT files */
#define SYS_SGX_PDS_TIMER_FREQ      (1000)   // 1ms (1000hz)
#define SYS_SGX_HWRECOVERY_TIMEOUT_FREQ   (100) // 10ms (100hz)

#define SYS_SGX_CLOCK_SPEED      304742400

/* Timer info for debug and timing builds
 * DRA7 uses GP11 TIMER
 */
#define SYS_OMAP_GPTIMER_ENABLE_SYS_PHYS_BASE   0x48088038
#define SYS_OMAP_GPTIMER_REGS_SYS_PHYS_BASE  0x4808803C
#define SYS_OMAP_GPTIMER_TSICR_SYS_PHYS_BASE 0x48088054

#elif (SGXCORE == 540) && (SGX_CORE_REV == 120) // OMAP44xx

/* Information not coming from DT files */
#define SYS_SGX_PDS_TIMER_FREQ 		(1000)	// 1ms (1000hz)
#define SYS_SGX_HWRECOVERY_TIMEOUT_FREQ	(100)	// 10ms (100hz)

#define SYS_SGX_CLOCK_SPEED		307200000

/* Timer info for debug and timing builds
 * DRA7 uses GP11 TIMER
 */
#define SYS_OMAP_GPTIMER_ENABLE_SYS_PHYS_BASE	0x48088038
#define SYS_OMAP_GPTIMER_REGS_SYS_PHYS_BASE	0x4808803C
#define SYS_OMAP_GPTIMER_TSICR_SYS_PHYS_BASE	0x48088054

#endif

/* Allow the AP latency to be overridden in the build config */
#if !defined(SYS_SGX_ACTIVE_POWER_LATENCY_MS)
#define SYS_SGX_ACTIVE_POWER_LATENCY_MS		(2)
#endif

/* Interrupt bits */
#define DEVICE_SGX_INTERRUPT		(1<<0)

#endif 	/* defined(__linux__) */

#endif	/* __SYSCONFIG_H__ */
