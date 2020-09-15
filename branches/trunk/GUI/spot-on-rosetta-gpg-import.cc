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

#ifdef SPOTON_GPGME_ENABLED
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#endif

#include "Common/spot-on-misc.h"
#include "spot-on-defines.h"
#include "spot-on-rosetta-gpg-import.h"

spoton_rosetta_gpg_import::spoton_rosetta_gpg_import(QWidget *parent):
  QMainWindow(parent)
{
  m_ui.setupUi(this);
  connect(m_ui.importButton,
	  SIGNAL(clicked(void)),
	  this,
	  SLOT(slotImport(void)));
  setWindowTitle(tr("%1: Rosetta GPG Import").arg(SPOTON_APPLICATION_NAME));
}

spoton_rosetta_gpg_import::~spoton_rosetta_gpg_import()
{
}

void spoton_rosetta_gpg_import::slotImport(void)
{
#ifdef SPOTON_GPGME_ENABLED
  QString connectionName("");

  {
    QSqlDatabase db = spoton_misc::database(connectionName);

    db.setDatabaseName
      (spoton_misc::homePath() + QDir::separator() + "idiotes.db");

    if(db.open())
      {
	QSqlQuery query(db);

	query.exec("CREATE TABLE IF NOT EXISTS gpg ("
		   "private_keys TEXT NOT NULL, "
		   "private_keys_hash TEXT NOT NULL, "
		   "public_keys TEXT NOT NULL, "
		   "public_keys_hash TEXT NOT NULL, "
		   "PRIMARY KEY (private_keys_hash, public_keys_hash))");
      }

    db.close();
  }

  QSqlDatabase::removeDatabase(connectionName);
#endif
}
