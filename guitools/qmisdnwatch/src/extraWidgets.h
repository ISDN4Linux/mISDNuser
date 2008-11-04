/* $Id: extraWidgets.h 9 2008-10-30 20:44:26Z daxtar $
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

#ifndef _EXTRA_WIDGETS_H_
#define _EXTRA_WIDGETS_H_

#include <QPushButton>
#include <QAction>

/* QBushbutton sending mISDN device ID at clicked() signal */
class idButton : public QPushButton {
	Q_OBJECT
	public:
		idButton(const QString& text, QWidget* parent = NULL,
			  unsigned int id=0);
	private:
		unsigned int devId;
	public slots:
		void click();
	signals:
		void clicked(unsigned int id);
};


/* QAction sending mISDN device ID at clicked() signal */
class idAction : public QAction {
	Q_OBJECT
	public:
		idAction(const QString & text, QObject * parent,
			 unsigned int id);
		idAction(const QIcon & icon, const QString & text,
			 QObject * parent, unsigned int id);
	private:
		unsigned int devId;
	public slots:
		void trigger();
	signals:
		void triggered(unsigned int id);
};

#endif // _EXTRA_WIDGETS_H_
