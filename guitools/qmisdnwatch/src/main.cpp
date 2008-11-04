/* $Id: main.cpp 10 2008-10-31 12:58:01Z daxtar $
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
#include "mainWindow.h"
#include "misdn.h"

Q_DECLARE_METATYPE(timeval);
QDataStream &operator<<( QDataStream &out, const timeval& ) { return out; }
QDataStream &operator>>( QDataStream &in, timeval& ) { return in; }

int main(int argc, char *argv[])  {
	qRegisterMetaType<timeval>("timeval");
	qRegisterMetaTypeStreamOperators<timeval>("timeval");

	QCoreApplication::setOrganizationName("mISDN");
	QCoreApplication::setOrganizationDomain("misdn.org");
	QCoreApplication::setApplicationName("qmisdnwatch");

	QApplication app(argc, argv);
	mainWindow* window = new mainWindow;
	window->show();
	return app.exec();
}
