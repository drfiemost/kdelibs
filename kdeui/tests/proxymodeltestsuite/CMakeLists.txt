project(proxymodeltestsuite)

set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")

  find_package(Qt4 REQUIRED)
  find_package(Automoc4 REQUIRED)
  cmake_minimum_required(VERSION 2.6.3)
endif()

include_directories(
  ${QT_INCLUDES}
  ${proxymodeltestsuite_BINARY_DIR}
)

set(proxymodeltestsuite_SRCS
    dynamictreemodel.cpp
    dynamictreewidget.cpp
    modelcommander.cpp
    modelspy.cpp
    modelselector.cpp
    modeltest.cpp
    proxymodeltest.cpp
    modeldumper.cpp
    modeleventlogger.cpp
    eventloggerregister.cpp
)

set(eventlogger_RCSS
  eventlogger.qrc
)

qt4_add_resources(
  eventlogger_RCS_SRCS
  ${eventlogger_RCSS}
)

automoc4_add_library(proxymodeltestsuite ${LIBRARY_TYPE}
  ${proxymodeltestsuite_SRCS}
  ${eventlogger_RCS_SRCS}
)

target_link_libraries(proxymodeltestsuite
   ${QT_QTCORE_LIBRARY}
   ${QT_QTGUI_LIBRARY}
   ${QT_QTTEST_LIBRARY}
)
set_target_properties(proxymodeltestsuite PROPERTIES
    INTERFACE_LINK_LIBRARIES ""
    DEFINE_SYMBOL MAKE_PROXYMODELTESTSUITE_LIB)
