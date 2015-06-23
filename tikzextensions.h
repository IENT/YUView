#ifndef TIKZEXTENSIONS_H
#define TIKZEXTENSIONS_H

#include <QColor>

enum arrowtype_t
{
    none=0,
    stels
};


struct TikzDrawSettings{
    bool snapToGrid;
    bool setBW;
    bool plainText;
};

struct TikzDrawLayerSettings{
    QColor maxColor;
    QColor minColor;
    QColor gridColor;
    QColor vectorColor;
    arrowtype_t arrowType;
    int lineWidth;

};

#endif // TIKZEXTENSIONS_H
