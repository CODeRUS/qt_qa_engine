##
# Copyright (c) New Cloud Technologies, Ltd., 2014-2022
#
# You can not use the contents of the file in any way without
# New Cloud Technologies, Ltd. written permission.
#
# To obtain such a permit, you should contact New Cloud Technologies, Ltd.
# at http://ncloudtech.com/contact.html
#
##

# KDM-395. CMake minimum required version set to 3.13 for unified policy CMP0077 following.
cmake_minimum_required(VERSION 3.13)

# Load local CMake definitions. Before the project() command strictly!
find_file(LOCAL_DEFS local_defs.cmake "${CMAKE_CURRENT_BINARY_DIR}" NO_DEFAULT_PATH)
if (LOCAL_DEFS)
    include("${CMAKE_CURRENT_BINARY_DIR}/local_defs.cmake")
    message(STATUS "File ${CMAKE_CURRENT_BINARY_DIR}/local_defs.cmake loaded.")
elseif (CALLED_FROM_CBE)
    message(FATAL_ERROR "Can't find file ${CMAKE_CURRENT_BINARY_DIR}/local_defs.cmake!")
endif ()

project (qt_qa_engine)

include(GNUInstallDirs)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(LIBS "")
set(LIB_DEFS "")
set(LIB_INCLUDES "")

if(UNIX AND NOT APPLE)
    # Linux
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    # Temporary use MO_PLATFORM_MOBILE
    set(MO_PATFORM_MOBILE ON)
endif()

# Find Qt5 Core module
find_package(Qt5 ${CURRENT_QT_VERSION} COMPONENTS Core REQUIRED)
list(APPEND LIBS_INCLUDES ${Qt5Core_INCLUDE_DIRS})
list(APPEND LIBS_DEFS ${Qt5Core_DEFINITIONS})
list(APPEND LIBS Qt5::Core)

# Find Qt5 Quick module
find_package(Qt5 ${CURRENT_QT_VERSION} COMPONENTS Quick REQUIRED)
list(APPEND LIBS_INCLUDES ${Qt5Quick_INCLUDE_DIRS})
list(APPEND LIBS_INCLUDES ${Qt5Quick_PRIVATE_INCLUDE_DIRS})
list(APPEND LIBS_DEFS ${Qt5Quick_DEFINITIONS})
list(APPEND LIBS Qt5::Quick)

# Find Qt5 Gui module
# For uiapplication.h
find_package(Qt5 ${CURRENT_QT_VERSION} COMPONENTS Gui REQUIRED)
list(APPEND LIBS_INCLUDES ${Qt5Gui_INCLUDE_DIRS})
list(APPEND LIBS_DEFS ${Qt5Gui_DEFINITIONS})
list(APPEND LIBS Qt5::Gui)

# Find Qt5 Qml module
# For uiengine.h
find_package(Qt5 ${CURRENT_QT_VERSION} COMPONENTS Qml REQUIRED)
list(APPEND LIBS_INCLUDES ${Qt5Qml_INCLUDE_DIRS})
list(APPEND LIBS_DEFS ${Qt5Qml_DEFINITIONS})
list(APPEND LIBS Qt5::Qml)

# Find widgets module but only for linux build (used for system file dialog)
if(UNIX AND NOT APPLE AND NOT ANDROID)
    find_package(Qt5 ${CURRENT_QT_VERSION} COMPONENTS Widgets REQUIRED)
    list(APPEND LIBS_INCLUDES ${Qt5Widgets_INCLUDE_DIRS})
    list(APPEND LIBS_INCLUDES ${Qt5Widgets_PRIVATE_INCLUDE_DIRS})
    list(APPEND LIBS_DEFS ${Qt5Widgets_DEFINITIONS})
    list(APPEND LIBS Qt5::Widgets)
endif()

if (MO_PLATFORM_MOBILE)
    add_definitions(-DMO_PLATFORM_MOBILE)
endif()

file (GLOB_RECURSE ${PROJECT_NAME}_SOURCES *.cpp)
file (GLOB_RECURSE ${PROJECT_NAME}_HEADERS *.h)

qt5_add_resources(QRC_RESOURCES qml/qtqaengine.qrc)

add_library (${PROJECT_NAME}
    ${QRC_RESOURCES}
    ${${PROJECT_NAME}_SOURCES}
    ${${PROJECT_NAME}_HEADERS}
)

# > Generated moc_*.cpp and *.moc files are placed in the build directory
# so it is convenient to set the CMAKE_INCLUDE_CURRENT_DIR variable.
# @see https://cmake.org/cmake/help/v3.1/manual/cmake-qt.7.html#automoc

set_target_properties (${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    AUTOMOC ON
    EXPORT_NAME QAEngineLib
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        ${LIBS_DEFS}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${LIBS}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${LIBS_INCLUDES}
    PUBLIC
        $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>
)

# Installation
include(CMakePackageConfigHelpers)

set(TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")
set(TARGETS_EXPORT_FILE "${TARGETS_EXPORT_NAME}.cmake")
set(PROJECT_CONFIG "${PROJECT_NAME}Config.cmake")
set(PROJECT_CONFIG_IN "${PROJECT_CONFIG}.in")
set(NAMESPACE "AutoQA::")

if (ANDROID)
    set(CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
else()
    set(CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/")
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/cmake/${PROJECT_CONFIG_IN}
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_CONFIG}
    INSTALL_DESTINATION ${CONFIG_INSTALL_DIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_CONFIG}
    DESTINATION ${CONFIG_INSTALL_DIR}
)

install(TARGETS ${PROJECT_NAME} EXPORT ${TARGETS_EXPORT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT ${TARGETS_EXPORT_NAME}
    FILE ${TARGETS_EXPORT_FILE}
    NAMESPACE ${NAMESPACE}
    DESTINATION ${CONFIG_INSTALL_DIR}
)
