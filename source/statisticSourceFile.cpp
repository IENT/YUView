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

#include "statisticSourceFile.h"

#include <QFileInfo>
#include <QDir>
#include <QtConcurrent>
#include <iostream>

statisticSourceFile::statisticSourceFile(QString fileName)
{
	// Open the input file for reading
	p_srcFile = new QFile(fileName);
	p_srcFile->open(QIODevice::ReadOnly);
	p_srcFileName = fileName;
	p_frameRate = 1;
	p_nrFrames = -1;

	// get some more information from file
	QStringList components = fileName.split(QDir::separator());
	p_FileName = components.last();

	QFileInfo fileInfo(fileName);
	p_filePath = fileInfo.path();
	p_createdtime = fileInfo.created().toString("yyyy-MM-dd hh:mm:ss");
	p_modifiedtime = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
	p_fileSize = fileInfo.size();

	bFileSortedByPOC = false;

	// Read the statistics file header
	readHeaderFromFile();

	// Run the parsing of the file in the backfround
	p_cancelBackgroundParser = false;
	p_backgroundParserFuture = QtConcurrent::run(this, &statisticSourceFile::readFrameAndTypePositionsFromFile);
}

statisticSourceFile::~statisticSourceFile()
{
	// The statisticSourceFile object is being deleted.
	// Check if the background thread is still running.
	if (p_backgroundParserFuture.isRunning())
	{
		// signal to background thread that we want to cancel the processing
		p_cancelBackgroundParser = true;

		p_backgroundParserFuture.waitForFinished();
	}
}

/** The background task that parsese the file and extracts the exact file positions
* where a new frame or a new type starts. If the user then later requests this type/POC
* we can directly jump there and parse the actual information. This way we don't have to
* scan the whole file which can get very slow for large files.
*
* This function might emit the objectInformationChanged() signal if something went wrong,
* setting the error message, or if parsing finished successfully.
*/
void statisticSourceFile::readFrameAndTypePositionsFromFile()
{
	try {
		// Open the file.
		QFile inputFile(p_srcFileName);
		if (!inputFile.open(QIODevice::ReadOnly))
			return;

		int lastPOC = INT_INVALID;
		int lastType = INT_INVALID;
		int numFrames = 0;
		qint64 nextSignalAtByte = 0;
		while (!inputFile.atEnd() && !p_cancelBackgroundParser)
		{
			qint64 lineStartPos = inputFile.pos();

			// read one line
			QByteArray aLineByteArray = inputFile.readLine();
			QString aLine(aLineByteArray);

			// get components of this line
			QStringList rowItemList = parseCSVLine(aLine, ';');

			// ignore empty stuff
			if (rowItemList[0].isEmpty())
				continue;

			// ignore headers
			if (rowItemList[0][0] == '%')
				continue;

			// check for POC/type information
			int poc = rowItemList[0].toInt();
			int typeID = rowItemList[5].toInt();

			if (lastType == -1 && lastPOC == -1)
			{
				// First POC/type line
				p_pocTypeStartList[poc][typeID] = lineStartPos;
				lastType = typeID;
				lastPOC = poc;
				numFrames++;
				p_nrFrames = numFrames;
			}
			else if (typeID != lastType && poc == lastPOC)
			{
				// we found a new type but the POC stayed the same.
				// This seems to be an interleaved file
				// Check if we already collected a start position for this type
				bFileSortedByPOC = true;
				lastType = typeID;
				if (p_pocTypeStartList[poc].contains(typeID))
					// POC/type start position already collected
					continue;
				p_pocTypeStartList[poc][typeID] = lineStartPos;
			}
			else if (poc != lastPOC)
			{
				// We found a new POC
				lastPOC = poc;
				lastType = typeID;
				if (bFileSortedByPOC)
				{
					// There must not be a start position for any type with this POC already.
					if (p_pocTypeStartList.contains(poc))
						throw "The data for each POC must be continuous in an interleaved statistics file.";
				}
				else
				{
					// There must not be a start position for this POC/type already.
					if (p_pocTypeStartList.contains(poc) && p_pocTypeStartList[poc].contains(typeID))
						throw "The data for each typeID must be continuous in an non interleaved statistics file.";
				}
				p_pocTypeStartList[poc][typeID] = lineStartPos;

				// update number of frames
				if (poc + 1 > numFrames)
				{
					numFrames = poc + 1;
					p_nrFrames = numFrames;
				}
				// Update after parsing 5Mb of the file
				if (lineStartPos > nextSignalAtByte)
				{
					// Set progress text
					int percent = (int)((double)lineStartPos * 100 / (double)p_fileSize);
					p_status = QString("Parsing (") + QString::number(percent) + QString("%) ...");
					emit signal_statisticInformationChanged();
					nextSignalAtByte = lineStartPos + 5000000;
				}
			}
			// typeID and POC stayed the same
			// do nothing
		}

		// Parsing complete
		p_status = "OK";
		emit signal_statisticInformationChanged();

		inputFile.close();

	} // try
	catch (const char * str) {
		std::cerr << "Error while parsing meta data: " << str << '\n';
		setErrorState(QString("Error while parsing meta data: ") + QString(str));
		emit signal_statisticInformationChanged();
		return;
	}
	catch (...) {
		std::cerr << "Error while parsing meta data.";
		setErrorState(QString("Error while parsing meta data."));
		emit signal_statisticInformationChanged();
		return;
	}

	return;
}

void statisticSourceFile::readHeaderFromFile()
{
	try {
		if (!p_srcFile->isOpen())
			return;
		
		// cleanup old types
		p_statsTypeList.clear();

		// scan headerlines first
		// also count the lines per Frame for more efficient memory allocation
		// if an ID is used twice, the data of the first gets overwritten
		bool typeParsingActive = false;
		StatisticsType aType;

		while (!p_srcFile->atEnd())
		{
			// read one line
			QByteArray aLineByteArray = p_srcFile->readLine();
			QString aLine(aLineByteArray);

			// get components of this line
			QStringList rowItemList = parseCSVLine(aLine, ';');

			if (rowItemList[0].isEmpty())
				continue;

			// either a new type or a line which is not header finishes the last type
			if (((rowItemList[1] == "type") || (rowItemList[0][0] != '%')) && typeParsingActive)
			{
				// last type is complete
				p_statsTypeList.append(aType);

				// start from scratch for next item
				aType = StatisticsType();
				typeParsingActive = false;

				// if we found a non-header line, stop here
				if (rowItemList[0][0] != '%')
					return;
			}

			if (rowItemList[1] == "type")   // new type
			{
				aType.typeID = rowItemList[2].toInt();

				aType.readFromRow(rowItemList); // get remaining info from row
				typeParsingActive = true;
			}
			else if (rowItemList[1] == "mapColor")
			{
				int id = rowItemList[2].toInt();

				// assign color
				unsigned char r = (unsigned char)rowItemList[3].toInt();
				unsigned char g = (unsigned char)rowItemList[4].toInt();
				unsigned char b = (unsigned char)rowItemList[5].toInt();
				unsigned char a = (unsigned char)rowItemList[6].toInt();
				aType.colorMap[id] = QColor(r, g, b, a);
			}
			else if (rowItemList[1] == "range")
			{
				aType.colorRange = new ColorRange(rowItemList);
			}
			else if (rowItemList[1] == "defaultRange")
			{
				aType.colorRange = new DefaultColorRange(rowItemList);
			}
			else if (rowItemList[1] == "vectorColor")
			{
				unsigned char r = (unsigned char)rowItemList[2].toInt();
				unsigned char g = (unsigned char)rowItemList[3].toInt();
				unsigned char b = (unsigned char)rowItemList[4].toInt();
				unsigned char a = (unsigned char)rowItemList[5].toInt();
				aType.vectorColor = QColor(r, g, b, a);
			}
			else if (rowItemList[1] == "gridColor")
			{
				unsigned char r = (unsigned char)rowItemList[2].toInt();
				unsigned char g = (unsigned char)rowItemList[3].toInt();
				unsigned char b = (unsigned char)rowItemList[4].toInt();
				unsigned char a = 255;
				aType.gridColor = QColor(r, g, b, a);
			}
			else if (rowItemList[1] == "scaleFactor")
			{
				aType.vectorSampling = rowItemList[2].toInt();
			}
			else if (rowItemList[1] == "scaleToBlockSize")
			{
				aType.scaleToBlockSize = (rowItemList[2] == "1");
			}
			else if (rowItemList[1] == "seq-specs")
			{
				QString seqName = rowItemList[2];
				QString layerId = rowItemList[3];
				// For now do nothing with this information.
				// Show the file name for this item instead.
				int width = rowItemList[4].toInt();
				int height = rowItemList[5].toInt();
				if (width > 0 && height > 0)
					p_size = QSize(width, height);
				if (rowItemList[6].toDouble() > 0.0)
					p_frameRate = rowItemList[6].toDouble();
			}
		}

	} // try
	catch (const char * str) {
		std::cerr << "Error while parsing meta data: " << str << '\n';
		setErrorState(QString("Error while parsing meta data: ") + QString(str));
		return;
	}
	catch (...) {
		std::cerr << "Error while parsing meta data.";
		setErrorState(QString("Error while parsing meta data."));
		return;
	}

	return;
}

void statisticSourceFile::loadStatisticToCache(int frameIdx, int typeID)
{
	try {
		if (!p_srcFile->isOpen())
			return;

		StatisticsItem anItem;
		QTextStream in(p_srcFile);

        if (!p_pocTypeStartList.contains(frameIdx) || !p_pocTypeStartList[frameIdx].contains(typeID))
            return;
		qint64 startPos = p_pocTypeStartList[frameIdx][typeID];
		if (bFileSortedByPOC)
		{
			// If the statistics file is sorted by POC we have to start at the first entry of this POC and parse the 
			// file until another POC is encountered. If this is not done, some information from a different typeID 
			// could be ignored during parsing.

			// Get the position of the first line with the given frameIdx
			startPos = std::numeric_limits<qint64>::max();
			QMap<int, qint64>::iterator it;
			for (it = p_pocTypeStartList[frameIdx].begin(); it != p_pocTypeStartList[frameIdx].end(); ++it)
				if (it.value() < startPos)
					startPos = it.value();
		}

		// fast forward
		in.seek(startPos);

		while (!in.atEnd())
		{
			// read one line
			QString aLine = in.readLine();

			// get components of this line
			QStringList rowItemList = parseCSVLine(aLine, ';');

			if (rowItemList[0].isEmpty())
				continue;

			int poc = rowItemList[0].toInt();
			int type = rowItemList[5].toInt();

			// if there is a new poc, we are done here!
			if (poc != frameIdx)
				break;
			// if there is a new type and this is a non interleaved file, we are done here.
			if (!bFileSortedByPOC && type != typeID)
				break;

			int value1 = rowItemList[6].toInt();
			int value2 = (rowItemList.count() >= 8) ? rowItemList[7].toInt() : 0;

			int posX = rowItemList[1].toInt();
			int posY = rowItemList[2].toInt();
			int width = rowItemList[3].toUInt();
			int height = rowItemList[4].toUInt();

			// Check if block is within the image range
			if (posX + width > p_size.width() || posY + height > p_size.height()) {
				// Block not in image. Warn about this.
				p_info = "Warning: A block in the statistic file is outside of the specified image area.";
			}

			StatisticsType *statsType = getStatisticsType(type);
			Q_ASSERT_X(statsType != NULL, "StatisticsObject::readStatisticsFromFile", "Stat type not found.");
			anItem.type = ((statsType->visualizationType == colorMapType) || (statsType->visualizationType == colorRangeType)) ? blockType : arrowType;

			anItem.positionRect = QRect(posX, posY, width, height);

			anItem.rawValues[0] = value1;
			anItem.rawValues[1] = value2;
			anItem.color = QColor();

			if (statsType->visualizationType == colorMapType)
			{
				ColorMap colorMap = statsType->colorMap;
				anItem.color = colorMap[value1];
			}
			else if (statsType->visualizationType == colorRangeType)
			{
				if (statsType->scaleToBlockSize)
					anItem.color = statsType->colorRange->getColor((float)value1 / (float)(anItem.positionRect.width() * anItem.positionRect.height()));
				else
					anItem.color = statsType->colorRange->getColor((float)value1);
			}
			else if (statsType->visualizationType == vectorType)
			{
				// find color
				anItem.color = statsType->vectorColor;

				// calculate the vector size
				anItem.vector[0] = (float)value1 / statsType->vectorSampling;
				anItem.vector[1] = (float)value2 / statsType->vectorSampling;
			}

			// set grid color. if unset for this type, use color of type for grid, too
			if (statsType->gridColor.isValid())
			{
				anItem.gridColor = statsType->gridColor;
			}
			else
			{
				anItem.gridColor = anItem.color;
			}

			p_statsCache[poc][type].append(anItem);
		}
		

	} // try
	catch (const char * str) {
		std::cerr << "Error while parsing: " << str << '\n';
		setErrorState(QString("Error while parsing meta data: ") + QString(str));
		return;
	}
	catch (...) {
		std::cerr << "Error while parsing.";
		setErrorState(QString("Error while parsing meta data."));
		return;
	}

	return;
}

QStringList statisticSourceFile::parseCSVLine(QString line, char delimiter)
{
	// first, trim newline and whitespaces from both ends of line
	line = line.trimmed().replace(" ", "");

	// now split string with delimiter
	return line.split(delimiter);
}

void statisticSourceFile::setErrorState(QString sError)
{
	// The statistics file is invalid. Set the error message.
	p_status = sError;

	if (p_backgroundParserFuture.isRunning())
	{
		// signal to background thread that we want to cancel the processing
		p_cancelBackgroundParser = true;

		p_backgroundParserFuture.waitForFinished();
	}
}
