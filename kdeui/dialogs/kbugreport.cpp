/* This file is part of the KDE project
   Copyright (C) 1999 David Faure <faure@kde.org>

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

#include "kbugreport.h"

#include <QtCore/QProcess>
#include <QtCore/QCoreApplication>
#include <QtGui/QPushButton>
#include <QtGui/QLayout>
#include <QtGui/QRadioButton>
#include <QtGui/QGroupBox>
#include <QtGui/QCloseEvent>

#include <kaboutdata.h>
#include <kcombobox.h>
#include <ktoolinvocation.h>
#include <kdebug.h>
#include <klineedit.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kcomponentdata.h>
#include <kurllabel.h>
#include <ktextedit.h>
#include <ktitlewidget.h>

#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/utsname.h>

#include "kdepackages.h"
#include "kdeversion.h"

#include <config-compiler.h>

class KBugReportPrivate {
public:
    KBugReportPrivate(KBugReport *q): q(q) {}

    void _k_slotConfigureEmail();
    void _k_slotSetFrom();
    void _k_appChanged(int);
    void _k_updateUrl();

    KBugReport *q;
    QProcess * m_process;
    const KAboutData * m_aboutData;

    KTextEdit * m_lineedit;
    KLineEdit * m_subject;
    QLabel * m_version;
    QString m_strVersion;
    QGroupBox * m_bgSeverity;

    KComboBox *appcombo;
    QString lastError;
    QString kde_version;
    QString appname;
    QString os;
    KUrl url;
    QList<QRadioButton*> severityButtons;
    int currentSeverity() {
        for (int i=0;i<severityButtons.count();i++)
            if (severityButtons[i]->isChecked()) return i;
        return -1;
    }
};

KBugReport::KBugReport( QWidget * _parent, bool modal, const KAboutData *aboutData )
  : KDialog( _parent ), d( new KBugReportPrivate(this) )
{
  setCaption( i18n("Submit Bug Report") );
  setButtons( Ok | Cancel );
  setModal(modal);

  // Use supplied aboutdata, otherwise the one from the active componentData
  // otherwise the KGlobal one. _activeInstance should neved be 0L in theory.
  d->m_aboutData = aboutData ? aboutData
      : (KGlobal::activeComponent().isValid() ? KGlobal::activeComponent().aboutData()
                                  : KGlobal::mainComponent().aboutData());
  d->m_process = nullptr;
  QWidget * parent = new QWidget(this);

  setButtonGuiItem( Cancel, KStandardGuiItem::close() );

  QLabel * tmpLabel;
  QVBoxLayout * lay = new QVBoxLayout( parent);

  KTitleWidget *title = new KTitleWidget( this );
  title->setText(i18n( "Submit Bug Report" ) );
  title->setPixmap( KIcon( "tools-report-bug" ).pixmap( 32 ) );
  lay->addWidget( title );

  QGridLayout *glay = new QGridLayout();
  lay->addLayout(glay);

  int row = 0;

  // Program name
  QString qwtstr = i18n( "The application for which you wish to submit a bug report - if incorrect, please use the Report Bug menu item of the correct application" );
  tmpLabel = new QLabel( i18n("Application: "), parent );
  glay->addWidget( tmpLabel, row, 0 );
  tmpLabel->setWhatsThis(qwtstr );
  d->appcombo = new KComboBox( false, parent );
  d->appcombo->setWhatsThis(qwtstr );
  QStringList packageList;
  for (int c = 0; packages[c]; ++c)
    packageList << QString::fromLatin1(packages[c]);
  d->appcombo->addItems(packageList);
  connect(d->appcombo, SIGNAL(activated(int)), SLOT(_k_appChanged(int)));
  d->appname = d->m_aboutData
                                    ? d->m_aboutData->productName()
                                    : qApp->applicationName() ;
  glay->addWidget( d->appcombo, row, 1 );
  int index = 0;
  for (; index < d->appcombo->count(); index++) {
      if (d->appcombo->itemText(index) == d->appname) {
          break;
      }
  }
  if (index == d->appcombo->count()) { // not present
      d->appcombo->addItem(d->appname);
  }
  d->appcombo->setCurrentIndex(index);

  tmpLabel->setWhatsThis(qwtstr );

  // Version
  qwtstr = i18n( "The version of this application - please make sure that no newer version is available before sending a bug report" );
  tmpLabel = new QLabel( i18n("Version:"), parent );
  glay->addWidget( tmpLabel, ++row, 0 );
  tmpLabel->setWhatsThis(qwtstr );
  if (d->m_aboutData)
      d->m_strVersion = d->m_aboutData->version();
  else
      d->m_strVersion = i18n("no version set (programmer error)");
  d->kde_version = QString::fromLatin1( KDE_VERSION_STRING );
  d->kde_version += ", " + QString::fromLatin1( KDE_DISTRIBUTION_TEXT );
  d->m_version = new QLabel( d->m_strVersion, parent );
  d->m_version->setTextInteractionFlags(Qt::TextBrowserInteraction);
  //glay->addWidget( d->m_version, row, 1 );
  glay->addWidget( d->m_version, row, 1, 1, 2 );
  d->m_version->setWhatsThis(qwtstr );

  tmpLabel = new QLabel(i18n("OS:"), parent);
  glay->addWidget( tmpLabel, ++row, 0 );

  struct utsname unameBuf;
  uname( &unameBuf );
  d->os = QString::fromLatin1( unameBuf.sysname ) +
          " (" + QString::fromLatin1( unameBuf.machine ) + ") "
          "release " + QString::fromLatin1( unameBuf.release );

  tmpLabel = new QLabel(d->os, parent);
  tmpLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  glay->addWidget( tmpLabel, row, 1, 1, 2 );

  tmpLabel = new QLabel(i18n("Compiler:"), parent);
  glay->addWidget( tmpLabel, ++row, 0 );
  tmpLabel = new QLabel(QString::fromLatin1(KDE_COMPILER_VERSION), parent);
  tmpLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  glay->addWidget( tmpLabel, row, 1, 1, 2 );

    // Point to the web form

    lay->addSpacing(10);
    QString text = i18n("<qt>To submit a bug report, click on the button below. This will open a web browser "
                        "window on <a href=\"http://bugs.kde.org\">http://bugs.kde.org</a> where you will find "
                        "a form to fill in. The information displayed above will be transferred to that server.</qt>");
    QLabel * label = new QLabel( text, parent);
    label->setOpenExternalLinks( true );
    label->setTextInteractionFlags( Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard );
    label->setWordWrap( true );
    lay->addWidget( label );
    lay->addSpacing(10);

    d->appcombo->setFocus();

    d->_k_updateUrl();

    setButtonText(Ok, i18n("&Launch Bug Report Wizard"));
    setButtonIcon(Ok, KIcon("tools-report-bug"));

  parent->setMinimumHeight( parent->sizeHint().height() + 20 ); // WORKAROUND: prevent "cropped" kcombobox
  setMainWidget(parent);
}

KBugReport::~KBugReport()
{
    delete d;
}

void KBugReportPrivate::_k_updateUrl()
{
    url = KUrl( "https://bugs.kde.org/enter_bug.cgi" );
    url.addQueryItem( "format", "guided" );  // use the guided form

    // the string format is product/component, where component is optional
    QStringList list = appcombo->currentText().split('/');
    url.addQueryItem( "product", list[0] );
    if (list.size() == 2) {
        url.addQueryItem( "component", list[1] );
    }

    url.addQueryItem( "version", m_strVersion );

    // TODO: guess and fill OS(sys_os) and Platform(rep_platform) fields
}

void KBugReportPrivate::_k_appChanged(int i)
{
    QString appName = appcombo->itemText(i);
    int index = appName.indexOf( '/' );
    if ( index > 0 )
        appName = appName.left( index );
    kDebug() << "appName " << appName;

    QString strDisplayVersion; //Version string to show in the UI
    if (appname == appName && m_aboutData) {
        m_strVersion = m_aboutData->version();
        strDisplayVersion = m_strVersion;
    } else {
        m_strVersion = QLatin1String("unknown"); //English string to put in the bug report
        strDisplayVersion = i18nc("unknown program name", "unknown");
    }

    m_version->setText(strDisplayVersion);
    _k_updateUrl();
}

void KBugReportPrivate::_k_slotConfigureEmail()
{
  if (m_process) return;
  m_process = new QProcess;
  QObject::connect( m_process, SIGNAL(finished(int,QProcess::ExitStatus)), q, SLOT(_k_slotSetFrom()) );
  m_process->start( QString::fromLatin1("kcmshell4"), QStringList() << QString::fromLatin1("kcm_useraccount") );
  if ( !m_process->waitForStarted() )
  {
    kDebug() << "Couldn't start kcmshell4..";
    delete m_process;
    m_process = 0;
    return;
  }
}

void KBugReportPrivate::_k_slotSetFrom()
{
  delete m_process;
  m_process = 0;

  // ### KDE4: why oh why is KEMailSettings in kio?
  KConfig emailConf( QString::fromLatin1("emaildefaults") );

  KConfigGroup cg(&emailConf, "Defaults");
  // find out the default profile
  QString profile = QString::fromLatin1("PROFILE_");
  profile += cg.readEntry( QString::fromLatin1("Profile"),
                           QString::fromLatin1("Default") );

  KConfigGroup profileGrp(&emailConf, profile );
  QString fromaddr = profileGrp.readEntry( "EmailAddress" );
  if (fromaddr.isEmpty()) {
     struct passwd *p;
     p = getpwuid(getuid());
     fromaddr = QString::fromLatin1(p->pw_name);
  } else {
     QString name = profileGrp.readEntry( "FullName" );
     if (!name.isEmpty())
        fromaddr = name + QString::fromLatin1(" <") + fromaddr + QString::fromLatin1(">");
  }
}

void KBugReport::accept()
{
    KToolInvocation::invokeBrowser( d->url.url() );
    KDialog::accept();
}

#include "kbugreport.moc"
