HEADERS += \
        edge.h \
        node.h \
        graphwidget.h

SOURCES += \
        edge.cpp \
        main.cpp \
        node.cpp \
        graphwidget.cpp

DEFINES += NETWORKACCESS

INCLUDEPATH += /home/hardaker/src/dnssec/dnssec-tools.git/dnssec-tools/validator/include
LIBS        += -lval-threads -lsres -lnsl -lcrypto -lpthread

TARGET.EPOCHEAPSIZE = 0x200000 0xA00000

include(deployment.pri)
qtcAddDeployment()

RESOURCES += dnssec-nodes.qrc

symbian {
    TARGET.UID3 = 0xA000A642
    include($$PWD/../../symbianpkgrules.pri)
}

simulator: warning(This example might not fully work on Simulator platform)