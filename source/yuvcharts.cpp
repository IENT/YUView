#include "yuvcharts.h"
#include "playlistItem.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSlider>
#include <QtWidgets/QFontComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>

int YUVCharts::getTotalAmountOfPixel(playlistItem* aItem, const chartShow aShow, const indexRange aRange)
{
  // first calculate total amount of pixel
  QSize size = aItem->getSize();
  int totalAmountPixel = size.height() * size.width();

  // the amount of pixel depends on the amount of frames
  if(aShow == csPerFrame)
    return totalAmountPixel;
  else if(aShow == csAllFrames)
    return (totalAmountPixel * aItem->getFrameIdxRange().second);
  else if(aShow == csRange)
    // in calculation you have to add 1, because the amount of viewed frames is always one higher than the calculated
    return (totalAmountPixel * (aRange.second - aRange.first + 1));
  else
    return 1;
}

bool YUVCharts::is2DData(QList<collectedData>* aSortedData)
{
  bool is2DData = aSortedData->count() > 0;

  for (int i = 0; i < aSortedData->count(); i++)
  {
    collectedData data = aSortedData->at(i);
    if(data.is3DData())
    {
      is2DData = false;
      break;
    }
  }

  return is2DData;
}

bool YUVCharts::is3DData(QList<collectedData>* aSortedData)
{
  bool is3DData = aSortedData->count() > 0;

  for (int i = 0; i < aSortedData->count(); i++)
  {
    collectedData data = aSortedData->at(i);
    if(! data.is3DData())
    {
      is3DData = false;
      break;
    }
  }

  return is3DData;
}

YUVChartFactory::YUVChartFactory(QWidget *aNoDataToShowWidget, QWidget *aDataIsLoadingWidget) :
  YUVCharts(aNoDataToShowWidget, aDataIsLoadingWidget),
  mBarChart(this->mNoDataToShowWidget, this->mDataIsLoadingWidget),
  mBarChart3D(this->mNoDataToShowWidget, this->mDataIsLoadingWidget),
  mSurfaceChart3D(this->mNoDataToShowWidget, this->mDataIsLoadingWidget)
{

}

QWidget* YUVChartFactory::createChart(const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData)
{
  // first get the settings
  chartSettingsData settings = this->createSettings(aOrderBy, aItem, aRange, aType, aSortedData);

  // create the chart depends on the settings an return it
  return this->createChart(settings);
}

QWidget* YUVChartFactory::createChart(chartSettingsData aSettings)
{
  // first we have to check wether we have 2D or 3D data. after we know it, we can decide what to do
  if(aSettings.m3DData.empty()) // we have 2D-data
  {
    switch (this->m2DType) // check which chart type we have to draw
    {
      case ct2DBarChart:
        return this->mBarChart.createChart(aSettings);
      default:
        return  this->mNoDataToShowWidget; // as default
    }
  }
  else if(! aSettings.m3DData.empty()) // we have 3D-Data
  {
    switch (this->m3DType)
    {
      case ct3DBarChart:
        return this->mBarChart3D.createChart(aSettings);
      case ct3DSurfaceChart:
        return this->mSurfaceChart3D.createChart(aSettings);
      default:
        return  this->mNoDataToShowWidget; // as default
    }
  }

  // in case something gone wrong
  return this->mNoDataToShowWidget;
}

chartSettingsData YUVChartFactory::createSettings(const chartOrderBy aOrderBy, playlistItem *aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData)
{
  Q_UNUSED(aSortedData)

  // first we define a default result
  chartSettingsData defaultResult;
  defaultResult.mSettingsIsValid = false;

  // getting the data
  QList<collectedData>* sortedData = aItem->sortAndCategorizeDataByRange(aType, aRange);

  // our bar chart can not display 3D, so we check if in one collectedData has 3D-Data
  bool is2DData = this->is2DData(sortedData);
  if(is2DData)
  {
    switch (this->m2DType)
    {
      case ct2DBarChart:
        return this->mBarChart.createSettings(aOrderBy, aItem, aRange, aType, sortedData);
      default:
        return defaultResult;
    }
  }

  // check that we have 3D-data
  bool is3DData = this->is3DData(sortedData);
  if(is3DData)
  {
    //check if we have to take care of the 3D-data-range
    if(this->mUse3DCoordinationLimits)
    {
      this->mBarChart3D.set3DCoordinationRange(this->mMinX, this->mMaxX, this->mMinY, this->mMaxY);
      this->mSurfaceChart3D.set3DCoordinationRange(this->mMinX, this->mMaxX, this->mMinY, this->mMaxY);
    }

    switch (this->m3DType)
    {
      case ct3DBarChart:
        return this->mBarChart3D.createSettings(aOrderBy, aItem, aRange, aType, sortedData);
      case ct3DSurfaceChart:
        return this->mSurfaceChart3D.createSettings(aOrderBy, aItem, aRange, aType, sortedData);
      default:
        return defaultResult;
    }
  }

  return defaultResult;
}

void YUVChartFactory::set3DCoordinationRange(const int aMinX, const int aMaxX, const int aMinY, const int aMaxY)
{
  // mark that we have to use the 3D limits
  this->mUse3DCoordinationLimits = true;
  // set the limits
  this->mMinX = aMinX;
  this->mMaxX = aMaxX;
  this->mMinY = aMinY;
  this->mMaxY = aMaxY;
}

void YUVChartFactory::set3DCoordinationtoDefault()
{
  // using the 3d limit should be false, so we have the default
  this->mUse3DCoordinationLimits = false;
  // set the limits to the min / max of int
  this->mMinX = INT_MIN;
  this->mMaxX = INT_MAX;
  this->mMinY = INT_MIN;
  this->mMaxY = INT_MAX;
}

QWidget* YUVBarChart::createChart(const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData)
{
  // first we generate the settings
  chartSettingsData settings = this->createSettings(aOrderBy, aItem, aRange, aType, aSortedData);

  // next we generate the chart and give it back
  return this->createChart(settings);
}

QWidget* YUVBarChart::createChart(chartSettingsData aSettings)
{
  // check if settings are not valid, so we display a default-widget
  if(!aSettings.mSettingsIsValid)
    return this->mNoDataToShowWidget;

  // creating the result
  QChart* chart = new QChart();

  // appending the series to the chart
  chart->addSeries(aSettings.mSeries);
  // setting an animationoption (not necessary but it's nice to see)
  chart->setAnimationOptions(QChart::SeriesAnimations);
  // creating default-axes: always have to be called before you add some custom axes
  chart->createDefaultAxes();
  // accept to hover with the mouse
  chart->setAcceptHoverEvents(true);

  // set hover elements
  if(dynamic_cast<QAbstractBarSeries*>(aSettings.mSeries))
  {
    connect(dynamic_cast<QAbstractBarSeries*>(aSettings.mSeries), &QBarSeries::hovered, chart, [&](bool aStatus, int aIndex, QBarSet* aBarSet){
      if(aStatus)
      {
        qreal value = aBarSet->at(aIndex);
        QToolTip::showText(QCursor::pos(), "Amount is: " + QString::number(value));
      }

    });

  }

  // we check if we have to create custom axes,
  // but first we implement an default

  // if we have set any categories, we can add a custom x-axis
  if(aSettings.mCategories.count() > 0)
  {
    QBarCategoryAxis* axis = new QBarCategoryAxis();
    axis->setCategories(aSettings.mCategories);
    chart->setAxisX(axis, aSettings.mSeries);
  }
  else
  {
    // check that we can cast to the specifi axis-type
    if (dynamic_cast<QBarCategoryAxis*> (chart->axisX()))
    {
      // get the specific axis-type
      QBarCategoryAxis* xaxis = dynamic_cast<QBarCategoryAxis*> (chart->axisX());
      QStringList categorieslist = xaxis->categories(); // get the categories

      // check how much elements we have
      // the axis was created by the chart itself, so it has a default-value
      if(categorieslist.count() == 1)
      {
        QBarCategoryAxis* axis = new QBarCategoryAxis();
        axis->setLabelsVisible(false);
        chart->setAxisX(axis, aSettings.mSeries);
      }
    }
  }

  // in this case we check that we have to set custom axes
  if(aSettings.mSetCustomAxes)
  {
    switch (aSettings.mStatDataType)
    {
      case sdtStructStatisticsItem_Value:
        // first no custom axes
        break;
      case sdtStructStatisticsItem_Vector:
        // first no custom axes
        break;
      case sdtRGB:
        break;
      default:
        // was set before
        break;
    }
  }

  // setting Options for the chart-legend
  chart->legend()->setVisible(aSettings.mShowLegend);
  chart->legend()->setAlignment(Qt::AlignBottom);

  // creating result chartview and set the data
  QChartView *chartView = new QChartView(chart);
  chartView->setRenderHint(QPainter::Antialiasing);

  // final return the created chart
  return chartView;
}

chartSettingsData YUVBarChart::createSettings(const chartOrderBy aOrderBy, playlistItem *aItem, const indexRange aRange, const QString aType, QList<collectedData> *aSortedData)
{
  // just a holder
  QList<collectedData>* sortedData = NULL;

  // check the datasource
  if(aSortedData)
    sortedData = aSortedData;
  else
    sortedData = aItem->sortAndCategorizeDataByRange(aType, aRange);

  chartSettingsData result;
  result.mSettingsIsValid = false;

  // can we display it?
  if(this->is2DData(sortedData))
    return this->makeStatistic(sortedData, aOrderBy, aItem, aRange);
  else // at this point we have 3D Data and we can not display it with YUVBarChart
    return result;
}

chartSettingsData YUVBarChart::makeStatistic(QList<collectedData>* aSortedData, const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange)
{
  // define the result settings
  chartSettingsData settings;
  settings.mSettingsIsValid = false;

  // if we have no keys, we cant show any data so return at this point
  if(!aSortedData->count())
    return settings;

  // check what we have to do
  switch (aOrderBy)
  {
    case cobPerFrameGrpByValueNrmNone:
      settings = this->makeStatisticsPerFrameGrpByValNrmNone(aSortedData);
      break;
    case cobPerFrameGrpByValueNrmByArea:
      settings = this->makeStatisticsPerFrameGrpByValNrmArea(aSortedData, aItem);
      break;
    case cobPerFrameGrpByBlocksizeNrmNone:
      settings = this->makeStatisticsPerFrameGrpByBlocksizeNrmNone(aSortedData);
      break;
    case cobPerFrameGrpByBlocksizeNrmByArea:
      settings = this->makeStatisticsPerFrameGrpByBlocksizeNrmArea(aSortedData);
      break;

    case cobRangeGrpByValueNrmNone:
      settings = this->makeStatisticsFrameRangeGrpByValNrmNone(aSortedData);
      break;
    case cobRangeGrpByValueNrmByArea:
      settings = this->makeStatisticsFrameRangeGrpByValNrmArea(aSortedData, aItem, aRange);
      break;
    case cobRangeGrpByBlocksizeNrmNone:
      settings = this->makeStatisticsFrameRangeGrpByBlocksizeNrmNone(aSortedData);
      break;
    case cobRangeGrpByBlocksizeNrmByArea:
      settings = this->makeStatisticsFrameRangeGrpByBlocksizeNrmArea(aSortedData);
      break;

    case cobAllFramesGrpByValueNrmNone:
      settings = this->makeStatisticsAllFramesGrpByValNrmNone(aSortedData);
      break;
    case cobAllFramesGrpByValueNrmByArea:
      settings = this->makeStatisticsAllFramesGrpByValNrmArea(aSortedData, aItem);
      break;
    case cobAllFramesGrpByBlocksizeNrmNone:
      settings = this->makeStatisticsAllFramesGrpByBlocksizeNrmNone(aSortedData);
      break;
    case cobAllFramesGrpByBlocksizeNrmByArea:
      settings = this->makeStatisticsAllFramesGrpByBlocksizeNrmArea(aSortedData);
      break;

    default:
      return settings;
  }

  // at least we have to return our settings
  return settings;
}

chartSettingsData YUVBarChart::makeStatisticsPerFrameGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData)
{
  QBarSeries* series = new QBarSeries();

  // define result
  chartSettingsData settings;
  settings.mSeries = series;

  statisticsDataType dataType = sdtUnknown;
  statisticsDataType lastDataType = sdtUnknown;

  // just a holder
  QBarSet *set;
  // auxililary var, to coordinate categories and sets
  QHash<QString, QBarSet*> coordCategorieSet;

  // running thru the sorted Data
  for(int i = 0; i < aSortedData->count(); i++)
  {
    // first getting the data
    collectedData data = aSortedData->at(i);
    if(data.mStatDataType == sdtStructStatisticsItem_Value)
    {
      settings.mShowLegend = true;

      // ceate an auxiliary var's
      bool moreThanOneElement = data.mValues.count() > 1;

      // creating the set
      set = new QBarSet(data.mLabel);
      // if we have more than one value
      foreach (auto chartData, data.mValues)
      {
        QVariant variant = chartData->first;
        int value = variant.toInt(); // because auf sdtStructStatisticsItem_Value we know that the variant is an int
        int amount = chartData->second;

        // convert the number to a String, we can use it as key or something other
        QString valueString = QString::number(value);

        if(moreThanOneElement)
        {
          // getting the set, if possible otherwise create a new set and add to the coordCategorieSet
          if(!coordCategorieSet.contains(valueString))
          {
            set = new QBarSet(valueString);
            coordCategorieSet.insert(valueString, set);
          }
          else
            set = coordCategorieSet.value(valueString);

          // appending data to the set
          *set << amount;
        }
        else
        {
          // appending data to the set
          *set << amount;
        }
      }

      // check if we just had one data for each label
      if(! moreThanOneElement)
      {
        set->setLabel(data.mLabel); // setting new label
        series->append(set);      // appending the set to the series
      }
      else
      {
        // append all sets to the series
        foreach (QString key, coordCategorieSet.keys())
          series->append(coordCategorieSet.value(key));
      }

      if(moreThanOneElement)
        //at least appending the label to the categories for the axes if necessary
         settings.mCategories << data.mLabel;

      // check the type
      if((data.mStatDataType != dataType) && (dataType == lastDataType))
        dataType = data.mStatDataType;

      lastDataType  = data.mStatDataType;
    }

    // set the datatype
    settings.mStatDataType = dataType;
  }

  return settings;
}

chartSettingsData YUVBarChart::makeStatisticsPerFrameGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData)
{
  return this->calculateAndDefineGrpByBlocksizeNrmArea(aSortedData);
}

chartSettingsData YUVBarChart::makeStatisticsPerFrameGrpByValNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem)
{
  // first calculate total amount of pixel
  // the range doesn matter, we look at one frame
  int totalAmountPixel = this->getTotalAmountOfPixel(aItem, csPerFrame, indexRange(0, 0));

  return this->calculateAndDefineGrpByValueNrmArea(aSortedData, totalAmountPixel);
}

chartSettingsData YUVBarChart::makeStatisticsPerFrameGrpByValNrmNone(QList<collectedData>* aSortedData)
{
  QBarSeries* series = new QBarSeries();
  // define result
  chartSettingsData settings;
  settings.mSeries = series;

  statisticsDataType dataType = sdtUnknown;
  statisticsDataType lastDataType = sdtUnknown;

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
    if(data.mStatDataType == sdtStructStatisticsItem_Value)
    {
      settings.mShowLegend = true;

      // if we have more than one value
      foreach (auto chartData, data.mValues)
      {
        QVariant variant = chartData->first;
        int value = variant.toInt(); // because auf sdtStructStatisticsItem_Value we know that the variant is an int
        int amount = chartData->second;

        int* count = NULL; // at this point we need an holder for an int, but if we dont set to NULL, the system requires a pointer
        // check if we have insert the count yet
        if(hashValueCount.value(value))
          count = hashValueCount.value(value); // was inserted
        else
        {
          // at this point we have to get a new int by the system and we save the adress of this int in count
          // so we have later for each int a new adress! and an new int, which we can save
          count = new int(0);
          // inserting the adress to get it later back
          hashValueCount.insert(value, count);
        }

        // at least we need to sum up the data, remember, that we have to dereference count, to change the value!
        *count += amount;
      }
    }

    // check the type
    if((data.mStatDataType != dataType) && (dataType == lastDataType))
      dataType = data.mStatDataType;

    lastDataType  = data.mStatDataType;
  }

  // set the datatype
  settings.mStatDataType = dataType;

  // at this point we order the keys new from low to high
  // we cant use QHash at this point, because the items are arbitrarily ordered in QHash, so we have to use QMap at this point
  QMap<int, int*> mapValueCountSorted;
  int smallestKey = INT_MAX;
  int maxElementsNeeded = hashValueCount.keys().count();

  while (mapValueCountSorted.keys().count() < maxElementsNeeded)
  {
    foreach (int key, hashValueCount.keys())
    {
      if(key < smallestKey)
        smallestKey = key;
    }

    mapValueCountSorted.insert(smallestKey, hashValueCount.value(smallestKey));
    hashValueCount.remove(smallestKey);
    smallestKey = INT_MAX;
  }

  // because of the QHash we know how many QBarSet we have to create and add to the series in settings-struct
  foreach (int key, mapValueCountSorted.keys())
  {
    // creating the set with an label from the given Value
    QBarSet* set = new QBarSet(QString::number(key));
    //settings.mCategories << QString::number(key);
    // adding the count to the set, which we want to display
    int *count = mapValueCountSorted.value(key); // getting the adress of the total
    *set << *count; // remenber to dereference the count to get the real value of count
    // at least add the set to the series in the settings-struct
    series->append(set);
  }

  return settings;
}

chartSettingsData YUVBarChart::makeStatisticsFrameRangeGrpByValNrmNone(QList<collectedData>* aSortedData)
{
  // does the same as perFrame, just the amount of data considered is different
  return this->makeStatisticsPerFrameGrpByValNrmNone(aSortedData);
}

chartSettingsData YUVBarChart::makeStatisticsFrameRangeGrpByValNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem, const indexRange aRange)
{
  // first calculate total amount of pixel
  int totalAmountPixel = this->getTotalAmountOfPixel(aItem, csRange, aRange);

  return this->calculateAndDefineGrpByValueNrmArea(aSortedData, totalAmountPixel);
}

chartSettingsData YUVBarChart::makeStatisticsFrameRangeGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData)
{
  // does the same as perFrame, just the amount of data considered is different
  return this->makeStatisticsPerFrameGrpByBlocksizeNrmNone(aSortedData);
}

chartSettingsData YUVBarChart::makeStatisticsFrameRangeGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData)
{
  return this->calculateAndDefineGrpByBlocksizeNrmArea(aSortedData);
}

chartSettingsData YUVBarChart::makeStatisticsAllFramesGrpByValNrmNone(QList<collectedData>* aSortedData)
{
  // does the same as perFrame, just the amount of data considered is different
  return this->makeStatisticsPerFrameGrpByValNrmNone(aSortedData);
}

chartSettingsData YUVBarChart::makeStatisticsAllFramesGrpByValNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem)
{
  // first calculate total amount of pixel
  // the range doesn matter, we look at all frames
  int totalAmountPixel = this->getTotalAmountOfPixel(aItem, csAllFrames, indexRange(0, 0));

  return this->calculateAndDefineGrpByValueNrmArea(aSortedData, totalAmountPixel);
}

chartSettingsData YUVBarChart::makeStatisticsAllFramesGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData)
{
  // does the same as perFrame, just the amount of data considered is different
  return this->makeStatisticsPerFrameGrpByBlocksizeNrmNone(aSortedData);
}

chartSettingsData YUVBarChart::makeStatisticsAllFramesGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData)
{
  return this->calculateAndDefineGrpByBlocksizeNrmArea(aSortedData);
}

chartSettingsData YUVBarChart::calculateAndDefineGrpByValueNrmArea(QList<collectedData>* aSortedData, const int aTotalAmountPixel)
{
  QBarSeries* series = new QBarSeries();
  // define result
  chartSettingsData settings;
  settings.mSeries = series;

  statisticsDataType dataType = sdtUnknown;
  statisticsDataType lastDataType = sdtUnknown;

  // we order by the value, so we want to find out how many times the value was count in this frame
  QHash<int, int*> hashValueCount;

  // we save in the QHash the value first as key and later we use it as label, and we save the total of counts to the value
  // we save the total as pointer, so we have the advantage, that we dont need to replace the last added count
  // but we have to observe that this is not so easy it might be
  // always remember if you want to change the value of an primitive datatype which you saved as pointer
  // you have to dereference the pointer and then you can change it!
  for (int i = 0; i < aSortedData->count(); i++)
  {
    // first getting the data
    collectedData data = aSortedData->at(i);
    if(data.mStatDataType == sdtStructStatisticsItem_Value)
    {
      settings.mShowLegend = true;

      // get the width and the heigth
      QStringList numberStrings = data.mLabel.split("x");
      QString widthStr  = numberStrings.at(0);
      QString heightStr = numberStrings.at(1);
      int width = widthStr.toInt();
      int height = heightStr.toInt();

      // if we have more than one value
      foreach (auto chartData, data.mValues)
      {
        QVariant variant = chartData->first;
        int value = variant.toInt(); // because auf sdtStructStatisticsItem_Value we know that the variant is an int
        int amount = chartData->second;
        int* count = NULL; // at this point we need an holder for an int, but if we dont set to NULL, the system requires a pointer

        // check if we have insert the count yet
        if(hashValueCount.value(value))
          count = hashValueCount.value(value); // was inserted
        else
        {
          // at this point we have to get a new int by the system and we save the adress of this int in count
          // so we have later for each int a new adress! and an new int, which we can save
          count = new int(0);
          // inserting the adress to get it later back
          hashValueCount.insert(value, count);
        }

        // at least we need to sum up the data, remember, that we have to dereference count, to change the value!
        *count += ((width * height) * amount);
      }
      // check the type
      if((data.mStatDataType != dataType) && (dataType == lastDataType))
        dataType = data.mStatDataType;

      lastDataType  = data.mStatDataType;
    }
    // set the datatype
    settings.mStatDataType = dataType;
  }

  // order the items from low to high
  // we cant use QHash at this point, because the items are arbitrarily ordered in QHash, so we have to use QMap at this point
  QMap<int, int*> mapValueCountSorted;
  int smallestKey = INT_MAX;
  int maxElementsNeeded = hashValueCount.keys().count();

  while (mapValueCountSorted.keys().count() < maxElementsNeeded)
  {
    foreach (int key, hashValueCount.keys())
      if(key < smallestKey)
        smallestKey = key;

    mapValueCountSorted.insert(smallestKey, hashValueCount.value(smallestKey));
    hashValueCount.remove(smallestKey);
    smallestKey = INT_MAX;
  }

  foreach (int key, mapValueCountSorted.keys())
  {
    QBarSet* set = new QBarSet(QString::number(key));
    int amountPixelofValue = *(mapValueCountSorted.value(key));
    // calculate the ratio, (remember that we have to cast one int to an double, to get a double as result)
    double ratio = (amountPixelofValue / (double)aTotalAmountPixel) * 100;

    // cause of maybe other pixelvalues it can happen that we calculate more pixel than we have really
    if(ratio > 100.0)
      ratio = 100.0;

    *set << ratio;
    series->append(set);
  }

  return settings;
}

chartSettingsData YUVBarChart::calculateAndDefineGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData)
{
  QBarSeries* series = new QBarSeries();
  // define result
  chartSettingsData settings;
  settings.mSeries = series;

  statisticsDataType dataType = sdtUnknown;
  statisticsDataType lastDataType = sdtUnknown;

  // just a holder
  QBarSet* set;
  QHash<int, QBarSet*> coordCategorieSet; // saving the set, to add them later

  int totalAmountBlocks = 0;

  // calculate the total amount of blocks, it doesn´t matter which size the block has
  for (int i = 0; i < aSortedData->count(); ++i)
  {
    collectedData data = aSortedData->at(i);
    // check that we can add them
    if(data.mStatDataType == sdtStructStatisticsItem_Value) // if the type is sdtStructStatisticsItem_Value we know that the first element of qpair is an int
    {
      foreach (auto chartData, data.mValues)
      {
        // we sum up the blocks and weight them with the value
        if(chartData->first.toInt() == 0) // if value is zero, the amount of block with the weight are zero too. So we add them with an weight of 1
          totalAmountBlocks += chartData->second;
        else
          totalAmountBlocks += chartData->second * chartData->first.toInt();
      }
    }
  }

  // we have no data, so we can return at this point. Remenber to set the settings to false and return the settings
  if(totalAmountBlocks == 0)
  {
    settings.mSettingsIsValid = false;
    return settings;
  }

  // calculate all ratio
  for (int i = 0; i < aSortedData->count(); i++)
  {
    // first getting the data
    collectedData data = aSortedData->at(i);
    set = new QBarSet(data.mLabel);

    // ceate an auxiliary var's
    bool moreThanOneElement = data.mValues.count() > 1;

    if(data.mStatDataType == sdtStructStatisticsItem_Value)
    {
      settings.mShowLegend = true;

      // if we have more than one value
      foreach (auto chartData, data.mValues)
      {
        int chartValue = chartData->first.toInt();
        int weight = (chartValue == 0) ? 1 : chartValue; // if the amount is zero we have to adjust the weight. We set the weight ot 1
        int amountBlocks = chartData->second;
        // calculate the ratio, (remember that we have to cast one int to an double, to get a double as result)
        double ratio = ((amountBlocks * weight) / (double)totalAmountBlocks) * 100;

        // cause of maybe other pixel values it can happen that we calculate more pixel than we have really
        if(ratio > 100.0)
          ratio = 100.0;

        if(moreThanOneElement)
        {
          // getting the set, if possible otherwise create a new set and add to the coordCategorieSet
          if(!coordCategorieSet.contains(chartValue))
          {
            set = new QBarSet(QString::number(chartValue));
            coordCategorieSet.insert(chartValue, set);
          }
          else
            set = coordCategorieSet.value(chartValue); // get the set

          // appending data to the set
          *set << ratio;
        }
        else
          *set << ratio; // appending data to the set
      }
    }

    // check the type
    if((data.mStatDataType != dataType) && (dataType == lastDataType))
      dataType = data.mStatDataType;

    lastDataType  = data.mStatDataType;

    // check if we have more than one value for the set
    if(moreThanOneElement)
    {
      // append all sets to the series
      foreach (int key, coordCategorieSet.keys())
        series->append(coordCategorieSet.value(key));
    }
    else
    {
      // just had one data for each label
      set->setLabel(data.mLabel); // setting new label
      series->append(set);      // appending the set to the series
    }

    if(moreThanOneElement)
      //at least appending the label to the categories for the axes if necessary
       settings.mCategories << data.mLabel;
  }

  // set the datatype
  settings.mStatDataType = dataType;

  return settings;
}

YUV3DBarChart::YUV3DBarChart(QWidget *aNoDataToShowWidget, QWidget *aDataIsLoadingWidget) :
  YUV3DCharts(aNoDataToShowWidget, aDataIsLoadingWidget)
{
  // create the graph
  Q3DBars* widgetgraph = new Q3DBars();

  // necessary! get an container and check that we can init OpenGL
  QWidget* container = QWidget::createWindowContainer(widgetgraph);

  // very important! no OpenGL --> no 3d graph
  this->mHasOpenGL = widgetgraph->hasContext();

  if(!this->mHasOpenGL) // check for OpenGL
  {
    QMessageBox msgBox;
    msgBox.setText("Couldn't initialize the OpenGL context. Can´t display 3D charts.");
    msgBox.exec();
  }
  else
  {
    // get basic widget
    this->mWidgetGraph = new QWidget;

    // create basic-layout and set to widget
    QVBoxLayout* lyBasic = new QVBoxLayout(this->mWidgetGraph);

    // create gridlayout for the controls
    QGridLayout* lyGridControls = new QGridLayout();

    // add container and control-layout to basiclayout
    lyBasic->addWidget(container, 1);
    lyBasic->addLayout(lyGridControls);

    // create a button to zoom to an specific bar
    QPushButton* zoomToSelectedButton = new QPushButton(this->mWidgetGraph);
    zoomToSelectedButton->setText(QStringLiteral("Zoom to selected bar"));

    // create a slider to rotate around the x-axis
    QSlider *rotationSliderX = new QSlider(Qt::Horizontal, this->mWidgetGraph);
    rotationSliderX->setTickInterval(30);
    rotationSliderX->setTickPosition(QSlider::TicksBelow);
    rotationSliderX->setMinimum(-180);
    rotationSliderX->setValue(0);
    rotationSliderX->setMaximum(180);

    // create a slider to rotate around the y-axis
    QSlider *rotationSliderY = new QSlider(Qt::Horizontal, this->mWidgetGraph);
    rotationSliderY->setTickInterval(15);
    rotationSliderY->setTickPosition(QSlider::TicksBelow);
    rotationSliderY->setMinimum(-90);
    rotationSliderY->setValue(0);
    rotationSliderY->setMaximum(90);

    // create a slider to rotate the labels
    QSlider *axisLabelRotationSlider = new QSlider(Qt::Horizontal, this->mWidgetGraph);
    axisLabelRotationSlider->setTickInterval(10);
    axisLabelRotationSlider->setTickPosition(QSlider::TicksBelow);
    axisLabelRotationSlider->setMinimum(0);
    axisLabelRotationSlider->setValue(30);
    axisLabelRotationSlider->setMaximum(90);

    // create the control-layout
    // all labels in row 0
    lyGridControls->addWidget(new QLabel(QStringLiteral("Rotate horizontally")),0, 0);
    lyGridControls->addWidget(new QLabel(QStringLiteral("Rotate vertically")), 0, 1);
    lyGridControls->addWidget(new QLabel(QStringLiteral("Axis label rotation")), 0, 2);
    lyGridControls->addWidget(new QLabel(QStringLiteral("Zoom zo selected value")), 0, 3);

    // all controls in row 1
    lyGridControls->addWidget(rotationSliderX, 1, 0);
    lyGridControls->addWidget(rotationSliderY, 1, 1);
    lyGridControls->addWidget(axisLabelRotationSlider, 1, 2);
    lyGridControls->addWidget(zoomToSelectedButton, 1, 3);

    // create modifier to handle the sliders and so on
    this->mModifier = new GraphModifier3DBars(widgetgraph);

    // connect modifier to handle the actions
    GraphModifier3DBars* modifier = dynamic_cast<GraphModifier3DBars*> (this->mModifier);

    connect(rotationSliderX, &QSlider::valueChanged, modifier , &GraphModifier3DBars::rotateX);
    connect(rotationSliderY, &QSlider::valueChanged, modifier, &GraphModifier3DBars::rotateY);
    connect(axisLabelRotationSlider, &QSlider::valueChanged, modifier, &GraphModifier3DBars::changeLabelRotation);
    connect(zoomToSelectedButton, &QPushButton::clicked, modifier, &GraphModifier3DBars::zoomToSelectedBar);
  }
}

YUV3DCharts::YUV3DCharts(QWidget* aNoDataToShowWidget, QWidget* aDataIsLoadingWidget) :
  YUVCharts(aNoDataToShowWidget, aDataIsLoadingWidget)
{
  this->set3DCoordinationtoDefault();
}

QWidget* YUV3DCharts::createChart(const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData)
{
  chartSettingsData settings = this->createSettings(aOrderBy, aItem, aRange, aType, aSortedData);

  if(settings.mIs3DData)
    return this->createChart(settings);
  else
  {
    //set as default that we display barChart
    YUVBarChart barChart(this->mNoDataToShowWidget, this->mDataIsLoadingWidget);
    return barChart.createChart(settings);
  }
}

QWidget* YUV3DCharts::createChart(chartSettingsData aSettings)
{
  // settings are not valid? so we display a default widget
  if(!aSettings.mSettingsIsValid)
    return this->mNoDataToShowWidget;

  // change data in the 3D-graph
  this->mModifier->applyDataToGraph(aSettings);

  return this->mWidgetGraph;
}

chartSettingsData YUV3DCharts::createSettings(const chartOrderBy aOrderBy, playlistItem *aItem, const indexRange aRange, const QString aType, QList<collectedData> *aSortedData)
{
  // define a basic result
  chartSettingsData result;
  result.mSettingsIsValid = false;

  // check that we have init OpenGl
  if(!this->hasOpenGL()) // no OpenGL -- no 3D -Graph
    return result;

  // just a holder
  QList<collectedData>* sortedData = NULL;

  // check the datasource
  if(aSortedData)
    sortedData = aSortedData;
  else
    sortedData = aItem->sortAndCategorizeDataByRange(aType, aRange); // no source given, get the data

  // can we display it?
  if(this->is3DData(sortedData))
    return  this->makeStatistic(sortedData, aOrderBy, aItem, aRange);
  else // at this point we have 2D Data and we can not display it with YUV3DBarChart
    return result;
}

void YUV3DCharts::set3DCoordinationRange(const int aMinX, const int aMaxX, const int aMinY, const int aMaxY)
{
  // mark that we use the 3d limits
  this->mUse3DCoordinationLimits = true;

  // set the new limits
  this->mMinX = aMinX;
  this->mMaxX = aMaxX;
  this->mMinY = aMinY;
  this->mMaxY = aMaxY;
}

void YUV3DCharts::set3DCoordinationtoDefault()
{
  // mark taht we dont use the limits
  this->mUse3DCoordinationLimits = false;

  // set the new limits
  this->mMinX = INT_MIN;
  this->mMaxX = INT_MAX;
  this->mMinY = INT_MIN;
  this->mMaxY = INT_MAX;
}

chartSettingsData YUV3DCharts::makeStatistic(QList<collectedData>* aSortedData, const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange)
{
  Q_UNUSED(aItem)
  Q_UNUSED(aRange)

  // define the result settings
  chartSettingsData settings;
  settings.mSettingsIsValid = false;

  // if we have no keys, we cant show any data so return at this point
  if(!aSortedData->count())
    return settings;

  switch (aOrderBy)
  {
    case cobPerFrameGrpByValueNrmNone:
      settings = this->makeStatisticsPerFrameGrpByValNrmNone(aSortedData);
      break;
    case cobPerFrameGrpByValueNrmByArea:
      settings = this->makeStatisticsPerFrameGrpByValNrm(aSortedData);
      break;
    case cobPerFrameGrpByBlocksizeNrmNone:
      settings = this->makeStatisticsPerFrameGrpByBlocksizeNrmNone(aSortedData);    // take care, we set the settings for a 2D-graph
      break;
    case cobPerFrameGrpByBlocksizeNrmByArea:
      settings =  this->makeStatisticsPerFrameGrpByBlocksizeNrm(aSortedData);       // take care, we set the settings for a 2D-graph
      break;

    case cobRangeGrpByValueNrmNone:
      settings = this->makeStatisticsFrameRangeGrpByValNrmNone(aSortedData);
      break;
    case cobRangeGrpByValueNrmByArea:
      settings = this->makeStatisticsFrameRangeGrpByValNrm(aSortedData);
      break;
    case cobRangeGrpByBlocksizeNrmNone:
      settings = this->makeStatisticsFrameRangeGrpByBlocksizeNrmNone(aSortedData);  // take care, we set the settings for a 2D-graph
      break;
    case cobRangeGrpByBlocksizeNrmByArea:
      settings = this->makeStatisticsFrameRangeGrpByBlocksizeNrm(aSortedData);      // take care, we set the settings for a 2D-graph
      break;

    case cobAllFramesGrpByValueNrmNone:
      settings = this->makeStatisticsAllFramesGrpByValNrmNone(aSortedData);
      break;
    case cobAllFramesGrpByValueNrmByArea:
      settings = this->makeStatisticsAllFramesGrpByValNrm(aSortedData);
      break;
    case cobAllFramesGrpByBlocksizeNrmNone:
      settings = this->makeStatisticsAllFramesGrpByBlocksizeNrmNone(aSortedData);   // take care, we set the settings for a 2D-graph
      break;
    case cobAllFramesGrpByBlocksizeNrmByArea:
      settings = this->makeStatisticsAllFramesGrpByBlocksizeNrm(aSortedData);       // take care, we set the settings for a 2D-graph
      break;

    default:
      return settings;
  }

  return settings;
}

chartSettingsData YUV3DCharts::makeStatisticsPerFrameGrpByValNrmNone(QList<collectedData>* aSortedData)
{
  chartSettingsData settings;

  statisticsDataType dataType = sdtUnknown;
  statisticsDataType lastDataType = sdtUnknown;

  // the result-value is a 2D-Array
  // we realize the array with an 2 dimensiol qmap, so we don´t have to search for the maximum x-value and y-value
  // in case of this, we can reduce the amount of loops
  // each x and y index we use is already defined with 0 (int)
  QMap<int, QMap<int, double>> resultValueCount;

  // in this case we programm two identic loops and we check the 3D-coordinates before.
  // the advantage is, that we dont need to check every loop pass
  // we look at a specific range
  if(this->mUse3DCoordinationLimits)
  {
    for (int i = 0; i < aSortedData->count(); i++)
    {
      collectedData data = aSortedData->at(i);

      if(data.mStatDataType == sdtStructStatisticsItem_Vector)
      {
        foreach (auto valuepair, data.mValues)
        {
          // getting the values
          QVariant variant = valuepair->first;
          int amount = valuepair->second;

          // in case of sdtStructStatisticsItem_Vector we know that we can cast the QVariant to an QPoint
          QPoint point = variant.toPoint();

          // save the coordinates in vars, better to unterstand
          int x = point.x();
          int y = point.y();

          // check that our values is in the limit-range
          if((x >= this->mMinX && x <= this->mMaxX) && (y >= this->mMinY && y <= this->mMaxY))
            // getting the coordinates from the point and use them as index for our 2D-Map
            resultValueCount[x][y] += amount;
        }
      }
      // check the type
      if((data.mStatDataType != dataType) && (dataType == lastDataType))
        dataType = data.mStatDataType;

      lastDataType  = data.mStatDataType;
    }
  }
  // we look at all items we have
  else
  {
    for (int i = 0; i < aSortedData->count(); i++)
    {
      collectedData data = aSortedData->at(i);

      if(data.mStatDataType == sdtStructStatisticsItem_Vector)
      {
        foreach (auto valuepair, data.mValues)
        {
          // getting the values
          QVariant variant = valuepair->first;
          int amount = valuepair->second;

          // in case of sdtStructStatisticsItem_Vector we know that we can cast the QVariant to an QPoint
          QPoint point = variant.toPoint();

          // getting the coordinates from the point and use them as index for our 2D-Map
          resultValueCount[point.x()][point.y()] += amount;
        }
      }
      // check the type
      if((data.mStatDataType != dataType) && (dataType == lastDataType))
        dataType = data.mStatDataType;

      lastDataType  = data.mStatDataType;
    }
  }

  settings.m3DData        = resultValueCount;
  settings.mIs3DData      = true;
  settings.mStatDataType  = dataType;

  settings.define3DRanges(mMinX, mMaxX, mMinY, mMaxY);

  return settings;
}

chartSettingsData YUV3DCharts::makeStatisticsFrameRangeGrpByValNrmNone(QList<collectedData>* aSortedData)
{
  // does the same as makeStatisticsPerFrameGrpByValNrmNone just the amount of sortedData is other
  return this->makeStatisticsPerFrameGrpByValNrmNone(aSortedData);
}

chartSettingsData YUV3DCharts::makeStatisticsAllFramesGrpByValNrmNone(QList<collectedData>* aSortedData)
{
  // does the same as makeStatisticsPerFrameGrpByValNrmNone just the amount of sortedData is other
  return this->makeStatisticsPerFrameGrpByValNrmNone(aSortedData);
}

chartSettingsData YUV3DCharts::makeStatisticsPerFrameGrpByValNrm(QList<collectedData> *aSortedData)
{
  chartSettingsData settings;
  statisticsDataType dataType = sdtUnknown;
  statisticsDataType lastDataType = sdtUnknown;

  int maxAmountVector = 0;

  // the result-value is a 2D-Array
  // we realize the array with an 2 dimensiol qmap, so we don´t have to search for the maximum x-value and y-value
  // in case of this, we can reduce the amount of loops
  // each x and y index we use is already defined with 0 (int)
  QMap<int, QMap<int, double>> resultValueCount;

  // in this case we programm two identic loops and we check the 3D-coordinates before.
  // the advantage is, that we dont need to check every loop pass
  // we look at a specific range
  if(this->mUse3DCoordinationLimits)
  {
    for (int i = 0; i < aSortedData->count(); i++)
    {
      collectedData data = aSortedData->at(i);

      if(data.mStatDataType == sdtStructStatisticsItem_Vector)
      {
        foreach (auto valuepair, data.mValues)
        {
          // getting the values
          QVariant variant = valuepair->first;
          int amount = valuepair->second;

          // in case of sdtStructStatisticsItem_Vector we know that we can cast the QVariant to an QPoint
          QPoint point = variant.toPoint();

          // save the coordinates in vars, better to unterstand
          int x = point.x();
          int y = point.y();

          // check that our value is in the limit-range
          if((x >= this->mMinX && x <= this->mMaxX) && (y >= this->mMinY && y <= this->mMaxY))
          {
            // getting the coordinates from the point and use them as index for our 2D-Map
            resultValueCount[x][y] += amount;
            maxAmountVector += amount;
          }
        }
      }

      // check the type
      if((data.mStatDataType != dataType) && (dataType == lastDataType))
        dataType = data.mStatDataType;

      lastDataType  = data.mStatDataType;
    }
  }
  // we look a all items we have
  else
  {
    for (int i = 0; i < aSortedData->count(); i++)
    {
      collectedData data = aSortedData->at(i);

      if(data.mStatDataType == sdtStructStatisticsItem_Vector)
      {
        foreach (auto valuepair, data.mValues)
        {
          // getting the values
          QVariant variant = valuepair->first;
          int amount = valuepair->second;

          // in case of sdtStructStatisticsItem_Vector we know that we can cast the QVariant to an QPoint
          QPoint point = variant.toPoint();

          // getting the coordinates from the point and use them as index for our 2D-Map
          resultValueCount[point.x()][point.y()] += amount;
          maxAmountVector += amount;
        }
      }
      // check the type
      if((data.mStatDataType != dataType) && (dataType == lastDataType))
        dataType = data.mStatDataType;

      lastDataType  = data.mStatDataType;
    }
  }

  // calculate the ratio of the items to normalize
  QMap<int, QMap<int, double>> resultValue;
  foreach (int x, resultValueCount.keys())
  {
    QMap<int, double> row = resultValueCount.value(x);
    foreach (int y, row.keys())
      resultValue[x][y] = (resultValueCount[x][y] / maxAmountVector) * 100.0;
  }

  settings.m3DData        = resultValue;
  settings.mIs3DData      = true;
  settings.mStatDataType  = dataType;
  settings.define3DRanges(mMinX, mMaxX, mMinY, mMaxY);

  return settings;
}

chartSettingsData YUV3DCharts::makeStatisticsFrameRangeGrpByValNrm(QList<collectedData> *aSortedData)
{
  // does the same as makeStatisticsPerFrameGrpByValNrm, data amount is other
  return this->makeStatisticsPerFrameGrpByValNrm(aSortedData);
}

chartSettingsData YUV3DCharts::makeStatisticsAllFramesGrpByValNrm(QList<collectedData> *aSortedData)
{
  // does the same as makeStatisticsPerFrameGrpByValNrm, data amount is other
  return this->makeStatisticsPerFrameGrpByValNrm(aSortedData);
}

chartSettingsData YUV3DCharts::makeStatisticsPerFrameGrpByBlocksizeNrmNone(QList<collectedData> *aSortedData)
{
  chartSettingsData settings;

  statisticsDataType dataType = sdtUnknown;
  statisticsDataType lastDataType = sdtUnknown;

  QBarSeries* series = new QBarSeries();
  // just a holder
  QBarSet *set;

  // go thru all elements
  for (int i = 0; i < aSortedData->count(); i++)
  {
    // get the element at pos i
    collectedData data = aSortedData->at(i);

    if(data.mStatDataType == sdtStructStatisticsItem_Vector) // check that we can use the data
    {
      QString blocksize = data.mLabel;

      // creating the set with the blocksize label
      set = new QBarSet(blocksize);
      int totalAmount = 0;
      foreach (auto valuepair, data.mValues)
        // getting the values, we just need the amount, the vector doesnt matter
        totalAmount += valuepair->second;

      // append the data
      *set << totalAmount;
      series->append(set);
    }


    // check the type
    if((data.mStatDataType != dataType) && (dataType == lastDataType))
      dataType = data.mStatDataType;

    lastDataType  = data.mStatDataType;
  }

  settings.mSetCustomAxes = false;
  settings.mIs3DData      = false;
  settings.mShowLegend    = true;
  settings.mSeries        = series;

  return settings;
}

chartSettingsData YUV3DCharts::makeStatisticsFrameRangeGrpByBlocksizeNrmNone(QList<collectedData> *aSortedData)
{
  // the amount of data is not the same, but the code is the same
  return this->makeStatisticsPerFrameGrpByBlocksizeNrmNone(aSortedData);
}

chartSettingsData YUV3DCharts::makeStatisticsAllFramesGrpByBlocksizeNrmNone(QList<collectedData> *aSortedData)
{
  // the amount of data is not the same, but the code is the same
  return this->makeStatisticsPerFrameGrpByBlocksizeNrmNone(aSortedData);
}

chartSettingsData YUV3DCharts::makeStatisticsPerFrameGrpByBlocksizeNrm(QList<collectedData> *aSortedData)
{
  chartSettingsData settings;

  statisticsDataType dataType = sdtUnknown;
  statisticsDataType lastDataType = sdtUnknown;

  QBarSeries* series = new QBarSeries();

  // just a holder
  QBarSet *set;

  // calculating the total amount of all vector
  int totalAmount = 0;

  for (int i = 0; i < aSortedData->count(); i++)
  {
    collectedData data = aSortedData->at(i);

    if(data.mStatDataType == sdtStructStatisticsItem_Vector)
      foreach (auto valuepair, data.mValues)
        totalAmount += valuepair->second;
  }

  // normalize
  // go thru all elements
  for (int i = 0; i < aSortedData->count(); i++)
  {
    // get the element at pos i
    collectedData data = aSortedData->at(i);

    if(data.mStatDataType == sdtStructStatisticsItem_Vector) // check that we can handle it
    {
      QString blocksize = data.mLabel;

      // creating the set with the blocksize as label
      set = new QBarSet(blocksize);

      // getting the amount of one vecotr in one blocksize
      int amount = 0;
      foreach (auto valuepair, data.mValues)
        // getting the values, we just need the amount, the vektor doesnt matter
        amount += valuepair->second;

      // calc the ratio
      double ratio = (amount /(double) totalAmount) * 100.0;

      // check if ratio is higher than 100%
      if(ratio > 100.0)
        ratio = 100.0;

      // append data
      *set << ratio;
      series->append(set);
      // check the type
    }

    if((data.mStatDataType != dataType) && (dataType == lastDataType))
      dataType = data.mStatDataType;

    lastDataType  = data.mStatDataType;
  }

  settings.mSetCustomAxes = false;
  settings.mIs3DData      = false;
  settings.mShowLegend    = true;
  settings.mSeries        = series;

  return settings;
}

chartSettingsData YUV3DCharts::makeStatisticsFrameRangeGrpByBlocksizeNrm(QList<collectedData> *aSortedData)
{
  // the amount of data is not the same, but the code is the same
  return this->makeStatisticsPerFrameGrpByBlocksizeNrm(aSortedData);
}

chartSettingsData YUV3DCharts::makeStatisticsAllFramesGrpByBlocksizeNrm(QList<collectedData> *aSortedData)
{
  // the amount of data is not the same, but the code is the same
  return this->makeStatisticsPerFrameGrpByBlocksizeNrm(aSortedData);
}

CollapsibleWidget::CollapsibleWidget(const QString& aTitle, const int aAnimationDuration, QWidget* aParent) : QWidget(aParent), mAnimationDuration(aAnimationDuration)
{
  // define the toggle button
  this->mToggleButton.setStyleSheet("QToolButton { border: none; }"); // no border
  this->mToggleButton.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  this->mToggleButton.setArrowType(Qt::ArrowType::RightArrow);
  this->mToggleButton.setText(aTitle);
  this->mToggleButton.setCheckable(true);
  this->mToggleButton.setChecked(false);

  // define the headerline
  this->mHeaderLine.setFrameShape(QFrame::HLine);
  this->mHeaderLine.setFrameShadow(QFrame::Sunken);
  this->mHeaderLine.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

  // define the content
  this->mContentArea.setStyleSheet("QScrollArea { background-color: white; border: none; }");
  this->mContentArea.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  // start out collapsed
  this->mContentArea.setMaximumHeight(0);
  this->mContentArea.setMinimumHeight(0);

  // let the entire widget grow and shrink with its content
  this->mToggleAnimation.addAnimation(new QPropertyAnimation(this, "minimumHeight"));
  this->mToggleAnimation.addAnimation(new QPropertyAnimation(this, "maximumHeight"));
  this->mToggleAnimation.addAnimation(new QPropertyAnimation(&mContentArea, "maximumHeight"));

  // don't waste space
  this->mMainLayout.setVerticalSpacing(0);
  this->mMainLayout.setContentsMargins(0, 0, 0, 0);

  // add elements to mainlayout
  int row = 0;
  this->mMainLayout.addWidget(&mToggleButton, row, 0, 1, 1, Qt::AlignLeft);
  this->mMainLayout.addWidget(&mHeaderLine, row++, 2, 1, 1);
  this->mMainLayout.addWidget(&mContentArea, row, 0, 1, 3);
  this->setLayout(&mMainLayout);

  //define what happen if toggle-button clicked
  connect(&mToggleButton, &QToolButton::clicked, [this](const bool aChecked) {
      this->mToggleButton.setArrowType(aChecked ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
      this->mToggleAnimation.setDirection(aChecked ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
      this->mToggleAnimation.start();
  });
}

void CollapsibleWidget::setContentLayout(QLayout& aContentLayout, const bool aDisplay)
{
  // delete the old layout from our content
  delete this->mContentArea.layout();

  // set the new layout to the content-area
  this->mContentArea.setLayout(&aContentLayout);

  // calculate the height's
  const auto collapsedHeight = sizeHint().height() - this->mContentArea.maximumHeight();
  auto contentHeight = aContentLayout.sizeHint().height();

  // set for all animations the height (start and end-values)
  for (int i = 0; i < this->mToggleAnimation.animationCount() - 1; ++i)
  {
    QPropertyAnimation* animation = static_cast<QPropertyAnimation*>(this->mToggleAnimation.animationAt(i));
    animation->setDuration(this->mAnimationDuration);
    animation->setStartValue(collapsedHeight);
    animation->setEndValue(collapsedHeight + contentHeight);
  }

  // set the animation for the content
  QPropertyAnimation* contentAnimation = static_cast<QPropertyAnimation*>(this->mToggleAnimation.animationAt(this->mToggleAnimation.animationCount() - 1));
  contentAnimation->setDuration(this->mAnimationDuration);
  contentAnimation->setStartValue(0);
  contentAnimation->setEndValue(contentHeight);

  // the widget is always collapsed, at this point it will unfold
  if(aDisplay)
    this->mToggleButton.clicked();
}

YUV3DSurfaceChart::YUV3DSurfaceChart(QWidget* aNoDataToShowWidget, QWidget* aDataIsLoadingWidget) :
  YUV3DCharts(aNoDataToShowWidget, aDataIsLoadingWidget)
{
  // create the graph
  Q3DSurface* widgetgraph = new Q3DSurface();

  // necessary! get an container and check that we can init OpenGL
  QWidget* container = QWidget::createWindowContainer(widgetgraph);

  // very important! no OpenGL --> no 3d graph
  this->mHasOpenGL = widgetgraph->hasContext();

  if(!this->mHasOpenGL) // check for OpenGL
  {
    QMessageBox msgBox;
    msgBox.setText("Couldn't initialize the OpenGL context. Can´t display 3D charts.");
    msgBox.exec();
  }
  else
  {
    // get basic widget
    this->mWidgetGraph = new QWidget;

    // create basic-layout and set to widget
    //QVBoxLayout* lyBasic = new QVBoxLayout(this->mWidgetGraph);

    QHBoxLayout *hLayout = new QHBoxLayout(this->mWidgetGraph);
    QVBoxLayout *vLayout = new QVBoxLayout();
    hLayout->addWidget(container, 1);
    hLayout->addLayout(vLayout);
    vLayout->setAlignment(Qt::AlignTop);    // create gridlayout for the controls

    QGroupBox *selectionGroupBox = new QGroupBox(QStringLiteral("Selection Mode"));

    QRadioButton *modeNoneRB = new QRadioButton(this->mWidgetGraph);
    modeNoneRB->setText(QStringLiteral("No selection"));
    modeNoneRB->setChecked(false);

    QRadioButton *modeItemRB = new QRadioButton(this->mWidgetGraph);
    modeItemRB->setText(QStringLiteral("Item"));
    modeItemRB->setChecked(false);

    QRadioButton *modeSliceRowRB = new QRadioButton(this->mWidgetGraph);
    modeSliceRowRB->setText(QStringLiteral("Row Slice"));
    modeSliceRowRB->setChecked(false);

    QRadioButton *modeSliceColumnRB = new QRadioButton(this->mWidgetGraph);
    modeSliceColumnRB->setText(QStringLiteral("Column Slice"));
    modeSliceColumnRB->setChecked(false);

    QVBoxLayout *selectionVBox = new QVBoxLayout;
    selectionVBox->addWidget(modeNoneRB);
    selectionVBox->addWidget(modeItemRB);
    selectionVBox->addWidget(modeSliceRowRB);
    selectionVBox->addWidget(modeSliceColumnRB);
    selectionGroupBox->setLayout(selectionVBox);

    QSlider *axisMinSliderX = new QSlider(Qt::Horizontal, this->mWidgetGraph);
    axisMinSliderX->setMinimum(0);
    axisMinSliderX->setTickInterval(1);
    axisMinSliderX->setEnabled(true);
    QSlider *axisMaxSliderX = new QSlider(Qt::Horizontal, this->mWidgetGraph);
    axisMaxSliderX->setMinimum(1);
    axisMaxSliderX->setTickInterval(1);
    axisMaxSliderX->setEnabled(true);
    QSlider *axisMinSliderZ = new QSlider(Qt::Horizontal, this->mWidgetGraph);
    axisMinSliderZ->setMinimum(0);
    axisMinSliderZ->setTickInterval(1);
    axisMinSliderZ->setEnabled(true);
    QSlider *axisMaxSliderZ = new QSlider(Qt::Horizontal, this->mWidgetGraph);
    axisMaxSliderZ->setMinimum(1);
    axisMaxSliderZ->setTickInterval(1);
    axisMaxSliderZ->setEnabled(true);

    vLayout->addWidget(selectionGroupBox);
    vLayout->addWidget(new QLabel(QStringLiteral("Column range")));
    vLayout->addWidget(axisMinSliderX);
    vLayout->addWidget(axisMaxSliderX);
    vLayout->addWidget(new QLabel(QStringLiteral("Row range")));
    vLayout->addWidget(axisMinSliderZ);
    vLayout->addWidget(axisMaxSliderZ);

    // create modifier to handle the sliders and so on
    Surface3DGraphModifier* modifier = new Surface3DGraphModifier(widgetgraph);
    this->mModifier = modifier;
    modifier->setAxisMinSliderX(axisMinSliderX);
    modifier->setAxisMaxSliderX(axisMaxSliderX);
    modifier->setAxisMinSliderZ(axisMinSliderZ);
    modifier->setAxisMaxSliderZ(axisMaxSliderZ);

    // connect modifier to handle the actions
    connect(modeNoneRB,         &QRadioButton::toggled, modifier, &Surface3DGraphModifier::toggleModeNone);
    connect(modeItemRB,         &QRadioButton::toggled, modifier, &Surface3DGraphModifier::toggleModeItem);
    connect(modeSliceRowRB,     &QRadioButton::toggled, modifier, &Surface3DGraphModifier::toggleModeSliceRow);
    connect(modeSliceColumnRB,  &QRadioButton::toggled, modifier, &Surface3DGraphModifier::toggleModeSliceColumn);
    connect(axisMinSliderX,     &QSlider::valueChanged, modifier, &Surface3DGraphModifier::adjustXMin);
    connect(axisMaxSliderX,     &QSlider::valueChanged, modifier, &Surface3DGraphModifier::adjustXMax);
    connect(axisMinSliderZ,     &QSlider::valueChanged, modifier, &Surface3DGraphModifier::adjustZMin);
    connect(axisMaxSliderZ,     &QSlider::valueChanged, modifier, &Surface3DGraphModifier::adjustZMax);

    modeItemRB->setChecked(true);
  }
}

bool YUV3DCharts::hasOpenGL() const
{
  return this->mHasOpenGL;
}
