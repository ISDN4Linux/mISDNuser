/* $Id: Ql1logThread.h 10 2008-10-31 12:58:01Z daxtar $
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
		int socket;
		struct sockaddr_mISDN addr;
		mISDN & misdn;
		int protocol;
		int dch_echo;

	public:
		Ql1logThread(int id, mISDN & m);
		~Ql1logThread();
		bool connectLayer1(struct mISDN_devinfo *devInfo);
		void disconnectLayer1(void);
		bool isConnected(void);
		bool sendActivateReq(void);
		bool sendInformationReq(void);
		void setProtocol(int p);
		int getProtocol(void);
		int hasEcho(void);

	protected:
		void run();

	public slots:
		void finish();

	signals:
		void finished(unsigned int id);
		void l1Connected(unsigned int id);
		void l1Disonnected(unsigned int id);
		void rcvData(unsigned int id, QByteArray data, struct timeval);
		
};


#endif //  _QL1LOGTHREAD_H_
