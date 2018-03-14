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

int YUVCharts::getTotalAmountOfPixel(playlistItem* aItem, const ChartShow aShow, const indexRange aRange)
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
  mBbarChart3D(this->mNoDataToShowWidget, this->mDataIsLoadingWidget)
{

}

QWidget* YUVChartFactory::createChart(const ChartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData)
{
  Q_UNUSED(aSortedData)

  QList<collectedData>* sortedData = aItem->sortAndCategorizeDataByRange(aType, aRange);

  // our bar chart can not display 3D, so we check if in one collectedData has 3D-Data
  bool is2DData = this->is2DData(sortedData);
  if(is2DData)
  {
    return this->mBarChart.createChart(aOrderBy, aItem, aRange, aType, sortedData);
  }

  bool is3DData = this->is3DData(sortedData);
  if(is3DData)
  {
    //check if we have to take care of the 3D-data-range
    if(this->mUse3DCoordination)
      this->mBbarChart3D.set3DCoordinationRange(this->mMinX, this->mMaxX, this->mMinY, this->mMaxY);

    return this->mBbarChart3D.createChart(aOrderBy, aItem, aRange, aType, sortedData);
  }

  return this->mNoDataToShowWidget;
}

void YUVChartFactory::set3DCoordinationRange(const int aMinX, const int aMaxX, const int aMinY, const int aMaxY)
{
  this->mUse3DCoordination = true;
  this->mMinX = aMinX;
  this->mMaxX = aMaxX;
  this->mMinY = aMinY;
  this->mMaxY = aMaxY;
}

void YUVChartFactory::set3DCoordinationtoDefault()
{
  this->mUse3DCoordination = false;
}

QWidget* YUVBarChart::createChart(const ChartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData)
{
  // just a holder
  QList<collectedData>* sortedData = NULL;

  // check the datasource
  if(aSortedData)
    sortedData = aSortedData;
  else
    sortedData = aItem->sortAndCategorizeDataByRange(aType, aRange);

  // can we display it?
  if(this->is2DData(sortedData))
    return this->makeStatistic(sortedData, aOrderBy, aItem, aRange);
  else // at this point we have 3D Data and we can not display it with YUVBarChart
    return this->mNoDataToShowWidget;
}

QWidget* YUVBarChart::makeStatistic(QList<collectedData>* aSortedData, const ChartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange)
{
  // if we have no keys, we cant show any data so return at this point
  if(!aSortedData->count())
    return this->mNoDataToShowWidget;

  chartSettingsData settings;

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
  // auxililary var, to coordinate categories and sets
  QHash<QString, QBarSet*> coordCategorieSet;


  // running thru the sorted Data
  for(int i = 0; i < aSortedData->count(); i++)
  {
    // first getting the data
    collectedData data = aSortedData->at(i);
    if(data.mStatDataType == sdtStructStatisticsItem_Value)
    {
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
          if(!coordCategorieSet.contains(valueString))
          {
            set = new QBarSet(valueString);
            coordCategorieSet.insert(valueString, set);
          }
          else
            set = coordCategorieSet.value(valueString);

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
        foreach (QString key, coordCategorieSet.keys())
          series->append(coordCategorieSet.value(key));
      }

      if(moreThanOneElement)
        //at least appending the label to the categories for the axes if necessary
         settings.mCategories << data.mLabel;
    }
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
    if(data.mStatDataType == sdtStructStatisticsItem_Value)
    {
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

chartSettingsData YUVBarChart::makeStatisticsFrameRangeGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem, const indexRange aRange)
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

chartSettingsData YUVBarChart::calculateAndDefineGrpByValueNrmArea(QList<collectedData>* aSortedData, const int aTotalAmountPixel)
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
    if(data.mStatDataType == sdtStructStatisticsItem_Value)
    {
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
        *count += (width * height) * amount;
      }
    }
  }

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

chartSettingsData YUVBarChart::calculateAndDefineGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData, const int aTotalAmountPixel)
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
    // first getting the data
    collectedData data = aSortedData->at(i);
    if(data.mStatDataType == sdtStructStatisticsItem_Value)
    {
      // get the width and the heigth
      QStringList numberStrings = data.mLabel.split("x");
      QString widthStr  = numberStrings.at(0);
      QString heightStr = numberStrings.at(1);
      int width = widthStr.toInt();
      int height = heightStr.toInt();

      int amountPixelofValue = 0;
      // if we have more than one value
      foreach (auto chartData, data.mValues)
        amountPixelofValue += ((width * height) * chartData->second); // chartData->second holds the amount

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
  }

  return settings;
}

YUV3DBarChart::YUV3DBarChart(QWidget *aNoDataToShowWidget, QWidget *aDataIsLoadingWidget) :
  YUVCharts(aNoDataToShowWidget, aDataIsLoadingWidget)
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
    this->mWidget = new QWidget;

    // create basic-layout and set to widget
    QVBoxLayout* lyBasic = new QVBoxLayout(this->mWidget);

    // create gridlayout for the controls
    QGridLayout* lyGridControls = new QGridLayout();

    // add container and control-layout to basiclayout
    lyBasic->addWidget(container, 1);
    lyBasic->addLayout(lyGridControls);

    // create a button to zoom to an specific bar
    QPushButton* zoomToSelectedButton = new QPushButton(this->mWidget);
    zoomToSelectedButton->setText(QStringLiteral("Zoom to selected bar"));

    // create a slider to rotate around the x-axis
    QSlider *rotationSliderX = new QSlider(Qt::Horizontal, this->mWidget);
    rotationSliderX->setTickInterval(30);
    rotationSliderX->setTickPosition(QSlider::TicksBelow);
    rotationSliderX->setMinimum(-180);
    rotationSliderX->setValue(0);
    rotationSliderX->setMaximum(180);

    // create a slider to rotate around the y-axis
    QSlider *rotationSliderY = new QSlider(Qt::Horizontal, this->mWidget);
    rotationSliderY->setTickInterval(15);
    rotationSliderY->setTickPosition(QSlider::TicksBelow);
    rotationSliderY->setMinimum(-90);
    rotationSliderY->setValue(0);
    rotationSliderY->setMaximum(90);

    // create a slider to rotate the labels
    QSlider *axisLabelRotationSlider = new QSlider(Qt::Horizontal, this->mWidget);
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
    connect(rotationSliderX, &QSlider::valueChanged, this->mModifier, &GraphModifier3DBars::rotateX);
    connect(rotationSliderY, &QSlider::valueChanged, this->mModifier, &GraphModifier3DBars::rotateY);
    connect(axisLabelRotationSlider, &QSlider::valueChanged, this->mModifier, &GraphModifier3DBars::changeLabelRotation);
    connect(zoomToSelectedButton, &QPushButton::clicked, this->mModifier, &GraphModifier3DBars::zoomToSelectedBar);
  }
}

QWidget* YUV3DBarChart::createChart(const ChartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData)
{
  // check that we have init OpenGl
  if(!this->hasOpenGL())
    return this->mNoDataToShowWidget;

  // just a holder
  QList<collectedData>* sortedData = NULL;

  // check the datasource
  if(aSortedData)
    sortedData = aSortedData;
  else
    sortedData = aItem->sortAndCategorizeDataByRange(aType, aRange);

  // can we display it?
  if(this->is3DData(sortedData))
    return  this->makeStatistic(sortedData, aOrderBy, aItem, aRange);
  else // at this point we have 3D Data and we can not display it with YUVBarChart
    return this->mNoDataToShowWidget;
}

void YUV3DBarChart::set3DCoordinationRange(const int aMinX, const int aMaxX, const int aMinY, const int aMaxY)
{
  this->mUse3DCoordination = true;
  this->mMinX = aMinX;
  this->mMaxX = aMaxX;
  this->mMinY = aMinY;
  this->mMaxY = aMaxY;
}

void YUV3DBarChart::set3DCoordinationtoDefault()
{
  this->mUse3DCoordination = false;
}

bool YUV3DBarChart::hasOpenGL() const
{
  return mHasOpenGL;
}

QWidget* YUV3DBarChart::makeStatistic(QList<collectedData>* aSortedData, const ChartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange)
{
  // if we have no keys, we cant show any data so return at this point
  if(!aSortedData->count())
    return this->mNoDataToShowWidget;

  chartSettingsData settings;

  switch (aOrderBy)
  {
    case cobPerFrameGrpByValueNrmNone:
      settings = this->makeStatisticsPerFrameGrpByValNrmNone(aSortedData);
      break;
    case cobPerFrameGrpByValueNrmByArea:
      settings = this->makeStatisticsPerFrameGrpByValNrm(aSortedData);
      break;
    case cobPerFrameGrpByBlocksizeNrmNone:
      settings.mSettingsIsValid = false;
      break;
    case cobPerFrameGrpByBlocksizeNrmByArea:
      settings.mSettingsIsValid = false;
      break;

    case cobRangeGrpByValueNrmNone:
      settings = this->makeStatisticsFrameRangeGrpByValNrmNone(aSortedData);
      break;
    case cobRangeGrpByValueNrmByArea:
      settings = this->makeStatisticsFrameRangeGrpByValNrm(aSortedData);
      break;
    case cobRangeGrpByBlocksizeNrmNone:
    settings.mSettingsIsValid = false;
      break;
    case cobRangeGrpByBlocksizeNrmByArea:
    settings.mSettingsIsValid = false;
      break;

    case cobAllFramesGrpByValueNrmNone:
      settings = this->makeStatisticsAllFramesGrpByValNrmNone(aSortedData);
      break;
    case cobAllFramesGrpByValueNrmByArea:
      settings = this->makeStatisticsAllFramesGrpByValNrm(aSortedData);
      break;
    case cobAllFramesGrpByBlocksizeNrmNone:
    settings.mSettingsIsValid = false;
      break;
    case cobAllFramesGrpByBlocksizeNrmByArea:
    settings.mSettingsIsValid = false;
      break;

    default:
      return this->mNoDataToShowWidget;
  }

  if(!settings.mSettingsIsValid)
    return this->mNoDataToShowWidget;


  this->mModifier->applyDataToGraph(settings);

  return this->mWidget;
}

chartSettingsData YUV3DBarChart::makeStatisticsPerFrameGrpByValNrmNone(QList<collectedData>* aSortedData)
{
  chartSettingsData settings;

  // the result-value is a 2D-Array
  // we realize the array with an 2 dimensiol qmap, so we don´t have to search for the maximum x-value and y-value
  // in case of this, we can reduce the amount of loops
  // each x and y index we use is already defined with 0 (int)
  QMap<int, QMap<int, double>> resultValueCount;

  // in this case we programm two identic loops and we check the 3D-coordinates before.
  // the advantage is, that we dont need to check every loop pass
  // we look at a specific range
  if(this->mUse3DCoordination)
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

          if((x >= this->mMinX && x <= this->mMaxX) && (y >= this->mMinY && y <= this->mMaxY))
              // getting the coordinates from the point and use them as index for our 2D-Map
              resultValueCount[x][y] += amount;
        }
      }
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
        }
      }
    }
  }

  settings.m3DData = resultValueCount;

  return settings;
}

chartSettingsData YUV3DBarChart::makeStatisticsFrameRangeGrpByValNrmNone(QList<collectedData>* aSortedData)
{
  // does the same as makeStatisticsPerFrameGrpByValNrmNone just the amount of sortedData is other
  return this->makeStatisticsPerFrameGrpByValNrmNone(aSortedData);
}

chartSettingsData YUV3DBarChart::makeStatisticsAllFramesGrpByValNrmNone(QList<collectedData>* aSortedData)
{
  // does the same as makeStatisticsPerFrameGrpByValNrmNone just the amount of sortedData is other
  return this->makeStatisticsPerFrameGrpByValNrmNone(aSortedData);
}

chartSettingsData YUV3DBarChart::makeStatisticsPerFrameGrpByValNrm(QList<collectedData> *aSortedData)
{
  chartSettingsData settings;

  int maxAmountVector = 0;

  // the result-value is a 2D-Array
  // we realize the array with an 2 dimensiol qmap, so we don´t have to search for the maximum x-value and y-value
  // in case of this, we can reduce the amount of loops
  // each x and y index we use is already defined with 0 (int)
  QMap<int, QMap<int, double>> resultValueCount;

  // in this case we programm two identic loops and we check the 3D-coordinates before.
  // the advantage is, that we dont need to check every loop pass
  // we look at a specific range
  if(this->mUse3DCoordination)
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

          if((x >= this->mMinX && x <= this->mMaxX) && (y >= this->mMinY && y <= this->mMaxY))
          {
            // getting the coordinates from the point and use them as index for our 2D-Map
            resultValueCount[x][y] += amount;
            maxAmountVector++;
          }
        }
      }
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
          maxAmountVector++;
          // getting the values
          QVariant variant = valuepair->first;
          int amount = valuepair->second;

          // in case of sdtStructStatisticsItem_Vector we know that we can cast the QVariant to an QPoint
          QPoint point = variant.toPoint();

          // getting the coordinates from the point and use them as index for our 2D-Map
          resultValueCount[point.x()][point.y()] += amount;
        }
      }
    }
  }

  QMap<int, QMap<int, double>> resultValue;
  foreach (int x, resultValueCount.keys())
  {
    QMap<int, double> row = resultValueCount.value(x);
    foreach (int y, row.keys())
    {
      resultValue[x][y] = (resultValueCount[x][y] / maxAmountVector) * 100.0;
    }
  }

  settings.m3DData = resultValue;

  return settings;
}

chartSettingsData YUV3DBarChart::makeStatisticsFrameRangeGrpByValNrm(QList<collectedData> *aSortedData)
{
  // does the same as makeStatisticsPerFrameGrpByValNrm, data amount is other
  return this->makeStatisticsPerFrameGrpByValNrm(aSortedData);
}

chartSettingsData YUV3DBarChart::makeStatisticsAllFramesGrpByValNrm(QList<collectedData> *aSortedData)
{
  // does the same as makeStatisticsPerFrameGrpByValNrm, data amount is other
  return this->makeStatisticsPerFrameGrpByValNrm(aSortedData);
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

  this->mHeaderLine.setFrameShape(QFrame::HLine);
  this->mHeaderLine.setFrameShadow(QFrame::Sunken);
  this->mHeaderLine.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

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
  delete this->mContentArea.layout();
  this->mContentArea.setLayout(&aContentLayout);
  const auto collapsedHeight = sizeHint().height() - this->mContentArea.maximumHeight();
  auto contentHeight = aContentLayout.sizeHint().height();

  for (int i = 0; i < this->mToggleAnimation.animationCount() - 1; ++i)
  {
    QPropertyAnimation* animation = static_cast<QPropertyAnimation*>(this->mToggleAnimation.animationAt(i));
    animation->setDuration(this->mAnimationDuration);
    animation->setStartValue(collapsedHeight);
    animation->setEndValue(collapsedHeight + contentHeight);
  }

  QPropertyAnimation* contentAnimation = static_cast<QPropertyAnimation*>(this->mToggleAnimation.animationAt(this->mToggleAnimation.animationCount() - 1));
  contentAnimation->setDuration(this->mAnimationDuration);
  contentAnimation->setStartValue(0);
  contentAnimation->setEndValue(contentHeight);

  if(aDisplay)
    this->mToggleButton.clicked();
}
