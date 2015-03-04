#ifndef DISPLAYOBJECT_H
#define DISPLAYOBJECT_H

#include <QObject>
#include <QImage>
#include "typedef.h"

class DisplayObject : public QObject
{
    Q_OBJECT
public:
    explicit DisplayObject(QObject *parent = 0);
    ~DisplayObject();

    virtual void loadImage(unsigned int idx) = 0;       // needs to be implemented by subclasses
    QImage displayImage() { return p_displayImage; }

    QString name() { return p_name; }

    int width() { return p_width; }
    int height() { return p_height; }

    int startFrame() { return p_startFrame; }
    int numFrames() { return p_numFrames; }
    float frameRate() { return p_frameRate; }
    int sampling() { return p_sampling; }
    bool playUntilEnd() { return p_playUntilEnd; }

    virtual int getPixelValue(int x, int y) = 0; // needs to be implemented by subclass

signals:

    void informationChanged();

public slots:

    void setName(QString& newName) { p_name = newName; emit informationChanged(); }

    virtual void setWidth(int newWidth) { p_width = newWidth; emit informationChanged(); }
    virtual void setHeight(int newHeight) { p_height = newHeight; emit informationChanged(); }

    virtual void setFrameRate(double newRate) { p_frameRate = newRate; emit informationChanged(); }
    virtual void setNumFrames(int newNumFrames) { p_numFrames = newNumFrames; emit informationChanged(); }
    virtual void setStartFrame(int newStartFrame) { p_startFrame = newStartFrame; emit informationChanged(); }
    virtual void setSampling(int newSampling) { p_sampling = newSampling; emit informationChanged(); }
    virtual void setPlayUntilEnd(bool play) { p_playUntilEnd = play; emit informationChanged(); }

    virtual void refreshDisplayImage() = 0;

protected:
    QImage p_displayImage;
    unsigned int p_lastIdx;

    QString p_name;

    int p_width;
    int p_height;

    // timing related member variables
    int p_numFrames;
    int p_startFrame;
    double p_frameRate;
    bool p_playUntilEnd;
    int p_sampling;
};

#endif // DISPLAYOBJECT_H
