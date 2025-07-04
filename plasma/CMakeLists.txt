# strange conversion error in pushbutton.cpp
kde4_no_enable_final(plasma)

if(KDE_PLATFORM_FEATURE_BINARY_COMPATIBLE_FEATURE_REDUCTION)
    set(PLASMA_NO_SOLID TRUE)
    set(PLASMA_NO_KIO TRUE)
    set(PLASMA_NO_KUTILS TRUE)
    set(PLASMA_NO_GLOBAL_SHORTCUTS TRUE)
endif(KDE_PLATFORM_FEATURE_BINARY_COMPATIBLE_FEATURE_REDUCTION)

set(PLASMA_NO_PACKAGEKIT TRUE)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${KDE4_KDECORE_INCLUDES}
                    ${KDE4_KDEUI_INCLUDES}
                    ${CMAKE_SOURCE_DIR}/experimental/libkdeclarative
                    ${CMAKE_BINARY_DIR}/experimental/libkdeclarative
                    ${CMAKE_SOURCE_DIR}/threadweaver
                    ${CMAKE_SOURCE_DIR}/plasma/extenders
                    ${CMAKE_SOURCE_DIR}/plasma/remote
                    ${CMAKE_SOURCE_DIR}/plasma/private/qtjolie-branch/qtjolie
                    ${CMAKE_SOURCE_DIR}/plasma/private/qtjolie-branch
                    ${CMAKE_SOURCE_DIR}/plasma/private/qtjolie-branch/includes
                    )

if(NOT PLASMA_NO_KIO)
   include_directories(${KDE4_KIO_INCLUDES})
   set(PLASMA_EXTRA_LIBS ${PLASMA_EXTRA_LIBS} ${KDE4_KIO_LIBS})
endif(NOT PLASMA_NO_KIO)

if(NOT PLASMA_NO_SOLID)
    include_directories(${CMAKE_BINARY_DIR}/solid/)
    include_directories(${CMAKE_SOURCE_DIR}/solid/)
    set(PLASMA_EXTRA_LIBS ${PLASMA_EXTRA_LIBS} ${KDE4_SOLID_LIBS})
endif(NOT PLASMA_NO_SOLID)

if(NOT PLASMA_NO_PACKAGEKIT)
    add_definitions(-DPLASMA_ENABLE_PACKAGEKIT_SUPPORT=1)
    set(PLASMA_EXTRA_LIBS ${PLASMA_EXTRA_LIBS} Qt4::QtDBus)
endif(NOT PLASMA_NO_PACKAGEKIT)

if (NOT PLASMA_NO_KUTILS)
    include_directories(${CMAKE_SOURCE_DIR}/kutils)
    set(PLASMA_EXTRA_LIBS ${PLASMA_EXTRA_LIBS} ${KDE4_KUTILS_LIBS})
endif(NOT PLASMA_NO_KUTILS)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-plasma.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-plasma.h)

add_subdirectory(tests)
add_definitions(-DKDE_DEFAULT_DEBUG_AREA=1209)

########### next target ###############

set(plasmagik_SRCS
    packagemetadata.cpp
    packagestructure.cpp
    package.cpp
    )

#to make the Temporary solution to not link to libkdeclarative work, disable the exports of libkdeclarative  
add_definitions(-DKDECLARATIVE_EXPORT=)

set(plasma_LIB_SRCS
    ${plasmagik_SRCS}
    abstractrunner.cpp
    abstracttoolbox.cpp
    animator.cpp
    animations/animation.cpp
    animations/animationscriptengine.cpp
    animations/bindings/animationgroup.cpp
    animations/bindings/easingcurve.cpp
    animations/easinganimation.cpp
    animations/fade.cpp
    animations/grow.cpp
    animations/javascriptanimation.cpp
    animations/pendulumcurve.cpp
    animations/pixmaptransition.cpp
    animations/pulser.cpp
    animations/rotation.cpp
    animations/rotationstacked.cpp
    animations/slide.cpp
    animations/stackedlayout.cpp
    animations/geometry.cpp
    animations/water.cpp
    animations/widgetsnapshot.cpp
    animations/zoom.cpp
    applet.cpp
    configloader.cpp
    containment.cpp
    containmentactions.cpp
    containmentactionspluginsconfig.cpp
    context.cpp
    corona.cpp
    datacontainer.cpp
    dataengine.cpp
    dataenginemanager.cpp
    delegate.cpp
    dialog.cpp
    abstractdialogmanager.cpp
    extenders/extender.cpp
    extenders/extendergroup.cpp
    extenders/extenderitem.cpp
    pluginloader.cpp
    paintutils.cpp
    framesvg.cpp
    plasma.cpp
    popupapplet.cpp
    private/applethandle.cpp
    private/associatedapplicationmanager.cpp
    private/componentinstaller.cpp
    private/datacontainer_p.cpp
    private/dataenginebindings.cpp
    private/dataengineconsumer.cpp
    private/dataengineservice.cpp
    private/denyallauthorization.cpp
    private/dialogshadows.cpp
    private/effectwatcher.cpp
    private/extenderapplet.cpp
    private/extenderitemmimedata.cpp
    private/focusindicator.cpp
    private/getsource.cpp
    private/nativetabbar.cpp
    private/packages.cpp
    private/pinpairingauthorization.cpp
    private/pinpairingdialog.cpp
    private/plasmoidservice.cpp
    private/remotedataengine.cpp
    private/remoteservice.cpp
    private/remoteservicejob.cpp
    private/runnerjobs.cpp
    private/serviceprovider.cpp
    private/storage.cpp
    private/storagethread.cpp
    private/style.cpp
    private/themedwidgetinterface.cpp
    private/trustedonlyauthorization.cpp
    private/tooltip.cpp
    private/wallpaperrenderthread.cpp
    private/windowpreview.cpp
    private/kineticscroll.cpp
    private/declarative/declarativenetworkaccessmanagerfactory.cpp
    private/effects/halopainter.cpp
    private/effects/ripple.cpp
    querymatch.cpp
    remote/accessmanager.cpp
    remote/accessappletjob.cpp
    remote/authorizationinterface.cpp
    remote/authorizationmanager.cpp
    remote/authorizationrule.cpp
    remote/clientpinrequest.cpp
    remote/credentials.cpp
    remote/serviceaccessjob.cpp
    runnercontext.cpp
    runnermanager.cpp
    runnersyntax.cpp
    scripting/appletscript.cpp
    scripting/dataenginescript.cpp
    scripting/runnerscript.cpp
    scripting/wallpaperscript.cpp
    scripting/scriptengine.cpp
    service.cpp
    servicejob.cpp
    svg.cpp
    theme.cpp
    tooltipcontent.cpp
    tooltipmanager.cpp
    version.cpp
    view.cpp
    wallpaper.cpp
    windoweffects.cpp
    widgets/checkbox.cpp
    widgets/combobox.cpp
    widgets/declarativewidget.cpp
    widgets/flashinglabel.cpp
    widgets/frame.cpp
    widgets/groupbox.cpp
    widgets/iconwidget.cpp
    widgets/itembackground.cpp
    widgets/label.cpp
    widgets/lineedit.cpp
    widgets/meter.cpp
    widgets/pushbutton.cpp
    widgets/radiobutton.cpp
    widgets/scrollbar.cpp
    widgets/signalplotter.cpp
    widgets/slider.cpp
    widgets/spinbox.cpp
    widgets/toolbutton.cpp
    widgets/busywidget.cpp
    widgets/scrollwidget.cpp
    widgets/separator.cpp
    widgets/svgwidget.cpp
    widgets/tabbar.cpp
    widgets/textbrowser.cpp
    widgets/treeview.cpp
    widgets/textedit.cpp

    #Temporary QtJolie branch
    private/qtjolie-branch/qtjolie/abstractadaptor.cpp
    private/qtjolie-branch/qtjolie/client.cpp
    private/qtjolie-branch/qtjolie/clientthread.cpp
    private/qtjolie-branch/qtjolie/value.cpp
    private/qtjolie-branch/qtjolie/fault.cpp
    private/qtjolie-branch/qtjolie/message.cpp
    private/qtjolie-branch/qtjolie/metaservice.cpp
    private/qtjolie-branch/qtjolie/pendingcall.cpp
    private/qtjolie-branch/qtjolie/pendingcallwatcher.cpp
    private/qtjolie-branch/qtjolie/pendingreply.cpp
    private/qtjolie-branch/qtjolie/server.cpp
    private/qtjolie-branch/qtjolie/serverthread.cpp

    #Temporary solution to not link to libkdeclarative
    ${CMAKE_SOURCE_DIR}/experimental/libkdeclarative/kdeclarative.cpp
    ${CMAKE_SOURCE_DIR}/experimental/libkdeclarative/private/engineaccess.cpp
    ${CMAKE_SOURCE_DIR}/experimental/libkdeclarative/private/kiconprovider.cpp
    ${CMAKE_SOURCE_DIR}/experimental/libkdeclarative/bindings/qscriptnonguibookkeeping.cpp
    ${CMAKE_SOURCE_DIR}/experimental/libkdeclarative/bindings/i18n.cpp
    ${CMAKE_SOURCE_DIR}/experimental/libkdeclarative/bindings/icon.cpp
    ${CMAKE_SOURCE_DIR}/experimental/libkdeclarative/bindings/url.cpp
)

kde4_add_kcfg_files(plasma_LIB_SRCS data/kconfigxt/libplasma-theme-global.kcfgc)

kde4_add_ui_files(plasma_LIB_SRCS
                  private/pinpairing.ui
                  private/publish.ui)

if (QT_QTOPENGL_FOUND)
    message(STATUS "Adding support for OpenGL applets to libplasma")
    set(plasma_LIB_SRCS
        ${plasma_LIB_SRCS}
        glapplet.cpp)
endif(QT_QTOPENGL_FOUND)

if (PHONON_FOUND)
    message(STATUS "Adding support for Phonon to libplasma")
    include_directories(${KDE4_PHONON_INCLUDES})
    set(plasma_LIB_SRCS
        ${plasma_LIB_SRCS}
        widgets/videowidget.cpp)
endif(PHONON_FOUND)

kde4_add_library(plasma ${LIBRARY_TYPE} ${plasma_LIB_SRCS})

target_link_libraries(plasma LINK_PRIVATE Qt4::QtUiTools Qt4::QtScript
                            Qt4::QtNetwork Qt4::QtXml Qt4::QtSql Qt4::QtDeclarative
                            ${KDE4_KDEUI_LIBS} ${KDE4_KDNSSD_LIBS} ${KDE4_THREADWEAVER_LIBS} ${PLASMA_EXTRA_LIBS})

if(X11_FOUND)
   target_link_libraries(plasma LINK_PRIVATE ${X11_LIBRARIES})
endif(X11_FOUND)

if(PHONON_FOUND)
   target_link_libraries(plasma LINK_PRIVATE ${KDE4_PHONON_LIBS})
endif(PHONON_FOUND)

if(DL_LIBRARY)
    target_link_libraries(plasma LINK_PRIVATE ${DL_LIBRARY})
endif(DL_LIBRARY)

if(QT_QTOPENGL_FOUND)
    target_link_libraries(plasma LINK_PRIVATE Qt4::QtOpenGL)
endif(QT_QTOPENGL_FOUND)

target_link_libraries(plasma LINK_PUBLIC kdeui kdecore Qt4::QtGui)

#do NOT use GENERIC versioning -- the plasma team will take care of versioning
set_target_properties(plasma PROPERTIES
                             VERSION 3.0.0
                             SOVERSION 3
                      )

install(TARGETS plasma EXPORT kdelibsLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})


########### install files ###############

set(plasmagik_HEADERS
    packagemetadata.h
    packagestructure.h
    package.h
    )

install(FILES ${plasmagik_HEADERS} DESTINATION ${INCLUDE_INSTALL_DIR}/plasma/ COMPONENT Devel)

set(plasma_LIB_INCLUDES
    abstractdialogmanager.h
    abstractrunner.h
    abstracttoolbox.h
    animations/animation.h
    animator.h
    applet.h
    configloader.h
    containment.h
    containmentactions.h
    containmentactionspluginsconfig.h
    context.h
    corona.h
    datacontainer.h
    dataengine.h
    dataenginemanager.h
    delegate.h
    dialog.h
    extenders/extender.h
    extenders/extendergroup.h
    extenders/extenderitem.h
    pluginloader.h
    paintutils.h
    windoweffects.h
    framesvg.h
    plasma.h
    plasma_export.h
    popupapplet.h
    querymatch.h
    remote/accessappletjob.h
    remote/accessmanager.h
    remote/authorizationmanager.h
    remote/authorizationrule.h
    remote/authorizationinterface.h
    remote/clientpinrequest.h
    remote/credentials.h
    remote/serviceaccessjob.h
    runnercontext.h
    runnermanager.h
    runnersyntax.h
    service.h
    servicejob.h
    svg.h
    theme.h
    tooltipcontent.h
    tooltipmanager.h
    version.h
    view.h
    wallpaper.h)

if(QT_QTOPENGL_FOUND)
    set(plasma_LIB_INCLUDES
        ${plasma_LIB_INCLUDES}
        glapplet.h)
endif(QT_QTOPENGL_FOUND)

install(FILES
        ${plasma_LIB_INCLUDES}
        DESTINATION ${INCLUDE_INSTALL_DIR}/plasma COMPONENT Devel)

install(FILES
    widgets/checkbox.h
    widgets/combobox.h
    widgets/declarativewidget.h
    widgets/flashinglabel.h
    widgets/frame.h
    widgets/groupbox.h
    widgets/iconwidget.h
    widgets/itembackground.h
    widgets/label.h
    widgets/lineedit.h
    widgets/meter.h
    widgets/pushbutton.h
    widgets/toolbutton.h
    widgets/radiobutton.h
    widgets/scrollbar.h
    widgets/signalplotter.h
    widgets/slider.h
    widgets/spinbox.h
    widgets/busywidget.h
    widgets/separator.h
    widgets/svgwidget.h
    widgets/scrollwidget.h
    widgets/tabbar.h
    widgets/textbrowser.h
    widgets/treeview.h
    widgets/textedit.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/plasma/widgets COMPONENT Devel)

install(FILES
    scripting/appletscript.h
    scripting/dataenginescript.h
    scripting/runnerscript.h
    scripting/wallpaperscript.h
    scripting/scriptengine.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/plasma/scripting COMPONENT Devel)

if(PHONON_FOUND)
    install (FILES
        widgets/videowidget.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/plasma/widgets COMPONENT Devel)
endif(PHONON_FOUND)

install(FILES
    animations/animation.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/plasma/animations COMPONENT Devel)


install(FILES
   data/servicetypes/plasma-animator.desktop
   data/servicetypes/plasma-applet.desktop
   data/servicetypes/plasma-applet-popupapplet.desktop
   data/servicetypes/plasma-containment.desktop
   data/servicetypes/plasma-containmentactions.desktop
   data/servicetypes/plasma-dataengine.desktop
   data/servicetypes/plasma-packagestructure.desktop
   data/servicetypes/plasma-runner.desktop
   data/servicetypes/plasma-scriptengine.desktop
   data/servicetypes/plasma-service.desktop
   data/servicetypes/plasma-toolbox.desktop
   data/servicetypes/plasma-wallpaper.desktop
   DESTINATION ${SERVICETYPES_INSTALL_DIR})

install(FILES
   data/services/plasma-applet-extenderapplet.desktop
   data/services/plasma.protocol
   DESTINATION ${SERVICES_INSTALL_DIR})

install(FILES data/operations/dataengineservice.operations DESTINATION ${DATA_INSTALL_DIR}/plasma/services)
install(FILES data/operations/plasmoidservice.operations DESTINATION ${DATA_INSTALL_DIR}/plasma/services)
install(FILES data/operations/storage.operations DESTINATION ${DATA_INSTALL_DIR}/plasma/services)

