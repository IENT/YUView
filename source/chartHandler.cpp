/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2017  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "chartHandler.h"


// Default-Constructor
ChartHandler::ChartHandler() : mYUVChartFactory(&this->mNoDataToShowWidget, &this->mDataIsLoadingWidget)
{
  // creating the default widget if no data is avaible
  QFormLayout  noDataLayout(&(this->mNoDataToShowWidget));
  QLabel* lblNoDataInformation = new QLabel(WIDGET_NO_DATA_TO_SHOW);
  lblNoDataInformation->setWordWrap(true);
  noDataLayout.addWidget(lblNoDataInformation);

  // creating the default widget if data is loading
  QVBoxLayout* basicLayout = new QVBoxLayout;

  QGridLayout*  dataLoadingLayout = new QGridLayout;
  QLabel* lblDataLoadingInformation = new QLabel(WIDGET_DATA_IS_LOADING);
  lblDataLoadingInformation->setWordWrap(true);

  QLabel* lblImageHolder = new QLabel();
  QPixmap image(":/img_hourglass.png");
  lblImageHolder->setPixmap(image);

  dataLoadingLayout->addWidget(lblImageHolder, 0, 0, Qt::AlignRight);
  dataLoadingLayout->addWidget(lblDataLoadingInformation, 0, 1);

  basicLayout->addLayout(dataLoadingLayout);
  basicLayout->setAlignment(dataLoadingLayout, Qt::AlignTop);

  this->mDataIsLoadingWidget.setLayout(basicLayout);

  // set defaults to the draw-chart-checkbox
  this->mCbDrawChart = new QCheckBox("");
  this->mCbDrawChart->setChecked(true);
  this->mCbDrawChart->setObjectName(CHECKBOX_DRAW_CHART);

  // implement basic behaviour to the checkbox
  connect(this->mCbDrawChart, &QCheckBox::clicked, this, [this](bool aClicked) {
    if(aClicked)
      this->playbackControllerFrameChanged(-1);
  });
}

/*-------------------- public functions --------------------*/
QWidget* ChartHandler::createChartWidget(playlistItem *aItem)
{
  //! a small lambda; the lambda is a workaround for the timer.
  //! if all items implement the isDataAvaible() correct, the lambda
  //! can be removed
  auto playlistItemIsSupported = [=](playlistItem* aItem) {
    // add dynamic_cast<YOURPLAYLISTITEMTYPE*>(aItem) with OR-check to support your playlistitem
    return dynamic_cast<playlistItemStatisticsFile*>(aItem)
           || false;
  };

  // check if the widget was already created and stored
  itemWidgetCoord coord;
  coord.mItem = aItem;

  if (this->mListItemWidget.contains(coord)) // was stored
    return this->mListItemWidget.at(this->mListItemWidget.indexOf(coord)).mWidget;

  if((playlistItemIsSupported(aItem) && (aItem) && (!aItem->isDataAvaible())))
  {
    this->mTimer.start(1000, this);
    return &(this->mDataIsLoadingWidget);
  }
  else
    if(this->mTimer.isActive())
      this->mTimer.stop();

  // in case of playlistItemStatisticsFile
  if(dynamic_cast<playlistItemStatisticsFile*>(aItem))
  {
    // cast item to right type
    playlistItemStatisticsFile* pltsf = dynamic_cast<playlistItemStatisticsFile*>(aItem);
    // get the widget and save it
    coord.mWidget = this->createStatisticFileWidget(pltsf, coord);
    // we save an item-widget combination in a list
    // in case of getting the widget a second time, we just have to load it from the list
    this->mListItemWidget << coord;
  }
  else
  {
    // in case of default, that we dont know the item-type
    coord.mWidget = this->mChartWidget->getDefaultWidget();
    coord.mItem = NULL;
  }

  return coord.mWidget;
}

QString ChartHandler::getChartsTitle(playlistItem *aItem)
{
  // every item will get a specified title, if null or item-type was not found, default-title will return
  // in case of playlistItemStatisticsFile
  if(dynamic_cast<playlistItemStatisticsFile*> (aItem))
    return CHARTSWIDGET_WINDOW_TITLE_STATISTICS;

  // return default name
  return CHARTSWIDGET_WINDOW_TITLE_DEFAULT;
}

void ChartHandler::removeWidgetFromList(playlistItem* aItem)
{
  // if a item is deleted from the playlist, we have to remove the widget from the list
  itemWidgetCoord tmp;
  tmp.mItem = aItem;
  if (this->mListItemWidget.contains(tmp))
    this->mListItemWidget.removeAll(tmp);
}

/*-------------------- private functions --------------------*/
itemWidgetCoord ChartHandler::getItemWidgetCoord(playlistItem *aItem)
{
  itemWidgetCoord coord;
  coord.mItem = aItem;

  if (this->mListItemWidget.contains(coord))
    coord = this->mListItemWidget.at(this->mListItemWidget.indexOf(coord));
  else
    coord.mItem   = NULL;

  return coord;
}

void ChartHandler::placeChart(itemWidgetCoord aCoord, QWidget* aChart)
{
  if(aCoord.mChart->indexOf(aChart) == -1)
    aCoord.mChart->addWidget(aChart);

  aCoord.mChart->setCurrentWidget(aChart);
}

QList<QWidget*> ChartHandler::generateOrderWidgetsOnly(bool aAddOptions)
{
  // creating the result-list
  QList<QWidget*> result;

  // we need a label for a simple text-information
  QLabel* lblOptionsShow    = new QLabel(CBX_LABEL_FRAME);
  // furthermore we need the combobox
  QComboBox* cbxOptionsShow = new QComboBox;

  // we need a label for a simple text-information
  QLabel* lblOptionsGroup    = new QLabel(CBX_LABEL_GROUPBY);
  // furthermore we need the combobox
  QComboBox* cbxOptionsGroup = new QComboBox;

  // we need a label for a simple text-information
  QLabel* lblOptionsNormalize    = new QLabel(CBX_LABEL_NORMALIZE );
  // furthermore we need the combobox
  QComboBox* cbxOptionsNormalize = new QComboBox;

  // init all members we need for the showing the range
  // we need: a label to show the name
  // we need a slider and a spinbox to show, select and change the value
  // at least we need a layout and a widget to place the silder and the spinbox
  QLabel* lblBeginFrame = new QLabel(SLIDER_LABEL_BEGIN_FRAME);
  QSlider* sldBeginFrame = new QSlider(Qt::Horizontal);
  QSpinBox* sbxBeginFrame = new QSpinBox();
  QGridLayout* lyBeginFrame = new QGridLayout();
  QWidget* wdgBeginFrame = new QWidget();

  QLabel* lblEndFrame = new QLabel(SLIDER_LABEL_END_FRAME);
  QSlider* sldEndFrame = new QSlider(Qt::Horizontal);
  QSpinBox* sbxEndFrame = new QSpinBox();
  QGridLayout* lyEndFrame = new QGridLayout();
  QWidget* wdgEndFrame = new QWidget();

  // define the slider
  sldBeginFrame->setTickPosition(QSlider::TicksBelow);
  sldEndFrame->setTickPosition(QSlider::TicksBelow);

  // set options name
  cbxOptionsShow->setObjectName(OPTION_NAME_CBX_CHART_FRAMESHOW);
  cbxOptionsGroup->setObjectName(OPTION_NAME_CBX_CHART_GROUPBY);
  cbxOptionsNormalize->setObjectName(OPTION_NAME_CBX_CHART_NORMALIZE);

  lblBeginFrame->setObjectName(LABEL_FRAME_RANGE_BEGIN);
  lblEndFrame->setObjectName(LABEL_FRAME_RANGE_END);

  sldBeginFrame->setObjectName(SLIDER_FRAME_RANGE_BEGIN);
  sldEndFrame->setObjectName(SLIDER_FRAME_RANGE_END);

  sbxBeginFrame->setObjectName(SPINBOX_FRAME_RANGE_BEGIN);
  sbxEndFrame->setObjectName(SPINBOX_FRAME_RANGE_END);

  // disable the elements first, as default. it should be enabled later
  cbxOptionsShow->setEnabled(false);
  cbxOptionsGroup->setEnabled(false);
  cbxOptionsNormalize->setEnabled(false);

  lblBeginFrame->setEnabled(false);
  lblEndFrame->setEnabled(false);

  sldBeginFrame->setEnabled(false);
  sldEndFrame->setEnabled(false);

  sbxBeginFrame->setEnabled(false);
  sbxEndFrame->setEnabled(false);

  // set the necessary connects
  connect(sldBeginFrame, &QSlider::valueChanged, this, &ChartHandler::sliderRangeChange);
  connect(sldEndFrame, &QSlider::valueChanged, this, &ChartHandler::sliderRangeChange);

  connect(sbxBeginFrame,
          static_cast<void (QSpinBox::*)(int)> (&QSpinBox::valueChanged),
          this,
          &ChartHandler::spinboxRangeChange);
  connect(sbxEndFrame,
          static_cast<void (QSpinBox::*)(int)> (&QSpinBox::valueChanged),
          this,
          &ChartHandler::spinboxRangeChange);

  // add the elements to the layout and add the layout to the widget
  lyBeginFrame->addWidget(sldBeginFrame, 0, 1);
  lyBeginFrame->addWidget(sbxBeginFrame, 0, 2);

  lyEndFrame->addWidget(sldEndFrame, 0, 1);
  lyEndFrame->addWidget(sbxEndFrame, 0 ,2);

  wdgBeginFrame->setLayout(lyBeginFrame);
  wdgEndFrame->setLayout(lyEndFrame);

  // adding the options with the enum ChartOrderBy
  if(aAddOptions)
  {
    // adding the show options
    cbxOptionsShow->addItem(EnumAuxiliary::asString(csPerFrame), csPerFrame);
    cbxOptionsShow->setItemData(0, EnumAuxiliary::asTooltip(csPerFrame), Qt::ToolTipRole);

    cbxOptionsShow->addItem(EnumAuxiliary::asString(csRange), csRange);
    cbxOptionsShow->setItemData(1, EnumAuxiliary::asTooltip(csRange), Qt::ToolTipRole);

    cbxOptionsShow->addItem(EnumAuxiliary::asString(csAllFrames), csAllFrames);
    cbxOptionsShow->setItemData(2, EnumAuxiliary::asTooltip(csAllFrames), Qt::ToolTipRole);

    // adding the group by options
    cbxOptionsGroup->addItem(EnumAuxiliary::asString(cgbByValue), cgbByValue);
    cbxOptionsGroup->setItemData(0, EnumAuxiliary::asTooltip(cgbByValue), Qt::ToolTipRole);

    cbxOptionsGroup->addItem(EnumAuxiliary::asString(cgbByBlocksize), cgbByBlocksize);
    cbxOptionsGroup->setItemData(1, EnumAuxiliary::asTooltip(cgbByBlocksize), Qt::ToolTipRole);

    // adding the normalize options
    cbxOptionsNormalize->addItem(EnumAuxiliary::asString(cnNone), cnNone);
    cbxOptionsNormalize->setItemData(0, EnumAuxiliary::asTooltip(cnNone), Qt::ToolTipRole);

    cbxOptionsNormalize->addItem(EnumAuxiliary::asString(cnByArea), cnByArea);
    cbxOptionsNormalize->setItemData(1, EnumAuxiliary::asTooltip(cnByArea), Qt::ToolTipRole);
  }
  else
  {
    // adding the show option unknown
    cbxOptionsShow->addItem(EnumAuxiliary::asString(csUnknown), csUnknown);
    cbxOptionsShow->setItemData(0, EnumAuxiliary::asTooltip(csUnknown), Qt::ToolTipRole);

    // adding the group by option unknown
    cbxOptionsGroup->addItem(EnumAuxiliary::asString(cgbUnknown), cgbUnknown);
    cbxOptionsGroup->setItemData(0, EnumAuxiliary::asTooltip(cgbUnknown), Qt::ToolTipRole);

    // adding the normalize option unknown
    cbxOptionsNormalize->addItem(EnumAuxiliary::asString(cnUnknown), cnUnknown);
    cbxOptionsNormalize->setItemData(0, EnumAuxiliary::asTooltip(cnUnknown), Qt::ToolTipRole);
  }

  // add all the widgets to the list
  result << lblOptionsShow;
  result << cbxOptionsShow;

  result << lblBeginFrame;
  result << wdgBeginFrame;

  result << lblEndFrame;
  result << wdgEndFrame;

  result << lblOptionsGroup;
  result << cbxOptionsGroup;

  result << lblOptionsNormalize;
  result << cbxOptionsNormalize;

  return result;
}

QLayout* ChartHandler::generateOrderByLayout(bool aAddOptions)
{
  // define a result-layout --> maybe do changable?
  QHBoxLayout* lytResult    = new QHBoxLayout;

  // getting all widgets we have to place on a layout
  QList<QWidget*> listWidgets = this->generateOrderWidgetsOnly(aAddOptions);

  // place all widgets on the result-layout
  foreach (auto widget, listWidgets)
    lytResult->addWidget(widget);

  return lytResult;
}

void ChartHandler::rangeChange(bool aSlider, bool aSpinbox)
{
  // the object holders
  QSlider* sldBeginFrame  = NULL;
  QSlider* sldEndFrame    = NULL;
  QSpinBox* sbxBeginFrame = NULL;
  QSpinBox* sbxEndFrame   = NULL;
  // a small lambda function, to reduce same code
  auto findAndSetComponents = [&] (QObject* aChild)
  {
    QString objectname = aChild->objectName();

    if(objectname == "")
      return;

    if(objectname == SLIDER_FRAME_RANGE_BEGIN)
      sldBeginFrame = dynamic_cast<QSlider*> (aChild);

    if(objectname == SLIDER_FRAME_RANGE_END)
      sldEndFrame = dynamic_cast<QSlider*> (aChild);

    if(objectname == SPINBOX_FRAME_RANGE_BEGIN)
      sbxBeginFrame = dynamic_cast<QSpinBox*> (aChild);

    if(objectname == SPINBOX_FRAME_RANGE_END)
      sbxEndFrame = dynamic_cast<QSpinBox*> (aChild);
  };

  auto items = this->mPlaylist->getSelectedItems();
  bool anyItemsSelected = items[0] != NULL || items[1] != NULL;
  if(!anyItemsSelected) // check that really something is selected
    return;

  // now we need the combination, try to find it
  itemWidgetCoord coord = this->getItemWidgetCoord(items[0]);

  // we need the order-combobox, so we have to find the combobox and get the text of it
  QObjectList children = coord.mWidget->children();


  //try to find the childs
  foreach (auto child, children)
  {
    if(child->children().count() > 1)
    {
      foreach (auto innerchild, child->children())
        findAndSetComponents(innerchild);
    }
    else
      findAndSetComponents(child);

    if(sldBeginFrame && sldEndFrame && sbxBeginFrame &&  sbxEndFrame)
      break;
  }

  //what happens!, we have had find both sliders and spinboxes
  if(!(sldBeginFrame && sldEndFrame && sbxBeginFrame && sbxEndFrame))
    return;

  // getting the frame from the startslider and endslider
  int startframe = -1;
  int endframe = -1;

  if(aSlider)
  {
    startframe = sldBeginFrame->value();
    endframe = sldEndFrame->value();
  }
  else if(aSpinbox)
  {
    startframe = sbxBeginFrame->value();
    endframe = sbxEndFrame->value();

    indexRange range = coord.mItem->getFrameIdxRange();

    // why?! it shouldn't be possible, that the spinbox value is higher than the maximum
    // check and if true, set to an valid value and return at this point
    if(startframe > range.second)
    {
      sbxBeginFrame->setValue(range.second);
      return;
    }

    // check and if true, set to an valid value and return at this point
    if(endframe > range.second)
    {
      sbxEndFrame->setValue(range.second);
      return;
    }
  }
  else
    return;

  // check if we have valid start and end frame
  if((startframe == -1) || (endframe == -1))
    return;

  // let's do some magic :P
  // the logic part for the slider and the spinbox.
  // the endframe should be always the same or higher than the startframe
  if (endframe > startframe)
  {
    // block signals so we dont fire recursively
    sldEndFrame->blockSignals(true);
    sldEndFrame->setValue(endframe);
    sldEndFrame->blockSignals(false);

    sbxEndFrame->blockSignals(true);
    sbxEndFrame->setValue(endframe);
    sbxEndFrame->blockSignals(false);
  }

  if(endframe < startframe)
  {
    // block signals so we dont fire recursively
    sldEndFrame->blockSignals(true);
    sldEndFrame->setValue(startframe);
    sldEndFrame->blockSignals(false);

    sbxEndFrame->blockSignals(true);
    sbxEndFrame->setValue(startframe);
    sbxEndFrame->blockSignals(false);

    // block signals so we dont fire recursively
    sldBeginFrame->blockSignals(true);
    sldBeginFrame->setValue(startframe);
    sldBeginFrame->blockSignals(false);

    sbxBeginFrame->blockSignals(true);
    sbxBeginFrame->setValue(startframe);
    sbxBeginFrame->blockSignals(false);

    // we can leave at this point, because we dont have to recalculate the chart
    return;
  }
  else
  {
    // we set the new values to the slider and the spinbox
    // block signals so we dont fire recursively
    sbxBeginFrame->blockSignals(true);
    sbxBeginFrame->setValue(startframe);
    sbxBeginFrame->blockSignals(false);

    sbxEndFrame->blockSignals(true);
    sbxEndFrame->setValue(endframe);
    sbxEndFrame->blockSignals(false);

    // block signals so we dont fire recursively
    sldBeginFrame->blockSignals(true);
    sldBeginFrame->setValue(startframe);
    sldBeginFrame->blockSignals(false);

    sldEndFrame->blockSignals(true);
    sldEndFrame->setValue(endframe);
    sldEndFrame->blockSignals(false);
  }

  // at least, create the statistics
  // no valid string is possible, because it will get later
  this->onStatisticsChange("");
}

void ChartHandler::setRangeToComponents(itemWidgetCoord aCoord, QObject* aObject)
{
  QObjectList list;

  if(aObject)
    list = aObject->children();
  else
     list = aCoord.mWidget->children();

  // in this loop we go thru all widget-elements we have
  foreach (auto widget, list)
  {
    // if our widget has child-widgets we have to control them again (recursiv)
    if(widget->children().count() > 1)
      this->setRangeToComponents(aCoord, widget);
    else
    {
      //first we get the range
      indexRange range = aCoord.mItem->getFrameIdxRange();

      // next step getting the name of our widget we look at
      QString objectName = widget->objectName();

      // is the name, the one we search for
      if((objectName == SLIDER_FRAME_RANGE_BEGIN) || (objectName == SLIDER_FRAME_RANGE_END))
      {
        // we have found the searched slider
        QSlider* slider = dynamic_cast<QSlider*>(widget); // cast it to a slider
        slider->blockSignals(true);// we block the signal, so no emit will done
        slider->setRange(range.first, range.second); // set the range
        slider->blockSignals(false); // free the blocked signal
      }

      // is the name, the one we search for
      if((objectName == SPINBOX_FRAME_RANGE_BEGIN) || (objectName == SPINBOX_FRAME_RANGE_END))
      {
        QSpinBox* spinbox = dynamic_cast<QSpinBox*>(widget); // cast it to a spinbox
        spinbox->blockSignals(true); // we block the signal, so no emit will done
        spinbox->setRange(range.first, range.second); // set the range
        spinbox->blockSignals(false); // free the blocked signal
      }
    }
  }
}

indexRange ChartHandler::getFrameRange(itemWidgetCoord aCoord)
{
  // for the range it doesn't matter if we look at the slider or the spinbox
  // both elements should have the same value

  // preparing the result
  indexRange result;

  // in this loop we go thru all widget-elements we have
  foreach (auto widget, aCoord.mWidget->children())
  {
    // if our widget has child-widgets we have to control them again
    if(widget->children().count() > 1)
    {
      foreach (auto innerwidget, widget->children())
      {
        QString objectName =  innerwidget->objectName();

        // is the name, the one we search for
        if(objectName == SLIDER_FRAME_RANGE_BEGIN)
        {
          // we have found the searched slider
          QSlider* slider = dynamic_cast<QSlider*>(innerwidget); // cast it to a slider
          result.first = slider->value();
        }

        // is the name, the one we search for
        if(objectName == SLIDER_FRAME_RANGE_END)
        {
          // we have found the searched slider
          QSlider* slider = dynamic_cast<QSlider*>(innerwidget); // cast it to a slider
          result.second = slider->value();
        }
      }
    }
    else
    {
      QString objectName =  widget->objectName();

      // is the name, the one we search for
      if(objectName == SLIDER_FRAME_RANGE_BEGIN)
      {
        // we have found the searched slider
        QSlider* slider = dynamic_cast<QSlider*>(widget); // cast it to a slider
        result.first = slider->value();
      }

      // is the name, the one we search for
      if(objectName == SLIDER_FRAME_RANGE_END)
      {
        // we have found the searched slider
        QSlider* slider = dynamic_cast<QSlider*>(widget); // cast it to a slider
        result.second = slider->value();
      }
    }
  }

  return result;
}

/*-------------------- public slots --------------------*/
void ChartHandler::currentSelectedItemsChanged(playlistItem *aItem1, playlistItem *aItem2)
{
  Q_UNUSED(aItem2)

  // get and set title
  if (this->mChartWidget->parentWidget())
    this->mChartWidget->parentWidget()->setWindowTitle(this->getChartsTitle(aItem1));

  // create the chartwidget based on the selected item
  auto widget = this->createChartWidget(aItem1);

  // show the created widget
  this->mChartWidget->setChartWidget(widget);

  // check if playbackcontroller has changed the value if another file is selected
  this->playbackControllerFrameChanged(-1); // we can use -1 as Index, because the index of the frame will be get later by another function
}

void ChartHandler::itemAboutToBeDeleted(playlistItem *aItem)
{
  itemWidgetCoord coord;
  coord.mItem = aItem;

  // check if we hold the widget from the item
  if (mListItemWidget.contains(coord))
    coord = mListItemWidget.at(mListItemWidget.indexOf(coord));
  else // we dont hold?! why
    return;

  // remove the chart from the shown stack
  this->mChartWidget->removeChartWidget(coord.mWidget);
  //remove the item-widget tupel from our list
  this->removeWidgetFromList(aItem);
}

void ChartHandler::playbackControllerFrameChanged(int aNewFrameIndex)
{
  Q_UNUSED(aNewFrameIndex)

  if(!this->mCbDrawChart->isChecked())
    return;

  // get the selected playListItemStatisticFiles-item
  auto items = this->mPlaylist->getSelectedItems();
  bool anyItemsSelected = items[0] != NULL || items[1] != NULL;

  // check that really something is selected
  if(anyItemsSelected)
  {
    // now we need the combination, try to find it
    itemWidgetCoord coord = this->getItemWidgetCoord(items[0]);

    // check which show-option is selected
    // if something other is selected than csPerFrame, we can leave
    if(coord.mWidget) // only do the part, if we have created the widget before
    {
      QVariant showVariant(csUnknown);
      QObjectList children = coord.mWidget->children();
      foreach (auto child, children)
      {
        if(dynamic_cast<QComboBox*> (child)) // finding the combobox
        {
          if(child->objectName() == OPTION_NAME_CBX_CHART_FRAMESHOW)
          {
            showVariant = (dynamic_cast<QComboBox*>(child))->itemData((dynamic_cast<QComboBox*>(child))->currentIndex());
            break;
          }
        }
      }
      chartShow showart = showVariant.value<chartShow>();

      if(showart != csPerFrame)
        return;
    }
    QWidget* chart; // just a holder, will be set in the following

    // check what form of playlistitem was selected
    if(dynamic_cast<playlistItemStatisticsFile*> (items[0]))
      chart = this->createStatisticsChart(coord);
    else
      // the selected item is not defined at this point, so show a default
      chart = &(this->mNoDataToShowWidget);

    this->placeChart(coord, chart);
  }
}

/*-------------------- playListItemStatisticsFile and the private slots --------------------*/
QWidget* ChartHandler::createStatisticFileWidget(playlistItemStatisticsFile *aItem, itemWidgetCoord& aCoord)
{
  //define a simple layout for the statistic file
  QWidget *basicWidget      = new QWidget;
  QVBoxLayout *basicLayout  = new QVBoxLayout(basicWidget);
  QComboBox* cbxTypes       = new QComboBox;
  QLabel* lblStat           = new QLabel(CBX_LABEL_STATISTICS_TYPE);
  QFormLayout* topLayout    = new QFormLayout;

  //setting name for the combobox, to find it later dynamicly
  cbxTypes->setObjectName(OPTION_NAME_CBX_CHART_TYPES);

  StatisticsTypeList statTypeList = aItem->getStatisticsHandler()->getStatisticsTypeList();

  //check if map contains items
  if(statTypeList.count() > 0)
  {
    //map has items, so add them
    cbxTypes->addItem(CBX_OPTION_SELECT);

    foreach (StatisticsType statType , statTypeList)
      cbxTypes->addItem(statType.typeName); // fill with data
  }
  else
    // no items, add a info
    cbxTypes->addItem(CBX_OPTION_NO_TYPES);

  // @see http://stackoverflow.com/questions/16794695/connecting-overloaded-signals-and-slots-in-qt-5
  // do the connect after adding the items otherwise the connect will be call
  connect(cbxTypes,
          static_cast<void (QComboBox::*)(const QString &)> (&QComboBox::currentIndexChanged),
          this,
          &ChartHandler::onStatisticsChange);
  // this will connect checks that the order combobox is enabled or disabled. disable the combobox for the type "Select..."
  connect(cbxTypes,
          static_cast<void (QComboBox::*)(const QString &)> (&QComboBox::currentIndexChanged),
          this,
          &ChartHandler::switchOrderEnableStatistics);

  // getting the list to the order by - components
  QList<QWidget*> listGeneratedWidgets = this->generateOrderWidgetsOnly(cbxTypes->count() > 1);
  bool hasOddAmount = listGeneratedWidgets.count() % 2 == 1;

  // adding the components, check how we add them
  if(hasOddAmount)
  {
    topLayout->addWidget(lblStat);
    topLayout->addWidget(cbxTypes);
  }
  else
    topLayout->addRow(lblStat, cbxTypes);

  // adding the widgets from the list to the toplayout
  // we do this here at this way, because if we use generateOrderByLayout we get a distance-difference in the layout
  foreach (auto widget, listGeneratedWidgets)
  {
    if(hasOddAmount)
      topLayout->addWidget(widget);

    if((widget->objectName() == OPTION_NAME_CBX_CHART_FRAMESHOW)
       || (widget->objectName() == OPTION_NAME_CBX_CHART_GROUPBY)
       || (widget->objectName() == OPTION_NAME_CBX_CHART_NORMALIZE))
      // finding the combobox and define the action
    {
      connect(dynamic_cast<QComboBox*> (widget),
              static_cast<void (QComboBox::*)(const QString &)> (&QComboBox::currentIndexChanged),
              this,
              &ChartHandler::onStatisticsChange);
      connect(dynamic_cast<QComboBox*> (widget),
              static_cast<void (QComboBox::*)(const QString &)> (&QComboBox::currentIndexChanged),
              this,
              &ChartHandler::switchOrderEnableStatistics);
    }
  }

  if(!hasOddAmount)
    for (int i = 0; i < listGeneratedWidgets.count(); i +=2) // take care, we increment i every time by 2!!
      topLayout->addRow(listGeneratedWidgets.at(i), listGeneratedWidgets.at(i+1));

  // generate groubbox for 3D-Data
  CollapsibleWidget* collapseGroup = new CollapsibleWidget("3D-Data options");

  QGridLayout* lyGrid3dLimits = new QGridLayout();

  QLabel* lblXLimNegative = new QLabel("-x limit:");
  QLineEdit* edXLimNegative = new QLineEdit();

  QLabel* lblXLimPositive = new QLabel("x limit:");
  QLineEdit* edXLimPositive = new QLineEdit();

  QLabel* lblYLimNegative = new QLabel("-y limit:");
  QLineEdit* edYLimNegative = new QLineEdit();

  QLabel* lblYLimPositive = new QLabel("y limit:");
  QLineEdit* edYLimPositive = new QLineEdit();

  // setting the object-names
  edXLimNegative->setObjectName(EDIT_NAME_LIMIT_NEGX);
  edXLimPositive->setObjectName(EDIT_NAME_LIMIT_POSX);
  edYLimNegative->setObjectName(EDIT_NAME_LIMIT_NEGY);
  edYLimPositive->setObjectName(EDIT_NAME_LIMIT_POSY);

  // setting some placeholders
  edXLimNegative->setPlaceholderText("negative x");
  edXLimPositive->setPlaceholderText("positive x");
  edYLimNegative->setPlaceholderText("negative y");
  edYLimPositive->setPlaceholderText("positive y");

  // only numbers are allowed
  edXLimNegative->setValidator(new QIntValidator(INT_MIN, INT_MAX, this));
  edXLimPositive->setValidator(new QIntValidator(INT_MIN, INT_MAX, this));
  edYLimNegative->setValidator(new QIntValidator(INT_MIN, INT_MAX, this));
  edYLimPositive->setValidator(new QIntValidator(INT_MIN, INT_MAX, this));

  // position the elements
  lyGrid3dLimits->addWidget(lblXLimNegative, 0, 0);
  lyGrid3dLimits->addWidget(edXLimNegative,  0, 1);
  lyGrid3dLimits->addWidget(lblXLimPositive, 0, 2);
  lyGrid3dLimits->addWidget(edXLimPositive,  0, 3);

  lyGrid3dLimits->addWidget(lblYLimNegative, 1, 0);
  lyGrid3dLimits->addWidget(edYLimNegative,  1, 1);
  lyGrid3dLimits->addWidget(lblYLimPositive, 1, 2);
  lyGrid3dLimits->addWidget(edYLimPositive,  1, 3);

  QCheckBox* cbLimit64 = new QCheckBox("set limits from -64 to 64 for x and y");
  QCheckBox* cbLimit128 = new QCheckBox("set limits from -128 to 128 for x and y");

  lyGrid3dLimits->addWidget(cbLimit64, 2, 0);
  lyGrid3dLimits->addWidget(cbLimit128,  2, 2);

  // connects to react if text changed
  { // mostly same code for all connects, but we cant implement in one method, because of the signal-slot-machenism we need a method with only one parameter. To check everything we need about 3 or 4 parameters
    connect(edXLimNegative, &QLineEdit::textChanged, this, [edXLimNegative, edXLimPositive, this](QString aString) {
        Q_UNUSED(aString)
        // getting the strings from the edits
        QString xnegstr = edXLimNegative->text();
        QString xposstr = edXLimPositive->text();

        // just the minus for the negative number
        if((xnegstr == "-") || (xposstr == "-"))
          return;

        int xneg = xnegstr.toInt();
        int xpos = xposstr.toInt();

        // if xpos has no string, we have no value
        if(xposstr == "")
          xpos = INT_MAX;

        // if xneg has no string
        if(xnegstr == "")
          xneg = INT_MIN;

        // check: if negative limit is greater
        if(xneg > xpos)
        {
          QMessageBox::information(NULL, "x-limits", "The negative limit (" + xnegstr + ") is greater than the positive (" + xposstr + ") limit.", QMessageBox::Ok);
          edXLimNegative->blockSignals(true);
          edXLimNegative->setText(QString::number(xpos - 1));
          edXLimNegative->blockSignals(false);
          return;
        }

        playbackControllerFrameChanged(42);
      }
    );

    connect(edXLimPositive, &QLineEdit::textChanged, this, [edXLimNegative, edXLimPositive, this](QString aString) {
        Q_UNUSED(aString)
        QString xnegstr = edXLimNegative->text();
        QString xposstr = edXLimPositive->text();

        // just the minus for the negative number
        if((xnegstr == "-") || (xposstr == "-"))
          return;

        int xneg = xnegstr.toInt();
        int xpos = xposstr.toInt();

        // if xpos has no string, we have no value
        if(xposstr == "")
          xpos = INT_MAX;

        // if xneg has no string
        if(xnegstr == "")
          xneg = INT_MIN;

        // check: if negative limit is greater
        if(xneg > xpos)
        {
          QMessageBox::information(NULL, "x-limits", "The negative limit (" + xnegstr + ") is greater than the positive (" + xposstr + ") limit.", QMessageBox::Ok);
          edXLimNegative->blockSignals(true);
          edXLimNegative->setText(QString::number(xpos - 1));
          edXLimNegative->blockSignals(false);
          return;
        }

        playbackControllerFrameChanged(42);
      }
    );

    connect(edYLimNegative, &QLineEdit::textChanged, this, [edYLimNegative, edYLimPositive, this](QString aString) {
        Q_UNUSED(aString)
        QString ynegstr = edYLimNegative->text();
        QString yposstr = edYLimPositive->text();

        // just the minus for the negative number
        if((ynegstr == "-") || (yposstr == "-"))
          return;

        int yneg = ynegstr.toInt();
        int ypos = yposstr.toInt();

        // if xpos has no string, we have no value
        if(yposstr == "")
          ypos = INT_MAX;

        // if xneg has no string
        if(ynegstr == "")
          yneg = INT_MIN;

        // check: if negative limit is greater
        if(yneg > ypos)
        {
          QMessageBox::information(NULL, "y-limits", "The negative limit (" + ynegstr + ") is greater than the positive (" + yposstr + ") limit.", QMessageBox::Ok);
          edYLimNegative->blockSignals(true);
          edYLimNegative->setText(QString::number(ypos - 1));
          edYLimNegative->blockSignals(false);
          return;
        }

        playbackControllerFrameChanged(42);
      }
    );

    connect(edYLimPositive, &QLineEdit::textChanged, this, [edYLimNegative, edYLimPositive, this](QString aString) {
        Q_UNUSED(aString)
        QString ynegstr = edYLimNegative->text();
        QString yposstr = edYLimPositive->text();

        // just the minus for the negative number
        if((ynegstr == "-") || (yposstr == "-"))
          return;

        int yneg = ynegstr.toInt();
        int ypos = yposstr.toInt();

        // if xpos has no string, we have no value
        if(yposstr == "")
          ypos = INT_MAX;

        // if xneg has no string
        if(ynegstr == "")
          yneg = INT_MIN;

        // check: if negative limit is greater
        if(yneg > ypos)
        {
          QMessageBox::information(NULL, "y-limits", "The negative limit (" + ynegstr + ") is greater than the positive (" + yposstr + ") limit.", QMessageBox::Ok);
          edYLimNegative->blockSignals(true);
          edYLimNegative->setText(QString::number(ypos- 1));
          edYLimNegative->blockSignals(false);
          return;
        }

        playbackControllerFrameChanged(42);
      }
    );

    connect(cbLimit64, &QCheckBox::clicked, this, [cbLimit64, cbLimit128, edXLimNegative, edXLimPositive, edYLimNegative, edYLimPositive, this] (bool aClicked) {
        Q_UNUSED(aClicked)
        edXLimNegative->blockSignals(true);
        edXLimNegative->setText(QString::number(-64));
        edXLimNegative->blockSignals(false);

        edXLimPositive->blockSignals(true);
        edXLimPositive->setText(QString::number(64));
        edXLimPositive->blockSignals(false);

        edYLimNegative->blockSignals(true);
        edYLimNegative->setText(QString::number(-64));
        edYLimNegative->blockSignals(false);

        edYLimPositive->blockSignals(true);
        edYLimPositive->setText(QString::number(64));
        edYLimPositive->blockSignals(false);

        cbLimit128->blockSignals(true);
        cbLimit128->setChecked(false);
        cbLimit128->blockSignals(false);

        playbackControllerFrameChanged(42);
      }
    );

    connect(cbLimit128, &QCheckBox::clicked, this, [cbLimit64, cbLimit128, edXLimNegative, edXLimPositive, edYLimNegative, edYLimPositive, this] (bool aClicked) {
        Q_UNUSED(aClicked)
        edXLimNegative->blockSignals(true);
        edXLimNegative->setText(QString::number(-128));
        edXLimNegative->blockSignals(false);

        edXLimPositive->blockSignals(true);
        edXLimPositive->setText(QString::number(128));
        edXLimPositive->blockSignals(false);

        edYLimNegative->blockSignals(true);
        edYLimNegative->setText(QString::number(-128));
        edYLimNegative->blockSignals(false);

        edYLimPositive->blockSignals(true);
        edYLimPositive->setText(QString::number(128));
        edYLimPositive->blockSignals(false);

        cbLimit64->blockSignals(true);
        cbLimit64->setChecked(false);
        cbLimit64->blockSignals(false);

        playbackControllerFrameChanged(42);
      }
    );
  }

  //set content to our collapse-widget
  collapseGroup->setContentLayout(*lyGrid3dLimits);

  // at least, add to the widget
  topLayout->addRow(collapseGroup);
  topLayout->addRow(new QLabel("draw chart"), this->mCbDrawChart);

  // add all to our layout
  basicLayout->addLayout(topLayout);
  basicLayout->addWidget(aCoord.mChart);

  return basicWidget;
}

QWidget* ChartHandler::createStatisticsChart(itemWidgetCoord& aCoord)
{
  // TODO -oCH: maybe save a created statistic-chart by an key

  // if aCoord.mWidget is null, can happen if loading a new file and the playbackcontroller will be set to 0
  // return that we cant show data
  if(!aCoord.mWidget)
    return &(this->mNoDataToShowWidget);

  // get current frame index, we use the playback controller
  int frameIndex = this->mPlayback->getCurrentFrame();

  QString type("");
  QVariant showVariant(csUnknown);
  QVariant groupVariant(cgbUnknown);
  QVariant normaVariant(cnUnknown);

  // we need the selected StatisticType, so we have to find the combobox and get the text of it
  QObjectList children = aCoord.mWidget->children();
  foreach (auto child, children)
  {
    if(dynamic_cast<QComboBox*> (child)) // finding the combobox
    {
      // we need to differentiate between the type-combobox and order-combobox, but we have to find both values
      if(child->objectName() == OPTION_NAME_CBX_CHART_TYPES)
        type = (dynamic_cast<QComboBox*>(child))->currentText();
      else if(child->objectName() == OPTION_NAME_CBX_CHART_FRAMESHOW)
        showVariant = (dynamic_cast<QComboBox*>(child))->itemData((dynamic_cast<QComboBox*>(child))->currentIndex());
      else if(child->objectName() == OPTION_NAME_CBX_CHART_GROUPBY)
        groupVariant = (dynamic_cast<QComboBox*>(child))->itemData((dynamic_cast<QComboBox*>(child))->currentIndex());
      else if(child->objectName() == OPTION_NAME_CBX_CHART_NORMALIZE)
        normaVariant = (dynamic_cast<QComboBox*>(child))->itemData((dynamic_cast<QComboBox*>(child))->currentIndex());

      // all found, so we can leave here
      //! take care, this is one if-statement
      if((type != "")
         && (showVariant.value<chartShow>() != csUnknown)
         && (groupVariant.value<chartGroupBy>() != cgbUnknown)
         && (normaVariant.value<chartNormalize>() != cnUnknown))
        break;
    }
  }

  // type was not found, so we return a default
  if(type == "" || type == CBX_OPTION_SELECT)
    return &(this->mNoDataToShowWidget);

  // results for the lambda
  QString negXstring = "";
  QString posXstring = "";
  QString negYstring = "";
  QString posYstring = "";

  // small lambda to find the edits with the max and min of 3d data
  std::function<void(QObjectList)> findAndSetNumberStrings3D = [&] (QObjectList aChildrenList)
  {
    foreach (auto child, aChildrenList)
    {
      if(child->children().count() > 1)
        findAndSetNumberStrings3D(child->children());
      else
      {
        if(child->objectName() == EDIT_NAME_LIMIT_NEGX)
          negXstring = (dynamic_cast<QLineEdit*>(child))->text();
        if(child->objectName() == EDIT_NAME_LIMIT_POSX)
          posXstring = (dynamic_cast<QLineEdit*>(child))->text();
        if(child->objectName() == EDIT_NAME_LIMIT_NEGY)
          negYstring = (dynamic_cast<QLineEdit*>(child))->text();
        if(child->objectName() == EDIT_NAME_LIMIT_POSY)
          posYstring = (dynamic_cast<QLineEdit*>(child))->text();
      }
    }
  };

  // call the lambda not only define it
  findAndSetNumberStrings3D(children);

  chartOrderBy order = cobUnknown; // set an default
  // we dont have found the sort-order so set it
  chartShow showart = showVariant.value<chartShow>();

  if(showart == csPerFrame)
  {
    // this is for the case, that the playlistitem selection changed and the playbackcontroller has a selected frame
    // which is greater than the maxFrame of the new selected file
    indexRange maxrange = aCoord.mItem->getStartEndFrameLimits();
    if(frameIndex > maxrange.second)
    {
      frameIndex = maxrange.second;
      this->mPlayback->setCurrentFrame(frameIndex);
    }
  }

  if((showart != csUnknown)
     && (groupVariant.value<chartGroupBy>() != cgbUnknown)
     && (normaVariant.value<chartNormalize>() != cnUnknown))
    // get selected one
    order = EnumAuxiliary::makeChartOrderBy(showVariant.value<chartShow>(), groupVariant.value<chartGroupBy>(), normaVariant.value<chartNormalize>());

  if(showart == csAllFrames && order == this->mLastChartOrderBy && type == this->mLastStatisticsType)
    return this->mLastStatisticsWidget;
  else
  {
    this->mLastChartOrderBy = order;
    this->mLastStatisticsType = type;
  }

  // and at this point we create the statistic
  indexRange range;
  if(showart == csPerFrame)
  {
    range.first = frameIndex;
    range.second = frameIndex;
  }
  else if(showart == csAllFrames)
    range = aCoord.mItem->getFrameIdxRange();
  else if(showart == csRange)
    range = this->getFrameRange(aCoord);
  else // this case should never happen, but who now :D
    return &(this->mNoDataToShowWidget);

  //define the max and min; set INT_MIN as min and INT_MAX as max as default
  int negX = INT_MIN;
  if(negXstring != "")
    negX = negXstring.toInt();

  int posX = INT_MAX;
  if(posXstring != "")
    posX = posXstring.toInt();

  int negY = INT_MIN;
  if(negYstring != "")
    negY = negYstring.toInt();

  int posY = INT_MAX;
  if(posYstring != "")
    posY = posYstring.toInt();

  this->mYUVChartFactory.set3DCoordinationRange(negX, posX, negY, posY);

  this->mLastStatisticsWidget = this->mYUVChartFactory.createChart(order, aCoord.mItem, range, type);

  return this->mLastStatisticsWidget;
}

void ChartHandler::onStatisticsChange(const QString aString)
{
  if(!this->mCbDrawChart->isChecked())
    return;

  // get the selected playListItemStatisticFiles-item
  auto items = this->mPlaylist->getSelectedItems();
  bool anyItemsSelected = items[0] != NULL || items[1] != NULL;
  if(anyItemsSelected) // check that really something is selected
  {
    // now we need the combination, try to find it
    itemWidgetCoord coord = this->getItemWidgetCoord(items[0]);
    // necessary at this point, because now we have all the data
    this->setRangeToComponents(coord, NULL);

    QWidget* chart;
    if(aString != CBX_OPTION_SELECT) // new type was selected in the combobox
      chart = this->createStatisticsChart(coord); // so we generate the statistic
    else // "Select..." was selected so
    {
      // we set a default-widget
      chart = this->mChartWidget->getDefaultWidget();

      // remove all, cause we dont need them anymore
      for(int i = coord.mChart->count(); i >= 0; i--)
      {
        QWidget* widget = coord.mChart->widget(i);
        coord.mChart->removeWidget(widget);
      }
    }

    // at least, we have to place the chart and show it
    this->placeChart(coord, chart);
  }
}

void ChartHandler::switchOrderEnableStatistics(const QString aString)
{
  // a small lambda function, to reduce same code
  auto switchEnableStatus = [] (bool aEnabled, QObjectList aChildrenList)
  {
    foreach (auto child, aChildrenList)
    {
      if(child->children().count() > 1)
      {
        foreach (auto innerchild, child->children())
        {
          QString innerobjectname = innerchild->objectName();
          if(innerobjectname == "")
            continue;
          if(   innerobjectname == LABEL_FRAME_RANGE_BEGIN
             || innerobjectname == LABEL_FRAME_RANGE_END
             || innerobjectname == SLIDER_FRAME_RANGE_BEGIN
             || innerobjectname == SLIDER_FRAME_RANGE_END
             || innerobjectname == SPINBOX_FRAME_RANGE_BEGIN
             || innerobjectname == SPINBOX_FRAME_RANGE_END)
          {
            (dynamic_cast<QWidget*>(innerchild))->setEnabled(aEnabled);
          }
        }
      }
      else
      {
        QString innerobjectname = child->objectName();
        if(   innerobjectname == LABEL_FRAME_RANGE_BEGIN
           || innerobjectname == LABEL_FRAME_RANGE_END
           || innerobjectname == SLIDER_FRAME_RANGE_BEGIN
           || innerobjectname == SLIDER_FRAME_RANGE_END
           || innerobjectname == SPINBOX_FRAME_RANGE_BEGIN
           || innerobjectname == SPINBOX_FRAME_RANGE_END)
        {
          (dynamic_cast<QWidget*>(child))->setEnabled(aEnabled);
        }
      }
    }
  };

  // TODO -oCH:think about getting a better and faster solution

  // aString is the selected value from the Type-combobox from the playliststatisticsfilewidget
  // get the selected playListItemStatisticFiles-item
  auto items = this->mPlaylist->getSelectedItems();
  bool anyItemsSelected = items[0] != NULL || items[1] != NULL;
  if(anyItemsSelected) // check that really something is selected
  {
    // now we need the combination, try to find it
    itemWidgetCoord coord = this->getItemWidgetCoord(items[0]);

    // we need the order-combobox, so we have to find the combobox and get the text of it
    QObjectList children = coord.mWidget->children();
    foreach (auto child, children)
    {
      if(dynamic_cast<QComboBox*> (child)) // check if child is combobox
      {
        QString objectname = child->objectName();
        if((objectname == OPTION_NAME_CBX_CHART_FRAMESHOW)
           || (child->objectName() == OPTION_NAME_CBX_CHART_GROUPBY)
           || (child->objectName() == OPTION_NAME_CBX_CHART_NORMALIZE)) // check if found child the correct combobox
          (dynamic_cast<QComboBox*>(child))->setEnabled(aString != CBX_OPTION_SELECT);

        // at this point, we check which kind of frame-show is selected
        if(objectname == OPTION_NAME_CBX_CHART_FRAMESHOW)
        {
          // getting the showkind
          chartShow selectedShow = (dynamic_cast<QComboBox*>(child))->itemData((dynamic_cast<QComboBox*>(child))->currentIndex()).value<chartShow>();
          if(selectedShow == csRange) // the frame range is selected
            switchEnableStatus(true, children); // enable the sliders / spinboxes etc
          else // was not selected
            switchEnableStatus(false, children); // disable the sliders / spinboxes etc
        }
      }
    }
  }
}

void ChartHandler::sliderRangeChange(int aValue)
{
  Q_UNUSED(aValue)
  this->rangeChange(true, false);
}

void ChartHandler::spinboxRangeChange(int aValue)
{
  Q_UNUSED(aValue)
  this->rangeChange(false, true);
}

void ChartHandler::timerEvent(QTimerEvent *event)
{
  if(event->timerId() != this->mTimer.timerId())
    return;

  auto items = this->mPlaylist->getSelectedItems();
  bool anyItemsSelected = items[0] != NULL || items[1] != NULL;
  if(!anyItemsSelected) // check that really something is selected
    return;

  this->currentSelectedItemsChanged(items[0], items[0]);
}
