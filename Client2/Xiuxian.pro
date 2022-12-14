QT       += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat
CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    Augur/augurwindow.cpp \
    Augur/guastacked.cpp \
    RedisLib/RedisClient.cpp \
    RedisLib/RedisCluster.cpp \
    RedisLib/RedisRespParser.cpp \
    RedisLib/RedisRoute.cpp \
    RedisLib/RedisTransMgr.cpp \
    UpdataInfo/feedback.cpp \
    UpdataInfo/updatainfp.cpp \
    chatmessage/chatwindow.cpp \
    chatmessage/conservsocket.cpp \
    chatmessage/emojipicker.cpp \
    chatmessage/qnchatmessage.cpp \
    chatmessage/wordfilter.cpp \
    jianghu/battleai.cpp \
    jianghu/jhleitai.cpp \
    jianghu/jhleitaiconfirm.cpp \
    jianghu/jianghu.cpp \
    jianghu/jianghuglobal.cpp \
    jianghu/skillbar.cpp \
    jianghu/skilldata.cpp \
    jianghu/userinfo.cpp \
    main.cpp \
    dialog.cpp \
    makename.cpp \
    qaesencryption.cpp \
    qlog.cpp \
    ranklist.cpp

HEADERS += \
    Augur/augurwindow.h \
    Augur/guastacked.h \
    RedisLib/RedisClient.h \
    RedisLib/RedisCluster.h \
    RedisLib/RedisDefine.h \
    RedisLib/RedisRespParser.h \
    RedisLib/RedisRoute.h \
    RedisLib/RedisTransMgr.h \
    UpdataInfo/feedback.h \
    UpdataInfo/updatainfp.h \
    chatmessage/chatwindow.h \
    chatmessage/conservsocket.h \
    chatmessage/emojipicker.h \
    chatmessage/qnchatmessage.h \
    chatmessage/wordfilter.h \
    dialog.h \
    jianghu/battleai.h \
    jianghu/jhleitai.h \
    jianghu/jhleitaiconfirm.h \
    jianghu/jianghu.h \
    jianghu/jianghuglobal.h \
    jianghu/skillbar.h \
    jianghu/skilldata.h \
    jianghu/userinfo.h \
    makename.h \
    qaesencryption.h \
    qlog.h \
    ranklist.h

FORMS += \
    Augur/augurwindow.ui \
    Augur/guastacked.ui \
    UpdataInfo/feedback.ui \
    UpdataInfo/updatainfp.ui \
    chatmessage/chatwindow.ui \
    chatmessage/emojipicker.ui \
    dialog.ui \
    jianghu/jhleitai.ui \
    jianghu/jhleitaiconfirm.ui \
    jianghu/jianghu.ui \
    jianghu/skillbar.ui \
    jianghu/userinfo.ui \
    ranklist.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    img.qrc \
    jianghu/jianghu.qrc
include(popupmanage.pri)
VERSION       = 1.5.1
RC_ICONS    = apjpm-5t3s5.ico

DEFINES += QT_MESSAGELOGCONTEXT
