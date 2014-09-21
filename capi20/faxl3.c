/*
 * faxl3.c
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

#include "m_capi.h"
#include "mc_buffer.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <mISDN/q931.h>
#include "ncci.h"
#include "alaw.h"
#include "sff.h"
#include "g3_mh.h"

#ifdef USE_SOFTFAX
#include <spandsp.h>

struct mFAX {
	struct mCAPIobj		cobj;
	struct BInstance	*binst;
	struct lPLCI		*lplci;
	unsigned int		outgoing:1;
	unsigned int		modem_active:1;
	unsigned int		modem_end:1;
	unsigned int		startdownlink:1;
	unsigned int		b3transfer_active:1;
	unsigned int		b3transfer_end:1;
	unsigned int		extended:1;
	unsigned int		high_res:1;
	unsigned int		polling:1;
	unsigned int		more_doc:1;
	unsigned int		no_ecm:1;
	unsigned int		b3data_mapped:1;
	unsigned int		B3DisconnectDone:1;
	unsigned char		phase; /* A B C D E */
	int			b3transfer_error;
	int			b3_format;
	unsigned char		*b3data;
	size_t			b3data_size;
	size_t			b3data_pos;
	struct sff_state	sff;
	char			file_name[120];
	int			file_d;
	char			StationID[24];
	char			RemoteID[24];
	char			HeaderInfo[128];
	int			compressions;
	int			resolutions;
	int			image_sizes;
	int			rate;
	int			modems;
	fax_state_t		*fax;
	t30_state_t		*t30;
	logging_state_t		*logging;
	t30_stats_t		t30stats;
	int			phE_res;
};
#ifdef MISDN_CAPI_REFCOUNT_DEBUG
#define get_faxncci(f)	__get_faxncci(f, __FILE__, __LINE__)
#define put_ncci(n)	__put_ncci(n, __FILE__, __LINE__)
#define get_mFAX(f)	__get_mFAX(f, __FILE__, __LINE__)
#define put_mFAX(f)	__put_mFAX(f, __FILE__, __LINE__)

#define _Xget_cobj(c)	__get_cobj(c, file, lineno)
#define _Xput_cobj(c)   __put_cobj(c, file, lineno)
#else
#define _Xget_cobj(c)	get_cobj(c)
#define _Xput_cobj(c)   put_cobj(c)
#endif


#ifdef MISDN_CAPI_REFCOUNT_DEBUG
static struct mNCCI *__get_faxncci(struct mFAX *fax, const char *file, int lineno)
#else
static struct mNCCI *get_faxncci(struct mFAX *fax)
#endif
{
	struct mNCCI *ncci = NULL;
	struct mCAPIobj *co;

	if (fax && fax->cobj.parent) {
		co = _Xget_cobj(fax->cobj.parent);
		if (co)
			ncci = container_of(co, struct mNCCI, cobj);
	}
	return ncci;
}

#ifdef MISDN_CAPI_REFCOUNT_DEBUG
static int __put_ncci(struct mNCCI *ncci, const char *file, int lineno)
#else
static int put_ncci(struct mNCCI *ncci)
#endif
{
	int ret;

	if (ncci)
		ret = _Xput_cobj(&ncci->cobj);
	else
		ret = -EINVAL;
	return ret;
}

#ifdef MISDN_CAPI_REFCOUNT_DEBUG
static struct mFAX *__get_mFAX(struct mFAX *fax, const char *file, int lineno)
#else
static struct mFAX *get_mFAX(struct mFAX *fax)
#endif
{
	struct mCAPIobj *co;

	if (fax) {
		co = _Xget_cobj(&fax->cobj);
		if (!co)
			fax = NULL;
	}
	return fax;
}

#ifdef MISDN_CAPI_REFCOUNT_DEBUG
static int __put_mFAX(struct mFAX *fax, const char *file, int lineno)
#else
static int put_mFAX(struct mFAX *fax)
#endif
{
	int ret;

	if (fax)
		ret = _Xput_cobj(&fax->cobj);
	else
		ret = -EINVAL;
	return ret;
}

void dump_fax_status(struct BInstance *bi)
{
	struct mFAX *f = get_mFAX(bi->b3data);
	const char *ids;

	if (!f) {
		iprint("FaxCh[%d] no fax data assigned\n", bi->nr);
	} else {
		ids = CAPIobjIDstr(&f->cobj);
		iprint("%s: phase:%c %s modem:%sactive,%send startdownlink:%s polling:%s\n",
			ids, f->phase, f->outgoing ? "outgoing" : "incoming", f->modem_active ? "" : "not ",
			f->modem_end ? "" : "not ", f->startdownlink ? "yes" : "no", f->polling ? "yes" : "no");
		iprint("%s: modems:0x%x b3transfer:%sactive,%send B3Disconn:%sdone\n", ids,
			f->modems, f->b3transfer_active ? "" : "not ", f->b3transfer_end ? "" : "not ",
			f->B3DisconnectDone ? "" : "not ");

		iprint("%s: high_res:%s more_doc:%s ecm:%s extended:%s b3data:%smapped\n",
			ids, f->high_res ? "yes" : "no", f->more_doc ? "yes" : "no", f->no_ecm ? "no" : "yes",
			f->extended ? "yes" : "no", f->b3data_mapped ? "" : "not ");
		iprint("%s: b3transfer_error:%d b3_format:%d b3data_size:%zd b3data_pos:%zd\n", ids,
			f->b3transfer_error, f->b3_format, f->b3data_size, f->b3data_pos);
		iprint("%s: File:%s  fd: %d\n", ids, f->file_name, f->file_d);
		iprint("%s: StationID:%s RemoteID:%s HeaderInfo:%s\n", ids, f->StationID, f->RemoteID, f->HeaderInfo);
		iprint("%s: compressions:0x%x resolutions:0x%x image_sizes:0x%x rate:%d\n", ids,
			f->compressions, f->resolutions, f->image_sizes, f->rate);
		put_mFAX(f);
	}
}

static void StopDownLink(struct mFAX *fax, struct mNCCI *ncci)
{
	int i;
	struct mc_buf *mc;

	dprint(MIDEBUG_NCCI, "%s: StopDownLink phase:%c\n", CAPIobjIDstr(&fax->cobj), fax->phase);
	if (!ncci)
		return;
	pthread_mutex_lock(&ncci->lock);
	fax->modem_active = 0;
	for (i = 0; i < ncci->window; i++) {
		mc = ncci->xmit_handles[i].pkt;
		if (mc) {
			ncci->xmit_handles[i].pkt = NULL;
			ncci->xmit_handles[i].PktId = 0;
			free_mc_buf(mc);
		}
	}
	ncci->dlbusy = 0;
	ncci->oidx = 0;
	ncci->iidx = 0;
	if (ncci->BIlink) {
		ncci->BIlink->release_pending = 1;
		ncciL4L3(ncci, PH_DEACTIVATE_REQ, 0, 0, NULL, NULL);
	}
	pthread_mutex_unlock(&ncci->lock);
}

static _cbyte *CodeNCPI(struct mFAX *fax, struct mNCCI *ncci, uint16_t pages, int OnlyExtended)
{
	_cbyte l, *ncpi;
	uint16_t opt;

	if (!ncci)
		return NULL;
	if (OnlyExtended && !fax->extended) {
		ncpi = NULL;
	} else {
		l = strlen(fax->RemoteID);
		ncpi = calloc(1, l + 12);
		if (ncpi) {
			opt = fax->high_res ? 0x0001 : 0;
			if (fax->extended) {
				/* Bprotocol.B3 == 5 */
				opt |= fax->polling  ? 0x0002 : 0;
				opt |= fax->more_doc ? 0x0004 : 0;
				opt |= fax->no_ecm   ? 0x8000 : 0;
			}
			capimsg_setu16(ncpi, 1, fax->rate);
			capimsg_setu16(ncpi, 3, opt);
			capimsg_setu16(ncpi, 5, fax->b3_format);
			capimsg_setu16(ncpi, 7, pages);
			l = strlen(fax->RemoteID);
			ncpi[9] = l;
			strcpy((char *)&ncpi[10], fax->RemoteID);
			l += 9;
			ncpi[0] = l;
		} else
			eprint("%s: no memory for NCPI(%d bytes)\n", CAPIobjIDstr(&fax->cobj),  l + 12);
	}
	pthread_mutex_lock(&ncci->lock);
	if (ncci->ncpi)
		free(ncci->ncpi);
	ncci->ncpi = ncpi;
	pthread_mutex_unlock(&ncci->lock);
	return ncpi;
}

static void FaxB3Disconnect(struct mFAX *fax, struct mNCCI *ncci)
{
	struct mc_buf *mc;

	if (!fax || !ncci) {
		wprint("called with NULL fax struct\n");
		return;
	}
	dprint(MIDEBUG_NCCI, "%s: phase:%c B3DisconnectDone:%s b3transfer_active:%s\n",
		CAPIobjIDstr(&fax->cobj), fax->phase, fax->B3DisconnectDone ? "yes" : "no", fax->b3transfer_active ? "yes" : "no");
	if (fax->B3DisconnectDone || fax->b3transfer_active)
		return;

	if (fax->phase != 'E')
		fax->phase = 'e';

	mc = alloc_mc_buf();
	if (mc) {
		fax->B3DisconnectDone = 1;
		ncciCmsgHeader(ncci, mc, CAPI_DISCONNECT_B3, CAPI_IND);
		fax->rate = fax->t30stats.bit_rate & 0xffff;
		mc->cmsg.NCPI = CodeNCPI(fax, ncci, fax->t30stats.pages_in_file, 0);
		if (fax->b3transfer_error) {
			/* TODO error mapping */
			ncci->Reason_B3 = CapiProtocolErrorLayer1;
			mc->cmsg.Reason_B3 = CapiProtocolErrorLayer1;
		} else {
			ncci->Reason_B3 = 0;
			mc->cmsg.Reason_B3 = 0;
		}
		ncciB3Message(ncci, mc);
		free_mc_buf(mc);
	} else {
		eprint("No msg buffer\n");
		/* TODO error shutdown */
	}
}

static void FaxB3Ind(struct mFAX *fax, struct mNCCI *ncci)
{
	int ret, tot;
	uint16_t dlen, dh;
	unsigned char *dp;

	if (!ncci)
		return;
	pthread_mutex_lock(&ncci->lock);
	if (ncci->recv_handles[ncci->ridx] != 0) {
		pthread_mutex_unlock(&ncci->lock);
		wprint("%s: Send FaxInd queue full\n", CAPIobjIDstr(&fax->cobj));
		return;
	}
	tot = fax->b3data_size	- fax->b3data_pos;
	if (tot > MAX_DATA_SIZE)
		dlen = MAX_DATA_SIZE;
	else
		dlen = tot;
	dp = &fax->b3data[fax->b3data_pos];
	fax->b3data_pos += dlen;
	if (dlen == 0) {
		/* eof */
		fax->b3transfer_active = 0;
		fax->b3transfer_end = 1;
		fax->b3transfer_error = 0;
		pthread_mutex_unlock(&ncci->lock);
		return;
	}
	dh = ++ncci->BIlink->UpId;
	if (dh == 0)
		dh = ++ncci->BIlink->UpId;

	ncci->recv_handles[ncci->ridx] = dh;
	ncci->ridx++;
	if (ncci->ridx >= ncci->window)
		ncci->ridx = 0;

	CAPIMSG_SETMSGID(ncci->up_header, ncci->appl->MsgId++);
	CAPIMSG_SETDATALEN(ncci->up_header, dlen);
	capimsg_setu16(ncci->up_header, 18, dh);
	// FIXME FLAGS
	// capimsg_setu16(cm, 20, 0);

	ncci->up_iv[1].iov_len = dlen;
	ncci->up_iv[1].iov_base = dp;
	tot = dlen + CAPI_B3_DATA_IND_HEADER_SIZE;

	ret = sendmsg(ncci->appl->fd, &ncci->up_msg, MSG_DONTWAIT);

	pthread_mutex_unlock(&ncci->lock);
	if (ret != tot) {
		wprint("%s: : frame with %d + %d bytes only %d bytes are sent - %s\n",
			CAPIobjIDstr(&fax->cobj), dlen, CAPI_B3_DATA_IND_HEADER_SIZE, ret, strerror(errno));
	} else
		dprint(MIDEBUG_NCCI_DATA, "%s: phase:%c DATA_B3_IND with %d + %d bytes handle %d was sent ret %d\n",
			CAPIobjIDstr(&fax->cobj), fax->phase, CAPI_B3_DATA_IND_HEADER_SIZE, dlen, dh, ret);
}

static int FaxPrepareReceivedTiff(struct mFAX *fax)
{
	int ret, fd;
	struct stat fst;

	fd = open(fax->file_name, O_RDONLY);
	if (fd < 0) {
		fax->b3transfer_error = errno;
		wprint("%s: Cannot open received file %s - %s\n", CAPIobjIDstr(&fax->cobj), fax->file_name,
			strerror(fax->b3transfer_error));
		return -fax->b3transfer_error;
	}
	ret = fstat(fd, &fst);
	if (ret) {
		fax->b3transfer_error = errno;
		wprint("%s: Cannot fstat received file %s - %s\n", CAPIobjIDstr(&fax->cobj), fax->file_name,
			strerror(fax->b3transfer_error));
		close(fd);
		return -fax->b3transfer_error;
	}
	fax->b3data_size = fst.st_size;
	fax->b3data = mmap(NULL, fax->b3data_size, PROT_READ, MAP_FILE, fd, 0);
	if (!fax->b3data) {
		fax->b3transfer_error = errno;
		wprint("%s: Cannot map received file %s - %s\n", CAPIobjIDstr(&fax->cobj), fax->file_name,
			strerror(fax->b3transfer_error));
		close(fd);
		return -fax->b3transfer_error;
	}
	fax->b3data_mapped = 1;
	close(fd);
	return 0;
}

static void FaxPrepareReceivedData(struct mFAX *fax, struct mNCCI *ncci)
{
	int ret;

	switch (fax->b3_format) {
	case FAX_B3_FORMAT_SFF:
		ret = SFF_ReadTiff(&fax->sff, fax->file_name);
		if (ret >= 0) {
			fax->b3data = fax->sff.data;
			fax->b3data_size = fax->sff.size;
		}
		break;
	case FAX_B3_FORMAT_TIFF:
		ret = FaxPrepareReceivedTiff(fax);
		break;
	default:
		eprint("%s: B3 data format %d not supported\n", CAPIobjIDstr(&fax->cobj), fax->b3_format);
		ret = -1;
		break;
	}
	if (ret < 0)
		FaxB3Disconnect(fax, ncci);
	else {
		fax->b3transfer_active = 1;
		fax->b3data_pos = 0;
		FaxB3Ind(fax, ncci);
	}
}

static void FaxConnectActiveB3(struct mFAX *fax, struct mNCCI *ncci, struct mc_buf *mc) {
	struct mc_buf *lmc = mc;

	if (!ncci)
		return;
	if (!mc)
		lmc = alloc_mc_buf();

	ncciCmsgHeader(ncci, lmc, CAPI_CONNECT_B3_ACTIVE, CAPI_IND);
	if (!fax->outgoing)
		fax->phase = 'C';
	lmc->cmsg.NCPI = CodeNCPI(fax, ncci, 0, 1);

	ncciB3Message(ncci, lmc);
	if (!mc)
		free_mc_buf(lmc);
}

static void rt_frame_handler(t30_state_t *t30, void *user_data, int direction, const uint8_t *msg, int len)
{
	struct mFAX *fax = get_mFAX(user_data);
	const char *ids;

	if (!fax) {
		eprint("Unsuccessful get_mFAX()\n");
		return;
	}
	ids = CAPIobjIDstr(&fax->cobj);
	dprint(MIDEBUG_NCCI, "%s: phase:%c RT handler direction %d msg %p len %d\n",
		ids, fax->phase, direction, msg, len);
	dhexprint(MIDEBUG_NCCI_DATA, (char *)ids, (unsigned char *)msg, len);
	put_mFAX(fax);
}

static int phaseB_handler(t30_state_t *t30, void *user_data, int result)
{
	const char *ident = "no T30";
	struct mc_buf *mc;
	int ret;
	struct mISDN_ctrl_req creq;
	struct mNCCI *ncci;
	struct mFAX *fax = get_mFAX(user_data);


	if (!fax) {
		eprint("Unsuccessful get_mFAX()\n");
		return 0;
	}
	if (fax->outgoing)
		ident = t30_get_tx_ident(t30);
	else
		ident = t30_get_rx_ident(t30);
	if (!ident)
		ident = "no ident";
	switch (result) {
	case T30_DIS:
		ident = t30_get_rx_ident(t30);
		if (!ident)
			ident = "no ident";
		dprint(MIDEBUG_NCCI, "%s: PhaseB DIS %s\n", CAPIobjIDstr(&fax->cobj), ident);
		strncpy(fax->RemoteID, ident, 20);
		if (fax->extended) {
			ncci = get_faxncci(fax);
			FaxConnectActiveB3(fax, ncci, NULL);
			if (fax->binst) {
				fax->binst->waiting = 1;
				/* Send silence when Underrun */
				creq.op = MISDN_CTRL_FILL_EMPTY;
				creq.channel = fax->binst->nr;
				creq.p1 = 1;
				creq.p2 = 0;
				creq.p2 |= 0xff & slin2alaw[0];
				creq.unused = 0;
				ret = ioctl(fax->binst->fd, IMCTRLREQ, &creq);
				/* MISDN_CTRL_FILL_EMPTY is not mandatory warn if not supported */
				if (ret < 0)
					wprint("Error on MISDN_CTRL_FILL_EMPTY ioctl - %s\n", strerror(errno));
				creq.op = MISDN_CTRL_RX_OFF;
				creq.channel = fax->binst->nr;
				creq.p1 = 1;
				creq.p2 = 0;
				creq.unused = 0;
				ret = ioctl(fax->binst->fd, IMCTRLREQ, &creq);
				/* RX OFF is not mandatory warn if not supported */
				if (ret < 0)
					wprint("Error on MISDN_CTRL_RX_OFF ioctl - %s\n", strerror(errno));
				while ((ret = sem_wait(&fax->binst->wait))) {
					wprint("sem_wait - %s\n", strerror(errno));
				}
				if (fax->binst) {
					fax->binst->waiting = 0;
					creq.op = MISDN_CTRL_RX_OFF;
					creq.channel = fax->binst->nr;
					creq.p1 = 0;
					creq.p2 = 0;
					creq.unused = 0;
					ret = ioctl(fax->binst->fd, IMCTRLREQ, &creq);
					if (ret < 0)
						wprint("Error on MISDN_CTRL_RX_OFF ioctl - %s\n", strerror(errno));
					else
						dprint(MIDEBUG_NCCI, "%s: Dropped %d bytes during waiting\n",
							CAPIobjIDstr(&fax->cobj), creq.p2);
					creq.op = MISDN_CTRL_FILL_EMPTY;
					creq.channel = fax->binst->nr;
					creq.p1 = 0;
					creq.p2 = 0;
					creq.p2 = -1;
					creq.unused = 0;
					ret = ioctl(fax->binst->fd, IMCTRLREQ, &creq);
					/* MISDN_CTRL_FILL_EMPTY is not mandatory warn if not supported */
					if (ret < 0)
						wprint("%s: Error on MISDN_CTRL_FILL_EMPTY ioctl - %s\n",
							CAPIobjIDstr(&fax->cobj), strerror(errno));
				}
			}
			put_ncci(ncci);
			dprint(MIDEBUG_NCCI, "%s: PhaseB wait resumed %05d (%05d)\n", CAPIobjIDstr(&fax->cobj),
				gettid(), fax->binst->tid);
			fax->phase = 'C';
		}
		break;
	case T30_DCN:
		dprint(MIDEBUG_NCCI, "NCCI %s: PhaseB DCN %s\n", CAPIobjIDstr(&fax->cobj), ident);
		break;
	case T30_DCS:
	case T30_DCS | 0x01:
		dprint(MIDEBUG_NCCI, "NCCI %s: PhaseB DCS(%x) %s\n", CAPIobjIDstr(&fax->cobj), result, ident);
		strncpy(fax->RemoteID, ident, 20);
		if (!fax->outgoing) {
			ncci = get_faxncci(fax);
			if (ncci) {
				if (ncci->ncci_m.state == ST_NCCI_N_0) {
					mc = alloc_mc_buf();
					if (mc) {
						ncciCmsgHeader(ncci, mc, CAPI_CONNECT_B3, CAPI_IND);
						mc->cmsg.NCPI = CodeNCPI(fax, ncci, 0, 1);
						ncciB3Message(ncci, mc);
						free_mc_buf(mc);
					} else
						eprint("No msg buffer\n");
				} else
					wprint("%s: Received second DCS in state %s - ignored\n",
						CAPIobjIDstr(&fax->cobj), _mi_ncci_st2str(ncci));
				put_ncci(ncci);
			} else
				wprint("%s: NCCI gone - no CAPI_CONNECT_B3 sent\n", CAPIobjIDstr(&fax->cobj));
		}
		break;
	default:
		dprint(MIDEBUG_NCCI, "%s: PhaseB called result 0x%02x :%s: ident %s\n", CAPIobjIDstr(&fax->cobj),
			result, t30_frametype(result), ident);
		break;
	}
	put_mFAX(fax);
	return 0;
}

static int phaseD_handler(t30_state_t *t30, void *user_data, int result)
{
	struct mFAX *fax = get_mFAX(user_data);

	if (!fax) {
		eprint("Unsuccessful get_mFAX()\n");
		return 0;
	}
	fax->phase = 'D';
	dprint(MIDEBUG_NCCI, "%s: PhaseD called result 0x%02x :%s:\n", CAPIobjIDstr(&fax->cobj),
		result, t30_frametype(result));
	switch(result) {
	case T30_MPS:
	case T30_MPS | 1:
	case T30_MCF:
	case T30_MCF | 1:
		fax->phase = 'C';
		break;
	default:
		break;
	}
	put_mFAX(fax);
	return 0;
}

static void phaseE_handler(t30_state_t *t30, void *user_data, int result)
{
	const char *ids;
	struct mFAX *fax = get_mFAX(user_data);
	struct mNCCI *ncci;
	struct mc_buf *mc;

	if (!fax) {
		eprint("Unsuccessful get_mFAX()\n");
		return;
	}
	fax->phase = 'E';
	dprint(MIDEBUG_NCCI, "%s: PhaseE called result 0x%02x\n", CAPIobjIDstr(&fax->cobj), result);
	fax->phE_res = result;
	t30_get_transfer_statistics(t30, &fax->t30stats);
	ncci = get_faxncci(fax);
	if (ncci && result) {
		if ((ncci->ncci_m.state == ST_NCCI_N_0) && !ncci->cobj.cleaned) {
			/* Never left state N0 , so we need indicate a call first */
			fax->b3transfer_error = 1;
			mc = alloc_mc_buf();
			if (mc) {
				ncciCmsgHeader(ncci, mc, CAPI_CONNECT_B3, CAPI_IND);
				mc->cmsg.NCPI = NULL; /* default - we do not have info */
				ncciB3Message(ncci, mc);
				free_mc_buf(mc);
			} else
				eprint("No msg buffer\n");
		}
	}
	StopDownLink(fax, ncci);
	ids = CAPIobjIDstr(&fax->cobj);
	dprint(MIDEBUG_NCCI, "%s: BitRate %d\n", ids, fax->t30stats.bit_rate);
	dprint(MIDEBUG_NCCI, "%s: Pages RX: %d   TX: %d InFile: %d\n", ids,
		fax->t30stats.pages_rx, fax->t30stats.pages_tx, fax->t30stats.pages_in_file);
	dprint(MIDEBUG_NCCI, "%s: Resolution X: %d    Y: %d\n", ids,
		fax->t30stats.x_resolution, fax->t30stats.y_resolution);
	dprint(MIDEBUG_NCCI, "%s: Width: %d  Length: %d\n", ids, fax->t30stats.width, fax->t30stats.length);
	dprint(MIDEBUG_NCCI, "%s: Size: %d  Encoding: %d\n", ids,
		fax->t30stats.image_size, fax->t30stats.encoding);
	dprint(MIDEBUG_NCCI, "%s: Bad Rows: %d longest: %d\n", ids,
		fax->t30stats.bad_rows, fax->t30stats.longest_bad_row_run);
	dprint(MIDEBUG_NCCI, "%s: ECM Retries:%d  Status: %d\n", ids,
		fax->t30stats.error_correcting_mode_retries, fax->t30stats.current_status);
	if (result == 0) {
		if (fax->outgoing) {
			FaxB3Disconnect(fax, ncci);
		} else {
			FaxPrepareReceivedData(fax, ncci);
		}
	} else {
		fax->b3transfer_error = 1;
	}
	put_ncci(ncci);
	put_mFAX(fax);
}

static void spandsp_msg_log(int level, const char *text)
{
	dprint(MIDEBUG_NCCI_DATA, "Spandsp: L%d, %s", level, text);
}

static int InitFax(struct mFAX *fax)
{
	int ret = 0, l;
	unsigned char *p;

	fax->fax = fax_init(fax->fax, fax->outgoing);
	if (fax->fax) {
		fax_set_transmit_on_idle(fax->fax, TRUE);
		fax_set_tep_mode(fax->fax, TRUE);
		fax->t30 = fax_get_t30_state(fax->fax);

		// Supported compressions
		fax->compressions |= T30_SUPPORT_NO_COMPRESSION;
		fax->compressions |= T30_SUPPORT_T4_1D_COMPRESSION;
		fax->compressions |= T30_SUPPORT_T4_2D_COMPRESSION;
		fax->compressions |= T30_SUPPORT_T6_COMPRESSION;

		// Supported resolutions
		fax->resolutions |= T30_SUPPORT_STANDARD_RESOLUTION;
		fax->resolutions |= T30_SUPPORT_FINE_RESOLUTION;
		fax->resolutions |= T30_SUPPORT_SUPERFINE_RESOLUTION;
		fax->resolutions |= T30_SUPPORT_R4_RESOLUTION;
		fax->resolutions |= T30_SUPPORT_R8_RESOLUTION;
		fax->resolutions |= T30_SUPPORT_R16_RESOLUTION;
		fax->resolutions |= T30_SUPPORT_300_300_RESOLUTION;
		fax->resolutions |= T30_SUPPORT_400_400_RESOLUTION;
		fax->resolutions |= T30_SUPPORT_600_600_RESOLUTION;
		fax->resolutions |= T30_SUPPORT_1200_1200_RESOLUTION;
		fax->resolutions |= T30_SUPPORT_300_600_RESOLUTION;
		fax->resolutions |= T30_SUPPORT_400_800_RESOLUTION;
		fax->resolutions |= T30_SUPPORT_600_1200_RESOLUTION;

		// Supported image sizes
		fax->image_sizes |= T30_SUPPORT_215MM_WIDTH;
		fax->image_sizes |= T30_SUPPORT_255MM_WIDTH;
		fax->image_sizes |= T30_SUPPORT_303MM_WIDTH;
		fax->image_sizes |= T30_SUPPORT_UNLIMITED_LENGTH;
		fax->image_sizes |= T30_SUPPORT_A4_LENGTH;
		fax->image_sizes |= T30_SUPPORT_B4_LENGTH;
		fax->image_sizes |= T30_SUPPORT_US_LETTER_LENGTH;
		fax->image_sizes |= T30_SUPPORT_US_LEGAL_LENGTH;

		// Supported modems
		switch (fax->rate) { /* maximum rate 0 adaptiv */
		case 0:
		case 14400:
		case 12000:
			fax->modems |= T30_SUPPORT_V17;
		case 9600:
		case 7200:
			fax->modems |= T30_SUPPORT_V29;
		case 4800:
			fax->modems |= T30_SUPPORT_V27TER;
			break;
		default: /* wrong code - should not happen */
			eprint("Fax wrong bitrate %d, fallback to all\n", fax->rate);
			fax->modems = T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17;
		}

		// Error correction
		if (!fax->no_ecm)
			t30_set_ecm_capability(fax->t30, TRUE);

		t30_set_supported_compressions(fax->t30, fax->compressions);
		t30_set_supported_resolutions(fax->t30, fax->resolutions);
		t30_set_supported_image_sizes(fax->t30, fax->image_sizes);
		t30_set_supported_modems(fax->t30, fax->modems);

		// spandsp loglevel
		fax->logging = t30_get_logging_state(fax->t30);
		span_log_set_level(fax->logging, 0xFFFFFF);
		span_log_set_message_handler(fax->logging, spandsp_msg_log);

		if (fax->lplci->Bprotocol.B3cfg[0]) {
			l = fax->lplci->Bprotocol.B3cfg[5];
			p = &fax->lplci->Bprotocol.B3cfg[6];
			if (l) {
				if (l > 20)
					l = 20;
				memcpy(fax->StationID, p, l);
				fax->StationID[l] = 0;
				t30_set_tx_ident(fax->t30, fax->StationID);
			} else
				fax->StationID[0] = 0;
			l = fax->lplci->Bprotocol.B3cfg[5];
			p += l;
			l = *p++;
			if (l) {
				if (l > 100)
					l = 100;
				memcpy(fax->HeaderInfo, p, l);
				fax->HeaderInfo[l] = 0;
				t30_set_tx_page_header_info(fax->t30, fax->HeaderInfo);
			} else
				fax->HeaderInfo[0] = 0;
		}
		dprint(MIDEBUG_NCCI, "Ident:%s Header: %s\n", fax->StationID, fax->HeaderInfo);

		if (fax->outgoing) {
			if (!fax->extended)
				t30_set_tx_file(fax->t30, fax->file_name, -1, -1);
		} else
			t30_set_rx_file(fax->t30, fax->file_name, -1);

		t30_set_phase_b_handler(fax->t30, phaseB_handler, fax);
		t30_set_phase_d_handler(fax->t30, phaseD_handler, fax);
		t30_set_phase_e_handler(fax->t30, phaseE_handler, fax);

		t30_set_real_time_frame_handler(fax->t30, rt_frame_handler, fax);
		ret = 0;
	} else {
		wprint("Cannot init spandsp fax\n");
		ret = -EINVAL;
	}
	return ret;
}

static struct mFAX *mFaxCreate(struct BInstance	*bi)
{
	struct mFAX *nf;
	int ret, val;
	time_t cur_time;
	struct mPLCI *plci;
	struct mNCCI *ncci;

	nf = bi->b3data;
	if (nf) {
		wprint("PLCI %04x: already created %s\n", bi->lp->cobj.id, CAPIobjIDstr(&nf->cobj));
		return nf;
	}
	nf = calloc(1, sizeof(*nf));
	if (nf) {
		nf->file_d = -2;
		ncci = ncciCreate(bi->lp);
		if (ncci) {
			nf->cobj.id2 = bi->nr;
			ret = init_cobj_registered(&nf->cobj, &ncci->cobj, Cot_FAX, 0x00ffffff);
			if (ret) {
				cleanup_ncci(ncci);
				put_cobj(&ncci->cobj);
				free(nf);
				return NULL;
			}
			nf->phase = 'A'; /* initial */
			nf->binst = bi;
			nf->lplci = bi->lp;
			plci = p4lPLCI(bi->lp);
			nf->outgoing = plci->outgoing;
			dprint(MIDEBUG_NCCI, "%s: create %s\n", CAPIobjIDstr(&ncci->cobj),
				nf->outgoing ? "outgoing" : "incoming");
			if (nf->lplci->Bprotocol.B1cfg[0])
				nf->rate = CAPIMSG_U16(nf->lplci->Bprotocol.B1cfg, 1);
			if (nf->lplci->Bprotocol.B3cfg[0]) {
				nf->b3_format = CAPIMSG_U16(nf->lplci->Bprotocol.B3cfg, 3);
				val = CAPIMSG_U16(nf->lplci->Bprotocol.B3cfg, 1);
				if (nf->lplci->Bprotocol.B3 == 4) {
					if (val)
						nf->high_res = 1;
				} else { /* 5 - extended */
					nf->extended = 1;
					if (val & 0x0001)
						nf->high_res = 1;
					if (val & 0x0002)
						nf->polling = 1;
					if (val & 0x8000)
						nf->no_ecm = 1;
				}
			}
			cur_time = time(NULL);
			sprintf(nf->file_name, "%s/Contr%d_ch%d_%lx.tif", TempDirectory,
				bi->pc->profile.ncontroller, bi->nr, (unsigned long)cur_time);
			if (!nf->outgoing) {
				ret = InitFax(nf);
				if (ret) {
					wprint("Cannot InitFax for incoming PLCI %04x\n", bi->lp ? bi->lp->cobj.id : 0xffff);
					cleanup_ncci(ncci);
					put_cobj(&ncci->cobj);
					free(nf);
					nf = NULL;
					return nf;
				}
			}
#ifdef HW_FIFO_STATUS_ON
			ncciL4L3(ncci, PH_CONTROL_REQ, HW_FIFO_STATUS_ON, 0, NULL, NULL);
#endif
			nf->startdownlink = 1;
			nf->modem_active = 1;
			put_ncci(ncci);
		} else {
			eprint("Cannot create NCCI for PLCI %04x\n", bi->lp ? bi->lp->cobj.id : 0xffff);
			free(nf);
			nf = NULL;
		}
	}
	bi->b3data = get_mFAX(nf);
	return nf;
}

void Free_Faxobject(struct mCAPIobj *co)
{
	struct mFAX *fax;
	int ret;

	fax = container_of(co, struct mFAX, cobj);
	dprint(MIDEBUG_NCCI, "%s: phase:%c free fax data structs:%s%s%s\n",
		CAPIobjIDstr(&fax->cobj), fax->phase,
		fax->fax ? " SPANDSP" : "",
		fax->sff.data ? " SFF data" : "",
		fax->b3data ? " B3" :"");

	if (fax->fax) {
		fax_free(fax->fax);
		fax->fax = NULL;
	}
	if (fax->sff.data) {
		dprint(MIDEBUG_NCCI, "%s: SFF data:%p fax->b3data %p\n",
			CAPIobjIDstr(&fax->cobj), fax->sff.data, fax->b3data);
		if (fax->sff.data && fax->b3data != fax->sff.data)
			free(fax->sff.data);
		fax->sff.data = NULL;
	}
	if (fax->b3data) {
		if (fax->b3data_mapped)
			munmap(fax->b3data, fax->b3data_size);
		else
			free(fax->b3data);
		fax->b3data = NULL;
		fax->b3data_mapped = 0;
	}
	if (!KeepTemporaryFiles) {
		ret = unlink(fax->file_name);
		if (ret)
			wprint("%s: Temporary file %s - not deleted because %s\n", CAPIobjIDstr(&fax->cobj),
				fax->file_name, strerror(errno));
		else
			dprint(MIDEBUG_NCCI, "%s: Removed temporary file %s\n", CAPIobjIDstr(&fax->cobj), fax->file_name);
	}
	if (co->parent) {
		if (co->parent->listhead)
			delist_cobj(co);
		put_cobj(co->parent);
	}
	free_capiobject(&fax->cobj, fax);
}


static void cleanup_Fax(struct mFAX *fax, struct mNCCI *ncci)
{
	if (fax->cobj.cleaned) {
		wprint("%s: already cleaned\n", CAPIobjIDstr(&fax->cobj));
		return;
	}

	dprint(MIDEBUG_NCCI, "%s: cleaning phase:%c\n",
		CAPIobjIDstr(&fax->cobj), fax->phase);

	fax->cobj.cleaned = 1;

	if (ncci)
		pthread_mutex_lock(&ncci->lock);
	fax->modem_active = 0;
	fax->modem_end = 1;
	if (ncci)
		pthread_mutex_unlock(&ncci->lock);
	if (ncci) {
		if (!fax->B3DisconnectDone) {
			fax->B3DisconnectDone = 1;
			ncciReleaseLink(ncci);
		}
		delist_cobj(&fax->cobj);
	}
}

static void mFaxRelease(struct mFAX *fax, struct mNCCI *ncci)
{
	dprint(MIDEBUG_NCCI, "%s: phase:%c fax->binst %p b3data %p\n",
		CAPIobjIDstr(&fax->cobj), fax->phase, fax->binst, fax->binst ? fax->binst->b3data : NULL);
	if (fax->binst) {
		if (fax->binst->b3data) {
			fax->binst->b3data = NULL;
			put_mFAX(fax); /* b3data ref gone */
		}
	}
	cleanup_Fax(fax, ncci);
}

void FaxReleaseLink(struct BInstance *bi)
{
	struct mFAX *fax;
	struct mNCCI *ncci;

	if (!bi) {
		eprint("B instance gone\n");
		return;
	}
	fax = get_mFAX(bi->b3data);
	if (!fax) {
		eprint("No fax struct\n");
		return;
	}
	ncci = get_faxncci(fax);
	bi->b3data = NULL;
	put_mFAX(fax);	/* ref to b3data */
	mFaxRelease(fax, ncci);
	put_ncci(ncci);
	put_mFAX(fax);	/* ref from get_mFAX() */
}

static int DisconnectIndication(struct mFAX *fax, struct mNCCI *ncci, struct mc_buf *mc)
{
	int ret = 0;

	dprint(MIDEBUG_NCCI, "%s: phase:%c D-channel disconnect\n",
		CAPIobjIDstr(&fax->cobj), fax->phase);
	if (fax->cobj.cleaned)
		return ret; /* Do disconnect in lPLCI */

	if (ncci) {
		switch (fax->phase) {
		case 'A':
			ncci->Reason_B3 = CapiConnectionNoSuccess_noG3;
			break;
		case 'B':
			if (fax->RemoteID[0]) {
				ncci->Reason_B3 = CapiConnectionNoSuccess_TrainingErr;
				CodeNCPI(fax, ncci, 0, 0);
			} else
				ncci->Reason_B3 = CapiConnectionNoSuccess_noG3;
			break;
		case 'C':
			ncci->Reason_B3 = CapiDisconnectDuringTrans_RemoteAbort;
			CodeNCPI(fax, ncci, 0, 0);
			break;
		default: /* phase D or E */
			ret = -EBUSY;
			break;
		}
	}

	return ret;
}

static void FaxSendDown(struct mFAX *fax, struct mNCCI *ncci, int ind)
{
	int pktid, l, tot, ret, i;
	//struct mc_buf *mc;
	int16_t wav_buf[MAX_DATA_SIZE];
	int16_t *w;
	uint8_t *p, sbuf[MAX_DATA_SIZE];

	if (!ncci)
		return;
	pthread_mutex_lock(&ncci->lock);
	if (!fax->modem_active) {
		dprint(MIDEBUG_NCCI, "%s: Modem not active%s - do not send down anymore\n",
			CAPIobjIDstr(&fax->cobj), fax->modem_active ? " and end" : "");
		pthread_mutex_unlock(&ncci->lock);
		return;
	}
#ifdef USE_DLBUSY
	if (!ncci->xmit_handles[ncci->oidx].pkt) {
		ncci->dlbusy = 0;
		dprint(MIDEBUG_NCCI_DATA, "%s: no data\n", CAPIobjIDstr(&fax->cobj));
		pthread_mutex_unlock(&ncci->lock);
		return;
	}
	mc = ncci->xmit_handles[ncci->oidx].pkt;
#endif
	if (!ncci->BIlink) {
		wprint("%s: BInstance is gone - packet ignored\n", CAPIobjIDstr(&fax->cobj));
		pthread_mutex_unlock(&ncci->lock);
		return;
	}
	pktid = ++ncci->BIlink->DownId;
	if (0 == pktid || pktid > 0x7fff) {
		pktid = 1;
		ncci->BIlink->DownId = 1;
	}
#ifdef USE_DLBUSY
	ncci->xmit_handles[ncci->oidx].PktId = pktid;
	if (ncci->flowmode == flmPHDATA) {
		if (ncci->dlbusy) {
			wprint("%s: dlbusy set\n", CAPIobjIDstr(&fax->cobj));
			pthread_mutex_unlock(&ncci->lock);
			return;
		} else
			ncci->dlbusy = 1;
	}
	/* complete paket */
	l = ncci->xmit_handles[ncci->oidx].dlen;
	ncci->down_iv[1].iov_len = l;
	ncci->down_iv[1].iov_base = mc->rp;
	ncci->xmit_handles[ncci->oidx].sent = l;
#else
	l = ncci->BIlink->rx_min;
	pthread_mutex_unlock(&ncci->lock);
	if (fax->fax) {
		ret = fax_tx(fax->fax, wav_buf, l);
	} else {
		wprint("%s: fax status gone - do not send down anymore\n", CAPIobjIDstr(&fax->cobj));
		return;
	}
	dprint(MIDEBUG_NCCI_DATA, "%s: phase:%c got %d/%d samples from faxmodem(%p)\n",
		CAPIobjIDstr(&fax->cobj), fax->phase, ret, l, fax->fax);

	w = wav_buf;
	p = sbuf;
	l = ret;
	// convert (pcm -> alaw)
	for (i = 0; i < ret; i++)
		*p++ = slin2alaw[*w++];
	pthread_mutex_lock(&ncci->lock);
	if (!ncci->BIlink) {
		wprint("%s: BInstance is gone - packet ignored\n", CAPIobjIDstr(&fax->cobj));
		pthread_mutex_unlock(&ncci->lock);
		return;
	}
	if (!fax->modem_active) {
		dprint(MIDEBUG_NCCI, "%s: Modem not active%s - do not send down anymore\n",
			CAPIobjIDstr(&fax->cobj), fax->modem_active ? " and end" : "");
		pthread_mutex_unlock(&ncci->lock);
		return;
	}
	ncci->down_iv[1].iov_len = l;
	ncci->down_iv[1].iov_base = sbuf;
#endif
	ncci->down_header.id = pktid;
	ncci->down_msg.msg_iovlen = 2;
	tot = l + ncci->down_iv[0].iov_len;
	ret = sendmsg(ncci->BIlink->fd, &ncci->down_msg, MSG_DONTWAIT);
	if (ret != tot) {
		wprint("%s: send returned %d while sending %d bytes type %s id %d - %s\n", CAPIobjIDstr(&fax->cobj),
			ret, tot, _mi_msg_type2str(ncci->down_header.prim), ncci->down_header.id, strerror(errno));
#ifdef USE_DLBUSY
		ncci->dlbusy = 0;
		ncci->xmit_handles[ncci->oidx].pkt = NULL;
		ncci->xmit_handles[ncci->oidx].PktId = 0;
		ncci->oidx++;
		if (ncci->oidx == ncci->window)
			ncci->oidx = 0;
#endif
		pthread_mutex_unlock(&ncci->lock);
		return;
	} else
#ifdef USE_DLBUSY
		dprint(MIDEBUG_NCCI_DATA, "%s: send down %d bytes type %s id %d current oidx[%d] sent %d/%d %s\n",
			CAPIobjIDstr(&fax->cobj), ret, _mi_msg_type2str(ncci->down_header.prim), ncci->down_header.id, ncci->oidx,
			ncci->xmit_handles[ncci->oidx].sent, ncci->xmit_handles[ncci->oidx].dlen,
			ind ? "ind" : "cnf");
#else
		dprint(MIDEBUG_NCCI_DATA, "%s: send down %d bytes type %s id %d sent %d - %p\n",
			CAPIobjIDstr(&fax->cobj), ret, _mi_msg_type2str(ncci->down_header.prim),
			ncci->down_header.id, ret, fax->fax);
#endif
#ifdef USE_DLBUSY
	ncci->dlbusy = 1;
	ncci->oidx++;
	if (ncci->oidx == ncci->window)
		ncci->oidx = 0;
#endif
	pthread_mutex_unlock(&ncci->lock);
}

static int FaxDataInd(struct mFAX *fax, struct mNCCI *ncci, struct mc_buf *mc)
{
	int i, ret;
	uint16_t dlen;
	struct mISDNhead *hh;
	int16_t wav_buf[MAX_DATA_SIZE];
	int16_t *w;
	unsigned char *p;
	fax_state_t *fst;

	hh = (struct mISDNhead *)mc->rb;
	dlen = mc->len - sizeof(*hh);
	if (!ncci)
		return -EINVAL;
	pthread_mutex_lock(&ncci->lock);
	if ((!ncci->BIlink) || (!fax->fax)) {
		wprint("%s: frame with %d bytes dropped Blink(%p) or spandsp fax(%p) gone\n",
			CAPIobjIDstr(&fax->cobj), dlen, ncci->BIlink, fax->fax);
		pthread_mutex_unlock(&ncci->lock);
		return -EINVAL;
	}
	if (!fax->modem_active) {
		wprint("%s: Modem not active %d bytes dropped\n", CAPIobjIDstr(&fax->cobj), dlen);
		pthread_mutex_unlock(&ncci->lock);
		return -ENOTCONN;
	}
	fst = fax->fax;
	pthread_mutex_unlock(&ncci->lock);

	if (dlen > MAX_DATA_SIZE) {
		wprint("%s: frame overflow %d/%d - truncated\n", CAPIobjIDstr(&fax->cobj), dlen, MAX_DATA_SIZE);
		dlen =  MAX_DATA_SIZE;
	}
	w = wav_buf;
	mc->rp = mc->rb + sizeof(*hh);
	p = mc->rp;
	// convert (alaw -> pcm)
	for (i = 0; i < dlen; i++)
		*w++ = alaw2lin[*p++];

	ret = fax_rx(fst, wav_buf, dlen);
	dprint(MIDEBUG_NCCI_DATA, "%s: phase:%c send %d samples to faxmodem(%p)ret %d\n",
		CAPIobjIDstr(&fax->cobj), fax->phase, dlen, fax->fax, ret);
#ifdef USE_DLBUSY
	/* try to get same amount from faxmodem */
	ret = fax_tx(fst, wav_buf, dlen);
	dprint(MIDEBUG_NCCI_DATA, "got %d/%d samples from faxmodem\n", ret, dlen);

	w = wav_buf;
	p = mc->rp;
	mc->len = ret;
	// convert (pcm -> alaw)
	for (i = 0; i < ret; i++)
		*p++ = slin2alaw[*w++];

	pthread_mutex_lock(&ncci->lock);

	if (ncci->xmit_handles[ncci->iidx].pkt) {
		wprint("%s: SendQueueFull\n", CAPIobjIDstr(&fax->cobj));
		pthread_mutex_unlock(&ncci->lock);
		return -EBUSY;
	}
	ncci->xmit_handles[ncci->iidx].pkt = mc;
	ncci->xmit_handles[ncci->iidx].dlen = ret;
	ncci->xmit_handles[ncci->iidx].sent = 0;
	ncci->xmit_handles[ncci->iidx].sp = mc->rp;

	ncci->iidx++;
	if (ncci->iidx == ncci->window)
		ncci->iidx = 0;
	if (!ncci->dlbusy)
		ret = 1;
	else
		ret = 0;
	pthread_mutex_unlock(&ncci->lock);
	if (ret)
		FaxSendDown(fax, ncci, 1);
#else
	free_mc_buf(mc);
	if (fax->startdownlink) {
		fax->startdownlink = 0;
		FaxSendDown(fax, ncci, 1);
	}
#endif
	return 0;
}

static void FaxDataConf(struct mFAX *fax, struct mNCCI *ncci, struct mc_buf *mc)
{
#ifdef USE_DLBUSY
	int i;
	struct mISDNhead *hh;

	if (!fax || !ncci)
		return;
	pthread_mutex_lock(&ncci->lock);
	if ((!ncci->BIlink) || (!fax->fax)) {
		wprint("%s: Blink(%p) or spandsp fax(%p) gone\n", CAPIobjIDstr(&fax->cobj), ncci->BIlink, fax->fax);
		pthread_mutex_unlock(&ncci->lock);
		free_mc_buf(mc);
		return;
	}
	if (!fax->modem_active) {
		wprint("%s: Modem not active\n", CAPIobjIDstr(&fax->cobj));
		pthread_mutex_unlock(&ncci->lock);
		free_mc_buf(mc);
		return;
	}
	hh = (struct mISDNhead *)mc->rb;
	if (ncci->flowmode != flmPHDATA) {
		wprint("%s: Got DATA confirm for %x - but flow mode(%d)\n", CAPIobjIDstr(&fax->cobj),
			hh->id, ncci->flowmode);
		pthread_mutex_unlock(&ncci->lock);
		free_mc_buf(mc);
		return;
	}

	for (i = 0; i < ncci->window; i++) {
		if (ncci->xmit_handles[i].PktId == hh->id) {
			if (ncci->xmit_handles[i].pkt)
				break;
		}
	}
	if (i == ncci->window) {
		wprint("%s: Got DATA confirm for %x - but ID not found\n", CAPIobjIDstr(&fax->cobj), hh->id);
		for (i = 0; i < ncci->window; i++)
			wprint("%s: PktId[%d] %x\n", CAPIobjIDstr(&fax->cobj), i, ncci->xmit_handles[i].PktId);
		pthread_mutex_unlock(&ncci->lock);
		free_mc_buf(mc);
		return;
	}
	dprint(MIDEBUG_NCCI_DATA, "%s: phase:%c confirm xmit_handles[%d] pktid=%x handle=%d\n",
		CAPIobjIDstr(&fax->cobj), fax->phase, i, hh->id, ncci->xmit_handles[i].DataHandle);
	free_mc_buf(mc);
	mc = ncci->xmit_handles[i].pkt;
	ncci->xmit_handles[i].pkt = NULL;
	ncci->xmit_handles[i].PktId = 0;
	ncci->dlbusy = 0;
	free_mc_buf(mc);
	pthread_mutex_unlock(&ncci->lock);
	FaxSendDown(fax, ncci, 0);
#else
	free_mc_buf(mc);
	FaxSendDown(fax, ncci, 0);
#endif
}

static void FaxDataResp(struct mFAX *fax, struct mNCCI *ncci, struct mc_buf *mc)
{
	int i;
	uint16_t dh = CAPIMSG_RESP_DATAHANDLE(mc->rb);

	if (!ncci)
		return;
	pthread_mutex_lock(&ncci->lock);
	for (i = 0; i < ncci->window; i++) {
		if (ncci->recv_handles[i] == dh) {
			dprint(MIDEBUG_NCCI_DATA, "%s: data handle %d acked at pos %d\n",
				CAPIobjIDstr(&fax->cobj), dh, i);
			ncci->recv_handles[i] = 0;
			break;
		}
	}
	if (i == ncci->window) {
		char deb[128], *dp;

		dp = deb;
		for (i = 0; i < ncci->window; i++)
			dp += sprintf(dp, " [%d]=%d", i, ncci->recv_handles[i]);
		wprint("%s: data handle %d not in%s\n", CAPIobjIDstr(&fax->cobj), dh, deb);
	}
	pthread_mutex_unlock(&ncci->lock);
	if (fax->b3transfer_active)
		FaxB3Ind(fax, ncci);
	if (fax->b3transfer_end)
		FaxB3Disconnect(fax, ncci);
}

static int FaxWriteTiff(struct mFAX *fax, struct mc_buf *mc)
{
	int ret, res = 0;

	if (fax->file_d == -2) {
		fax->file_d = open(fax->file_name, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP);
		if (fax->file_d < 0) {
			res = -errno;
			wprint("%s: Cannot open TIFF %s for writing - %s\n", CAPIobjIDstr(&fax->cobj),
				fax->file_name, strerror(errno));
		} else
			dprint(MIDEBUG_NCCI, "%s: open fax->file_d %d\n", CAPIobjIDstr(&fax->cobj), fax->file_d);
	}
	if (fax->file_d < 0) {
		if (!res)
			res = -1;
	} else {
		ret = write(fax->file_d, mc->rp, mc->len);
		if (ret != mc->len) {
			res = -errno;
			wprint("%s: Write data to %s only %d/%d - %s\n", CAPIobjIDstr(&fax->cobj),
				fax->file_name, ret, mc->len, strerror(ret));
			if (!res)
				res = -1;
		}
	}
	return res;
}

static int FaxDataB3Req(struct mFAX *fax, struct mNCCI *ncci, struct mc_buf *mc)
{
	int ret;
	uint16_t off, dlen, flg, dh, info;
	// unsigned char *dp;

	if (!ncci)
		return -EINVAL;
	off = CAPIMSG_LEN(mc->rb);
	if (off != 22 && off != 30) {
		wprint("%s: Illegal message len %d\n", CAPIobjIDstr(&fax->cobj), off);
		AnswerDataB3Req(ncci, mc, CapiIllMessageParmCoding);
		return 0;
	}
	pthread_mutex_lock(&ncci->lock);
	dlen = CAPIMSG_DATALEN(mc->rb);
	flg = CAPIMSG_REQ_FLAGS(mc->rb);

	/* Answer the DATAB3_REQ */
	info = fax->b3transfer_error ? CapiProtocolErrorLayer3 : 0;
	dh = CAPIMSG_U16(mc->rb, 18);
	mc->len = 16;
	CAPIMSG_SETLEN(mc->rb, 16);
	CAPIMSG_SETSUBCOMMAND(mc->rb, CAPI_CONF);
	capimsg_setu16(mc->rb, 12, dh);
	capimsg_setu16(mc->rb, 14, info);
	SendMessage2Application(ncci->appl, mc);

	mc->rp = mc->rb + off;
	mc->len = dlen;
	switch (fax->b3_format) {
	case FAX_B3_FORMAT_SFF:
		ret = SFF_Put_Data(&fax->sff, mc->rp, mc->len);
		if (ret == 0) {
			fax->b3transfer_end = 1;
			fax->b3transfer_active = 0;
		}
		break;
	case FAX_B3_FORMAT_TIFF:
		ret = FaxWriteTiff(fax, mc);
		break;
	default:
		wprint("%s: B3 data format %d not supported\n", CAPIobjIDstr(&fax->cobj), fax->b3_format);
		ret = -EINVAL;
		break;
	}
	if (ret < 0) {
		wprint("%s: handle = %d flags = %04x data offset %d delivered error ret %d\n",
			CAPIobjIDstr(&fax->cobj), CAPIMSG_REQ_DATAHANDLE(mc->rb), flg, off, ret);
		fax->b3transfer_error = ret;
	} else
		dprint(MIDEBUG_NCCI_DATA, "%s: handle = %d flags = %04x data offset %d delivered ret %d\n",
			CAPIobjIDstr(&fax->cobj), CAPIMSG_REQ_DATAHANDLE(mc->rb), flg, off, ret);

	pthread_mutex_unlock(&ncci->lock);
	free_mc_buf(mc);

	if (fax->b3transfer_end) {
		if (fax->b3transfer_error == 0) {
			switch (fax->b3_format) {
			case FAX_B3_FORMAT_SFF:
				fax->b3transfer_error = SFF_WriteTiff(&fax->sff, fax->file_name);
				break;
			case FAX_B3_FORMAT_TIFF:
				break;
			default:
				eprint("B3 data format %d not supported\n", fax->b3_format);
				fax->b3transfer_error = -1;
				break;
			}
		}
		if (fax->b3transfer_error) {
			FaxB3Disconnect(fax, ncci);
			if (fax->binst && fax->binst->waiting)
				sem_post(&fax->binst->wait);
		} else {
			if (fax->extended) {
				t30_set_tx_file(fax->t30, fax->file_name, -1, -1);
				if (fax->binst && fax->binst->waiting)
					sem_post(&fax->binst->wait);
			} else {
				ret = InitFax(fax);
				if (ret) {
					wprint("%s: cannot init Faxmodem return %d\n", CAPIobjIDstr(&fax->cobj), ret);
					fax->b3transfer_error = 1;
					FaxB3Disconnect(fax, ncci);
				}
			}
		}
	}
	return 0;
}

static int FaxDisconnectB3Req(struct mFAX *fax, struct mNCCI *ncci, struct mc_buf *mc)
{
	int ret;

	dprint(MIDEBUG_NCCI, "%s: phase:%c DisconnectB3Req b3transfer %s\n",
		CAPIobjIDstr(&fax->cobj), fax->phase, fax->b3transfer_active ? "active" : "not active");
	if (!ncci)
		return -EINVAL;
	SendCmsgAnswer2Application(ncci->appl, mc, 0);
	pthread_mutex_lock(&ncci->lock);
	if (fax->b3transfer_active) {
		fax->b3transfer_end = 1;
		fax->b3transfer_active = 0;
		switch (fax->b3_format) {
		case FAX_B3_FORMAT_SFF:
			wprint("%s: got no EOF - fax document incomplete\n", CAPIobjIDstr(&fax->cobj));
			fax->b3transfer_error = -1;
			break;
		case FAX_B3_FORMAT_TIFF:
			if (fax->file_d >= 0) {
				dprint(MIDEBUG_NCCI, "%s: close fax->file_d %d\n", CAPIobjIDstr(&fax->cobj), fax->file_d);
				close(fax->file_d);
				fax->file_d = -1;
			} else
				fax->b3transfer_error = -1;
			break;
		default:
			eprint("B3 data format %d not supported\n", fax->b3_format);
			fax->b3transfer_error = -1;
			break;
		}
		if (fax->b3transfer_error) {
			FaxB3Disconnect(fax, ncci);
		} else {
			if (fax->extended) {
				if (fax->binst && fax->binst->waiting)
					sem_post(&fax->binst->wait);
			} else {
				ret = InitFax(fax);
				if (ret) {
					wprint("%s: cannot init Faxmodem return %d\n",
						CAPIobjIDstr(&fax->cobj), ret);
					fax->b3transfer_error = 1;
					FaxB3Disconnect(fax, ncci);
				}
			}
		}
	} else {
		if (fax->binst && fax->binst->waiting)
			sem_post(&fax->binst->wait);
	}
	pthread_mutex_unlock(&ncci->lock);
	return 0;
}


static int FaxConnectB3Req(struct mFAX *fax, struct mNCCI *ncci, struct mc_buf *mc)
{
	uint16_t info = 0;
	int ret = 0;
	int val;

	if (!ncci)
		return -EINVAL;
	if (mc->cmsg.NCPI && mc->cmsg.NCPI[0]) {
		val = CAPIMSG_U16(mc->cmsg.NCPI, 5);
		if (val != fax->b3_format) {
			dprint(MIDEBUG_NCCI, "%s: Fax changed format %d ->%d\n", CAPIobjIDstr(&fax->cobj),
				fax->b3_format, val);
			switch (val) {
			case FAX_B3_FORMAT_SFF:
			case FAX_B3_FORMAT_TIFF:
				fax->b3_format = val;
				info = 0;
				break;
			default:
				info = CapiNCPINotSupported;
				break;
			}
		}
		if (fax->extended) {
			val = CAPIMSG_U16(mc->cmsg.NCPI, 3);
			fax->high_res = (val & 0x0001) ? 1 : 0;
			fax->polling = (val & 0x0002) ? 1 : 0;
			fax->more_doc = (val & 0x0004) ? 1 : 0;
			fax->no_ecm = (val & 0x8000) ? 1 : 0;
		}
	}
	mc->cmsg.Subcommand = CAPI_CONF;
	mc->cmsg.adr.adrNCCI = ncci->cobj.id;
	mc->cmsg.Info = info;
	pthread_mutex_lock(&ncci->lock);
	if (info) {
		fax->b3transfer_end = 1;
		fax->b3transfer_error = info;
		pthread_mutex_unlock(&ncci->lock);
		ret = ncciB3Message(ncci, mc);
	} else {
		fax->b3transfer_active = 1;
		pthread_mutex_unlock(&ncci->lock);
		ret = activate_bchannel(ncci->BIlink);
		if (!ret) {
			ret = ncciB3Message(ncci, mc);
			if (!ret) {
				if (fax->extended) {
					ret = InitFax(fax);
					if (ret) {
						wprint("%s: cannot init Faxmodem return %d\n", CAPIobjIDstr(&fax->cobj), ret);
						fax->b3transfer_error = 1;
						FaxB3Disconnect(fax, ncci);
					}
				} else {
					FaxConnectActiveB3(fax, ncci, mc);
				}
			} else {
				wprint("%s: CAPI_CONNECT_B3_CONF not handled by ncci statemachine\n", CAPIobjIDstr(&fax->cobj));
			}
		} else {
			wprint("%s: cannot activate outgoing link %s\n", CAPIobjIDstr(&fax->cobj), strerror(-ret));
		}
	}
	return ret;
}

int FaxB3Message(struct BInstance *bi, struct mc_buf *mc)
{
	struct mFAX *fax;
	int retval = CapiNoError;
	uint8_t cmd, subcmd;
	struct mNCCI *ncci;

	fax = get_mFAX(bi->b3data);
	if (!mc) {
		if (fax) {
			dprint(MIDEBUG_NCCI, "%s: release b3data\n", CAPIobjIDstr(&fax->cobj));
			put_mFAX(fax); /* bi->b3data ref set NULL on caller */
			put_mFAX(fax); /*  get_mFAX() ref */
		} else
			dprint(MIDEBUG_NCCI, "release b3data - b3data already gone\n");
		return CapiNoError;
	}
	cmd = CAPIMSG_COMMAND(mc->rb);
	subcmd = CAPIMSG_SUBCOMMAND(mc->rb);
	if (!fax) {
		if (cmd == CAPI_CONNECT_B3 && subcmd == CAPI_REQ) {
			fax = mFaxCreate(bi);
		}
		if (!fax) {
			wprint("No Fax struct assigned\n");
			return CapiMsgOSResourceErr;
		}
	}
	ncci = get_faxncci(fax);
	switch (CAPICMD(cmd, subcmd)) {
	case CAPI_DISCONNECT_IND:
		/* This does inform NCCI about a received DISCONNECT in the D-channel */
		retval = DisconnectIndication(fax, ncci, mc);
		if (retval)
			goto unlock;
		break;
	case CAPI_DATA_B3_REQ:
		retval = FaxDataB3Req(fax, ncci, mc);
		break;
	case CAPI_CONNECT_B3_REQ:
		retval = FaxConnectB3Req(fax, ncci, mc);
		break;
	case CAPI_DATA_B3_RESP:
		FaxDataResp(fax, ncci, mc);
		retval = 0;
		break;
	case CAPI_CONNECT_B3_RESP:
		if (!fax->b3transfer_error) /* Do not send CONNECT_B3_ACTIVE if error */
			retval = ncciB3Message(ncci, mc);
		else
			dprint(MIDEBUG_NCCI, "%s: ignored CONNECT_B3_RESP in %s\n",
				CAPIobjIDstr(&fax->cobj), _mi_ncci_st2str(ncci));
		break;
	case CAPI_CONNECT_B3_ACTIVE_RESP:
		retval = ncciB3Message(ncci, mc);
		break;
	case CAPI_DISCONNECT_B3_REQ:
		retval = FaxDisconnectB3Req(fax, ncci, mc);
		break;
	case CAPI_DISCONNECT_B3_RESP:
		if (ncci) {
			retval = ncciB3Message(ncci, mc);
		}
		fax->B3DisconnectDone = 1;
		mFaxRelease(fax, ncci);
		break;
#if 0
	case CAPI_FACILITY_REQ:
		retval = FsmEvent(&ncci->ncci_m, EV_AP_FACILITY_REQ, mc);
		break;
	case CAPI_FACILITY_RESP:
		/* no need to handle */
		retval = 0;
		break;
	case CAPI_MANUFACTURER_REQ:
		retval = FsmEvent(&ncci->ncci_m, EV_AP_MANUFACTURER_REQ, mc);
		break;
#endif
	default:
		eprint("%s: error unhandled command %02x/%02x\n",
			CAPIobjIDstr(&fax->cobj), cmd, subcmd);
		retval = CapiMessageNotSupportedInCurrentState;
	}
	if (retval) {
		if (subcmd == CAPI_REQ)
			retval = CapiMessageNotSupportedInCurrentState;
		else {		/* RESP */
			wprint("%s: Error Message %02x/%02x not supported\n",
				CAPIobjIDstr(&fax->cobj), cmd, subcmd);
			retval = CapiNoError;
		}
	} else if (!(cmd == CAPI_DATA_B3 && subcmd == CAPI_REQ)) {
		free_mc_buf(mc);
	}
unlock:
	put_ncci(ncci);
	put_mFAX(fax);
	return retval;
}

int FaxRecvBData(struct BInstance *bi, struct mc_buf *mc)
{
	struct mISDNhead *hh;
	struct mFAX *fax;
	struct mNCCI *ncci;
	int ret = 0;

	fax = get_mFAX(bi->b3data);
	ncci = get_faxncci(fax);
	if (!mc) {
		/* timeout RELEASE */
		if (fax) {
			dprint(MIDEBUG_NCCI, "%s: got mc=NULL - RELEASE\n", CAPIobjIDstr(&fax->cobj));
			if (ncci)
				pthread_mutex_lock(&ncci->lock);
			fax->modem_end = 1;
			fax->modem_active = 0;
			if (ncci)
				pthread_mutex_unlock(&ncci->lock);
			FaxB3Disconnect(fax, ncci);
			goto unref;
		}
		/* no refs so return direct */
		return 0;
	}
	hh = (struct mISDNhead *)mc->rb;
	switch (hh->prim) {
	case PH_DATA_IND:
		if (!fax) {
			wprint("Controller%d ch%d: Got %s but but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr,
				_mi_msg_type2str(hh->prim));
			ret = -EINVAL;
			break;
		} else
			ret = FaxDataInd(fax, ncci, mc);
		break;
	case PH_DATA_CNF:
		if (!fax) {
			wprint("Controller%d ch%d: Got %s but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr,
				_mi_msg_type2str(hh->prim));
			ret = -EINVAL;
		} else {
			FaxDataConf(fax, ncci, mc);
			ret = 0;
		}
		break;
	case PH_ACTIVATE_IND:
	case PH_ACTIVATE_CNF:
		if (!fax) {
			fax = mFaxCreate(bi);
			if (!fax) {
				eprint("Cannot create Fax struct for PLCI %04x\n", bi->lp ? bi->lp->cobj.id : 0xffff);
				return -ENOMEM;
			}
		} else
			dprint(MIDEBUG_NCCI, "%s: got %s\n", CAPIobjIDstr(&fax->cobj), _mi_msg_type2str(hh->prim));
		fax->phase = 'B';
		ret = 1;
		break;
	case PH_DEACTIVATE_IND:
		if (!fax) {
			wprint("Controller%d ch%d: Got %s but but no b3data set\n", bi->pc->profile.ncontroller, bi->nr,
				_mi_msg_type2str(hh->prim));
			ret = -EINVAL;
			break;
		} else
			dprint(MIDEBUG_NCCI, "%s: got %s\n", CAPIobjIDstr(&fax->cobj), _mi_msg_type2str(hh->prim));
		if (ncci)
			pthread_mutex_lock(&ncci->lock);
		fax->modem_end = 1;
		fax->modem_active = 0;
		if (ncci)
			pthread_mutex_unlock(&ncci->lock);
		FaxB3Disconnect(fax, ncci);
		ret = 1;
		break;
	case PH_DEACTIVATE_CNF:
		if (!fax) {
			wprint("Controller%d ch%d: Got %s but but no b3data set\n", bi->pc->profile.ncontroller, bi->nr,
				_mi_msg_type2str(hh->prim));
			ret = -EINVAL;
			break;
		} else
			dprint(MIDEBUG_NCCI, "%s: got %s\n", CAPIobjIDstr(&fax->cobj), _mi_msg_type2str(hh->prim));
		if (ncci)
			pthread_mutex_lock(&ncci->lock);
		fax->modem_end = 1;
		fax->modem_active = 0;
		if (ncci)
			pthread_mutex_unlock(&ncci->lock);
		FaxB3Disconnect(fax, ncci);
		ret = 1;
		break;
	case PH_CONTROL_CNF:
		if (!fax) {
			wprint("Controller%d ch%d: Got %s id %x but but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr,
				_mi_msg_type2str(hh->prim), hh->id);
			ret = -EINVAL;
		} else
			dprint(MIDEBUG_NCCI, "%s: got %s id %x\n", CAPIobjIDstr(&fax->cobj),
				_mi_msg_type2str(hh->prim), hh->id);
		break;
	case PH_CONTROL_IND:
		if (!fax) {
			wprint("Controller%d ch%d: Got %s id %x but but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr,
				_mi_msg_type2str(hh->prim), hh->id);
			ret = -EINVAL;
		} else
			dprint(MIDEBUG_NCCI, "%s: got %s id %x\n", CAPIobjIDstr(&fax->cobj),
				_mi_msg_type2str(hh->prim), hh->id);
		break;
	default:
		wprint("Controller%d ch%d: Got %s (%x) id=%x len %d\n", bi->pc->profile.ncontroller, bi->nr,
			_mi_msg_type2str(hh->prim), hh->prim, hh->id, mc->len);
		ret = -EINVAL;
	}
unref:
	put_ncci(ncci);
	put_mFAX(fax);
	return ret;
}

#endif // USE_SOFTFAX
