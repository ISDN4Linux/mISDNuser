/* $Id: Ql1logThread.cpp 10 2008-10-31 12:58:01Z daxtar $
 * (c) 2008 Martin Bachem, m.bachem@gmx.de
 *
 * This file is part of qmisdnwatch
 *
 * Project's Home
 *     http://www.misdn.org/index.php/Qmisdnwatch
 *
 * qmisdnwatch is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2
 *
 * qmisdnwatch is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with qmisdnwatch.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <QByteArray>
#include <QInputDialog>
#include <sys/ioctl.h>
#include "misdn.h"
#include "Ql1logThread.h"

void Ql1logThread::run(void)
{
	qDebug("Ql1logThread::run socket(%i) devId(%d) proto(%d)",
		socket, devId, protocol);
	if (isConnected())
	{
		struct msghdr mh;
		struct iovec iov[1];
		struct ctstamp cts;
		int ret;
		int buflen = 512;
		unsigned char buffer[buflen];

		while (1) {
			mh.msg_name = NULL;
			mh.msg_namelen = 0;
			mh.msg_iov = iov;
			mh.msg_iovlen = 1;
			mh.msg_control = &cts;
			mh.msg_controllen = sizeof(cts);
			mh.msg_flags = 0;
			iov[0].iov_base = buffer;
			iov[0].iov_len = buflen;
			ret = recvmsg(socket, &mh, 0);

			if (ret >= 0) {
				if (cts.cmsg_type != MISDN_TIME_STAMP) {
					cts.tv.tv_sec = 0;
					cts.tv.tv_usec = 0;
				}
				QByteArray * data = new QByteArray((const char *)buffer, ret);
				emit rcvData(devId, *data, cts.tv);
			}
		}
	} else 
		qDebug("ERROR socket not connected...");
}

bool Ql1logThread::connectLayer1(struct mISDN_devinfo *devInfo) {
	if (isConnected())
		return true;

	protocol = devInfo->protocol;
	if (!protocol) {
		QStringList items;
		bool Ok;
		if (devInfo->Dprotocols & (1 << ISDN_P_TE_S0))
			items << "TE mode S0 layer1";
		if (devInfo->Dprotocols & (1 << ISDN_P_NT_S0))
			items << "NT mode S0 layer1";
		if (devInfo->Dprotocols & (1 << ISDN_P_TE_E1))
			items << "TE mode E1 layer1";
		if (devInfo->Dprotocols & (1 << ISDN_P_NT_E1))
			items << "NT mode E1 layer1";
		if (devInfo->Dprotocols & (1 << ISDN_P_TE_UP0))
			items << "TE mode UP0 layer1";
		if (devInfo->Dprotocols & (1 << ISDN_P_NT_UP0))
			items << "NT mode UP0 layer1";
		QString value = QInputDialog::getItem(NULL,
				QString(devInfo->name),
				tr("port still unused, please select layer1 mode:"),
				items, 0, false, &Ok);

		if (Ok && !value.isEmpty()) {
			if (value == "TE mode S0 layer1")
				setProtocol(ISDN_P_TE_S0);
			if (value == "NT mode S0 layer1")
				setProtocol(ISDN_P_NT_S0);
			if (value == "TE mode E1 layer1")
				setProtocol(ISDN_P_TE_E1);
			if (value == "NT mode E1 layer1")
				setProtocol(ISDN_P_NT_E1);
			if (value == "TE mode UP0 layer1")
				setProtocol(ISDN_P_TE_UP0);
			if (value == "NT mode UP0 layer1")
				setProtocol(ISDN_P_NT_UP0);
		}
	}

	if (protocol) {
		devInfo->protocol = protocol;
		dch_echo = misdn.openl1Log(devId, protocol, &socket, &addr);
		qDebug("Ql1logThread::connectLayer1 socket(%i) devId(%d) proto(%d)",
		       socket, devId, protocol);
		if (socket >= 0)
			start();
	}

	if (isConnected()) {
		emit l1Connected(devId);
		return true;
	}
	return false;
}

bool Ql1logThread::sendActivateReq(void) {
	if (isConnected()) {
		int ret;
		unsigned char buffer[MISDN_HEADER_LEN];
		struct mISDNhead * hh = (struct mISDNhead *)buffer;
		hh->prim = PH_ACTIVATE_REQ;
		hh->id = MISDN_ID_ANY;
		ret = sendto(socket, buffer, MISDN_HEADER_LEN, 0, NULL, 0);
		return (ret >= 0);
	}
	return false;
}

bool Ql1logThread::sendInformationReq(void) {
	if (isConnected()) {
		int ret;
		unsigned char buffer[MISDN_HEADER_LEN];
		struct mISDNhead * hh = (struct mISDNhead *)buffer;
		hh->prim = MPH_INFORMATION_REQ;
		hh->id = MISDN_ID_ANY;
		ret = sendto(socket, buffer, MISDN_HEADER_LEN, 0, NULL, 0);
		return (ret >= 0);
	}
	return false;
}

bool Ql1logThread::isConnected(void) {
	return ((socket >= 0) && isRunning());
}

void Ql1logThread::disconnectLayer1(void) {
	close(socket);
	terminate();
	wait(1000);
	socket = -1;
	emit l1Disonnected(devId);
	qDebug("dev %i layer1 disconnected", devId);
}

void Ql1logThread::setProtocol(int p) {
	protocol = p;
	qDebug("Ql1logThread protocol %d (%d)", protocol, p);
}

int Ql1logThread::getProtocol(void) {
	return protocol;
}

int Ql1logThread::hasEcho(void) {
	return (dch_echo);
}

Ql1logThread::Ql1logThread(int id, mISDN & m) : devId(id), misdn(m) {
	dch_echo = -1;
	socket = -1;
	protocol = 0;
	connect(this, SIGNAL(finished()), this, SLOT(finish()));
}

Ql1logThread::~Ql1logThread() {
	disconnectLayer1();
}

void Ql1logThread::finish() {
	emit finished(devId);
}
