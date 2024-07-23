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

#include <QPainter>
#include <QPointer>
#include <cmath>
#include <limits>

#include <common/FunctionsGui.h>
#include <common/EnumMapper.h>

#define PLAYLISTITEMOVERLAY_DEBUG 0
#if PLAYLISTITEMOVERLAY_DEBUG && !NDEBUG
#include <QDebug>
#define DEBUG_OVERLAY qDebug
#else
#define DEBUG_OVERLAY(fmt, ...) ((void)0)
#endif

#define CUSTOM_POS_MAX 100000

namespace
{

constexpr EnumMapper<OverlayLayoutMode, 3>
    OverlayLayoutModeMapper(std::make_pair(OverlayLayoutMode::Overlay, "Overlay"sv),
                            std::make_pair(OverlayLayoutMode::Arange, "Average"sv),
                            std::make_pair(OverlayLayoutMode::Custom, "Custom"sv));

}

playlistItemOverlay::playlistItemOverlay() : playlistItemContainer("Overlay Item")
{
  this->setIcon(0, functionsGui::convertIcon(":img_overlay.png"));
  // Enable dropping for overlay objects. The user can drop items here to draw them as an overlay.
  this->setFlags(flags() | Qt::ItemIsDropEnabled);

  this->prop.propertiesWidgetTitle = "Overlay Properties";

  // This text is drawn if there are no child items in the overlay
  this->infoText =
      "Please drop some items onto this overlay. All child items will be drawn on top of "
      "each other.";
}

/* For an overlay item, the info list is just a list of the names of the
 * child elements.
 */
InfoData playlistItemOverlay::getInfo() const
{
  InfoData info("Overlay Info");

  // Add the size of this playlistItemOverlay
  info.items.append(
      InfoItem("Overlay Size", QString("(%1,%2)").arg(getSize().width()).arg(getSize().height())));

  // Add the sizes of all child items
  for (int i = 0; i < this->childCount(); i++)
  {
    if (auto childItem = getChildPlaylistItem(i))
    {
      auto childSize = childItem->getSize();
      info.items.append(
          InfoItem(QString("Item %1 size").arg(i),
                   QString("(%1,%2)").arg(childSize.width()).arg(childSize.height())));
    }
  }
  return info;
}

ValuePairListSets playlistItemOverlay::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;

  // The given pixelPos is relative to the bounding rectangle. For every child we have to calculate
  // the relative point within that item.
  auto relPoint = boundingRect.topLeft() + pixelPos;

  for (int i = 0; i < this->childCount(); i++)
  {
    if (auto childItem = this->getChildPlaylistItem(i))
    {
      // First check if the point is even within the child bounding rectangle
      if (this->childItemRects[i].contains(relPoint))
      {
        // Calculate the relative pixel position within this child item
        auto childPixelPos = relPoint - this->childItemRects[i].topLeft();

        auto childSets = childItem->getPixelValues(childPixelPos, frameIdx);
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

ItemLoadingState playlistItemOverlay::needsLoading(int frameIdx, bool loadRawdata)
{
  // The overlay needs to load if one of the child items needs to load
  for (int i = 0; i < this->childCount(); i++)
  {
    if (this->getChildPlaylistItem(i)->needsLoading(frameIdx, loadRawdata) ==
        ItemLoadingState::LoadingNeeded)
    {
      DEBUG_OVERLAY("playlistItemOverlay::needsLoading LoadingNeeded child %s",
                    this->getChildPlaylistItem(i)->getName().toLatin1().data());
      return ItemLoadingState::LoadingNeeded;
    }
  }
  for (int i = 0; i < this->childCount(); i++)
  {
    if (this->getChildPlaylistItem(i)->needsLoading(frameIdx, loadRawdata) ==
        ItemLoadingState::LoadingNeededDoubleBuffer)
    {
      DEBUG_OVERLAY("playlistItemOverlay::needsLoading LoadingNeededDoubleBuffer child %s",
                    this->getChildPlaylistItem(i)->getName().toLatin1().data());
      return ItemLoadingState::LoadingNeededDoubleBuffer;
    }
  }

  DEBUG_OVERLAY("playlistItemOverlay::needsLoading LoadingNotNeeded");
  return ItemLoadingState::LoadingNotNeeded;
}

void playlistItemOverlay::drawItem(QPainter *painter,
                                   int       frameIdx,
                                   double    zoomFactor,
                                   bool      drawRawData)
{
  DEBUG_OVERLAY("playlistItemOverlay::drawItem frame %d", frameIdx);

  if (this->childLlistUpdateRequired)
  {
    this->updateChildList();
    this->updateCustomPositionGrid();
  }

  if (this->childCount() == 0)
  {
    playlistItem::drawItem(painter, frameIdx, zoomFactor, drawRawData);
    return;
  }

  // Update the layout if the number of items changedupdateLayout
  this->updateLayout();

  // Translate to the center of this overlay item
  painter->translate(centerRoundTL(boundingRect) * zoomFactor * -1);

  // Draw all child items at their positions
  for (int i = 0; i < this->childCount(); i++)
  {
    if (auto childItem = this->getChildPlaylistItem(i))
    {
      auto center = centerRoundTL(this->childItemRects[i]);
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
  if (this->childCount() == 0)
    return playlistItemContainer::getSize();

  return this->boundingRect.size();
}

void playlistItemOverlay::updateLayout(bool onlyIfItemsChanged)
{
  if (this->childCount() == 0)
  {
    this->childItemRects.clear();
    this->childItemsIDs.clear();
    this->boundingRect = QRect();
    return;
  }

  // Check if the nr/order of items changed
  bool nrItemsChanged   = this->childCount() != this->childItemRects.count();
  bool itemOrderChanged = false;
  if (!nrItemsChanged)
  {
    for (int i = 0; i < this->childCount(); i++)
    {
      auto childItem = this->getChildPlaylistItem(i);
      if (int(this->childItemsIDs[i]) != childItem->properties().id)
      {
        itemOrderChanged = true;
        break;
      }
    }
  }

  if (onlyIfItemsChanged && !nrItemsChanged && !itemOrderChanged)
    return;

  DEBUG_OVERLAY("playlistItemOverlay::updateLayout%s",
                onlyIfNrItemsChanged ? " onlyIfNrItemsChanged" : "");

  if (nrItemsChanged || itemOrderChanged)
  {
    // Resize the childItems/IDs list
    this->childItemRects.clear();
    this->childItemsIDs.clear();
    for (int i = 0; i < this->childCount(); i++)
    {
      this->childItemRects.append(QRect());
      auto childItem = this->getChildPlaylistItem(i);
      this->childItemsIDs.append(childItem->properties().id);
    }
  }

  // Update the layout in all children which are also playlistItemOverlays
  for (int i = 0; i < this->childCount(); i++)
  {
    auto childItem = this->getChildPlaylistItem(i);
    if (auto childOverlay = dynamic_cast<playlistItemOverlay *>(childItem))
      childOverlay->updateLayout();
  }

  // The first playlist item is the "root".
  // We will arange all other items relative to this one
  auto firstItem = this->getChildPlaylistItem(0);
  boundingRect.setSize(firstItem->getSize());
  boundingRect.moveCenter(QPoint(0, 0));

  QRect firstItemRect;
  firstItemRect.setSize(firstItem->getSize());
  firstItemRect.moveCenter(QPoint(0, 0));
  this->childItemRects[0] = firstItemRect;
  DEBUG_OVERLAY("playlistItemOverlay::updateLayout item 0 size (%d,%d) firstItemRect (%d,%d)",
                firstItem->getSize().width(),
                firstItem->getSize().height(),
                firstItemRect.left(),
                firstItemRect.top());

  QList<int> columns, rows;
  const int  nrRowsCols = int(sqrt(childCount() - 1)) + 1;
  if (this->layoutMode == OverlayLayoutMode::Arange)
  {
    // Before calculating the actual position of each item, we have to pre-calculate
    // the row/column height/width for the 2D layout
    for (int i = 0; i < nrRowsCols; i++)
    {
      columns.append(0);
      rows.append(0);
    }
    for (int i = 0; i < this->childCount(); i++)
    {
      const auto r = i / nrRowsCols;
      const auto c = i % nrRowsCols;
      auto       p = this->getChildPlaylistItem(i);
      auto       s = p->getSize();
      if (columns[c] < s.width())
        columns[c] = s.width();
      if (rows[r] < s.height())
        rows[r] = s.height();
    }
  }

  // Align the rest of the items
  DEBUG_OVERLAY(
      "playlistItemOverlay::updateLayout childCount %d layoutMode %d", childCount(), layoutMode);
  for (int i = 1; i < this->childCount(); i++)
  {
    auto childItem = this->getChildPlaylistItem(i);
    if (childItem)
    {
      auto  childSize = childItem->getSize();
      QRect targetRect;
      targetRect.setSize(childSize);
      targetRect.moveCenter(QPoint(0, 0));

      if (this->layoutMode == OverlayLayoutMode::Overlay)
      {
        // Align based on alignment mode (must be between 0 and 8)
        switch (this->overlayMode)
        {
        case 0:
          targetRect.moveTopLeft(firstItemRect.topLeft());
          break;
        case 1:
          targetRect.moveTop(firstItemRect.top());
          break;
        case 2:
          targetRect.moveTopRight(firstItemRect.topRight());
          break;
        case 3:
          targetRect.moveLeft(firstItemRect.left());
          break;
        case 5:
          targetRect.moveRight(firstItemRect.right());
          break;
        case 6:
          targetRect.moveBottomLeft(firstItemRect.bottomLeft());
          break;
        case 7:
          targetRect.moveBottom(firstItemRect.bottom());
          break;
        case 8:
          targetRect.moveBottomRight(firstItemRect.bottomRight());
          break;
        default:
          targetRect.moveCenter(QPoint(0, 0));
          break;
        }
      }
      else if (this->layoutMode == OverlayLayoutMode::Arange)
      {
        if (this->arangementMode == 0)
        {
          // 2D
          const auto r = i / nrRowsCols;
          const auto c = i % nrRowsCols;
          auto       y = 0;
          for (int i = 0; i < r; i++)
            y += rows[i];
          int x = 0;
          for (int i = 0; i < c; i++)
            x += columns[i];
          targetRect.moveTopLeft(firstItemRect.topLeft() + QPoint(x, y));
        }
        else if (this->arangementMode == 1)
        {
          // Side by side
          auto newTopLeft = childItemRects[i - 1].topRight();
          newTopLeft.setX(newTopLeft.x() + 1);
          targetRect.moveTopLeft(newTopLeft);
        }
        else if (this->arangementMode == 2)
        {
          // Stacked
          auto newTopLeft = childItemRects[i - 1].bottomLeft();
          newTopLeft.setY(newTopLeft.y() + 1);
          targetRect.moveTopLeft(newTopLeft);
        }
      }
      else if (this->layoutMode == OverlayLayoutMode::Custom)
      {
        // Just use the provided custion positions per item
        auto pos = this->customPositions[i - 1];
        targetRect.moveTopLeft(firstItemRect.topLeft() + pos);
      }

      // Set item bounding rectangle
      this->childItemRects[i] = targetRect;

      DEBUG_OVERLAY("playlistItemOverlay::updateLayout item %d size (%d,%d) alignmentMode %d "
                    "targetRect (%d,%d)",
                    i,
                    childSize.width(),
                    childSize.height(),
                    alignmentMode,
                    targetRect.left(),
                    targetRect.top());

      // Expand the bounding rectangle
      this->boundingRect = this->boundingRect.united(targetRect);
    }
  }
}

void playlistItemOverlay::createPropertiesWidget()
{
  Q_ASSERT_X(!this->propertiesWidget, "createPropertiesWidget", "Properties widget already exists");

  // Create a new widget and populate it with controls
  this->propertiesWidget = std::make_unique<QWidget>();
  this->ui.setupUi(this->propertiesWidget.get());

  this->ui.verticalLayout->insertLayout(0, this->createPlaylistItemControls());

  // Alignment mode
  this->ui.comboBoxOverlayMode->addItems(QStringList() << "Top Left"
                                                       << "Top Center"
                                                       << "Top Right");
  this->ui.comboBoxOverlayMode->addItems(QStringList() << "Center Left"
                                                       << "Center"
                                                       << "Center Right");
  this->ui.comboBoxOverlayMode->addItems(QStringList() << "Bottom Left"
                                                       << "Bottom Center"
                                                       << "Bottom Right");
  this->ui.comboBoxOverlayMode->setCurrentIndex(this->overlayMode);

  this->ui.comboBoxArangementMode->addItems(QStringList() << "2D Grid"
                                                          << "Side by Side"
                                                          << "Stacked");
  this->ui.comboBoxArangementMode->setCurrentIndex(this->arangementMode);

  this->ui.overlayGroupBox->setChecked(this->layoutMode == OverlayLayoutMode::Overlay);
  this->ui.arangeGroupBox->setChecked(this->layoutMode == OverlayLayoutMode::Arange);
  this->ui.customGroupBox->setChecked(this->layoutMode == OverlayLayoutMode::Custom);

  // Create and add the grid layout for the custom positions
  this->customPositionGrid = new QGridLayout(this->ui.customGroupBox);

  this->ui.verticalLayout->insertLayout(-1, createContainerItemControls());
  this->ui.verticalLayout->addStretch(1);

  // Connect signals/slots
  this->connect(this->ui.overlayGroupBox,
                &QGroupBox::toggled,
                this,
                &playlistItemOverlay::on_overlayGroupBox_toggled);
  this->connect(this->ui.arangeGroupBox,
                &QGroupBox::toggled,
                this,
                &playlistItemOverlay::on_arangeGroupBox_toggled);
  this->connect(this->ui.customGroupBox,
                &QGroupBox::toggled,
                this,
                &playlistItemOverlay::on_customGroupBox_toggled);
  this->connect(this->ui.comboBoxOverlayMode,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                &playlistItemOverlay::slotControlChanged);
  this->connect(this->ui.comboBoxArangementMode,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                &playlistItemOverlay::slotControlChanged);
}

void playlistItemOverlay::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  YUViewDomElement d = root.ownerDocument().createElement("playlistItemOverlay");

  // Append the playlist item properties
  playlistItem::appendPropertiesToPlaylist(d);

  // Append the overlay properties
  d.appendProperiteChild("layoutMode", OverlayLayoutModeMapper.getName(this->layoutMode));
  if (this->layoutMode == OverlayLayoutMode::Overlay)
    d.appendProperiteChild("overlayMode", QString::number(overlayMode));
  else if (this->layoutMode == OverlayLayoutMode::Arange)
    d.appendProperiteChild("arangementMode", QString::number(arangementMode));
  else
  {
    assert(this->layoutMode == OverlayLayoutMode::Custom);
    for (int i = 1; i < this->childCount(); i++)
    {
      auto p = this->customPositions[i - 1];
      d.appendProperiteChild(QString("ItemPos%1X").arg(i), QString::number(p.x()));
      d.appendProperiteChild(QString("ItemPos%1Y").arg(i), QString::number(p.y()));
    }
  }

  playlistItemContainer::savePlaylistChildren(d, playlistDir);

  root.appendChild(d);
}

playlistItemOverlay *playlistItemOverlay::newPlaylistItemOverlay(const YUViewDomElement &root,
                                                                 const QString &)
{
  bool oldFormat = (!root.findChildValue("alignmentMode").isEmpty());
  bool newFormat = (!root.findChildValue("layoutMode").isEmpty());
  if (!oldFormat && !newFormat)
    return nullptr;

  auto newOverlay = new playlistItemOverlay();

  if (oldFormat)
  {
    auto alignmentMode    = root.findChildValue("alignmentMode").toInt();
    auto manualAlignmentX = root.findChildValue("manualAlignmentX").toInt();
    auto manualAlignmentY = root.findChildValue("manualAlignmentY").toInt();

    // We can not really map the old (and not really pratical) notation
    if (manualAlignmentX != 0 || manualAlignmentY != 0)
    {
      newOverlay->layoutMode         = OverlayLayoutMode::Custom;
      newOverlay->customPositions[0] = QPoint(manualAlignmentX, manualAlignmentY);
    }
    else
    {
      newOverlay->layoutMode     = OverlayLayoutMode::Overlay;
      newOverlay->arangementMode = alignmentMode;
    }

    DEBUG_OVERLAY(
        "playlistItemOverlay::newPlaylistItemOverlay alignmentMode %d manualAlignment (%d,%d)",
        alignment,
        manualAlignmentX,
        manualAlignmentY);
  }
  else
  {
    if (auto mode = OverlayLayoutModeMapper.getValueFromNameOrIndex(
            root.findChildValue("layoutMode").toStdString()))
      newOverlay->layoutMode = *mode;
    DEBUG_OVERLAY("playlistItemOverlay::newPlaylistItemOverlay layoutMode %d",
                  newOverlay->layoutMode);
    if (newOverlay->layoutMode == OverlayLayoutMode::Overlay)
    {
      newOverlay->overlayMode = root.findChildValue("overlayMode").toInt();
    }
    else if (newOverlay->layoutMode == OverlayLayoutMode::Arange)
    {
      newOverlay->arangementMode = root.findChildValue("arangementMode").toInt();
    }
    else if (newOverlay->layoutMode == OverlayLayoutMode::Custom)
    {
      int i = 1;
      while (true)
      {
        auto strX = root.findChildValue(QString("ItemPos%1X").arg(i));
        auto strY = root.findChildValue(QString("ItemPos%1Y").arg(i));
        if (strX.isEmpty() || strY.isEmpty())
          break;

        newOverlay->customPositions[i - 1] = QPoint(strX.toInt(), strY.toInt());
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
  this->overlayMode    = this->ui.comboBoxOverlayMode->currentIndex();
  this->arangementMode = this->ui.comboBoxArangementMode->currentIndex();
  for (int i = 1; i < this->childCount(); i++)
  {
    auto p                       = this->getCutomPositionOfItem(i);
    this->customPositions[i - 1] = p;
  }

  // No new item was added but update the layout of the items
  this->updateLayout(false);

  emit SignalItemChanged(true, RECACHE_NONE);
}

void playlistItemOverlay::childChanged(bool redraw, recacheIndicator recache)
{
  if (redraw)
    this->updateLayout(false);

  playlistItemContainer::childChanged(redraw, recache);
}

void playlistItemOverlay::onGroupBoxToggled(int idx, bool on)
{
  const QSignalBlocker blocker0(this->ui.overlayGroupBox);
  const QSignalBlocker blocker1(this->ui.arangeGroupBox);
  const QSignalBlocker blocker2(this->ui.customGroupBox);
  if (on)
  {
    // Disable the other two
    if (idx != 0)
      this->ui.overlayGroupBox->setChecked(false);
    if (idx != 1)
      this->ui.arangeGroupBox->setChecked(false);
    if (idx != 2)
      this->ui.customGroupBox->setChecked(false);
  }
  else
  {
    // Switch it back on. We behave like radio buttons.
    if (idx == 0)
      this->ui.overlayGroupBox->setChecked(true);
    if (idx == 1)
      this->ui.arangeGroupBox->setChecked(true);
    if (idx == 2)
      this->ui.customGroupBox->setChecked(true);
  }

  // Update the arangement mode
  if (this->ui.overlayGroupBox->isChecked())
    this->layoutMode = OverlayLayoutMode::Overlay;
  else if (this->ui.arangeGroupBox->isChecked())
    this->layoutMode = OverlayLayoutMode::Arange;
  else if (this->ui.customGroupBox->isChecked())
    this->layoutMode = OverlayLayoutMode::Custom;

  this->slotControlChanged();
}

void playlistItemOverlay::loadFrame(int frameIdx, bool playing, bool loadRawData, bool emitSignals)
{
  // Does one of the items need loading?
  bool itemLoadedDoubleBuffer = false;
  bool itemLoaded             = false;

  for (int i = 0; i < this->childCount(); i++)
  {
    auto item  = this->getChildPlaylistItem(i);
    auto state = item->needsLoading(frameIdx, loadRawData);
    if (state != ItemLoadingState::LoadingNotNeeded)
    {
      // Load the requested current frame (or the double buffer) without emitting any signals.
      // We will emit the signal that loading is complete when all overlay items have loaded.
      DEBUG_OVERLAY("playlistItemWithVideo::loadFrame loading frame %d%s%s",
                    frameIdx,
                    playing ? " playing" : "",
                    loadRawData ? " raw" : "");
      item->loadFrame(frameIdx, playing, loadRawData, false);
    }

    if (state == ItemLoadingState::LoadingNeeded)
      itemLoaded = true;
    if (playing && (state == ItemLoadingState::LoadingNeeded ||
                    state == ItemLoadingState::LoadingNeededDoubleBuffer))
      itemLoadedDoubleBuffer = true;
  }

  if (emitSignals && itemLoaded)
    emit SignalItemChanged(true, RECACHE_NONE);
  if (emitSignals && itemLoadedDoubleBuffer)
    emit signalItemDoubleBufferLoaded();
}

bool playlistItemOverlay::isLoading() const
{
  // We are loading if one of the child items is loading
  for (int i = 0; i < this->childCount(); i++)
    if (this->getChildPlaylistItem(i)->isLoading())
      return true;
  return false;
}

bool playlistItemOverlay::isLoadingDoubleBuffer() const
{
  // We are loading to the double buffer if one of the child items is loading to the double buffer
  for (int i = 0; i < this->childCount(); i++)
    if (this->getChildPlaylistItem(i)->isLoadingDoubleBuffer())
      return true;
  return false;
}

// Returns a possibly new widget at given row and column, having a set column span.
// Any existing widgets of other types or other span will be removed.
template <typename W> static W *widgetAt(QGridLayout *grid, int row, int column)
{
  Q_ASSERT(grid->columnCount() <= 3);
  QPointer<QWidget> widgets[3];
  for (int j = 0; j < grid->columnCount(); ++j)
  {
    auto item = grid->itemAtPosition(row, j);
    if (item)
      widgets[j] = item->widget();
  }

  auto widget = qobject_cast<W *>(widgets[column]);
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
  for (int i = startRow; i < this->customPositionGrid->rowCount(); ++i)
    for (int j = 0; j < this->customPositionGrid->columnCount(); ++j)
    {
      auto item = this->customPositionGrid->itemAtPosition(i, j);
      if (item)
        delete item->widget();
    }
}

void playlistItemOverlay::updateCustomPositionGrid()
{
  if (!this->propertiesWidget)
    return;

  const int row = this->childCount() - 1;
  for (int i = 0; i < row; i++)
  {
    auto nameLabel = widgetAt<QLabel>(this->customPositionGrid, i, 0);
    nameLabel->setText(QString("Item %1").arg(i));

    // Width
    auto widthSpinBox = widgetAt<QSpinBox>(this->customPositionGrid, i, 1);
    widthSpinBox->setRange(-CUSTOM_POS_MAX, CUSTOM_POS_MAX);
    connect(widthSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &playlistItemOverlay::slotControlChanged);
    QSignalBlocker widthSignalBlocker(widthSpinBox);
    widthSpinBox->setValue(this->customPositions[i].x());

    // Height
    auto heightSpinBox = widgetAt<QSpinBox>(this->customPositionGrid, i, 2);
    heightSpinBox->setRange(-CUSTOM_POS_MAX, CUSTOM_POS_MAX);
    connect(heightSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &playlistItemOverlay::slotControlChanged);
    QSignalBlocker heightSignalBlocker(heightSpinBox);
    heightSpinBox->setValue(customPositions[i].y());
  }

  // Remove all widgets (rows) which are not used anymore
  this->clear(row);

  if (row > 0)
  {
    this->customPositionGrid->setColumnStretch(1, 1); // Last tow columns should stretch
    this->customPositionGrid->setColumnStretch(2, 1);
  }
}

QPoint playlistItemOverlay::getCutomPositionOfItem(int itemIdx) const
{
  assert(itemIdx >= 1);

  if (this->customPositionGrid == nullptr)
    return {};
  if (this->customPositionGrid->columnCount() < 3)
    return {};

  int gridRowIdx = itemIdx - 1;
  if (gridRowIdx >= this->customPositionGrid->rowCount())
    return QPoint();

  // There should be 2 spin boxes in this row
  auto layoutItemX   = this->customPositionGrid->itemAtPosition(gridRowIdx, 1);
  auto layoutWidgetX = dynamic_cast<QWidgetItem *>(layoutItemX);
  if (layoutWidgetX == nullptr)
    return {};
  auto widgetX  = dynamic_cast<QWidget *>(layoutWidgetX->widget());
  auto spinBoxX = dynamic_cast<QSpinBox *>(widgetX);
  if (spinBoxX == nullptr)
    return {};

  auto layoutItemY   = this->customPositionGrid->itemAtPosition(gridRowIdx, 2);
  auto layoutWidgetY = dynamic_cast<QWidgetItem *>(layoutItemY);
  if (layoutWidgetY == nullptr)
    return {};
  auto widgetY  = dynamic_cast<QWidget *>(layoutWidgetY->widget());
  auto spinBoxY = dynamic_cast<QSpinBox *>(widgetY);
  if (spinBoxY == nullptr)
    return {};

  return QPoint(spinBoxX->value(), spinBoxY->value());
}

void playlistItemOverlay::guessBestLayout()
{
  // Are there statistic items? If yes, we select an overlay. If no, we select a 2D arangement
  bool statisticsPresent = false;
  for (int i = 0; i < this->childCount(); i++)
  {
    auto childItem = this->getChildPlaylistItem(i);
    auto childStas = dynamic_cast<playlistItemStatisticsFile *>(childItem);
    if (childStas)
      statisticsPresent = true;
  }

  this->layoutMode = OverlayLayoutMode::Arange;
  if (statisticsPresent)
    this->layoutMode = OverlayLayoutMode::Overlay;
}
