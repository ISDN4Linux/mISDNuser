/* $Id: Ql1logThread.h 4 2008-10-28 00:04:24Z daxtar $
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

#ifndef _QL1LOGTHREAD_H_
#define _QL1LOGTHREAD_H_

#include <QObject>
#include <QThread>
#include <QByteArray>
#include "misdn.h"

class Ql1logThread : public QThread
{
	Q_OBJECT
	private:
		int devId;
		mISDN & misdn;
		int protocol;
		int log_socket;
		int dch_echo;
		struct sockaddr_mISDN log_addr;

	public:
		Ql1logThread(int id, mISDN & m);
		void setProtocol(int p);
		int getProtocol(void);
		int hasEcho(void);

	protected:
		void run();

	public slots:
		void finish();

	signals:
		void finished(unsigned int id);
		void rcvData(unsigned int id, QByteArray data, struct timeval);
};


#endif //  _QL1LOGTHREAD_H_
