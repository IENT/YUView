#ifndef GRAPHMODIFIER_H
#define GRAPHMODIFIER_H

#include "chartHandlerTypedef.h"

#include <QtDataVisualization/q3dbars.h>
#include <QtDataVisualization/qbardataproxy.h>
#include <QtDataVisualization/qabstract3dseries.h>

#include <QtDataVisualization/Q3DSurface>
#include <QtDataVisualization/QSurfaceDataProxy>
#include <QtDataVisualization/QHeightMapSurfaceDataProxy>
#include <QtDataVisualization/QSurface3DSeries>

#include <QtGui/QFont>
#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtCore/QPointer>
#include <QtCore/QPropertyAnimation>

using namespace QtDataVisualization;

class Modifier : public QObject
{
  Q_OBJECT
public:
  /**
   * @brief applyDataToGraph
   * sets the data to the graph
   */
  virtual void applyDataToGraph(chartSettingsData aSettings) = 0;
};


/**
 * @brief The GraphModifier3DBars class
 * the GraphModifier3DBars-Class will modify the given 3D-chart. It can modify the displaying and the dataset
 */
class GraphModifier3DBars : public Modifier
{
  Q_OBJECT
public:
  /**
   * @brief GraphModifier3DBars
   * default-constructor
   *
   * @param bargraph
   * chart
   *
   * @param aSettings
   * necessary settings to set the data
   */
  explicit GraphModifier3DBars(Q3DBars* aBarGraph);

  /**
  * @brief
  * default-destructor
  */
  ~GraphModifier3DBars();

  /**
   * @brief applyDataToGraph
   * sets the data to the graph
   */
  void applyDataToGraph(chartSettingsData aSettings) Q_DECL_OVERRIDE;

  /**
   * @brief changePresetCamera
   * changes the camera settings
   */
  void changePresetCamera();

public slots:
  /**
   * @brief rotateX
   * slot for slider to rotate the x-axis
   *
   * @param aRotation
   * value to rotate
   */
  void rotateX(int aRotation);

  /**
   * @brief rotateY
   * slot for slider to rotate the y-axis
   *
   * @param aRotation
   * value to rotate
   */
  void rotateY(int aRotation);

  /**
   * @brief changeLabelRotation
   * slot for slider to rotate the label-rotation
   *
   * @param aRotation
   * value to rotate
   */
  void changeLabelRotation(int aRotation);

  /**
   * @brief zoomToSelectedBar
   * slot for QPushButton to zoom into an selected bar
   * if no bar is selected, an default-angle will display
   */
  void zoomToSelectedBar();

private:
    Q3DBars* mGraph;
    QBar3DSeries* mPrimarySeries;
    chartSettingsData mSettings;
    float mRotationX;
    float mRotationY;
    float mMinval;
    float mMaxVal;
    QStringList mCategoriesY;
    QStringList mCategoriesX;
    QValue3DAxis* mAmountAxis;
    QCategory3DAxis* mXValueAxis;
    QCategory3DAxis* mYValueAxis;
    QPropertyAnimation mAnimationCameraX;
    QPropertyAnimation mAnimationCameraY;
    QPropertyAnimation mAnimationCameraZoom;
    QPropertyAnimation mAnimationCameraTarget;
    float mDefaultAngleX;
    float mDefaultAngleY;
    float mDefaultZoom;
    QVector3D mDefaultTarget;
};

/**
 * @brief The Surface3DGraphModifier class
 * the Surface3DGraphModifier-Class will modify the given 3D-chart. It can modify the displaying and the dataset
 */
class Surface3DGraphModifier : public Modifier
{
    Q_OBJECT
public:
  /**
   * @brief Surface3DGraphModifier
   * default-constructor
   *
   * @param aSurfaceGraph
   * graph to handle
   */
  explicit Surface3DGraphModifier(Q3DSurface* aSurfaceGraph);

  /**
   * @brief applyDataToGraph
   * sets the data to the graph
   */
  void applyDataToGraph(chartSettingsData aSettings) Q_DECL_OVERRIDE;

  /**
   * @brief ~Surface3DGraphModifier
   * default-destructor
   *
   */
  ~Surface3DGraphModifier();

  /**
   * @brief setAxisMinSliderX
   * set the min slider for the x-axis
   *
   * @param aSlider
   * min slider x-axis
   */
  void setAxisMinSliderX(QSlider* aSlider) { this->mAxisMinSliderX = aSlider; }

  /**
   * @brief setAxisMaxSliderX
   * set the max slider for the x-axis
   *
   * @param aSlider
   * max slider x-axis
   */
  void setAxisMaxSliderX(QSlider* aSlider) { this->mAxisMaxSliderX = aSlider; }

  /**
   * @brief setAxisMinSliderZ
   * set the min slider for the z-axis
   *
   * @param aSlider
   * min slider z-axis
   */
  void setAxisMinSliderZ(QSlider* aSlider) { this->mAxisMinSliderZ = aSlider; }

  /**
   * @brief setAxisMaxSliderZ
   * set the max slider for the z-axis
   *
   * @param aSlider
   * max slider z-axis
   */
  void setAxisMaxSliderZ(QSlider* aSlider) { this->mAxisMaxSliderZ = aSlider; }

public slots:
  /**
   * @brief toggleModeNone
   * if QRadioButton toggle to mode: select none
   */
  void toggleModeNone() { this->mGraph->setSelectionMode(QAbstract3DGraph::SelectionNone); }

  /**
   * @brief toggleModeItem
   * if QRadioButton toggle to mode: select Item
   */
  void toggleModeItem() { this->mGraph->setSelectionMode(QAbstract3DGraph::SelectionItem); }

  /**
   * @brief toggleModeSliceRow
   * if QRadioButton toggle to mode: select Item and Row
   */
  void toggleModeSliceRow() { this->mGraph->setSelectionMode(QAbstract3DGraph::SelectionItemAndRow | QAbstract3DGraph::SelectionSlice); }

  /**
   * @brief toggleModeSliceRow
   * if QRadioButton toggle to mode: select Item and column
   */
  void toggleModeSliceColumn() { this->mGraph->setSelectionMode(QAbstract3DGraph::SelectionItemAndColumn | QAbstract3DGraph::SelectionSlice); }

  /**
   * @brief adjustXMin
   * check the new range for the x-min
   *
   * @param aMin
   * minium of new range
   */
  void adjustXMin(int aMin);

  /**
   * @brief adjustXMax
   * check the new range for the x-max
   *
   * @param aMax
   * maximum of new range
   */
  void adjustXMax(int aMax);

  /**
   * @brief adjustZMin
   * chceck the new range for the z-min
   *
   * @param aMin
   * minimum of new range
   */
  void adjustZMin(int aMin);

  /**
   * @brief adjustZMax
   * check the new range for the z-max
   *
   * @param aMax
   * maximum of new range
   */
  void adjustZMax(int aMax);

private:
  Q3DSurface* mGraph;
  QSurface3DSeries* mPrimarySeries;
  QSurfaceDataProxy* mPrimarySeriesProxy;
  chartSettingsData mSettings;
  QLinearGradient mColorGradient;

  QValue3DAxis* mAxisX;
  QValue3DAxis* mAxisY;
  QValue3DAxis* mAxisZ;

  QSlider* mAxisMinSliderX;
  QSlider* mAxisMaxSliderX;
  QSlider* mAxisMinSliderZ;
  QSlider* mAxisMaxSliderZ;

  /**
   * @brief setAxisXRange
   * set a new range to the x-axis
   *
   * @param aMin
   * minimum of range
   *
   * @param aMax
   * maximum of range
   */
  void setAxisXRange(float aMin, float aMax);

  /**
   * @brief setAxisZRange
   * set a new range to the z-axis
   *
   * @param aMin
   * minimum of range
   *
   * @param aMax
   * maximum of range
   */
  void setAxisZRange(float aMin, float aMax);
};

#endif // GRAPHMODIFIER_H
