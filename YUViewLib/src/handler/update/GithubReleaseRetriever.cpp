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

#include "GithubReleaseRetriever.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QNetworkReply>

#define UPDATER_GITHUBRELEASETAG_OUTPUT 0
#if UPDATER_GITHUBRELEASETAG_OUTPUT && !NDEBUG
#include <QDebug>
#include <source_location>
#define DEBUG_GETTAG(f)                                                                            \
  do                                                                                               \
  {                                                                                                \
    qDebug() << std::source_location::current().function_name() << " " << f;                       \
  } while (0)
#else
#define DEBUG_GETTAG(f) ((void)0)
#endif

namespace update
{

namespace
{

constexpr auto RELEASE_URL = "https://api.github.com/repos/IENT/YUView/releases";

std::optional<QString> parseGithubReleaseResponse(QByteArray data)
{
  const auto json      = QJsonDocument::fromJson(data);
  const auto jsonArray = json.array();
  if (jsonArray.size() == 0)
  {
    DEBUG_GETTAG("Error parsing JSON. Array is empty.");
    return {};
  }

  const auto firstObject = jsonArray[0].toObject();
  if (!firstObject.contains("tag_name"))
  {
    DEBUG_GETTAG(
      "The returned JSON object does not contain version information on the latest release.");
    return {};
  }

  const auto serverVersion = firstObject["tag_name"].toString();
  if (serverVersion.isEmpty())
  {
    DEBUG_GETTAG("The tag_name is empty.");
    return {};
  }

  return serverVersion;
}

} // namespace

GithubReleaseRetriever::GithubReleaseRetriever()
{
  connect(&networkManager,
          &QNetworkAccessManager::finished,
          this,
          &GithubReleaseRetriever::replyFinished);
  connect(
    &networkManager, &QNetworkAccessManager::sslErrors, this, &GithubReleaseRetriever::sslErrors);
}

void GithubReleaseRetriever::startGettingOfReleaseTag()
{
  if (this->state != State::Idle)
  {
    DEBUG_GETTAG("Invalid state. Getting of tag already in progress.");
    return;
  }

  this->state = State::GettingReleaseTag;

  QNetworkRequest request{QUrl(RELEASE_URL)};
  request.setHeader(QNetworkRequest::UserAgentHeader, "YUView");
  request.setRawHeader("Accept", "application/vnd.github+json");
  request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setTransferTimeout(15'000);

  const auto githubToken = qgetenv("GITHUB_TOKEN");
  if (!githubToken.isEmpty())
    request.setRawHeader("Authorization", "Bearer " + githubToken);

  this->networkManager.get(request);
}

void GithubReleaseRetriever::replyFinished(QNetworkReply *reply)
{
  DEBUG_GETTAG("Reply for URL " << reply->url() << " finished.");

  if (this->state != State::GettingReleaseTag)
  {
    DEBUG_GETTAG("Got a response but we are not getting a tag currently. Ignoring it.");
    return;
  }

  if (reply->error() != QNetworkReply::NoError)
  {
    DEBUG_GETTAG("Error " << reply->error());
    this->state = State::Idle;
    const auto replyErrorName =
      QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(reply->error());
    emit gettingReleaseTagFinished(
      {"", QString("Network reply error: %1 (%2)").arg(reply->error()).arg(replyErrorName)});
    return;
  }

  const auto downloadEncrypted =
    reply->attribute(QNetworkRequest::ConnectionEncryptedAttribute).toBool();
  if (!downloadEncrypted)
  {
    DEBUG_GETTAG("Error. The connection was not encrypted.");
    this->state = State::Idle;
    emit gettingReleaseTagFinished({"", "Connection was not encrypted."});
    return;
  }

  this->state = State::Idle;
  if (const auto tag = parseGithubReleaseResponse(reply->readAll()))
    emit gettingReleaseTagFinished({*tag, ""});
  else
    emit gettingReleaseTagFinished({"", QString("Got Invalid response: %1").arg(reply->readAll())});
}

void GithubReleaseRetriever::sslErrors(QNetworkReply *, const QList<QSslError> &errors)
{
  DEBUG_GETTAG("Error");
  QTextStream message;

  message << "SSL Error \n";
  for (auto s : errors)
  {
    message << s.errorString();
    message << s.certificate().toText();
    for (const auto name : s.certificate().subjectAlternativeNames())
      message << name;
  }
  DEBUG_GETTAG(message.string());

  this->state = State::Idle;
  emit gettingReleaseTagFinished({"", *message.string()});
}

} // namespace update
