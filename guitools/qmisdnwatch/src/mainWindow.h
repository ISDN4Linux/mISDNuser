/* $Id: mainWindow.h 7 2008-10-28 22:06:36Z daxtar $
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

#ifndef mainWindow_H
#define mainWindow_H

#include <QWidget>
#include <QMainWindow>
#include <QTextEdit>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QThread>
#include <mISDNuser/mISDNif.h>
#include "Ql1logThread.h"
#include "extraWidgets.h"
#include "misdn.h"


struct isdnDeviceStuff {
	unsigned int id;
	QWidget * tab; /* tab in TabWidget */
	int tabId; /* tabID in TabWidget */
	QLabel * mainLabel;
	QTreeWidgetItem * treeHead;
	QTextEdit * log;
	idButton * buttonRename;
	idButton * buttonCleanL2;
	idButton * buttonLogL1;
	idButton * buttonSaveLog;
	Ql1logThread * l1log;
	QByteArray eyeSDN;
};

class mainWindow : public QMainWindow {
	Q_OBJECT
	public:
		mainWindow( QMainWindow *parent = 0,
			Qt::WindowFlags flags = 0 );

	private:
		QList <struct mISDN_devinfo> deviceList;
		QList <struct isdnDeviceStuff> devStack;

		QTabWidget * tabsheet;
		QWidget * maintab;
		QTreeWidget * devtree;
		QTimer * deviceListTimer;
		QTextEdit * logText;
		mISDN misdn;

		void renewDeviceWidgets(void);
		void removeVanishedDevices(void);
		void createNewDeviceTab(struct mISDN_devinfo *devinfo);
		void debugOut(QTextEdit * textEdit, QString text);
		void binaryOut(QTextEdit * textEdit, QByteArray & data, int offset);
		struct isdnDeviceStuff * getStuffbyId(unsigned int id);
		struct mISDN_devinfo * getDevInfoById(unsigned int id);
		void eyeSDNappend(QByteArray & target, QByteArray & data, int offset);

	private slots:
		void updateDeviceList(void);
		int renameDevice(unsigned int id);
		int cleanL2(unsigned int id);
		int switchL1Logging(unsigned int id);
		int saveL1Log(unsigned int id);
		void logRcvData(unsigned int id, QByteArray data, struct timeval tv);
		int logFinished(unsigned int id);
		void about();
		void aboutQt();
};

#endif
