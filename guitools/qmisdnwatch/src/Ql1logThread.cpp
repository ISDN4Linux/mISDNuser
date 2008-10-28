/* $Id: Ql1logThread.cpp 4 2008-10-28 00:04:24Z daxtar $
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
#include <sys/ioctl.h>
#include "misdn.h"
#include "Ql1logThread.h"

void Ql1logThread::run(void)
{
	struct sockaddr_mISDN addr;
	int log_socket;
	dch_echo = misdn.openl1Log(devId, protocol, &log_socket, &addr);

	qDebug("Ql1logThread::run log_socket(%i) devId(%d) proto(%d)",
	       log_socket, devId, protocol);

	if (log_socket >= 0)
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
			ret = recvmsg(log_socket, &mh, 0);

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
		qDebug("ERROR connecting log_socket");
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
	connect(this, SIGNAL(finished()), this, SLOT(finish()));
}

void Ql1logThread::finish() {
	close(log_socket);
	emit finished(devId);
}
