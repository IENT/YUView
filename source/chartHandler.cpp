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

#include "playlistItem.h"
#include "playlistItemStatisticsFile.h"
#include "statisticsExtensions.h"

// Default-Constructor
ChartHandler::ChartHandler()
{
}

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

// see docu @ChartHandler.h
QChartView* ChartHandler::createChart(itemWidgetCoord& aCoord)
{
  Q_UNUSED(aCoord)
  /*
  // getting tha data
  QMap<QString, QList<QList<QVariant>>>* map = aItem->getData(aItem->getFrameIndexRange(), true);

  // creating default chart
  QChart *chart = new QChart();
  chart->setTitle("Simple barchart");
  chart->setAnimationOptions(QChart::SeriesAnimations);

  foreach (QString key, map->keys())
  {
    QList<QList<QVariant>> alldata = map->value(key);

    QBarSeries *series = new QBarSeries();
    foreach (QList<QVariant> list, alldata)
    {
      QBarSet* set = new QBarSet(key);
      foreach (QVariant data, list)
      {
        if(data.canConvert<statisticsItem_Value>())
        {
          statisticsItem_Value item = data.value<statisticsItem_Value>();
          *set << item.value;
        }
      }
      series->append(set);
    }
    chart->addSeries(series);
  }

  QChartView *chartView = new QChartView(chart);
  chartView->setRenderHint(QPainter::Antialiasing);

  return chartView;
  */

  return this->makeDummyChart();
}

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
    coord.mWidget = mListItemWidget.at(mListItemWidget.indexOf(coord)).mWidget;
  else // we dont hold?! why
    return;

  // remove the chart from the shown stack
  this->mChartWidget->removeChartWidget(coord.mWidget);
  //remove the item-widget tupel from our list
  this->removeWidgetFromList(aItem);
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

QWidget* ChartHandler::createChartWidget(playlistItem *aItem)
{
  // check if the widget was already created and stored
  itemWidgetCoord coord;
  coord.mItem = aItem;
  if (mListItemWidget.contains(coord)) // was stored
    return mListItemWidget.at(mListItemWidget.indexOf(coord)).mWidget;

  // define a basic layout
  QWidget* result = new QWidget();
  QVBoxLayout* mainLayout = new QVBoxLayout;
  result->setLayout(mainLayout);

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
  else // in case of default, that we dont know the item-type
    coord.mWidget = this->mChartWidget->getDefaultWidget();

  // adding the widget to the baseic layout after getting it
  mainLayout->addWidget(coord.mWidget);

  return result;
}

QWidget* ChartHandler::createStatisticFileWidget(playlistItemStatisticsFile *aItem, itemWidgetCoord& aCoord)
{
  //define a simple layout for the statistic file
  QGroupBox *formGroupBox = new QGroupBox(tr(""));
  QFormLayout *layout = new QFormLayout;
  QComboBox* cbxTypes = new QComboBox;

  formGroupBox->setLayout(layout);

  // geting the range
  auto range = aItem->getFrameIndexRange();
  // save the data, that we dont have to load it later again
  aCoord.mData = aItem->getData(range, true);

  //check if map contains items
  if(aCoord.mData->keys().count() > 0)
  {
    //map has items, so add them
    cbxTypes->addItem("Select..."); // index 0, default

    foreach (QString type, aCoord.mData->keys())
      cbxTypes->addItem(type); // fill with data
  }
  else
    // no items, add a info
    cbxTypes->addItem("No types"); // index 0, default

  // @see http://stackoverflow.com/questions/16794695/connecting-overloaded-signals-and-slots-in-qt-5
  // do the connect after adding the items otherwise the connect will be call
  connect(cbxTypes,
          static_cast<void (QComboBox::*)(const QString &)> (&QComboBox::currentIndexChanged),
          this,
          &ChartHandler::onStatisticsChange);


  // adding the components
  layout->addRow(new QLabel(tr("Statistics: ")), cbxTypes);

  return formGroupBox;
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
        // create a widget includes the item-widget and the chart
        QGroupBox *grbChartForm = new QGroupBox(tr(""));
        QFormLayout *layout = new QFormLayout;

        // create the chart, based on the data, item etc
        QWidget* chart = this->createChart(coord);

        grbChartForm->setLayout(layout);
        layout->addWidget(coord.mWidget);
        layout->addWidget(chart);

        this->mChartWidget->setChartWidget(grbChartForm);
      }
      else
        // in the combobox "Select..." was chosen, so we have to load the basic Widget
        this->mChartWidget->setChartWidget(coord.mWidget);
    }
  }
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
