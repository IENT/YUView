#ifndef RENDERWIDGET_H
#define RENDERWIDGET_H

#include <QMouseEvent>
#include "yuvobject.h"
#include "renderer.h"
#include "viewinterpolator.h"
#include "nointerpolator.h"
#include "fullinterpolator.h"
#include "anaglyphrenderer.h"
#include "defaultrenderer.h"
#include "shutterrenderer.h"
#include "leftrightlinerenderer.h"
#include "statisticsparser.h"

enum render_mode_t {
    normalRenderMode = 0,
    shutterRenderMode,
    anaglyphRenderMode,
    leftrightlineRenderMode,
    unspecifiedRenderMode
};

enum interpolation_mode_t {
    noInterpolation = 0,
    fullInterplation,
    unspecifiedInterpolation
};

class RenderWidget : public QGLWidget
{
    Q_OBJECT
public:
    RenderWidget(QWidget *parent);
    ~RenderWidget();

    void updateTex();

    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

signals:
    void isFocus();

public slots:

    void setCurrentRenderObjects( YUVObject* newObjectLeft, YUVObject* newObjectRight );
    void setCurrentDepthObjects( YUVObject* newObjectLeft, YUVObject* newObjectRight );
    void setCurrentStatistics(StatisticsParser* stats, QVector<StatisticsRenderItem> &renderTypes);
    void drawFrame(unsigned int frameIdx);

    void drawSelectionRectangle();
    void drawZoomBox(QPoint mousePos);
    void drawStatistics(Statistics& stats, StatisticsRenderItem &item);

    int getRenderMode();
    void setRenderMode(int newMode);

    void setViewInterpolationMode(int newMode);
    interpolation_mode_t getViewInterpolationMode() { return p_viewInterpolationMode; }
    void setViewInterpolationFactor(float factor);
    void setEyeDistance(double dist);
    void setGridParameters(bool show, int size, unsigned char color[4]);
    void setStatisticsParameters(bool doSimplify, int threshold, unsigned char color[3]);

    void clear();

    void zoomIn(QPoint *to=0, float factor=1.2);
    void zoomOut(QPoint *to=0, float factor=1.2);
    void zoomToFit();
    void zoomToStandard();
    void setZoomFactor(float zoomFactor) { p_zoomFactor = zoomFactor; }
    void setZoomBoxEnabled(bool enabled);

    bool getRenderSize(int &xOffset, int &yOffset, int &width, int &height);

    bool isReady() { return ( p_gl_initialized && p_currentRenderer != NULL && p_currentRenderObjectLeft != NULL ); }

    void setCurrentMousePosition(QPoint mousePos) { p_currentMousePosition = mousePos; }

    void setNeighborRenderWidget(RenderWidget* pRenderWidget) {p_RenderWidget2 = pRenderWidget;}
    RenderWidget* getNeighborRenderWidget() {return p_RenderWidget2;}

    Renderer* currentRenderer() { return p_currentRenderer; }


protected:
     void initializeGL();
     void paintGL();
     void resizeGL(int width, int height);

     virtual void mousePressEvent(QMouseEvent *e);
     virtual void mouseMoveEvent(QMouseEvent *e);
     virtual void mouseReleaseEvent(QMouseEvent *e);
     virtual void wheelEvent (QWheelEvent *e);

private:
     DefaultRenderer* p_defaultRenderer;

     void drawFrame();
     void drawStatisticsOverlay();

     void drawStatistics(Statistics &stats, StatisticsRenderItem &item) const;
     void drawSelectionRectangle() const;
     void rotateVector(float angle, float x, float y, float &nx, float &ny) const;

     YUVObject *p_currentRenderObjectLeft;
     YUVObject *p_currentRenderObjectRight;
     YUVObject *p_currentDepthObjectLeft;
     YUVObject *p_currentDepthObjectRight;

     float p_videoHeight;
     float p_videoWidth;

     float p_zoomFactor;
     float p_boxZoomFactor;

     float p_interpolationFactor;
     double p_eyeDistance;

     bool p_drawGrid;
     unsigned char p_gridColor[4];
     int p_gridSize;

     bool p_simplifyStats;
     int p_simplificationTheshold;
     unsigned char p_simplifiedGridColor[3];
     StatisticsParser *p_currentStatsParser;
     QVector<StatisticsRenderItem> p_renderStatsTypes; // contains all type-IDs of stats that should be rendered (in order)

     interpolation_mode_t p_viewInterpolationMode;

     render_mode_t p_renderMode;

     int p_currentFrameIdx;

     // Current rectangular selection
     QPoint p_selectionStartPoint;
     QPoint p_selectionEndPoint;
     QPoint p_currentMousePosition;
     QRect p_selectionRect;
     int p_dragRenderOffsetStartX, p_dragRenderOffsetStartY;

     // Different selection modes
     enum SelectionMode { NONE, SELECT, DRAG };
     SelectionMode selectionMode_;
     bool p_zoomBoxEnabled;
     bool p_customViewport;

     Renderer* p_currentRenderer;

     NoInterpolator* p_noInterpolator;
     FullInterpolator* p_fullInterpolator;
     ViewInterpolator* p_currentViewInterpolator;

     GLuint p_renderTextures[2];

     bool p_gl_initialized;
     GLint p_maxViewportDims[2];
     RenderWidget* p_RenderWidget2;
};

#endif // RENDERWIDGET_H
