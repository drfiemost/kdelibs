include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${QT_INCLUDE_DIR} ${QT_QTCORE_INCLUDE_DIR})

if(WIN32)
   include_directories(${KDEWIN_INCLUDES})
endif(WIN32)


########### next target ###############

set(ThreadWeaver_LIB_SRCS
    WeaverInterface.cpp
    ThreadWeaver.cpp
    WeaverImpl.cpp
    DebuggingAids.cpp
    Thread.cpp
    Job.cpp
    State.cpp
    StateImplementation.cpp
    InConstructionState.cpp
    WorkingHardState.cpp
    SuspendingState.cpp
    SuspendedState.cpp
    ShuttingDownState.cpp
    DestructedState.cpp
    WeaverObserver.cpp
    JobCollection.cpp
    JobSequence.cpp
    DependencyPolicy.cpp
    ResourceRestrictionPolicy.cpp
)


kde4_add_library(threadweaver ${LIBRARY_TYPE} ${ThreadWeaver_LIB_SRCS})
target_link_libraries(threadweaver LINK_PRIVATE Qt4::QtCore)
target_link_libraries(threadweaver LINK_PUBLIC Qt4::QtCore)

set_target_properties(threadweaver PROPERTIES
    VERSION ${GENERIC_LIB_VERSION}
    SOVERSION ${GENERIC_LIB_SOVERSION}
)
install(TARGETS threadweaver EXPORT kdelibsLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
    WeaverInterface.h
    ThreadWeaver.h
    DebuggingAids.h
    Thread.h
    Job.h
    State.h
    WeaverObserver.h
    JobCollection.h
    JobSequence.h
    DependencyPolicy.h
    ResourceRestrictionPolicy.h
    QueuePolicy.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/threadweaver COMPONENT Devel
)
