/* $Id: extraWidgets.cpp 9 2008-10-30 20:44:26Z daxtar $
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

/*
 * the Widgets below are childs of commmon Qt Widgets, but signal the mISDN
 * device ID along with their main action trigger.
 *
 */


/* idButton (QPushButton) */

idButton::idButton( const QString& text, QWidget* parent,
		    unsigned int id)
		: QPushButton(text, parent), devId(id) {
	connect(this, SIGNAL(clicked()), this, SLOT(click()));
}

void idButton::click() {
	emit clicked(devId);
}

/******************************************************************************/


/* idAction (QAction) */

idAction::idAction(const QString & text, QObject * parent,
		   unsigned int id)
	: QAction(text, parent), devId(id) {
	connect(this, SIGNAL(triggered()), this, SLOT(trigger()));
}

idAction::idAction(const QIcon & icon, const QString & text, QObject * parent,
		   unsigned int id)
	: QAction(icon, text, parent), devId(id) {
	connect(this, SIGNAL(triggered()), this, SLOT(trigger()));
}

void idAction::trigger() {
	emit triggered(devId);
}
