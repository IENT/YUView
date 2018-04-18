#include "graphmodifier.h"
#include <QtDataVisualization/qcategory3daxis.h>
#include <QtDataVisualization/qvalue3daxis.h>
#include <QtDataVisualization/qbardataproxy.h>
#include <QtDataVisualization/q3dscene.h>
#include <QtDataVisualization/q3dcamera.h>
#include <QtDataVisualization/qbar3dseries.h>
#include <QtDataVisualization/q3dtheme.h>
#include <QtCore/QTime>
#include <QtWidgets/QComboBox>
#include <QtCore/qmath.h>


#include <QtDataVisualization/QValue3DAxis>
#include <QtDataVisualization/Q3DTheme>
#include <QtGui/QImage>

using namespace QtDataVisualization;

GraphModifier3DBars::GraphModifier3DBars(Q3DBars* aBarGraph)
  : mGraph(aBarGraph),
    mPrimarySeries(new QBar3DSeries),
    mRotationX(0.0f),
    mRotationY(0.0f),
    mMinval(0.0f),
    mMaxVal(10.0f),
    mAmountAxis(new QValue3DAxis),
    mXValueAxis(new QCategory3DAxis),
    mYValueAxis(new QCategory3DAxis)
{
  this->mGraph->setShadowQuality(QAbstract3DGraph::ShadowQualitySoftMedium);
  this->mGraph->activeTheme()->setBackgroundEnabled(false);
  this->mGraph->activeTheme()->setFont(QFont("Times New Roman", 8));
  this->mGraph->activeTheme()->setLabelBackgroundEnabled(true);
  this->mGraph->setMultiSeriesUniform(true);

  this->mGraph->setBarSpacingRelative(true);
  this->mGraph->setBarThickness(0.5f);
  this->mGraph->setBarSpacing(QSizeF(2.5, 2.5));

  // necessary, that we can display the title of the axis, the stringlists includes at minimum of one element
  this->mCategoriesY << "";
  this->mCategoriesX << "";

  // generate default settings
  this->mAmountAxis->setTitle("Amount (z-axis)");
  this->mAmountAxis->setRange(mMinval, mMaxVal);
  this->mAmountAxis->setLabelFormat(QString(QStringLiteral("%.0f ")));
  this->mAmountAxis->setLabelAutoRotation(30.0f);
  this->mAmountAxis->setTitleVisible(true);

  this->mXValueAxis->setTitle("x-value (x-axis)");
  this->mXValueAxis->setLabelAutoRotation(30.0f);
  this->mXValueAxis->setTitleVisible(true);

  this->mYValueAxis->setTitle("y-value (y-axis)");
  this->mYValueAxis->setLabelAutoRotation(30.0f);
  this->mYValueAxis->setTitleVisible(true);

  this->mGraph->setValueAxis(mAmountAxis);
  this->mGraph->setRowAxis(mXValueAxis);
  this->mGraph->setColumnAxis(mYValueAxis);

  this->mPrimarySeries->setItemLabelFormat(QStringLiteral("(x:@rowLabel/y:@colLabel) - amount:@valueLabel"));
  this->mPrimarySeries->setMesh(QAbstract3DSeries::MeshBevelBar);
  this->mPrimarySeries->setMeshSmooth(true);

  this->mGraph->addSeries(this->mPrimarySeries);

  this->changePresetCamera();

  // Set up property animations for zooming to the selected bar
  Q3DCamera* camera     = mGraph->scene()->activeCamera();
  this->mDefaultAngleX  = camera->xRotation();
  this->mDefaultAngleY  = camera->yRotation();
  this->mDefaultZoom    = camera->zoomLevel();
  this->mDefaultTarget  = camera->target();

  this->mAnimationCameraX.setTargetObject(camera);
  this->mAnimationCameraY.setTargetObject(camera);
  this->mAnimationCameraZoom.setTargetObject(camera);
  this->mAnimationCameraTarget.setTargetObject(camera);

  this->mAnimationCameraX.setPropertyName("xRotation");
  this->mAnimationCameraY.setPropertyName("yRotation");
  this->mAnimationCameraZoom.setPropertyName("zoomLevel");
  this->mAnimationCameraTarget.setPropertyName("target");

  int duration = 1700;
  this->mAnimationCameraX.setDuration(duration);
  this->mAnimationCameraY.setDuration(duration);
  this->mAnimationCameraZoom.setDuration(duration);
  this->mAnimationCameraTarget.setDuration(duration);

  // The zoom always first zooms out above the graph and then zooms in
  qreal zoomOutFraction = 0.3;
  this->mAnimationCameraX.setKeyValueAt(zoomOutFraction, QVariant::fromValue(0.0f));
  this->mAnimationCameraY.setKeyValueAt(zoomOutFraction, QVariant::fromValue(90.0f));
  this->mAnimationCameraZoom.setKeyValueAt(zoomOutFraction, QVariant::fromValue(50.0f));
  this->mAnimationCameraTarget.setKeyValueAt(zoomOutFraction, QVariant::fromValue(QVector3D(0.0f, 0.0f, 0.0f)));
}

GraphModifier3DBars::~GraphModifier3DBars()
{
  delete this->mGraph;
}

void GraphModifier3DBars::applyDataToGraph(chartSettingsData aSettings)
{
  this->mSettings = aSettings;

  this->mCategoriesX.clear();
  this->mCategoriesY.clear();

  //holder for the maximum amount for the amount-axis
  float maxAmount = -1.0f;

  // Set up data
  // Create data arrays
  QBarDataArray* dataSet = new QBarDataArray;
  QBarDataRow* dataRow;

  dataSet->reserve(qAbs(this->mSettings.mX3DRange.first) + qAbs(this->mSettings.mX3DRange.second) + 1);

  for(int x = this->mSettings.mX3DRange.first; x <  this->mSettings.mX3DRange.second; x++)
  {
    this->mCategoriesX << QString::number(x);

    // generate the new datarow with the range of y
    int rowItemAmount = qAbs(this->mSettings.mY3DRange.first) + qAbs(this->mSettings.mY3DRange.second) + 1;
    dataRow = new QBarDataRow(rowItemAmount);
    // this is necessary for the position in the row. The value of y is not always the position in the row
    int posY = 0;

    for(int y = this->mSettings.mY3DRange.first; y <=  this->mSettings.mY3DRange.second; y++)
    {
      this->mCategoriesY << QString::number(y);

      // Add data to the row
      double amount = this->mSettings.m3DData[x][y];
      (*dataRow)[posY++].setValue(amount);

      // getting the maximun amount to resize the axis
      if(amount > maxAmount)
          maxAmount = amount;
    }

    dataSet->append(dataRow);
  }

  // Add data to the data proxy (the data proxy assumes ownership of it)
  this->mPrimarySeries->dataProxy()->resetArray(dataSet, mCategoriesX, mCategoriesY);

  // setting the new maxval
  if(maxAmount != -1.0f)
    this->mMaxVal = maxAmount;

  // setting the new range
  this->mAmountAxis->setRange(this->mMinval, this->mMaxVal);

  int segments = 1;

  if(this->mMaxVal != (int)this->mMaxVal) // check if number has precision, so we have percentages
  {
    this->mAmountAxis->setLabelFormat(QString(QStringLiteral("%.4f\%")));

    // calculate the amount of segments for the amount-axis
    if(this->mMaxVal > 1.0 && this->mMaxVal < 10.0)
      segments = (int)this->mMaxVal;
    else if(this->mMaxVal >= 10.0 && this->mMaxVal <= 100.0)
      segments = 5;
  }
  else
  {
    this->mPrimarySeries->setItemLabelFormat(QStringLiteral("(x:@rowLabel/y:@colLabel) - amount:@valueLabel"));
    this->mAmountAxis->setLabelFormat(QString(QStringLiteral("%.0f")));

    int maxval = ((int)this->mMaxVal);

    if(maxval > 1 && maxval <= 10)
      segments = maxval;

    // calculate the amount of segments for the amount-axis
    int pass = 1; // how often the loop was passed
    while(segments == 1)
    {
      int left  = 1 + pass * 10;
      int right = pass++ * 100;
      int dev   = right / 10;

      if(maxval > left && maxval <= right)
        segments = maxval / dev;
    }
  }

  this->mAmountAxis->setSegmentCount(segments);
}

void GraphModifier3DBars::changePresetCamera()
{
  this->mAnimationCameraX.stop();
  this->mAnimationCameraY.stop();
  this->mAnimationCameraZoom.stop();
  this->mAnimationCameraTarget.stop();

  // Restore camera target in case animation has changed it
  this->mGraph->scene()->activeCamera()->setTarget(QVector3D(0.0f, 0.0f, 0.0f));

  // static is necessary, so we can look next time which position the camera has
  static int preset = Q3DCamera::CameraPresetFront;

  this->mGraph->scene()->activeCamera()->setCameraPreset((Q3DCamera::CameraPreset)preset);

  if (++preset > Q3DCamera::CameraPresetDirectlyBelow)
      preset = Q3DCamera::CameraPresetFrontLow;
}

void GraphModifier3DBars::changeLabelRotation(int aRotation)
{
  this->mAmountAxis->setLabelAutoRotation(float(aRotation));
  this->mYValueAxis->setLabelAutoRotation(float(aRotation));
  this->mXValueAxis->setLabelAutoRotation(float(aRotation));
}

void GraphModifier3DBars::zoomToSelectedBar()
{
  // stop all cameras
  this->mAnimationCameraX.stop();
  this->mAnimationCameraY.stop();
  this->mAnimationCameraZoom.stop();
  this->mAnimationCameraTarget.stop();

  // get actual active camera position
  Q3DCamera* camera       = this->mGraph->scene()->activeCamera();

  float currentX          = camera->xRotation();
  float currentY          = camera->yRotation();
  float currentZoom       = camera->zoomLevel();
  QVector3D currentTarget = camera->target();

  this->mAnimationCameraX.setStartValue(QVariant::fromValue(currentX));
  this->mAnimationCameraY.setStartValue(QVariant::fromValue(currentY));
  this->mAnimationCameraZoom.setStartValue(QVariant::fromValue(currentZoom));
  this->mAnimationCameraTarget.setStartValue(QVariant::fromValue(currentTarget));

  // get the selected bar or mark as invalid
  QPoint selectedBar = mGraph->selectedSeries()
                                    ? this->mGraph->selectedSeries()->selectedBar()
                                    : QBar3DSeries::invalidSelectionPosition();

  if (selectedBar != QBar3DSeries::invalidSelectionPosition())
  {
    // Normalize selected bar position within axis range to determine target coordinates
    QVector3D endTarget;
    float xMin    = this->mGraph->columnAxis()->min();
    float xRange  = this->mGraph->columnAxis()->max() - xMin;
    float zMin    = this->mGraph->rowAxis()->min();
    float zRange  = this->mGraph->rowAxis()->max() - zMin;
    endTarget.setX((selectedBar.y() - xMin) / xRange * 2.0f - 1.0f);
    endTarget.setZ((selectedBar.x() - zMin) / zRange * 2.0f - 1.0f);

    // Rotate the camera so that it always points approximately to the graph center
    qreal endAngleX = 90.0 - qRadiansToDegrees(qAtan(qreal(endTarget.z() / endTarget.x())));

    if (endTarget.x() > 0.0f)
        endAngleX -= 180.0f;

    // get selected barvalue
    float barValue = this->mGraph->selectedSeries()->dataProxy()->itemAt(selectedBar.x(), selectedBar.y())->value();
    float endAngleY = barValue >= 0.0f ? 30.0f : -30.0f;

    if (this->mGraph->valueAxis()->reversed())
        endAngleY *= -1.0f;

    this->mAnimationCameraX.setEndValue(QVariant::fromValue(float(endAngleX)));
    this->mAnimationCameraY.setEndValue(QVariant::fromValue(endAngleY));
    this->mAnimationCameraZoom.setEndValue(QVariant::fromValue(250));
    this->mAnimationCameraTarget.setEndValue(QVariant::fromValue(endTarget));
  }
  else // No selected bar, so return to the default view
  {
    this->mAnimationCameraX.setEndValue(QVariant::fromValue(this->mDefaultAngleX));
    this->mAnimationCameraY.setEndValue(QVariant::fromValue(this->mDefaultAngleY));
    this->mAnimationCameraZoom.setEndValue(QVariant::fromValue(this->mDefaultZoom));
    this->mAnimationCameraTarget.setEndValue(QVariant::fromValue(this->mDefaultTarget));
  }

  // start the camera, because at the beginning we stop it
  this->mAnimationCameraX.start();
  this->mAnimationCameraY.start();
  this->mAnimationCameraZoom.start();
  this->mAnimationCameraTarget.start();
}

void GraphModifier3DBars::rotateX(int aRotation)
{
  this->mRotationX = aRotation;
  this->mGraph->scene()->activeCamera()->setCameraPosition(mRotationX, mRotationY);
}

void GraphModifier3DBars::rotateY(int aRotation)
{
  this->mRotationY = aRotation;
  this->mGraph->scene()->activeCamera()->setCameraPosition(mRotationX, mRotationY);
}

Surface3DGraphModifier::Surface3DGraphModifier(Q3DSurface* aSurfaceGraph) :
  mGraph(aSurfaceGraph)
{
  this->mAxisX = new QValue3DAxis;
  this->mAxisY = new QValue3DAxis;
  this->mAxisZ = new QValue3DAxis;

  this->mAxisX->setLabelFormat("%.0f");
  this->mAxisY->setLabelFormat("%.0f");
  this->mAxisZ->setLabelFormat("%.0f");
  this->mAxisX->setLabelAutoRotation(30);
  this->mAxisY->setLabelAutoRotation(90);
  this->mAxisZ->setLabelAutoRotation(30);

  this->mGraph->setAxisX(this->mAxisX);
  this->mGraph->setAxisY(this->mAxisY);
  this->mGraph->setAxisZ(this->mAxisZ);

  this->mPrimarySeriesProxy = new QSurfaceDataProxy();
  this->mPrimarySeries = new QSurface3DSeries(this->mPrimarySeriesProxy);

  this->mPrimarySeries->setItemLabelFormat(QStringLiteral("(x:@xLabel/y:@zLabel) - amount:@yLabel"));

  this->mGraph->addSeries(this->mPrimarySeries);

  // create the colorGradient
  this->mColorGradient.setColorAt(0.0,  Qt::blue);
  this->mColorGradient.setColorAt(0.25, Qt::cyan);
  this->mColorGradient.setColorAt(0.5,  Qt::green);
  this->mColorGradient.setColorAt(0.75, Qt::yellow);
  this->mColorGradient.setColorAt(1.0,  Qt::red);
}

void Surface3DGraphModifier::applyDataToGraph(chartSettingsData aSettings)
{
  this->mSettings = aSettings;

  //holder for the maximum amount for the amount-axis
  float maxAmount = -1.0f;

  // Set up data
  // Create data arrays
  QSurfaceDataArray* dataSet = new QSurfaceDataArray;
  QSurfaceDataRow* dataRow;

  int colItemsAmount = qAbs(this->mSettings.mX3DRange.first) + qAbs(this->mSettings.mX3DRange.second) + 1;
  dataSet->reserve(colItemsAmount);

  for(int x = this->mSettings.mX3DRange.first; x <=  this->mSettings.mX3DRange.second; x++)
  {
    // generate the new datarow with the range of y
    int rowItemsAmount = qAbs(this->mSettings.mY3DRange.first) + qAbs(this->mSettings.mY3DRange.second) + 1;
    dataRow = new QSurfaceDataRow(rowItemsAmount);

    // this is necessary for the position in the row. The value of y is not always the position in the row
    int posY = 0;

    for(int y = this->mSettings.mY3DRange.first; y <=  this->mSettings.mY3DRange.second; y++)
    {
      // Add data to the row
      double amount = this->mSettings.m3DData[x][y];
      (*dataRow)[posY++].setPosition(QVector3D(y, amount, x));

      // getting the maximun amount to resize the axis
      if(amount > maxAmount)
          maxAmount = amount;
    }

    dataSet->append(dataRow);
  }

  this->mPrimarySeriesProxy->resetArray(dataSet);

  this->mPrimarySeries->setDrawMode(QSurface3DSeries::DrawSurfaceAndWireframe);
  this->mPrimarySeries->setFlatShadingEnabled(true);

  this->mAxisX->setRange(this->mSettings.mX3DRange.first, this->mSettings.mX3DRange.second);
  this->mAxisZ->setRange(this->mSettings.mY3DRange.first, this->mSettings.mY3DRange.second);
  this->mAxisY->setRange(0.0f, maxAmount);

  this->mGraph->removeSeries(this->mPrimarySeries);
  this->mGraph->addSeries(this->mPrimarySeries);
  this->mGraph->seriesList().at(0)->setBaseGradient(this->mColorGradient);
  this->mGraph->seriesList().at(0)->setColorStyle(Q3DTheme::ColorStyleRangeGradient);

  // Reset range sliders
  this->mAxisMinSliderX->setMinimum(this->mSettings.mX3DRange.first);
  this->mAxisMinSliderX->setMaximum(this->mSettings.mX3DRange.second);

  this->mAxisMaxSliderX->setMinimum(this->mSettings.mX3DRange.first);
  this->mAxisMaxSliderX->setMaximum(this->mSettings.mX3DRange.second);

  this->mAxisMinSliderZ->setMinimum(this->mSettings.mY3DRange.first);
  this->mAxisMinSliderZ->setMaximum(this->mSettings.mY3DRange.second);

  this->mAxisMaxSliderZ->setMinimum(this->mSettings.mY3DRange.first);
  this->mAxisMaxSliderZ->setMaximum(this->mSettings.mY3DRange.second);

  this->blockSignals(true);
  this->mAxisMinSliderX->setValue(this->mSettings.mX3DRange.first);
  this->mAxisMaxSliderX->setValue(this->mSettings.mX3DRange.second);
  this->mAxisMinSliderZ->setValue(this->mSettings.mY3DRange.first);
  this->mAxisMaxSliderZ->setValue(this->mSettings.mY3DRange.second);
  this->blockSignals(false);
}

Surface3DGraphModifier::~Surface3DGraphModifier()
{
  delete this->mGraph;
}

void Surface3DGraphModifier::adjustXMin(int aMin)
{
  int max = this->mAxisMaxSliderX->value();
  if (aMin >= max)
  {
    max = aMin + 1;
    this->mAxisMaxSliderX->setValue(max);
    this->setAxisXRange(aMin, max);
  }
  else
    this->setAxisXRange(aMin, max);
}

void Surface3DGraphModifier::adjustXMax(int aMax)
{
  int min = this->mAxisMinSliderX->value();
  if (aMax <= min)
  {
    min = aMax - 1;
    this->mAxisMinSliderX->setValue(min);
    this->setAxisXRange(min, aMax);
  }
  else
    this->setAxisXRange(min, aMax);
}

void Surface3DGraphModifier::adjustZMin(int aMin)
{
  int max = this->mAxisMaxSliderZ->value();
  if (aMin >= max)
  {
    max = aMin + 1;
    this->mAxisMaxSliderZ->setValue(max);
    this->setAxisZRange(aMin, max);
  }
  else
    this->setAxisZRange(aMin, max);
}

void Surface3DGraphModifier::adjustZMax(int aMax)
{
  int min = this->mAxisMinSliderZ->value();
  if (aMax <= min)
  {
    min = aMax - 1;
    this->mAxisMinSliderZ->setValue(min);
    this->setAxisZRange(min, aMax);
  }
  else
    this->setAxisZRange(min, aMax);
}

void Surface3DGraphModifier::setAxisXRange(float aMin, float aMax)
{
  this->mAxisX->setRange(aMin, aMax);
}

void Surface3DGraphModifier::setAxisZRange(float aMin, float aMax)
{
  this->mAxisZ->setRange(aMin, aMax);
}
