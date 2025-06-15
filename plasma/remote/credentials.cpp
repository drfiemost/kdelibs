/*
 *   Copyright © 2009 Rob Scheepmaker <r.scheepmaker@student.utwente.nl>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "credentials.h"

#include "authorizationmanager.h"
#include "config-plasma.h"

#include <QObject>

#include <kdebug.h>
#include <kstandarddirs.h>

#define REQUIRED_FEATURES "rsa,sha1,pkey"

namespace Plasma {

class CredentialsPrivate {
public:
    CredentialsPrivate()
    {
    }

    CredentialsPrivate(const QString &id, const QString &name,
                    const QString &pemKey, bool isPrivateKey)
        : id(id),
          name(name)
    {
    }

    ~CredentialsPrivate()
    {
    }

    QString id;
    QString name;
};

Credentials::Credentials(const QString &id, const QString &name,
                   const QString &key, bool isPrivateKey)
         : d(new CredentialsPrivate(id, name, key, isPrivateKey))
{
}

Credentials::Credentials()
         : d(new CredentialsPrivate())
{
}

Credentials::Credentials(const Credentials &other)
        : d(new CredentialsPrivate())
{
    *d = *other.d;
}

Credentials::~Credentials()
{
    delete d;
}

Credentials &Credentials::operator=(const Credentials &other)
{
    *d = *other.d;
    return *this;
}

Credentials Credentials::createCredentials(const QString &name)
{
    return Credentials();
}

TrustLevel Credentials::trustLevel() const
{
    /**
    QString pemFile = KStandardDirs::locate("trustedkeys", id());

    if (!pemFile.isEmpty()) {
        QCA::PublicKey pubKey = QCA::PublicKey::fromPEMFile(pemFile);
        if (pubKey == d->publicKey) {
            return true;
        }
    }
    */
    //Trust no one ;)
    return ValidCredentials;
}

bool Credentials::isValid() const
{
    kDebug() << "libplasma is compiled without support for remote widgets. Key invalid.";
    return false;
}

QString Credentials::name() const
{
    return d->name;
}

QString Credentials::id() const
{
    return d->id;
}

bool Credentials::isValidSignature(const QByteArray &signature, const QByteArray &payload) 
{
    return false;
}

bool Credentials::canSign() const
{
    return false;
}

QByteArray Credentials::signMessage(const QByteArray &message)
{
    return QByteArray();
}

Credentials Credentials::toPublicCredentials() const
{
    return Credentials();
}

QDataStream &operator<<(QDataStream &out, const Credentials &myObj)
{
    return out;
}

QDataStream &operator>>(QDataStream &in, Credentials &myObj)
{
    return in;
}

}
