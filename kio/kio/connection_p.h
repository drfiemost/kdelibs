/* This file is part of the KDE libraries
    Copyright (C) 2007 Thiago Macieira <thiago@kde.org>

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

#ifndef KIO_CONNECTION_P_H
#define KIO_CONNECTION_P_H

#include <QObject>
#include <klocalsocket.h>

class KUrl;

namespace KIO {
    struct Task {
        int cmd;
        QByteArray data;
    };

    class ConnectionBackend: public QObject
    {
        Q_OBJECT
    public:
        enum { Idle, Listening, Connected } state;
        enum Mode { LocalSocketMode, TcpSocketMode };
        QString address;
        QString errorString;

    private:
        QTcpSocket *socket;
        union {
            KLocalSocketServer *localServer;
            QTcpServer *tcpServer;
        };
        long len;
        int cmd;
        int port;
        bool signalEmitted;
        quint8 mode;

        static constexpr int HeaderSize = 10;
        static constexpr int StandardBufferSize = 32 * 1024;

    Q_SIGNALS:
        void disconnected();
        void commandReceived(const Task &task);
        void newConnection();

    public:
        explicit ConnectionBackend(Mode m, QObject *parent = 0);
        ~ConnectionBackend();

        void setSuspended(bool enable);
        bool connectToRemote(const KUrl &url);
        bool listenForRemote();
        bool waitForIncomingTask(int ms);
        bool sendCommand(const Task &task);
        ConnectionBackend *nextPendingConnection();
    public slots:
        void socketReadyRead();
        void socketDisconnected();
    };
}

#endif
