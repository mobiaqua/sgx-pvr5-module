/*************************************************************************/ /*!
@Title          PowerVR drm driver
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    linux module setup
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
#if defined(SUPPORT_DRI_DRM)

#include <linux/version.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/ioctl.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_drv.h>
#include <drm/drm_device.h>
#include <drm/drm.h>
#include <linux/of_reserved_mem.h>
#include <linux/of.h>
#include <linux/sched/signal.h>

#include "img_defs.h"
#include "services.h"
#include "kerneldisplay.h"
#include "kernelbuffer.h"
#include "syscommon.h"
#include "pvrmmap.h"
#include "mm.h"
#include "mmap.h"
#include "mutex.h"
#include "pvr_debug.h"
#include "srvkm.h"
#include "perproc.h"
#include "handle.h"
#include "pvr_bridge_km.h"
#include "pvr_bridge.h"
#include "pvrmodule.h"
#include "pvrversion.h"
#include "lock.h"
#include "linkage.h"
#include "pvr_drm.h"

#if defined(PVR_DRI_DRM_NOT_PCI)
#include "pvr_drm_mod.h"
#endif

#define DRM_ARRAY_SIZE(x) ARRAY_SIZE(x)

#if (defined(PVR_LDM_PLATFORM_PRE_REGISTERED) || defined(PVR_LDM_DEVICE_TREE)) && !defined(NO_HARDWARE)
#define PVR_USE_PRE_REGISTERED_PLATFORM_DEV
#endif

#if defined(PVR_LDM_DEVICE_TREE) && !defined(NO_HARDWARE)
#define PVR_USE_DEVICE_TREE
#endif

#if (defined(PVR_DRI_DRM_PLATFORM_DEV) && !defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV)) || defined(NO_HARDWARE)
#define PVR_DRM_NAME	SYS_SGX_DEV_NAME
#else
#define PVR_DRM_NAME	PVRSRV_MODNAME
#endif

#define PVR_DRM_DESC	"Imagination Technologies PVR DRM"

#define	PVR_DRM_DATE	"20110701"

#if defined(PVR_DRI_DRM_PLATFORM_DEV) && !defined(SUPPORT_DRI_DRM_PLUGIN)
#define PVR_NEW_STYLE_DRM_PLATFORM_DEV
#endif

/*
 * Prior to Linux 2.6.36, we couldn't do the release processing in post close
 * when workqueues were being used, because drm_release held the big kernel
 * lock (BKL) when it called post close.
 * If the resman needs to wait for processing being done by a workqueue,
 * that processing won't complete whilst the lock is held by another thread,
 * as the workqueue won't get scheduled.
 */
#undef	PVR_DRI_DRM_USE_POST_CLOSE
#define	PVR_DRI_DRM_USE_POST_CLOSE

#define PVR_DRM_DRIVER_RENDER	DRIVER_RENDER
#define PVR_DRM_RENDER_ALLOW	DRM_RENDER_ALLOW

DECLARE_WAIT_QUEUE_HEAD(sWaitForInit);

#if defined(SUPPORT_DRM_MODESET)
static struct drm_driver sPVRDrmDriver;
#endif

/* Once bInitComplete and bInitFailed are set, they stay set */
IMG_BOOL bInitComplete;
IMG_BOOL bInitFailed;

#if !defined(PVR_DRI_DRM_NOT_PCI) && !defined(SUPPORT_DRI_DRM_PLUGIN)
#if defined(PVR_DRI_DRM_PLATFORM_DEV)
#if defined(PVR_NEW_STYLE_DRM_PLATFORM_DEV) && !defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV)
static struct platform_device *psPlatDev;
#endif
struct platform_device *gpsPVRLDMDev;
#else
struct pci_dev *gpsPVRLDMDev;
#endif
#endif

struct drm_device *gpsPVRDRMDev;

#define PVR_DRM_FILE struct drm_file *

#if !defined(SUPPORT_DRI_DRM_EXT) && !defined(SUPPORT_DRI_DRM_PLUGIN)
#if defined(PVR_USE_DEVICE_TREE)
static struct of_device_id asPlatIdList[] = {
	{	.compatible = "ti,am654-sgx544"		},
	{	.compatible = "ti,dra7-sgx544"		},
	{	.compatible = "ti,am4376-sgx530"	},
	{	.compatible = "ti,am3352-sgx530"	},
	{	.compatible = "ti,omap3-sgx530-121"	},
	{	.compatible = "ti,omap4-sgx540-120"	},
	{	.compatible = "ti,am5728-gpu"		},
	{	.compatible = "ti,omap3430-gpu"		},
	{	.compatible = "ti,omap3630-gpu"		},
	{	.compatible = "ti,omap4430-gpu"		},
	{	.compatible = "ti,omap5432-gpu"		},
	{	.compatible = "img,powervr-sgx530"	},
	{	.compatible = "img,powervr-sgx544"	},
	{}
};
MODULE_DEVICE_TABLE(of,  asPlatIdList);
#else
#if defined(PVR_DRI_DRM_PLATFORM_DEV)
static struct platform_device_id asPlatIdList[] = {
	{SYS_SGX_DEV_NAME, 0},
	{}
};
#else	/* defined(PVR_DRI_DRM_PLATFORM_DEV) */
static struct pci_device_id asPciIdList[] = {
#if defined(PVR_DRI_DRM_NOT_PCI)
	{1, 1, 1, 1, 0, 0, 0},
#else	/* defined(PVR_DRI_DRM_NOT_PCI) */
	{SYS_SGX_DEV_VENDOR_ID, SYS_SGX_DEV_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
#if defined(SYS_SGX_DEV1_DEVICE_ID)
	{SYS_SGX_DEV_VENDOR_ID, SYS_SGX_DEV1_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
#endif	/* defined(SYS_SGX_DEV1_DEVICE_ID) */
#endif	/* defined(PVR_DRI_DRM_NOT_PCI) */
	{0}
};
#endif	/* defined(PVR_DRI_DRM_PLATFORM_DEV) */
#endif	/* defined(PVR_DEVICE_TREE) */
#endif	/* !defined(SUPPORT_DRI_DRM_EXT) */

struct device *
PVRLDMGetDevice(void)
{
	return gpsPVRDRMDev->dev;
}

DRI_DRM_STATIC int
PVRSRVDrmLoad(struct drm_device *dev, unsigned long flags)
{
	int iRes = 0;

	PVR_TRACE(("PVRSRVDrmLoad"));

	gpsPVRDRMDev = dev;
#if !defined(PVR_DRI_DRM_NOT_PCI) && !defined(SUPPORT_DRI_DRM_PLUGIN)
#if defined(PVR_DRI_DRM_PLATFORM_DEV)
	gpsPVRLDMDev = to_platform_device(dev->dev);
#else
	gpsPVRLDMDev = dev->pdev;
#endif
#endif

#if defined(PDUMP)
	iRes = dbgdrv_init();
	if (iRes != 0)
	{
		goto exit;
	}
#endif
	/* Module initialisation */
	iRes = PVRCore_Init();
	if (iRes != 0)
	{
		goto exit_dbgdrv_cleanup;
	}

#if defined(DISPLAY_CONTROLLER)
	iRes = PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Init)(dev);
	if (iRes != 0)
	{
		goto exit_pvrcore_cleanup;
	}
#endif
	goto exit;

#if defined(DISPLAY_CONTROLLER)
exit_pvrcore_cleanup:
	PVRCore_Cleanup();
#endif
exit_dbgdrv_cleanup:
#if defined(PDUMP)
	dbgdrv_cleanup();
#endif
exit:
	if (iRes != 0)
	{
		bInitFailed = IMG_TRUE;
	}
	bInitComplete = IMG_TRUE;

	wake_up_interruptible(&sWaitForInit);

	return iRes;
}

DRI_DRM_STATIC void
PVRSRVDrmUnload(struct drm_device *dev)
{
	PVR_TRACE(("PVRSRVDrmUnload"));

#if defined(DISPLAY_CONTROLLER)
	PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Cleanup)(dev);
#endif

	PVRCore_Cleanup();

#if defined(PDUMP)
	dbgdrv_cleanup();
#endif
}

DRI_DRM_STATIC int
PVRSRVDrmOpen(struct drm_device *dev, struct drm_file *file)
{
	while (!bInitComplete)
	{
		DEFINE_WAIT(sWait);

		prepare_to_wait(&sWaitForInit, &sWait, TASK_INTERRUPTIBLE);

		if (!bInitComplete)
		{
			PVR_TRACE(("%s: Waiting for module initialisation to complete", __FUNCTION__));

			schedule();
		}

		finish_wait(&sWaitForInit, &sWait);

		if (signal_pending(current))
		{
			return -ERESTARTSYS;
		}
	}

	if (bInitFailed)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Module initialisation failed", __FUNCTION__));
		return -EINVAL;
	}

	return PVRSRVOpen(dev, file);
}

#if defined(PVR_DRI_DRM_USE_POST_CLOSE) || defined(SUPPORT_DRI_DRM_PLUGIN)
#if defined(SUPPORT_DRI_DRM_PLUGIN)
DRI_DRM_STATIC int
PVRSRVDrmRelease(struct drm_device *dev, struct drm_file *file)
#else
DRI_DRM_STATIC void
PVRSRVDrmPostClose(struct drm_device *dev, struct drm_file *file)
#endif
{
	PVRSRVRelease(file->driver_priv);

	file->driver_priv = NULL;

#if defined(SUPPORT_DRI_DRM_PLUGIN)
	return 0;
#endif
}
#else
DRI_DRM_STATIC int
PVRSRVDrmRelease(struct inode *inode, struct file *filp)
{
	struct drm_file *file_priv = filp->private_data;
	void *psDriverPriv = file_priv->driver_priv;
	int ret;

	ret = drm_release(inode, filp);

	if (ret != 0)
	{
		/*
		 * An error means drm_release didn't call drm_lastclose,
		 * but it will have freed file_priv.
		 */
		PVR_DPF((PVR_DBG_ERROR, "%s : drm_release failed: %d",
			__FUNCTION__, ret));
	}

	PVRSRVRelease(psDriverPriv);

	return 0;
}
#endif

DRI_DRM_STATIC int
PVRDRMIsMaster(struct drm_device *dev, void *arg, struct drm_file *pFile)
{
	return 0;
}

#if defined(SUPPORT_DRI_DRM_EXT)
int
PVRDRM_Dummy_ioctl(struct drm_device *dev, void *arg, struct drm_file *pFile)
{
	return 0;
}
#endif

DRI_DRM_STATIC int
PVRDRMUnprivCmd(struct drm_device *dev, void *arg, struct drm_file *pFile)
{
	int ret = 0;

	LinuxLockMutexNested(&gPVRSRVLock, PVRSRV_LOCK_CLASS_BRIDGE);

	if (arg == NULL)
	{
		ret = -EFAULT;
	}
	else
	{
		drm_pvr_unpriv_cmd *psArgs = (drm_pvr_unpriv_cmd *)arg;

		switch (psArgs->cmd)
		{
			case PVR_DRM_UNPRIV_INIT_SUCCESFUL:
				psArgs->res = PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL) ? 1 : 0;
				break;

			default:
				ret = -EFAULT;
		}

	}

	LinuxUnLockMutex(&gPVRSRVLock);

	return ret;
}

#if defined(DISPLAY_CONTROLLER) && defined(PVR_DISPLAY_CONTROLLER_DRM_IOCTL)
static int
PVRDRM_Display_ioctl(struct drm_device *dev, void *arg, struct drm_file *pFile)
{
	int res;

	LinuxLockMutexNested(&gPVRSRVLock, PVRSRV_LOCK_CLASS_BRIDGE);

	res = PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Ioctl)(dev, arg, pFile);

	LinuxUnLockMutex(&gPVRSRVLock);

	return res;
}
#endif

#if defined(SUPPORT_DRM_MODESET)
static int
PVRSRVPciProbe(struct pci_dev *dev, const struct pci_device_id *id)
{
	PVR_TRACE(("PVRSRVPciProbe"));

	return drm_get_pci_dev(dev, id, &sPVRDrmDriver);
}

static void
PVRSRVPciRemove(struct pci_dev *dev)
{
	struct drm_device *psDrmDev;

	PVR_TRACE(("PVRSRVPciRemove"));

	psDrmDev = pci_get_drvdata(dev);
	drm_put_dev(psDrmDev);
}
#endif

/*
 * For Linux 2.6.33 and above, the DRM ioctl entry point is of the unlocked
 * variety.  The big kernel lock is still taken for ioctls, unless
 * the DRM_UNLOCKED flag is set.  If you revise one of the driver specific
 * ioctls, or add a new one, consider whether the gPVRSRVLock mutex needs
 * to be taken.
 */
#define	PVR_DRM_FOPS_IOCTL	.unlocked_ioctl
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
#define	PVR_DRM_UNLOCKED	DRM_UNLOCKED
#endif

#if !defined(DRM_IOCTL_DEF_DRV)
#define DRM_IOCTL_DEF_DRV(ioctl, _func, _flags) DRM_IOCTL_DEF(DRM_##ioctl, _func, _flags)
#endif

#if !defined(SUPPORT_DRI_DRM_EXT)
struct drm_ioctl_desc sPVRDrmIoctls[] = {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
	DRM_IOCTL_DEF_DRV(PVR_SRVKM, PVRSRV_BridgeDispatchKM, PVR_DRM_RENDER_ALLOW | PVR_DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(PVR_IS_MASTER, PVRDRMIsMaster, PVR_DRM_RENDER_ALLOW | DRM_MASTER | PVR_DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(PVR_UNPRIV, PVRDRMUnprivCmd, PVR_DRM_RENDER_ALLOW | PVR_DRM_UNLOCKED),
#if defined(PDUMP)
	DRM_IOCTL_DEF_DRV(PVR_DBGDRV, dbgdrv_ioctl, PVR_DRM_RENDER_ALLOW | PVR_DRM_UNLOCKED),
#endif
#if defined(DISPLAY_CONTROLLER) && defined(PVR_DISPLAY_CONTROLLER_DRM_IOCTL)
	DRM_IOCTL_DEF_DRV(PVR_DISP, PVRDRM_Display_ioctl, DRM_MASTER | PVR_DRM_UNLOCKED)
#endif
#else
	DRM_IOCTL_DEF_DRV(PVR_SRVKM, PVRSRV_BridgeDispatchKM, PVR_DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(PVR_IS_MASTER, PVRDRMIsMaster, PVR_DRM_RENDER_ALLOW | DRM_MASTER),
	DRM_IOCTL_DEF_DRV(PVR_UNPRIV, PVRDRMUnprivCmd, PVR_DRM_RENDER_ALLOW),
#if defined(PDUMP)
	DRM_IOCTL_DEF_DRV(PVR_DBGDRV, dbgdrv_ioctl, PVR_DRM_RENDER_ALLOW),
#endif
#if defined(DISPLAY_CONTROLLER) && defined(PVR_DISPLAY_CONTROLLER_DRM_IOCTL)
	DRM_IOCTL_DEF_DRV(PVR_DISP, PVRDRM_Display_ioctl, DRM_MASTER)
#endif
#endif
};

#if !defined(SUPPORT_DRI_DRM_PLUGIN)
static int pvr_max_ioctl = DRM_ARRAY_SIZE(sPVRDrmIoctls);
#endif

#if defined(PVR_DRI_DRM_PLATFORM_DEV) && !defined(SUPPORT_DRI_DRM_EXT) && \
	!defined(SUPPORT_DRI_DRM_PLUGIN)
static int PVRSRVDrmProbe(struct platform_device *pDevice);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,12,0)
static int PVRSRVDrmRemove(struct platform_device *pDevice);
#else
static void PVRSRVDrmRemove(struct platform_device *pDevice);
#endif
#endif	/* defined(PVR_DRI_DRM_PLATFORM_DEV) && !defined(SUPPORT_DRI_DRM_EXT) */

#if defined(SUPPORT_DRI_DRM_PLUGIN)
static PVRSRV_DRM_PLUGIN sPVRDrmPlugin =
{
	.name = PVR_DRM_NAME,

	.open = PVRSRVDrmOpen,
	.load = PVRSRVDrmLoad,
	.unload = PVRSRVDrmUnload,

	.release = PVRSRVDrmRelease,

	.mmap = PVRMMap,

	.ioctls = sPVRDrmIoctls,
	.num_ioctls = DRM_ARRAY_SIZE(sPVRDrmIoctls),
	.ioctl_start = 0
};
#else	/* defined(SUPPORT_DRI_DRM_PLUGIN) */

#if defined(CONFIG_COMPAT)
static long pvr_compat_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	unsigned int nr = DRM_IOCTL_NR(cmd);

	if (nr < DRM_COMMAND_BASE)
	{
		return drm_compat_ioctl(file, cmd, arg);
	}

	return drm_ioctl(file, cmd, arg);
}
#endif /* defined(CONFIG_COMPAT) */

static const struct file_operations sPVRFileOps = 
{
	.owner = THIS_MODULE,
	.open = drm_open,
#if defined(PVR_DRI_DRM_USE_POST_CLOSE)
	.release = drm_release,
#else
	.release = PVRSRVDrmRelease,
#endif
	PVR_DRM_FOPS_IOCTL = drm_ioctl,
#if defined(CONFIG_COMPAT)
	.compat_ioctl  = pvr_compat_ioctl,
#endif
	.mmap = PVRMMap,
	.poll = drm_poll,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,12,0))
	.fop_flags = FOP_UNSIGNED_OFFSET,
#endif
};

static struct drm_driver sPVRDrmDriver = 
{
	.driver_features = PVR_DRM_DRIVER_RENDER
#if defined(PVR_OLD_STYLE_DRM_PLATFORM_DEV)
		| DRIVER_USE_PLATFORM_DEVICE
#endif
		,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,11,0))
	.dev_priv_size = 0,
#endif
	.load = PVRSRVDrmLoad,
	.unload = PVRSRVDrmUnload,
	.open = PVRSRVDrmOpen,
#if defined(PVR_DRI_DRM_USE_POST_CLOSE)
	.postclose = PVRSRVDrmPostClose,
#endif
#if !defined(PVR_DRI_DRM_PLATFORM_DEV) && !defined(SUPPORT_DRM_MODESET)
	.suspend = PVRSRVDriverSuspend,
	.resume = PVRSRVDriverResume,
#endif
	.ioctls = sPVRDrmIoctls,
	.fops = &sPVRFileOps,
	.name = "pvr",
	.desc = PVR_DRM_DESC,
	.date = PVR_DRM_DATE,
	.major = PVRVERSION_MAJ,
	.minor = PVRVERSION_MIN,
	.patchlevel = PVRVERSION_BUILD,
};

#if !defined(PVR_DRI_DRM_PLATFORM_DEV)
static struct pci_driver sPVRPCIDriver =
{
		.name = PVR_DRM_NAME,
		.id_table = asPciIdList,
#if defined(SUPPORT_DRM_MODESET)
		.probe = PVRSRVPciProbe,
		.remove = PVRSRVPciRemove,
		.suspend = PVRSRVDriverSuspend,
		.resume = PVRSRVDriverResume,
#endif
};
#endif

#if defined(PVR_NEW_STYLE_DRM_PLATFORM_DEV)
#if !defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV)
static struct platform_device_info sPVRPlatDeviceInfo = {
	.name			= PVR_DRM_NAME,
	.id			= -1,
	.dma_mask = DMA_BIT_MASK(32)
};
#endif

static struct platform_driver sPVRPlatDriver =
{
#if !defined(PVR_USE_DEVICE_TREE)
	.id_table = asPlatIdList,
#endif
	.driver =
	{
		.name = PVR_DRM_NAME,
#if defined(PVR_USE_DEVICE_TREE)
		.of_match_table = asPlatIdList,
#endif
	},
	.probe = PVRSRVDrmProbe,
	.remove = PVRSRVDrmRemove,
	.suspend = PVRSRVDriverSuspend,
	.resume = PVRSRVDriverResume,
	.shutdown = PVRSRVDriverShutdown,
};
#endif

#endif	/* defined(SUPPORT_DRI_DRM_PLUGIN) */

#if defined(PVR_DRI_DRM_PLATFORM_DEV) && !defined(SUPPORT_DRI_DRM_EXT) && \
	!defined(SUPPORT_DRI_DRM_PLUGIN)
static int
PVRSRVDrmProbe(struct platform_device *pDevice)
{
	int ret;
	struct device *dev = &pDevice->dev;
	struct drm_device *drm_dev;

	PVR_TRACE(("PVRSRVDrmProbe"));

	LinuxSetCMARegion(of_reserved_mem_device_init(&pDevice->dev) ?
			IMG_FALSE: IMG_TRUE);

#if defined(PVR_NEW_STYLE_DRM_PLATFORM_DEV)
	gpsPVRLDMDev = pDevice;

	drm_dev = drm_dev_alloc(&sPVRDrmDriver, dev);
	if (IS_ERR(drm_dev)) {
		dev_err(dev, "Unable to allocate SGX DRM device!\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pDevice, drm_dev);
	ret = drm_dev_register(drm_dev, 0);
	if (ret != 0) {
		dev_err(dev, "Unable to register SGX DRM device\n");
		drm_dev_put(drm_dev);
	}

	return ret;

#else
	return drm_get_platform_dev(pDevice, &sPVRDrmDriver);
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,12,0)
static int
#else
static void
#endif
PVRSRVDrmRemove(struct platform_device *pDevice)
{
	struct drm_device *drm_dev = platform_get_drvdata(pDevice);

	PVR_TRACE(("PVRSRVDrmRemove"));

	drm_dev_unregister(drm_dev);
	drm_dev_put(drm_dev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,12,0)
	return 0;
#endif
}
#endif

static int __init PVRSRVDrmInit(void)
{
	int iRes;
#if !defined(SUPPORT_DRI_DRM_PLUGIN)
	sPVRDrmDriver.num_ioctls = pvr_max_ioctl;
#endif

#if defined(SUPPORT_DRM_MODESET)
	sPVRDrmDriver.driver_features |= DRIVER_MODESET;
#endif

	/* Must come before attempting to print anything via Services */
	PVRDPFInit();

#if defined(PVR_NEW_STYLE_DRM_PLATFORM_DEV)
	iRes = platform_driver_register(&sPVRPlatDriver);
#if !defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV)
	if (iRes == 0)
	{
		psPlatDev = platform_device_register_full(&sPVRPlatDeviceInfo);
		if (IS_ERR(psPlatDev))
		{
			iRes = PTR_ERR(psPlatDev);
			psPlatDev = NULL;
			platform_driver_unregister(&sPVRPlatDriver);
		}
	}
#endif
#else	/* defined(PVR_NEW_STYLE_DRM_PLATFORM_DEV) */
#if defined(SUPPORT_DRI_DRM_PLUGIN)
	iRes = SysDRMRegisterPlugin(&sPVRDrmPlugin);
#else	/* defined(SUPPORT_DRI_DRM_PLUGIN) */
#if defined(PVR_DRI_DRM_NOT_PCI)
	iRes = drm_pvr_dev_add();
	if (iRes != 0)
	{
		return iRes;
	}
#endif

#if defined(PVR_DRI_DRM_PLATFORM_DEV)
	iRes = drm_platform_init(&sPVRDrmDriver, gpsPVRLDMDev);
#else
	iRes = drm_pci_init(&sPVRDrmDriver, &sPVRPCIDriver);
#endif

#if defined(PVR_DRI_DRM_NOT_PCI)
	if (iRes != 0)
	{
		drm_pvr_dev_remove();
	}
#endif
#endif	/* defined(SUPPORT_DRI_DRM_PLUGIN) */
#endif	/* defined(PVR_NEW_STYLE_DRM_PLATFORM_DEV) */
	return iRes;
}
	
static void __exit PVRSRVDrmExit(void)
{
#if defined(PVR_NEW_STYLE_DRM_PLATFORM_DEV)
#if !defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV)
	platform_device_unregister(psPlatDev);
#endif
	platform_driver_unregister(&sPVRPlatDriver);
#else	/* defined(PVR_NEW_STYLE_DRM_PLATFORM_DEV) */
#if defined(SUPPORT_DRI_DRM_PLUGIN)
	SysDRMUnregisterPlugin(&sPVRDrmPlugin);
#else	/* defined(SUPPORT_DRI_DRM_PLUGIN) */
#if defined(PVR_DRI_DRM_PLATFORM_DEV)
	drm_platform_exit(&sPVRDrmDriver, gpsPVRLDMDev);
#else
	drm_pci_exit(&sPVRDrmDriver, &sPVRPCIDriver);
#endif

#if defined(PVR_DRI_DRM_NOT_PCI)
	drm_pvr_dev_remove();
#endif
#endif	/* defined(SUPPORT_DRI_DRM_PLUGIN) */
#endif	/* defined(PVR_NEW_STYLE_DRM_PLATFORM_DEV) */
}

/*
 * These macro calls define the initialisation and removal functions of the
 * driver.  Although they are prefixed `module_', they apply when compiling
 * statically as well; in both cases they define the function the kernel will
 * run to start/stop the driver.
*/
module_init(PVRSRVDrmInit);
module_exit(PVRSRVDrmExit);
#endif	/* !defined(SUPPORT_DRI_DRM_EXT) */
#endif	/* defined(SUPPORT_DRI_DRM) */


