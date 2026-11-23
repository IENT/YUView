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

#include <common/Testing.h>

#define private public
#include <handler/update/UpdateHandler.h>
#undef private

#include <QNetworkReply>

namespace update::test
{

class TestNetworkReply : public QNetworkReply
{
public:
  explicit TestNetworkReply(const QByteArray &content, QObject *parent = nullptr)
      : QNetworkReply(parent), content(content)
  {
    open(QIODevice::ReadOnly);
  }

  void abort() override {}

  bool isSequential() const override { return true; }

protected:
  qint64 readData(char *data, qint64 maxSize) override
  {
    if (position >= content.size())
      return -1;

    const auto size = qMin(maxSize, qint64(content.size()) - position);
    memcpy(data, content.constData() + position, size_t(size));
    position += size;
    return size;
  }

private:
  QByteArray content;
  qint64     position{};
};

TEST(UpdateHandlerTest, testNoUpdateResponse_shouldSetHandlerToIdle)
{
  UpdateHandler handler(nullptr, false);
  handler.updaterStatus    = UpdateHandler::UpdateStatus::Checking;
  handler.userCheckRequest = false;

  TestNetworkReply reply(QByteArray("[{\"tag_name\":\"v1\"}]"));
  handler.replyFinished(&reply);

  EXPECT_EQ(handler.updaterStatus, UpdateHandler::UpdateStatus::Idle);
}

} // namespace update::test
