
project(kdesu)

include_directories( ${KDE4_KPTY_INCLUDES} )

check_function_exists(setpriority HAVE_SETPRIORITY)
check_function_exists(getpeereid HAVE_GETPEEREID)

set(CMAKE_EXTRA_INCLUDE_FILES sys/socket.h)
check_type_size("struct ucred" STRUCT_UCRED) #defines HAVE_STRUCT_UCRED (bool) and STRUCT_UCRED (size, unused)
set(CMAKE_EXTRA_INCLUDE_FILES)  #reset CMAKE_EXTRA_INCLUDE_FILES

configure_file(config-kdesu.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kdesu.h )

########### next target ###############

set(kdesu_LIB_SRCS
   client.cpp
   process.cpp
   kcookie.cpp
   su.cpp
   ssh.cpp
   stub.cpp
   )


kde4_add_library(kdesu ${LIBRARY_TYPE} ${kdesu_LIB_SRCS})

target_link_libraries(kdesu LINK_PRIVATE ${KDE4_KPTY_LIBS} )
target_link_libraries(kdesu LINK_PUBLIC Qt4::QtCore)

if(KDESU_USE_SUDO_DEFAULT)
   set_source_files_properties(su.cpp PROPERTIES 
                               COMPILE_FLAGS -DKDESU_USE_SUDO_DEFAULT)
endif(KDESU_USE_SUDO_DEFAULT)

set_target_properties(kdesu PROPERTIES
   VERSION ${KDE_NON_GENERIC_LIB_VERSION}
   SOVERSION ${KDE_NON_GENERIC_LIB_SOVERSION}
)

install(TARGETS kdesu EXPORT kdelibsLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS} )


########### next target ###############

set(kdesu_stub_SRCS kdesu_stub.c )

kde4_add_executable(kdesu_stub NOGUI ${kdesu_stub_SRCS})

install(TARGETS kdesu_stub DESTINATION ${LIBEXEC_INSTALL_DIR} )


########### install files ###############
install( FILES kdesu_export.h client.h process.h su.h ssh.h stub.h defaults.h 
         DESTINATION  ${INCLUDE_INSTALL_DIR}/kdesu COMPONENT Devel)




