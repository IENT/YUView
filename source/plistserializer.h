/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

// Qt includes
#include <QIODevice>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QDomElement>
#include <QDomDocument>
#include <QString>

class PListSerializer {
public:
	static QString toPList(const QVariant &variant);
private:
	static QDomElement serializeElement(QDomDocument &doc, const QVariant &variant);
	static QDomElement serializeMap(QDomDocument &doc, const QVariantMap &map);
	static QDomElement serializeList(QDomDocument &doc, const QVariantList &list);
};

