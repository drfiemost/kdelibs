/*  This file is part of the KDE project
    Copyright (C) 2003,2007 Matthias Kretz <kretz@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

*/

#ifndef KDECORE_KPLUGININFO_H
#define KDECORE_KPLUGININFO_H

#include <QtCore/QExplicitlySharedDataPointer>
#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QStringList>

#include <kconfiggroup.h>
#include <kservice.h>
#include <kaboutdata.h>
#include <QtCore/QList>

class KPluginInfoPrivate;

/**
 * Information about a plugin.
 *
 * This holds all the information about a plugin there is. It's used for the
 * user to decide whether he wants to use this plugin or not.
 *
 * @author Matthias Kretz <kretz@kde.org>
 */
class KDECORE_EXPORT KPluginInfo
{
    public:
        typedef QList<KPluginInfo> List;

        /**
         * Read plugin info from @p filename.
         *
         * The file should be of the following form:
         * \verbatim
           [Desktop Entry]
           Encoding=UTF-8
           Icon=mypluginicon
           Type=Service
           ServiceTypes=KPluginInfo

           Name=User Visible Name
           Comment=Description of what the plugin does

           X-KDE-PluginInfo-Author=Author's Name
           X-KDE-PluginInfo-Email=author@foo.bar
           X-KDE-PluginInfo-Name=internalname
           X-KDE-PluginInfo-Version=1.1
           X-KDE-PluginInfo-Website=http://www.plugin.org/
           X-KDE-PluginInfo-Category=playlist
           X-KDE-PluginInfo-Depends=plugin1,plugin3
           X-KDE-PluginInfo-License=GPL
           X-KDE-PluginInfo-EnabledByDefault=true
           \endverbatim
         * The Name and Comment fields must always be present.
         *
         * The "X-KDE-PluginInfo" keys you may add further entries which
         * will be available using property(). The Website,Category,Require
         * keys are optional.
         * For EnabledByDefault look at isPluginEnabledByDefault.
         *
         * @param filename  The filename of the .desktop file.
         * @param resource  If filename is relative, you need to specify a resource type
         * (e.g. "service", "apps"... KStandardDirs). Otherwise,
         * resource isn't used.
         */
        explicit KPluginInfo( const QString & filename, const char* resource = 0 );

        /**
         * Read plugin info from a KService object.
         *
         * The .desktop file should look like this:
         * \verbatim
           [Desktop Entry]
           Encoding=UTF-8
           Icon=mypluginicon
           Type=Service
           ServiceTypes=KPluginInfo

           X-KDE-PluginInfo-Author=Author's Name
           X-KDE-PluginInfo-Email=author@foo.bar
           X-KDE-PluginInfo-Name=internalname
           X-KDE-PluginInfo-Version=1.1
           X-KDE-PluginInfo-Website=http://www.plugin.org/
           X-KDE-PluginInfo-Category=playlist
           X-KDE-PluginInfo-Depends=plugin1,plugin3
           X-KDE-PluginInfo-License=GPL
           X-KDE-PluginInfo-EnabledByDefault=true

           Name=User Visible Name
           Comment=Description of what the plugin does
           \endverbatim
         * In the first three entries the Icon entry is optional.
         */
        explicit KPluginInfo( const KService::Ptr service );

        /**
         * Creates an invalid plugin.
         *
         * \see isValid
         */
        KPluginInfo();

        ~KPluginInfo();

        /**
         * @return A list of KPluginInfo objects constructed from a list of
         * KService objects. If you get a trader offer of the plugins you want
         * to use you can just pass them to this function.
         *
         * @param services The list of services to construct the list of KPluginInfo objects from
         * @param config The config group where to save/load whether the plugin is enabled/disabled
         */
        static KPluginInfo::List fromServices(const KService::List &services, const KConfigGroup &config = KConfigGroup());

        /**
         * @return A list of KPluginInfo objects constructed from a list of
         * filenames. If you make a lookup using, for example,
         * KStandardDirs::findAllResources() you pass the list of files to this
         * function.
         *
         * @param files The list of files to construct the list of KPluginInfo objects from
         * @param config The config group where to save/load whether the plugin is enabled/disabled
         */
        static KPluginInfo::List fromFiles(const QStringList &files, const KConfigGroup &config = KConfigGroup());

        /**
         * @return A list of KPluginInfo objects for the KParts plugins of a
         * component. You only need the name of the component not a pointer to the
         * KComponentData object.
         *
         * @param componentName Use the component name to look up all KParts plugins for it.
         * @param config The config group where to save/load whether the plugin is enabled/disabled
         */
        static KPluginInfo::List fromKPartsInstanceName(const QString &componentName, const KConfigGroup &config = KConfigGroup());

        /**
         * @return Whether the plugin should be hidden.
         */
        bool isHidden() const;

        /**
         * Set whether the plugin is currently loaded.
         *
         * @see isPluginEnabled()
         * @see save()
         */
        void setPluginEnabled(bool enabled);

        /**
         * @return Whether the plugin is currently loaded.
         *
         * @see setPluginEnabled()
         * @see load()
         */
        bool isPluginEnabled() const;

        /**
         * @return The default value whether the plugin is enabled or not.
         * Defaults to the value set in the desktop file, or if that isn't set
         * to false.
         */
        bool isPluginEnabledByDefault() const;

        /**
         * @return The value associated to the @p key. You can use it if you
         *         want to read custom values. To do this you need to define
         *         your own servicetype and add it to the ServiceTypes keys.
         */
        QVariant property( const QString & key ) const;

        /**
         * @return The user visible name of the plugin.
         */
        QString name() const;

        /**
         * @return A comment describing the plugin.
         */
        QString comment() const;

        /**
         * @return The iconname for this plugin
         */
        QString icon() const;

        /**
         * @return The file containing the information about the plugin.
         */
        QString entryPath() const;

        /**
         * @return The author of this plugin.
         */
        QString author() const;

        /**
         * @return The email address of the author.
         */
        QString email() const;

        /**
         * @return The category of this plugin (e.g. playlist/skin).
         */
        QString category() const;

        /**
         * @return The internal name of the plugin (for KParts Plugins this is
         * the same name as set in the .rc file).
         */
        QString pluginName() const;

        /**
         * @return The version of the plugin.
         */
        QString version() const;

        /**
         * @return The website of the plugin/author.
         */
        QString website() const;


        /**
         * @return The license keyword of this plugin.
         */
        QString license() const;

        /**
         * @return The full license object, according to the license keyword.
         *         It can be used to present friendlier and more detailed
         *         license info to the user, when the license is one of the
         *         widespread within KDE. For other licenses, the license
         *         object will state not very useful, "custom license" info
         *         (this can be identified by KAboutLicense::key() returning
         *          KAboutData::License_Custom).
         *
         * @see KAboutLicense::byKeyword()
         * @since 4.1
         */
        KAboutLicense fullLicense() const;

        /**
         * @return A list of plugins required for this plugin to be enabled. Use
         *         the pluginName in this list.
         */
        QStringList dependencies() const;

        /**
         * @return The KService object for this plugin. You might need it if you
         *         want to read custom values. To do this you need to define
         *         your own servicetype and add it to the ServiceTypes keys.
         *         Then you can use the KService::property() method to read your
         *         keys.
         *
         * @see property()
         */
        KService::Ptr service() const;

        /**
         * @return A list of Service pointers if the plugin installs one or more
         *         KCModule
         */
        QList<KService::Ptr> kcmServices() const;

        /**
         * Set the KConfigGroup to use for load()ing and save()ing the
         * configuration. This will be overridden by the KConfigGroup passed to
         * save() or load() (if one is passed).
         */
        void setConfig(const KConfigGroup &config);

        /**
         * @return If the KPluginInfo object has a KConfig object set return
         * it, else returns an invalid KConfigGroup.
         */
        KConfigGroup config() const;

        /**
         * Save state of the plugin - enabled or not.
         *
         * @param config    The KConfigGroup holding the information whether
         *                  plugin is enabled.
         */
        void save(KConfigGroup config = KConfigGroup());

        /**
         * Load the state of the plugin - enabled or not.
         *
         * @param config    The KConfigGroup holding the information whether
         *                  plugin is enabled.
         */
        void load(const KConfigGroup &config = KConfigGroup());

        /**
         * Restore defaults (enabled or not).
         */
        void defaults();

        /**
         * Returns whether the object is valid. Treat invalid KPluginInfo objects like you would
         * treat a null pointer.
         */
        bool isValid() const;

        /**
         * Creates a KPluginInfo object that shares the data with \p copy.
         */
        KPluginInfo(const KPluginInfo &copy);

        /**
         * Copies the KPluginInfo object to share the data with \p copy.
         */
        KPluginInfo &operator=(const KPluginInfo &rhs);

        /**
         * Compares two objects whether they share the same data.
         */
        bool operator==(const KPluginInfo &rhs) const;

        /**
         * Compares two objects whether they don't share the same data.
         */
        bool operator!=(const KPluginInfo &rhs) const;

        /**
         * Less than relation comparing the categories and if they are the same using the names.
         */
        bool operator<(const KPluginInfo &rhs) const;

        /**
         * Greater than relation comparing the categories and if they are the same using the names.
         */
        bool operator>(const KPluginInfo &rhs) const;

    private:
        friend KDECORE_EXPORT uint qHash(const KPluginInfo &, uint seed);
        QExplicitlySharedDataPointer<KPluginInfoPrivate> d;
};

KDECORE_EXPORT uint qHash(const KPluginInfo &, uint seed = 0);

#endif // KDECORE_KPLUGININFO_H
