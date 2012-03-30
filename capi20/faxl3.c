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
#include <spandsp.h>
#include "alaw.h"
#include "sff.h"
#include "g3_mh.h"

#ifdef USE_SOFTFAX

#define DEFAULT_PKT_SIZE	512

struct fax {
	struct mNCCI		*ncci;
	struct BInstance	*binst;
	struct lPLCI		*lplci;
	unsigned int		outgoing:1;
	unsigned int		modem_active:1;
	unsigned int		modem_end:1;
	unsigned int		startdownlink:1;
	unsigned int		b3transfer_active:1;
	unsigned int		b3transfer_end:1;
	unsigned int		high_res:1;
	unsigned int		polling:1;
	unsigned int		more_doc:1;
	unsigned int		no_ecm:1;
	unsigned int		b3data_mapped:1;
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

static void StopDownLink(struct fax *fax)
{
	int i;
	struct mNCCI *ncci = fax->ncci;
	struct mc_buf *mc;

	dprint(MIDEBUG_NCCI, "NCCI %06x: StopDownLink\n", ncci->ncci);
	ncciL4L3(ncci, PH_DEACTIVATE_REQ, 0, 0, NULL, NULL);
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
	pthread_mutex_unlock(&ncci->lock);
}

static void FaxB3Disconnect(struct fax *fax)
{
	unsigned char ncpi[32], len, ncpi_len;
	struct mc_buf *mc;

	mc = alloc_mc_buf();
	if (mc) {
		ncciCmsgHeader(fax->ncci, mc, CAPI_DISCONNECT_B3, CAPI_IND);
		ncpi_len = 8;
		capimsg_setu16(ncpi, 1, fax->t30stats.bit_rate & 0xffff);
		/* todo set from stats */
		capimsg_setu16(ncpi, 3, 0);
		capimsg_setu16(ncpi, 5, fax->b3_format);
		capimsg_setu16(ncpi, 7, fax->t30stats.pages_in_file);
		len = (uint8_t)strlen(fax->RemoteID);
		if (len > 20)
			len = 20;
		ncpi[9] = len;
		ncpi_len += 1 + len;
		if (len)
			strncpy((char *)&ncpi[10], fax->RemoteID, len);
		ncpi[0] = ncpi_len;
		mc->cmsg.NCPI = ncpi;
		if (fax->b3transfer_error) {
			/* TODO error mapping */
			mc->cmsg.Reason_B3 = 0x3301;
		} else
			mc->cmsg.Reason_B3 = 0;
		ncciB3Message(fax->ncci, mc);
		free_mc_buf(mc);
	} else {
		eprint("No msg buffer\n");
		/* TODO error shutdown */
	}
}

static void FaxB3Ind(struct fax *fax)
{
	struct mNCCI *ncci = fax->ncci;
	int ret, tot;
	uint16_t dlen, dh;
	unsigned char *dp;

	pthread_mutex_lock(&ncci->lock);
	if (ncci->recv_handles[ncci->ridx] != 0) {
		pthread_mutex_unlock(&ncci->lock);
		wprint("NCCI %06x: Send FaxInd queue full\n", ncci->ncci);
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
		wprint("NCCI %06x: : frame with %d + %d bytes only %d bytes are sent - %s\n",
			ncci->ncci, dlen, CAPI_B3_DATA_IND_HEADER_SIZE, ret, strerror(errno));
	} else
		dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: frame with %d + %d bytes handle %d was sent ret %d\n",
			ncci->ncci, CAPI_B3_DATA_IND_HEADER_SIZE, dlen, dh, ret);
}

static int FaxPrepareReceivedTiff(struct fax *fax)
{
	struct mNCCI *ncci = fax->ncci;
	int ret, fd;
	struct stat fst;

	fd = open(fax->file_name, O_RDONLY);
	if (fd < 0) {
		fax->b3transfer_error = errno;
		wprint("NCCI %06x: Cannot open received file %s - %s\n", ncci->ncci, fax->file_name, strerror(fax->b3transfer_error));
		return -fax->b3transfer_error;
	}
	ret = fstat(fd, &fst);
	if (ret) {
		fax->b3transfer_error = errno;
		wprint("NCCI %06x: Cannot fstat received file %s - %s\n", ncci->ncci, fax->file_name, strerror(fax->b3transfer_error));
		return -fax->b3transfer_error;
	}
	fax->b3data_size = fst.st_size;
	fax->b3data = mmap(NULL, fax->b3data_size, PROT_READ, MAP_FILE, fd, 0);
	if (!fax->b3data) {
		fax->b3transfer_error = errno;
		wprint("NCCI %06x: Cannot map received file %s - %s\n", ncci->ncci, fax->file_name, strerror(fax->b3transfer_error));
		return -fax->b3transfer_error;
	}
	fax->b3data_mapped = 1;
	close(fd);
	return 0;
}

static void FaxPrepareReceivedData(struct fax *fax)
{
	struct mNCCI *ncci = fax->ncci;
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
		eprint("NCCI %06x: B3 data format %d not supported\n", ncci->ncci, fax->b3_format);
		ret = -1;
		break;
	}
	if (ret < 0)
		FaxB3Disconnect(fax);
	else {
		fax->b3transfer_active = 1;
		fax->b3data_pos = 0;
		FaxB3Ind(fax);
	}
}

static void FaxConnectActiveB3(struct fax *fax, struct mc_buf *mc) {
	struct mc_buf *lmc = mc;
	struct mNCCI *ncci = fax->ncci;
	unsigned char ncpi[32];

	if (!mc)
		lmc = alloc_mc_buf();
	if (fax->lplci->Bprotocol.B3 == 4) {
		ncpi[0] = 0;
	} else {
		capimsg_setu16(ncpi, 1, 14400); // FIXME
		capimsg_setu16(ncpi, 3, fax->high_res);
		capimsg_setu16(ncpi, 5, fax->b3_format);
		capimsg_setu16(ncpi, 7, 0);
		ncpi[9] = strlen(fax->RemoteID);
		strncpy((char *)&ncpi[10], fax->RemoteID, 20);
		ncpi[0] = 10 + ncpi[9];
	}
	
	ncciCmsgHeader(ncci, lmc, CAPI_CONNECT_B3_ACTIVE, CAPI_IND);
	lmc->cmsg.NCPI = ncpi;
	ncciB3Message(fax->ncci, lmc);
	if (!mc)
		free_mc_buf(lmc);
}

static void rt_frame_handler(t30_state_t *t30, void *user_data, int direction, const uint8_t *msg, int len)
{
	struct fax *fax = user_data;
	struct mNCCI *ncci = fax->ncci;

	dprint(MIDEBUG_NCCI, "NCCI %06x: RT handler direction %d msg %p len %d\n", ncci->ncci, direction, msg, len);
	dhexprint(MIDEBUG_NCCI_DATA, "RT msg:", (unsigned char *)msg, len);
}

static int phaseB_handler(t30_state_t *t30, void *user_data, int result)
{
	struct fax *fax = user_data;
	struct mNCCI *ncci = fax->ncci;
	const char *ident = "no T30";
	unsigned char ncpi[132];
	struct mc_buf *mc;
	int ret;
	struct mISDN_ctrl_req creq;

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
		dprint(MIDEBUG_NCCI, "NCCI %06x: PhaseB DIS %s\n", ncci->ncci, ident);
		strncpy(fax->RemoteID, ident, 20);
		if (fax->lplci->Bprotocol.B3 == 5) {
			FaxConnectActiveB3(fax, NULL);
			if (fax->binst) {
				fax->binst->waiting = 1;
				/* Send silence when Underrun */
				creq.op = MISDN_CTRL_FILL_EMPTY;
				creq.channel = fax->binst->nr;
				creq.p1 = 1;
				creq.p2 = 0;
				creq.p2 |= 0xff & slin2alaw[0];
				creq.p3 = 0;
				ret = ioctl(fax->binst->fd, IMCTRLREQ, &creq);
				/* MISDN_CTRL_FILL_EMPTY is not mandatory warn if not supported */
				if (ret < 0)
					wprint("Error on MISDN_CTRL_FILL_EMPTY ioctl - %s\n", strerror(errno));
				creq.op = MISDN_CTRL_RX_OFF;
				creq.channel = fax->binst->nr;
				creq.p1 = 1;
				creq.p2 = 0;
				creq.p3 = 0;
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
					creq.p3 = 0;
					ret = ioctl(fax->binst->fd, IMCTRLREQ, &creq);
					if (ret < 0)
						wprint("Error on MISDN_CTRL_RX_OFF ioctl - %s\n", strerror(errno));
					else
						dprint(MIDEBUG_NCCI, "NCCI %06x: Dropped %d bytes during waiting\n",
							ncci->ncci, creq.p2);
					creq.op = MISDN_CTRL_FILL_EMPTY;
					creq.channel = fax->binst->nr;
					creq.p1 = 0;
					creq.p2 = 0;
					creq.p2 = -1;
					creq.p3 = 0;
					ret = ioctl(fax->binst->fd, IMCTRLREQ, &creq);
					/* MISDN_CTRL_FILL_EMPTY is not mandatory warn if not supported */
					if (ret < 0)
						wprint("NCCI %06x: Error on MISDN_CTRL_FILL_EMPTY ioctl - %s\n",
							ncci->ncci, strerror(errno));
				}
			}
			dprint(MIDEBUG_NCCI, "NCCI %06x: PhaseB wait resumed %d (%d)\n", ncci->ncci,
				(int)pthread_self(), (int)fax->binst->tid);
		}
		break;
	case T30_DCN:
		dprint(MIDEBUG_NCCI, "NCCI %06x: PhaseB DCN %s\n", ncci->ncci, ident);
		break;
	case T30_DCS:
	case T30_DCS | 0x01:
		dprint(MIDEBUG_NCCI, "NCCI %06x: PhaseB DCS %s\n", ncci->ncci, ident);
		if (!fax->outgoing) {
			mc = alloc_mc_buf();
			if (mc) {
				ncciCmsgHeader(fax->ncci, mc, CAPI_CONNECT_B3, CAPI_IND);
				if (fax->lplci->Bprotocol.B3 == 4) {
					ncpi[0] = 0;
				} else {
					/* TODO proto 5 */
					ncpi[0] = 0;
				}
				mc->cmsg.NCPI = ncpi;
				ncciB3Message(fax->ncci, mc);
				free_mc_buf(mc);
			} else
				eprint("No msg buffer\n");
		}
		strncpy(fax->RemoteID, ident, 20);
		break;
	default:
		dprint(MIDEBUG_NCCI, "NCCI %06x: PhaseB called result 0x%02x :%s: ident %s\n", ncci->ncci,
			result, t30_frametype(result), ident);
		break;
	}
	return 0;
}

static int phaseD_handler(t30_state_t *t30, void *user_data, int result)
{
	struct fax *fax = user_data;
	struct mNCCI *ncci = fax->ncci;

	dprint(MIDEBUG_NCCI, "NCCI %06x: PhaseD called result 0x%02x :%s:\n", ncci->ncci,
		result, t30_frametype(result));
	return 0;
}

static void phaseE_handler(t30_state_t *t30, void *user_data, int result)
{
	struct fax *fax = user_data;
	struct mNCCI *ncci = fax->ncci;

	dprint(MIDEBUG_NCCI, "NCCI %06x: PhaseE called result 0x%02x\n", ncci->ncci, result);
	fax->phE_res = result;
	t30_get_transfer_statistics(t30, &fax->t30stats);
	StopDownLink(fax);
	dprint(MIDEBUG_NCCI, "NCCI %06x: BitRate %d\n", ncci->ncci, fax->t30stats.bit_rate);
	dprint(MIDEBUG_NCCI, "NCCI %06x: Pages RX: %d   TX: %d InFile: %d\n", ncci->ncci,
		fax->t30stats.pages_rx, fax->t30stats.pages_tx, fax->t30stats.pages_in_file);
	dprint(MIDEBUG_NCCI, "NCCI %06x: Resolution X: %d    Y: %d\n", ncci->ncci,
		fax->t30stats.x_resolution, fax->t30stats.y_resolution);
	dprint(MIDEBUG_NCCI, "NCCI %06x: Width: %d  Length: %d\n", ncci->ncci, fax->t30stats.width, fax->t30stats.length);
	dprint(MIDEBUG_NCCI, "NCCI %06x: Size: %d  Encoding: %d\n", ncci->ncci,
		fax->t30stats.image_size, fax->t30stats.encoding);
	dprint(MIDEBUG_NCCI, "NCCI %06x: Bad Rows: %d longest: %d\n", ncci->ncci,
		fax->t30stats.bad_rows, fax->t30stats.longest_bad_row_run);
	dprint(MIDEBUG_NCCI, "NCCI %06x: ECM Retries:%d  Status: %d\n", ncci->ncci,
		fax->t30stats.error_correcting_mode_retries, fax->t30stats.current_status);
	if (result == 0) {
		if (fax->outgoing) {
			FaxB3Disconnect(fax);
		} else {
			FaxPrepareReceivedData(fax);
		}
	} else {
		fax->b3transfer_error = 1;
		FaxB3Disconnect(fax);
	}
}

static void spandsp_msg_log(int level, const char *text)
{
	dprint(MIDEBUG_NCCI, "Spandsp: L%d, %s", level, text);
}

static int InitFax(struct fax *fax)
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
			if (fax->lplci->Bprotocol.B3 == 4)
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

static struct fax *mFaxCreate(struct BInstance	*bi)
{
	struct fax *nf;
	int ret, val;
	time_t cur_time;
	struct mISDN_ctrl_req creq;


	nf = calloc(1, sizeof(*nf));
	if (nf) {
		nf->file_d = -2;
		nf->ncci = ncciCreate(bi->lp);
		if (nf->ncci) {
			dprint(MIDEBUG_NCCI, "NCCI %06x: create\n", nf->ncci->ncci);
			nf->binst = bi;
			nf->lplci = bi->lp;
			nf->outgoing = nf->lplci->PLCI->outgoing;
			if (nf->lplci->Bprotocol.B1cfg[0])
				nf->rate = CAPIMSG_U16(nf->lplci->Bprotocol.B1cfg, 1);
			if (nf->lplci->Bprotocol.B3cfg[0]) {
				nf->b3_format = CAPIMSG_U16(nf->lplci->Bprotocol.B3cfg, 3);
				val = CAPIMSG_U16(nf->lplci->Bprotocol.B3cfg, 1);
				if (nf->lplci->Bprotocol.B3 == 4) {
					if (val)
						nf->high_res = 1;
				} else { /* 5 - extended */
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
					wprint("Cannot InitFax for incoming PLCI %04x\n", bi->lp ? bi->lp->plci : 0xffff);
					free(nf);
					nf = NULL;
				}
			}
			ncciL4L3(nf->ncci, PH_CONTROL_REQ, HW_FIFO_STATUS_ON, 0, NULL, NULL);
			/* Set buffersize */
			creq.op = MISDN_CTRL_RX_BUFFER;
			creq.channel = bi->nr;
			creq.p1 = DEFAULT_PKT_SIZE; // minimum
			creq.p2 = MISDN_CTRL_RX_SIZE_IGNORE; // do not change max
			creq.p3 = 0;
			ret = ioctl(bi->fd, IMCTRLREQ, &creq);
			/* MISDN_CTRL_RX_BUFFER is not mandatory warn if not supported */
			if (ret < 0)
				wprint("Error on MISDN_CTRL_RX_BUFFER  ioctl - %s\n", strerror(errno));
			else
				dprint(MIDEBUG_NCCI, "MISDN_CTRL_RX_BUFFER  old values: min=%d max=%d\n", creq.p1, creq.p2);
			nf->startdownlink = 1;
		} else {
			eprint("Cannot create NCCI for PLCI %04x\n", bi->lp ? bi->lp->plci : 0xffff);
			free(nf);
			nf = NULL;
		}
	}
	bi->b3data = nf;
	return nf;
}

void mFaxFree(struct fax *fax)
{
	int ret;

	dprint(MIDEBUG_NCCI, "Free fax data structs:%s%s%s%s\n",
		fax->fax ? " SPANDSP" : "",
		fax->ncci ? " NCCI" : "",
		fax->sff.data ? " SFF data" : "",
		fax->b3data ? " B3" :"");
	if (fax->fax)
		fax_free(fax->fax);
	fax->fax = NULL;
	if (fax->ncci)
		ncciReleaseLink(fax->ncci);
	fax->ncci = NULL;
	if (fax->sff.data) {
		dprint(MIDEBUG_NCCI, "SFF data:%p fax->b3data %p\n",
			fax->sff.data, fax->b3data);
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
			wprint("Temporary file %s - not deleted because %s\n", fax->file_name, strerror(errno));
		else
			dprint(MIDEBUG_NCCI, "Removed temporary file %s\n", fax->file_name);
	}
	free(fax);
}

static void mFaxRelease(struct fax *fax)
{
	dprint(MIDEBUG_NCCI, "fax->binst %p b3data %p\n", fax->binst, fax->binst ? fax->binst->b3data : NULL);
	if (fax->binst) {
		fax->binst->b3data = NULL;
	}
	mFaxFree(fax);
}

void FaxReleaseLink(struct BInstance *bi)
{
	struct fax *fax;

	if (!bi) {
		eprint("B instance gone\n");
		return;
	}
	fax = bi->b3data;
	if (!fax) {
		eprint("No fax struct\n");
		return;
	}
	bi->b3data = NULL;
	ncciReleaseLink(fax->ncci);
	fax->ncci = NULL;
	mFaxRelease(fax);
}

static void FaxSendDown(struct fax *fax, int ind)
{
	int pktid, l, tot, ret, i;
	struct mNCCI *ncci = fax->ncci;
	//struct mc_buf *mc;
	int16_t wav_buf[MAX_DATA_SIZE];
	int16_t *w;
	uint8_t *p, sbuf[MAX_DATA_SIZE];

	pthread_mutex_lock(&ncci->lock);
#ifdef USE_DLBUSY
	if (!ncci->xmit_handles[ncci->oidx].pkt) {
		ncci->dlbusy = 0;
		dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: no data\n", ncci->ncci);
		pthread_mutex_unlock(&ncci->lock);
		return;
	}
	mc = ncci->xmit_handles[ncci->oidx].pkt;
#endif
	if (!ncci->BIlink) {
		wprint("NCCI %06x: BInstance is gone - packet ignored\n", ncci->ncci);
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
			wprint("NCCI %06x: dlbusy set\n", ncci->ncci);
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
	pthread_mutex_unlock(&ncci->lock);
	l = DEFAULT_PKT_SIZE;
	ret = fax_tx(fax->fax, wav_buf, l);
	dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: got %d/%d samples from faxmodem(%p)\n",
		ncci->ncci, ret, l, fax->fax);

	w = wav_buf;
	p = sbuf;
	l = ret;
	// convert (pcm -> alaw)
	for (i = 0; i < ret; i++)
		*p++ = slin2alaw[*w++];
	pthread_mutex_lock(&ncci->lock);
	ncci->down_iv[1].iov_len = l;
	ncci->down_iv[1].iov_base = sbuf;
#endif
	ncci->down_header.id = pktid;
	ncci->down_msg.msg_iovlen = 2;
	tot = l + ncci->down_iv[0].iov_len;
	ret = sendmsg(ncci->BIlink->fd, &ncci->down_msg, MSG_DONTWAIT);
	if (ret != tot) {
		wprint("NCCI %06x: send returned %d while sending %d bytes type %s id %d - %s\n", ncci->ncci, ret, tot,
		       _mi_msg_type2str(ncci->down_header.prim), ncci->down_header.id, strerror(errno));
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
		dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: send down %d bytes type %s id %d current oidx[%d] sent %d/%d %s\n",
			ncci->ncci, ret, _mi_msg_type2str(ncci->down_header.prim), ncci->down_header.id, ncci->oidx,
			ncci->xmit_handles[ncci->oidx].sent, ncci->xmit_handles[ncci->oidx].dlen,
			ind ? "ind" : "cnf");
#else
		dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: send down %d bytes type %s id %d sent %d - %p\n",
			ncci->ncci, ret, _mi_msg_type2str(ncci->down_header.prim), ncci->down_header.id, ret, fax->fax);
#endif
#ifdef USE_DLBUSY
	ncci->dlbusy = 1;
	ncci->oidx++;
	if (ncci->oidx == ncci->window)
		ncci->oidx = 0;
#endif
	pthread_mutex_unlock(&ncci->lock);
}

static int FaxDataInd(struct fax *fax, struct mc_buf *mc)
{
	int i, ret;
	struct mNCCI *ncci = fax->ncci;
	uint16_t dlen;
	struct mISDNhead *hh;
	int16_t wav_buf[MAX_DATA_SIZE];
	int16_t *w;
	unsigned char *p;
	fax_state_t *fst;

	hh = (struct mISDNhead *)mc->rb;
	dlen = mc->len - sizeof(*hh);
	pthread_mutex_lock(&ncci->lock);
	if ((!ncci->BIlink) || (!fax->fax)) {
		pthread_mutex_unlock(&ncci->lock);
		wprint("NCCI %06x: frame with %d bytes dropped Blink(%p) or sandsp fax(%p) gone\n",
			ncci->ncci, dlen, ncci->BIlink, fax->fax);
		return -EINVAL;
	} else
		dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: fax(%p)\n", ncci->ncci, fax->fax);
	fst = fax->fax;
	pthread_mutex_unlock(&ncci->lock);

	if (dlen > MAX_DATA_SIZE) {
		wprint("NCCI %06x: frame overflow %d/%d - truncated\n", ncci->ncci, dlen, MAX_DATA_SIZE);
		dlen =  MAX_DATA_SIZE;
	}
	w = wav_buf;
	mc->rp = mc->rb + sizeof(*hh);
	p = mc->rp;
	// convert (alaw -> pcm)
	for (i = 0; i < dlen; i++)
		*w++ = alaw2lin[*p++];

	ret = fax_rx(fst, wav_buf, dlen);
	dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: send %d samples to faxmodem(%p)ret %d\n", ncci->ncci, dlen, fax->fax, ret);
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
		wprint("Fax NCCI %06x: SendQueueFull\n", ncci->ncci);
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
		FaxSendDown(fax, 1);
#else
	free_mc_buf(mc);
	if (fax->startdownlink) {
		fax->startdownlink = 0;
		FaxSendDown(fax, 1);
	}
#endif
	return 0;
}

static void FaxDataConf(struct fax *fax, struct mc_buf *mc)
{
#ifdef USE_DLBUSY
	int i;
	struct mNCCI *ncci = fax->ncci;
	struct mISDNhead *hh;

	hh = (struct mISDNhead *)mc->rb;
	if (ncci->flowmode != flmPHDATA) {
		wprint("Fax NCCI %06x: Got DATA confirm for %x - but flow mode(%d)\n", ncci->ncci, hh->id, ncci->flowmode);
		free_mc_buf(mc);
		return;
	}

	pthread_mutex_lock(&ncci->lock);
	for (i = 0; i < ncci->window; i++) {
		if (ncci->xmit_handles[i].PktId == hh->id) {
			if (ncci->xmit_handles[i].pkt)
				break;
		}
	}
	if (i == ncci->window) {
		wprint("Fax NCCI %06x: Got DATA confirm for %x - but ID not found\n", ncci->ncci, hh->id);
		for (i = 0; i < ncci->window; i++)
			wprint("Fax NCCI %06x: PktId[%d] %x\n", ncci->ncci, i, ncci->xmit_handles[i].PktId);
		pthread_mutex_unlock(&ncci->lock);
		free_mc_buf(mc);
		return;
	}
	dprint(MIDEBUG_NCCI_DATA, "Fax NCCI:%06x: confirm xmit_handles[%d] pktid=%x handle=%d\n", ncci->ncci, i, hh->id,
	       ncci->xmit_handles[i].DataHandle);
	free_mc_buf(mc);
	mc = ncci->xmit_handles[i].pkt;
	ncci->xmit_handles[i].pkt = NULL;
	ncci->xmit_handles[i].PktId = 0;
	ncci->dlbusy = 0;
	free_mc_buf(mc);
	pthread_mutex_unlock(&ncci->lock);
	FaxSendDown(fax, 0);
#else
	free_mc_buf(mc);
	FaxSendDown(fax, 0);
#endif
}

static void FaxDataResp(struct fax *fax, struct mc_buf *mc)
{
	int i;
	struct mNCCI *ncci = fax->ncci;
	uint16_t dh = CAPIMSG_RESP_DATAHANDLE(mc->rb);

	pthread_mutex_lock(&ncci->lock);
	for (i = 0; i < ncci->window; i++) {
		if (ncci->recv_handles[i] == dh) {
			dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: data handle %d acked at pos %d\n", ncci->ncci, dh, i);
			ncci->recv_handles[i] = 0;
			break;
		}
	}
	if (i == ncci->window) {
		char deb[128], *dp;

		dp = deb;
		for (i = 0; i < ncci->window; i++)
			dp += sprintf(dp, " [%d]=%d", i, ncci->recv_handles[i]);
		wprint("NCCI %06x: data handle %d not in%s\n", ncci->ncci, dh, deb);
	}
	pthread_mutex_unlock(&ncci->lock);
	if (fax->b3transfer_active)
		FaxB3Ind(fax);
	if (fax->b3transfer_end)
		FaxB3Disconnect(fax);
}

static int FaxWriteTiff(struct fax *fax, struct mc_buf *mc)
{
	int ret, res = 0;

	if (fax->file_d == -2) {
		fax->file_d = open(fax->file_name, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP);
		if (fax->file_d < 0) {
			res = -errno;
			wprint("NCCI %06x: Cannot open TIFF %s for writing - %s\n", fax->ncci->ncci, fax->file_name, strerror(errno));
		}
	}
	if (fax->file_d < 0) {
		if (!res)
			res = -1;
	} else {
		ret = write(fax->file_d, mc->rp, mc->len);
		if (ret != mc->len) {
			res = -errno;
			wprint("NCCI %06x: Write data to %s only %d/%d - %s\n", fax->ncci->ncci, fax->file_name,
					ret, mc->len, strerror(ret));
			if (!res)
				res = -1;
		}
	}
	return res;
}

static int FaxDataB3Req(struct fax *fax, struct mc_buf *mc)
{
	struct mNCCI *ncci = fax->ncci;
	int ret;
	uint16_t off, dlen, flg, dh, info;
	// unsigned char *dp;

	off = CAPIMSG_LEN(mc->rb);
	if (off != 22 && off != 30) {
		wprint("NCCI %06x: Illegal message len %d\n", ncci->ncci, off);
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
		wprint("NCCI %06x: B3 data format %d not supported\n", ncci->ncci, fax->b3_format);
		ret = -EINVAL;
		break;
	}
	if (ret < 0) {
		wprint("Fax NCCI %06x: handle = %d flags = %04x data offset %d delivered error ret %d\n",
			ncci->ncci, CAPIMSG_REQ_DATAHANDLE(mc->rb), flg, off, ret);
		fax->b3transfer_error = ret;
	} else
		dprint(MIDEBUG_NCCI_DATA, "Fax NCCI %06x: handle = %d flags = %04x data offset %d delivered ret %d\n",
			ncci->ncci, CAPIMSG_REQ_DATAHANDLE(mc->rb), flg, off, ret);

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
			FaxB3Disconnect(fax);
			if (fax->binst && fax->binst->waiting)
				sem_post(&fax->binst->wait);
		} else {
			if (fax->lplci->Bprotocol.B3 == 4) {
				ret = InitFax(fax);
				if (ret) {
					wprint("Fax NCCI %06x: cannot init Faxmodem return %d\n", ncci->ncci, ret);
					fax->b3transfer_error = 1;
					FaxB3Disconnect(fax);
				}
			} else {
				t30_set_tx_file(fax->t30, fax->file_name, -1, -1);
				if (fax->binst && fax->binst->waiting)
					sem_post(&fax->binst->wait);
			}
		}
	}
	return 0;
}

static int FaxDisconnectB3Req(struct fax *fax, struct mc_buf *mc)
{
	struct mNCCI *ncci = fax->ncci;
	int ret;

	dprint(MIDEBUG_NCCI, "Fax NCCI %06x: DisconnectB3Req b3transfer %s\n",
		ncci->ncci, fax->b3transfer_active ? "active" : "not active");
	SendCmsgAnswer2Application(ncci->appl, mc, 0);
	pthread_mutex_lock(&ncci->lock);
	if (fax->b3transfer_active) {
		fax->b3transfer_end = 1;
		fax->b3transfer_active = 0;
		switch (fax->b3_format) {
		case FAX_B3_FORMAT_SFF:
			wprint("Fax NCCI %06x: got no EOF - fax document incomplete\n", ncci->ncci);
			fax->b3transfer_error = -1;
			break;
		case FAX_B3_FORMAT_TIFF:
			if (fax->file_d >= 0) {
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
			FaxB3Disconnect(fax);
		} else {
			if (fax->lplci->Bprotocol.B3 == 4) {
				ret = InitFax(fax);
				if (ret) {
					wprint("Fax NCCI %06x: cannot init Faxmodem return %d\n", ncci->ncci, ret);
					fax->b3transfer_error = 1;
					FaxB3Disconnect(fax);
				}
			} else {
				if (fax->binst && fax->binst->waiting)
					sem_post(&fax->binst->wait);
			}
		}
	} else {
		if (fax->binst && fax->binst->waiting)
			sem_post(&fax->binst->wait);
	}
	pthread_mutex_unlock(&ncci->lock);
	return 0;
}


static int FaxConnectB3Req(struct fax *fax, struct mc_buf *mc)
{
	struct mNCCI *ncci = fax->ncci;
	uint16_t info = 0;
	int ret = 0;
	int val;

	if (mc->cmsg.NCPI && mc->cmsg.NCPI[0]) {
		val = CAPIMSG_U16(mc->cmsg.NCPI, 5);
		if (val != fax->b3_format) {
			dprint(MIDEBUG_NCCI, "NCCI %06x: Fax changed format %d ->%d\n", ncci->ncci, fax->b3_format, val);
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
		if (fax->lplci->Bprotocol.B3 == 5) {
			val = CAPIMSG_U16(mc->cmsg.NCPI, 3);
			fax->high_res = (val & 0x0001) ? 1 : 0;
			fax->polling = (val & 0x0002) ? 1 : 0;
			fax->more_doc = (val & 0x0004) ? 1 : 0;
			fax->no_ecm = (val & 0x8000) ? 1 : 0;
		}
	}
	mc->cmsg.Subcommand = CAPI_CONF;
	mc->cmsg.adr.adrNCCI = ncci->ncci;
	mc->cmsg.Info = info;
	pthread_mutex_lock(&ncci->lock);
	if (info) {
		fax->b3transfer_end = 1;
		fax->b3transfer_error = info;
		pthread_mutex_unlock(&ncci->lock);
		ret = ncciB3Message(fax->ncci, mc);
	} else {
		fax->b3transfer_active = 1;
		pthread_mutex_unlock(&ncci->lock);
		ret = ncciB3Message(fax->ncci, mc);
		if (!ret) {
			if (fax->lplci->Bprotocol.B3 == 4)
				FaxConnectActiveB3(fax, mc);
			else {
				ret = InitFax(fax);
				if (ret) {
					wprint("Fax NCCI %06x: cannot init Faxmodem return %d\n", ncci->ncci, ret);
					fax->b3transfer_error = 1;
					FaxB3Disconnect(fax);
				}
			}
		} else {
			wprint("NCCI %06x: CAPI_CONNECT_B3_CONF not handled by ncci statemachine\n", ncci->ncci);
		}
	}
	return ret;
}

int FaxB3Message(struct BInstance *bi, struct mc_buf *mc)
{
	struct fax *fax = bi->b3data;
	int retval = CapiNoError;
	uint8_t cmd, subcmd;

	cmd = CAPIMSG_COMMAND(mc->rb);
	subcmd = CAPIMSG_SUBCOMMAND(mc->rb);
	if (!fax) {
		if (cmd == CAPI_CONNECT_B3 && subcmd == CAPI_REQ) {
			fax = mFaxCreate(bi);
		}
		if (fax) {
			bi->b3data = fax;
		} else {
			wprint("No Fax struct assigned\n");
			return CapiMsgOSResourceErr;
		}
	}
	switch (CAPICMD(cmd, subcmd)) {

	case CAPI_DATA_B3_REQ:
		retval = FaxDataB3Req(fax, mc);
		break;
	case CAPI_CONNECT_B3_REQ:
		retval = FaxConnectB3Req(fax, mc);
		break;
	case CAPI_DATA_B3_RESP:
		FaxDataResp(fax, mc);
		retval = 0;
		break;
	case CAPI_CONNECT_B3_RESP:
		retval = ncciB3Message(fax->ncci, mc);
		break;
	case CAPI_CONNECT_B3_ACTIVE_RESP:
		retval = ncciB3Message(fax->ncci, mc);
		break;
	case CAPI_DISCONNECT_B3_REQ:
		retval = FaxDisconnectB3Req(fax, mc);
		break;
	case CAPI_DISCONNECT_B3_RESP:
		retval = ncciB3Message(fax->ncci, mc);
		if (!retval)
			fax->ncci = NULL;
		mFaxRelease(fax);
		free_mc_buf(mc);
		return 0;
#if 0
	case CAPI_FACILITY_REQ:
		retval = FsmEvent(&fax->ncci->ncci_m, EV_AP_FACILITY_REQ, mc);
		break;
	case CAPI_FACILITY_RESP:
		/* no need to handle */
		retval = 0;
		break;
	case CAPI_MANUFACTURER_REQ:
		retval = FsmEvent(&fax->ncci->ncci_m, EV_AP_MANUFACTURER_REQ, mc);
		break;
#endif
	default:
		eprint("NCCI %06x: Error Unhandled command %02x/%02x\n", fax->ncci->ncci, cmd, subcmd);
		retval = CapiMessageNotSupportedInCurrentState;
	}
	if (retval) {
		if (subcmd == CAPI_REQ)
			retval = CapiMessageNotSupportedInCurrentState;
		else {		/* RESP */
			wprint("NCCI %06x: Error Message %02x/%02x not supported\n", fax->ncci->ncci, cmd, subcmd);
			retval = CapiNoError;
		}
	} else if (!(cmd == CAPI_DATA_B3 && subcmd == CAPI_REQ)) {
		free_mc_buf(mc);
	}
	return retval;
}

int FaxRecvBData(struct BInstance *bi, struct mc_buf *mc)
{
	struct mISDNhead *hh;
	struct fax *fax = bi->b3data;
	int ret = 0;

	hh = (struct mISDNhead *)mc->rb;
	switch (hh->prim) {
	case PH_DATA_IND:
		if (!fax) {
			wprint("Controller%d ch%d: Got %s but but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr,
				_mi_msg_type2str(hh->prim));
			ret = -EINVAL;
			break;
		} else
			ret = FaxDataInd(fax, mc);
		break;
	case PH_DATA_CNF:
		if (!fax) {
			wprint("Controller%d ch%d: Got %s but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr,
				_mi_msg_type2str(hh->prim));
			ret = -EINVAL;
		} else {
			FaxDataConf(fax, mc);
			ret = 0;
		}
		break;
	case PH_ACTIVATE_IND:
	case PH_ACTIVATE_CNF:
		if (!fax) {
			fax = mFaxCreate(bi);
			if (!fax) {
				eprint("Cannot create Fax struct for PLCI %04x\n", bi->lp ? bi->lp->plci : 0xffff);
				return -ENOMEM;
			}
		}
		ret = 1;
		break;
	case PH_DEACTIVATE_IND:
		if (!fax) {
			wprint("Controller%d ch%d: Got %s but but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr,
				_mi_msg_type2str(hh->prim));
			ret = -EINVAL;
			break;
		}
		fax->modem_end = 1;
		fax->modem_active = 0;
		ret = 1;
		break;
	case PH_DEACTIVATE_CNF:
		if (!fax) {
			wprint("Controller%d ch%d: Got %s but but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr,
				_mi_msg_type2str(hh->prim));
			ret = -EINVAL;
			break;
		}
		fax->modem_end = 1;
		fax->modem_active = 0;
		ret = 1;
		break;
	case PH_CONTROL_CNF:
		if (!fax) {
			wprint("Controller%d ch%d: Got %s id %x but but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr,
				_mi_msg_type2str(hh->prim), hh->id);
			ret = -EINVAL;
		} else
			dprint(MIDEBUG_NCCI, "NCCI %06x: got %s id %x\n", fax->ncci->ncci, _mi_msg_type2str(hh->prim), hh->id);
		break;
	case PH_CONTROL_IND:
		if (!fax) {
			wprint("Controller%d ch%d: Got %s id %x but but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr,
				_mi_msg_type2str(hh->prim), hh->id);
			ret = -EINVAL;
		} else
			dprint(MIDEBUG_NCCI, "NCCI %06x: got %s id %x\n", fax->ncci->ncci, _mi_msg_type2str(hh->prim), hh->id);
		break;
	default:
		wprint("Controller%d ch%d: Got %s (%x) id=%x len %d\n", bi->pc->profile.ncontroller, bi->nr,
			_mi_msg_type2str(hh->prim), hh->prim, hh->id, mc->len);
		ret = -EINVAL;
	}
	return ret;
}

#endif // USE_SOFTFAX
