bin_PROGRAMS = qmisdnwatch
nodist_qmisdnwatch_SOURCES = Makefile.qt 

QMISDNWATCH_S = res/16x16-bullet-gray.png res/16x16-bullet-green.png \
		res/16x16-bullet-red.png res/16x16-bullet-yellow.png res/icons.qrc \
		src/extraWidgets.cpp src/extraWidgets.h src/main.cpp \
		src/mainWindow.cpp src/mainWindow.h src/misdn.cpp \
		src/misdn.h src/Ql1logThread.cpp src/Ql1logThread.h


CLEANFILES = *~ src/*~ res/*~ Makefile.qt

Makefile.qt: qmisdnwatch.pro
		$(QMAKE) -o $@

qmisdnwatch: Makefile.qt $(QMISDNWATCH_S)
		$(MAKE) -f Makefile.qt

EXTRA_DIST = $(QMISDNWATCH_S) qmisdnwatch.pro

