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

#include "GithubFileDownloader.h"

#include <QNetworkReply>
#include <QTextStream>
#include <QMetaEnum>

#define UPDATER_GITHUBDOWNLOADER_OUTPUT 0
#if UPDATER_GITHUBDOWNLOADER_OUTPUT && !NDEBUG
#include <QDebug>
#include <source_location>
#define DEBUG_DOWNNLOAD(f)                                                                         \
  do                                                                                               \
  {                                                                                                \
    qDebug() << std::source_location::current().function_name() << " " << f;                       \
  } while (0)
#else
#define DEBUG_DOWNNLOAD(f) ((void)0)
#endif

namespace update
{

namespace
{

QString getBaseUrl(GithubFileDownloader::UseRepoBranch repoBranch)
{
  if (repoBranch == GithubFileDownloader::UseRepoBranch::Deploy)
    return "https://raw.githubusercontent.com/IENT/YUViewReleases/master/win/autoupdate/";
  if (repoBranch == GithubFileDownloader::UseRepoBranch::TestDeply)
    return "https://raw.githubusercontent.com/IENT/YUViewReleases/dev/win/autoupdate/";
  if (repoBranch == GithubFileDownloader::UseRepoBranch::UnitTest)
    return "https://raw.githubusercontent.com/IENT/YUViewReleases/unitTest/";
  return "";
}

} // namespace

GithubFileDownloader::GithubFileDownloader(UseRepoBranch repoBranch) : useRepoBranch(repoBranch)
{
  connect(
    &networkManager, &QNetworkAccessManager::finished, this, &GithubFileDownloader::replyFinished);
  connect(
    &networkManager, &QNetworkAccessManager::sslErrors, this, &GithubFileDownloader::sslErrors);
}

void GithubFileDownloader::startDownloadOfFile(const QString &filename)
{
  if (this->state != State::Disconnected && this->state != State::Connected)
  {
    DEBUG_DOWNNLOAD("Invalid state " << stateMapper.getName(this->state) << " to start download;");
    emit downloadFinished({"", "Invalid state to start download."});
    return;
  }

  this->downloadUrl = getBaseUrl(this->useRepoBranch) + filename;
  if (this->state == State::Connected)
    this->startDownloadIfAlreadyConnected();
  else
  {
    DEBUG_DOWNNLOAD("Connecting to raw.githubusercontent.com");
    this->state = State::EstablishingConnection;
    networkManager.connectToHostEncrypted("raw.githubusercontent.com");
  }
}

void GithubFileDownloader::startDownloadIfAlreadyConnected()
{
  assert(this->state == State::Connected);
  assert(!this->downloadUrl.isEmpty());

  DEBUG_DOWNNLOAD("Start download of file " << this->downloadUrl);

  this->state = State::DownloadingFile;
  auto reply  = networkManager.get(QNetworkRequest(QUrl(this->downloadUrl)));
  connect(reply,
          &QNetworkReply::downloadProgress,
          this,
          &GithubFileDownloader::networkReplyDownloadProgress);
}

void GithubFileDownloader::replyFinished(QNetworkReply *reply)
{
  DEBUG_DOWNNLOAD("State " << stateMapper.getName(this->state) << " Reply for URL " << reply->url()
                           << " finished.");

  if (this->state == State::Disconnected || this->state == State::Connected)
  {
    DEBUG_DOWNNLOAD(
      "Got a reply although we not in a state where we are expecting one. Ignore it.");
    return;
  }

  const auto connectionEncrypted =
    reply->attribute(QNetworkRequest::ConnectionEncryptedAttribute).toBool();
  if (reply->error() != QNetworkReply::NoError || !connectionEncrypted)
  {
    QTextStream errorMessage;
    errorMessage << "Error ";
    if (connectionEncrypted)
      errorMessage << "Error The connection was not encrypted.";
    else
      errorMessage << "Error "
                   << QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(reply->error())
                   << " (" << reply->error() << ")";

    this->state =
      this->state == State::EstablishingConnection ? State::Disconnected : State::Connected;

    DEBUG_DOWNNLOAD(*errorMessage.string());
    emit downloadFinished({"", *errorMessage.string()});
  }

  if (!connectionEncrypted)
  {
    DEBUG_DOWNNLOAD("Error. The connection was not encrypted.");
    this->state =
      this->state == State::EstablishingConnection ? State::Disconnected : State::Connected;
    return;
  }

  if (this->state == State::EstablishingConnection)
  {
    DEBUG_DOWNNLOAD("Connection established successfully");
    this->state = State::Connected;
    this->startDownloadIfAlreadyConnected();
  }
  else if (this->state == State::DownloadingFile)
  {
    const auto downloadedData = reply->readAll();

    // Here, some check could go that checks the MD5 sum.
    // However, we got the file from a secure connection from our github server. So I don't know
    // if this check would really add any more security.
    this->state = State::Connected;
    emit downloadFinished({downloadedData, ""});
  }
}

void GithubFileDownloader::sslErrors(QNetworkReply *, const QList<QSslError> &errors)
{
  QTextStream message;

  message << "SSL Error \n";
  for (auto s : errors)
  {
    message << s.errorString();
    message << s.certificate().toText();
    for (const auto name : s.certificate().subjectAlternativeNames())
      message << name;
  }
  DEBUG_DOWNNLOAD(message.string());

  this->state = State::Disconnected;
  emit downloadFinished({"", *message.string()});
}

void GithubFileDownloader::networkReplyDownloadProgress(qint64 bytesSent, qint64 bytesTotal)
{
}

} // namespace update