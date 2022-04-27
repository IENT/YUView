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

#include "YUViewDomElement.h"

QString YUViewDomElement::findChildValue(const QString &tagName) const
{
  for (auto n = firstChild(); !n.isNull(); n = n.nextSibling())
  {
    if (n.isElement() && n.toElement().tagName() == tagName)
      return n.toElement().text();
  }
  return {};
}

std::pair<QString, QStringPairList>
YUViewDomElement::findChildValueWithAttributes(const QString &tagName) const
{
  for (auto n = firstChild(); !n.isNull(); n = n.nextSibling())
  {
    if (n.isElement() && n.toElement().tagName() == tagName)
    {
      QStringPairList attributeList;
      auto            attributes = n.toElement().attributes();
      for (int i = 0; i < attributes.length(); i++)
      {
        auto name = attributes.item(i).nodeName();
        auto val  = attributes.item(i).nodeValue();
        attributeList.append(QStringPair(name, val));
      }
      auto text = n.toElement().text();
      return {text, attributeList};
    }
  }
  return {};
}

int YUViewDomElement::findChildValueInt(const QString &tagName, int defaultValue) const
{
  auto r = findChildValue(tagName);
  return r.isEmpty() ? defaultValue : r.toInt();
};

double YUViewDomElement::findChildValueDouble(const QString &tagName, double defaultValue) const
{
  auto r = findChildValue(tagName);
  return r.isEmpty() ? defaultValue : r.toDouble();
};

void YUViewDomElement::appendProperiteChild(const QString &        type,
                                            const QString &        name,
                                            const QStringPairList &attributes)
{
  auto newChild = ownerDocument().createElement(type);
  newChild.appendChild(ownerDocument().createTextNode(name));
  for (int i = 0; i < attributes.length(); i++)
    newChild.setAttribute(attributes[i].first, attributes[i].second);
  appendChild(newChild);
}

void YUViewDomElement::setAttribute(const std::string &name, const std::string &value)
{
  QDomElement::setAttribute(QString::fromStdString(name), QString::fromStdString(value));
}

void YUViewDomElement::appendProperiteChild(const std::string &type, const std::string &name)
{
  auto newChild = ownerDocument().createElement(QString::fromStdString(type));
  newChild.appendChild(ownerDocument().createTextNode(QString::fromStdString(name)));
  appendChild(newChild);
}
