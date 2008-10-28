/* $Id: extraWidgets.h 4 2008-10-28 00:04:24Z daxtar $
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

/* QBushbutton  sending mISDN device ID at clicked() signal */
class idButton : public QPushButton {
	Q_OBJECT
	public:
		idButton( const QString& text, unsigned int id, QWidget* parent = NULL );
	private:
		unsigned int devId;
	public slots:
		void click();
	signals:
		void clicked(unsigned int id);
};

#endif // _EXTRA_WIDGETS_H_
