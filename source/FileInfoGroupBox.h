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

#ifndef FILEINFOFROUPBOX_H
#define FILEINFOFROUPBOX_H

#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>

typedef QPair<QString, QString> fileInfoItem;

class FileInfoGroupBox : public QGroupBox
{
	Q_OBJECT

public:
	FileInfoGroupBox(QWidget *parent);
	~FileInfoGroupBox();

	void setFileInfo(QString fileInfoTitle, QList<fileInfoItem> fileInfoList);

protected:
	QGridLayout *p_gridLayout;

	// The list containing pointers to all labels in the grid layout.
	QList<QLabel*> p_labelList;

	int p_nrLabelPairs;		///< The number of label pairs currently in the groupBox
};

#endif