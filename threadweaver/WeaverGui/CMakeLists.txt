include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/Weaver/)


set(ThreadWeaverGui_LIB_SRCS  WeaverThreadGrid.cpp )


include_directories( ../Weaver )

kde4_add_library(ThreadWeaverGui ${LIBRARY_TYPE} ${ThreadWeaverGui_LIB_SRCS})
target_link_libraries(ThreadWeaverGui threadweaverQt4::QtGui)
set_target_properties(ThreadWeaverGui PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION})

install(TARGETS ThreadWeaverGui EXPORT kdelibsLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES WeaverThreadGrid.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/threadweaver/ COMPONENT Devel
)
