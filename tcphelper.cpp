//  Copyright © 2011  Vinícius dos Santos Oliveira

#include "tcphelper.h"
#include <QTcpSocket>
#include <QDataStream>
#include <QHostAddress>

using namespace JsonRPC;

TcpHelper::TcpHelper(QObject *parent) :
    QObject(parent),
    peer(NULL),
    socket(NULL),
    nextMessageSize(0)
{
}

bool TcpHelper::setSocket(QTcpSocket *socket)
{
//    if (this->socket)
//        onDisconnected();

    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        socket->setParent(this);

        connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
        connect(socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));

        QString peerIP = socket->peerAddress().toString().split("::ffff:")[1];
        peer = new Peer(this);
        peer->setPeerIP(peerIP);

        connect(peer, SIGNAL(readyRequestMessage(QByteArray)),
                this, SLOT(onReadyMessage(QByteArray)));
        connect(peer, SIGNAL(readyResponseMessage(QByteArray)),
                this, SLOT(onReadyMessage(QByteArray)));

        connect(peer, SIGNAL(readyResponse(QVariant,QVariant)),
                this, SIGNAL(readyResponse(QVariant,QVariant)));
        connect(peer, SIGNAL(requestError(int,QString,QVariant,QVariant)),
                this, SIGNAL(requestError(int,QString,QVariant,QVariant)));
        connect(peer,
                SIGNAL(readyRequest(QSharedPointer<JsonRPC::ResponseHandler>)),
                this,
                SIGNAL(readyRequest(QSharedPointer<JsonRPC::ResponseHandler>)));

        this->socket = socket;
        return true;
    } else {
        return false;
    }
}

bool TcpHelper::call(const QString &method, const QVariant &params, const QVariant &id)
{
    if (peer)
        return peer->call(method, params, id);
    else
        return false;
}

void TcpHelper::onReadyMessage(const QByteArray &json)
{
    {
        QDataStream stream(socket);
        stream.setVersion(QDataStream::Qt_4_6);
        quint16 size = json.size();
        stream << size;
    }
    socket->write(json);
}

void TcpHelper::onReadyRead()
{
    buffer.append(socket->readAll());

    if (buffer.size() > 10)
        goto STATE_WAITING_FOR_CONTENT;

    STATE_UNKNOW_SIZE:
    // nextMessageSize is 2 bytes long (quint16)
    if (buffer.size() >= 2) {
        QDataStream stream(&buffer, QIODevice::ReadWrite);
        stream.setVersion(QDataStream::Qt_4_6);
        stream >> nextMessageSize;
        buffer.remove(0, 2);
    } else {
        return;
    }

    STATE_WAITING_FOR_CONTENT:
    if (buffer.size() >= 30) {
        peer->handleMessage(buffer.left(buffer.size()));
        buffer.remove(0, buffer.size());

//        nextMessageSize = 0;
        goto STATE_UNKNOW_SIZE;
    }
}

void TcpHelper::onDisconnected()
{
    // clear peer data
    peer->deleteLater();
    peer = NULL;

    // clear buffer data
    buffer.clear();
    nextMessageSize = 0;

    // clear socket data
    socket->disconnect();
    socket->deleteLater();
    socket = NULL;

    emit disconnected();
}

QTcpSocket *TcpHelper::getSocket() const
{
    return socket;
}

quint32 TcpHelper::nextMessageId()
{
    return ++messageId;
}

void TcpHelper::setMessageId(quint32 newMessageId)
{
    messageId = newMessageId;
}
