bin_SCRIPTS = qmisdnwatch$(EXEEXT)

QMISDNWATCH_S = res/16x16-bullet-gray.png res/16x16-bullet-green.png \
		res/16x16-bullet-red.png res/16x16-bullet-yellow.png res/icons.qrc \
		src/extraWidgets.cpp src/extraWidgets.h src/main.cpp \
		src/mainWindow.cpp src/mainWindow.h src/misdn.cpp \
		src/misdn.h src/Ql1logThread.cpp src/Ql1logThread.h

AM_CPPFLAGS = -I$(top_srcdir)/include

CLEANFILES = *~ src/*~ res/*~ Makefile.qt moc_*.cpp qrc_icons.cpp

Makefile.qt: $(srcdir)/qmisdnwatch.pro
		$(QMAKE) -o $@ $(srcdir)/qmisdnwatch.pro

qmisdnwatch$(EXEEXT): Makefile.qt $(QMISDNWATCH_S)
		$(MAKE) -f Makefile.qt

EXTRA_DIST = $(QMISDNWATCH_S) $(srcdir)/qmisdnwatch.pro

