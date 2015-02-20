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

    virtual QString name() { return p_name; }

    virtual int width() { return p_width; }
    virtual int height() { return p_height; }

    virtual int startFrame() { return p_startFrame; }
    virtual int numFrames() { return p_numFrames; }
    virtual float frameRate() { return p_frameRate; }
    virtual int sampling() { return p_sampling; }

    virtual bool playUntilEnd() { return p_playUntilEnd; }

    virtual ColorFormat colorFormat() { return p_colorFormat; }
    virtual int bitPerPixel() { return p_bitPerPixel; }

    virtual int getPixelValue(int x, int y) { return 0; }

signals:

    void informationChanged();

public slots:

    void setName(QString& newName) { p_name = newName; }

    void setWidth(int newWidth) { p_width = newWidth; emit informationChanged(); }
    void setHeight(int newHeight) { p_height = newHeight; emit informationChanged(); }

    void setFrameRate(double newRate) { p_frameRate = newRate; emit informationChanged(); }
    void setNumFrames(int newNumFrames) { p_numFrames = newNumFrames; emit informationChanged(); }
    void setStartFrame(int newStartFrame) { p_startFrame = newStartFrame; emit informationChanged(); }
    void setSampling(int newSampling) { p_sampling = newSampling; emit informationChanged(); }

    void setPlayUntilEnd(bool play) { p_playUntilEnd = play; emit informationChanged(); }

    void setColorFormat(int newFormat) { p_colorFormat = (ColorFormat)newFormat; emit informationChanged(); }

    void setBitPerPixel(int bitPerPixel) { p_bitPerPixel = bitPerPixel; emit informationChanged(); }

    virtual void setInterpolationMode(int newMode) {}

protected:
    QImage p_displayImage;

    QString p_name;

    int p_width;
    int p_height;

    int p_numFrames;
    int p_startFrame;
    double p_frameRate;
    bool p_playUntilEnd;
    int p_sampling;

    ColorFormat p_colorFormat;
    int p_bitPerPixel;
};

#endif // DISPLAYOBJECT_H
