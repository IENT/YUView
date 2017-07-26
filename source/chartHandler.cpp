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

//#include "statisticsExtensions.h"

// Default-Constructor
ChartHandler::ChartHandler()
{
  // creating the default widget if no data is avaible
  QVBoxLayout  nodataLayout(&(this->mNoDataToShowWidget));
  nodataLayout.addWidget(new QLabel("No data to show."));

  // creating the default widget if data is loading
  QVBoxLayout dataLoadingLayout(&(this->mDataIsLoadingWidget));
  dataLoadingLayout.addWidget(new QLabel("Data is loading.\nPlease wait."));
}

/*-------------------- public functions --------------------*/
QWidget* ChartHandler::createChartWidget(playlistItem *aItem)
{
  // check if the widget was already created and stored
  itemWidgetCoord coord;
  coord.mItem = aItem;

  if (mListItemWidget.contains(coord)) // was stored
    return mListItemWidget.at(mListItemWidget.indexOf(coord)).mWidget;

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

// every item will get a specified title, if null or item-type was not found, default-title will return
QString ChartHandler::getStatisticTitle(playlistItem *aItem)
{
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
  if (mListItemWidget.contains(tmp))
    this->mListItemWidget.removeAll(tmp);
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


  // this is for the case, that the playlistitem selection changed and the playbackcontroller has a selected frame
  // which is greater than the maxFrame of the new selected file
  indexRange maxrange = aCoord.mItem->getStartEndFrameLimits();
  if(frameIndex > maxrange.second)
  {
    frameIndex = maxrange.second;
    this->mPlayback->setCurrentFrame(frameIndex);
  }

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
      if(child->objectName() == "cbxTypes")
        type = (dynamic_cast<QComboBox*>(child))->currentText();
      else if(child->objectName() == this->mObNaCbxChartFrameShow)
        showVariant = (dynamic_cast<QComboBox*>(child))->itemData((dynamic_cast<QComboBox*>(child))->currentIndex());
      else if(child->objectName() == this->mObNaCbxChartGroupBy)
        groupVariant = (dynamic_cast<QComboBox*>(child))->itemData((dynamic_cast<QComboBox*>(child))->currentIndex());
      else if(child->objectName() == this->mObNaCbxChartNormalize)
        normaVariant = (dynamic_cast<QComboBox*>(child))->itemData((dynamic_cast<QComboBox*>(child))->currentIndex());

      // all found, so we can leave here
      if((type != "")
         && (showVariant.value<ChartShow>() != csUnknown)
         && (groupVariant.value<ChartGroupBy>() != cgbUnknown)
         && (normaVariant.value<ChartNormalize>() != cnUnknown))
        break;
    }
  }

  // type was not found, so we return a default
  if(type == "" || type == "Select...")
    return &(this->mNoDataToShowWidget);


  ChartOrderBy order = cobPerFrameGrpByBlocksizeNrmNone; // set an default
  // we dont have found the sort-order so set it
  if((showVariant.value<ChartShow>() != csUnknown)
     && (groupVariant.value<ChartGroupBy>() != cgbUnknown)
     && (normaVariant.value<ChartNormalize>() != cnUnknown))
    // get selected one
    order = EnumAuxiliary::makeChartOrderBy(showVariant.value<ChartShow>(), groupVariant.value<ChartGroupBy>(), normaVariant.value<ChartNormalize>());


  QList<collectedData>* sortedData;

  // now we sort and categorize the data
  if(showVariant.value<ChartShow>() == csPerFrame) // we just have one frame
    sortedData = this->sortAndCategorizeData(aCoord, type, frameIndex);
  else // we look at all frames
    sortedData = this->sortAndCategorizeDataAllFrames(aCoord, type);

  // and at this point we create the statistic
  return this->makeStatistic(sortedData, order);
}

/*-------------------- private functions --------------------*/

itemWidgetCoord ChartHandler::getItemWidgetCoord(playlistItem *aItem)
{
  itemWidgetCoord coord;
  coord.mItem = aItem;

  if (mListItemWidget.contains(coord))
    coord = mListItemWidget.at(mListItemWidget.indexOf(coord));
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
  QLabel* lblOptionsShow    = new QLabel(tr("Show for: "));
  // furthermore we need the combobox
  QComboBox* cbxOptionsShow = new QComboBox;

  // we need a label for a simple text-information
  QLabel* lblOptionsGroup    = new QLabel(tr("Group by: "));
  // furthermore we need the combobox
  QComboBox* cbxOptionsGroup = new QComboBox;

  // we need a label for a simple text-information
  QLabel* lblOptionsNormalize    = new QLabel(tr("Normalize: "));
  // furthermore we need the combobox
  QComboBox* cbxOptionsNormalize = new QComboBox;

  // set options name, to find the combobox later dynamicly
  cbxOptionsShow->setObjectName(this->mObNaCbxChartFrameShow);
  cbxOptionsGroup->setObjectName(this->mObNaCbxChartGroupBy);
  cbxOptionsNormalize->setObjectName(this->mObNaCbxChartNormalize);

  // disable the combobox first, as default. it should be enabled later
  cbxOptionsShow->setEnabled(false);
  cbxOptionsGroup->setEnabled(false);
  cbxOptionsNormalize->setEnabled(false);

  // adding the options with the enum ChartOrderBy
  if(aAddOptions)
  {
    // adding the show options
    cbxOptionsShow->addItem(EnumAuxiliary::asString(csPerFrame), csPerFrame);
    cbxOptionsShow->setItemData(0, EnumAuxiliary::asTooltip(csPerFrame), Qt::ToolTipRole);

    cbxOptionsShow->addItem(EnumAuxiliary::asString(csAllFrames), csAllFrames);
    cbxOptionsShow->setItemData(1, EnumAuxiliary::asTooltip(csAllFrames), Qt::ToolTipRole);

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

QList<collectedData>* ChartHandler::sortAndCategorizeData(const itemWidgetCoord aCoord, const QString aType, const int aFrameIndex)
{
  //prepare the result
  QMap<QString, QMap<int, int*>*>* dataMap = new QMap<QString, QMap<int, int*>*>;

  // getting allData from the type
  QList<QList<QVariant>> allData = aCoord.mData->value(aType);

  // getting the data depends on the actual selected frameIndex / POC
  QList<QVariant> data = allData.at(aFrameIndex);

  // now we go thru all elements of the frame
  foreach (QVariant item, data)
  {
    // item-value was defined by statisticsItem_Value @see statisticsExtensions.h
    if(item.canConvert<statisticsItem_Value>())
    {
      statisticsItem_Value value = item.value<statisticsItem_Value>();
      // creating the label: height x width
      QString label = QString::number(value.size[0]) + "x" + QString::number(value.size[1]);

      int* chartDepthCnt;

      // hard part of the function
      // 1. check if label is in map
      // 2. if not: insert label and a new / second Map with the new values for depth
      // 3. if it was inside: check if Depth was inside the second map
      // 4. if not in second map create new Depth-data-container, fill with data and add to second map
      // 5. if it was in second map just increment the Depth-Counter
      if(!dataMap->contains(label))
      {
        // label was not inside
        QMap<int, int*>* map = new QMap<int, int*>();

        // create Data, set to 0 and increment (or set count to the value 1, same as set to 0 and increment) and add to second map
        chartDepthCnt = new int[2];

        chartDepthCnt[0] = value.value;
        chartDepthCnt[1] = 1;

        map->insert(chartDepthCnt[0], chartDepthCnt);
        dataMap->insert(label, map);
      }
      else
      {
        // label was inside, check if Depth-value is inside
        QMap<int, int*>* map = dataMap->value(label);

        // Depth-Value not inside
        if(!(map->contains(value.value)))
        {
          chartDepthCnt = new int[2];                   // creating new result
          chartDepthCnt[0] = value.value;               // holding the value
          chartDepthCnt[1] = 0;                         // initialise counter to 0
          chartDepthCnt[1]++;                           // increment the counter
          map->insert(chartDepthCnt[0], chartDepthCnt); // at least add to list
        }
        else  // Depth-Value was inside
        {
          // finding the result, every item "value" is just one time in the list
          int* counter = map->value(value.value);
          counter[1]++; // increment the counter
        }
      }
    }
  }

  // at least we order the data based on the width & height (from low to high) and make the data handling easier
  QList<collectedData>* resultData = new QList<collectedData>;

  // setting data and search optionscbxOptionsGroup
  long smallestFoundNumber = LONG_LONG_MAX;
  QString numberString = "";
  int maxElementsToNeed = dataMap->keys().count();

  while(resultData->count() < maxElementsToNeed)
  {
    QString key = ""; // just a holder

    // getting the smallest number and the label
    foreach (QString label, dataMap->keys())
    {
      numberString.clear(); // cleaning the String
      for (int run = 0; run < label.length(); run++)
      {
        if(label[run] != 'x') // finding the number befor the 'x'
         numberString.append(label[run]); // creating the number from the chars
        else // we have found the 'x' so the number is finished
          break;
      }

      int number = numberString.toInt(); // convert to int

      // check if we have found the smallest number
      if(number < smallestFoundNumber)
      {
        // found a smaller number so hold it
        smallestFoundNumber = number;
        // we hold the label, so we dont need to "create / build" the key again
        key = label;
      }

    }

    // getting the data depends on the "smallest" key
    auto map = dataMap->value(key);

    collectedData data;   // creating the data
    data.mLabel = key;    // setting the label

    // copy each data into the list
    foreach (int value, map->keys())
      data.mValueList.append(map->value(value));

    // appending the collectedData to the result
    resultData->append(data);

    // reset settings to find
    dataMap->remove(key);
    smallestFoundNumber = INT_MAX;
    key.clear();
  }

  // we can delete the dataMap, cause we dont need anymore
  delete dataMap;

//  // a debug output
//  for(int i = 0; i< resultData->count(); i++) {
//    collectedData cd = resultData->at(i);
//    foreach (int* valuePair, cd.mValueList) {
//      QString debugstring(cd.mLabel + ": " + QString::number(valuePair[0]) + " : " + QString::number(valuePair[1]));
//      qDebug() << debugstring;
//    }
//  }

  return resultData;
}

QList<collectedData>* ChartHandler::sortAndCategorizeDataAllFrames(const itemWidgetCoord aCoord, const QString aType)
{
  QList<collectedData>* result = new QList<collectedData>();

  // ok, we have to go thru all frames!
  // first get the range
  indexRange range = aCoord.mItem->getStartEndFrameLimits();

  // next step get the data for each frame
  for (int frame = range.first; frame < range.second; frame++)
  {
    // get the data for the actual frame
    QList<collectedData>* collectedDataByFrameList = this->sortAndCategorizeData(aCoord, aType, frame);

    // now we have to integrate the new Data from one Frame to all other frames
    for(int i = 0; i< collectedDataByFrameList->count(); i++)
    {

    }
  }

  return result;
}

QWidget* ChartHandler::makeStatistic(QList<collectedData>* aSortedData, const ChartOrderBy aOrderBy)
{
  // if we have no keys, we cant show any data so return at this point
  if(!aSortedData->count())
    return &(this->mNoDataToShowWidget);

  chartSettingsData settings;

  switch (aOrderBy) {
    case cobPerFrameGrpByValueNrmNone:
      settings = this->makeStatisticsPerFrameGrpByValNrmNone(aSortedData);
      break;
    case cobPerFrameGrpByValueNrmByArea:
      settings = this->makeStatisticsPerFrameGrpByValNrmArea(aSortedData);
      break;
    case cobPerFrameGrpByBlocksizeNrmNone:
      settings = this->makeStatisticsPerFrameGrpByBlocksizeNrmNone(aSortedData);
      break;
    case cobPerFrameGrpByBlocksizeNrmByArea:
      settings = this->makeStatisticsPerFrameGrpByBlocksizeNrmArea(aSortedData);
      break;
    case cobAllFramesGrpByValueNrmNone:
      settings = this->makeStatisticsAllFramesGrpByValNrmNone(aSortedData);
      break;
    case cobAllFramesGrpByValueNrmByArea:
      settings = this->makeStatisticsAllFramesGrpByValNrmArea(aSortedData);
      break;
    case cobAllFramesGrpByBlocksizeNrmNone:
      settings = this->makeStatisticsAllFramesGrpByBlocksizeNrmNone(aSortedData);
      break;
    case cobAllFramesGrpByBlocksizeNrmByArea:
      settings = this->makeStatisticsAllFramesGrpByBlocksizeNrmArea(aSortedData);
      break;
    default:
      return &(this->mNoDataToShowWidget);
  }

  if(!settings.mSettingsIsValid)
    return &(this->mNoDataToShowWidget);

  // creating the result
  QChart* chart = new QChart();

  // appending the series to the chart
  chart->addSeries(settings.mSeries);
  // setting an animationoption (not necessary but it's nice to see)
  chart->setAnimationOptions(QChart::SeriesAnimations);
  // creating default-axes: always have to be called before you add some custom axes
  chart->createDefaultAxes();


  // if we have set any categories, we can add a custom x-axis
  if(settings.mCategories.count() > 0)
  {
    QBarCategoryAxis *axis = new QBarCategoryAxis();
    axis->setCategories(settings.mCategories);
    chart->setAxisX(axis, settings.mSeries);
  }

  // setting Options for the chart-legend
  chart->legend()->setVisible(true);
  chart->legend()->setAlignment(Qt::AlignBottom);

  // creating result chartview and set the data
  QChartView *chartView = new QChartView(chart);
  chartView->setRenderHint(QPainter::Antialiasing);

  // final return the created chart
  return chartView;
}

chartSettingsData ChartHandler::makeStatisticsPerFrameGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData)
{
  // define result
  chartSettingsData settings;

  // just a holder
  QBarSet *set;

  // running thru the sorted Data
  for(int i = 0; i < aSortedData->count(); i++)
  {
    // first getting the data
    collectedData data = aSortedData->at(i);

    // ceate an auxiliary var's
    bool moreThanOneElement = data.mValueList.count() > 1;

    // creating the set
    set = new QBarSet(data.mLabel);

    // if we have more than one value
    foreach (int* chartData, data.mValueList)
    {
      if(moreThanOneElement)
      {
        if(!settings.mTmpCoordCategorieSet.contains(QString::number(chartData[0])))
        {
          set = new QBarSet(QString::number(chartData[0]));
          settings.mTmpCoordCategorieSet.insert(QString::number(chartData[0]), set);
        }
        else
          set = settings.mTmpCoordCategorieSet.value(QString::number(chartData[0]));

        *set << chartData[1];
      }
      else
      {
        // appending data to the set
        *set << chartData[1];
      }
    }

    // check if we just had one data for each label
    if(! moreThanOneElement)
    {
      int* chartData = data.mValueList.at(0); // getting data
      set->setLabel(data.mLabel + " (" + QString::number(chartData[0])+ ")"); // setting new label in form: "Label (value)"
      settings.mSeries->append(set);      // appending the set to the series
    }
    else
    {
      foreach (QString key, settings.mTmpCoordCategorieSet.keys())
        settings.mSeries->append(settings.mTmpCoordCategorieSet.value(key));
    }

    if(moreThanOneElement)
      //at least appending the label to the categories for the axes if necessary
       settings.mCategories << data.mLabel;
  }

  return settings;
}

chartSettingsData ChartHandler::makeStatisticsPerFrameGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData)
{
  // define result
  chartSettingsData settings;
  // just a holder
  QBarSet *set;

  // first calculate total amount of pixel
  int totalAmountPixel = 0;
  for (int i = 0; i < aSortedData->count(); i++)
  {
    // get the data
    collectedData data = aSortedData->at(i);

    // get the width and the heigth
    QStringList numberStrings = data.mLabel.split("x");
    QString widthStr  = numberStrings.at(0);
    QString heightStr = numberStrings.at(1);
    int width = widthStr.toInt();
    int height = heightStr.toInt();


    for (int j = 0; j < data.mValueList.count(); j++)
    {
      int* chartData = data.mValueList.at(j);
      totalAmountPixel += ((width * height) * chartData[1]);
    }
  }

  // calculate total amount of pixel depends on the blocksize
  for (int i = 0; i < aSortedData->count(); i++)
  {
    // get the data
    collectedData data = aSortedData->at(i);

    // get the width and the heigth
    QStringList numberStrings = data.mLabel.split("x");
    QString widthStr  = numberStrings.at(0);
    QString heightStr = numberStrings.at(1);
    int width = widthStr.toInt();
    int height = heightStr.toInt();


    int amountPixelofValue = 0;
    for (int j = 0; j < data.mValueList.count(); j++)
    {
      int* chartData = data.mValueList.at(j);
      amountPixelofValue += ((width * height) * chartData[1]);
    }

    // calculate the ratio, (remember that we have to cast one int to an double, to get a double as result)
    double ratio = (amountPixelofValue / (double)totalAmountPixel) * 100;

    // create the set
    set = new QBarSet(data.mLabel);
    // fill the set with the data
    *set << ratio;
    // appen the set to the series
    settings.mSeries->append(set);
  }

  return settings;
}

chartSettingsData ChartHandler::makeStatisticsPerFrameGrpByValNrmArea(QList<collectedData>* aSortedData)
{
  // define result
  chartSettingsData settings;
  // just a holder
  QBarSet *set;

  // first calculate total amount of pixel
  int totalAmountPixel = 0;
  for (int i = 0; i < aSortedData->count(); i++)
  {
    // get the data
    collectedData data = aSortedData->at(i);

    // get the width and the heigth
    QStringList numberStrings = data.mLabel.split("x");
    QString widthStr  = numberStrings.at(0);
    QString heightStr = numberStrings.at(1);
    int width = widthStr.toInt();
    int height = heightStr.toInt();


    for (int j = 0; j < data.mValueList.count(); j++)
    {
      int* chartData = data.mValueList.at(j);
      totalAmountPixel += ((width * height) * chartData[1]);
    }
  }

  // calculate total amount of pixel depends on the value
  for (int i = 0; i < aSortedData->count(); i++)
  {
    // get the data
    collectedData data = aSortedData->at(i);

    // get the width and the heigth
    QStringList numberStrings = data.mLabel.split("x");
    QString widthStr  = numberStrings.at(0);
    QString heightStr = numberStrings.at(1);
    int width = widthStr.toInt();
    int height = heightStr.toInt();


    for (int j = 0; j < data.mValueList.count(); j++)
    {
      int amountPixelofValue = 0;
      int* chartData = data.mValueList.at(j);

      // create the set for each value
      set = new QBarSet(QString::number(chartData[0]));

      // calculate the pixel of the value
      amountPixelofValue += ((width * height) * chartData[1]);

      // calculate the ratio, (remember that we have to cast one int to an double, to get a double as result)
      double ratio = (amountPixelofValue / (double)totalAmountPixel) * 100;

      *set << ratio;
      settings.mSeries->append(set);
    }
  }

  return settings;
}

chartSettingsData ChartHandler::makeStatisticsPerFrameGrpByValNrmNone(QList<collectedData>* aSortedData)
{
  // define result
  chartSettingsData settings;

  // we order by the value, so we want to find out how many times the value was count in this frame
  QHash<int, int*> hashValueCount;

  // we save in the QHash the value first as key and later we use it as label, and we save the total of counts to the value
  // we save the total as pointer, so we have the advantage, that we dont need to replace the last added count
  // but we have to observe that this is not so easy it might be
  // always remember if you want to change the value of an primitive datat ype which you saved as pointer
  // you have to dereference the pointer and then you can change it!

  for (int i = 0; i < aSortedData->count(); i++)
  {
    // first getting the data
    collectedData data = aSortedData->at(i);

    // if we have more than one value
    foreach (int* chartData, data.mValueList)
    {
      int* count = NULL; // at this point we need an holder for an int, but if we dont set to NULL, the system requires a pointer
      // check if we have insert the count yet
      if(hashValueCount.value(chartData[0]))
        count = hashValueCount.value(chartData[0]); // was inserted
      else
      {
        // at this point we have to get a new int by the system and we save the adress of this int in count
        // so we have later for each int a new adress! and an new int, which we can save
        count = new int(0);
        // inserting the adress to get it later back
        hashValueCount.insert(chartData[0], count);
      }

      // at least we need to sum up the data, remember, that we have to dereference count, to change the value!
      *count += chartData[1];
    }
  }

  // because of the QHash we know how many QBarSet we have to create and add to the series in settings-struct

  foreach (int key, hashValueCount.keys())
  {
    // creating the set with an label from the given Value
    QBarSet* set = new QBarSet(QString::number(key));
    //settings.mCategories << QString::number(key);
    // adding the count to the set, which we want to display
    int *count = hashValueCount.value(key); // getting the adress of the total
    *set << *count; // remenber to dereference the count to get the real value of count
    // at least add the set to the series in the settings-struct
    settings.mSeries->append(set);
  }

  return settings;
}

chartSettingsData ChartHandler::makeStatisticsAllFramesGrpByValNrmNone(QList<collectedData>* aSortedData)
{
  // define result
  chartSettingsData settings;

  // TODO delete after implement
  settings.mSettingsIsValid = false;

  return settings;
}

chartSettingsData ChartHandler::makeStatisticsAllFramesGrpByValNrmArea(QList<collectedData>* aSortedData)
{
  // define result
  chartSettingsData settings;

  // TODO delete after implement
  settings.mSettingsIsValid = false;

  return settings;
}

chartSettingsData ChartHandler::makeStatisticsAllFramesGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData)
{
  // define result
  chartSettingsData settings;

  // TODO delete after implement
  settings.mSettingsIsValid = false;

  return settings;
}

chartSettingsData ChartHandler::makeStatisticsAllFramesGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData)
{
  // define result
  chartSettingsData settings;

  // TODO delete after implement
  settings.mSettingsIsValid = false;

  return settings;
}

/*-------------------- public slots --------------------*/
void ChartHandler::currentSelectedItemsChanged(playlistItem *aItem1, playlistItem *aItem2)
{
  Q_UNUSED(aItem2)

  if (this->mChartWidget->parentWidget())
    // get and set title
    this->mChartWidget->parentWidget()->setWindowTitle(this->getStatisticTitle(aItem1));

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

  // get the selected playListItemStatisticFiles-item
  auto items = this->mPlaylist->getSelectedItems();
  bool anyItemsSelected = items[0] != NULL || items[1] != NULL;

  // check that really something is selected
  if(anyItemsSelected)
  {
    // now we need the combination, try to find it
    itemWidgetCoord coord = this->getItemWidgetCoord(items[0]);

    QWidget* chart; // just a holder, will be set in the following

    // check what form of palylistitem was selected
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
  QLabel* lblStat           = new QLabel(tr("Statistics: "));
  QFormLayout* topLayout    = new QFormLayout;

  //setting name for the combobox, to find it later dynamicly
  cbxTypes->setObjectName("cbxTypes");

  // getting the range
  auto range = aItem->getFrameIndexRange();
  // save the data, that we dont have to load it later again
  aCoord.mData = aItem->getData(range, true);

  //check if map contains items
  if(aCoord.mData->keys().count() > 0)
  {
    //map has items, so add them
    cbxTypes->addItem("Select...");

    foreach (QString type, aCoord.mData->keys())
      cbxTypes->addItem(type); // fill with data
  }
  else
    // no items, add a info
    cbxTypes->addItem("No types");

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
  bool hashOddAmount = listGeneratedWidgets.count() % 2 == 1;

  // adding the components, check how we add them
  if(hashOddAmount)
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
    if(hashOddAmount)
      topLayout->addWidget(widget);
    if((widget->objectName() == this->mObNaCbxChartFrameShow)
       || (widget->objectName() == this->mObNaCbxChartGroupBy)
       || (widget->objectName() == this->mObNaCbxChartNormalize)) // finding the combobox and define the action
      connect(dynamic_cast<QComboBox*> (widget),
              static_cast<void (QComboBox::*)(const QString &)> (&QComboBox::currentIndexChanged),
              this,
              &ChartHandler::onStatisticsChange);
  }

  if(!hashOddAmount)
    for (int i = 0; i < listGeneratedWidgets.count(); i +=2)  // take care, we increment i every time by 2!!
      topLayout->addRow(listGeneratedWidgets.at(i), listGeneratedWidgets.at(i+1));

  basicLayout->addLayout(topLayout);
  basicLayout->addWidget(aCoord.mChart);

  return basicWidget;
}

void ChartHandler::onStatisticsChange(const QString aString)
{
  // get the selected playListItemStatisticFiles-item
  auto items = this->mPlaylist->getSelectedItems();
  bool anyItemsSelected = items[0] != NULL || items[1] != NULL;
  if(anyItemsSelected) // check that really something is selected
  {
    // now we need the combination, try to find it
    itemWidgetCoord coord = this->getItemWidgetCoord(items[0]);

    QWidget* chart;
    if(aString != "Select...") // new type was selected in the combobox
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
        if((objectname == this->mObNaCbxChartFrameShow)
           || (child->objectName() == this->mObNaCbxChartGroupBy)
           || (child->objectName() == this->mObNaCbxChartNormalize)) // check if found child the correct combobox
          (dynamic_cast<QComboBox*>(child))->setEnabled(aString != "Select...");
      }
    }
  }
}
