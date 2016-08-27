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

#include "spot-on-crypt.h"
#ifdef SPOTON_MCELIECE_ENABLED
#include "spot-on-mceliece.h"
#endif
#include "spot-on-misc.h"

void spoton_crypt::generateMcElieceKeys(const QString &keySize,
					QByteArray &privateKey,
					QByteArray &publicKey,
					bool *ok)
{
  if(ok)
    *ok = false;

#ifdef SPOTON_MCELIECE_ENABLED
  size_t m = 0;
  size_t t = 0;

  if(keySize == "m11t51")
    {
      m = 11;
      t = 51;
    }
  else
    return;

  spoton_mceliece *mceliece = new (std::nothrow) spoton_mceliece(m, t);

  if(mceliece)
    if(mceliece->generatePrivatePublicKeys())
      {
	mceliece->privateKeyParameters(privateKey);

	if(!privateKey.isEmpty())
	  privateKey.prepend("mceliece-private-key-");

	mceliece->publicKeyParameters(publicKey);

	if(!publicKey.isEmpty())
	  publicKey.prepend("mceliece-public-key-");

	if(!publicKey.isEmpty() && !privateKey.isEmpty())
	  if(ok)
	    *ok = true;
      }

  delete mceliece;
#else
  Q_UNUSED(keySize);
  Q_UNUSED(privateKey);
  Q_UNUSED(publicKey);
#endif
}

QByteArray spoton_crypt::publicKeyDecryptMcEliece
(const QByteArray &data, bool *ok)
{
  if(ok)
    *ok = false;

#ifdef SPOTON_MCELIECE_ENABLED
  if(!m_mceliece)
    {
      QReadLocker locker(&m_privateKeyMutex);

      m_mceliece = new spoton_mceliece(m_privateKey, m_privateKeyLength);
    }

  if(!m_mceliece)
    return QByteArray();

  QByteArray bytes;
  std::stringstream ciphertext;
  std::stringstream plaintext;

  ciphertext << data.constData();

  if(m_mceliece->decrypt(ciphertext, plaintext))
    {
      bytes = QByteArray // A deep copy is required.
	(plaintext.str().c_str(),
	 static_cast<int> (plaintext.str().size()));

      if(!bytes.isEmpty())
	if(ok)
	  *ok = true;
    }

  return bytes;
#else
  Q_UNUSED(data);
  return QByteArray();
#endif
}

QByteArray spoton_crypt::publicKeyEncryptMcEliece(const QByteArray &data,
						  const QByteArray &publicKey,
						  bool *ok)
{
  if(ok)
    *ok = false;

#ifdef SPOTON_MCELIECE_ENABLED
  if(data.isEmpty() || !publicKey.startsWith("mceliece-public-key-"))
    return QByteArray();

  QByteArray bytes;
  spoton_mceliece *mceliece = new (std::nothrow) spoton_mceliece
    (publicKey.mid(static_cast<int> (qstrlen("mceliece-public-key-"))));
  std::stringstream ciphertext;

  if(mceliece)
    if(mceliece->encrypt(data.constData(),
			 static_cast<size_t> (data.length()),
			 ciphertext))
      {
	bytes = QByteArray // A deep copy is required.
	  (ciphertext.str().c_str(),
	   static_cast<int> (ciphertext.str().size()));

	if(!bytes.isEmpty())
	  if(ok)
	    *ok = true;
      }

  delete mceliece;
  return bytes;
#else
  Q_UNUSED(data);
  Q_UNUSED(publicKey);
  return QByteArray();
#endif
}

QString spoton_crypt::publicKeySizeMcEliece(const QByteArray &data)
{
  QString keySize("");

#ifdef SPOTON_MCELIECE_ENABLED
  if(!data.startsWith("mceliece-public-key-"))
    return keySize;

  spoton_mceliece *mceliece = new (std::nothrow) spoton_mceliece
    (data.mid(static_cast<int> (qstrlen("mceliece-public-key-"))));

  if(mceliece)
    keySize = QString("m%1t%2").arg(mceliece->m()).arg(mceliece->t());

  delete mceliece;
#endif
  return keySize;
}

QString spoton_crypt::publicKeySizeMcEliece(void)
{
#ifdef SPOTON_MCELIECE_ENABLED
  bool ok = true;

  publicKey(&ok);

  if(!ok)
    {
      spoton_misc::logError
	("spoton_crypt::publicKeySizeMcEliece(): publicKey() failure.");
      return "";
    }

  return publicKeySizeMcEliece(m_publicKey);
#else
  return publicKeySizeMcEliece(QByteArray());
#endif
}
