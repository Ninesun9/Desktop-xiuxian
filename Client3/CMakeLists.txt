cmake_minimum_required(VERSION 3.21)
project(XiuxianPet VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

qt_add_executable(XiuxianPet
    # ── Entry ────────────────────────────────────────────────────────────
    src/main.cpp
    src/petwindow.h
    src/petwindow.cpp

    # ── Core ─────────────────────────────────────────────────────────────
    src/core/playerprofile.h
    src/core/playerprofilestore.h
    src/core/playerprofilestore.cpp
    src/core/gamelogic.h
    src/core/gamelogic.cpp

    # ── Network ──────────────────────────────────────────────────────────
    src/network/apiclient.h
    src/network/apiclient.cpp

    # ── Util ─────────────────────────────────────────────────────────────
    src/util/makename.h
    src/util/makename.cpp
    src/util/messageshow.h
    src/util/messageshow.cpp
    src/util/popupmanager.h
    src/util/popupmanager.cpp

    # ── Augur ────────────────────────────────────────────────────────────
    src/augur/guastacked.h
    src/augur/guastacked.cpp
    src/augur/augurwindow.h
    src/augur/augurwindow.cpp

    # ── Mail ─────────────────────────────────────────────────────────────
    src/mail/mailservice.h
    src/mail/mailservice.cpp
    src/mail/mailstore.h
    src/mail/mailstore.cpp
    src/mail/mailapiclient.h
    src/mail/mailapiclient.cpp
    src/mail/maildrawer.h
    src/mail/maildrawer.cpp

    # ── Chat ─────────────────────────────────────────────────────────────
    src/chat/wordfilter.h
    src/chat/wordfilter.cpp
    src/chat/conservsocket.h
    src/chat/conservsocket.cpp
    src/chat/qnchatmessage.h
    src/chat/qnchatmessage.cpp
    src/chat/emojipicker.h
    src/chat/emojipicker.cpp
    src/chat/chatwindow.h
    src/chat/chatwindow.cpp

    # ── Jianghu ──────────────────────────────────────────────────────────
    src/jianghu/jianghuglobal.h
    src/jianghu/jianghuglobal.cpp
    src/jianghu/skilldata.h
    src/jianghu/skilldata.cpp
    src/jianghu/skillbar.h
    src/jianghu/skillbar.cpp
    src/jianghu/battleai.h
    src/jianghu/battleai.cpp
    src/jianghu/userinfo.h
    src/jianghu/userinfo.cpp
    src/jianghu/jhleitai.h
    src/jianghu/jhleitai.cpp
    src/jianghu/jianghu.h
    src/jianghu/jianghu.cpp

    # ── Update / Feedback ────────────────────────────────────────────────
    src/update/updatechecker.h
    src/update/updatechecker.cpp
    src/update/feedback.h
    src/update/feedback.cpp

    # ── Rank ─────────────────────────────────────────────────────────────
    src/rank/rankwindow.h
    src/rank/rankwindow.cpp

    # ── Resources ────────────────────────────────────────────────────────
    resources/resources.qrc
)

target_include_directories(XiuxianPet PRIVATE src)

target_link_libraries(XiuxianPet PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

set_target_properties(XiuxianPet PROPERTIES
    WIN32_EXECUTABLE TRUE
    QT_TARGET_VERSION "1.0.0"
)

if(WIN32)
    find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET XiuxianPet POST_BUILD
            COMMAND ${WINDEPLOYQT}
                --no-translations
                --no-system-d3d-compiler
                $<TARGET_FILE:XiuxianPet>
            COMMENT "Running windeployqt..."
        )
    endif()
endif()
