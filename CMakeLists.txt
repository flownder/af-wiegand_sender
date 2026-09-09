cmake_minimum_required(VERSION 3.16)

project(af_weigand_sender VERSION 0.1 LANGUAGES CXX)

# ---------------------------------------------------------
# 1. Подключаем QMQTT через FetchContent
# ---------------------------------------------------------
include(FetchContent)

# Отключаем сборку примеров и тестов из qmqtt, чтобы ускорить сборку
set(QMQTT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QMQTT_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# ВАЖНО: Отключаем SSL, если он вам не нужен. 
# Сборка OpenSSL под Windows в CI — это отдельная головная боль.
# Если вам нужен wss:// (MQTT over SSL), эту строку нужно убрать, 
# но тогда придется отдельно устанавливать OpenSSL через vcpkg или choco.
set(QMQTT_BUILD_WITH_SSL OFF CACHE BOOL "" FORCE) 

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)

FetchContent_Declare(
    qmqtt
    GIT_REPOSITORY https://github.com/emqx/qmqtt.git
    GIT_TAG master # Рекомендуется указать конкретный тег, например 'v1.0.0', для стабильности
)

FetchContent_MakeAvailable(qmqtt)
# ---------------------------------------------------------

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort)

set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        src/mainwindow.h
        src/mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(af_weigand_sender
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET af_weigand_sender APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(af_weigand_sender SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(af_weigand_sender
            ${PROJECT_SOURCES}
            src/mqtt_client.cpp
            src/mqtt_client.h
            src/serialworker.h src/serialworker.cpp
        )
    endif()
endif()


target_link_libraries(af_weigand_sender PRIVATE Qt${QT_VERSION_MAJOR}::Widgets qmqtt Qt${QT_VERSION_MAJOR}::SerialPort)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.af_weigand_sender)
endif()
set_target_properties(af_weigand_sender PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS af_weigand_sender
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(af_weigand_sender)
endif()
