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

#include "fileInfoWidget.h"
#include "playlistItem.h"
#include <assert.h>

/* The file info group box can display information on a file (or any other displayobject).
 * If you provide a list of QString tuples, this class will fill a grid layout with the 
 * corresponding labels.
 */
FileInfoWidget::FileInfoWidget(QWidget *parent) : QWidget(parent)
{
  infoLayout = new QGridLayout;
  setLayout(infoLayout);
  
  currentItem1 = NULL;
  currentItem2 = NULL;

  // Load the warning icon
  warningIcon = QPixmap(":img_warning.png");

  // Clear the file info dock
  setFileInfo();
}

FileInfoWidget::~FileInfoWidget()
{
  delete infoLayout;
}

void FileInfoWidget::updateFileInfo(bool redraw)
{
  Q_UNUSED(redraw);

  // Only show the info of the first selection
  // TODO: why not show both?
  if (currentItem1)
    setFileInfo( currentItem1->getInfoTitel(), currentItem1->getInfoList() );
  else
    setFileInfo();
}

void FileInfoWidget::currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2)
{
  currentItem1 = item1;
  currentItem2 = item2;

  updateFileInfo();
}

// ------- Private ---------

void FileInfoWidget::setFileInfo()
{
  // Clear the title of the dock widget (our parent)
  if (parentWidget())
    parentWidget()->setWindowTitle(FILEINFOWIDGET_DEFAULT_WINDOW_TITEL);

  // Clear the grid layout
  foreach(QLabel *l, labelList) {
    infoLayout->removeWidget(l);
    delete l;
  }
  labelList.clear();
  nrLabelPairs = 0;
}

void FileInfoWidget::setFileInfo(QString fileInfoTitle, QList<infoItem> fileInfoList)
{
  // Set the title of the dock widget (our parent)
  if (parentWidget())
    parentWidget()->setWindowTitle(fileInfoTitle);

  if (fileInfoList.count() == nrLabelPairs) {
    // The correct number of label pairs is already in the groupBox.
    // No need to delete all items and reattach them. Just update the text.
    for (int i = 0; i < nrLabelPairs; i++)
    {
      assert(nrLabelPairs * 2 == labelList.count());

      // Set left text or icon
      if (fileInfoList[i].name == "Warning")
        labelList[i*2]->setPixmap(warningIcon);
      else
        labelList[i*2]->setText(fileInfoList[i].name);

      // Set "value" text
      labelList[i*2+1]->setText(fileInfoList[i].text);
      if (!fileInfoList[i].toolTip.isEmpty())
        labelList[i*2]->setToolTip(fileInfoList[i].toolTip);
    }
  }
  else {
    // Update the grid layout. Delete all the labels and add as many new ones as necessary.

    // Clear the grid layout
    foreach(QLabel *l, labelList) {
      infoLayout->removeWidget(l);
      delete l;
    }
    labelList.clear();

    // For each item in the list add a two labels to the grid layout
    int i = 0;
    foreach(infoItem info, fileInfoList) 
    {
      
      // Create labels
      QLabel *newTextLabel = new QLabel();
      if (info.name == "Warning")
        newTextLabel->setPixmap(warningIcon);
      else
        newTextLabel->setText(info.name);
      if (!fileInfoList[i].toolTip.isEmpty())
        newTextLabel->setToolTip(fileInfoList[i].toolTip);
      QLabelElided *newValueLabel = new QLabelElided(info.text);
      newValueLabel->setWordWrap(true);

      // Add to grid
      infoLayout->addWidget(newTextLabel, i, 0);
      infoLayout->addWidget(newValueLabel, i, 1);

      // Set row stretch to 0
      infoLayout->setRowStretch(i, 0);

      i++;

      // Add to list of labels
      labelList.append(newTextLabel);
      labelList.append(newValueLabel);
    }

    infoLayout->setColumnStretch(1, 1);	///< Set the second column to strectch
    infoLayout->setRowStretch(i, 1);		///< Set the last rwo to strectch

    nrLabelPairs = i;
  }
}
