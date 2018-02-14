#include "yuvcharts.h"
#include "playlistItem.h"

QWidget* YUVBarChart::createChart(const ChartOrderBy aOrderBy, playlistItem *aItem, indexRange aRange, QString aType)
{
  QList<collectedData>* sortedData = aItem->sortAndCategorizeDataByRange(aType, aRange);

  return this->makeStatistic(sortedData, aOrderBy, aItem, aRange);
}

QWidget* YUVLineChart::createChart(const ChartOrderBy aOrderBy, playlistItem *aItem, indexRange aRange, QString aType)
{
  QList<collectedData>* sortedData = aItem->sortAndCategorizeDataByRange(aType, aRange);

  return this->plotLineGraph(sortedData, aOrderBy, aItem, aRange);
}

int YUVCharts::getTotalAmountOfPixel(playlistItem* aItem, ChartShow aShow, indexRange aRange)
{
  // first calculate total amount of pixel
  QSize size = aItem->getSize();
  int totalAmountPixel = size.height() * size.width();

  if(aShow == csPerFrame)
    return totalAmountPixel;
  else if(aShow == csAllFrames)
    return (totalAmountPixel * aItem->getFrameIdxRange().second);
  else if(aShow == csRange)
    // in calculation you have to add 1, because the amount of viewed frames is always one higher than the calculated
    return (totalAmountPixel * (aRange.second - aRange.first + 1));
  else
    return 0;
}

QWidget* YUVLineChart::plotLineGraph(QList<collectedData>* aSortedData, const ChartOrderBy aOrderBy, playlistItem* aItem, indexRange aRange)
{

    QLineSeries *series = new QLineSeries();

    series->append(0, 6);
    series->append(2, 4);
    series->append(3, 8);
    series->append(7, 4);
    series->append(10, 5);
    *series << QPointF(11, 1) << QPointF(13, 3) << QPointF(17, 6) << QPointF(18, 3) << QPointF(20, 2);

    QChart *chart = new QChart();
    chart->legend()->hide();
    chart->addSeries(series);
    chart->createDefaultAxes();
    chart->setTitle("Simple line chart example");

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    //QMainWindow window;
    //window.setCentralWidget(chartView);
    //window.resize(400, 300);
    //window.show();

    // final return the created chart
    return chartView;

}

QWidget* YUVBarChart::makeStatistic(QList<collectedData>* aSortedData, const ChartOrderBy aOrderBy, playlistItem* aItem, indexRange aRange)
{
  // if we have no keys, we cant show any data so return at this point
  if(!aSortedData->count())
    return this->mNoDataToShowWidget;

  chartSettingsData settings;

  switch (aOrderBy) {
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
      settings = this->makeStatisticsPerFrameGrpByBlocksizeNrmArea(aSortedData, aItem);
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
      settings = this->makeStatisticsFrameRangeGrpByBlocksizeNrmArea(aSortedData, aItem, aRange);
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
      settings = this->makeStatisticsAllFramesGrpByBlocksizeNrmArea(aSortedData, aItem);
      break;

    default:
      return this->mNoDataToShowWidget;
  }

  if(!settings.mSettingsIsValid)
    return this->mNoDataToShowWidget;

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

chartSettingsData YUVBarChart::makeStatisticsPerFrameGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData)
{
  QBarSeries* series = new QBarSeries();

  // define result
  chartSettingsData settings;
  settings.mSeries = series;


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
      set->setLabel(data.mLabel); // setting new label
      series->append(set);      // appending the set to the series
    }
    else
    {
      foreach (QString key, settings.mTmpCoordCategorieSet.keys())
        series->append(settings.mTmpCoordCategorieSet.value(key));
    }

    if(moreThanOneElement)
      //at least appending the label to the categories for the axes if necessary
       settings.mCategories << data.mLabel;
  }

  return settings;
}

chartSettingsData YUVBarChart::makeStatisticsPerFrameGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem)
{
  // first calculate total amount of pixel
  // the range doesn matter, we look at one frame
  int totalAmountPixel = this->getTotalAmountOfPixel(aItem, csPerFrame, indexRange(0, 0));

  return this->calculateAndDefineGrpByBlocksizeNrmArea(aSortedData, totalAmountPixel);
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


  // at this pint we order the keys new from low to high
  // we cant use QHash at this point, because Qthe items are arbitrarily ordered in QHash, so we have to use QMap at this point
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

chartSettingsData YUVBarChart::makeStatisticsFrameRangeGrpByValNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem, indexRange aRange)
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

chartSettingsData YUVBarChart::makeStatisticsFrameRangeGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem, indexRange aRange)
{
  int totalAmountPixel = this->getTotalAmountOfPixel(aItem, csRange, aRange);

  return this->calculateAndDefineGrpByBlocksizeNrmArea(aSortedData, totalAmountPixel);
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

chartSettingsData YUVBarChart::makeStatisticsAllFramesGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem)
{
  // the range doesn matter, we look at all frames
  int totalAmountPixel = this->getTotalAmountOfPixel(aItem, csAllFrames, indexRange(0, 0));

  return this->calculateAndDefineGrpByBlocksizeNrmArea(aSortedData, totalAmountPixel);
}

chartSettingsData YUVBarChart::calculateAndDefineGrpByValueNrmArea(QList<collectedData>* aSortedData, int aTotalAmountPixel)
{
  QBarSeries* series = new QBarSeries();
  // define result
  chartSettingsData settings;
  settings.mSeries = series;

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

    // get the width and the heigth
    QStringList numberStrings = data.mLabel.split("x");
    QString widthStr  = numberStrings.at(0);
    QString heightStr = numberStrings.at(1);
    int width = widthStr.toInt();
    int height = heightStr.toInt();

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
      *count += (width * height) * chartData[1];
    }
  }

  // we cant use QHash at this point, because Qthe items are arbitrarily ordered in QHash, so we have to use QMap at this point
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

chartSettingsData YUVBarChart::calculateAndDefineGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData, int aTotalAmountPixel)
{
  QBarSeries* series = new QBarSeries();
  // define result
  chartSettingsData settings;
  settings.mSeries = series;

  // just a holder
  QBarSet *set;

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
    double ratio = (amountPixelofValue / (double)aTotalAmountPixel) * 100;

    // cause of maybe other pixelvalues it can happen that we calculate more pixel than we have really
    if(ratio > 100.0)
      ratio = 100.0;

    // create the set
    set = new QBarSet(data.mLabel);
    // fill the set with the data
    *set << ratio;
    // appen the set to the series
    series->append(set);
  }

  return settings;
}
