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

// Own includes
#include "plistparser.h"

// Qt includes
#include <QDomNode>
#include <QDomDocument>
#include <QDateTime>
#include <QDebug>

QVariant PListParser::parsePList(QIODevice *device) {
	QVariantMap result;
	QDomDocument doc;
	QString errorMessage;
	int errorLine;
	int errorColumn;
	bool success = doc.setContent(device, false, &errorMessage, &errorLine, &errorColumn);
	if (!success) {
		qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "PListParser Warning: Could not parse PList file!";
		qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error message: " << errorMessage;
		qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error line: " << errorLine;
		qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error column: " << errorColumn;
		return result;
	}
	QDomElement root = doc.documentElement();
	if (root.attribute(QStringLiteral("version"), QStringLiteral("1.0")) != QLatin1String("1.0")) {
		qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "PListParser Warning: plist is using an unknown format version, parsing might fail unexpectedly";
	}
	return parseElement(root.firstChild().toElement());
}

QVariant PListParser::parseElement(const QDomElement &e) {
	QString tagName = e.tagName();
	QVariant result;
	if (tagName == QLatin1String("dict")) {
		result = parseDictElement(e);
	}
	else if (tagName == QLatin1String("array")) {
		result = parseArrayElement(e);
	}
	else if (tagName == QLatin1String("string")) {
		result = e.text();
	}
	else if (tagName == QLatin1String("data")) {
		result = QByteArray::fromBase64(e.text().toUtf8());
	}
	else if (tagName == QLatin1String("integer")) {
		result = e.text().toInt();
	}
	else if (tagName == QLatin1String("real")) {
		result = e.text().toFloat();
	}
	else if (tagName == QLatin1String("true")) {
		result = true;
	}
	else if (tagName == QLatin1String("false")) {
		result = false;
	}
	else if (tagName == QLatin1String("date")) {
		result = QDateTime::fromString(e.text(), Qt::ISODate);
	}
	else {
		qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "PListParser Warning: Invalid tag found: " << e.tagName() << e.text();
	}
	return result;
}

QVariantList PListParser::parseArrayElement(const QDomElement& element) {
	QVariantList result;
	QDomNodeList children = element.childNodes();
	for (int i = 0; i < children.count(); i++) {
		QDomNode child = children.at(i);
		QDomElement e = child.toElement();
		if (!e.isNull()) {
			result.append(parseElement(e));
		}
	}
	return result;
}

QVariantMap PListParser::parseDictElement(const QDomElement& element) {
	QVariantMap result;
	QDomNodeList children = element.childNodes();
	QString currentKey;
	for (int i = 0; i < children.count(); i++) {
		QDomNode child = children.at(i);
		QDomElement e = child.toElement();
		if (!e.isNull()) {
			QString tagName = e.tagName();
			if (tagName == QLatin1String("key")) {
				currentKey = e.text();
			}
			else if (!currentKey.isEmpty()) {
				result[currentKey] = parseElement(e);
			}
		}
	}
	return result;
}
