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

#include "playlistItemOverlay.h"

#include "playlistItemStatisticsFile.h"

#include <cmath>
#include <limits>
#include <QPainter>
#include <QPointer>

#include "common/functions.h"

#define PLAYLISTITEMOVERLAY_DEBUG 0
#if PLAYLISTITEMOVERLAY_DEBUG && !NDEBUG
#include <QDebug>
#define DEBUG_OVERLAY qDebug
#else
#define DEBUG_OVERLAY(fmt,...) ((void)0)
#endif

#define CUSTOM_POS_MAX 100000

playlistItemOverlay::playlistItemOverlay() :
  playlistItemContainer("Overlay Item")
{
  setIcon(0, functions::convertIcon(":img_overlay.png"));
  // Enable dropping for overlay objects. The user can drop items here to draw them as an overlay.
  setFlags(flags() | Qt::ItemIsDropEnabled);

  // This text is drawn if there are no child items in the overlay
  infoText = "Please drop some items onto this overlay. All child items will be drawn on top of each other.";

  startEndFrame = indexRange(-1,-1);
}

/* For an overlay item, the info list is just a list of the names of the
 * child elements.
 */
infoData playlistItemOverlay::getInfo() const
{
  infoData info("Overlay Info");

  // Add the size of this playlistItemOverlay
  info.items.append(infoItem("Overlay Size",QString("(%1,%2)").arg(getSize().width()).arg(getSize().height())));

  // Add the sizes of all child items
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = getChildPlaylistItem(i);
    if (childItem)
    {
      QSize childSize = childItem->getSize();
      info.items.append(infoItem(QString("Item %1 size").arg(i),QString("(%1,%2)").arg(childSize.width()).arg(childSize.height())));
    }
  }
  return info;
}

ValuePairListSets playlistItemOverlay::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;

  // The given pixelPos is relative to the bounding rectangle. For every child we have to calculate
  // the relative point within that item.
  QPoint relPoint = boundingRect.topLeft() + pixelPos;

  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = getChildPlaylistItem(i);
    if (childItem)
    {
      // First check if the point is even within the child bounding rectangle
      if (childItemRects[i].contains(relPoint))
      {
        // Calculate the relative pixel position within this child item
        QPoint childPixelPos = relPoint - childItemRects[i].topLeft();

        ValuePairListSets childSets = childItem->getPixelValues(childPixelPos, frameIdx);
        // Append the item id for every set in the child
        for (int j = 0; j < childSets.count(); j++)
        {
          childSets[j].first = QString("Item %1 - %2").arg(i).arg(childSets[j].first);
        }
        newSet.append(childSets);
      }
    }
  }

  return newSet;
}

itemLoadingState playlistItemOverlay::needsLoading(int frameIdx, bool loadRawdata)
{
  // The overlay needs to load if one of the child items needs to load
  for (int i = 0; i < childCount(); i++)
  {
    if (getChildPlaylistItem(i)->needsLoading(frameIdx, loadRawdata) == LoadingNeeded)
    {
      DEBUG_OVERLAY("playlistItemOverlay::needsLoading LoadingNeeded child %s", getChildPlaylistItem(i)->getName().toLatin1().data());
      return LoadingNeeded;
    }
  }
  for (int i = 0; i < childCount(); i++)
  {
    if (getChildPlaylistItem(i)->needsLoading(frameIdx, loadRawdata) == LoadingNeededDoubleBuffer)
    {
      DEBUG_OVERLAY("playlistItemOverlay::needsLoading LoadingNeededDoubleBuffer child %s", getChildPlaylistItem(i)->getName().toLatin1().data());
      return LoadingNeededDoubleBuffer;
    }
  }

  DEBUG_OVERLAY("playlistItemOverlay::needsLoading LoadingNotNeeded");
  return LoadingNotNeeded;
}

void playlistItemOverlay::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData)
{
  DEBUG_OVERLAY("playlistItemOverlay::drawItem frame %d", frameIdx);

  if (childLlistUpdateRequired)
  {
    updateChildList();
    updateCustomPositionGrid();
  }

  if (childCount() == 0)
  {
    playlistItem::drawItem(painter, frameIdx, zoomFactor, drawRawData);
    return;
  }

  // Update the layout if the number of items changedupdateLayout
  updateLayout();

  // Translate to the center of this overlay item
  painter->translate(centerRoundTL(boundingRect) * zoomFactor * -1);

  // Draw all child items at their positions
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = getChildPlaylistItem(i);
    if (childItem)
    {
      QPoint center = centerRoundTL(childItemRects[i]);
      painter->translate(center * zoomFactor);
      childItem->drawItem(painter, frameIdx, zoomFactor, drawRawData);
      painter->translate(center * zoomFactor * -1);
    }
  }

  // Reverse translation to the center of this overlay item
  painter->translate(centerRoundTL(boundingRect) * zoomFactor);
}

QSize playlistItemOverlay::getSize() const
{
  if (childCount() == 0)
    return playlistItemContainer::getSize();

  return boundingRect.size();
}

void playlistItemOverlay::updateLayout(bool onlyIfItemsChanged)
{
  if (childCount() == 0)
  {
    childItemRects.clear();
    childItemsIDs.clear();
    boundingRect = QRect();
    return;
  }

  // Check if the nr/order of items changed
  bool nrItemsChanged = childCount() != childItemRects.count();
  bool itemOrderChanged = false;
  if (!nrItemsChanged)
  {
    for (int i = 0; i < childCount(); i++)
    {
      playlistItem *childItem = getChildPlaylistItem(i);
      if (childItemsIDs[i] != childItem->getID())
      {
        itemOrderChanged = true;
        break;
      }
    }
  }

  if (onlyIfItemsChanged && !nrItemsChanged && !itemOrderChanged)
    return;

  DEBUG_OVERLAY("playlistItemOverlay::updateLayout%s", onlyIfNrItemsChanged ? " onlyIfNrItemsChanged" : "");

  if (nrItemsChanged || itemOrderChanged)
  {
    // Resize the childItems/IDs list
    childItemRects.clear();
    childItemsIDs.clear();
    for (int i = 0; i < childCount(); i++)
    {
      childItemRects.append(QRect());
      playlistItem *childItem = getChildPlaylistItem(i);
      childItemsIDs.append(childItem->getID());
    }
  }

  // Update the layout in all children which are also playlistItemOverlays
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = getChildPlaylistItem(i);
    playlistItemOverlay *childOverlay = dynamic_cast<playlistItemOverlay*>(childItem);
    if (childOverlay)
      childOverlay->updateLayout();
  }

  // The first playlist item is the "root".
  // We will arange all other items relative to this one
  playlistItem *firstItem = getChildPlaylistItem(0);
  boundingRect.setSize(firstItem->getSize());
  boundingRect.moveCenter(QPoint(0,0));

  QRect firstItemRect;
  firstItemRect.setSize(firstItem->getSize());
  firstItemRect.moveCenter(QPoint(0,0));
  childItemRects[0] = firstItemRect;
  DEBUG_OVERLAY("playlistItemOverlay::updateLayout item 0 size (%d,%d) firstItemRect (%d,%d)", firstItem->getSize().width(), firstItem->getSize().height(), firstItemRect.left(), firstItemRect.top());

  QList<int> columns, rows;
  const int nrRowsCols = int(sqrt(childCount() - 1)) + 1;
  if (layoutMode == ARANGE)
  {
    // Before calculating the actual position of each item, we have to pre-calculate 
    // the row/column height/width for the 2D layout
    for (int i=0; i<nrRowsCols; i++)
    {
      columns.append(0);
      rows.append(0);
    }
    for (int i=0; i<childCount(); i++)
    {
      const int r = i / nrRowsCols;
      const int c = i % nrRowsCols;
      playlistItem *p = getChildPlaylistItem(i);
      QSize s = p->getSize();
      if (columns[c] < s.width())
        columns[c] = s.width();
      if (rows[r] < s.height())
        rows[r] = s.height();
    }
  }

  // Align the rest of the items
  DEBUG_OVERLAY("playlistItemOverlay::updateLayout childCount %d layoutMode %d", childCount(), layoutMode);
  for (int i = 1; i < childCount(); i++)
  {
    playlistItem *childItem = getChildPlaylistItem(i);
    if (childItem)
    {
      QSize childSize = childItem->getSize();
      QRect targetRect;
      targetRect.setSize(childSize);
      targetRect.moveCenter(QPoint(0,0));

      if (layoutMode == OVERLAY)
      {
        // Align based on alignment mode (must be between 0 and 8)
        switch (overlayMode)
        {
        case 0:
          targetRect.moveTopLeft(firstItemRect.topLeft()); break;
        case 1:
          targetRect.moveTop(firstItemRect.top()); break;
        case 2:
          targetRect.moveTopRight(firstItemRect.topRight()); break;
        case 3:
          targetRect.moveLeft(firstItemRect.left()); break;
        case 5:
          targetRect.moveRight(firstItemRect.right()); break;
        case 6:
          targetRect.moveBottomLeft(firstItemRect.bottomLeft()); break;
        case 7:
          targetRect.moveBottom(firstItemRect.bottom()); break;
        case 8:
          targetRect.moveBottomRight(firstItemRect.bottomRight()); break;
        default:
          targetRect.moveCenter(QPoint(0,0)); break;
        }
      }
      else if (layoutMode == ARANGE)
      {
        if (arangementMode == 0)
        {
          // 2D
          const int r = i / nrRowsCols;
          const int c = i % nrRowsCols;
          int y = 0;
          for (int i=0; i<r; i++)
            y += rows[i];
          int x = 0;
          for (int i=0; i<c; i++)
            x += columns[i];
          targetRect.moveTopLeft(firstItemRect.topLeft() + QPoint(x, y));
        }
        else if (arangementMode == 1)
        {
          // Side by side
          QPoint newTopLeft = childItemRects[i-1].topRight();
          newTopLeft.setX(newTopLeft.x() + 1);
          targetRect.moveTopLeft(newTopLeft);
        }
        else if (arangementMode == 2)
        {
          // Stacked
          QPoint newTopLeft = childItemRects[i-1].bottomLeft();
          newTopLeft.setY(newTopLeft.y() + 1);
          targetRect.moveTopLeft(newTopLeft);
        }
      }
      else if (layoutMode == CUSTOM)
      {
        // Just use the provided custion positions per item
        QPoint pos = customPositions[i-1];
        targetRect.moveTopLeft(firstItemRect.topLeft() + pos);
      }

      // Set item bounding rectangle
      childItemRects[i] = targetRect;

      DEBUG_OVERLAY("playlistItemOverlay::updateLayout item %d size (%d,%d) alignmentMode %d targetRect (%d,%d)", i, childSize.width(), childSize.height(), alignmentMode, targetRect.left(), targetRect.top());

      // Expand the bounding rectangle
      boundingRect = boundingRect.united(targetRect);
    }
  }
}

void playlistItemOverlay::createPropertiesWidget()
{
  // Absolutely always only call this once
  Q_ASSERT_X(!propertiesWidget, "playlistItemOverlay::createPropertiesWidget", "Always create the properties only once!");

  // Create a new widget and populate it with controls
  propertiesWidget.reset(new QWidget);
  ui.setupUi(propertiesWidget.data());

  // Add the layout
  ui.verticalLayout->insertLayout(0, createPlaylistItemControls());

  // Alignment mode
  ui.comboBoxOverlayMode->addItems(QStringList() << "Top Left" << "Top Center" << "Top Right");
  ui.comboBoxOverlayMode->addItems(QStringList() << "Center Left" << "Center" << "Center Right");
  ui.comboBoxOverlayMode->addItems(QStringList() << "Bottom Left" << "Bottom Center" << "Bottom Right");
  ui.comboBoxOverlayMode->setCurrentIndex(overlayMode);
  
  ui.comboBoxArangementMode->addItems(QStringList() << "2D Grid" << "Side by Side" << "Stacked" );
  ui.comboBoxArangementMode->setCurrentIndex(arangementMode);

  ui.overlayGroupBox->setChecked(layoutMode == OVERLAY);
  ui.arangeGroupBox->setChecked(layoutMode == ARANGE);
  ui.customGroupBox->setChecked(layoutMode == CUSTOM);

  // Create and add the grid layout for the custom positions
  customPositionGrid = new QGridLayout(ui.customGroupBox);
  
  // Add the Container Layout
  ui.verticalLayout->insertLayout(3, createContainerItemControls());

  // Add a spacer item at the end
  ui.verticalLayout->addStretch(1);

  // Connect signals/slots
  connect(ui.overlayGroupBox, &QGroupBox::toggled, this, &playlistItemOverlay::on_overlayGroupBox_toggled);
  connect(ui.arangeGroupBox, &QGroupBox::toggled, this, &playlistItemOverlay::on_arangeGroupBox_toggled);
  connect(ui.customGroupBox, &QGroupBox::toggled, this, &playlistItemOverlay::on_customGroupBox_toggled);
  connect(ui.comboBoxOverlayMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &playlistItemOverlay::slotControlChanged);
  connect(ui.comboBoxArangementMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &playlistItemOverlay::slotControlChanged);
}

void playlistItemOverlay::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  YUViewDomElement d = root.ownerDocument().createElement("playlistItemOverlay");

  // Append the playlist item properties
  playlistItem::appendPropertiesToPlaylist(d);

  // Append the overlay properties
  d.appendProperiteChild("layoutMode", QString::number(layoutMode));
  if (layoutMode == OVERLAY)
    d.appendProperiteChild("overlayMode", QString::number(overlayMode));
  else if (layoutMode == ARANGE)
    d.appendProperiteChild("arangementMode", QString::number(arangementMode));
  else
  {
    assert(layoutMode == CUSTOM);
    for (int i = 1; i < childCount(); i++)
    {
      QPoint p = customPositions[i-1];
      d.appendProperiteChild(QString("ItemPos%1X").arg(i), QString::number(p.x()));
      d.appendProperiteChild(QString("ItemPos%1Y").arg(i), QString::number(p.y()));
    }
  }

  // Append all children
  playlistItemContainer::savePlaylistChildren(d, playlistDir);

  root.appendChild(d);
}

playlistItemOverlay *playlistItemOverlay::newPlaylistItemOverlay(const YUViewDomElement &root, const QString &filePath)
{
  Q_UNUSED(filePath);

  bool oldFormat = (!root.findChildValue("alignmentMode").isEmpty());
  bool newFormat = (!root.findChildValue("layoutMode").isEmpty());
  if (!oldFormat && !newFormat)
    // Error loading. It should be one of these formats
    return nullptr;

  playlistItemOverlay *newOverlay = new playlistItemOverlay();

  if (oldFormat)
  {
    int alignmentMode = root.findChildValue("alignmentMode").toInt();
    int manualAlignmentX = root.findChildValue("manualAlignmentX").toInt();
    int manualAlignmentY = root.findChildValue("manualAlignmentY").toInt();

    // We can not really map the old (and not really pratical) notation
    if (manualAlignmentX != 0 || manualAlignmentY != 0)
    {
      newOverlay->layoutMode = CUSTOM;
      newOverlay->customPositions[0] = QPoint(manualAlignmentX, manualAlignmentY);
    }
    else
    {
      newOverlay->layoutMode = OVERLAY;
      newOverlay->arangementMode = alignmentMode;
    }

    DEBUG_OVERLAY("playlistItemOverlay::newPlaylistItemOverlay alignmentMode %d manualAlignment (%d,%d)", alignment, manualAlignmentX, manualAlignmentY);
  }
  else
  {
    newOverlay->layoutMode = (layoutModeEnum)root.findChildValue("layoutMode").toInt();
    DEBUG_OVERLAY("playlistItemOverlay::newPlaylistItemOverlay layoutMode %d", newOverlay->layoutMode);
    if (newOverlay->layoutMode == OVERLAY)
    {
      newOverlay->overlayMode = root.findChildValue("overlayMode").toInt();
    }
    else if (newOverlay->layoutMode == ARANGE)
    {
      newOverlay->arangementMode = root.findChildValue("arangementMode").toInt();
    }
    else if (newOverlay->layoutMode == CUSTOM)
    {
      int i = 1;
      while (true)
      {
        QString strX = root.findChildValue(QString("ItemPos%1X").arg(i));
        QString strY = root.findChildValue(QString("ItemPos%1Y").arg(i));
        if (strX.isEmpty() || strY.isEmpty())
          break;

        QPoint pos = QPoint(strX.toInt(), strY.toInt());
        newOverlay->customPositions[i-1] = pos;

        i++;
      }
    }
  }
  
  playlistItem::loadPropertiesFromPlaylist(root, newOverlay);

  return newOverlay;
}

void playlistItemOverlay::slotControlChanged()
{
  // One of the controls changed. Update the layout and emit the signal
  // that this item changed (needs to be redrawn)
  overlayMode    = ui.comboBoxOverlayMode->currentIndex();
  arangementMode = ui.comboBoxArangementMode->currentIndex();
  for (int i = 1; i < childCount(); i++)
  {
    QPoint p = getCutomPositionOfItem(i);
    customPositions[i-1] = p;
  }
  
  // No new item was added but update the layout of the items
  updateLayout(false);

  emit signalItemChanged(true, RECACHE_NONE);
}

void playlistItemOverlay::childChanged(bool redraw, recacheIndicator recache)
{
  if (redraw)
    updateLayout(false);

  playlistItemContainer::childChanged(redraw, recache);
}

void playlistItemOverlay::onGroupBoxToggled(int idx, bool on)
{
  const QSignalBlocker blocker0(ui.overlayGroupBox);
  const QSignalBlocker blocker1(ui.arangeGroupBox);
  const QSignalBlocker blocker2(ui.customGroupBox);
  if (on)
  {
    // Disable the other two
    if (idx != 0)
      ui.overlayGroupBox->setChecked(false);
    if (idx != 1)
      ui.arangeGroupBox->setChecked(false);
    if (idx != 2)
      ui.customGroupBox->setChecked(false);
  }
  else
  {
    // Switch it back on. We behave like radio buttons.
    if (idx == 0)
      ui.overlayGroupBox->setChecked(true);
    if (idx == 1)
      ui.arangeGroupBox->setChecked(true);
    if (idx == 2)
      ui.customGroupBox->setChecked(true);
  }

  // Update the arangement mode
  if (ui.overlayGroupBox->isChecked())
    layoutMode = OVERLAY;
  else if (ui.arangeGroupBox->isChecked())
    layoutMode = ARANGE;
  else if (ui.customGroupBox->isChecked())
    layoutMode = CUSTOM;

  slotControlChanged();
}

void playlistItemOverlay::loadFrame(int frameIdx, bool playing, bool loadRawData, bool emitSignals)
{
  // Does one of the items need loading?
  bool itemLoadedDoubleBuffer = false;
  bool itemLoaded = false;

  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *item = getChildPlaylistItem(i);
    auto state = item->needsLoading(frameIdx, loadRawData);
    if (state != LoadingNotNeeded)
    {
      // Load the requested current frame (or the double buffer) without emitting any signals.
      // We will emit the signal that loading is complete when all overlay items have loaded.
      DEBUG_OVERLAY("playlistItemWithVideo::loadFrame loading frame %d%s%s", frameIdx, playing ? " playing" : "", loadRawData ? " raw" : "");
      item->loadFrame(frameIdx, playing, loadRawData, false);
    }

    if (state == LoadingNeeded)
      itemLoaded = true;
    if (playing && (state == LoadingNeeded || state == LoadingNeededDoubleBuffer))
      itemLoadedDoubleBuffer = true;
  }

  if (emitSignals && itemLoaded)
    emit signalItemChanged(true, RECACHE_NONE);
  if (emitSignals && itemLoadedDoubleBuffer)
    emit signalItemDoubleBufferLoaded();
}

bool playlistItemOverlay::isLoading() const
{
  // We are loading if one of the child items is loading
  for (int i = 0; i < childCount(); i++)
    if (getChildPlaylistItem(i)->isLoading())
      return true;
  return false;
}

bool playlistItemOverlay::isLoadingDoubleBuffer() const
{
  // We are loading to the double buffer if one of the child items is loading to the double buffer
  for (int i = 0; i < childCount(); i++)
    if (getChildPlaylistItem(i)->isLoadingDoubleBuffer())
      return true;
  return false;
}

// Returns a possibly new widget at given row and column, having a set column span.
// Any existing widgets of other types or other span will be removed.
template <typename W> static W * widgetAt(QGridLayout *grid, int row, int column)
{
  Q_ASSERT(grid->columnCount() <= 3);
  QPointer<QWidget> widgets[3];
  for (int j = 0; j < grid->columnCount(); ++j)
  {
    auto item = grid->itemAtPosition(row, j);
    if (item) 
      widgets[j] = item->widget();
  }

  auto widget = qobject_cast<W*>(widgets[column]);
  if (!widget)
  {
    // There may be an incompatible widget there.
    delete widgets[column];
    widget = new W;
    grid->addWidget(widget, row, column, 1, 1);
  }
  return widget;
}

void playlistItemOverlay::clear(int startRow)
{
  for (int i = startRow; i < customPositionGrid->rowCount(); ++i)
    for (int j = 0; j < customPositionGrid->columnCount(); ++j)
    {
      auto item = customPositionGrid->itemAtPosition(i, j);
      if (item) 
        delete item->widget();
    }
}

void playlistItemOverlay::updateCustomPositionGrid()
{
  if (!propertiesWidget)
    return;

  const int row = childCount() - 1;
  for (int i = 0; i < row; i++)
  {
    // Counter
    //playlistItem *item = getChildPlaylistItem(i);
    QLabel *name = widgetAt<QLabel>(customPositionGrid, i, 0);
    name->setText(QString("Item %1").arg(i));

    // Width
    QSpinBox *width = widgetAt<QSpinBox>(customPositionGrid, i, 1);
    width->setRange(-CUSTOM_POS_MAX, CUSTOM_POS_MAX);
    connect(width, QOverload<int>::of(&QSpinBox::valueChanged), this, &playlistItemOverlay::slotControlChanged);
    QSignalBlocker widthSignalBlocker(width);
    width->setValue(customPositions[i].x());

    // Height
    QSpinBox *height = widgetAt<QSpinBox>(customPositionGrid, i, 2);
    height->setRange(-CUSTOM_POS_MAX, CUSTOM_POS_MAX);
    connect(height, QOverload<int>::of(&QSpinBox::valueChanged), this, &playlistItemOverlay::slotControlChanged);
    QSignalBlocker heightSignalBlocker(height);
    height->setValue(customPositions[i].y());
  }

  // Remove all widgets (rows) which are not used anymore
  clear(row);

  if (row > 0)
  {
    customPositionGrid->setColumnStretch(1, 1); // Last tow columns should stretch
    customPositionGrid->setColumnStretch(2, 1);
  }
}

QPoint playlistItemOverlay::getCutomPositionOfItem(int itemIdx) const
{
  assert(itemIdx >= 1);

  if (customPositionGrid == nullptr)
    return QPoint();
  if (customPositionGrid->columnCount() < 3)
    return QPoint();

  int gridRowIdx = itemIdx - 1;
  if (gridRowIdx >= customPositionGrid->rowCount())
    return QPoint();

  // There should be 2 spin boxes in this row
  QLayoutItem *layoutItemX = customPositionGrid->itemAtPosition(gridRowIdx, 1);
  QWidgetItem *layoutWidgetX = dynamic_cast<QWidgetItem*>(layoutItemX);
  if (layoutWidgetX == nullptr)
    return QPoint();
  QWidget *widgetX = dynamic_cast<QWidget*>(layoutWidgetX->widget());
  QSpinBox *spinBoxX = dynamic_cast<QSpinBox*>(widgetX);
  if (spinBoxX == nullptr)
    return QPoint();
  int posX = spinBoxX->value();

  QLayoutItem *layoutItemY = customPositionGrid->itemAtPosition(gridRowIdx, 2);
  QWidgetItem *layoutWidgetY = dynamic_cast<QWidgetItem*>(layoutItemY);
  if (layoutWidgetY == nullptr)
    return QPoint();
  QWidget *widgetY = dynamic_cast<QWidget*>(layoutWidgetY->widget());
  QSpinBox *spinBoxY = dynamic_cast<QSpinBox*>(widgetY);
  if (spinBoxY == nullptr)
    return QPoint();
  int posY = spinBoxY->value();

  return QPoint(posX, posY);
}

void playlistItemOverlay::guessBestLayout()
{
  // Are there statistic items? If yes, we select an overlay. If no, we select a 2D arangement
  bool statisticsPresent = false;
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = getChildPlaylistItem(i);
    playlistItemStatisticsFile *childStas = dynamic_cast<playlistItemStatisticsFile*>(childItem);
    if (childStas)
      statisticsPresent = true;
  }

  layoutMode = ARANGE;
  if (statisticsPresent)
    layoutMode = OVERLAY;
}