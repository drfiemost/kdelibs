set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )


########### fakehardwaretest ###############

set(fakehardwaretest_SRCS fakehardwaretest.cpp )


kde4_add_unit_test(fakehardwaretest ${fakehardwaretest_SRCS})

if(WIN32)
  set_target_properties(fakehardwaretest PROPERTIES COMPILE_FLAGS -DSOLID_EXPORT=)
endif(WIN32)

target_link_libraries(fakehardwaretest solid_static ${QT_QTCORE_LIBRARY} ${QT_QTDBUS_LIBRARY} Qt4::QtXml Qt4::QtTest )
add_definitions(-DTEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../solid/backends/fakehw/fakecomputer.xml")


########### solidhwtest ###############

set(solidhwtest_SRCS 
   solidhwtest.cpp )


kde4_add_unit_test(solidhwtest ${solidhwtest_SRCS})
add_definitions(-DFAKE_COMPUTER_XML="${CMAKE_CURRENT_SOURCE_DIR}/../solid/backends/fakehw/fakecomputer.xml")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../solid/backends/fakehw )

if(WIN32)
  set_target_properties(solidhwtest PROPERTIES COMPILE_FLAGS -DSOLID_EXPORT=)
  set (LIBS ${KDEWIN_LIBRARIES})
endif(WIN32)

target_link_libraries(solidhwtest Qt4::QtCore Qt4::QtDBus Qt4::QtXml Qt4::QtTest ${LIBS} solid_static)

########### solidmttest ###############

set(solidmttest_SRCS 
   solidmttest.cpp )


kde4_add_unit_test(solidmttest ${solidmttest_SRCS})
add_definitions(-DFAKE_COMPUTER_XML="${CMAKE_CURRENT_SOURCE_DIR}/../solid/backends/fakehw/fakecomputer.xml")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../solid/backends/fakehw )

if(WIN32)
  set_target_properties(solidmttest PROPERTIES COMPILE_FLAGS -DSOLID_EXPORT=)
  set (LIBS ${KDEWIN_LIBRARIES})
endif(WIN32)

target_link_libraries(solidmttest Qt4::QtCore Qt4::QtDBus Qt4::QtXml Qt4::QtTest ${LIBS} solid_static)

########### solidnettestdbusservice ###############

#set(solidnettestdbusservice_SRCS
#   solidnettestdbusservice.cpp )


#QT4_ADD_DBUS_ADAPTOR(solidnettestdbusservice_SRCS ../solid/org.kde.Solid.Networking.Client.xml
#                         solidnettestdbusservice.h TestNetworkingService)

#kde4_add_executable(solidnettestdbusservice TEST ${solidnettestdbusservice_SRCS})

#target_link_libraries(solidnettestdbusservice ${QT_QTCORE_LIBRARY} ${QT_QTDBUS_LIBRARY} Qt4::QtXml Qt4::QtTest)

########### networkingclient ###############

include_directories (${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} 
    ${KDE4_KDECORE_INCLUDES} ${KDE4_KDEUI_INCLUDES} ${KDE4_KIO_INCLUDES})

set(networkingclient_SRCS networkingclient.cpp)

kde4_add_ui_files(networkingclient_SRCS networkingclientview.ui)

qt4_add_dbus_interface(networkingclient_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../solid/org.kde.Solid.Networking.Client.xml clientinterface)


kde4_add_executable(networkingclient TEST ${networkingclient_SRCS})

if(WIN32)
    set_target_properties( networkingclient PROPERTIES COMPILE_FLAGS -DSOLID_EXPORT=)
endif(WIN32)

target_link_libraries(networkingclient solid_static ${KDE4_KDEUI_LIBS})
########### install files ###############

