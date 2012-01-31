/* 
 * capi_mod_misdn.c
 *
 * Author       Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright 2011  by Karsten Keil <kkeil@linux-pingi.de>
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE
 * version 2.1 as published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU LESSER GENERAL PUBLIC LICENSE for more details.
 *
 */

/**
 * \file capi_mod_misdn.c
 * CAPI 2.0 module for mISDN
 */

#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/capi.h>
#include <errno.h>
#include <unistd.h>
#include <sys/un.h>
#include <poll.h>
#include <capi20.h>
#include <capi_mod.h>
#include <capiutils.h>
#include "../m_capi_sock.h"

#ifdef MISDND_CAPI_MODULE_DEBUG
static FILE *mIm_debug = NULL;
static char mIm_debug_file[128];


#define mId_print(fmt, ...)	do { \
					if (mIm_debug) { \
						fprintf(mIm_debug, fmt, ##__VA_ARGS__); \
						fflush(mIm_debug); \
					} \
				} while(0)
#else
#define mId_print(fmt, ...)	do {} while(0)
#endif

/**
 * \brief Create a socket to mISDNcapid
 * \return socket number
 */
static int misdnOpenSocket(void)
{
	struct sockaddr_un mcaddr;
	int nHandle;

	/* Create new socket */
	nHandle = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (nHandle < 0) {
		return -1;
	}
	mcaddr.sun_family = AF_UNIX;
	sprintf(mcaddr.sun_path, MISDN_CAPI_SOCKET_PATH);

	/* Connect socket to address */
	if (!connect(nHandle, (struct sockaddr *)&mcaddr, sizeof(mcaddr))) {
		/* no errors, return handle */
		return nHandle;
	}
	close(nHandle);
	return -1;
}

/**
 * \brief Send message to socket and wait for response
 * \param nHandle socket handle
 * \param pnBuffer data buffer pointer
 * \param nLen number of bytes to write from pnBuffer
 * \param nConf current configuration id
 * \return number of bytes read
 */
static int misdnRemoteCommand(int nHandle, unsigned char *pnBuffer, int nLen, int nConf)
{
	struct pollfd mypoll;
	int ret;

	/* write message to socket */
	ret = send(nHandle, pnBuffer, nLen, 0);
	if (ret != nLen)
		return -1;

	mypoll.fd = nHandle;
	mypoll.events = POLLIN | POLLPRI;
	/* wait max 1 sec for a answer */
	ret = poll(&mypoll, 1, 1000);
	if (ret < 1)
		return -2;
	/* read data */
	ret = recv(nHandle, pnBuffer, 1024, 0);
	return ret;
}

/**
 * \brief Add standard misdn header to buffer pointer
 * \param ppnPtr data buffer pointer
 * \param nLen length of message
 * \param nCmd command id
 */
static void misdnSetHeader(unsigned char *p, _cword nLen, _cword AppId, _cword nCmd, _cword Contr)
{

	CAPIMSG_SETLEN(p, nLen);
	CAPIMSG_SETAPPID(p, AppId);
	capimsg_setu8(p, 4, nCmd >> 8);
	capimsg_setu8(p, 5, nCmd & 0xff);
	capimsg_setu16(p, 8, Contr);
}

#if 0
/**
 * \brief Debug purpose, write capi data to file
 * \param nSend send
 * \param pnBuffer data buffer
 * \param nLength length of buffer
 * \param nDataMsg data message len
 */
static void misdnWriteCapiTrace(int nSend, unsigned char *pnBuffer, int nLength, int nDataMsg)
{
	int nHandle;
	_cdword nTime;
	unsigned char anHeader[7];
	char *pnTraceFile = getTraceFile();

	if (strlen(pnTraceFile) <= 0) {
		return;
	}

	if (getTraceLevel() < (nDataMsg + 1)) {
		return;
	}

	nHandle = open(pnTraceFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (nHandle >= 0) {
		nTime = (_cdword) time(NULL);
		capimsg_setu16(anHeader, 0, nLength + sizeof(anHeader));
		capimsg_setu32(anHeader, 2, nTime);
		anHeader[6] = (nSend) ? 0x80 : 0x81;
		close(nHandle);
	}
}
#endif


/**
 * \brief Check if misdn interface is available
 * \return file descriptor of socket, or error code
 */
static unsigned misdnIsInstalled(void)
{
	unsigned nHandle;

	nHandle = misdnOpenSocket();
#ifdef MISDND_CAPI_MODULE_DEBUG
	if (nHandle >= 0 && !mIm_debug) {
		int pid;
		pid = getpid();
		sprintf(mIm_debug_file, "/tmp/mIm_debug_%05d.log", pid);
		mIm_debug = fopen(mIm_debug_file, "wt");
	}
#endif
	return nHandle;
}

/**
 * \brief Register at misdn
 * \param nMaxB3Connection maximum b3 connection
 * \param nMaxB3Blks maximum b3 blocks
 * \param nMaxSizeB3 maximum b3 size
 * \param pnApplId pointer where we store the new application id
 * \return new socket handle
 */
static unsigned misdnRegister(unsigned nMaxB3Connection, unsigned nMaxB3Blks, unsigned nMaxSizeB3, unsigned *pnApplId)
{
	unsigned char anBuf[100];
	int nSock, ret;
	uint16_t ApplId;

	*pnApplId = -1;

	/* open a new socket for communication */
	nSock = misdnOpenSocket();
	if (nSock < 0)
		return nSock;

	ApplId = capi_alloc_applid(nSock);

	misdnSetHeader(anBuf, 20, ApplId, MIC_REGISTER_REQ, 0);
	
	capimsg_setu32(anBuf, 8, nMaxB3Connection);
	capimsg_setu32(anBuf, 12, nMaxB3Blks);
	capimsg_setu32(anBuf, 16, nMaxSizeB3);

	/* Send message to socket and wait for answer */
	ret = misdnRemoteCommand(nSock, anBuf, 20, MIC_REGISTER_REQ);
	if (ret != 10) {
		close(nSock);
		return -2;
	}
	ret = CAPIMSG_U16(anBuf, 8);
	if (ret == CapiNoError) {
		/* No error set it to pnApplId */
		*pnApplId = ApplId;
		mId_print("%s: fd=%d ApplId=%d\n", __func__, nSock, ApplId);
	} else {
		/* error occured, close socket give back ApplId and return -1 */
		capi_freeapplid(ApplId);
		close(nSock);
		nSock = -1;
	}
	return nSock;
}

/**
 * \brief Put capi message to misdn
 * \param nSock socket handle
 * \param nApplId application id
 * \param pnMsg message pointer
 * \return error code
 */
static unsigned misdnPutMessage(int nSock, unsigned nApplId, unsigned char *pnMsg)
{
	int nLen = CAPIMSG_LEN(pnMsg);
	int nCommand = CAPIMSG_COMMAND(pnMsg);
	int nSubCommand = CAPIMSG_SUBCOMMAND(pnMsg);
	int ret, tot, dlen = 0;
#ifdef MISDND_CAPI_MODULE_DEBUG
	uint8_t d = 0;
#endif
	uint16_t dh;
	void *dp = NULL;
	struct msghdr 	msg;
	struct iovec	iv[2];
	

	if (nCommand == CAPI_DATA_B3) {
		if (nSubCommand == CAPI_REQ) {
			dlen = CAPIMSG_DATALEN(pnMsg);
			if (sizeof(dp) == 4)
				dp = (void *)((unsigned long)CAPIMSG_U32(pnMsg, 12));
			else
				dp = (void *)((unsigned long)CAPIMSG_U64(pnMsg, 22));
#ifdef MISDND_CAPI_MODULE_DEBUG
			d = *((unsigned char *)dp);
#endif
			iv[0].iov_base = pnMsg;
			iv[0].iov_len = nLen;
			iv[1].iov_base = dp;
			iv[1].iov_len = dlen;
			tot = dlen + nLen;
			memset(&msg, 0, sizeof(msg));
			msg.msg_iov = iv;
			msg.msg_iovlen = 2;
			ret = sendmsg(nSock, &msg, 0);
		} else if (CAPI_RESP) {
			dh = CAPIMSG_U16(pnMsg, 12);
			dh = capi_return_buffer(nApplId, dh);
			capimsg_setu16(pnMsg, 12, dh);
			ret = send(nSock, pnMsg, nLen, 0);
			tot = nLen;
		}
	} else {
		ret = send(nSock, pnMsg, nLen, 0);
		tot = nLen;
	}
	mId_print("%s: %s fd=%d len=%d dp=%p dlen=%d tot=%d d=%02x ret=%d (%d - %s)\n", __func__, capi20_cmd2str(nCommand, nSubCommand),
		nSock, nLen, dp, dlen, tot, d, ret, errno, strerror(errno));
	if (tot != ret) {
		ret = CapiMsgOSResourceErr;
	} else
		ret = CapiNoError;
	return ret;
}

/**
 * \brief Get message from misdn
 * \param nSock socket handle
 * \param nApplId application id
 * \param ppnBuffer pointer to data buffer pointer (where we store the data)
 * \return error code
 */
static unsigned misdnGetMessage(int nSock, unsigned nApplId, unsigned char **ppnBuffer)
{
	unsigned char *pnBuffer;
	unsigned nOffset;
	size_t nBufSize;
	int nRet;
	uint16_t ml;
	unsigned long nData;

	/* try to get a new buffer from queue */
	if ((*ppnBuffer = pnBuffer = (unsigned char *)capi_get_buffer(nApplId, &nBufSize, &nOffset)) == 0) {
		mId_print("%s: no pnBuffer\n", __func__);
		return CapiMsgOSResourceErr;
	}

	/* Get message */
	nRet = recv(nSock, pnBuffer, nBufSize, MSG_DONTWAIT);

	if (nRet > 0) {
		/* DATA_B3? Then set buffer address */
		if ((CAPIMSG_COMMAND(pnBuffer) == CAPI_DATA_B3) && (CAPIMSG_SUBCOMMAND(pnBuffer) == CAPI_IND)) {
			capi_save_datahandle(nApplId, nOffset, CAPIMSG_U16(pnBuffer, 18), CAPIMSG_U32(pnBuffer, 8));
			/* patch datahandle */
			capimsg_setu16(pnBuffer, 18, nOffset);
			ml = CAPIMSG_LEN(pnBuffer);
			nData = (unsigned long) pnBuffer + ml;
			if (sizeof(void *) == 4) {
				pnBuffer[12] = nData & 0xFF;
				pnBuffer[13] = (nData >> 8) & 0xFF;
				pnBuffer[14] = (nData >> 16) & 0xFF;
				pnBuffer[15] = (nData >> 24) & 0xFF;
				ml = 22;
			} else {
				capimsg_setu32(pnBuffer, 12, 0);
				capimsg_setu64(pnBuffer, 22, nData);
				ml = 30;
			}
			CAPIMSG_SETLEN(pnBuffer, ml);
			/* keep buffer */
			return CapiNoError;
		}

		/* buffer is not needed, return it */
		capi_return_buffer(nApplId, nOffset);
		return CapiNoError;
	}

	capi_return_buffer(nApplId, nOffset);

	if (nRet == 0) {
		return CapiReceiveQueueEmpty;
	}

	switch (errno) {
	case EMSGSIZE:
		nRet = CapiIllCmdOrSubcmdOrMsgToSmall;
		break;
	case EAGAIN:
		nRet = CapiReceiveQueueEmpty;
		break;
	default:
		nRet = CapiMsgOSResourceErr;
		break;
	}

	return nRet;
}

/**
 * \brief Get manufactor informations
 * \param nHandle socket handle
 * \param nController controller id
 * \param pnBuffer buffer pointer we write our informations to
 * \return pnBuffer
 */
static unsigned char *misdnGetManufactor(int nHandle, unsigned nController, unsigned char *pnBuffer)
{
	unsigned char anBuf[100];
	int ret;

	misdnSetHeader(anBuf, 10, 0, MIC_GET_MANUFACTURER_REQ, nController);
	ret = misdnRemoteCommand(nHandle, anBuf, 10, MIC_GET_MANUFACTURER_REQ);
	if (ret == 74)
		memcpy(pnBuffer, &anBuf[10], 64);
	else
		memset(pnBuffer, 0, 64);
	return pnBuffer;
}

/**
 * \brief Get version informations
 * \param nHandle socket handle
 * \param nController controller id
 * \param pnBuffer buffer pointer we write our informations to
 * \return pnBuffer
 */
static unsigned char *misdnGetVersion(int nHandle, unsigned nController, unsigned char *pnBuffer)
{
	unsigned char anBuf[100];
	int ret;

	misdnSetHeader(anBuf, 10, 0, MIC_VERSION_REQ, nController);
	ret = misdnRemoteCommand(nHandle, anBuf, 10, MIC_VERSION_REQ);
	if (ret == 26)
		memcpy(pnBuffer, &anBuf[10], 16);
	else
		memset(pnBuffer, 0, 16);
	return pnBuffer;
}

/**
 * \brief Get serial number informations
 * \param nHandle socket handle
 * \param nController controller id
 * \param pnBuffer buffer pointer we write our informations to
 * \return pnBuffer
 */
static unsigned char *misdnGetSerialNumber(int nHandle, unsigned nController, unsigned char *pnBuffer)
{
	unsigned char anBuf[100];
	int ret;

	misdnSetHeader(anBuf, 10, 0, MIC_SERIAL_NUMBER_REQ, nController);
	ret = misdnRemoteCommand(nHandle, anBuf, 10, MIC_SERIAL_NUMBER_REQ);
	*pnBuffer = 0;
	if (ret == 18)
		memcpy(pnBuffer, &anBuf[10], 8);
	return pnBuffer;
}

/**
 * \brief Get profile from fritzbox
 * \param nHandle socket handle
 * \param nControllerId controller
 * \param pnBuf buffer
 * \return error code
 */
static unsigned misdnGetProfile(int nHandle, unsigned nController, unsigned char *pnBuf)
{
	unsigned char anBuf[100];
	uint16_t err;
	int ret;

	misdnSetHeader(anBuf, 10, 0, MIC_GET_PROFILE_REQ, nController);
	ret = misdnRemoteCommand(nHandle, anBuf, 10, MIC_GET_PROFILE_REQ);
	
	if (ret != 74)
		return CapiMsgOSResourceErr;

	err = CAPIMSG_U16(anBuf, 8);
	if (err == CapiNoError) {
		/* Important !!! Only copy 2 bytes if the number of controllers is requested */
		if (nController)
			memcpy(pnBuf, &anBuf[10], 64);
		else
			memcpy(pnBuf, &anBuf[10], 2);
	}
	return err;
}

static int misdnFlagReq(uint16_t ApplId, uint32_t set_f, uint32_t clr_f)
{ 
	unsigned char anBuf[100];
	int ret, fd;

	fd = capi_applid2fd(ApplId);
	if (fd < 0)
		return -1;
	misdnSetHeader(anBuf, 16, ApplId, MIC_USERFLAG_REQ, 0);
	capimsg_setu32(anBuf, 8, set_f);
	capimsg_setu32(anBuf, 12, clr_f);
	ret = misdnRemoteCommand(fd, anBuf, 16, MIC_USERFLAG_REQ);
	if (ret == 12)
		ret = CAPIMSG_U32(anBuf, 8);
	else
		ret = -1;
	return ret;
}

static int misdnGetFlags(unsigned nApplId, unsigned *pnFlagsPtr)
{
	int ret;

	ret = misdnFlagReq(nApplId, 0, 0);
	if (ret < 0)
		*pnFlagsPtr = 0;
	else {
		*pnFlagsPtr = ret;
		ret = 0;
	}
	return ret;
}

static int misdnSetFlags(unsigned nApplId, unsigned nFlags)
{
	int ret;

	ret = misdnFlagReq(nApplId, nFlags, 0);
	if (ret >= 0)
		ret = 0;
	return ret;
}

static int misdnClearFlags(unsigned nApplId, unsigned nFlags)
{
	int ret;

	ret = misdnFlagReq(nApplId, 0, nFlags);
	if (ret >= 0)
		ret = 0;
	return ret;
}

static char *misdnGetTtyDeviceName(unsigned nApplId,unsigned nNcci, char *pnBuffer, size_t nSize)
{
	unsigned char *anBuf;
	int ret, fd;

	fd = capi_applid2fd(nApplId);
	if (fd < 0)
		return NULL;

	if (nSize > 64)
		nSize = 64;
	anBuf = malloc(nSize + 12);
	if (!anBuf)
		return NULL;
	misdnSetHeader(anBuf, 16, nApplId, MIC_TTYNAME_REQ, 0);
	capimsg_setu32(anBuf, 8, nNcci);
	capimsg_setu32(anBuf, 12, nSize & 0xff);
	ret = misdnRemoteCommand(fd, anBuf, 16, MIC_TTYNAME_REQ);
	if (ret > 8) {
		ret = ret - 8;
		memcpy(pnBuffer, &anBuf[8],  ret);
		pnBuffer[ret] = 0;
	} else
		return NULL;
	free(anBuf);
	return pnBuffer;
}

/** Module operations structure */
static struct sModuleOperations sRemoteCapi = {
	misdnIsInstalled,
	misdnRegister,
	NULL,
	misdnPutMessage,
	misdnGetMessage,
	misdnGetManufactor,
	misdnGetVersion,
	misdnGetSerialNumber,
	misdnGetProfile,
	NULL,
	misdnGetFlags,
	misdnSetFlags,
	misdnClearFlags,
	misdnGetTtyDeviceName,
	NULL,
	NULL
};

MODULE_INIT("misdn", &sRemoteCapi);
