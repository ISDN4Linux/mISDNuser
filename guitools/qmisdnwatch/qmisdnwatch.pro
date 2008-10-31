TEMPLATE = app
TARGET = 
DEPENDPATH += . src
INCLUDEPATH += . src

# not yet ;) LIBS += -lmisdn

# Input
HEADERS += src/mainWindow.h src/misdn.h src/extraWidgets.h src/Ql1logThread.h
SOURCES += src/main.cpp src/mainWindow.cpp src/misdn.cpp src/extraWidgets.cpp src/Ql1logThread.cpp

RESOURCES = res/icons.qrc
