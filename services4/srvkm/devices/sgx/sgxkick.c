/*************************************************************************/ /*!
@Title          Device specific kickTA routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
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

#include "img_defs.h" /* For the macro offsetof() */
#include "services_headers.h"
#include "sgxinfo.h"
#include "sgxinfokm.h"
#if defined (PDUMP)
#include "sgxapi_km.h"
#include "pdump_km.h"
#endif
#include "sgx_bridge_km.h"
#include "osfunc.h"
#include "pvr_debug.h"
#include "sgxutils.h"
#include "ttrace.h"

#if defined(PVR_ANDROID_NATIVE_WINDOW_HAS_SYNC) || defined(PVR_ANDROID_NATIVE_WINDOW_HAS_FENCE)
#include "pvr_sync_common.h"
#endif

#if defined(SUPPORT_PVRSRV_ANDROID_SYSTRACE) && defined(EUR_CR_TIMER)
#include "systrace.h"
#endif

#if defined(SUPPORT_DMABUF)
#include "pvr_linux_fence.h"
#endif

/*!
******************************************************************************

 @Function	SGXDoKickKM

 @Description

 Really kicks the TA

 @Input hDevHandle - Device handle

 @Return ui32Error - success or failure

******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR SGXDoKickKM(IMG_HANDLE hDevHandle, SGX_CCB_KICK *psCCBKick)
{
	PVRSRV_ERROR eError;
	PVRSRV_KERNEL_SYNC_INFO	*psSyncInfo;
	PVRSRV_KERNEL_MEM_INFO	*psCCBMemInfo = (PVRSRV_KERNEL_MEM_INFO *) psCCBKick->hCCBKernelMemInfo;
	SGXMKIF_CMDTA_SHARED *psTACmd;
	IMG_UINT32 i;
	IMG_HANDLE hDevMemContext = IMG_NULL;
	IMG_HANDLE *pahDstSyncHandles;
#if defined(SUPPORT_DMABUF)
	IMG_UINT32 ui32FenceTag = 0;
	IMG_UINT32 ui32NumResvObjs = 0;
	IMG_BOOL bBlockingFences = IMG_FALSE;
#endif
#if  (defined(SUPPORT_PVRSRV_ANDROID_SYSTRACE) && defined(EUR_CR_TIMER))
	PVRSRV_DEVICE_NODE      *psDeviceNode;
	PVRSRV_SGXDEV_INFO      *psDevInfo;

	psDeviceNode = (PVRSRV_DEVICE_NODE *)hDevHandle;
	psDevInfo = (PVRSRV_SGXDEV_INFO *)psDeviceNode->pvDevice;
#endif
#if defined(FIX_HW_BRN_31620)
	hDevMemContext = psCCBKick->hDevMemContext;
#endif
	PVR_TTRACE(PVRSRV_TRACE_GROUP_KICK, PVRSRV_TRACE_CLASS_FUNCTION_ENTER, KICK_TOKEN_DOKICK);

	if (!CCB_OFFSET_IS_VALID(SGXMKIF_CMDTA_SHARED, psCCBMemInfo, psCCBKick, ui32CCBOffset))
	{
		PVR_DPF((PVR_DBG_ERROR, "SGXDoKickKM: Invalid CCB offset"));
		PVR_TTRACE(PVRSRV_TRACE_GROUP_KICK, PVRSRV_TRACE_CLASS_FUNCTION_EXIT, KICK_TOKEN_DOKICK);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	/* override QAC warning about stricter alignment */
	/* PRQA S 3305 1 */
	psTACmd = CCB_DATA_FROM_OFFSET(SGXMKIF_CMDTA_SHARED, psCCBMemInfo, psCCBKick, ui32CCBOffset);

	PVR_TTRACE(PVRSRV_TRACE_GROUP_KICK, PVRSRV_TRACE_CLASS_CMD_START, KICK_TOKEN_DOKICK);

	PVR_TTRACE_UI32(PVRSRV_TRACE_GROUP_KICK, PVRSRV_TRACE_CLASS_FRAMENUM, KICK_TOKEN_FRAMENUM, psCCBKick->ui32FrameNum);
	PVR_TTRACE_UI32(PVRSRV_TRACE_GROUP_KICK, PVRSRV_TRACE_CLASS_NONE, KICK_TOKEN_RENDERCONTEXT, psCCBKick->sCommand.ui32Data[1]);
	PVR_TTRACE_DEV_VIRTADDR(PVRSRV_TRACE_GROUP_KICK, PVRSRV_TRACE_CLASS_DEVVADDR, KICK_TOKEN_HWRTDATASET, psCCBKick->sHWRTDataSetDevAddr);
	PVR_TTRACE_DEV_VIRTADDR(PVRSRV_TRACE_GROUP_KICK, PVRSRV_TRACE_CLASS_DEVVADDR, KICK_TOKEN_HWRTDATA, psCCBKick->sHWRTDataDevAddr);

#if defined(TTRACE)
	if (psCCBKick->bFirstKickOrResume)
	{
		PVR_TTRACE(PVRSRV_TRACE_GROUP_KICK,
				   PVRSRV_TRACE_CLASS_FLAGS,
				   KICK_TOKEN_FIRST_KICK);
	}

	if (psCCBKick->bLastInScene)
	{
		PVR_TTRACE(PVRSRV_TRACE_GROUP_KICK,
				   PVRSRV_TRACE_CLASS_FLAGS,
				   KICK_TOKEN_LAST_KICK);
	}
#endif
	PVR_TTRACE_UI32(PVRSRV_TRACE_GROUP_KICK, PVRSRV_TRACE_CLASS_CCB,
			KICK_TOKEN_CCB_OFFSET, psCCBKick->ui32CCBOffset);

	/* TA/3D dependency */
	if (psCCBKick->hTA3DSyncInfo)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->hTA3DSyncInfo;

		PVR_TTRACE_SYNC_OBJECT(PVRSRV_TRACE_GROUP_KICK, KICK_TOKEN_TA3D_SYNC,
					  psSyncInfo, PVRSRV_SYNCOP_SAMPLE);

		psTACmd->sTA3DDependency.sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;

		psTACmd->sTA3DDependency.ui32WriteOpsPendingVal   = psSyncInfo->psSyncData->ui32WriteOpsPending;

		if (psCCBKick->bTADependency)
		{
			SyncTakeWriteOp(psSyncInfo, SYNC_OP_CLASS_KICKTA);
		}
	}

	if (psCCBKick->hTASyncInfo != IMG_NULL)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->hTASyncInfo;

		PVR_TTRACE_SYNC_OBJECT(PVRSRV_TRACE_GROUP_KICK, KICK_TOKEN_TA_SYNC,
					  psSyncInfo, PVRSRV_SYNCOP_SAMPLE);

		psTACmd->sTATQSyncReadOpsCompleteDevVAddr  = psSyncInfo->sReadOpsCompleteDevVAddr;
		psTACmd->sTATQSyncWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;

		psTACmd->ui32TATQSyncReadOpsPendingVal = SyncTakeReadOp(psSyncInfo, SYNC_OP_CLASS_KICKTA);
		psTACmd->ui32TATQSyncWriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending;
	}

	if (psCCBKick->h3DSyncInfo != IMG_NULL)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->h3DSyncInfo;

		PVR_TTRACE_SYNC_OBJECT(PVRSRV_TRACE_GROUP_KICK, KICK_TOKEN_3D_SYNC,
					  psSyncInfo, PVRSRV_SYNCOP_SAMPLE);

		psTACmd->s3DTQSyncReadOpsCompleteDevVAddr  = psSyncInfo->sReadOpsCompleteDevVAddr;
		psTACmd->s3DTQSyncWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;

		psTACmd->ui323DTQSyncReadOpsPendingVal = SyncTakeReadOp(psSyncInfo, SYNC_OP_CLASS_KICKTA);
		psTACmd->ui323DTQSyncWriteOpsPendingVal  = psSyncInfo->psSyncData->ui32WriteOpsPending;
	}

	psTACmd->ui32NumTAStatusVals = psCCBKick->ui32NumTAStatusVals;
	if (psCCBKick->ui32NumTAStatusVals != 0)
	{
		/* Copy status vals over */
		for (i = 0; i < psCCBKick->ui32NumTAStatusVals; i++)
		{
#if defined(SUPPORT_SGX_NEW_STATUS_VALS)
			psTACmd->sCtlTAStatusInfo[i] = psCCBKick->asTAStatusUpdate[i].sCtlStatus;
#else
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->ahTAStatusSyncInfo[i];
			psTACmd->sCtlTAStatusInfo[i].sStatusDevAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
			psTACmd->sCtlTAStatusInfo[i].ui32StatusValue = psSyncInfo->psSyncData->ui32ReadOpsPending;
#endif
		}
	}

	psTACmd->ui32Num3DStatusVals = psCCBKick->ui32Num3DStatusVals;
	if (psCCBKick->ui32Num3DStatusVals != 0)
	{
		/* Copy status vals over */
		for (i = 0; i < psCCBKick->ui32Num3DStatusVals; i++)
		{
#if defined(SUPPORT_SGX_NEW_STATUS_VALS)
			psTACmd->sCtl3DStatusInfo[i] = psCCBKick->as3DStatusUpdate[i].sCtlStatus;
#else
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->ah3DStatusSyncInfo[i];
			psTACmd->sCtl3DStatusInfo[i].sStatusDevAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
			psTACmd->sCtl3DStatusInfo[i].ui32StatusValue = psSyncInfo->psSyncData->ui32ReadOpsPending;
#endif
		}
	}


	/* texture dependencies */
#if defined(PVR_ANDROID_NATIVE_WINDOW_HAS_SYNC) || defined(PVR_ANDROID_NATIVE_WINDOW_HAS_FENCE)
	eError = PVRSyncPatchCCBKickSyncInfos(psCCBKick->ahSrcKernelSyncInfo,
										  psTACmd->asSrcSyncs,
										  &psCCBKick->ui32NumSrcSyncs);
	if(eError != PVRSRV_OK)
	{
		/* We didn't kick yet, or perform PDUMP processing, so we should
		 * be able to trivially roll back any changes made to the sync
		 * data. If we don't do this, we'll wedge services cleanup.
		 */

		if (psCCBKick->h3DSyncInfo != IMG_NULL)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->h3DSyncInfo;
			psSyncInfo->psSyncData->ui32ReadOpsPending--;
		}

		if (psCCBKick->hTASyncInfo != IMG_NULL)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->hTASyncInfo;
			psSyncInfo->psSyncData->ui32ReadOpsPending--;
		}

		if (psCCBKick->hTA3DSyncInfo && psCCBKick->bTADependency)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->hTA3DSyncInfo;
			psSyncInfo->psSyncData->ui32WriteOpsPending--;
		}

		PVR_DPF((PVR_DBG_ERROR, "SGXDoKickKM: PVRSyncPatchCCBKickSyncInfos failed."));
		PVR_TTRACE(PVRSRV_TRACE_GROUP_KICK, PVRSRV_TRACE_CLASS_FUNCTION_EXIT,
				   KICK_TOKEN_DOKICK);
		return eError;
	}
#else /* defined(PVR_ANDROID_NATIVE_WINDOW_HAS_SYNC) */
#if defined(SUPPORT_DMABUF)
	ui32NumResvObjs = PVRLinuxFenceNumResvObjs(&bBlockingFences,
			psCCBKick->ui32NumSrcSyncs,
			psCCBKick->ahSrcKernelSyncInfo,
			NULL,
			psCCBKick->bFirstKickOrResume ? psCCBKick->ui32NumDstSyncObjects : 0,
			(IMG_HANDLE*)psCCBKick->hDstSyncHandles,
			NULL);
	/*
	 * If there are no blocking fences, the GPU need not wait whilst
	 * the reservation objects are being processed. They can be processed
	 * later, after the kick.
	 */
	if (ui32NumResvObjs && bBlockingFences)
	{
		eError = PVRLinuxFenceProcess(&ui32FenceTag,
				ui32NumResvObjs,
				bBlockingFences,
				psCCBKick->ui32NumSrcSyncs,
				psCCBKick->ahSrcKernelSyncInfo,
				NULL,
				psCCBKick->bFirstKickOrResume ? psCCBKick->ui32NumDstSyncObjects : 0,
				(IMG_HANDLE*)psCCBKick->hDstSyncHandles,
				NULL);
		if (eError != PVRSRV_OK)
		{
			return eError;
		}
	}
#endif
	for (i=0; i<psCCBKick->ui32NumSrcSyncs; i++)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *) psCCBKick->ahSrcKernelSyncInfo[i];

		PVR_TTRACE_SYNC_OBJECT(PVRSRV_TRACE_GROUP_KICK, KICK_TOKEN_SRC_SYNC,
					  psSyncInfo, PVRSRV_SYNCOP_SAMPLE);

		psTACmd->asSrcSyncs[i].sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		psTACmd->asSrcSyncs[i].sReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;

		/* Get ui32ReadOpsPending snapshot and copy into the CCB - before incrementing. */
		psTACmd->asSrcSyncs[i].ui32ReadOpsPendingVal = SyncTakeReadOp(psSyncInfo, SYNC_OP_CLASS_KICKTA);
		/* Copy ui32WriteOpsPending snapshot into the CCB. */
		psTACmd->asSrcSyncs[i].ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending;
	}
#endif /* defined(PVR_ANDROID_NATIVE_WINDOW_HAS_SYNC) */
	psTACmd->ui32NumSrcSyncs = psCCBKick->ui32NumSrcSyncs;

	if (psCCBKick->bFirstKickOrResume && psCCBKick->ui32NumDstSyncObjects > 0)
	{
		PVRSRV_KERNEL_MEM_INFO	*psHWDstSyncListMemInfo =
								(PVRSRV_KERNEL_MEM_INFO *)psCCBKick->hKernelHWSyncListMemInfo;
		SGXMKIF_HWDEVICE_SYNC_LIST *psHWDeviceSyncList = psHWDstSyncListMemInfo->pvLinAddrKM;
		IMG_UINT32	ui32NumDstSyncs = psCCBKick->ui32NumDstSyncObjects;

		PVR_ASSERT(((PVRSRV_KERNEL_MEM_INFO *)psCCBKick->hKernelHWSyncListMemInfo)->uAllocSize >= (sizeof(SGXMKIF_HWDEVICE_SYNC_LIST) +
								(sizeof(PVRSRV_DEVICE_SYNC_OBJECT) * ui32NumDstSyncs)));

		psHWDeviceSyncList->ui32NumSyncObjects = ui32NumDstSyncs;
#if defined(PDUMP)
		if (PDumpIsCaptureFrameKM())
		{
			PDUMPCOMMENT("HWDeviceSyncList for TACmd\r\n");
			PDUMPMEM(IMG_NULL,
					 psHWDstSyncListMemInfo,
					 0,
					 sizeof(SGXMKIF_HWDEVICE_SYNC_LIST),
					 0,
					 MAKEUNIQUETAG(psHWDstSyncListMemInfo));
		}
#endif
		pahDstSyncHandles = psCCBKick->hDstSyncHandles;
		for (i=0; i<ui32NumDstSyncs; i++)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)pahDstSyncHandles[i];

			if (psSyncInfo)
			{
				psSyncInfo->psSyncData->ui64LastWrite = ui64KickCount;

				PVR_TTRACE_SYNC_OBJECT(PVRSRV_TRACE_GROUP_KICK, KICK_TOKEN_DST_SYNC,
							psSyncInfo, PVRSRV_SYNCOP_SAMPLE);

				psHWDeviceSyncList->asSyncData[i].sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
				psHWDeviceSyncList->asSyncData[i].sReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
				psHWDeviceSyncList->asSyncData[i].sReadOps2CompleteDevVAddr = psSyncInfo->sReadOps2CompleteDevVAddr;

				psHWDeviceSyncList->asSyncData[i].ui32ReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending;
				psHWDeviceSyncList->asSyncData[i].ui32WriteOpsPendingVal = SyncTakeWriteOp(psSyncInfo, SYNC_OP_CLASS_KICKTA);
				psHWDeviceSyncList->asSyncData[i].ui32ReadOps2PendingVal = psSyncInfo->psSyncData->ui32ReadOps2Pending;

	#if defined(PDUMP)
				if (PDumpIsCaptureFrameKM())
				{
					IMG_UINT32 ui32ModifiedValue;
					IMG_UINT32 ui32SyncOffset = offsetof(SGXMKIF_HWDEVICE_SYNC_LIST, asSyncData)
												+ (i * sizeof(PVRSRV_DEVICE_SYNC_OBJECT));
					IMG_UINT32 ui32WOpsOffset = ui32SyncOffset
												+ offsetof(PVRSRV_DEVICE_SYNC_OBJECT, ui32WriteOpsPendingVal);
					IMG_UINT32 ui32ROpsOffset = ui32SyncOffset
												+ offsetof(PVRSRV_DEVICE_SYNC_OBJECT, ui32ReadOpsPendingVal);
					IMG_UINT32 ui32ROps2Offset = ui32SyncOffset
												+ offsetof(PVRSRV_DEVICE_SYNC_OBJECT, ui32ReadOps2PendingVal);

					PDUMPCOMMENT("HWDeviceSyncObject for RT: %i\r\n", i);

					PDUMPMEM(IMG_NULL,
							 psHWDstSyncListMemInfo,
							 ui32SyncOffset,
							 sizeof(PVRSRV_DEVICE_SYNC_OBJECT),
							 0,
							 MAKEUNIQUETAG(psHWDstSyncListMemInfo));

					psSyncInfo->psSyncData->ui32LastOpDumpVal++;

					ui32ModifiedValue = psSyncInfo->psSyncData->ui32LastOpDumpVal - 1;

					PDUMPCOMMENT("Modify RT %d WOpPendingVal in HWDevSyncList\r\n", i);

#if defined(SUPPORT_PDUMP_SYNC_DEBUG)
					PDUMPCOMMENT("TA Dst: PDump sync sample: uiAddr = 0x%08x, ui32LastOpDumpVal = 0x%08x\r\n",
											psSyncInfo->sWriteOpsCompleteDevVAddr.uiAddr,
											ui32ModifiedValue);
#endif
					PDUMPMEM(&ui32ModifiedValue,
						psHWDstSyncListMemInfo,
						ui32WOpsOffset,
						sizeof(IMG_UINT32),
						0,
						MAKEUNIQUETAG(psHWDstSyncListMemInfo));

					ui32ModifiedValue = 0;
					PDUMPCOMMENT("Modify RT %d ROpsPendingVal in HWDevSyncList\r\n", i);

					PDUMPMEM(&psSyncInfo->psSyncData->ui32LastReadOpDumpVal,
						 psHWDstSyncListMemInfo,
						 ui32ROpsOffset,
						 sizeof(IMG_UINT32),
						 0,
						MAKEUNIQUETAG(psHWDstSyncListMemInfo));

					/*
					* Force the ROps2Complete value to 0.
					*/
					PDUMPCOMMENT("Modify RT %d ROps2PendingVal in HWDevSyncList\r\n", i);
					PDUMPMEM(&ui32ModifiedValue,
						psHWDstSyncListMemInfo,
						ui32ROps2Offset,
						sizeof(IMG_UINT32),
						0,
						MAKEUNIQUETAG(psHWDstSyncListMemInfo));
#if defined(SUPPORT_PDUMP_SYNC_DEBUG)
					PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_PERSISTENT, "TA Dst: PDump sync update: uiAddr = 0x%08x, ui32LastOpDumpVal = 0x%08x\r\n",
											psSyncInfo->sWriteOpsCompleteDevVAddr.uiAddr,
											psSyncInfo->psSyncData->ui32LastOpDumpVal);
#endif
				}
	#endif	/* defined(PDUMP) */
			}
			else
			{
				psHWDeviceSyncList->asSyncData[i].sWriteOpsCompleteDevVAddr.uiAddr = 0;
				psHWDeviceSyncList->asSyncData[i].sReadOpsCompleteDevVAddr.uiAddr = 0;
				psHWDeviceSyncList->asSyncData[i].sReadOps2CompleteDevVAddr.uiAddr = 0;

				psHWDeviceSyncList->asSyncData[i].ui32ReadOpsPendingVal = 0;
				psHWDeviceSyncList->asSyncData[i].ui32ReadOps2PendingVal = 0;
				psHWDeviceSyncList->asSyncData[i].ui32WriteOpsPendingVal = 0;
			}
		}
	}
	
	/* 
		NOTE: THIS MUST BE THE LAST THING WRITTEN TO THE TA COMMAND!
		Set the ready for so the uKernel will process the command.
	*/
	psTACmd->ui32CtrlFlags |= SGXMKIF_CMDTA_CTRLFLAGS_READY;

#if defined(PDUMP)
	if (PDumpIsCaptureFrameKM())
	{
		PDUMPCOMMENT("Shared part of TA command\r\n");

		PDUMPMEM(psTACmd,
				 psCCBMemInfo,
				 psCCBKick->ui32CCBDumpWOff,
				 sizeof(SGXMKIF_CMDTA_SHARED),
				 0,
				 MAKEUNIQUETAG(psCCBMemInfo));

		for (i=0; i<psCCBKick->ui32NumSrcSyncs; i++)
		{
			IMG_UINT32 	ui32ModifiedValue;
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *) psCCBKick->ahSrcKernelSyncInfo[i];

			psSyncInfo->psSyncData->ui32LastReadOpDumpVal++;

			ui32ModifiedValue = psSyncInfo->psSyncData->ui32LastReadOpDumpVal - 1;

			PDUMPCOMMENT("Modify SrcSync %d ROpsPendingVal\r\n", i);

			PDUMPMEM(&ui32ModifiedValue,
				 psCCBMemInfo,
				 psCCBKick->ui32CCBDumpWOff + offsetof(SGXMKIF_CMDTA_SHARED, asSrcSyncs) +
					(i * sizeof(PVRSRV_DEVICE_SYNC_OBJECT)) + offsetof(PVRSRV_DEVICE_SYNC_OBJECT, ui32ReadOpsPendingVal),
				 sizeof(IMG_UINT32),
				 0,
				MAKEUNIQUETAG(psCCBMemInfo));

			PDUMPCOMMENT("Modify SrcSync %d WOpPendingVal\r\n", i);

#if defined(SUPPORT_PDUMP_SYNC_DEBUG)
			PDUMPCOMMENT("TA Src: PDump sync sample: uiAddr = 0x%08x, ui32LastOpDumpVal = 0x%08x\r\n",
									psSyncInfo->sWriteOpsCompleteDevVAddr.uiAddr,
									psSyncInfo->psSyncData->ui32LastOpDumpVal);
#endif

			PDUMPMEM(&psSyncInfo->psSyncData->ui32LastOpDumpVal,
				psCCBMemInfo,
				psCCBKick->ui32CCBDumpWOff + offsetof(SGXMKIF_CMDTA_SHARED, asSrcSyncs) +
					(i * sizeof(PVRSRV_DEVICE_SYNC_OBJECT)) + offsetof(PVRSRV_DEVICE_SYNC_OBJECT, ui32WriteOpsPendingVal),
				sizeof(IMG_UINT32),
				0,
				MAKEUNIQUETAG(psCCBMemInfo));

#if defined(SUPPORT_PDUMP_SYNC_DEBUG)
			PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_PERSISTENT, "TA Src: PDump sync update: uiAddr = 0x%08x, ui32LastReadOpDumpVal = 0x%08x\r\n",
									psSyncInfo->sReadOpsCompleteDevVAddr.uiAddr,
									psSyncInfo->psSyncData->ui32LastReadOpDumpVal);
#endif
		}

		if (psCCBKick->hTA3DSyncInfo != IMG_NULL)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->hTA3DSyncInfo;

			PDUMPCOMMENT("Modify TA/3D dependency WOpPendingVal\r\n");

#if defined(SUPPORT_PDUMP_SYNC_DEBUG)
			PDUMPCOMMENT("TA TADep: PDump sync sample: uiAddr = 0x%08x, ui32LastOpDumpVal = 0x%08x\r\n",
									psSyncInfo->sWriteOpsCompleteDevVAddr.uiAddr,
									psSyncInfo->psSyncData->ui32LastOpDumpVal);
#endif

			PDUMPMEM(&psSyncInfo->psSyncData->ui32LastOpDumpVal,
					psCCBMemInfo,
					psCCBKick->ui32CCBDumpWOff + offsetof(SGXMKIF_CMDTA_SHARED, sTA3DDependency.ui32WriteOpsPendingVal),
					sizeof(IMG_UINT32),
					0,
					MAKEUNIQUETAG(psCCBMemInfo));
			
			if (psCCBKick->bTADependency)
			{
				psSyncInfo->psSyncData->ui32LastOpDumpVal++;

#if defined(SUPPORT_PDUMP_SYNC_DEBUG)
				PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_PERSISTENT, "TA TADep: PDump sync update: uiAddr = 0x%08x, ui32LastOpDumpVal = 0x%08x\r\n",
										psSyncInfo->sWriteOpsCompleteDevVAddr.uiAddr,
										psSyncInfo->psSyncData->ui32LastOpDumpVal);
#endif
			}
		}

		if (psCCBKick->hTASyncInfo != IMG_NULL)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->hTASyncInfo;

			PDUMPCOMMENT("Modify TA/TQ ROpPendingVal\r\n");

			PDUMPMEM(&psSyncInfo->psSyncData->ui32LastReadOpDumpVal,
					psCCBMemInfo,
					psCCBKick->ui32CCBDumpWOff + offsetof(SGXMKIF_CMDTA_SHARED, ui32TATQSyncReadOpsPendingVal),
					sizeof(IMG_UINT32),
					0,
					MAKEUNIQUETAG(psCCBMemInfo));

			PDUMPCOMMENT("Modify TA/TQ WOpPendingVal\r\n");

#if defined(SUPPORT_PDUMP_SYNC_DEBUG)
			PDUMPCOMMENT("TA TATQ: PDump sync sample: uiAddr = 0x%08x, ui32LastOpDumpVal = 0x%08x\r\n",
									psSyncInfo->sWriteOpsCompleteDevVAddr.uiAddr,
									psSyncInfo->psSyncData->ui32LastOpDumpVal);
#endif

			PDUMPMEM(&psSyncInfo->psSyncData->ui32LastOpDumpVal,
					psCCBMemInfo,
					psCCBKick->ui32CCBDumpWOff + offsetof(SGXMKIF_CMDTA_SHARED, ui32TATQSyncWriteOpsPendingVal),
					sizeof(IMG_UINT32),
					0,
					MAKEUNIQUETAG(psCCBMemInfo));

			psSyncInfo->psSyncData->ui32LastReadOpDumpVal++;
		}

		if (psCCBKick->h3DSyncInfo != IMG_NULL)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->h3DSyncInfo;

			PDUMPCOMMENT("Modify 3D/TQ ROpPendingVal\r\n");

			PDUMPMEM(&psSyncInfo->psSyncData->ui32LastReadOpDumpVal,
					psCCBMemInfo,
					psCCBKick->ui32CCBDumpWOff + offsetof(SGXMKIF_CMDTA_SHARED, ui323DTQSyncReadOpsPendingVal),
					sizeof(IMG_UINT32),
					0,
					MAKEUNIQUETAG(psCCBMemInfo));

			PDUMPCOMMENT("Modify 3D/TQ WOpPendingVal\r\n");

#if defined(SUPPORT_PDUMP_SYNC_DEBUG)
			PDUMPCOMMENT("TA 3DTQ: PDump sync sample: uiAddr = 0x%08x, ui32LastOpDumpVal = 0x%08x\r\n",
									psSyncInfo->sWriteOpsCompleteDevVAddr.uiAddr,
									psSyncInfo->psSyncData->ui32LastOpDumpVal);
#endif

			PDUMPMEM(&psSyncInfo->psSyncData->ui32LastOpDumpVal,
					psCCBMemInfo,
					psCCBKick->ui32CCBDumpWOff + offsetof(SGXMKIF_CMDTA_SHARED, ui323DTQSyncWriteOpsPendingVal),
					sizeof(IMG_UINT32),
					0,
					MAKEUNIQUETAG(psCCBMemInfo));

			psSyncInfo->psSyncData->ui32LastReadOpDumpVal++;
		}

		for (i = 0; i < psCCBKick->ui32NumTAStatusVals; i++)
		{
#if !defined(SUPPORT_SGX_NEW_STATUS_VALS)
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->ahTAStatusSyncInfo[i];
			PDUMPCOMMENT("Modify TA status value in TA cmd\r\n");
			PDUMPMEM(&psSyncInfo->psSyncData->ui32LastOpDumpVal,
				 psCCBMemInfo,
				 psCCBKick->ui32CCBDumpWOff + (IMG_UINT32)offsetof(SGXMKIF_CMDTA_SHARED, sCtlTAStatusInfo[i].ui32StatusValue),
				 sizeof(IMG_UINT32),
				 0,
				MAKEUNIQUETAG(psCCBMemInfo));
#endif
		}

		for (i = 0; i < psCCBKick->ui32Num3DStatusVals; i++)
		{
#if !defined(SUPPORT_SGX_NEW_STATUS_VALS)
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->ah3DStatusSyncInfo[i];
			PDUMPCOMMENT("Modify 3D status value in TA cmd\r\n");
			PDUMPMEM(&psSyncInfo->psSyncData->ui32LastOpDumpVal,
				 psCCBMemInfo,
				 psCCBKick->ui32CCBDumpWOff + (IMG_UINT32)offsetof(SGXMKIF_CMDTA_SHARED, sCtl3DStatusInfo[i].ui32StatusValue),
				 sizeof(IMG_UINT32),
				 0,
				MAKEUNIQUETAG(psCCBMemInfo));
#endif
		}
	}
#endif	/* defined(PDUMP) */

	PVR_TTRACE(PVRSRV_TRACE_GROUP_KICK, PVRSRV_TRACE_CLASS_CMD_END,
			KICK_TOKEN_DOKICK);

	eError = SGXScheduleCCBCommandKM(hDevHandle, SGXMKIF_CMD_TA, &psCCBKick->sCommand, KERNEL_ID, 0, hDevMemContext, psCCBKick->bLastInScene);
	if (eError == PVRSRV_ERROR_RETRY)
	{
#if defined(SUPPORT_DMABUF)
		if (ui32NumResvObjs && bBlockingFences)
		{
			PVRLinuxFenceRelease(ui32FenceTag,
				psCCBKick->ui32NumSrcSyncs,
				psCCBKick->ahSrcKernelSyncInfo,
				NULL,
				psCCBKick->bFirstKickOrResume ? psCCBKick->ui32NumDstSyncObjects : 0,
				(IMG_HANDLE*)psCCBKick->hDstSyncHandles,
				NULL);
		}
#endif
		if (psCCBKick->bFirstKickOrResume && psCCBKick->ui32NumDstSyncObjects > 0)
		{
			pahDstSyncHandles = psCCBKick->hDstSyncHandles;
			for (i=0; i < psCCBKick->ui32NumDstSyncObjects; i++)
			{
				/* Client will retry, so undo the write ops pending increment done above. */
				psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)pahDstSyncHandles[i];

				if (psSyncInfo)
				{
					psSyncInfo->psSyncData->ui32WriteOpsPending--;
					SyncRollBackWriteOp(psSyncInfo, SYNC_OP_CLASS_KICKTA);
#if defined(PDUMP)
					if (PDumpIsCaptureFrameKM())
					{
						psSyncInfo->psSyncData->ui32LastOpDumpVal--;
					}
#endif
				}
			}
		}

		for (i=0; i<psCCBKick->ui32NumSrcSyncs; i++)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *) psCCBKick->ahSrcKernelSyncInfo[i];
			psSyncInfo->psSyncData->ui32ReadOpsPending--;
			SyncRollBackReadOp(psSyncInfo, SYNC_OP_CLASS_KICKTA);
		}

		if (psCCBKick->hTA3DSyncInfo)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->hTA3DSyncInfo;
			psSyncInfo->psSyncData->ui32ReadOpsPending--;
			SyncRollBackReadOp(psSyncInfo, SYNC_OP_CLASS_KICKTA);
		}
	
		if (psCCBKick->hTASyncInfo)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->hTASyncInfo;
			psSyncInfo->psSyncData->ui32ReadOpsPending--;
			SyncRollBackReadOp(psSyncInfo, SYNC_OP_CLASS_KICKTA);
		}
	
		if (psCCBKick->h3DSyncInfo)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->h3DSyncInfo;
			psSyncInfo->psSyncData->ui32ReadOpsPending--;
			SyncRollBackReadOp(psSyncInfo, SYNC_OP_CLASS_KICKTA);
		}

		PVR_TTRACE(PVRSRV_TRACE_GROUP_KICK, PVRSRV_TRACE_CLASS_FUNCTION_EXIT,
				KICK_TOKEN_DOKICK);
		return eError;
	}
	else if (PVRSRV_OK != eError)
	{
		PVR_DPF((PVR_DBG_ERROR, "SGXDoKickKM: SGXScheduleCCBCommandKM failed."));
		PVR_TTRACE(PVRSRV_TRACE_GROUP_KICK, PVRSRV_TRACE_CLASS_FUNCTION_EXIT,
				KICK_TOKEN_DOKICK);
#if defined(SUPPORT_DMABUF) && defined(NO_HARDWARE)
		PVRLinuxFenceCheckAll();
#endif
		return eError;
	}
#if defined(SUPPORT_DMABUF)
	else if (ui32NumResvObjs && !bBlockingFences)
	{
		eError = PVRLinuxFenceProcess(&ui32FenceTag,
				ui32NumResvObjs,
				bBlockingFences,
				psCCBKick->ui32NumSrcSyncs,
				psCCBKick->ahSrcKernelSyncInfo,
				NULL,
				psCCBKick->bFirstKickOrResume ? psCCBKick->ui32NumDstSyncObjects : 0,
				(IMG_HANDLE*)psCCBKick->hDstSyncHandles,
				NULL);
		if (eError != PVRSRV_OK)
		{
			return eError;
		}
	}
#endif


#if defined(NO_HARDWARE)


	/* TA/3D dependency */
	if (psCCBKick->hTA3DSyncInfo)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->hTA3DSyncInfo;

		if (psCCBKick->bTADependency)
		{
			psSyncInfo->psSyncData->ui32WriteOpsComplete = psSyncInfo->psSyncData->ui32WriteOpsPending;
		}
	}

	if (psCCBKick->hTASyncInfo != IMG_NULL)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->hTASyncInfo;

		psSyncInfo->psSyncData->ui32ReadOpsComplete =  psSyncInfo->psSyncData->ui32ReadOpsPending;
	}

	if (psCCBKick->h3DSyncInfo != IMG_NULL)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->h3DSyncInfo;

		psSyncInfo->psSyncData->ui32ReadOpsComplete =  psSyncInfo->psSyncData->ui32ReadOpsPending;
	}

	/* Copy status vals over */
	for (i = 0; i < psCCBKick->ui32NumTAStatusVals; i++)
	{
#if defined(SUPPORT_SGX_NEW_STATUS_VALS)
		PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo = (PVRSRV_KERNEL_MEM_INFO*)psCCBKick->asTAStatusUpdate[i].hKernelMemInfo;
		/* derive offset into meminfo and write the status value */
		*(IMG_UINT32*)((IMG_UINTPTR_T)psKernelMemInfo->pvLinAddrKM
						+ (psTACmd->sCtlTAStatusInfo[i].sStatusDevAddr.uiAddr
						- psKernelMemInfo->sDevVAddr.uiAddr)) = psTACmd->sCtlTAStatusInfo[i].ui32StatusValue;
#else
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->ahTAStatusSyncInfo[i];
		psSyncInfo->psSyncData->ui32ReadOpsComplete = psTACmd->sCtlTAStatusInfo[i].ui32StatusValue;
#endif
	}

	/* texture dependencies */
	for (i=0; i<psCCBKick->ui32NumSrcSyncs; i++)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *) psCCBKick->ahSrcKernelSyncInfo[i];
		psSyncInfo->psSyncData->ui32ReadOpsComplete =  psSyncInfo->psSyncData->ui32ReadOpsPending;
	}

	if (psCCBKick->bTerminateOrAbort)
	{
		if (psCCBKick->ui32NumDstSyncObjects > 0)
		{
			PVRSRV_KERNEL_MEM_INFO	*psHWDstSyncListMemInfo =
								(PVRSRV_KERNEL_MEM_INFO *)psCCBKick->hKernelHWSyncListMemInfo;
			SGXMKIF_HWDEVICE_SYNC_LIST *psHWDeviceSyncList = psHWDstSyncListMemInfo->pvLinAddrKM;
			pahDstSyncHandles = psCCBKick->hDstSyncHandles;
			for (i=0; i<psCCBKick->ui32NumDstSyncObjects; i++)
			{
				psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)pahDstSyncHandles[i];
				if (psSyncInfo)
					psSyncInfo->psSyncData->ui32WriteOpsComplete = psHWDeviceSyncList->asSyncData[i].ui32WriteOpsPendingVal+1;
			}
		}

		/* Copy status vals over */
		for (i = 0; i < psCCBKick->ui32Num3DStatusVals; i++)
		{
#if defined(SUPPORT_SGX_NEW_STATUS_VALS)
			PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo = (PVRSRV_KERNEL_MEM_INFO*)psCCBKick->as3DStatusUpdate[i].hKernelMemInfo;
			/* derive offset into meminfo and write the status value */
			*(IMG_UINT32*)((IMG_UINTPTR_T)psKernelMemInfo->pvLinAddrKM
							+ (psTACmd->sCtl3DStatusInfo[i].sStatusDevAddr.uiAddr
							- psKernelMemInfo->sDevVAddr.uiAddr)) = psTACmd->sCtl3DStatusInfo[i].ui32StatusValue;
#else
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psCCBKick->ah3DStatusSyncInfo[i];
			psSyncInfo->psSyncData->ui32ReadOpsComplete = psTACmd->sCtl3DStatusInfo[i].ui32StatusValue;
#endif
		}
	}
#if defined(SUPPORT_DMABUF)
	PVRLinuxFenceCheckAll();
#endif
#endif
	PVR_TTRACE(PVRSRV_TRACE_GROUP_KICK, PVRSRV_TRACE_CLASS_FUNCTION_EXIT,
			KICK_TOKEN_DOKICK);
#if defined(SUPPORT_PVRSRV_ANDROID_SYSTRACE) && defined(EUR_CR_TIMER)
	SystraceTAKick(psDevInfo, psCCBKick->ui32FrameNum, psCCBKick->sHWRTDataDevAddr.uiAddr, psCCBKick->bIsFirstKick);
#endif
	return eError;
}

/******************************************************************************
 End of file (sgxkick.c)
******************************************************************************/
