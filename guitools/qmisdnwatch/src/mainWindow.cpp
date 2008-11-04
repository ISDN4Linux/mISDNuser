/* $Id: mainWindow.cpp 11 2008-10-31 18:35:50Z daxtar $
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
#include <QHBoxLayout>
#include <QGridLayout>
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
#include <QSettings>
#include <QPixmap>
#include <QCloseEvent>
#include <mISDNuser/mISDNif.h>
#include "mainWindow.h"


void mainWindow::about() {
	QMessageBox::information(this, tr("About qmisdnwatch"),
		tr("<b>qmisdnwatch</b> <b>v0.0.4</b><br>m.bachem@gmx.de<br>http://www.misdn.org/index.php/Qmisdnwatch"),
		QMessageBox::Ok);
}

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

int mainWindow::getDevIdByTabId(int tabId) {
	int i;
	for (i=0; i<devStack.count(); i++)
		if (devStack[i].tabId == tabId)
			return i;
	return -1;
}

int mainWindow::renameDevice(unsigned int id) {
	struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
	struct mISDN_devinfo * devInfo = getDevInfoById(id);
	if (!thisDevStuff || !devInfo)
		return -1;
	
	bool inputOk;
	QString newName = QInputDialog::getText(this, tr("rename Layer1"), "new device name:",
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

int mainWindow::actionRenameL1(void) {
	int i = getDevIdByTabId(tabsheet->currentIndex());
	if (i >= 0)
		return renameDevice(i);
	else
		return -1;
}

void mainWindow::actionConnectL1(void) {
	int id = getDevIdByTabId(tabsheet->currentIndex());
	if (id >= 0) {
		struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
		struct mISDN_devinfo * devInfo = getDevInfoById(id);
		if (!thisDevStuff || !devInfo)
			return;
		thisDevStuff->l1Thread->connectLayer1(devInfo);
	}
}

void mainWindow::actionDisconnectL1(void) {
	int id = getDevIdByTabId(tabsheet->currentIndex());
	if (id >= 0) {
		struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
		if (!thisDevStuff)
			return;
		thisDevStuff->l1Thread->disconnectLayer1();
	}
}

bool mainWindow::sendActivateReq(unsigned int id) {
	struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
	struct mISDN_devinfo * devInfo = getDevInfoById(id);
	if (!thisDevStuff || !devInfo)
		return false;
	if (thisDevStuff->l1Thread->connectLayer1(devInfo)) {
		if (!thisDevStuff->activationPending) {
			thisDevStuff->activationPending = true;
			thisDevStuff->labelDchIcon->setPixmap(*thisDevStuff->bulletYellow);
			debugOut(thisDevStuff->log, tr("<== LAYER1 ACTIVATE REQUEST\n"));
			return thisDevStuff->l1Thread->sendActivateReq();
		}
	}
	return false;
}

bool mainWindow::actionActivateReq(void) {
	int id = getDevIdByTabId(tabsheet->currentIndex());
	if (id >= 0)
		sendActivateReq(id);
	return false;
}

bool mainWindow::actionInformationReq(void) {
	int id = getDevIdByTabId(tabsheet->currentIndex());
	if (id >= 0) {
		struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
		struct mISDN_devinfo * devInfo = getDevInfoById(id);
		if (!thisDevStuff || !devInfo)
			return false;
		if ((thisDevStuff) && (thisDevStuff->l1Thread->connectLayer1(devInfo))) {
			debugOut(thisDevStuff->log, tr("<== LAYER1 INFORMATION REQUEST\n"));
			return thisDevStuff->l1Thread->sendInformationReq();
		}
	}
	return false;
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

int mainWindow::actionCleanL2(void) {
	int i = getDevIdByTabId(tabsheet->currentIndex());
	if (i >= 0)
		return cleanL2(i);
	else
		return -1;
}

void mainWindow::l1Connected(unsigned int id) {
	struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
	struct mISDN_devinfo * devInfo = getDevInfoById(id);
	if (!thisDevStuff || !devInfo)
		return;
	tabsheet->setTabText(thisDevStuff->tabId, QString("* %1").arg(devInfo->name));
	thisDevStuff->menuDisconnectL1->setEnabled(true);
	thisDevStuff->menuConnectL1->setEnabled(false);
	debugOut(thisDevStuff->log, tr("Layer1 socket connected (dev %1)\n").arg(id));
}

void mainWindow::l1Disonnected(unsigned int id) {
	struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
	struct mISDN_devinfo * devInfo = getDevInfoById(id);
	if (!thisDevStuff || !devInfo)
		return;
	tabsheet->setTabText(thisDevStuff->tabId, QString(devInfo->name));
	thisDevStuff->menuDisconnectL1->setEnabled(false);
	thisDevStuff->menuConnectL1->setEnabled(true);
	debugOut(thisDevStuff->log, tr("Layer1 socket disconnected (dev %1)\n").arg(id));
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

	switch (hh->prim) {
		case PH_ACTIVATE_CNF:
			debugOut(thisDevStuff->log, tr("==> LAYER1 ACTIVE\n"));
			thisDevStuff->labelDchIcon->setPixmap(*thisDevStuff->bulletGreen);
			thisDevStuff->activated = true;
			thisDevStuff->activationPending = false;
			break;
		case PH_ACTIVATE_IND:
			debugOut(thisDevStuff->log, tr("==> LAYER1 ACTIVATED\n"));
			thisDevStuff->labelDchIcon->setPixmap(*thisDevStuff->bulletGreen);
			thisDevStuff->activated = true;
			thisDevStuff->activationPending = false;
			break;
		case PH_DEACTIVATE_CNF:
			debugOut(thisDevStuff->log, tr("==> LAYER1 DEACTIVE\n"));
			thisDevStuff->labelDchIcon->setPixmap(*thisDevStuff->bulletGray);
			thisDevStuff->activated = false;
			thisDevStuff->activationPending = false;
			break;
		case PH_DEACTIVATE_IND:
			debugOut(thisDevStuff->log, tr("==> LAYER1 DEACTIVATED\n"));
			thisDevStuff->labelDchIcon->setPixmap(*thisDevStuff->bulletGray);
			thisDevStuff->activated = false;
			thisDevStuff->activationPending = false;
			break;
		case MPH_INFORMATION_IND:
			debugOut(thisDevStuff->log, tr("==> LAYER1 INFORMATION IND:"));
			binaryOut(thisDevStuff->log, data, sizeof(struct mISDNhead));
			break;
	}

	if (!thisDevStuff->captureL1)
		return;

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
	if (thisDevStuff->l1Thread->hasEcho() && (hh->prim == PH_DATA_REQ))
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
	thisDevStuff->captureFrameCnt++;
}

int mainWindow::switchL1Logging(unsigned int id) {
	struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
	struct mISDN_devinfo * devInfo = getDevInfoById(id);
	if ((!thisDevStuff) || (!devInfo))
		return -1;

	thisDevStuff->captureL1 = !thisDevStuff->captureL1;

	if (!thisDevStuff->captureL1) {
		if (thisDevStuff->l1Thread->hasEcho()) {
			debugOut(thisDevStuff->log,
				 tr("D/E channel logging stopped\n"));
			thisDevStuff->buttonLogL1->setText(
					tr("start D-/E-Channel logging"));
		}
		else {
			debugOut(thisDevStuff->log,
				 tr("D channel logging stopped\n"));
			thisDevStuff->buttonLogL1->setText(
					tr("start D-Channel logging"));
		}
	} else {
		thisDevStuff->eyeSDN.clear();
		thisDevStuff->captureFrameCnt = 0;
		if (thisDevStuff->l1Thread->connectLayer1(devInfo)) {
			QByteArray eyeHeader("EyeSDN");
			thisDevStuff->eyeSDN.append(eyeHeader);
			if (thisDevStuff->l1Thread->hasEcho()) {
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
				 tr("no connetion Layer1 established\n"));
			thisDevStuff->captureL1 = false;
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

int mainWindow::showActionMenu(unsigned int id) {
	struct isdnDeviceStuff * thisDevStuff = getStuffbyId(id);
	struct mISDN_devinfo * devInfo = getDevInfoById(id);
	if ((!thisDevStuff) || (!devInfo))
		return -1;
	thisDevStuff->actionPopup->exec(QCursor::pos()); // popup();
	return 0;
}

void mainWindow::createNewDeviceTab(struct mISDN_devinfo *devInfo) {
	struct isdnDeviceStuff newDevStuff;
	unsigned int i;

	qDebug("device %d arised (%s) nrbchan(%d)", devInfo->id, devInfo->name,
		devInfo->nrbchan);

	/* misc control vars */
	newDevStuff.id = devInfo->id;
	newDevStuff.captureL1 = false;
	newDevStuff.activated = false;
	newDevStuff.activationPending = false;
	newDevStuff.tab = new QWidget;
	newDevStuff.tabId = tabsheet->addTab(newDevStuff.tab,
					     QString(devInfo->name));

	/* Button 'L1 logging' */
	newDevStuff.buttonLogL1 = new idButton(tr("start D-/E-Channel logging"),
			newDevStuff.tab, newDevStuff.id);
	connect(newDevStuff.buttonLogL1, SIGNAL(clicked(unsigned int)),
		this, SLOT(switchL1Logging(unsigned int)));

	/* Button 'saveLog' */
	newDevStuff.buttonSaveLog = new idButton(tr("Save Log As"),
			newDevStuff.tab, newDevStuff.id);
	connect(newDevStuff.buttonSaveLog, SIGNAL(clicked(unsigned int)),
		this, SLOT(saveL1Log(unsigned int)));

	/* Button 'ISDN Actions' */
	newDevStuff.buttonActionMenu = new idButton(tr("Device Actions"),
			newDevStuff.tab, newDevStuff.id);
	connect(newDevStuff.buttonActionMenu, SIGNAL(clicked(unsigned int)),
		this, SLOT(showActionMenu(unsigned int)));

	/* color bullets */
	newDevStuff.bulletRed = new QPixmap(":red.png");
	newDevStuff.bulletGreen = new QPixmap(":green.png");
	newDevStuff.bulletGray = new QPixmap(":gray.png");
	newDevStuff.bulletYellow = new QPixmap(":yellow.png");

	/* D-Channel icon */
	newDevStuff.labelDchText = new QLabel(newDevStuff.tab);
	newDevStuff.labelDchIcon = new QLabel(newDevStuff.tab);
	newDevStuff.labelDchIcon->setPixmap(*newDevStuff.bulletRed);

	/* B-Channel icon */
	newDevStuff.labelBchText = new QLabel(tr("B-Channels: "), newDevStuff.tab);
	for (i=0; i<devInfo->nrbchan; i++) {
		newDevStuff.labelBchIcon[i] = new QLabel(newDevStuff.tab);
		newDevStuff.labelBchIcon[i]->setPixmap(*newDevStuff.bulletRed);
	}

	/* Text Widget */
	newDevStuff.log = new QTextEdit();

	/* l1 Thread */
	newDevStuff.l1Thread = new Ql1logThread(devInfo->id, misdn);
	newDevStuff.l1Thread->setParent(newDevStuff.tab);
	connect(newDevStuff.l1Thread, SIGNAL(l1Connected(unsigned int)),
		this, SLOT(l1Connected(unsigned int)));
	connect(newDevStuff.l1Thread, SIGNAL(l1Disonnected(unsigned int)),
		this, SLOT(l1Disonnected(unsigned int)));
	connect(newDevStuff.l1Thread, SIGNAL(rcvData(unsigned int, QByteArray, struct timeval)),
		this, SLOT(logRcvData(unsigned int, QByteArray, struct timeval)));

	/* action PopupMenu */
	newDevStuff.actionPopup = new QMenu(newDevStuff.tab);
	newDevStuff.menuConnectL1 = newDevStuff.actionPopup->
			addAction(tr("connect Layer1"), this, SLOT(actionConnectL1()));
	newDevStuff.menuDisconnectL1 = newDevStuff.actionPopup->
			addAction(tr("disconnect Layer1"), this, SLOT(actionDisconnectL1()));
	newDevStuff.menuDisconnectL1->setEnabled(false);
	newDevStuff.actionPopup->addSeparator();
	newDevStuff.actionPopup->addAction(tr("send ACTIVATE REQUEST"), this, SLOT(actionActivateReq()));
	newDevStuff.actionPopup->addAction(tr("send INFORMATION REQUEST"), this, SLOT(actionInformationReq()));
	newDevStuff.actionPopup->addSeparator();
	newDevStuff.actionPopup->addAction(tr("rename Layer1"), this, SLOT(actionRenameL1()));
	newDevStuff.actionPopup->addAction(tr("clean Layer2"), this, SLOT(actionCleanL2()));

	QHBoxLayout *hbox;
	QVBoxLayout *vbox;
	QGridLayout *grid;

	/* Layout */
	vbox = new QVBoxLayout(newDevStuff.tab);

	hbox = new QHBoxLayout();
	hbox->addWidget(newDevStuff.buttonActionMenu);
	hbox->addSpacing(5);
	hbox->addWidget(newDevStuff.buttonLogL1);
	hbox->insertStretch(3, 0);
	vbox->addItem(hbox);

	hbox = new QHBoxLayout();
	grid = new QGridLayout();
	grid->addWidget(newDevStuff.labelDchText, 0, 0);
	grid->addWidget(newDevStuff.labelDchIcon, 0, 1);
	grid->addWidget(newDevStuff.labelBchText, 1, 0);
	for (i=0; i<devInfo->nrbchan; i++)
		grid->addWidget(newDevStuff.labelBchIcon[i], 1, i+1);
	hbox->addItem(grid);
	hbox->insertStretch(2,0);
	vbox->addItem(hbox);

	vbox->addWidget(newDevStuff.log);

	hbox = new QHBoxLayout();
	hbox->addWidget(newDevStuff.buttonSaveLog);
	hbox->insertStretch(1, 0);
	vbox->addItem(hbox);

	devStack << newDevStuff;

	struct isdnDeviceStuff * thisDevStuff =  getStuffbyId(devInfo->id);
	if (!thisDevStuff)
		return;
	if ((optionAutoConnectTE->isChecked()) && IS_ISDN_P_TE(devInfo->protocol))
		thisDevStuff->l1Thread->connectLayer1(devInfo);
	if ((optionAutoConnectNT->isChecked()) && IS_ISDN_P_NT(devInfo->protocol))
		thisDevStuff->l1Thread->connectLayer1(devInfo);
	if ((optionAutoConnectUnused->isChecked()) && !devInfo->protocol)
		thisDevStuff->l1Thread->connectLayer1(devInfo);
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
	struct mISDN_devinfo devInfo;
	int newdev;

	/* clear old widgets */
	devtree->clear();
	removeVanishedDevices();

	QTreeWidgetItem* devitem;
	for (i=0; i<n; i++) {
		devInfo = deviceList[i];

		newdev = 1;
		for (j=0; j<devStack.count(); j++)
			if (devInfo.id == devStack[j].id)
				newdev = 0;

		/* create new Device Tab and stuff */
		if (newdev)
			 createNewDeviceTab(&devInfo);

		struct isdnDeviceStuff * thisDevStuff = getStuffbyId(devInfo.id);
		if (!thisDevStuff)
			return;

		/* Update main Device Tree */
		devitem = new QTreeWidgetItem(devtree);
		devitem->setText(0, QString(devInfo.name));
		thisDevStuff->treeHead = devitem;

		QTreeWidgetItem* optitem;

		optitem = new QTreeWidgetItem(devitem);
		optitem->setText(0, tr("Id: %1")
				.arg(devInfo.id));

		optitem = new QTreeWidgetItem(devitem);
		optitem->setText(0, tr("D protocols: %1")
				.arg(devInfo.Dprotocols));
		if (devInfo.Dprotocols) {
			QTreeWidgetItem* l1item = new QTreeWidgetItem(optitem);
			l1item->setText(0, tr("Layer 1"));

			QTreeWidgetItem* doptitem;
			if (devInfo.Dprotocols & (1 << ISDN_P_TE_S0)) {
				doptitem = new QTreeWidgetItem(l1item);
				doptitem->setText(0, tr("S0 TE"));
			}
			if (devInfo.Dprotocols & (1 << ISDN_P_NT_S0)) {
				doptitem = new QTreeWidgetItem(l1item);
				doptitem->setText(0, tr("S0 NT"));
			}
			if (devInfo.Dprotocols & (1 << ISDN_P_TE_E1)) {
				doptitem = new QTreeWidgetItem(l1item);
				doptitem->setText(0, tr("E1 TE"));
			}
			if (devInfo.Dprotocols & (1 << ISDN_P_NT_E1)) {
				doptitem = new QTreeWidgetItem(l1item);
				doptitem->setText(0, tr("E1 NT"));
			}
			if (devInfo.Dprotocols & (1 << ISDN_P_TE_UP0)) {
				doptitem = new QTreeWidgetItem(l1item);
				doptitem->setText(0, tr("UP0 TE"));
			}
			if (devInfo.Dprotocols & (1 << ISDN_P_NT_UP0)) {
				doptitem = new QTreeWidgetItem(l1item);
				doptitem->setText(0, tr("UP0 NT"));
			}
		}

		optitem = new QTreeWidgetItem(devitem);
		optitem->setText(0, tr("B protocols: %1")
				.arg(devInfo.Bprotocols));
		if (devInfo.Bprotocols) {
			QTreeWidgetItem* l1item = new QTreeWidgetItem(optitem);
			l1item->setText(0, QString("Layer 1"));
			QTreeWidgetItem* boptitem;
			if (devInfo.Bprotocols & (1 <<
						 (ISDN_P_B_RAW
						 - ISDN_P_B_START))) {
				boptitem = new QTreeWidgetItem(l1item);
				boptitem->setText(0,
						tr("RAW (transparent)"));
			}
			if (devInfo.Bprotocols & (1 <<
						 (ISDN_P_B_HDLC
						 - ISDN_P_B_START))) {
				boptitem = new QTreeWidgetItem(l1item);
				boptitem->setText(0, tr("HDLC"));
			}
			if (devInfo.Bprotocols & (1 <<
						 (ISDN_P_B_X75SLP
						 - ISDN_P_B_START))) {
				boptitem = new QTreeWidgetItem(l1item);
				boptitem->setText(0, tr("X.75"));
			}
			QTreeWidgetItem* l2item = new QTreeWidgetItem(optitem);
			l2item->setText(0, tr("Layer 2"));
			if (devInfo.Bprotocols & (1 <<
						 (ISDN_P_B_L2DTMF
						 - ISDN_P_B_START))) {
				boptitem = new QTreeWidgetItem(l2item);
				boptitem->setText(0, tr("DTMF"));
			}
		}
		optitem = new QTreeWidgetItem(devitem);
		optitem->setText(0, QString(tr("protocol: %1"))
				.arg(devInfo.protocol));
		QTreeWidgetItem* l1item = new QTreeWidgetItem(optitem);
		thisDevStuff->labelDchText->setText("D-Channel: ");
		switch (devInfo.protocol) {
			case 0:
				l1item->setText(0, tr("unused"));
				break;
			case ISDN_P_TE_S0:
				l1item->setText(0, tr("S0 TE"));
				thisDevStuff->labelDchText->setText("D-Channel S0 TE: ");
				break;
			case ISDN_P_NT_S0:
				l1item->setText(0, tr("S0 NT"));
				thisDevStuff->labelDchText->setText("D-Channel S0 NT: ");
				break;
			case ISDN_P_TE_E1:
				l1item->setText(0, tr("E1 TE"));
				thisDevStuff->labelDchText->setText("D-Channel E1 TE: ");
				break;
			case ISDN_P_NT_E1:
				l1item->setText(0, tr("E1 NT"));
				thisDevStuff->labelDchText->setText("D-Channel E1 NT: ");
				break;
			case ISDN_P_TE_UP0:
				l1item->setText(0, tr("UP0 TE"));
				thisDevStuff->labelDchText->setText("D-Channel UP0 TE: ");
				break;
			case ISDN_P_NT_UP0:
				l1item->setText(0, tr("UP0 NT"));
				thisDevStuff->labelDchText->setText("D-Channel UP0 NT: ");
				break;
			default:
				l1item->setText(0, tr("unkown protocol"));
				break;
		}
		optitem = new QTreeWidgetItem(devitem);
		optitem->setText(0, tr("B-Channels: %1")
				.arg(devInfo.nrbchan));
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
		struct mISDN_devinfo devInfo;

		for (j=0; ((j<MAX_DEVICE_ID) && (tmpDevList.count() < i)); j++)
			if (misdn.getDeviceInfo(&devInfo, j) >= 0)
				tmpDevList << devInfo;

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
	
	/* auto Activate? */
	if (optionAutoActivateTE->isChecked() || optionAutoActivateNT->isChecked()) {
		for (i=0; i<devStack.count(); i++) {
			struct mISDN_devinfo * devInfo = getDevInfoById(devStack[i].id);
			if (devInfo) {
				if (IS_ISDN_P_TE(devInfo->protocol) && optionAutoActivateTE->isChecked()
						&& !devStack[i].activated) {
					sendActivateReq(devStack[i].id);
				}
				if (IS_ISDN_P_NT(devInfo->protocol) && optionAutoActivateNT->isChecked()
						&& !devStack[i].activated) {
					sendActivateReq(devStack[i].id);
				}
			}
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

void mainWindow::aboutQt() {
	QMessageBox::aboutQt(this);
}

void mainWindow::closeEvent( QCloseEvent* ce ) {
	QSettings settings;
	qDebug("closing...");
	settings.setValue(tr("layer1/autoConnectTE"), optionAutoConnectTE->isChecked()?1:0);
	settings.setValue(tr("layer1/autoConnectNT"), optionAutoConnectNT->isChecked()?1:0);
	settings.setValue(tr("layer1/autoConnectUnused"), optionAutoConnectUnused->isChecked()?1:0);
	settings.setValue(tr("layer1/autoActivateTE"), optionAutoActivateTE->isChecked()?1:0);
	settings.setValue(tr("layer1/autoActivateNT"), optionAutoActivateNT->isChecked()?1:0);
	// settings.setValue(tr("layer1/pollInformation"), optionPollInformation->isChecked()?1:0);
	ce->accept();
}

mainWindow::mainWindow( QMainWindow *parent,
                    Qt::WindowFlags flags ) :
	QMainWindow(parent, flags)
{
	QSettings settings;
	
	resize(520, 400);

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
	QMenu * autoConnect = new QMenu(tr("auto-Connect"), optionsMenu);
	optionsMenu->addMenu(autoConnect);

	optionAutoConnectTE = autoConnect->addAction(tr("TE ports"));
	optionAutoConnectTE->setCheckable(true);
	optionAutoConnectTE->setChecked(settings.value(tr("layer1/autoConnectTE"), 1).toInt() == 1);

	optionAutoConnectNT = autoConnect->addAction(tr("NT ports"));
	optionAutoConnectNT->setCheckable(true);
	optionAutoConnectNT->setChecked(settings.value(tr("layer1/autoConnectNT"), 1).toInt() == 1);

	optionAutoConnectUnused = autoConnect->addAction(tr("unused Ports"));
	optionAutoConnectUnused->setCheckable(true);
	optionAutoConnectUnused->setChecked(settings.value(tr("layer1/autoConnectUnused"), 0).toInt() == 1);

	QMenu * autoActivate = new QMenu(tr("auto-Activate"), optionsMenu);
	optionsMenu->addMenu(autoActivate);

	optionAutoActivateTE = autoActivate->addAction(tr("TE ports"));
	optionAutoActivateTE->setCheckable(true);
	optionAutoActivateTE->setChecked(settings.value(tr("layer1/autoActivateTE"), 1).toInt() == 1);

	optionAutoActivateNT = autoActivate->addAction(tr("NT ports"));
	optionAutoActivateNT->setCheckable(true);
	optionAutoActivateNT->setChecked(settings.value(tr("layer1/autoActivateNT"), 0).toInt() == 1);

	/*
	optionPollInformation = optionsMenu->addAction(tr("poll Layer1 Information"));
	optionPollInformation->setCheckable(true);
	optionPollInformation->setChecked(settings.value(tr("layer1/pollInformation"), 0).toInt() == 1);
	*/

	QMenu * helpMenu = new QMenu(tr("?"), this);
	helpMenu->addAction(tr("about qmisdnwatch"), this, SLOT(about()));
	helpMenu->addAction(tr("about Qt"), this, SLOT(aboutQt()));
	menuBar()->addMenu(optionsMenu);
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

