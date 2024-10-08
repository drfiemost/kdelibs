/*
   Copyright (c) 2000 Matthias Elter <elter@kde.org>
   Copyright (c) 2003 Daniel Molkentin <molkentin@kde.org>
   Copyright (c) 2003,2006 Matthias Kretz <kretz@kde.org>
   Copyright (c) 2004 Frans Englich <frans.englich@telia.com>
   Copyright (c) 2006 Tobias Koenig <tokoe@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.

*/

#include "kcmultidialog.h"
#include "kcmultidialog_p.h"

#include <QtCore/QStringList>
#include <QtCore/QProcess>

#include <kauthorized.h>
#include <kguiitem.h>
#include <khbox.h>
#include <kicon.h>
#include <klocale.h>
#include <kpagewidgetmodel.h>
#include <kpushbutton.h>
#include <ktoolinvocation.h>
#include <kdebug.h>
#include <kmessagebox.h>

#include "auth/kauthaction.h"

#include "kcolorscheme.h"

#include "kcmoduleloader.h"
#include "kcmoduleproxy.h"

bool KCMultiDialogPrivate::resolveChanges(KCModuleProxy *currentProxy)
{
    Q_Q(KCMultiDialog);
    if( !currentProxy || !currentProxy->changed() ) {
        return true;
    }

    // Let the user decide
    const int queryUser = KMessageBox::warningYesNoCancel(
        q,
        i18n("The settings of the current module have changed.\n"
             "Do you want to apply the changes or discard them?"),
        i18n("Apply Settings"),
        KStandardGuiItem::apply(),
        KStandardGuiItem::discard(),
        KStandardGuiItem::cancel());

    switch (queryUser) {
        case KMessageBox::Yes:
            return moduleSave(currentProxy);

        case KMessageBox::No:
            currentProxy->load();
            return true;

        case KMessageBox::Cancel:
            return false;

        default:
            Q_ASSERT(false);
            return false;
    }
}

void KCMultiDialogPrivate::_k_slotCurrentPageChanged( KPageWidgetItem *current, KPageWidgetItem *previous )
{
    Q_Q(KCMultiDialog);
    kDebug(710);

    q->blockSignals(true);
    q->setCurrentPage(previous);

    KCModuleProxy *previousModule = 0;
    for ( int i = 0; i < modules.count(); ++i ) {
        if ( modules[ i ].item == previous ) {
            previousModule = modules[ i ].kcm;
            break;
        }
    }

    if( resolveChanges(previousModule) ) {
        q->setCurrentPage(current);
    }
    q->blockSignals(false);

    // We need to get the state of the now active module
    _k_clientChanged();
}

void KCMultiDialogPrivate::_k_clientChanged()
{
    Q_Q(KCMultiDialog);
    kDebug(710);
    // Get the current module
    KCModuleProxy *activeModule = 0;
    for ( int i = 0; i < modules.count(); ++i ) {
        if ( modules[ i ].item == q->currentPage() ) {
            activeModule = modules[ i ].kcm;
            break;
        }
    }

    bool change = false;
    if (activeModule) {
        change = activeModule->changed();

        if (q->button(KDialog::Apply)) {
            q->disconnect(q, SIGNAL(applyClicked()), q, SLOT(slotApplyClicked()));
            q->disconnect(q->button(KDialog::Apply), SIGNAL(authorized(KAuth::Action*)), q, SLOT(slotApplyClicked()));
            q->button(KDialog::Apply)->setEnabled(change);
        }

        if (q->button(KDialog::Ok)) {
            q->disconnect(q, SIGNAL(okClicked()), q, SLOT(slotOkClicked()));
            q->disconnect(q->button(KDialog::Ok), SIGNAL(authorized(KAuth::Action*)), q, SLOT(slotOkClicked()));
        }

        if (activeModule->realModule()->needsAuthorization()) {
            if (q->button(KDialog::Apply)) {
                q->button(KDialog::Apply)->setAuthAction(activeModule->realModule()->authAction());
                activeModule->realModule()->authAction()->setParentWidget(activeModule->realModule());
                q->connect(q->button(KDialog::Apply), SIGNAL(authorized(KAuth::Action*)), SLOT(slotApplyClicked()));
            }

            if (q->button(KDialog::Ok)) {
                q->button(KDialog::Ok)->setAuthAction(activeModule->realModule()->authAction());
                activeModule->realModule()->authAction()->setParentWidget(activeModule->realModule());
                q->connect(q->button(KDialog::Ok), SIGNAL(authorized(KAuth::Action*)), SLOT(slotOkClicked()));
            }
        } else {
            if (q->button(KDialog::Apply)) {
                q->connect(q, SIGNAL(applyClicked()), SLOT(slotApplyClicked()));
                q->button(KDialog::Apply)->setAuthAction(0);
            }

            if (q->button(KDialog::Ok)) {
                q->connect(q, SIGNAL(okClicked()), SLOT(slotOkClicked()));
                q->button(KDialog::Ok)->setAuthAction(0);
            }
        }
    }

    if (q->button(KDialog::Reset)) {
        q->button(KDialog::Reset)->setEnabled(change);
    }

    if (q->button(KDialog::Apply)) {
        q->button(KDialog::Apply)->setEnabled(change);
    }

    if (activeModule) {
        q->enableButton(KDialog::Help,    activeModule->buttons() & KCModule::Help);
        q->enableButton(KDialog::Default, activeModule->buttons() & KCModule::Default);
    }
}

void KCMultiDialogPrivate::_k_updateHeader(bool use, const QString &message)
{
    Q_Q(KCMultiDialog);
    KPageWidgetItem *item = q->currentPage();
    KCModuleProxy *kcm = qobject_cast<KCModuleProxy*>(item->widget());

    if (use) {
        item->setHeader( "<b>"+kcm->moduleInfo().comment() + "</b><br><i>" +
                         message + "</i>" );
        item->setIcon( KIcon( kcm->moduleInfo().icon(), 0, QStringList() << "dialog-warning" ) );
    } else {
        item->setHeader( kcm->moduleInfo().comment() );
        item->setIcon( KIcon( kcm->moduleInfo().icon() ) );
    }
}

void KCMultiDialogPrivate::_k_dialogClosed()
{
  kDebug(710) ;

  /**
   * If we don't delete them, the DBUS registration stays, and trying to load the KCMs
   * in other situations will lead to "module already loaded in Foo," while to the user
   * doesn't appear so(the dialog is hidden)
   */
  for ( int i = 0; i < modules.count(); ++i )
    modules[ i ].kcm->deleteClient();
}

void KCMultiDialogPrivate::init()
{
    Q_Q(KCMultiDialog);
    q->setFaceType(KPageDialog::Auto);
    q->setCaption(i18n("Configure"));
    q->setButtons(KDialog::Help | KDialog::Default |KDialog::Cancel | KDialog::Apply | KDialog::Ok | KDialog::Reset);

    q->setModal(false);

    q->connect(q, SIGNAL(finished()), SLOT(_k_dialogClosed()));
    q->connect(q, SIGNAL(applyClicked()), SLOT(slotApplyClicked()));
    q->connect(q, SIGNAL(okClicked()), SLOT(slotOkClicked()));
    q->connect(q, SIGNAL(defaultClicked()), SLOT(slotDefaultClicked()));
    q->connect(q, SIGNAL(helpClicked()), SLOT(slotHelpClicked()));
    q->connect(q, SIGNAL(user1Clicked()), SLOT(slotUser1Clicked()));
    q->connect(q, SIGNAL(resetClicked()), SLOT(slotUser1Clicked()));

    q->connect(q, SIGNAL(currentPageChanged(KPageWidgetItem*,KPageWidgetItem*)),
            SLOT(_k_slotCurrentPageChanged(KPageWidgetItem*,KPageWidgetItem*)));

    q->setInitialSize(QSize(800, 550));
}

KCMultiDialog::KCMultiDialog( QWidget *parent )
    : KPageDialog(*new KCMultiDialogPrivate, NULL, parent)
{
    d_func()->init();
}

KCMultiDialog::KCMultiDialog(KPageWidget *pageWidget, QWidget *parent, Qt::WindowFlags flags)
    : KPageDialog(*new KCMultiDialogPrivate, pageWidget, parent, flags)
{
    d_func()->init();
}

KCMultiDialog::KCMultiDialog(KCMultiDialogPrivate &dd, KPageWidget *pageWidget, QWidget *parent, Qt::WindowFlags flags)
    : KPageDialog(dd, pageWidget, parent, flags)
{
    d_func()->init();
}

KCMultiDialog::~KCMultiDialog()
{
}

void KCMultiDialog::slotDefaultClicked()
{
    Q_D(KCMultiDialog);
    const KPageWidgetItem *item = currentPage();
    if ( !item )
        return;

    for ( int i = 0; i < d->modules.count(); ++i ) {
        if ( d->modules[ i ].item == item ) {
            d->modules[ i ].kcm->defaults();
            d->_k_clientChanged();
            return;
        }
    }
}

void KCMultiDialog::slotUser1Clicked()
{
    const KPageWidgetItem *item = currentPage();
    if ( !item )
        return;

    Q_D(KCMultiDialog);
    for ( int i = 0; i < d->modules.count(); ++i ) {
        if ( d->modules[ i ].item == item ) {
            d->modules[ i ].kcm->load();
            d->_k_clientChanged();
        return;
        }
    }
}

bool KCMultiDialogPrivate::moduleSave(KCModuleProxy *module)
{
    if( !module ) {
        return false;
    }

    module->save();
    return true;
}

void KCMultiDialogPrivate::apply()
{
    Q_Q(KCMultiDialog);
    QStringList updatedComponents;

    foreach (const CreatedModule &module, modules) {
        KCModuleProxy *proxy = module.kcm;

        if (proxy->changed()) {
            proxy->save();
            /**
                * Add name of the components the kcm belongs to the list
                * of updated components.
                */
            const QStringList componentNames = module.componentNames;
            foreach (const QString &componentName, module.componentNames) {
                if (!updatedComponents.contains(componentName)) {
                    updatedComponents.append(componentName);
                }
            }
        }
    }

    // Send the configCommitted signal for every updated component.
    foreach (const QString &name, updatedComponents) {
        emit q->configCommitted(name.toLatin1());
    }

    emit q->configCommitted();
}

void KCMultiDialog::slotApplyClicked()
{
  setButtonFocus( Apply );

    d_func()->apply();
}


void KCMultiDialog::slotOkClicked()
{
  setButtonFocus( Ok );

    d_func()->apply();
  accept();
}

void KCMultiDialog::slotHelpClicked()
{
    const KPageWidgetItem *item = currentPage();
    if ( !item )
        return;

    Q_D(KCMultiDialog);
    QString docPath;
    for ( int i = 0; i < d->modules.count(); ++i ) {
        if ( d->modules[ i ].item == item ) {
            docPath = d->modules[ i ].kcm->moduleInfo().docPath();
            break;
        }
    }

    KUrl docUrl( KUrl( "help:/" ), docPath );
    if ( docUrl.protocol() == "help" || docUrl.protocol() == "man" || docUrl.protocol() == "info" ) {
        QProcess::startDetached("khelpcenter", QStringList() << docUrl.url());
    } else {
        KToolInvocation::invokeBrowser( docUrl.url() );
    }
}


KPageWidgetItem* KCMultiDialog::addModule( const QString& path, const QStringList& args )
{
    QString complete = path;

    if ( !path.endsWith( QLatin1String(".desktop") ) )
        complete += ".desktop";

    KService::Ptr service = KService::serviceByStorageId( complete );

    return addModule( KCModuleInfo( service ), 0, args );
}

KPageWidgetItem* KCMultiDialog::addModule( const KCModuleInfo& moduleInfo,
                                           KPageWidgetItem *parentItem, const QStringList& args )
{
    if ( !moduleInfo.service() )
        return 0;

    //KAuthorized::authorizeControlModule( moduleInfo.service()->menuId() ) is
    //checked in noDisplay already
    if ( moduleInfo.service()->noDisplay() )
        return 0;

    KCModuleProxy *kcm = new KCModuleProxy(moduleInfo, 0, args);

    kDebug(710) << moduleInfo.moduleName();
    KPageWidgetItem *item = new KPageWidgetItem(kcm, moduleInfo.moduleName());

    if (kcm->useRootOnlyMessage()) {
        item->setHeader( "<b>"+moduleInfo.comment() + "</b><br><i>" + kcm->rootOnlyMessage() + "</i>" );
        item->setIcon( KIcon( moduleInfo.icon(), 0, QStringList() << "dialog-warning" ) );
    } else {
        item->setHeader( moduleInfo.comment() );
        item->setIcon( KIcon( moduleInfo.icon() ) );
    }
    item->setProperty("_k_weight", moduleInfo.weight());

    bool updateCurrentPage = false;
    const KPageWidgetModel *model = qobject_cast<const KPageWidgetModel *>(pageWidget()->model());
    Q_ASSERT(model);
    if (parentItem) {
        const QModelIndex parentIndex = model->index(parentItem);
        const int siblingCount = model->rowCount(parentIndex);
        int row = 0;
        for (; row < siblingCount; ++row) {
            KPageWidgetItem *siblingItem = model->item(parentIndex.child(row, 0));
            if (siblingItem->property("_k_weight").toInt() > moduleInfo.weight()) {
                // the item we found is heavier than the new module
                kDebug(710) << "adding KCM " << item->name() << " before " << siblingItem->name();
                insertPage(siblingItem, item);
                break;
            }
        }
        if (row >= siblingCount) {
            // the new module is either the first or the heaviest item
            kDebug(710) << "adding KCM " << item->name() << " with parent " << parentItem->name();
            addSubPage(parentItem, item);
        }
    } else {
        const int siblingCount = model->rowCount();
        int row = 0;
        for (; row < siblingCount; ++row) {
            KPageWidgetItem *siblingItem = model->item(model->index(row, 0));
            if (siblingItem->property("_k_weight").toInt() > moduleInfo.weight()) {
                // the item we found is heavier than the new module
                kDebug(710) << "adding KCM " << item->name() << " before " << siblingItem->name();
                insertPage(siblingItem, item);
                if ( siblingItem == currentPage() )
                    updateCurrentPage = true;

                break;
            }
        }
        if (row == siblingCount) {
            // the new module is either the first or the heaviest item
            kDebug(710) << "adding KCM " << item->name() << " at the top level";
            addPage(item);
        }
    }

    connect(kcm, SIGNAL(changed(bool)), this, SLOT(_k_clientChanged()));
    connect(kcm->realModule(), SIGNAL(rootOnlyMessageChanged(bool,QString)), this, SLOT(_k_updateHeader(bool,QString)));

    Q_D(KCMultiDialog);
  KCMultiDialogPrivate::CreatedModule cm;
  cm.kcm = kcm;
  cm.item = item;
  cm.componentNames = moduleInfo.service()->property( "X-KDE-ParentComponents" ).toStringList();
  d->modules.append( cm );

  if ( d->modules.count() == 1 || updateCurrentPage )
  {
    setCurrentPage( item );
    d->_k_clientChanged();
  }
  return item;
}

void KCMultiDialog::clear()
{
    Q_D(KCMultiDialog);
  kDebug( 710 ) ;

  for ( int i = 0; i < d->modules.count(); ++i ) {
    removePage( d->modules[ i ].item );
    delete d->modules[ i ].kcm;
  }

  d->modules.clear();

    d->_k_clientChanged();
}

void KCMultiDialog::setButtons(ButtonCodes buttonMask)
{
    KPageDialog::setButtons(buttonMask);

    // Set Auto-Default mode ( KDE Bug #211187 )
    if (buttonMask & KDialog::Ok) {
        button(KDialog::Ok)->setAutoDefault(true);
    }
    if (buttonMask & KDialog::Apply) {
        button(KDialog::Apply)->setAutoDefault(true);
    }
    if (buttonMask & KDialog::Default) {
        button(KDialog::Default)->setAutoDefault(true);
    }
    if (buttonMask & KDialog::Reset) {
        button(KDialog::Reset)->setAutoDefault(true);
    }
    if (buttonMask & KDialog::Cancel) {
        button(KDialog::Cancel)->setAutoDefault(true);
    }
    if (buttonMask & KDialog::Help) {
        button(KDialog::Help)->setAutoDefault(true);
    }

    // Old Reset Button
    enableButton(KDialog::User1, false);
    enableButton(KDialog::Reset, false);
    enableButton(KDialog::Apply, false);
}


#include "kcmultidialog.moc"
