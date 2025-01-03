/* This file is part of the KDE libraries
   Copyright (C) 2001, 2002, 2007 David Faure <faure@kde.org>

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

#include "klimitediodevice_p.h"

KLimitedIODevice::KLimitedIODevice( QIODevice *dev, qint64 start, qint64 length )
    : m_dev( dev ), m_start( start ), m_length( length )
{
    //kDebug(7005) << "start=" << start << "length=" << length;
    open( QIODevice::ReadOnly ); //krazy:exclude=syscalls
}

bool KLimitedIODevice::open( QIODevice::OpenMode m )
{
    //kDebug(7005) << "m=" << m;
    if ( m & QIODevice::ReadOnly ) {
        /*bool ok = false;
          if ( m_dev->isOpen() )
          ok = ( m_dev->mode() == QIODevice::ReadOnly );
          else
          ok = m_dev->open( m );
          if ( ok )*/
        m_dev->seek( m_start ); // No concurrent access !
    }
    else
        kWarning(7005) << "KLimitedIODevice::open only supports QIODevice::ReadOnly!";
    setOpenMode( QIODevice::ReadOnly );
    return true;
}

void KLimitedIODevice::close()
{
}

qint64 KLimitedIODevice::size() const
{
    return m_length;
}

qint64 KLimitedIODevice::readData( char * data, qint64 maxlen )
{
    maxlen = std::min( maxlen, m_length - pos() ); // Apply upper limit
    return m_dev->read( data, maxlen );
}

bool KLimitedIODevice::seek( qint64 pos )
{
    Q_ASSERT( pos <= m_length );
    pos = std::min( pos, m_length ); // Apply upper limit
    bool ret = m_dev->seek( m_start + pos );
    if ( ret ) {
        QIODevice::seek( pos );
    }
    return ret;
}

qint64 KLimitedIODevice::bytesAvailable() const
{
     return QIODevice::bytesAvailable();
}

bool KLimitedIODevice::isSequential() const
{
    return m_dev->isSequential();
}
