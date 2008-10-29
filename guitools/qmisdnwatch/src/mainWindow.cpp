/* $Id: mainWindow.cpp 7 2008-10-28 22:06:36Z daxtar $
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

#include <QApplication>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTimer>
#include <QLabel>
#include <QInputDialog>
#include <QByteArray>
#include <QFile>
#include <QFileDialog>
#include <QDataStream>
#include <QMessageBox>
#include <QMenuBar>
#include <QMenu>
#include <mISDNuser/mISDNif.h>
#include "mainWindow.h"


struct isdnDeviceStuff * mainWindow::getStuffbyId(unsigned int id) {
	int i;
	for (i=0; i<devStack.count(); i++)
		if (devStack[i].id == id)
			return &devStack[i];
	return NULL;
}

struct mISDN_devinfo * mainWindow::getDevInfoById(unsigned int id) {
	int i;
	for (i=0; i<deviceList.count(); i++)
		if (deviceList[i].id == id)
			return &deviceList[i];
	return NULL;
}

int mainWindow::renameDevice(unsigned int id) {
	struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
	if (!thisDevStuff)
		return -1;
	struct mISDN_devinfo * devInfo = getDevInfoById(id);
	if (!devInfo)
		return -1;

	bool inputOk;
	QString newName = QInputDialog::getText(this, tr("device Name"), "new device name:",
			QLineEdit::Normal, QString(devInfo->name), &inputOk);
	if (inputOk && !newName.isEmpty()) {
		if (strcmp(newName.toAscii().data(), devInfo->name) != 0) {
			deviceListTimer->stop();
			if (!misdn.renameLayer1(id, newName.toAscii().data())) {
				strncpy(devInfo->name, newName.toAscii().data(), MISDN_MAX_IDLEN);
				tabsheet->setTabText(thisDevStuff->tabId, newName);
				thisDevStuff->treeHead->setText(0, newName);
				debugOut(thisDevStuff->log, tr("renamed device to '%1'\n")
						.arg(newName));
			} else {
				debugOut(thisDevStuff->log, tr("ERROR: renaming to '%1' failed\n")
						.arg(newName));
			}
			deviceListTimer->start(1000);
		}
	}
	return 0;
}

int mainWindow::cleanL2(unsigned int id) {
	struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
	if (!thisDevStuff)
		return -1;

	int ret = misdn.cleanl2(id);
	if (ret >= 0)
		debugOut(thisDevStuff->log, tr("cleansed Layer2\n"));
	else {
		debugOut(thisDevStuff->log, tr("ERROR: cleaning L2 failed: "));
		if (ret == -4)
			debugOut(thisDevStuff->log, tr("no L2 protocol set\n"));
		else if (ret == -1)
			debugOut(thisDevStuff->log, tr("ioctrl invalid argument\n"));
		else
			debugOut(thisDevStuff->log, QString("%1\n").arg(ret));
	}
	return 0;
}

int mainWindow::logFinished(unsigned int id) {
	struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
	if (!thisDevStuff)
		return -1;
	debugOut(thisDevStuff->log, tr("logging stopped\n"));
	if (thisDevStuff->l1log->hasEcho())
		thisDevStuff->buttonLogL1->setText(tr("start D-/E-Channel logging"));
	else
		thisDevStuff->buttonLogL1->setText(tr("start D-Channel logging"));
	return 0;
}

void mainWindow::eyeSDNappend(QByteArray & target, QByteArray & data, int offset) {
	for (int i=offset; i<data.count(); i++) {
		unsigned char byte = data[i];
		if ((byte == 0xFF) || (byte == 0xFE)) {
			target.append(0xFE);
			byte -= 2;
		}
		target.append(byte);
	}
}

void mainWindow::logRcvData(unsigned int id, QByteArray data, struct timeval tv) {
	struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
	struct mISDN_devinfo * devInfo = getDevInfoById(id);
	struct tm *mt;
	unsigned char origin;
	int len;

	if ((!thisDevStuff) || (!devInfo))
		return;

	unsigned char buffer[data.count()];
	for (int i=0; i<data.count(); i++)
		buffer[i] = data[i];
	struct mISDNhead * hh = (struct mISDNhead *)buffer;

	QString timeStr("");
	mt = localtime((time_t *)&tv.tv_sec);
	timeStr.sprintf("%02d.%02d.%04d %02d:%02d:%02d.%06ld:\t", mt->tm_mday, mt->tm_mon + 1, mt->tm_year + 1900,
		mt->tm_hour, mt->tm_min, mt->tm_sec, tv.tv_usec);
	debugOut(thisDevStuff->log, timeStr);

	debugOut(thisDevStuff->log, QString("prim(0x%1)\tid(%2)")
			.arg(QString::number(hh->prim, 16))
			.arg(QString::number(hh->id, 16)));

	binaryOut(thisDevStuff->log, data, sizeof(struct mISDNhead));

	// collect data as eyeSDN stream to SaveAs wireshark readable
	if (thisDevStuff->l1log->hasEcho() && (hh->prim == PH_DATA_REQ))
		return;
	if ((hh->prim != PH_DATA_REQ) && (hh->prim != PH_DATA_IND) &&
		    (hh->prim != PH_DATA_E_IND))
		return;

	if (devInfo->protocol == ISDN_P_NT_S0 || devInfo->protocol == ISDN_P_NT_E1)
		origin = hh->prim == PH_DATA_REQ ? 0 : 1;
	else
		origin = ((hh->prim == PH_DATA_REQ) ||
				(hh->prim == PH_DATA_E_IND)) ? 1 : 0;

	len = data.count() - MISDN_HEADER_LEN;
	QByteArray eyeHeader;
	eyeHeader.append((unsigned char)(0xff & (tv.tv_usec >> 16)));
	eyeHeader.append((unsigned char)(0xff & (tv.tv_usec >> 8)));
	eyeHeader.append((unsigned char)(0xff & tv.tv_usec));
	eyeHeader.append((char)0);
	eyeHeader.append((unsigned char)(0xff & (tv.tv_sec >> 24)));
	eyeHeader.append((unsigned char)(0xff & (tv.tv_sec >> 16)));
	eyeHeader.append((unsigned char)(0xff & (tv.tv_sec >> 8)));
	eyeHeader.append((unsigned char)(0xff & tv.tv_sec));
	eyeHeader.append((char)0);
	eyeHeader.append((unsigned char) origin);
	eyeHeader.append((unsigned char)(0xff & (len >> 8)));
	eyeHeader.append((unsigned char)(0xff & len));

	/* add Frame escaped */
	thisDevStuff->eyeSDN.append(0xFF);
	eyeSDNappend(thisDevStuff->eyeSDN, eyeHeader, 0);
	eyeSDNappend(thisDevStuff->eyeSDN, data, sizeof(struct mISDNhead));
}

int mainWindow::switchL1Logging(unsigned int id) {
	struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
	struct mISDN_devinfo * devInfo = getDevInfoById(id);
	if ((!thisDevStuff) || (!devInfo))
		return -1;

	if (thisDevStuff->l1log->isRunning()) {
		thisDevStuff->l1log->terminate();
		thisDevStuff->l1log->wait(1000);
		if (thisDevStuff->l1log->isRunning()) {
			debugOut(thisDevStuff->log, tr("ERROR: stop logging failed\n"));
		} else {
			thisDevStuff->buttonLogL1->setText(tr("start D-/E-Channel logging"));
		}
	} else {
		bool protoDefined=false;
		if (!devInfo->protocol) {
			QStringList items;
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
			QString value = QInputDialog::getItem(this,
					tr("select portmode"),
					tr("port still unused, please select layer1 mode:"),
					items, 0, false, &protoDefined);

			if (protoDefined && !value.isEmpty()) {
				if (value == "TE mode S0 layer1")
					thisDevStuff->l1log->setProtocol(ISDN_P_TE_S0);
				if (value == "NT mode S0 layer1")
					thisDevStuff->l1log->setProtocol(ISDN_P_NT_S0);
				if (value == "TE mode E1 layer1")
					thisDevStuff->l1log->setProtocol(ISDN_P_TE_E1);
				if (value == "NT mode E1 layer1")
					thisDevStuff->l1log->setProtocol(ISDN_P_NT_E1);
				if (value == "TE mode UP0 layer1")
					thisDevStuff->l1log->setProtocol(ISDN_P_TE_UP0);
				if (value == "NT mode UP0 layer1")
					thisDevStuff->l1log->setProtocol(ISDN_P_NT_UP0);
			}
		} else {
			thisDevStuff->l1log->setProtocol(devInfo->protocol);
			protoDefined = true;
		}

		if (protoDefined) {
			thisDevStuff->eyeSDN.clear();
			thisDevStuff->l1log->start();
			if (thisDevStuff->l1log->isRunning()) {
				QByteArray eyeHeader("EyeSDN");
				thisDevStuff->eyeSDN.append(eyeHeader);
				if (thisDevStuff->l1log->hasEcho()) {
					debugOut(thisDevStuff->log,
						tr("D/E channel logging started\n"));
					thisDevStuff->buttonLogL1->setText(
						tr("stop D-/E-Channel logging"));
				}
				else {
					debugOut(thisDevStuff->log,
						tr("D channel logging started\n"));
					thisDevStuff->buttonLogL1->setText(
						tr("stop D-Channel logging"));

				}
			} else {
				debugOut(thisDevStuff->log,
					 tr("ERROR: start logging failed\n"));
			}
		}
	}

	return 0;
}

int mainWindow::saveL1Log(unsigned int id) {
	struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
	struct mISDN_devinfo * devInfo = getDevInfoById(id);
	if ((!thisDevStuff) || (!devInfo))
		return -1;

	QString filename;
	filename = QFileDialog::getSaveFileName(
		this, tr("Save Log As WireShark readable"));

	//	QString("%1").arg(QDir::currentPath()));

	if (filename.isEmpty())
		return -1;

	/* save complete file */
	QFile file(filename);
	file.open(QIODevice::WriteOnly);
	QDataStream out(&file);
	for (int i = 0; i < thisDevStuff->eyeSDN.count(); ++i) {
		unsigned char byte = thisDevStuff->eyeSDN[i];
		out.writeRawData((const char*)&byte, 1);
	}
	file.close();

	return 0;
}

void mainWindow::createNewDeviceTab(struct mISDN_devinfo *devinfo) {
	struct isdnDeviceStuff newDevStuff;
	qDebug("device %d arised (%s)", devinfo->id, devinfo->name);

	newDevStuff.id = devinfo->id;
	newDevStuff.tab = new QWidget;
	newDevStuff.tabId = tabsheet->addTab(newDevStuff.tab,
					     QString(devinfo->name));

	/* Button 'rename' */
	newDevStuff.buttonRename = new idButton(tr("rename"),
			newDevStuff.id, newDevStuff.tab);
	connect(newDevStuff.buttonRename, SIGNAL(clicked(unsigned int)),
		this, SLOT(renameDevice(unsigned int)));

	/* Button 'clean Layer2' */
	newDevStuff.buttonCleanL2 = new idButton(tr("clean Layer2"),
			newDevStuff.id, newDevStuff.tab);
	connect(newDevStuff.buttonCleanL2, SIGNAL(clicked(unsigned int)),
		this, SLOT(cleanL2(unsigned int)));

	/* Button 'L1 logging' */
	newDevStuff.buttonLogL1 = new idButton(tr("start D-/E-Channel logging"),
		newDevStuff.id, newDevStuff.tab);
	connect(newDevStuff.buttonLogL1, SIGNAL(clicked(unsigned int)),
		this, SLOT(switchL1Logging(unsigned int)));

	/* Button 'saveLog' */
	newDevStuff.buttonSaveLog = new idButton(tr("Save Log As"),
		newDevStuff.id, newDevStuff.tab);
	connect(newDevStuff.buttonSaveLog, SIGNAL(clicked(unsigned int)),
		this, SLOT(saveL1Log(unsigned int)));

	/* mainLabel */
	newDevStuff.mainLabel = new QLabel(QString(tr("Device Controls")));

	/* Text Widget */
	newDevStuff.log = new QTextEdit();

	/* l1log Thread */
	newDevStuff.l1log = new Ql1logThread(devinfo->id, misdn);
	connect(newDevStuff.l1log, SIGNAL(finished(unsigned int)),
		this, SLOT(logFinished(unsigned int)));
	connect(newDevStuff.l1log, SIGNAL(rcvData(unsigned int, QByteArray, struct timeval)),
		this, SLOT(logRcvData(unsigned int, QByteArray, struct timeval)));

	/* Layout */
	QVBoxLayout *vbox = new QVBoxLayout(newDevStuff.tab);
	vbox->addWidget(newDevStuff.mainLabel);
	QHBoxLayout *hbox = new QHBoxLayout();
	hbox->addWidget(newDevStuff.buttonRename);
	hbox->addSpacing(5);
	hbox->addWidget(newDevStuff.buttonCleanL2);
	hbox->addSpacing(5);
	hbox->addWidget(newDevStuff.buttonLogL1);
	hbox->addWidget(newDevStuff.buttonSaveLog);
	hbox->insertStretch(5, 0);
	vbox->addItem(hbox);
	vbox->addWidget(newDevStuff.log);

	devStack << newDevStuff;
}

void mainWindow::removeVanishedDevices(void) {
	int i, j;
	int vanishedDev=1;
	while (vanishedDev && devStack.count()) {
		for (i=(devStack.count()-1); i>=0; i--) {
			vanishedDev = 1;
			for (j=0; j<deviceList.count(); j++)
				if (devStack[i].id == deviceList[j].id)
					vanishedDev = 0;
			if (vanishedDev) {
				qDebug("device %d vansihed", devStack[i].id);
				tabsheet->removeTab(devStack[i].tabId);
				for(j=i+1; j<devStack.count(); j++)
					devStack[j].tabId--;
				devStack.removeAt(i);
				break;
			}
		}
	}
}

void mainWindow::renewDeviceWidgets(void) {
	int i, j, n = deviceList.count();
	struct mISDN_devinfo devinfo;
	int newdev;

	/* clear old widgets */
	devtree->clear();
	removeVanishedDevices();

	QTreeWidgetItem* devitem;
	for (i=0; i<n; i++) {
		devinfo = deviceList[i];

		newdev = 1;
		for (j=0; j<devStack.count(); j++)
			if (devinfo.id == devStack[j].id)
				newdev = 0;

		/* create new Device Tab and stuff */
		if (newdev)
			 createNewDeviceTab(&devinfo);

		struct isdnDeviceStuff * thisDevStuff = getStuffbyId(devinfo.id);
		if (!thisDevStuff)
			return;

		/* Update main Device Tree */
		devitem = new QTreeWidgetItem(devtree);
		devitem->setText(0, QString(devinfo.name));
		thisDevStuff->treeHead = devitem;

		QTreeWidgetItem* optitem;

		optitem = new QTreeWidgetItem(devitem);
		optitem->setText(0, tr("Id: %1")
				.arg(devinfo.id));

		optitem = new QTreeWidgetItem(devitem);
		optitem->setText(0, tr("D protocols: %1")
				.arg(devinfo.Dprotocols));
		if (devinfo.Dprotocols) {
			QTreeWidgetItem* l1item = new QTreeWidgetItem(optitem);
			l1item->setText(0, tr("Layer 1"));

			QTreeWidgetItem* doptitem;
			if (devinfo.Dprotocols & (1 << ISDN_P_TE_S0)) {
				doptitem = new QTreeWidgetItem(l1item);
				doptitem->setText(0, tr("S0 TE"));
			}
			if (devinfo.Dprotocols & (1 << ISDN_P_NT_S0)) {
				doptitem = new QTreeWidgetItem(l1item);
				doptitem->setText(0, tr("S0 NT"));
			}
			if (devinfo.Dprotocols & (1 << ISDN_P_TE_E1)) {
				doptitem = new QTreeWidgetItem(l1item);
				doptitem->setText(0, tr("E1 TE"));
			}
			if (devinfo.Dprotocols & (1 << ISDN_P_NT_E1)) {
				doptitem = new QTreeWidgetItem(l1item);
				doptitem->setText(0, tr("E1 NT"));
			}
			if (devinfo.Dprotocols & (1 << ISDN_P_TE_UP0)) {
				doptitem = new QTreeWidgetItem(l1item);
				doptitem->setText(0, tr("UP0 TE"));
			}
			if (devinfo.Dprotocols & (1 << ISDN_P_NT_UP0)) {
				doptitem = new QTreeWidgetItem(l1item);
				doptitem->setText(0, tr("UP0 NT"));
			}
		}

		optitem = new QTreeWidgetItem(devitem);
		optitem->setText(0, tr("B protocols: %1")
				.arg(devinfo.Bprotocols));
		if (devinfo.Bprotocols) {
			QTreeWidgetItem* l1item = new QTreeWidgetItem(optitem);
			l1item->setText(0, QString("Layer 1"));
			QTreeWidgetItem* boptitem;
			if (devinfo.Bprotocols & (1 <<
						 (ISDN_P_B_RAW
						 - ISDN_P_B_START))) {
				boptitem = new QTreeWidgetItem(l1item);
				boptitem->setText(0,
						tr("RAW (transparent)"));
			}
			if (devinfo.Bprotocols & (1 <<
						 (ISDN_P_B_HDLC
						 - ISDN_P_B_START))) {
				boptitem = new QTreeWidgetItem(l1item);
				boptitem->setText(0, tr("HDLC"));
			}
			if (devinfo.Bprotocols & (1 <<
						 (ISDN_P_B_X75SLP
						 - ISDN_P_B_START))) {
				boptitem = new QTreeWidgetItem(l1item);
				boptitem->setText(0, tr("X.75"));
			}
			QTreeWidgetItem* l2item = new QTreeWidgetItem(optitem);
			l2item->setText(0, tr("Layer 2"));
			if (devinfo.Bprotocols & (1 <<
						 (ISDN_P_B_L2DTMF
						 - ISDN_P_B_START))) {
				boptitem = new QTreeWidgetItem(l2item);
				boptitem->setText(0, tr("DTMF"));
			}
		}
		optitem = new QTreeWidgetItem(devitem);
		optitem->setText(0, QString(tr("protocol: %1"))
				.arg(devinfo.protocol));
		QTreeWidgetItem* l1item = new QTreeWidgetItem(optitem);
		switch (devinfo.protocol) {
			case 0:
				l1item->setText(0, tr("unused"));
				break;
			case ISDN_P_TE_S0:
				l1item->setText(0, tr("S0 TE"));
				break;
			case ISDN_P_NT_S0:
				l1item->setText(0, tr("S0 NT"));
				break;
			case ISDN_P_TE_E1:
				l1item->setText(0, tr("E1 TE"));
				break;
			case ISDN_P_NT_E1:
				l1item->setText(0, tr("E1 NT"));
				break;
			case ISDN_P_TE_UP0:
				l1item->setText(0, tr("UP0 TE"));
				break;
			case ISDN_P_NT_UP0:
				l1item->setText(0, tr("UP0 NT"));
				break;
			default:
				l1item->setText(0, tr("unkown protocol"));
				break;
		}

		optitem = new QTreeWidgetItem(devitem);
		optitem->setText(0, tr("B-Channels: %1")
				.arg(devinfo.nrbchan));

	}
}

void mainWindow::showMISDNversion(void) {
	struct mISDNversion v;
	v = misdn.getVersion();
		debugOut(logText, tr("mISDN kernel driver v%1.%2.%4\n")
			.arg(v.major)
			.arg(v.minor)
			.arg(v.release));
}

void mainWindow::updateDeviceList(void) {
	int i, j;
	QList <struct mISDN_devinfo> tmpDevList;
	int newdev = 0, oldnumdevs = misdn.getLastNumDevs();

	if (!misdn.isCoreConnected()) {
		misdn.connectCore();
		if (!misdn.isCoreConnected())
			return;
		else
			showMISDNversion();
	}

	i = misdn.getNumDevices();
	if (i < 0) {
		debugOut(logText, tr("unable to connect mISDN socket\n"));
		devtree->clear();
		return;
	}

	if (i != oldnumdevs)
		debugOut(logText, tr("found %1 device%2\n")
				.arg(i)
				.arg((i==1)?"":"s"));

	if (i >= 0) {
		struct mISDN_devinfo devinfo;

		for (j=0; ((j<MAX_DEVICE_ID) && (tmpDevList.count() < i)); j++)
			if (misdn.getDeviceInfo(&devinfo, j) >= 0)
				tmpDevList << devinfo;

		if (tmpDevList.count() == deviceList.count()) {
			for (j=0; j<deviceList.count(); j++)
				newdev |= memcmp(&tmpDevList[j],
						&deviceList[j],
						sizeof(struct mISDN_devinfo)-8);
		} else
			newdev = 1;

		/* numDevices of devinfos had changed */
		if (newdev) {
			deviceList.clear();
			for (j=0; j<i; j++) {
				debugOut(logText, tr("id %1: '%2'\n")
						.arg(tmpDevList[j].id)
						.arg(QString(
							tmpDevList[j].name)));
				deviceList << tmpDevList[j];
			}
			renewDeviceWidgets();
		}
	}
}

void mainWindow::debugOut(QTextEdit * textEdit, QString text) {
	textEdit->moveCursor(QTextCursor::End);
	textEdit->insertPlainText(text);
	textEdit->ensureCursorVisible();
}

void mainWindow::binaryOut(QTextEdit * textEdit, QByteArray & data, int offset) {
	int i, j=0;
	QString lz;
	if (offset < data.size())
		debugOut(textEdit, ": ");
	for (i=offset; i<data.size(); i++) {
		lz = (((unsigned char)data[i]) < 0x10)?"0":"";
 		if (!(j++ % 8))
			debugOut(textEdit, "\n\t");
		debugOut(textEdit, QString("0x%1%2 ")
				.arg(lz)
				.arg(QString::number((unsigned char)data[i], 16))
			);
	}
	debugOut(textEdit, "\n\n");
}

void mainWindow::about() {
	QMessageBox::information(this,
		tr("About qmisdnwatch"),
		tr("<b>qmisdnwatch</b> <b>v0.0.2</b><br>m.bachem@gmx.de<br>http://www.misdn.org/index.php/Qmisdnwatch"),
		QMessageBox::Ok);
}

void mainWindow::aboutQt() {
	QMessageBox::aboutQt(this);
}

// Neue Widget-Klasse vom eigentlichen Widget ableiten
mainWindow::mainWindow( QMainWindow *parent,
                    Qt::WindowFlags flags ) :
	QMainWindow(parent, flags)
{
	resize(480, 400);

	QWidget * centralwidget = new QWidget(this);
	tabsheet = new QTabWidget(centralwidget);
	maintab = new QWidget;

	tabsheet->addTab(maintab, "mISDN");

	QVBoxLayout *vbox01 = new QVBoxLayout(centralwidget);
	QVBoxLayout *vbox02 = new QVBoxLayout(maintab);

	logText = new QTextEdit(maintab);
	logText->setMaximumHeight(120);

	devtree = new QTreeWidget(maintab);
	devtree->setHeaderLabel(tr("mISDN v2 devices"));

	deviceListTimer = new(QTimer);
	connect(deviceListTimer, SIGNAL(timeout()),
		this, SLOT(updateDeviceList()));

	vbox01->addWidget(tabsheet);
	vbox02->addWidget(devtree);
	vbox02->addWidget(logText);

	QMenu * optionsMenu = new QMenu(tr("Options"), this);
	// QMenu * actionsMenu = new QMenu(tr("Actions"), this);
	QMenu * helpMenu = new QMenu(tr("?"), this);
	helpMenu->addAction(tr("about qmisdnwatch"), this, SLOT(about()));
	helpMenu->addAction(tr("about Qt"), this, SLOT(aboutQt()));
	menuBar()->addMenu(optionsMenu);
	// menuBar()->addMenu(actionsMenu);
	menuBar()->addMenu(helpMenu);

	setCentralWidget(centralwidget);
	setWindowTitle(QString ("qmisdnwatch"));

	if (misdn.isCoreConnected())
		showMISDNversion();
	else
		debugOut(logText, "connecting mISDN core socket failed\n");

	updateDeviceList();
	deviceListTimer->start(1000);
}
