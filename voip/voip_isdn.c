#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include "g711.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "isdn_net.h"
#include "l3dss1.h"
#include "helper.h"
#include "bchannel.h"
#include "tone.h"
#include "isound.h"
#include "globals.h"

#include "iapplication.h"

vapplication_t	voip;

static int
getnext_record(FILE *f)
{
	int	opt = 0;
	char	line[128];

	while(!feof(f)) {
		if (fgets(line, 128, f)) {
//			fprintf(stderr, "%s: line:%s", __FUNCTION__, line);
			if (line[0]=='\n')
				continue;
			if (line[0]==0)
				continue;
			if (line[0]=='#')
				continue;
			sscanf(line,"%d", &opt);
			return(opt);
		}
	}
	return(0);
}

int
read_rec_ctrlfile(void)
{
	FILE		*f;
	int		opt;
	manager_t	*mgr = voip.mgr_lst;

	if (RecordCtrlFile[0] == 0) {
		dprint(DBGM_TOPLEVEL, "%s: no RecordCtrlFile\n", __FUNCTION__);
		return(-ENOENT);
	}
	f = fopen(RecordCtrlFile, "r");
	if (!f) {
		dprint(DBGM_TOPLEVEL, "%s: cannot open %s: %s\n", __FUNCTION__,
			RecordCtrlFile, strerror(errno));
		return(-errno);
	}
	while(mgr) {
		opt = getnext_record(f);
		dprint(DBGM_TOPLEVEL, "%s: mgr %p ch1: %d\n", __FUNCTION__,
			mgr, opt);
		if (opt) {
			mgr->bc[0].Flags |= FLG_BC_RECORD;
		} else {
			mgr->bc[0].Flags &= ~FLG_BC_RECORD;
		}
		opt = getnext_record(f);
		dprint(DBGM_TOPLEVEL, "%s: mgr %p ch2: %d\n", __FUNCTION__,
			mgr, opt);
		if (opt) {
			mgr->bc[1].Flags |= FLG_BC_RECORD;
		} else {
			mgr->bc[1].Flags &= ~FLG_BC_RECORD;
		}
		mgr = mgr->next;
	}
	fclose(f);
	return(0);
}

static void
sig_usr1_handler(int sig)
{
	dprint(DBGM_TOPLEVEL, "%s: got sig(%d)\n", __FUNCTION__, sig);
	read_rec_ctrlfile();
	signal(SIGUSR1, sig_usr1_handler);
}

#if 0
static void
sig_segfault(int sig, siginfo_t *si, void *arg) {
	int	i,*ip = arg;

	dprint(DBGM_TOPLEVEL, "segfault %d, %p, %p\n",
		sig, si, arg);
	if (si) {
		dprint(DBGM_TOPLEVEL, "si: sig(%d) err(%d) code(%d) pid(%d)\n",
			si->si_signo, si->si_errno, si->si_code, si->si_pid);
		dprint(DBGM_TOPLEVEL, "si: status(%x) value(%x)\n",
			si->si_status, si->si_value.sival_int);
		dprint(DBGM_TOPLEVEL, "si: int(%x) ptr(%p) addr(%p)\n",
			si->si_int, si->si_ptr, si->si_addr);
	}
	if (ip) {
		ip -= 10;
		for(i=0;i<20;i++)
			dprint(DBGM_TOPLEVEL, "ip %3d: %x\n", i-10, ip[i]);
	}
	ip = (int *)si;
	if (ip) {
		ip -= 10;
		for(i=0;i<20;i++)
			dprint(DBGM_TOPLEVEL, "si %3d: %x\n", i-10, ip[i]);
	}

}
#endif

static void
term_handler(int sig)
{
	pthread_t	tid;
	manager_t	*mgr = voip.mgr_lst;

	tid = pthread_self();
	dprint(DBGM_TOPLEVEL,"signal %d received from thread %ld\n", sig, tid);
	voip.flags |= AP_FLG_VOIP_ABORT;
	while(mgr) {
		term_netstack(mgr->nst);
		term_bchannel(&mgr->bc[0]);
		term_bchannel(&mgr->bc[1]);
		mgr = mgr->next;
	}
}

#if 0

static void
child_handler(int sig)
{
	manager_t	*mgr = voip.mgr_lst;
	pid_t		pid;
	int		stat;

	dprint(DBGM_TOPLEVEL,"signal %d received\n", sig);
	while (mgr) {
		if (mgr->bc[0].pid) {
			pid = waitpid(mgr->bc[0].pid, &stat, WNOHANG);
			dprint(DBGM_TOPLEVEL,  "%s: waitpid(%d) stat(%x) ret(%d)\n", __FUNCTION__,
				mgr->bc[0].pid, stat, pid);
			if (mgr->bc[0].pid == pid) {
				mgr->bc[0].pid = 0;
//				if (mgr->bc[0].state == BC_STATE_ACTIV)
				break;
			}
		}
		if (mgr->bc[1].pid) {
			pid = waitpid(mgr->bc[1].pid, &stat, WNOHANG);
			dprint(DBGM_TOPLEVEL,  "%s: waitpid(%d) stat(%x) ret(%d)\n", __FUNCTION__,
				mgr->bc[1].pid, stat, pid);
			if (mgr->bc[1].pid == pid) {
				mgr->bc[1].pid = 0;
//				if (mgr->bc[1].state == BC_STATE_ACTIV)
				break;
			}
		}
		mgr = mgr->next;
	}
	signal(SIGCHLD, child_handler);
}

#endif

static void *
read_audio(void *arg)
{
	isound_t	*ia = arg;
	pthread_t	tid;
	fd_set		rfd, efd;
	int		ret,i;

	tid = pthread_self();
	dprint(DBGM_TOPLEVEL,  "%s: tid %ld\n", __FUNCTION__, tid);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	while(1) {
		FD_ZERO(&rfd);
		FD_ZERO(&efd);
		FD_SET(ia->data, &rfd);
		FD_SET(ia->data, &efd);
		ret = select(ia->data +1, &rfd, NULL, &efd, NULL);
		if (ret < 0) {
			dprint(DBGM_TOPLEVEL,  "%s: select error %d %s\n", __FUNCTION__,
				errno, strerror(errno));
			if (errno == EAGAIN)
				continue;
			if (errno == EINTR)
				continue;
		}
		if (FD_ISSET(ia->data, &rfd)) {
			ret = read(ia->data, ia->rtmp, MAX_AUDIO_READ);
			if (ret < 0) {
				dprint(DBGM_TOPLEVEL,  "%s: read error %d %s\n", __FUNCTION__,
					errno, strerror(errno));
				if (errno == EAGAIN)
					continue;
				continue;
			}
			if (!ret) {
				dprint(DBGM_TOPLEVEL,  "%s: zero read\n", __FUNCTION__);
				continue;
			}
			if (ret > ibuf_freecount(ia->rbuf)) {
				dprint(DBGM_TOPLEVEL,  "%s: rbuf overflow %d/%d\n", __FUNCTION__,
					ret, ibuf_freecount(ia->rbuf));
				ret = ibuf_freecount(ia->rbuf);
			}
			for (i=0; i<ret; i++)
				ia->rtmp[i] = ulaw2alaw(ia->rtmp[i]);
			ibuf_memcpy_w(ia->rbuf, ia->rtmp, ret);
			if (ia->rbuf->rsem)
				sem_post(ia->rbuf->rsem);
		}
		if (FD_ISSET(ia->data, &efd)) {
			dprint(DBGM_TOPLEVEL,  "%s: exception\n", __FUNCTION__);
			break;
		}
	}
	return NULL;
}

static void *
work_audio(void *arg)
{
	isound_t	*ia = arg;
	pthread_t	tid;
	int		ret, i;

	tid = pthread_self();
	dprint(DBGM_TOPLEVEL,  "%s: tid %ld\n", __FUNCTION__, tid);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	while(1) {
		sem_wait(&ia->work);
		if (ia->wlen) {
			ret = write(ia->data, &ia->wtmp[ia->widx], ia->wlen);
			if (ret == -1)
				continue;
			if (ret < ia->wlen) {
				ia->wlen -= ret;
				ia->widx += ret;
				continue;
			}
			ia->wlen = 0;
			ia->widx = 0;
		}
		if ((ia->wlen = ibuf_usedcount(ia->sbuf))) {
			ibuf_memcpy_r(ia->wtmp, ia->sbuf, ia->wlen);
			for (i=0; i<ia->wlen; i++)
				ia->wtmp[i] = alaw2ulaw(ia->wtmp[i]);
			ret = write(ia->data, &ia->wtmp[0], ia->wlen);
			if (ret == -1)
				continue;
			if (ret < ia->wlen) {
				ia->wlen -= ret;
				ia->widx = ret;
				continue;
			}
			ia->wlen = 0;
		}
	}
	return(NULL);
}

int
setup_voip(iapplication_t *ap, bchannel_t *bc)
{
	isound_t	*ia;
	int		ret;

	dprint(DBGM_APPL, "%s(%p, %p)\n", __FUNCTION__, ap, bc);
	if (!bc)
		return(-EINVAL);
	if (!ap)
		return(-EINVAL);
	if (!bc->sbuf)
		return(-EINVAL);
	if (bc->rbuf)
		return(-EINVAL);
	bc->rbuf = init_ibuffer(2048);
	if (!bc->rbuf)
		return(-ENOMEM);
	bc->rbuf->wsem = &bc->work;
	ia = malloc(sizeof(isound_t));
	if (!ia)
		return(-ENOMEM);
	memset(ia, 0, sizeof(isound_t));
	ap->data2 = ia;
	sem_init(&ia->work, 0, 0);
	ia->sbuf = bc->rbuf;
	ia->rbuf = bc->sbuf;
	bc->sbuf->wsem = &ia->work;
	bc->rbuf->rsem = &ia->work;
	ret = pthread_create(&ap->tid, NULL, voip_sender, (void *)ap);
	dprint(DBGM_APPL,  "%s: create voip_sender %ld ret %d\n", __FUNCTION__,
		ap->tid, ret);
	return(ret);
}

int
close_voip(iapplication_t *ap, bchannel_t *bc)
{
	isound_t	*ia;
	int		ret, *retval;

	dprint(DBGM_APPL, "%s(%p, %p)\n", __FUNCTION__, ap, bc);
	if (!bc)
		return(-EINVAL);
	if (!ap)
		return(-EINVAL);
	ia = ap->data2;
	ap->data2 = NULL;
	ap->Flags &= ~AP_FLG_VOIP_ACTIV;
	if (!ia)
		return(-EINVAL);
	ret = pthread_cancel(ap->tid);
	dprint(DBGM_APPL, "%s: cancel sender ret(%d)\n", __FUNCTION__,
		ret);
	ret = pthread_join(ap->tid, (void *)&retval);
	dprint(DBGM_APPL, "%s: join sender ret(%d) rval(%p)\n", __FUNCTION__,
		ret, retval);
	ia->sbuf = NULL;
	ia->rbuf = NULL;
	if (bc->sbuf)
		bc->sbuf->wsem = NULL;
	if (bc->rbuf)
		free_ibuffer(bc->rbuf);
	bc->rbuf = NULL;
	ret = sem_destroy(&ia->work);
	dprint(DBGM_APPL, "%s: sem_destroy work %d\n", __FUNCTION__,
		ret);
	free(ia);
	return(0);
}


static int
setup_audio(iapplication_t *ap, bchannel_t *bc)
{
	isound_t	*ia;
	int		ret;

	if (!bc)
		return(-EINVAL);
	if (!ap)
		return(-EINVAL);
	if (!bc->sbuf)
		return(-EINVAL);
	if (bc->rbuf)
		return(-EINVAL);
	bc->rbuf = init_ibuffer(2048);
	if (!bc->rbuf)
		return(-ENOMEM);
	bc->rbuf->wsem = &bc->work;
	ia = malloc(sizeof(isound_t));
	if (!ia)
		return(-ENOMEM);
	memset(ia, 0, sizeof(isound_t));
	sem_init(&ia->work, 0, 0);
	ia->data = open("/dev/audio", O_RDWR | O_NONBLOCK);
	if (ia->data < 0) {
		free(ia);
		dprint(DBGM_TOPLEVEL,  "%s: open rdwr %d %s\n", __FUNCTION__,
			errno, strerror(errno));
		return(-errno);
	}
	ap->data2 = ia;
	ia->sbuf = bc->rbuf;
	ia->rbuf = bc->sbuf;
	bc->sbuf->wsem = &ia->work;
	bc->rbuf->rsem = &ia->work;
	ret = pthread_create(&ia->rd_t, NULL, read_audio, (void *)ia);
	dprint(DBGM_TOPLEVEL,  "%s: create rd_t %ld ret %d\n", __FUNCTION__,
		ia->rd_t, ret);
	ret = pthread_create(&ia->wr_t, NULL, work_audio, (void *)ia);
	dprint(DBGM_TOPLEVEL,  "%s: create wr_t %ld ret %d\n", __FUNCTION__,
		ia->wr_t, ret);
	return(0);
}

static int
close_audio(iapplication_t *ap, bchannel_t *bc)
{
	isound_t	*ia;
	int		ret, *retval;

	if (!bc)
		return(-EINVAL);
	if (!ap)
		return(-EINVAL);
	ia = ap->data2;
	ap->data2 = NULL;
	if (!ia)
		return(-EINVAL);
	close(ia->data);
	ret = pthread_cancel(ia->rd_t);
	dprint(DBGM_TOPLEVEL, "%s: cancel rd_t ret(%d)\n", __FUNCTION__,
		ret);
	ret = pthread_cancel(ia->wr_t);
	dprint(DBGM_TOPLEVEL, "%s: cancel wr_t ret(%d)\n", __FUNCTION__,
		ret);
	ret = pthread_join(ia->rd_t, (void *)&retval);
	dprint(DBGM_TOPLEVEL, "%s: join rd_t ret(%d) rval(%p)\n", __FUNCTION__,
		ret, retval);
	ret = pthread_join(ia->wr_t, (void *)&retval);
	dprint(DBGM_TOPLEVEL, "%s: join wr_t ret(%d) rval(%p)\n", __FUNCTION__,
		ret, retval);
	ia->sbuf = NULL;
	ia->rbuf = NULL;
	if (bc->sbuf)
		bc->sbuf->wsem = NULL;
	if (bc->rbuf)
		free_ibuffer(bc->rbuf);
	bc->rbuf = NULL;
	ret = sem_destroy(&ia->work);
	dprint(DBGM_TOPLEVEL, "%s: sem_destroy work %d\n", __FUNCTION__,
		ret);
	free(ia);
	return(0);
}

static int
route_call(iapplication_t *ap, bchannel_t *bc)
{
	bchannel_t	*newbc = NULL;
	int		ret;

	if (bc) {
		display_NR_IE(bc->msn,  __FUNCTION__, ": msn");
		display_NR_IE(bc->nr,   __FUNCTION__, ":  nr");
	}
	ap->data1 = bc;
	if (!bc)
		return(-EINVAL);
	read_rec_ctrlfile();
	if (bc->usednr->typ == NR_TYPE_INTERN) {
		ap->mode = AP_MODE_INTERN_CALL;
		ret = ap->mgr->app_bc(ap->mgr, PR_APP_OCHANNEL, &newbc);
		if (0 >= ret)
			dprint(DBGM_TOPLEVEL,  "%s: no free channel ret(%d)\n", __FUNCTION__,
				ret);
		if (!newbc) {
			bc->cause_loc = CAUSE_LOC_PNET_LOCUSER;
			bc->cause_val = CAUSE_USER_BUSY;
			ap->mgr->app_bc(ap->mgr, PR_APP_HANGUP, bc);
			return(0);
		}
		newbc->app = ap;
		ap->data2 = newbc;
		newbc->Flags |= FLG_BC_APPLICATION;
		newbc->msn[0] = bc->usednr->len +1;
		newbc->msn[1] = 0x81;
		memcpy(&newbc->msn[2], bc->usednr->nr, bc->usednr->len);
		if (bc->msn[0])
			memcpy(newbc->nr, bc->msn, bc->msn[0] + 1);
		newbc->l1_prot = ISDN_PID_L1_B_64TRANS;
		ap->mgr->app_bc(ap->mgr, PR_APP_OCALL, newbc);
	} else if (bc->usednr->typ == NR_TYPE_AUDIO) {
		if (ap->vapp->flags & AP_FLG_AUDIO_USED) {
			bc->cause_loc = CAUSE_LOC_PNET_LOCUSER;
			bc->cause_val = CAUSE_USER_BUSY;
			ap->mgr->app_bc(ap->mgr, PR_APP_HANGUP, bc);
			return(0);
		} else
			ap->vapp->flags |= AP_FLG_AUDIO_USED;
		ap->mode = AP_MODE_AUDIO_CALL;
		bc->Flags |= FLG_BC_PROGRESS;
		ap->mgr->app_bc(ap->mgr, PR_APP_ALERT, bc);
		ret = setup_audio(ap, bc);
		if (ret) {
			ap->vapp->flags &= ~AP_FLG_AUDIO_USED;
			bc->cause_loc = CAUSE_LOC_PNET_LOCUSER;
			bc->cause_val = CAUSE_INCOMPATIBLE_DEST;
			ap->mgr->app_bc(ap->mgr, PR_APP_HANGUP, bc);
			return(0);
		}
		ap->Flags |= AP_FLG_AUDIO_ACTIV;
		strcpy(bc->display,"connect to AUDIO");
		ap->mgr->app_bc(ap->mgr, PR_APP_CONNECT, bc);
	} else if (bc->usednr->typ == NR_TYPE_VOIP) {
		ap->mode = AP_MODE_VOIP_OCALL;
		ret = setup_voip_ocall(ap, bc);
	}
	return(0);
}

static int
connect_call(iapplication_t *ap, bchannel_t *bc)
{
	bchannel_t	*peer = NULL;
	int		ret;

	read_rec_ctrlfile();
	if (ap->mode == AP_MODE_INTERN_CALL) {
		if (ap->data1 == bc) {
			peer = ap->data2;
		} else if (ap->data2 == bc) {
			peer = ap->data1;
		}
		if (peer) {
			ap->mgr->app_bc(ap->mgr, PR_APP_CONNECT, peer);
			bc->rbuf = peer->sbuf;
			peer->rbuf = bc->sbuf;
			if (bc->sbuf)
				bc->sbuf->rsem = &peer->work;
			if (peer->sbuf)
				peer->sbuf->rsem = &bc->work;
		} else {
			return(-EINVAL);
		}
	} else if (ap->mode == AP_MODE_VOIP_OCALL) {
		bc = ap->data1;
		ap->Flags &= ~AP_FLG_VOIP_ALERTING;
		sprintf(bc->display,"connect to %s", bc->usednr->name);
		ap->mgr->app_bc(ap->mgr, PR_APP_CONNECT, bc);
	} else if (ap->mode == AP_MODE_VOIP_ICALL) {
		ret = connect_voip(ap, bc);
		if (!ret) {
			ap->Flags |= AP_FLG_VOIP_ACTIV;
			ap->mgr->app_bc(ap->mgr, PR_APP_CONNECT, bc);
		}
		return(ret);
	}
	return(0);
}

static int
hangup_call(iapplication_t *ap, bchannel_t *bc)
{
	if ((ap->mode == AP_MODE_VOIP_OCALL) ||
		(ap->mode == AP_MODE_VOIP_ICALL)) {
		if (ap->Flags & AP_FLG_VOIP_ACTIV) {
			close_voip(ap, bc);
		}
		return(disconnect_voip(ap, bc));
	}
	return(0);
}

static int
clear_call(iapplication_t *ap, bchannel_t *bc)
{
	bchannel_t	*peer = NULL;

	if (ap->mode == AP_MODE_INTERN_CALL) {
		if (ap->data1 == bc) {
			peer = ap->data2;
			ap->data1 = NULL;
		} else if (ap->data2 == bc) {
			peer = ap->data1;
			ap->data2= NULL;
		}
		bc->rbuf = NULL;
		if (bc->sbuf)
			bc->sbuf->rsem = &bc->work;
		if (peer) {
			peer->Flags |= FLG_BC_PROGRESS;
			peer->cause_loc = bc->cause_loc;
			peer->cause_val = bc->cause_val;
			peer->rbuf = NULL;
			if (peer->sbuf)
				peer->sbuf->rsem = &peer->work;
			ap->mgr->app_bc(ap->mgr, PR_APP_HANGUP, peer);
		} else {
			free_application(ap);
		}
		if (bc)
			bc->app = NULL;
	} else if (ap->mode == AP_MODE_AUDIO_CALL) {
		if (ap->Flags & AP_FLG_AUDIO_ACTIV) {
			close_audio(ap, bc);
			ap->Flags &= ~AP_FLG_AUDIO_ACTIV;
			ap->vapp->flags &= ~AP_FLG_AUDIO_USED;
		}
		if (bc)
			bc->app = NULL;
		free_application(ap);
	} else if (ap->mode == AP_MODE_VOIP_OCALL) {
		if (ap->Flags & AP_FLG_VOIP_ACTIV) {
			close_voip(ap, bc);
		}
		release_voip(ap, bc);
		ap->mode = AP_MODE_IDLE;
		free_application(ap);
	} else if (ap->mode == AP_MODE_VOIP_ICALL) {
		if (ap->Flags & AP_FLG_VOIP_ACTIV) {
			close_voip(ap, bc);
		}
		release_voip(ap, bc);
		ap->mode = AP_MODE_IDLE;
		free_application(ap);
	}
	return(0);
}

static int
alert_call(iapplication_t *ap, bchannel_t *bc)
{
	bchannel_t	*peer = NULL;

	if (ap->mode == AP_MODE_VOIP_ICALL) {
		return(alert_voip(ap, bc));
	} else if (ap->mode == AP_MODE_INTERN_CALL) {
		if (bc != ap->data2)
			return(0);
		peer = ap->data1;
		if (!peer)
			return(0);
		peer->Flags |= FLG_BC_PROGRESS;
		ap->mgr->app_bc(ap->mgr, PR_APP_ALERT, peer);
	}
	return(0);
}

static int
facility_info(iapplication_t *ap, bchannel_t *bc)
{
	if ((ap->mode == AP_MODE_VOIP_ICALL) ||
		(ap->mode == AP_MODE_VOIP_OCALL)) {
		return(facility_voip(ap, bc));
	}
	return(0);
}

static int
useruser_info(iapplication_t *ap, bchannel_t *bc)
{
	if ((ap->mode == AP_MODE_VOIP_ICALL) ||
		(ap->mode == AP_MODE_VOIP_OCALL)) {
		return(useruser_voip(ap, bc));
	}
	return(0);
}

static int
open_recfiles(iapplication_t *ap, bchannel_t *bc)
{
	char		filename[2048];
	struct timeval	tv;
	int		ret;

	if (!bc)
		return(-EINVAL);
	if (!RecordFilePath[0]) {
		dprint(DBGM_TOPLEVEL, "%s: RecordFilePath not set\n", __FUNCTION__);
		return(-EINVAL);
	}
	gettimeofday(&tv, NULL);
	sprintf(filename, "%s%08lx_%02d.r",
		RecordFilePath, tv.tv_sec, bc->channel);
	dprint(DBGM_TOPLEVEL, "%s: rf.r:%s\n", __FUNCTION__,
		filename);
	if (bc->rrid > 0)
		close(bc->rrid);
	bc->rrid = open(filename, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU);
	if (bc->rrid < 0) {
		ret = -errno;
		dprint(DBGM_TOPLEVEL, "%s: rf.r error %s\n", __FUNCTION__,
			strerror(errno));

		return(ret);
	}
	filename[strlen(filename)-1] = 's';
	dprint(DBGM_TOPLEVEL, "%s: rf.s:%s\n", __FUNCTION__,
		filename);
	if (bc->rsid > 0)
		close(bc->rsid);
	bc->rsid = open(filename, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU);
	if (bc->rsid < 0) {
		ret = -errno;
		dprint(DBGM_TOPLEVEL, "%s: rf.s error %s\n", __FUNCTION__,
			strerror(errno));
		close(bc->rrid);
		bc->rrid = -1;
		return(ret);
	}
	bc->Flags |= FLG_BC_RECORDING;
	return(0);
}

static int
close_recfiles(iapplication_t *ap, bchannel_t *bc)
{
	if (!bc)
		return(-EINVAL);
	if (bc->rrid > 0)
		close(bc->rrid);
	bc->rrid = -1;
	if (bc->rsid > 0)
		close(bc->rsid);
	bc->rsid = -1;
	bc->Flags &= ~FLG_BC_RECORDING;
	return(0);
}

static int
application_handler(manager_t *mgr, int prim, void *arg)
{
	bchannel_t	*bc = arg;
	iapplication_t	*appl = NULL;

	if (!bc)
		return(-EINVAL);
	if (prim == PR_APP_ICALL) {
		appl = new_application(&voip);
		if (!appl)
			return(-EBUSY);
		appl->mgr = mgr;
		bc->app = appl;
		return(route_call(appl, bc));
			return(-EBUSY);
	}
	appl = bc->app;
	if (!appl)
		return(-EINVAL);
	if (prim == PR_APP_CONNECT) {
		return(connect_call(appl, bc));
	} else if (prim == PR_APP_ALERT) {
		return(alert_call(appl, bc));
	} else if (prim == PR_APP_FACILITY) {
		return(facility_info(appl, bc));
	} else if (prim == PR_APP_USERUSER) {
		return(useruser_info(appl, bc));
	} else if (prim == PR_APP_HANGUP) {
		return(hangup_call(appl, bc));
	} else if (prim == PR_APP_CLEAR) {
		return(clear_call(appl, bc));
	} else if (prim == PR_APP_OPEN_RECFILES) {
		return(open_recfiles(appl, bc));
	} else if (prim == PR_APP_CLOSE_RECFILES) {
		return(close_recfiles(appl, bc));
	}
	return(-EINVAL);
}

int main(argc,argv)
int argc;
char *argv[];

{
	int		ret, *retp;
	char		host_cfg[MAX_HOST_SIZE+16];
	pthread_t	voip_id;
	nr_list_t	*nr;
	
	debug_init(global_debug, "testlog", NULL, NULL);
	memset(&voip, 0, sizeof(vapplication_t));
	voip.tout.tv_sec = NORMAL_TIMEOUT_s;
	voip.tout.tv_usec = NORMAL_TIMEOUT_us;
	msg_init();
	ret = init_manager(&voip.mgr_lst, application_handler);
	if (ret) {
		fprintf(stderr, "error in init_manager %d\n", ret);
		exit(1);
	}
	parse_cfg("voip.cfg", voip.mgr_lst);
	if (gethostname(voip.hostname, MAX_HOST_SIZE)) {
		fprintf(stderr, "error getting hostname: %s\n",
			strerror(errno));
		exit(1);
	}
	sprintf(host_cfg, "%s.voip.cfg", voip.hostname);
	parse_cfg(host_cfg, voip.mgr_lst);
	debug_init(global_debug, NULL, NULL, NULL);
	voip.port = rtp_port; 
	voip.flags = default_flags;
	voip.debug = global_debug;
	fprintf(stderr, "%s: debug(%x) port(%d)\n", __FUNCTION__,
		global_debug, rtp_port);
	nr = voip.mgr_lst->nrlist;
	while(nr) {
		dprint(DBGM_TOPLEVEL, "nr(%s) len(%d) flg(%x) typ(%d) name(%s)\n",
			nr->nr, nr->len, nr->flags, nr->typ, nr->name);
		nr = nr->next;
	}
	signal(SIGTERM, term_handler);
	signal(SIGINT, term_handler);
	signal(SIGPIPE, term_handler);
	signal(SIGUSR1, sig_usr1_handler);
	signal(SIGALRM, SIG_IGN);
	read_rec_ctrlfile();
#if 0
	signal(SIGCHLD, child_handler);
#endif
#if 0
	{
		static struct sigaction sa;
		
		sa.sa_handler = NULL;
		sa.sa_restorer = NULL;
		sa.sa_sigaction = sig_segfault;
		sa.sa_flags = SA_ONESHOT | SA_SIGINFO;
		sigemptyset(&sa.sa_mask);
		ret = sigaction(SIGSEGV, &sa, NULL);
		fprintf(stderr, "sigaction ret(%d)\n",
			ret);
	}
#endif
	voip_id = run_voip(&voip);
	retp = do_netthread(voip.mgr_lst->nst);
	fprintf(stderr, "do_main_loop returns(%p)\n", retp);
	while (voip.mgr_lst) {
		manager_t	*next = voip.mgr_lst->next;
		cleanup_manager(voip.mgr_lst);
		voip.mgr_lst = next;
	}
	voip.flags |= AP_FLG_VOIP_ABORT;
	ret = pthread_join(voip_id, (void *)&retp);
	fprintf(stderr, "%s: join voipscan ret(%d) rval(%p)\n", __FUNCTION__,
		ret, retp);
	debug_close();
	return(0);
}
