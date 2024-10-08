/* -*- indent-tabs-mode: t; tab-width: 4; c-basic-offset:4 -*-

   This file is part of the KDE libraries
   Copyright (C) 2000 David Smith <dsmith@algonet.se>
   Copyright (C) 2004 Scott Wheeler <wheeler@kde.org>

   This class was inspired by a previous KUrlCompletion by
   Henner Zeller <zeller@think.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.   If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kurlcompletion.h"

#include <config.h>

#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QMutableStringListIterator>
#include <QtCore/QRegExp>
#include <QtCore/QTimer>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QThread>
#include <QtGui/QActionEvent>

#include <kauthorized.h>
#include <kdebug.h>
#include <kurl.h>
#include <kio/job.h>
#include <kprotocolmanager.h>
#include <kconfig.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <kde_file.h>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>
#include <sys/param.h>
#include <kconfiggroup.h>

#ifdef Q_WS_WIN
#include <kkernel_win.h>
#endif

static bool expandTilde(QString&);
static bool expandEnv(QString&);

static QString unescape(const QString& text);

// Permission mask for files that are executable by
// user, group or other
#define MODE_EXE (S_IXUSR | S_IXGRP | S_IXOTH)

// Constants for types of completion
enum ComplType {CTNone = 0, CTEnv, CTUser, CTMan, CTExe, CTFile, CTUrl, CTInfo};

class CompletionThread;

///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
// KUrlCompletionPrivate
//
class KUrlCompletionPrivate
{
public:
    KUrlCompletionPrivate(KUrlCompletion* parent)
        : q(parent),
          url_auto_completion(true),
          userListThread(0),
          dirListThread(0) {
    }

    ~KUrlCompletionPrivate();

    void _k_slotEntries(KIO::Job*, const KIO::UDSEntryList&);
    void _k_slotIOFinished(KJob*);

    class MyURL;
    bool userCompletion(const MyURL& url, QString* match);
    bool envCompletion(const MyURL& url, QString* match);
    bool exeCompletion(const MyURL& url, QString* match);
    bool fileCompletion(const MyURL& url, QString* match);
    bool urlCompletion(const MyURL& url, QString* match);

    bool isAutoCompletion();

    // List the next dir in m_dirs
    QString listDirectories(const QStringList&,
                             const QString&,
                             bool only_exe = false,
                             bool only_dir = false,
                             bool no_hidden = false,
                             bool stat_files = true);

    void listUrls(const QList<KUrl> &urls,
                   const QString& filter = QString(),
                   bool only_exe = false,
                   bool no_hidden = false);

    void addMatches(const QStringList&);
    QString finished();

    void init();

    void setListedUrl(int compl_type /* enum ComplType */,
                       const QString& dir = QString(),
                       const QString& filter = QString(),
                       bool no_hidden = false);

    bool isListedUrl(int compl_type /* enum ComplType */,
                      const QString& dir = QString(),
                      const QString& filter = QString(),
                      bool no_hidden = false);

    KUrlCompletion* q;
    QList<KUrl> list_urls;

    bool onlyLocalProto;

    // urlCompletion() in Auto/Popup mode?
    bool url_auto_completion;

    // Append '/' to directories in Popup mode?
    // Doing that stat's all files and is slower
    bool popup_append_slash;

    // Keep track of currently listed files to avoid reading them again
    QString last_path_listed;
    QString last_file_listed;
    QString last_prepend;
    int last_compl_type;
    int last_no_hidden;

    QString cwd; // "current directory" = base dir for completion

    KUrlCompletion::Mode mode; // ExeCompletion, FileCompletion, DirCompletion
    bool replace_env;
    bool replace_home;
    bool complete_url; // if true completing a URL (i.e. 'prepend' is a URL), otherwise a path

    KIO::ListJob* list_job; // kio job to list directories

    QString prepend; // text to prepend to listed items
    QString compl_text; // text to pass on to KCompletion

    // Filters for files read with  kio
    bool list_urls_only_exe; // true = only list executables
    bool list_urls_no_hidden;
    QString list_urls_filter; // filter for listed files

    CompletionThread* userListThread;
    CompletionThread* dirListThread;
};

/**
 * A custom event type that is used to return a list of completion
 * matches from an asynchronous lookup.
 */

class CompletionMatchEvent : public QEvent
{
public:
    CompletionMatchEvent(CompletionThread* thread) :
        QEvent(uniqueType()),
        m_completionThread(thread)
    {}

    CompletionThread* completionThread() const {
        return m_completionThread;
    }
    static Type uniqueType() {
        return Type(User + 61080);
    }

private:
    CompletionThread* m_completionThread;
};

class CompletionThread : public QThread
{
protected:
    CompletionThread(KUrlCompletionPrivate* receiver) :
        QThread(),
        m_prepend(receiver->prepend),
        m_complete_url(receiver->complete_url),
        m_receiver(receiver),
        m_terminationRequested(false)
    {}

public:
    void requestTermination() {
        m_terminationRequested = true;
    }
    QStringList matches() const {
        return m_matches;
    }

protected:
    void addMatch(const QString& match) {
        m_matches.append(match);
    }
    bool terminationRequested() const {
        return m_terminationRequested;
    }
    void done() {
        if (!m_terminationRequested)
            qApp->postEvent(m_receiver->q, new CompletionMatchEvent(this));
        else
            deleteLater();
    }

    const QString m_prepend;
    const bool m_complete_url; // if true completing a URL (i.e. 'm_prepend' is a URL), otherwise a path

private:
    KUrlCompletionPrivate* m_receiver;
    QStringList m_matches;
    bool m_terminationRequested;
};

/**
 * A simple thread that fetches a list of tilde-completions and returns this
 * to the caller via a CompletionMatchEvent.
 */

class UserListThread : public CompletionThread
{
public:
    UserListThread(KUrlCompletionPrivate* receiver) :
        CompletionThread(receiver)
    {}

protected:
    virtual void run() {
        static const QChar tilde = '~';

        // we don't need to handle prepend here, right? ~user is always at pos 0
        assert(m_prepend.isEmpty());
        struct passwd* pw;
        while ((pw = ::getpwent()) && !terminationRequested())
            addMatch(tilde + QString::fromLocal8Bit(pw->pw_name));

        ::endpwent();

        addMatch(QString(tilde));

        done();
    }
};

class DirectoryListThread : public CompletionThread
{
public:
    DirectoryListThread(KUrlCompletionPrivate* receiver,
                         const QStringList& dirList,
                         const QString& filter,
                         bool onlyExe,
                         bool onlyDir,
                         bool noHidden,
                         bool appendSlashToDir) :
        CompletionThread(receiver),
        m_dirList(dirList),
        m_filter(filter),
        m_onlyExe(onlyExe),
        m_onlyDir(onlyDir),
        m_noHidden(noHidden),
        m_appendSlashToDir(appendSlashToDir)
    {}

    virtual void run();

private:
    QStringList m_dirList;
    QString m_filter;
    bool m_onlyExe;
    bool m_onlyDir;
    bool m_noHidden;
    bool m_appendSlashToDir;
};

void DirectoryListThread::run()
{
    // Thread safety notes:
    //
    // There very possibly may be thread safety issues here, but I've done a check
    // of all of the things that would seem to be problematic.  Here are a few
    // things that I have checked to be safe here (some used indirectly):
    //
    // QDir::currentPath(), QDir::setCurrent(), QFile::decodeName(), QFile::encodeName()
    // QString::fromLocal8Bit(), QString::toLocal8Bit(), QTextCodec::codecForLocale()
    //
    // Also see (for POSIX functions):
    // http://www.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_09.html

    // kDebug() << "Entered DirectoryListThread::run(), m_filter=" << m_filter << ", m_onlyExe=" << m_onlyExe << ", m_onlyDir=" << m_onlyDir << ", m_appendSlashToDir=" << m_appendSlashToDir << ", m_dirList.size()=" << m_dirList.size();

    QStringList::ConstIterator end = m_dirList.constEnd();
    for (QStringList::ConstIterator it = m_dirList.constBegin();
            it != end && !terminationRequested();
            ++it) {
        // kDebug() << "Scanning directory" << *it;

        // A trick from KIO that helps performance by a little bit:
        // chdir to the directory so we won't have to deal with full paths
        // with stat()

        QString path = QDir::currentPath();
        QDir::setCurrent(*it);

        QDir::Filters iterator_filter = (m_noHidden ? QDir::Filter(0) : QDir::Hidden) | QDir::Readable | QDir::NoDotAndDotDot;

        if (m_onlyExe)
            iterator_filter |= (QDir::Dirs | QDir::Files | QDir::Executable);
        else if (m_onlyDir)
            iterator_filter |= QDir::Dirs;
        else
            iterator_filter |= (QDir::Dirs | QDir::Files);

        QDirIterator current_dir_iterator(*it, iterator_filter);

        while (current_dir_iterator.hasNext()) {
            current_dir_iterator.next();

            QFileInfo file_info = current_dir_iterator.fileInfo();
            const QString file_name = file_info.fileName();

            //kDebug() << "Found" << file_name;

            if (m_filter.isEmpty() || file_name.startsWith(m_filter)) {

                QString toAppend = file_name;
                // Add '/' to directories
                if (m_appendSlashToDir && file_info.isDir())
                    toAppend.append(QLatin1Char('/'));

                if (m_complete_url) {
                    KUrl url(m_prepend);
                    url.addPath(toAppend);
                    addMatch(url.prettyUrl());
                } else {
                    addMatch(m_prepend + toAppend);
                }
            }
        }

        // chdir to the original directory
        QDir::setCurrent(path);
    }

    done();
}

KUrlCompletionPrivate::~KUrlCompletionPrivate()
{
    if (userListThread)
        userListThread->requestTermination();
    if (dirListThread)
        dirListThread->requestTermination();
}

///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
// MyURL - wrapper for KUrl with some different functionality
//

class KUrlCompletionPrivate::MyURL
{
public:
    MyURL(const QString& url, const QString& cwd);
    MyURL(const MyURL& url);
    ~MyURL();

    KUrl kurl() const {
        return m_kurl;
    }

    QString protocol() const {
        return m_kurl.protocol();
    }
    // The directory with a trailing '/'
    QString dir() const {
        return m_kurl.directory(KUrl::AppendTrailingSlash | KUrl::ObeyTrailingSlash);
    }
    QString file() const {
        return m_kurl.fileName(KUrl::ObeyTrailingSlash);
    }

    // The initial, unparsed, url, as a string.
    QString url() const {
        return m_url;
    }

    // Is the initial string a URL, or just a path (whether absolute or relative)
    bool isURL() const {
        return m_isURL;
    }

    void filter(bool replace_user_dir, bool replace_env);

private:
    void init(const QString& url, const QString& cwd);

    KUrl m_kurl;
    QString m_url;
    bool m_isURL;
};

KUrlCompletionPrivate::MyURL::MyURL(const QString& _url, const QString& cwd)
{
    init(_url, cwd);
}

KUrlCompletionPrivate::MyURL::MyURL(const MyURL& _url)
    : m_kurl(_url.m_kurl)
{
    m_url = _url.m_url;
    m_isURL = _url.m_isURL;
}

void KUrlCompletionPrivate::MyURL::init(const QString& _url, const QString& cwd)
{
    // Save the original text
    m_url = _url;

    // Non-const copy
    QString url_copy = _url;

    // Special shortcuts for "man:" and "info:"
    if (url_copy.startsWith(QLatin1Char('#'))) {
        if (url_copy.length() > 1 && url_copy.at(1) == QLatin1Char('#'))
            url_copy.replace(0, 2, QLatin1String("info:"));
        else
            url_copy.replace(0, 1, QLatin1String("man:"));
    }

    // Look for a protocol in 'url'
    QRegExp protocol_regex = QRegExp("^(?![A-Za-z]:)[^/\\s\\\\]*:");

    // Assume "file:" or whatever is given by 'cwd' if there is
    // no protocol.  (KUrl does this only for absolute paths)
    if (protocol_regex.indexIn(url_copy) == 0) {
        m_kurl = KUrl(url_copy);
        m_isURL = true;
    } else { // relative path or ~ or $something
        m_isURL = false;
        if (!QDir::isRelativePath(url_copy) ||
                url_copy.startsWith(QLatin1Char('~')) ||
                url_copy.startsWith(QLatin1Char('$'))) {
            m_kurl = KUrl();
            m_kurl.setPath(url_copy);
        } else {
            if (cwd.isEmpty()) {
                m_kurl = KUrl(url_copy);
            } else {
                m_kurl = KUrl(cwd);
                m_kurl.addPath(url_copy);
            }
        }
    }
}

KUrlCompletionPrivate::MyURL::~MyURL()
{
}

void KUrlCompletionPrivate::MyURL::filter(bool replace_user_dir, bool replace_env)
{
    QString d = dir() + file();
    if (replace_user_dir) expandTilde(d);
    if (replace_env) expandEnv(d);
    m_kurl.setPath(d);
}

///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
// KUrlCompletion
//

KUrlCompletion::KUrlCompletion() : KCompletion(), d(new KUrlCompletionPrivate(this))
{
    d->init();
}


KUrlCompletion::KUrlCompletion(Mode _mode)
    : KCompletion(),
      d(new KUrlCompletionPrivate(this))
{
    d->init();
    setMode(_mode);
}

KUrlCompletion::~KUrlCompletion()
{
    stop();
    delete d;
}


void KUrlCompletionPrivate::init()
{
    cwd = QDir::homePath();

    replace_home = true;
    replace_env = true;
    last_no_hidden = false;
    last_compl_type = 0;
    list_job = 0L;
    mode = KUrlCompletion::FileCompletion;

    // Read settings
    KConfigGroup cg(KGlobal::config(), "URLCompletion");

    url_auto_completion = cg.readEntry("alwaysAutoComplete", true);
    popup_append_slash = cg.readEntry("popupAppendSlash", true);
    onlyLocalProto = cg.readEntry("LocalProtocolsOnly", false);

    q->setIgnoreCase(true);
}

void KUrlCompletion::setDir(const QString& _dir)
{
    d->cwd = _dir;
}

QString KUrlCompletion::dir() const
{
    return d->cwd;
}

KUrlCompletion::Mode KUrlCompletion::mode() const
{
    return d->mode;
}

void KUrlCompletion::setMode(Mode _mode)
{
    d->mode = _mode;
}

bool KUrlCompletion::replaceEnv() const
{
    return d->replace_env;
}

void KUrlCompletion::setReplaceEnv(bool replace)
{
    d->replace_env = replace;
}

bool KUrlCompletion::replaceHome() const
{
    return d->replace_home;
}

void KUrlCompletion::setReplaceHome(bool replace)
{
    d->replace_home = replace;
}

/*
 * makeCompletion()
 *
 * Entry point for file name completion
 */
QString KUrlCompletion::makeCompletion(const QString& text)
{
    //kDebug() << text << "d->cwd=" << d->cwd;

    KUrlCompletionPrivate::MyURL url(text, d->cwd);

    d->compl_text = text;

    // Set d->prepend to the original URL, with the filename [and ref/query] stripped.
    // This is what gets prepended to the directory-listing matches.
    int toRemove = url.file().length() - url.kurl().query().length();
    if (url.kurl().hasRef())
        toRemove += url.kurl().ref().length() + 1;
    d->prepend = text.left(text.length() - toRemove);
    d->complete_url = url.isURL();

    QString aMatch;

    // Environment variables
    //
    if (d->replace_env && d->envCompletion(url, &aMatch))
        return aMatch;

    // User directories
    //
    if (d->replace_home && d->userCompletion(url, &aMatch))
        return aMatch;

    // Replace user directories and variables
    url.filter(d->replace_home, d->replace_env);

    //kDebug() << "Filtered: proto=" << url.protocol()
    //          << ", dir=" << url.dir()
    //          << ", file=" << url.file()
    //          << ", kurl url=" << *url.kurl();

    if (d->mode == ExeCompletion) {
        // Executables
        //
        if (d->exeCompletion(url, &aMatch))
            return aMatch;

        // KRun can run "man:" and "info:" etc. so why not treat them
        // as executables...

        if (d->urlCompletion(url, &aMatch))
            return aMatch;
    } else {
        // Local files, directories
        //
        if (d->fileCompletion(url, &aMatch))
            return aMatch;

        // All other...
        //
        if (d->urlCompletion(url, &aMatch))
            return aMatch;
    }

    d->setListedUrl(CTNone);
    stop();

    return QString();
}

/*
 * finished
 *
 * Go on and call KCompletion.
 * Called when all matches have been added
 */
QString KUrlCompletionPrivate::finished()
{
    if (last_compl_type == CTInfo)
        return q->KCompletion::makeCompletion(compl_text.toLower());
    else
        return q->KCompletion::makeCompletion(compl_text);
}

/*
 * isRunning
 *
 * Return true if either a KIO job or the DirLister
 * is running
 */
bool KUrlCompletion::isRunning() const
{
    return d->list_job || (d->dirListThread && !d->dirListThread->isFinished());
}

/*
 * stop
 *
 * Stop and delete a running KIO job or the DirLister
 */
void KUrlCompletion::stop()
{
    if (d->list_job) {
        d->list_job->kill();
        d->list_job = 0L;
    }

    if (d->dirListThread) {
        d->dirListThread->requestTermination();
        d->dirListThread = 0;
    }
}

/*
 * Keep track of the last listed directory
 */
void KUrlCompletionPrivate::setListedUrl(int complType,
        const QString& directory,
        const QString& filter,
        bool no_hidden)
{
    last_compl_type = complType;
    last_path_listed = directory;
    last_file_listed = filter;
    last_no_hidden = (int) no_hidden;
    last_prepend = prepend;
}

bool KUrlCompletionPrivate::isListedUrl(int complType,
        const QString& directory,
        const QString& filter,
        bool no_hidden)
{
    return  last_compl_type == complType
            && (last_path_listed == directory
                || (directory.isEmpty() && last_path_listed.isEmpty()))
            && (filter.startsWith (last_file_listed)
                || (filter.isEmpty() && last_file_listed.isEmpty()))
            && last_no_hidden == (int) no_hidden
            && last_prepend == prepend; // e.g. relative path vs absolute
}

/*
 * isAutoCompletion
 *
 * Returns true if completion mode is Auto or Popup
 */
bool KUrlCompletionPrivate::isAutoCompletion()
{
    return q->completionMode() == KGlobalSettings::CompletionAuto
           || q->completionMode() == KGlobalSettings::CompletionPopup
           || q->completionMode() == KGlobalSettings::CompletionMan
           || q->completionMode() == KGlobalSettings::CompletionPopupAuto;
}
//////////////////////////////////////////////////
//////////////////////////////////////////////////
// User directories
//

bool KUrlCompletionPrivate::userCompletion(const KUrlCompletionPrivate::MyURL& url, QString* pMatch)
{
    if (url.protocol() != QLatin1String("file")
            || !url.dir().isEmpty()
            || !url.file().startsWith(QLatin1Char('~')))
        return false;

    if (!isListedUrl(CTUser)) {
        q->stop();
        q->clear();

        if (!userListThread) {
            userListThread = new UserListThread(this);
            userListThread->start();

            // If the thread finishes quickly make sure that the results
            // are added to the first matching case.

            userListThread->wait(200);
            const QStringList l = userListThread->matches();
            addMatches(l);
        }
    }
    *pMatch = finished();
    return true;
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// Environment variables
//

#ifndef Q_OS_WIN
extern char** environ; // Array of environment variables
#endif

bool KUrlCompletionPrivate::envCompletion(const KUrlCompletionPrivate::MyURL& url, QString* pMatch)
{
    if (url.file().isEmpty() || url.file().at(0) != QLatin1Char('$'))
        return false;

    if (!isListedUrl(CTEnv)) {
        q->stop();
        q->clear();

        char** env = environ;

        QString dollar = QLatin1String("$");

        QStringList l;

        while (*env) {
            QString s = QString::fromLocal8Bit(*env);

            int pos = s.indexOf(QLatin1Char('='));

            if (pos == -1)
                pos = s.length();

            if (pos > 0)
                l.append(prepend + dollar + s.left(pos));

            env++;
        }

        addMatches(l);
    }

    setListedUrl(CTEnv);

    *pMatch = finished();
    return true;
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
// Executables
//

bool KUrlCompletionPrivate::exeCompletion(const KUrlCompletionPrivate::MyURL& url, QString* pMatch)
{
    if (url.protocol() != QLatin1String("file"))
        return false;

    QString directory = unescape(url.dir());  // remove escapes

    // Find directories to search for completions, either
    //
    // 1. complete path given in url
    // 2. current directory (d->cwd)
    // 3. $PATH
    // 4. no directory at all

    QStringList dirList;

    if (!url.file().isEmpty()) {
        // $PATH
        dirList = QString::fromLocal8Bit(qgetenv("PATH")).split(
                      KPATH_SEPARATOR, QString::SkipEmptyParts);

        QStringList::Iterator it = dirList.begin();

        for (; it != dirList.end(); ++it)
            it->append(QLatin1Char('/'));
    } else if (!QDir::isRelativePath(directory)) {
        // complete path in url
        dirList.append(directory);
    } else if (!directory.isEmpty() && !cwd.isEmpty()) {
        // current directory
        dirList.append(cwd + QLatin1Char('/') + directory);
    }

    // No hidden files unless the user types "."
    bool no_hidden_files = url.file().isEmpty() || url.file().at(0) != QLatin1Char('.');

    // List files if needed
    //
    if (!isListedUrl(CTExe, directory, url.file(), no_hidden_files)) {
        q->stop();
        q->clear();

        setListedUrl(CTExe, directory, url.file(), no_hidden_files);

        *pMatch = listDirectories(dirList, url.file(), true, false, no_hidden_files);
    } else if (!q->isRunning()) {
        *pMatch = finished();
    } else {
        if (dirListThread)
            setListedUrl(CTExe, directory, url.file(), no_hidden_files);
        pMatch->clear();
    }

    return true;
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
// Local files
//

bool KUrlCompletionPrivate::fileCompletion(const KUrlCompletionPrivate::MyURL& url, QString* pMatch)
{
    if (url.protocol() != QLatin1String("file"))
        return false;

    QString directory = unescape(url.dir());

    if (url.url().length() && url.url().at(0) == QLatin1Char('.')) {
        if (url.url().length() == 1) {
            *pMatch = (q->completionMode() == KGlobalSettings::CompletionMan) ?
                      QLatin1String(".") :
                      QLatin1String("..");
            return true;
        } else if (url.url().length() == 2 && url.url().at(1) == QLatin1Char('.')) {
            *pMatch = QLatin1String("..");
            return true;
        }
    }

    //kDebug() << "fileCompletion" << url << "dir=" << dir;

    // Find directories to search for completions, either
    //
    // 1. complete path given in url
    // 2. current directory (d->cwd)
    // 3. no directory at all

    QStringList dirList;

    if (!QDir::isRelativePath(directory)) {
        // complete path in url
        dirList.append(directory);
    } else if (!cwd.isEmpty()) {
        // current directory
        QString dirToAdd = cwd;
        if (!directory.isEmpty()) {
            if (!cwd.endsWith('/'))
                dirToAdd.append(QLatin1Char('/'));
            dirToAdd.append(directory);
        }
        dirList.append(dirToAdd);
    }

    // No hidden files unless the user types "."
    bool no_hidden_files = !url.file().startsWith(QLatin1Char('.'));

    // List files if needed
    //
    if (!isListedUrl(CTFile, directory, QString(), no_hidden_files)) {
        q->stop();
        q->clear();

        setListedUrl(CTFile, directory, QString(), no_hidden_files);

        // Append '/' to directories in Popup mode?
        bool append_slash = (popup_append_slash
                             && (q->completionMode() == KGlobalSettings::CompletionPopup ||
                                 q->completionMode() == KGlobalSettings::CompletionPopupAuto));

        bool only_dir = (mode == KUrlCompletion::DirCompletion);

        *pMatch = listDirectories(dirList, QString(), false, only_dir, no_hidden_files,
                                   append_slash);
    } else if (!q->isRunning()) {
        *pMatch = finished();
    } else {
        pMatch->clear();
    }

    return true;
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
// URLs not handled elsewhere...
//

static bool isLocalProtocol(const QString& protocol)
{
    return (KProtocolInfo::protocolClass(protocol) == QLatin1String(":local"));
}

bool KUrlCompletionPrivate::urlCompletion(const KUrlCompletionPrivate::MyURL& url, QString* pMatch)
{
    //kDebug() << *url.kurl();
    if (onlyLocalProto && isLocalProtocol(url.protocol()))
        return false;

    // Use d->cwd as base url in case url is not absolute
    KUrl url_dir = url.kurl();
    if (url_dir.isRelative() && !cwd.isEmpty()) {
        const KUrl url_cwd (cwd);
        // Create an URL with the directory to be listed
        url_dir = KUrl(url_cwd,  url_dir.url());
    }

    // url is malformed
    if (!url_dir.isValid())
        return false;

    // non local urls
    if (!isLocalProtocol(url.protocol())) {
        // url does not specify host
        if (url_dir.host().isEmpty())
            return false;

        // url does not specify a valid directory
        if (url_dir.directory(KUrl::AppendTrailingSlash | KUrl::ObeyTrailingSlash).isEmpty())
            return false;

        // automatic completion is disabled
        if (isAutoCompletion() && !url_auto_completion)
            return false;
    }

    // url handler doesn't support listing
    if (!KProtocolManager::supportsListing(url_dir))
        return false;

    url_dir.setFileName(QString()); // not really nesseccary, but clear the filename anyway...

    // Remove escapes
    QString directory = unescape(url_dir.directory(KUrl::AppendTrailingSlash | KUrl::ObeyTrailingSlash));

    url_dir.setPath(directory);

    // List files if needed
    //
    if (!isListedUrl(CTUrl, url_dir.prettyUrl(), url.file())) {
        q->stop();
        q->clear();

        setListedUrl(CTUrl, url_dir.prettyUrl(), QString());

        QList<KUrl> url_list;
        url_list.append(url_dir);

        listUrls(url_list, QString(), false);

        pMatch->clear();
    } else if (!q->isRunning()) {
        *pMatch = finished();
    } else {
        pMatch->clear();
    }

    return true;
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
// Directory and URL listing
//

/*
 * addMatches
 *
 * Called to add matches to KCompletion
 */
void KUrlCompletionPrivate::addMatches(const QStringList& matchList)
{
    q->insertItems(matchList);
}

/*
 * listDirectories
 *
 * List files starting with 'filter' in the given directories,
 * either using DirLister or listURLs()
 *
 * In either case, addMatches() is called with the listed
 * files, and eventually finished() when the listing is done
 *
 * Returns the match if available, or QString() if
 * DirLister timed out or using kio
 */
QString KUrlCompletionPrivate::listDirectories(
    const QStringList& dirList,
    const QString& filter,
    bool only_exe,
    bool only_dir,
    bool no_hidden,
    bool append_slash_to_dir)
{
    assert(!q->isRunning());

    if (qgetenv("KURLCOMPLETION_LOCAL_KIO").isEmpty()) {

        //kDebug() << "Listing (listDirectories):" << dirList << "filter=" << filter << "without KIO";

        // Don't use KIO

        if (dirListThread)
            dirListThread->requestTermination();

        QStringList dirs;

        QStringList::ConstIterator end = dirList.constEnd();
        for (QStringList::ConstIterator it = dirList.constBegin();
                it != end;
                ++it) {
            KUrl url;
            url.setPath(*it);
            if (KAuthorized::authorizeUrlAction(QLatin1String("list"), KUrl(), url))
                dirs.append(*it);
        }

        dirListThread = new DirectoryListThread(this, dirs, filter, only_exe, only_dir,
                                                no_hidden, append_slash_to_dir);
        dirListThread->start();
        dirListThread->wait(200);
        addMatches(dirListThread->matches());

        return finished();
    }

    // Use KIO
    //kDebug() << "Listing (listDirectories):" << dirList << "with KIO";

    QList<KUrl> url_list;

    QStringList::ConstIterator it = dirList.constBegin();
    QStringList::ConstIterator end = dirList.constEnd();

    for (; it != end; ++it) {
        url_list.append(KUrl(*it));
    }

    listUrls(url_list, filter, only_exe, no_hidden);
    // Will call addMatches() and finished()

    return QString();
}

/*
 * listURLs
 *
 * Use KIO to list the given urls
 *
 * addMatches() is called with the listed files
 * finished() is called when the listing is done
 */
void KUrlCompletionPrivate::listUrls(
    const QList<KUrl> &urls,
    const QString& filter,
    bool only_exe,
    bool no_hidden)
{
    assert(list_urls.isEmpty());
    assert(list_job == 0L);

    list_urls = urls;
    list_urls_filter = filter;
    list_urls_only_exe = only_exe;
    list_urls_no_hidden = no_hidden;

    //kDebug() << "Listing URLs:" << *urls[0] << ",...";

    // Start it off by calling _k_slotIOFinished
    //
    // This will start a new list job as long as there
    // are urls in d->list_urls
    //
    _k_slotIOFinished(0);
}

/*
 * _k_slotEntries
 *
 * Receive files listed by KIO and call addMatches()
 */
void KUrlCompletionPrivate::_k_slotEntries(KIO::Job*, const KIO::UDSEntryList& entries)
{
    QStringList matchList;

    KIO::UDSEntryList::ConstIterator it = entries.constBegin();
    const KIO::UDSEntryList::ConstIterator end = entries.constEnd();

    QString filter = list_urls_filter;

    int filter_len = filter.length();

    // Iterate over all files
    //
    for (; it != end; ++it) {
        const KIO::UDSEntry& entry = *it;
        const QString url = entry.stringValue(KIO::UDSEntry::UDS_URL);

        QString entry_name;
        if (!url.isEmpty()) {
            // kDebug() << "url:" << url;
            entry_name = KUrl(url).fileName();
        } else {
            entry_name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
        }

        // kDebug() << "name:" << name;

        if ((!entry_name.isEmpty() && entry_name.at(0) == QLatin1Char('.')) &&
                (list_urls_no_hidden ||
                 entry_name.length() == 1 ||
                 (entry_name.length() == 2 && entry_name.at(1) == QLatin1Char('.'))))
            continue;

        const bool isDir = entry.isDir();

        if (mode == KUrlCompletion::DirCompletion && !isDir)
            continue;

        if (filter_len == 0 || entry_name.left(filter_len) == filter) {

            QString toAppend = entry_name;

            if (isDir)
                toAppend.append(QLatin1Char('/'));

            if (!list_urls_only_exe ||
                    (entry.numberValue(KIO::UDSEntry::UDS_ACCESS) & MODE_EXE)  // true if executable
               ) {
                if (complete_url) {
                    KUrl url(prepend);
                    url.addPath(toAppend);
                    matchList.append(url.prettyUrl());
                } else {
                    matchList.append(prepend + toAppend);
                }
            }
        }
    }

    addMatches(matchList);
}

/*
 * _k_slotIOFinished
 *
 * Called when a KIO job is finished.
 *
 * Start a new list job if there are still urls in
 * list_urls, otherwise call finished()
 */
void KUrlCompletionPrivate::_k_slotIOFinished(KJob* job)
{
    assert(job == list_job); Q_UNUSED(job)

    if (list_urls.isEmpty()) {

        list_job = 0L;

        finished(); // will call KCompletion::makeCompletion()

    } else {

        KUrl kurl(list_urls.takeFirst());

//      list_urls.removeAll( kurl );

//      kDebug() << "Start KIO::listDir" << kurl;

        list_job = KIO::listDir(kurl, KIO::HideProgressInfo);
        list_job->addMetaData("no-auth-prompt", "true");

        assert(list_job);

        q->connect(list_job,
                    SIGNAL(result(KJob*)),
                    SLOT(_k_slotIOFinished(KJob*)));

        q->connect(list_job,
                    SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)),
                    SLOT(_k_slotEntries(KIO::Job*,KIO::UDSEntryList)));
    }
}

///////////////////////////////////////////////////
///////////////////////////////////////////////////

/*
 * postProcessMatch, postProcessMatches
 *
 * Called by KCompletion before emitting match() and matches()
 *
 * Append '/' to directories for file completion. This is
 * done here to avoid stat()'ing a lot of files
 */
void KUrlCompletion::postProcessMatch(QString* pMatch) const
{
//  kDebug() << *pMatch;

    if (!pMatch->isEmpty()) {

        // Add '/' to directories in file completion mode
        // unless it has already been done
        if (d->last_compl_type == CTFile
                && pMatch->at(pMatch->length() - 1) != QLatin1Char('/')) {
            QString copy;

            if (pMatch->startsWith(QLatin1String("file:")))
                copy = KUrl(*pMatch).toLocalFile();
            else
                copy = *pMatch;

            expandTilde(copy);
            expandEnv(copy);
#ifdef Q_WS_WIN
            DWORD dwAttr = GetFileAttributesW((LPCWSTR) copy.utf16());
            if (dwAttr == INVALID_FILE_ATTRIBUTES) {
                kDebug() << "Could not get file attribs ( "
                         << GetLastError()
                         << " ) for "
                         << copy;
            } else if ((dwAttr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
                pMatch->append(QLatin1Char('/'));
#else
            if (QDir::isRelativePath(copy))
                copy.prepend(d->cwd + QLatin1Char('/'));

//          kDebug() << "stat'ing" << copy;

            KDE_struct_stat sbuff;

            QByteArray file = QFile::encodeName(copy);

            if (KDE_stat(file.data(), &sbuff) == 0) {
                if (S_ISDIR(sbuff.st_mode))
                    pMatch->append(QLatin1Char('/'));
            } else {
                kDebug() << "Could not stat file" << copy;
            }
#endif
        }
    }
}

void KUrlCompletion::postProcessMatches(QStringList* /*matches*/) const
{
    // Maybe '/' should be added to directories here as in
    // postProcessMatch() but it would slow things down
    // when there are a lot of matches...
}

void KUrlCompletion::postProcessMatches(KCompletionMatches* /*matches*/) const
{
    // Maybe '/' should be added to directories here as in
    // postProcessMatch() but it would slow things down
    // when there are a lot of matches...
}

void KUrlCompletion::customEvent(QEvent* e)
{
    if (e->type() == CompletionMatchEvent::uniqueType()) {

        CompletionMatchEvent* matchEvent = static_cast<CompletionMatchEvent*>(e);

        matchEvent->completionThread()->wait();

        if (!d->isListedUrl(CTUser)) {
            stop();
            clear();
            d->addMatches(matchEvent->completionThread()->matches());
        } else {
            d->setListedUrl(CTUser);
        }

        if (d->userListThread == matchEvent->completionThread())
            d->userListThread = 0;

        if (d->dirListThread == matchEvent->completionThread())
            d->dirListThread = 0;

        delete matchEvent->completionThread();
    }
}

// static
QString KUrlCompletion::replacedPath(const QString& text, bool replaceHome, bool replaceEnv)
{
    if (text.isEmpty())
        return text;

    KUrlCompletionPrivate::MyURL url(text, QString());  // no need to replace something of our current cwd
    if (!url.kurl().isLocalFile())
        return text;

    url.filter(replaceHome, replaceEnv);
    return url.dir() + url.file();
}


QString KUrlCompletion::replacedPath(const QString& text) const
{
    return replacedPath(text, d->replace_home, d->replace_env);
}

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
// Static functions

/*
 * expandEnv
 *
 * Expand environment variables in text. Escaped '$' are ignored.
 * Return true if expansion was made.
 */
static bool expandEnv(QString& text)
{
    // Find all environment variables beginning with '$'
    //
    int pos = 0;

    bool expanded = false;

    while ((pos = text.indexOf(QLatin1Char('$'), pos)) != -1) {

        // Skip escaped '$'
        //
        if (pos > 0 && text.at(pos - 1) == QLatin1Char('\\')) {
            pos++;
        }
        // Variable found => expand
        //
        else {
            // Find the end of the variable = next '/' or ' '
            //
            int pos2 = text.indexOf(QLatin1Char(' '), pos + 1);
            int pos_tmp = text.indexOf(QLatin1Char('/'), pos + 1);

            if (pos2 == -1 || (pos_tmp != -1 && pos_tmp < pos2))
                pos2 = pos_tmp;

            if (pos2 == -1)
                pos2 = text.length();

            // Replace if the variable is terminated by '/' or ' '
            // and defined
            //
            if (pos2 >= 0) {
                int len = pos2 - pos;
                QString key = text.mid(pos + 1, len - 1);
                QString value =
                    QString::fromLocal8Bit(qgetenv(key.toLocal8Bit()));

                if (!value.isEmpty()) {
                    expanded = true;
                    text.replace(pos, len, value);
                    pos = pos + value.length();
                } else {
                    pos = pos2;
                }
            }
        }
    }

    return expanded;
}

/*
 * expandTilde
 *
 * Replace "~user" with the users home directory
 * Return true if expansion was made.
 */
static bool expandTilde(QString& text)
{
    if (text.isEmpty() || (text.at(0) != QLatin1Char('~')))
        return false;

    bool expanded = false;

    // Find the end of the user name = next '/' or ' '
    //
    int pos2 = text.indexOf(QLatin1Char(' '), 1);
    int pos_tmp = text.indexOf(QLatin1Char('/'), 1);

    if (pos2 == -1 || (pos_tmp != -1 && pos_tmp < pos2))
        pos2 = pos_tmp;

    if (pos2 == -1)
        pos2 = text.length();

    // Replace ~user if the user name is terminated by '/' or ' '
    //
    if (pos2 >= 0) {

        QString user = text.mid(1, pos2 - 1);
        QString dir;

        // A single ~ is replaced with $HOME
        //
        if (user.isEmpty()) {
            dir = QDir::homePath();
        }
        // ~user is replaced with the dir from passwd
        //
        else {
            struct passwd* pw = ::getpwnam(user.toLocal8Bit());

            if (pw)
                dir = QFile::decodeName(pw->pw_dir);

            ::endpwent();
        }

        if (!dir.isEmpty()) {
            expanded = true;
            text.replace(0, pos2, dir);
        }
    }

    return expanded;
}

/*
 * unescape
 *
 * Remove escapes and return the result in a new string
 *
 */
static QString unescape(const QString& text)
{
    QString result;

    for (int pos = 0; pos < text.length(); pos++)
        if (text.at(pos) != QLatin1Char('\\'))
            result.insert(result.length(), text.at(pos));

    return result;
}

#include "kurlcompletion.moc"
