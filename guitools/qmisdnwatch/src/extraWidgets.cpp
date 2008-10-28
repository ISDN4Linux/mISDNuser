/* $Id: extraWidgets.cpp 4 2008-10-28 00:04:24Z daxtar $
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

#include <QObject>
#include <QWidget>
#include <QPushButton>
#include "extraWidgets.h"

idButton::idButton( const QString& text, unsigned int id, QWidget* parent )
		: QPushButton(text, parent), devId(id) {
	connect(this, SIGNAL(clicked()), this, SLOT(click()));
}

void idButton::click() {
	emit clicked(devId);
}
