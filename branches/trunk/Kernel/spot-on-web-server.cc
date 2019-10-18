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

#include <QNetworkInterface>
#include <QSqlQuery>
#include <QSslKey>
#include <QSslSocket>

#include "Common/spot-on-common.h"
#include "Common/spot-on-crypt.h"
#include "Common/spot-on-misc.h"
#include "spot-on-web-server.h"
#include "spot-on-kernel.h"

static QByteArray s_search;
static quint64 s_urlLimit = 25;

#if QT_VERSION < 0x050000
void spoton_web_server_tcp_server::incomingConnection(int socketDescriptor)
#else
void spoton_web_server_tcp_server::incomingConnection(qintptr socketDescriptor)
#endif
{
  QPointer<QSslSocket> socket;

  try
    {
      socket = new QSslSocket(this);
      socket->setSocketDescriptor(socketDescriptor);
      socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
      connect(socket,
	      SIGNAL(modeChanged(QSslSocket::SslMode)),
	      this,
	      SIGNAL(modeChanged(QSslSocket::SslMode)));

      QSslConfiguration configuration;
      QString sslCS
	(spoton_kernel::
	 setting("gui/sslControlString",
		 spoton_common::SSL_CONTROL_STRING).toString());

      configuration.setLocalCertificate(QSslCertificate(m_certificate));
      configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
      configuration.setPrivateKey(QSslKey(m_privateKey, QSsl::Rsa));
#if QT_VERSION >= 0x040806
      configuration.setSslOption(QSsl::SslOptionDisableCompression, true);
      configuration.setSslOption(QSsl::SslOptionDisableEmptyFragments, true);
      configuration.setSslOption
	(QSsl::SslOptionDisableLegacyRenegotiation, true);
#endif
#if QT_VERSION >= 0x050501
      spoton_crypt::setSslCiphers
	(QSslConfiguration::supportedCiphers(), sslCS, configuration);
#else
      spoton_crypt::setSslCiphers
	(socket->supportedCiphers(), sslCS, configuration);
#endif
      socket->setSslConfiguration(configuration);
      socket->startServerEncryption();
      m_queue.enqueue(socket);
      emit newConnection();
    }
  catch(...)
    {
      m_queue.removeOne(socket);

      if(socket)
	socket->deleteLater();

      spoton_misc::closeSocket(socketDescriptor);
      spoton_misc::logError
	("spoton_web_server_tcp_server::incomingConnection(): socket deleted.");
    }
}

spoton_web_server::spoton_web_server(QObject *parent):
  spoton_web_server_tcp_server(parent)
{
  QFile file(":/search.html");

  file.open(QFile::ReadOnly);
  s_search = file.readAll();
  file.close();
  connect(&m_generalTimer,
	  SIGNAL(timeout(void)),
	  this,
	  SLOT(slotTimeout(void)));
  connect(this,
	  SIGNAL(finished(QSslSocket *, const QByteArray &)),
	  this,
	  SLOT(slotFinished(QSslSocket *, const QByteArray &)));
  connect(this,
	  SIGNAL(newConnection(void)),
	  this,
	  SLOT(slotClientConnected(void)));
  m_generalTimer.start(2500);
}

spoton_web_server::~spoton_web_server()
{
#if QT_VERSION < 0x050000
  QMutableHashIterator<int, QFuture<void> > it(m_futures);
#else
  QMutableHashIterator<qintptr, QFuture<void> > it(m_futures);
#endif

  while(it.hasNext())
    {
      it.next();
      it.value().cancel();
      it.value().waitForFinished();
      it.remove();
    }

  m_generalTimer.stop();
  m_webSocketData.clear();
}

QSqlDatabase spoton_web_server::database(void) const
{
  QSqlDatabase db;
  QString connectionName(spoton_misc::databaseName());

  if(spoton_kernel::setting("gui/sqliteSearch", true).toBool())
    {
      db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
      db.setDatabaseName
	(spoton_misc::homePath() + QDir::separator() + "urls.db");
      db.open();
    }
  else
    {
      QByteArray password;
      QString database
	(spoton_kernel::setting("gui/postgresql_database", "").
	 toString().trimmed());
      QString host
	(spoton_kernel::setting("gui/postgresql_host", "localhost").
	 toString().trimmed());
      QString name
	(spoton_kernel::setting("gui/postgresql_name", "").toString().
	 trimmed());
      QString str("connect_timeout=10");
      bool ok = true;
      bool ssltls = spoton_kernel::setting
	("gui/postgresql_ssltls", false).toBool();
      int port = spoton_kernel::setting("gui/postgresql_port", 5432).toInt();
      spoton_crypt *crypt = spoton_kernel::s_crypts.value("chat", 0);

      if(crypt)
	password = crypt->decryptedAfterAuthenticated
	  (QByteArray::
	   fromBase64(spoton_kernel::setting("gui/postgresql_password", "").
		      toByteArray()), &ok);

      if(ssltls)
	str.append(";requiressl=1");

      db = QSqlDatabase::addDatabase("QPSQL", connectionName);
      db.setConnectOptions(str);
      db.setDatabaseName(database);
      db.setHostName(host);
      db.setPort(port);

      if(ok)
	db.open(name, password);
    }

  return db;
}

int spoton_web_server::clientCount(void) const
{
  return m_webSocketData.size();
}

void spoton_web_server::process(QSslSocket *socket,
				const QByteArray &data,
				const QPair<QString, QString> &address)
{
  QStringList list(QString(data.mid(data.indexOf("current=") + 8)).split("&"));

  if(list.size() != 5)
    {
      emit finished(socket, QByteArray());
      return;
    }

  quint64 current = 0;
  quint64 offset = 0;
  quint64 pages = 0;

  for(int i = 0; i < list.size(); i++)
    list.replace
      (i,
       QString(list.at(i)).
       remove("link=").
       remove("offset=").
       remove("pages=").
       remove("search="));

  current = list.value(0).toULongLong();
  offset = list.value(2).toULongLong();
  pages = list.value(3).toULongLong();

  if(current > pages || offset % s_urlLimit != 0)
    {
      emit finished(socket, QByteArray());
      return;
    }

  QScopedPointer<spoton_crypt> crypt
    (spoton_misc::
     retrieveUrlCommonCredentials(spoton_kernel::s_crypts.value("chat", 0)));

  if(!crypt)
    {
      emit finished(socket, QByteArray());
      return;
    }

  QSqlDatabase db(database());
  QString connectionName(db.connectionName());
  QString html("");

  if(db.isOpen())
    {
      QString link(list.value(1).toLower());
      QString querystr("");
      QString search("");
      QString particles(data.mid(data.indexOf("current=")));
      quint64 count = 0;

      search = list.value(4);
      search = spoton_misc::percentEncoding(search);
      search.replace("+", " ");

      if(search.trimmed().isEmpty())
	{
	  for(int i = 0; i < 10 + 6; i++)
	    for(int j = 0; j < 10 + 6; j++)
	      {
		QChar c1;
		QChar c2;

		if(i <= 9)
		  c1 = QChar(i + 48);
		else
		  c1 = QChar(i + 97 - 10);

		if(j <= 9)
		  c2 = QChar(j + 48);
		else
		  c2 = QChar(j + 97 - 10);

		if(i == 15 && j == 15)
		  querystr.append
		    (QString("SELECT title, "
			     "url, "
			     "description, "
			     "url_hash, "
			     "date_time_inserted "
			     "FROM spot_on_urls_%1%2 ").arg(c1).arg(c2));
		else
		  querystr.append
		    (QString("SELECT title, "
			     "url, "
			     "description, "
			     "url_hash, "
			     "date_time_inserted "
			     "FROM spot_on_urls_%1%2 UNION ").arg(c1).arg(c2));
	      }

	  querystr.append(" ORDER BY 5 DESC ");
	  querystr.append(QString(" LIMIT %1 ").arg(s_urlLimit));
	  querystr.append(QString(" OFFSET %1 ").arg(offset));
	}
      else
	{
	  QSet<QString> keywords;
	  QString keywordsearch("");
	  QString originalSearch(search);
	  QStringList keywordsearches;
	  bool ok = true;

	  originalSearch.replace("&#34;", "\"");

	  do
	    {
	      int e = -1;
	      int s = originalSearch.indexOf('"');

	      if(s < 0)
		break;

	      e = originalSearch.indexOf('"', s + 1);

	      if(e < 0 || e - s - 1 <= 0)
		break;

	      QString bundle(originalSearch.mid(s + 1, e - s - 1).trimmed());

	      if(bundle.isEmpty())
		break;

	      originalSearch.remove(s, e - s + 1);
	      keywords = bundle.split
		     (QRegExp("\\W+"), QString::SkipEmptyParts).toSet();
	      keywordsearch.clear();

	      QSetIterator<QString> it(keywords);

	      while(it.hasNext())
		{
		  QByteArray keywordHash
		    (crypt->keyedHash(it.next().toUtf8(), &ok));

		  if(!ok)
		    continue;

		  QByteArray keywordHashHex(keywordHash.toHex());

		  keywordsearch.append
		     (QString("SELECT url_hash FROM "
			      "spot_on_keywords_%1 WHERE "
			      "keyword_hash = '%2' ").
		      arg(keywordHashHex.mid(0, 2).constData()).
		      arg(keywordHashHex.constData()));

		  if(it.hasNext())
		    keywordsearch.append(" INTERSECT ");
		}

	      keywordsearches << keywordsearch;
	    }
	  while(true);

	  keywords = originalSearch.toLower().trimmed().
	    split(QRegExp("\\W+"), QString::SkipEmptyParts).toSet();
	  keywordsearch.clear();

	  QSetIterator<QString> it(keywords);

	  while(it.hasNext())
	    {
	      QByteArray keywordHash(crypt->keyedHash(it.next().toUtf8(), &ok));

	      if(!ok)
		continue;

	      QByteArray keywordHashHex(keywordHash.toHex());

	      keywordsearch.append
		(QString("SELECT url_hash FROM "
			 "spot_on_keywords_%1 WHERE "
			 "keyword_hash = '%2' ").
		 arg(keywordHashHex.mid(0, 2).constData()).
		 arg(keywordHashHex.constData()));

	      if(it.hasNext())
		keywordsearch.append(" UNION ");
	    }

	  if(!keywords.isEmpty())
	    keywordsearches << keywordsearch;

	  keywordsearch.clear();

	  QMap<QString, QString> prefixes;

	  for(int i = 0; i < keywordsearches.size(); i++)
	    {
	      QSqlQuery query(db);

	      query.setForwardOnly(true);

	      if(query.exec(keywordsearches.at(i)))
		while(query.next())
		  {
		    QString hash(query.value(0).toString());
		    QString prefix(hash.mid(0, 2));

		    if(!prefixes.contains(prefix))
		      prefixes.insert(prefix, QString("'%1'").arg(hash));
		    else
		      {
			QString str(prefixes.value(prefix));

			str.append(QString(", '%1'").arg(hash));
			prefixes.insert(prefix, str);
		      }
		  }
	    }

	  if(!prefixes.isEmpty())
	    {
	      QMapIterator<QString, QString> it(prefixes);

	      while(it.hasNext())
		{
		  it.next();

		  /*
		  ** For absolute correctness, we ought to use parameters in
		  ** the SQL queries.
		  */

		  querystr.append
		    (QString("SELECT title, "
			     "url, "
			     "description, "
			     "url_hash, "
			     "date_time_inserted "
			     "FROM spot_on_urls_%1 WHERE url_hash IN (%2) ").
		     arg(it.key()).arg(it.value()));

		  if(it.hasNext())
		    querystr.append(" UNION ");
		}

	      querystr.append(" ORDER BY 5 DESC ");
	      querystr.append(QString(" LIMIT %1 ").arg(s_urlLimit));
	      querystr.append(QString(" OFFSET %1 ").arg(offset));
	    }
	}

      QSqlQuery query(db);

      if(!querystr.trimmed().isEmpty())
	{
	  query.setForwardOnly(true);
	  query.prepare(querystr);
	}

      if(query.exec() || querystr.trimmed().isEmpty())
	{
	  html.append
	    ("HTTP/1.1 200 OK\r\n"
	     "Content-Type: text/html; charset=utf-8\r\n\r\n");
	  html.append(s_search);
	  html.replace("value=\"\"", QString("value=\"%1\"").arg(search));
	  html.remove("</html>");
	  html.append("<div id=\"footer\">");

	  while(query.next())
	    {
	      QByteArray bytes;
	      QString description("");
	      QString title("");
	      QString urlHash(query.value(3).toByteArray());
	      QUrl url;
	      bool ok = true;

	      bytes = crypt-> decryptedAfterAuthenticated
		(QByteArray::fromBase64(query.value(2).toByteArray()), &ok);
	      description = QString::fromUtf8
		(bytes.constData(), bytes.length()).trimmed();

	      if(ok)
		{
		  bytes = crypt->decryptedAfterAuthenticated
		    (QByteArray::fromBase64(query.value(0).toByteArray()), &ok);
		  title = QString::fromUtf8
		    (bytes.constData(), bytes.length()).trimmed();
		  title = spoton_misc::removeSpecialHtmlTags(title).trimmed();
		}

	      if(ok)
		{
		  bytes = crypt->decryptedAfterAuthenticated
		    (QByteArray::fromBase64(query.value(1).toByteArray()), &ok);
		  url = QUrl::fromUserInput
		    (QString::fromUtf8(bytes.constData(), bytes.length()));
		}

	      if(ok)
		{
		  description = spoton_misc::removeSpecialHtmlTags(description);

	          if(description.length() > spoton_common::
		     MAXIMUM_DESCRIPTION_LENGTH_SEARCH_RESULTS)
		    {
		      description = description.mid
			(0, spoton_common::
			 MAXIMUM_DESCRIPTION_LENGTH_SEARCH_RESULTS).trimmed();

		      if(description.endsWith("..."))
			{
			}
		      else if(description.endsWith(".."))
			description.append(".");
		      else if(description.endsWith("."))
			description.append("..");
		      else
			description.append("...");
		    }

		  QString scheme(url.scheme().toLower().trimmed());

		  url.setScheme(scheme);

		  if(title.isEmpty())
		    title = spoton_misc::urlToEncoded(url);

		  html.append("<p>");
		  html.append("<a href=\"");
		  html.append(spoton_misc::urlToEncoded(url));
		  html.append("\" target=\"_blank\"><font color=\"#0000EE\">");
		  html.append(title);
		  html.append("</font></a> | ");
		  html.append("<a href=\"https://");
		  html.append(address.first);
		  html.append(":");
		  html.append(address.second);
		  html.append("/local-");
		  html.append(urlHash);
		  html.append("\" target=\"_blank\"><font color=\"#0000EE\">");
		  html.append("View Locally");
		  html.append("</font></a>");
		  html.append("<br>");
		  html.append
		    (QString("<font color=\"green\" size=2>%1</font>").
		     arg(spoton_misc::urlToEncoded(url).constData()));

		  if(!description.isEmpty())
		    {
		      html.append("<br>");
		      html.append
			(QString("<font color=\"#696969\" size=2>%1</font>").
			 arg(description));
		    }

		  html.append("</p><br>");
		  count += 1;
		}
	    }

	  if(link == "%3e")
	    {
	      current += 1;
	      offset += s_urlLimit;
	    }
	  else
	    {
	      current = link.toULongLong();
	      offset = (current - 1) * s_urlLimit;
	    }

	  if(count > 0)
	    if(link == "%3e")
	      if(offset / s_urlLimit > pages)
		pages += 1;

	  QString str("");
	  quint64 lower = 0;
	  quint64 upper = 0;

	  // 1  ... 10.
	  // 11 ... 20.
	  // Find the lower and upper bounds.

	  lower = offset / s_urlLimit + 1;
	  upper = lower + s_urlLimit;

	  if(pages < upper)
	    upper = pages;

	  if(upper > s_urlLimit) // Number of pages to display.
	    lower = upper - s_urlLimit;
	  else
	    lower = 1;

	  search.replace(" ", "+");

	  for(quint64 i = lower; i <= upper; i++)
	    if(i != current)
	      {
		particles = QString
		  ("current=%1&link=%2&offset=%3&pages=%4&search=%5").
		  arg(i - 1).
		  arg(i).
		  arg((i - 1) * s_urlLimit).
		  arg(pages).
		  arg(search);
		str.append
		  (QString(" <a href=\"%1\"><font color=\"#c0c0c0\">%2</font>"
			   "</a> ").arg(particles).arg(i));
	      }
	    else
	      str.append
		(QString(" <font color=\"#696969\">| %1 |</font> ").arg(i));

	  if(count >= s_urlLimit)
	    {
	      particles = QString
		("current=%1&link=>&offset=%2&pages=%3&search=%4").
		arg(current).arg(current * s_urlLimit).arg(pages).arg(search);
	      str.append(QString(" <a href=\"%1\">></a> ").arg(particles));
	    }

	  if(current != 1)
	    {
	      particles = QString
		("current=%1&link=%2&offset=%3&pages=%4&search=%5").
		arg(current - 2).
		arg(current - 1).
		arg((current - 2) * s_urlLimit).
		arg(pages).
		arg(search);
	      str.prepend
		(QString(" <a href=\"%1\"><</a> ").arg(particles));
	    }

	  str = str.trimmed();
	  html.append("<center>");
	  html.append(str);
	  html.append("</center></div></html>");
	}
    }

  db.close();
  db = QSqlDatabase();
  QSqlDatabase::removeDatabase(connectionName);
  emit finished(socket, html.toUtf8());
}

void spoton_web_server::processLocal(QSslSocket *socket, const QByteArray &data)
{
  QScopedPointer<spoton_crypt> crypt
    (spoton_misc::
     retrieveUrlCommonCredentials(spoton_kernel::s_crypts.value("chat", 0)));

  if(!crypt)
    {
      emit finished(socket, QByteArray());
      return;
    }

  QSqlDatabase db(database());
  QString connectionName(db.connectionName());
  QString html("");

  if(db.isOpen())
    {
      QSqlQuery query(db);
      QString querystr("");

      querystr.append("SELECT content FROM spot_on_urls_");
      querystr.append(data.mid(0, 2).constData());
      querystr.append(" WHERE url_hash = ?");
      query.setForwardOnly(true);
      query.prepare(querystr);
      query.addBindValue(data.constData());

      if(query.exec() && query.next())
	{
	  QByteArray content;
	  bool ok = true;

	  content = crypt->decryptedAfterAuthenticated
	    (QByteArray::fromBase64(query.value(0).toByteArray()), &ok);

	  if(ok)
	    {
	      content = qUncompress(content);

	      if(!content.isEmpty())
		{
		  html.append
		    ("HTTP/1.1 200 OK\r\n"
		     "Content-Type: text/html; charset=utf-8\r\n\r\n");
		  html.append(content);
		}
	    }
	}
    }

  db.close();
  db = QSqlDatabase();
  QSqlDatabase::removeDatabase(connectionName);
  emit finished(socket, html.toUtf8());
}

void spoton_web_server::slotClientConnected(void)
{
  QSslSocket *socket = qobject_cast<QSslSocket *> (nextPendingConnection());

  if(socket)
    {
      connect(socket,
	      SIGNAL(disconnected(void)),
	      this,
	      SLOT(slotClientDisconnected(void)));
      connect(socket,
	      SIGNAL(modeChanged(QSslSocket::SslMode)),
	      this,
	      SLOT(slotModeChanged(QSslSocket::SslMode)));
      connect(socket,
	      SIGNAL(readyRead(void)),
	      this,
	      SLOT(slotReadyRead(void)));
    }
}

void spoton_web_server::slotClientDisconnected(void)
{
  QSslSocket *socket = qobject_cast<QSslSocket *> (sender());

  if(socket)
    {
      spoton_misc::logError
	(QString("spoton_web_server::slotClientDisconnected(): "
		 "client %1:%2 disconnected.").
	 arg(socket->peerAddress().toString()).
	 arg(socket->peerPort()));

      auto fd = socket->socketDescriptor();

      m_futures.remove(fd);
      m_webSocketData.remove(fd);
      socket->deleteLater();
    }
}

void spoton_web_server::slotFinished(QSslSocket *socket, const QByteArray &data)
{
  if(socket)
    {
      if(data.isEmpty())
	{
	  socket->write
	    ("HTTP/1.1 200 OK\r\n"
	     "Content-Type: text/html; charset=utf-8\r\n\r\n");
	  socket->write(s_search);
	}
      else
	socket->write(data);

      socket->flush();
      socket->deleteLater();
    }
}

void spoton_web_server::slotModeChanged(QSslSocket::SslMode mode)
{
  QSslSocket *socket = qobject_cast<QSslSocket *> (sender());

  if(!socket)
    {
      spoton_misc::logError
	("spoton_web_server::slotModeChanged(): empty socket object.");
      return;
    }

  if(mode == QSslSocket::UnencryptedMode)
    {
      spoton_misc::logError
	(QString("spoton_web_server::slotModeChanged(): "
		 "plaintext mode. Disconnecting socket %1:%2.").
	 arg(socket->peerAddress().toString()).
	 arg(socket->peerPort()));
      socket->abort();
    }
}

void spoton_web_server::slotReadyRead(void)
{
  QSslSocket *socket = qobject_cast<QSslSocket *> (sender());

  if(!socket)
    {
      spoton_misc::logError
	("spoton_web_server::slotReadyRead(): empty socket object.");
      return;
    }

  /*
  ** What if socketDescriptor() equals negative one?
  */

  while(socket->bytesAvailable() > 0)
    m_webSocketData[socket->socketDescriptor()].append(socket->readAll());

  QByteArray data(m_webSocketData.value(socket->socketDescriptor()).toLower());

  if(data.endsWith("\r\n\r\n") &&
     data.simplified().trimmed().startsWith("get / http/1.1"))
    {
      socket->write
	("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n");
      socket->write(s_search);
      socket->flush();
      socket->deleteLater();
    }
  else if(data.endsWith("\r\n\r\n") &&
	  data.simplified().trimmed().startsWith("get /current="))
    {
      data = data.simplified().trimmed().mid(5); // get /c <- c
      data = data.mid(0, data.indexOf(' '));

      QPair<QString, QString> address (socket->localAddress().toString(),
				       QString::number(socket->localPort()));

      m_futures[socket->socketDescriptor()] =
	QtConcurrent::run(this,
			  &spoton_web_server::process,
			  socket,
			  data,
			  address);
    }
  else if(data.endsWith("\r\n\r\n") &&
	  data.simplified().trimmed().startsWith("get /local-"))
    {
      data = data.simplified().trimmed().mid(11); // get /local-x <- x
      data = data.mid(0, data.indexOf(' '));
      m_futures[socket->socketDescriptor()] =
	QtConcurrent::run(this, &spoton_web_server::processLocal, socket, data);
    }
  else if(data.simplified().startsWith("post / http/1.1") ||
	  data.simplified().startsWith("post /current="))
    {
      data = data.simplified().trimmed();
      data = data.mid(data.lastIndexOf("current="));

      QPair<QString, QString> address (socket->localAddress().toString(),
				       QString::number(socket->localPort()));

      m_futures[socket->socketDescriptor()] =
	QtConcurrent::run(this,
			  &spoton_web_server::process,
			  socket,
			  data,
			  address);
    }
  else if(m_futures.value(socket->socketDescriptor()).isFinished())
    socket->close();

  if(m_webSocketData.value(socket->socketDescriptor()).size() >
     spoton_common::MAXIMUM_KERNEL_WEB_SERVER_SINGLE_SOCKET_BUFFER_SIZE)
    {
      spoton_misc::logError
	(QString("spoton_web_server::slotReadyRead(): "
		 "container for socket %1:%2 contains too much data. "
		 "Discarding data.").
	 arg(socket->localAddress().toString()).
	 arg(socket->localPort()));
      socket->deleteLater();
    }
}

void spoton_web_server::slotTimeout(void)
{
  quint16 port = static_cast<quint16>
    (spoton_kernel::setting("gui/web_server_port", 0).toInt());

  if(port == 0)
    {
      close();

      foreach(QSslSocket *socket, findChildren<QSslSocket *> ())
	socket->deleteLater();

      return;
    }

  if(m_certificate.isEmpty() || m_privateKey.isEmpty())
    {
      spoton_crypt *crypt = spoton_kernel::s_crypts.value("chat", 0);

      if(crypt)
	{
	  QString connectionName("");

	  {
	    QSqlDatabase db = spoton_misc::database(connectionName);

	    db.setDatabaseName(spoton_misc::homePath() +
			       QDir::separator() +
			       "kernel_web_server.db");

	    if(db.open())
	      {
		QSqlQuery query(db);

		query.setForwardOnly(true);

		if(query.exec("SELECT certificate, private_key "
			      "FROM kernel_web_server"))
		  while(query.next())
		    {
		      bool ok = true;

		      m_certificate = crypt->decryptedAfterAuthenticated
			(QByteArray::fromBase64(query.value(0).toByteArray()),
			 &ok);
		      m_privateKey = crypt->decryptedAfterAuthenticated
			(QByteArray::fromBase64(query.value(1).toByteArray()),
			 &ok);
		    }
	      }

	    db.close();
	  }

	  QSqlDatabase::removeDatabase(connectionName);
	}
    }

  if(isListening())
    if(port != serverPort())
      {
	close();

	foreach(QSslSocket *socket, findChildren<QSslSocket *> ())
	  socket->deleteLater();

	m_certificate.clear();
	m_privateKey.clear();
      }

  if(!isListening())
    if(!listen(spoton_misc::localAddressIPv4(), port))
      spoton_misc::logError
	("spoton_web_server::slotTimeout(): listen() failure. "
	 "This is a serious problem!");
}
