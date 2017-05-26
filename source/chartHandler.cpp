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

#include "chartHandler.h"

#include <QVBoxLayout>
#include "playlistItem.h"
#include "playlistItemStatisticsFile.h"
#include "statisticsExtensions.h"

// Default-Constructor
ChartHandler::ChartHandler()
{}
/*-------------------- public functions --------------------*/
QWidget* ChartHandler::createChartWidget(playlistItem *aItem)
{
  // check if the widget was already created and stored
  itemWidgetCoord coord;
  coord.mItem = aItem;
  if (mListItemWidget.contains(coord)) // was stored
    return mListItemWidget.at(mListItemWidget.indexOf(coord)).mWidget;

  // in case of playlistItemStatisticsFile
  if(dynamic_cast<playlistItemStatisticsFile*> (aItem))
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
  {
    return "Statistics File Chart";
  }

  // return default name
  return CHARTSWIDGET_DEFAULT_WINDOW_TITLE;
}

void ChartHandler::removeWidgetFromList(playlistItem* aItem)
{
  // if a item is deleted from the playlist, we have to remove the widget from the list
  itemWidgetCoord tmp;
  tmp.mItem = aItem;
  if (mListItemWidget.contains(tmp))
    this->mListItemWidget.removeAll(tmp);
}

/*-------------------- auxiliary functions --------------------*/
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
  qDebug() << "Frame: " << aNewFrameIndex;
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

  // geting the range
  auto range = aItem->getFrameIndexRange();
  // save the data, that we dont have to load it later again
  aCoord.mData = aItem->getData(range, true);

  //check if map contains items
  if(aCoord.mData->keys().count() > 0)
  {
    //map has items, so add them
    cbxTypes->addItem("Select...");

    foreach (QString type, aCoord.mData->keys())
    {
      cbxTypes->addItem(type); // fill with data
      //qDebug() << "else if(" << type.trimmed().toLower() << " == type)" << "\n" << "{\n\n}" ;
    }

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

  // adding the components
  topLayout->addWidget(lblStat);
  topLayout->addWidget(cbxTypes);

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

    // check that the found combination is valid
      if(coord == items[0])
    {
      if(aString != "Select...")
      {
        auto chart = this->createStatisticsChart(coord, aString);

        if(chart) // if chart has no data
        {
          // add chart to show but check if it was in before
          if (coord.mChart->indexOf(chart) == -1)
            coord.mChart->addWidget(chart);

          coord.mChart->setCurrentWidget(chart);
        }
      }
      else
      {
        auto dftWidget = this->mChartWidget->getDefaultWidget();
        if (coord.mChart->indexOf(dftWidget) == -1)
          coord.mChart->addWidget(dftWidget);

        coord.mChart->setCurrentWidget(dftWidget);
      }

    }
  }
}

QChartView* ChartHandler::createStatisticsChart(itemWidgetCoord& aCoord, const QString aType)
{
  // first we have to check, which type was chosen.
  // trim and to lower makes ist easier to compare
  const QString type = aType.trimmed().toLower();

  // get current frame index, we use the playback controller
  int frameIndex = this->mPlayback->getCurrentFrame();

  if("depth" == type)
    return this->makeStatisticDepth(aCoord, frameIndex);

  else if("geo_ctu_flag" == type)
    return this->makeDummyChart3();

  else if("geo_cu_flag" == type)
    return this->makeDummyChart4();

  else if("geo_candidate" == type)
    return this->makeDummyChart5();

  else if("geo_idx" == type)
    return this->makeDummyChart6();

  else if("geo_lineend" == type)
   return this->makeDummyChart3();

  else if("geo_linepredictor_flag" == type)
    return this->makeDummyChart3();

  else if("geo_linestart" == type)
    return this->makeDummyChart3();

  else if("geo_offset" == type)
    return this->makeDummyChart3();

  else if("imvflag" == type)
    return this->makeDummyChart3();

  else if("mvdl0" == type)
    return this->makeDummyChart3();

  else if("mvdl1" == type)
    return this->makeDummyChart3();

  else if("mvl0" == type)
    return this->makeDummyChart3();

  else if("mvl1" == type)
    return this->makeDummyChart3();

  else if("mvpidxl0" == type)
    return this->makeDummyChart3();

  else if("mvpidxl1" == type)
    return this->makeDummyChart3();

  else if("mergecandidate" == type)
    return this->makeDummyChart3();

  else if("mergeidx" == type)
    return this->makeDummyChart3();

  else if("mergemode" == type)
    return this->makeDummyChart3();

  else if("obmcflag" == type)
    return this->makeDummyChart3();

  else if("partsize" == type)
    return this->makeDummyChart3();

  else if("predmode" == type)
    return this->makeDummyChart3();

  else if("reffrmidxl0" == type)
    return this->makeDummyChart3();

  else if("reffrmidxl1" == type)
    return this->makeDummyChart3();

  else if("skipmode" == type)
    return this->makeDummyChart3();

  // should never pass but maybe we dont know the type
  return new QChartView;
}

QChartView* ChartHandler::makeStatisticDepth(itemWidgetCoord& aCoord, int aFrameIndex)
{
  struct chartDataStruct {
    int mDepth = -1;
    int mCntDepth = 0;

    void inline increment()
    {
      this->mCntDepth++;
    }
  };

  QList<QList<QVariant>> allData = aCoord.mData->value("Depth");
  QList<QVariant> data = allData.at(aFrameIndex);


  QMap<QString, QMap<int, int*>*> resultData;

  foreach (QVariant item, data)
  {
    if(item.canConvert<statisticsItem_Value>())
    {
      statisticsItem_Value value = item.value<statisticsItem_Value>();
      QString label = QString::number(value.size[0]) + "x" + QString::number(value.size[1]);

      int* chartDepthCnt;

      // hard part of the function
      // 1. check if label is in map
      // 2. if not: insert label and a new / second Map with the new values for depth
      // 3. if it was inside: check if Depth was inside the second map
      // 4. if not in second map create new Depth-data-container, fill with data and add to second map
      // 5. if it was in second map just increment the Depth-Counter
      if(!resultData.contains(label))
      {
        // label was not inside
        QMap<int, int*>* map = new QMap<int, int*>();

        // create Data, increment and add to second map
        chartDepthCnt = new int[2];

        chartDepthCnt[0] = value.value;
        chartDepthCnt[1]++;

        map->insert(chartDepthCnt[0], chartDepthCnt);
        resultData.insert(label, map);
      }
      else
      {
        // label was inside, check if Depth-value is inside
        QMap<int, int*>* map = resultData.value(label);

        // Depth-Value not inside
        if(!(map->contains(value.value)))
        {
          chartDepthCnt = new int[2];
          chartDepthCnt[0] = value.value;
          chartDepthCnt[1]++;
          map->insert(chartDepthCnt[0], chartDepthCnt);
        }
        else  // Depth-Value was inside
        {
          int* counter = map->value(value.value);
          counter[1]++;
        }
      }
    }
  }


  // now we have at this point all data and they are sorted like a tree
  QStringList categories;
  QBarSeries *series = new QBarSeries();

  QBarSet *set = new QBarSet("");
  foreach (QString label, resultData.keys())
  {
    set->setLabel(label);

    QMap<int, int*>* depthMap = resultData.value(label);

    foreach (int depthValue, depthMap->keys()) {
      int* chartData = depthMap->value(depthValue);
      *set << chartData[1];
      series->append(set);
    }

    categories << label;
  }

  QChart *chart = new QChart();
  chart->addSeries(series);
  chart->setTitle("Depth");
  chart->setAnimationOptions(QChart::SeriesAnimations);

  QBarCategoryAxis *axis = new QBarCategoryAxis();
  axis->append(categories);
  chart->createDefaultAxes();
  chart->setAxisX(axis, series);

  chart->legend()->setVisible(true);
  chart->legend()->setAlignment(Qt::AlignBottom);

  QChartView *chartView = new QChartView(chart);
  chartView->setRenderHint(QPainter::Antialiasing);

  return chartView;
}





//dummy
QChartView* ChartHandler::makeDummyChart()
{
  QBarSet *set0 = new QBarSet("Jane");
  QBarSet *set1 = new QBarSet("John");
  QBarSet *set2 = new QBarSet("Axel");
  QBarSet *set3 = new QBarSet("Mary");
  QBarSet *set4 = new QBarSet("Samantha");

  *set0 << 1 << 2 << 3 << 4 << 5 << 6;
  *set1 << 5 << 0 << 0 << 4 << 0 << 7;
  *set2 << 3 << 5 << 8 << 13 << 8 << 5;
  *set3 << 5 << 6 << 7 << 3 << 4 << 5;
  *set4 << 9 << 7 << 5 << 3 << 1 << 2;

  QBarSeries *series = new QBarSeries();
  series->append(set0);
  series->append(set1);
  series->append(set2);
  series->append(set3);
  series->append(set4);

  QChart *chart = new QChart();
  chart->addSeries(series);
  chart->setTitle("Simple barchart example");
  chart->setAnimationOptions(QChart::SeriesAnimations);

  QStringList categories;
  categories << "Jan" << "Feb" << "Mar" << "Apr" << "May" << "Jun";
  QBarCategoryAxis *axis = new QBarCategoryAxis();
  axis->append(categories);
  chart->createDefaultAxes();
  chart->setAxisX(axis, series);

  chart->legend()->setVisible(true);
  chart->legend()->setAlignment(Qt::AlignBottom);

  QChartView *chartView = new QChartView(chart);
  chartView->setRenderHint(QPainter::Antialiasing);

  return chartView;
}

//dummy
QChartView* ChartHandler::makeDummyChart2()
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

  return chartView;
}

//dummy
QChartView* ChartHandler::makeDummyChart3()
{

  QLineSeries *series0 = new QLineSeries();
  QLineSeries *series1 = new QLineSeries();

  *series0 << QPointF(1, 5) << QPointF(3, 7) << QPointF(7, 6) << QPointF(9, 7) << QPointF(12, 6)
           << QPointF(16, 7) << QPointF(18, 5);
  *series1 << QPointF(1, 3) << QPointF(3, 4) << QPointF(7, 3) << QPointF(8, 2) << QPointF(12, 3)
           << QPointF(16, 4) << QPointF(18, 3);

  QAreaSeries *series = new QAreaSeries(series0, series1);
  series->setName("Batman");
  QPen pen(0x059605);
  pen.setWidth(3);
  series->setPen(pen);

  QLinearGradient gradient(QPointF(0, 0), QPointF(0, 1));
  gradient.setColorAt(0.0, 0x3cc63c);
  gradient.setColorAt(1.0, 0x26f626);
  gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
  series->setBrush(gradient);

  QChart *chart = new QChart();
  chart->addSeries(series);
  chart->setTitle("Simple areachart example");
  chart->createDefaultAxes();
  chart->axisX()->setRange(0, 20);
  chart->axisY()->setRange(0, 10);

  QChartView *chartView = new QChartView(chart);
  chartView->setRenderHint(QPainter::Antialiasing);
  return chartView;
}

//dummy
QChartView* ChartHandler::makeDummyChart4()
{
  QPieSeries *series = new QPieSeries();
  series->setHoleSize(0.35);
  series->append("Protein 4.2%", 4.2);
  QPieSlice *slice = series->append("Fat 15.6%", 15.6);
  slice->setExploded();
  slice->setLabelVisible();
  series->append("Other 23.8%", 23.8);
  series->append("Carbs 56.4%", 56.4);

  QChartView *chartView = new QChartView();
  chartView->setRenderHint(QPainter::Antialiasing);
  chartView->chart()->setTitle("Donut with a lemon glaze (100g)");
  chartView->chart()->addSeries(series);
  chartView->chart()->legend()->setAlignment(Qt::AlignBottom);
  chartView->chart()->setTheme(QChart::ChartThemeBlueCerulean);
  chartView->chart()->legend()->setFont(QFont("Arial", 7));
  return chartView;
}

//dummy
QChartView* ChartHandler::makeDummyChart5()
{
  QPieSeries *series = new QPieSeries();
  series->append("Jane", 1);
  series->append("Joe", 2);
  series->append("Andy", 3);
  series->append("Barbara", 4);
  series->append("Axel", 5);

  QPieSlice *slice = series->slices().at(1);
  slice->setExploded();
  slice->setLabelVisible();
  slice->setPen(QPen(Qt::darkGreen, 2));
  slice->setBrush(Qt::green);

  QChart *chart = new QChart();
  chart->addSeries(series);
  chart->setTitle("Simple piechart example");
  chart->legend()->hide();

  QChartView *chartView = new QChartView(chart);
  chartView->setRenderHint(QPainter::Antialiasing);
  return chartView;
}

//dummy
QChartView* ChartHandler::makeDummyChart6()
{
  QSplineSeries *series = new QSplineSeries();
  series->setName("spline");

  series->append(0, 6);
  series->append(2, 4);
  series->append(3, 8);
  series->append(7, 4);
  series->append(10, 5);
  *series << QPointF(11, 1) << QPointF(13, 3) << QPointF(17, 6) << QPointF(18, 3) << QPointF(20, 2);

  QChart *chart = new QChart();
  chart->legend()->hide();
  chart->addSeries(series);
  chart->setTitle("Simple spline chart example");
  chart->createDefaultAxes();
  chart->axisY()->setRange(0, 10);

  QChartView *chartView = new QChartView(chart);
  chartView->setRenderHint(QPainter::Antialiasing);
  return chartView;
}

