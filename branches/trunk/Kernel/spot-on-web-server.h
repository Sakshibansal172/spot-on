/*
** Copyright (c) 2011 - 10^10^10, Alexis Megas.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from Spot-On without specific prior written permission.
**
** SPOT-ON IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** SPOT-ON, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _spoton_web_server_h_
#define _spoton_web_server_h_

#include <QFuture>
#include <QPointer>
#include <QQueue>
#include <QSqlDatabase>
#include <QSslSocket>
#include <QTcpServer>
#include <QTimer>

class spoton_web_server_tcp_server: public QTcpServer
{
  Q_OBJECT

 public:
  spoton_web_server_tcp_server(QObject *parent):QTcpServer(parent)
  {
  }

  ~spoton_web_server_tcp_server()
  {
  }

#if QT_VERSION < 0x050000
  void incomingConnection(int socketDescriptor);
#else
  void incomingConnection(qintptr socketDescriptor);
#endif

 protected:
  QByteArray m_certificate;
  QByteArray m_privateKey;

 signals:
  void newConnection(const qint64 socketDescriptor);
};

class spoton_web_server: public spoton_web_server_tcp_server
{
  Q_OBJECT

 public:
  spoton_web_server(QObject *parent);
  ~spoton_web_server();
  int clientCount(void) const;

 private:
  QAtomicInt m_abort;
  QMultiHash<qint64, QFuture<void> > m_futures;
  QTimer m_generalTimer;
  QSqlDatabase database(void) const;
  void process(const QPair<QByteArray, QByteArray> &credentials,
	       const qint64 socketDescriptor);
  void process(QSslSocket *socket,
	       const QByteArray &data,
	       const QPair<QString, QString> &address);
  void processLocal(QSslSocket *socket, const QByteArray &data);

 private slots:
  void slotClientConnected(const qint64 socketDescriptor);
  void slotFinished(const qint64 socketDescriptor);
  void slotTimeout(void);

 signals:
  void finished(const qint64 socketDescriptor);
};

#endif
