/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   In addition, as a special exception, the copyright holders give
*   permission to link the code of portions of this program with the
*   OpenSSL library under certain conditions as described in each
*   individual source file, and distribute linked combinations including
*   the two.
*   
*   You must obey the GNU General Public License in all respects for all
*   of the code used other than OpenSSL. If you modify file(s) with this
*   exception, you may extend this exception to your version of the
*   file(s), but you are not obligated to do so. If you do not wish to do
*   so, delete this exception statement from your version. If you delete
*   this exception statement from all source files in the program, then
*   also delete it here.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "singleInstanceHandler.h"

// Activate this if you want to know when which difference is loaded
#define SINGLEINSTANCEHANDLER_DEBUG 0
#if SINGLEINSTANCEHANDLER_DEBUG && !NDEBUG
#include <qDebug>
#define DEBUG_SINGLEISNTANCE qDebug
#else
#define DEBUG_SINGLEISNTANCE(fmt,...) ((void)0)
#endif

singleInstanceHandler::singleInstanceHandler(QObject *parent) : QObject(parent)
{
  connect(&server, &QLocalServer::newConnection, this, &singleInstanceHandler::newConnection);
}

singleInstanceHandler::~singleInstanceHandler()
{
  server.close();
}

void singleInstanceHandler::newConnection()
{
  socket = server.nextPendingConnection();
  connect(socket.data(), &QLocalSocket::readyRead, this, &singleInstanceHandler::readyRead);
}

void singleInstanceHandler::readyRead()
{
  QByteArray tmp = socket->readAll();
  QStringList fileList = QString(tmp).split("\n");
  if (fileList.length() > 0)
  {
    QStringList openFiles;
    for (QString s: fileList)
    {
      if (!s.isEmpty())
      {
        DEBUG_SINGLEISNTANCE("singleInstanceHandler::readyRead got file %s", s.toLatin1().data());
        openFiles.append(s);
      }
    }

    emit newAppStarted(openFiles);
  }

  socket->close();
  socket->deleteLater();
}

bool singleInstanceHandler::isRunning(QString name, QStringList args)
{
  DEBUG_SINGLEISNTANCE("singleInstanceHandler::isRunning %s", name.toLatin1().data());

  QLocalSocket socket;
  socket.connectToServer(name, QLocalSocket::ReadWrite);

  if(socket.waitForConnected())
  {
    DEBUG_SINGLEISNTANCE("singleInstanceHandler::isRunning Connected to other instance. Sending data.");

    QByteArray buffer;
    foreach(QString item, args)
      buffer.append(item + "\n");
    socket.write(buffer);
    socket.waitForBytesWritten();
    return true;
  }

  DEBUG_SINGLEISNTANCE("singleInstanceHandler::isRunning we are the first session - %s", socket.errorString().toLatin1().data());
  return false;
}

void singleInstanceHandler::listen(QString name)
{
  server.removeServer(name);
  server.listen(name);

  DEBUG_SINGLEISNTANCE("singleInstanceHandler::listen name %s - %s", name.toLatin1().data(), server.errorString().toLatin1().data());
}