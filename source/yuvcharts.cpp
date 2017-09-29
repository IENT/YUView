#include "yuvcharts.h"

YUVCharts::YUVCharts()
{
  this->mNoDataToShowWidget = aNoDataToShowWidget;
  this->mDataIsLoadingWidget = aDataIsLoadingWidget;
}

QWidget* YUVBarChart::createChart(QList<collectedData> *aSortedData, const ChartOrderBy aOrderBy, playlistItem *aItem)
{

}
