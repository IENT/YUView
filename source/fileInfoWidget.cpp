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
FileInfoWidget::FileInfoWidget(QWidget *parent) :
  QWidget(parent),
  infoLayout(this)
{
  // Load the warning icon
  warningIcon = QPixmap(":img_warning.png");

  // Clear the file info dock
  setFileInfo();
}

FileInfoWidget::~FileInfoWidget()
{
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
  clearLayout();
}

void FileInfoWidget::clearLayout()
{
  foreach(QLabel *l, nameLabelList)
  {
    infoLayout.removeWidget(l);
    delete l;
  }
  foreach(QPushButton *l, valueButtonMap)
  {
    infoLayout.removeWidget(l);
    delete l;
  }
  foreach(QLabelElided *l, valueLabelMap)
  {
    infoLayout.removeWidget(l);
    delete l;
  }
  nameLabelList.clear();
  valueButtonMap.clear();
  valueLabelMap.clear();
  oldFileInfoList.clear();
}

void FileInfoWidget::setFileInfo(QString fileInfoTitle, QList<infoItem> fileInfoList)
{
  // Set the title of the dock widget (our parent)
  if (parentWidget())
    parentWidget()->setWindowTitle(fileInfoTitle);

  // First check if we have to delete all items in the lists and recreate them.
  bool recreate = true;
  if (oldFileInfoList.count() == fileInfoList.count())
  {
    // Same number of items. Thats a good sign. Go through all items and see if the type (button or not) of each item is identical.
    recreate = false;
    for (int i = 0; i < fileInfoList.count(); i++)
    {
      if (fileInfoList[i].button != oldFileInfoList[i].button)
        recreate = true;
    }
  }

  if (!recreate)
  {
    // No need to delete all items and reattach them. Just update the texts.
    for (int i = 0; i < fileInfoList.count(); i++)
    {
      assert(nameLabelList.count() == fileInfoList.count() && (valueButtonMap.count() + valueLabelMap.count()) == fileInfoList.count());

      // Set left text or icon
      if (fileInfoList[i].name == "Warning")
        nameLabelList[i]->setPixmap(warningIcon);
      else
        nameLabelList[i]->setText(fileInfoList[i].name);

      if (fileInfoList[i].button)
      {
        if (valueButtonMap.contains(i))
        {
          valueButtonMap[i]->setText(fileInfoList[i].text);
          if (!fileInfoList[i].toolTip.isEmpty())
            valueButtonMap[i]->setToolTip(fileInfoList[i].toolTip);
        }
      }
      else
      {
        if (valueLabelMap.contains(i))
        {
          valueLabelMap[i]->setText(fileInfoList[i].text);
          if (!fileInfoList[i].toolTip.isEmpty())
            valueLabelMap[i]->setToolTip(fileInfoList[i].toolTip);
        }
      }
    }
  }
  else 
  {
    // Update the grid layout. Delete all the labels and add as many new ones as necessary.

    // Clear the grid layout
    clearLayout();

    // For each item in the list add a two labels to the grid layout
    int i = 0;
    foreach(infoItem info, fileInfoList) 
    {
      // Create a new name label for the first column ...
      QLabel *newTextLabel = new QLabel();
      if (info.name == "Warning")
        newTextLabel->setPixmap(warningIcon);
      else
        newTextLabel->setText(info.name);
      // ... set the tooltip ...
      if (!fileInfoList[i].toolTip.isEmpty())
        newTextLabel->setToolTip(fileInfoList[i].toolTip);
      // ... and add it to the grid
      infoLayout.addWidget(newTextLabel, i, 0);
      nameLabelList.append(newTextLabel);

      if (info.button)
      {
        // Create a new button, connect it and add it to the layout and list
        QPushButton *newButton = new QPushButton(info.text);
        connect(newButton, SIGNAL(clicked()), this, SLOT(fileInfoButtonClicked()));
        infoLayout.addWidget(newButton, i, 1);
        valueButtonMap.insert(i, newButton);
      }
      else
      {
        QLabelElided *newValueLabel = new QLabelElided(info.text);
        newValueLabel->setWordWrap(true);

        infoLayout.addWidget(newValueLabel, i, 1);
        valueLabelMap.insert(i, newValueLabel);
      }

      // Set row stretch to 0
      infoLayout.setRowStretch(i, 0);

      i++;
    }

    infoLayout.setColumnStretch(1, 1); ///< Set the second column to strectch
    infoLayout.setRowStretch(i, 1);    ///< Set the last rwo to strectch
  }

  oldFileInfoList = fileInfoList;
}

void FileInfoWidget::fileInfoButtonClicked()
{
  // Find out which button was clicked
  QObject *sender = QObject::sender();
  QMapIterator<int, QPushButton*> i(valueButtonMap);
  while (i.hasNext()) 
  {
    i.next();
    if (i.value() == sender)
    {
      // Call the callback function for the button
      currentItem1->infoListButtonPressed(i.key());
      return;
    }
  }
}
