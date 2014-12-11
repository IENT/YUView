/****************************************************************************
** Meta object code from reading C++ file 'renderwidget.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.4.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../renderwidget.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <QtCore/QVector>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'renderwidget.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.4.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_RenderWidget_t {
    QByteArrayData data[66];
    char stringdata[878];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_RenderWidget_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_RenderWidget_t qt_meta_stringdata_RenderWidget = {
    {
QT_MOC_LITERAL(0, 0, 12), // "RenderWidget"
QT_MOC_LITERAL(1, 13, 7), // "isFocus"
QT_MOC_LITERAL(2, 21, 0), // ""
QT_MOC_LITERAL(3, 22, 23), // "setCurrentRenderObjects"
QT_MOC_LITERAL(4, 46, 10), // "YUVObject*"
QT_MOC_LITERAL(5, 57, 13), // "newObjectLeft"
QT_MOC_LITERAL(6, 71, 14), // "newObjectRight"
QT_MOC_LITERAL(7, 86, 22), // "setCurrentDepthObjects"
QT_MOC_LITERAL(8, 109, 20), // "setCurrentStatistics"
QT_MOC_LITERAL(9, 130, 17), // "StatisticsParser*"
QT_MOC_LITERAL(10, 148, 5), // "stats"
QT_MOC_LITERAL(11, 154, 30), // "QVector<StatisticsRenderItem>&"
QT_MOC_LITERAL(12, 185, 11), // "renderTypes"
QT_MOC_LITERAL(13, 197, 9), // "drawFrame"
QT_MOC_LITERAL(14, 207, 8), // "frameIdx"
QT_MOC_LITERAL(15, 216, 22), // "drawSelectionRectangle"
QT_MOC_LITERAL(16, 239, 11), // "drawZoomBox"
QT_MOC_LITERAL(17, 251, 8), // "mousePos"
QT_MOC_LITERAL(18, 260, 14), // "drawStatistics"
QT_MOC_LITERAL(19, 275, 11), // "Statistics&"
QT_MOC_LITERAL(20, 287, 21), // "StatisticsRenderItem&"
QT_MOC_LITERAL(21, 309, 4), // "item"
QT_MOC_LITERAL(22, 314, 13), // "getRenderMode"
QT_MOC_LITERAL(23, 328, 13), // "setRenderMode"
QT_MOC_LITERAL(24, 342, 7), // "newMode"
QT_MOC_LITERAL(25, 350, 24), // "setViewInterpolationMode"
QT_MOC_LITERAL(26, 375, 24), // "getViewInterpolationMode"
QT_MOC_LITERAL(27, 400, 20), // "interpolation_mode_t"
QT_MOC_LITERAL(28, 421, 26), // "setViewInterpolationFactor"
QT_MOC_LITERAL(29, 448, 6), // "factor"
QT_MOC_LITERAL(30, 455, 14), // "setEyeDistance"
QT_MOC_LITERAL(31, 470, 4), // "dist"
QT_MOC_LITERAL(32, 475, 17), // "setGridParameters"
QT_MOC_LITERAL(33, 493, 4), // "show"
QT_MOC_LITERAL(34, 498, 4), // "size"
QT_MOC_LITERAL(35, 503, 16), // "unsigned char[4]"
QT_MOC_LITERAL(36, 520, 5), // "color"
QT_MOC_LITERAL(37, 526, 23), // "setStatisticsParameters"
QT_MOC_LITERAL(38, 550, 10), // "doSimplify"
QT_MOC_LITERAL(39, 561, 9), // "threshold"
QT_MOC_LITERAL(40, 571, 16), // "unsigned char[3]"
QT_MOC_LITERAL(41, 588, 5), // "clear"
QT_MOC_LITERAL(42, 594, 6), // "zoomIn"
QT_MOC_LITERAL(43, 601, 7), // "QPoint*"
QT_MOC_LITERAL(44, 609, 2), // "to"
QT_MOC_LITERAL(45, 612, 7), // "zoomOut"
QT_MOC_LITERAL(46, 620, 9), // "zoomToFit"
QT_MOC_LITERAL(47, 630, 14), // "zoomToStandard"
QT_MOC_LITERAL(48, 645, 13), // "setZoomFactor"
QT_MOC_LITERAL(49, 659, 10), // "zoomFactor"
QT_MOC_LITERAL(50, 670, 17), // "setZoomBoxEnabled"
QT_MOC_LITERAL(51, 688, 7), // "enabled"
QT_MOC_LITERAL(52, 696, 13), // "getRenderSize"
QT_MOC_LITERAL(53, 710, 4), // "int&"
QT_MOC_LITERAL(54, 715, 7), // "xOffset"
QT_MOC_LITERAL(55, 723, 7), // "yOffset"
QT_MOC_LITERAL(56, 731, 5), // "width"
QT_MOC_LITERAL(57, 737, 6), // "height"
QT_MOC_LITERAL(58, 744, 7), // "isReady"
QT_MOC_LITERAL(59, 752, 23), // "setCurrentMousePosition"
QT_MOC_LITERAL(60, 776, 23), // "setNeighborRenderWidget"
QT_MOC_LITERAL(61, 800, 13), // "RenderWidget*"
QT_MOC_LITERAL(62, 814, 13), // "pRenderWidget"
QT_MOC_LITERAL(63, 828, 23), // "getNeighborRenderWidget"
QT_MOC_LITERAL(64, 852, 15), // "currentRenderer"
QT_MOC_LITERAL(65, 868, 9) // "Renderer*"

    },
    "RenderWidget\0isFocus\0\0setCurrentRenderObjects\0"
    "YUVObject*\0newObjectLeft\0newObjectRight\0"
    "setCurrentDepthObjects\0setCurrentStatistics\0"
    "StatisticsParser*\0stats\0"
    "QVector<StatisticsRenderItem>&\0"
    "renderTypes\0drawFrame\0frameIdx\0"
    "drawSelectionRectangle\0drawZoomBox\0"
    "mousePos\0drawStatistics\0Statistics&\0"
    "StatisticsRenderItem&\0item\0getRenderMode\0"
    "setRenderMode\0newMode\0setViewInterpolationMode\0"
    "getViewInterpolationMode\0interpolation_mode_t\0"
    "setViewInterpolationFactor\0factor\0"
    "setEyeDistance\0dist\0setGridParameters\0"
    "show\0size\0unsigned char[4]\0color\0"
    "setStatisticsParameters\0doSimplify\0"
    "threshold\0unsigned char[3]\0clear\0"
    "zoomIn\0QPoint*\0to\0zoomOut\0zoomToFit\0"
    "zoomToStandard\0setZoomFactor\0zoomFactor\0"
    "setZoomBoxEnabled\0enabled\0getRenderSize\0"
    "int&\0xOffset\0yOffset\0width\0height\0"
    "isReady\0setCurrentMousePosition\0"
    "setNeighborRenderWidget\0RenderWidget*\0"
    "pRenderWidget\0getNeighborRenderWidget\0"
    "currentRenderer\0Renderer*"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_RenderWidget[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      33,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,  179,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       3,    2,  180,    2, 0x0a /* Public */,
       7,    2,  185,    2, 0x0a /* Public */,
       8,    2,  190,    2, 0x0a /* Public */,
      13,    1,  195,    2, 0x0a /* Public */,
      15,    0,  198,    2, 0x0a /* Public */,
      16,    1,  199,    2, 0x0a /* Public */,
      18,    2,  202,    2, 0x0a /* Public */,
      22,    0,  207,    2, 0x0a /* Public */,
      23,    1,  208,    2, 0x0a /* Public */,
      25,    1,  211,    2, 0x0a /* Public */,
      26,    0,  214,    2, 0x0a /* Public */,
      28,    1,  215,    2, 0x0a /* Public */,
      30,    1,  218,    2, 0x0a /* Public */,
      32,    3,  221,    2, 0x0a /* Public */,
      37,    3,  228,    2, 0x0a /* Public */,
      41,    0,  235,    2, 0x0a /* Public */,
      42,    2,  236,    2, 0x0a /* Public */,
      42,    1,  241,    2, 0x2a /* Public | MethodCloned */,
      42,    0,  244,    2, 0x2a /* Public | MethodCloned */,
      45,    2,  245,    2, 0x0a /* Public */,
      45,    1,  250,    2, 0x2a /* Public | MethodCloned */,
      45,    0,  253,    2, 0x2a /* Public | MethodCloned */,
      46,    0,  254,    2, 0x0a /* Public */,
      47,    0,  255,    2, 0x0a /* Public */,
      48,    1,  256,    2, 0x0a /* Public */,
      50,    1,  259,    2, 0x0a /* Public */,
      52,    4,  262,    2, 0x0a /* Public */,
      58,    0,  271,    2, 0x0a /* Public */,
      59,    1,  272,    2, 0x0a /* Public */,
      60,    1,  275,    2, 0x0a /* Public */,
      63,    0,  278,    2, 0x0a /* Public */,
      64,    0,  279,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 4, 0x80000000 | 4,    5,    6,
    QMetaType::Void, 0x80000000 | 4, 0x80000000 | 4,    5,    6,
    QMetaType::Void, 0x80000000 | 9, 0x80000000 | 11,   10,   12,
    QMetaType::Void, QMetaType::UInt,   14,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QPoint,   17,
    QMetaType::Void, 0x80000000 | 19, 0x80000000 | 20,   10,   21,
    QMetaType::Int,
    QMetaType::Void, QMetaType::Int,   24,
    QMetaType::Void, QMetaType::Int,   24,
    0x80000000 | 27,
    QMetaType::Void, QMetaType::Float,   29,
    QMetaType::Void, QMetaType::Double,   31,
    QMetaType::Void, QMetaType::Bool, QMetaType::Int, 0x80000000 | 35,   33,   34,   36,
    QMetaType::Void, QMetaType::Bool, QMetaType::Int, 0x80000000 | 40,   38,   39,   36,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 43, QMetaType::Float,   44,   29,
    QMetaType::Void, 0x80000000 | 43,   44,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 43, QMetaType::Float,   44,   29,
    QMetaType::Void, 0x80000000 | 43,   44,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Float,   49,
    QMetaType::Void, QMetaType::Bool,   51,
    QMetaType::Bool, 0x80000000 | 53, 0x80000000 | 53, 0x80000000 | 53, 0x80000000 | 53,   54,   55,   56,   57,
    QMetaType::Bool,
    QMetaType::Void, QMetaType::QPoint,   17,
    QMetaType::Void, 0x80000000 | 61,   62,
    0x80000000 | 61,
    0x80000000 | 65,

       0        // eod
};

void RenderWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        RenderWidget *_t = static_cast<RenderWidget *>(_o);
        switch (_id) {
        case 0: _t->isFocus(); break;
        case 1: _t->setCurrentRenderObjects((*reinterpret_cast< YUVObject*(*)>(_a[1])),(*reinterpret_cast< YUVObject*(*)>(_a[2]))); break;
        case 2: _t->setCurrentDepthObjects((*reinterpret_cast< YUVObject*(*)>(_a[1])),(*reinterpret_cast< YUVObject*(*)>(_a[2]))); break;
        case 3: _t->setCurrentStatistics((*reinterpret_cast< StatisticsParser*(*)>(_a[1])),(*reinterpret_cast< QVector<StatisticsRenderItem>(*)>(_a[2]))); break;
        case 4: _t->drawFrame((*reinterpret_cast< uint(*)>(_a[1]))); break;
        case 5: _t->drawSelectionRectangle(); break;
        case 6: _t->drawZoomBox((*reinterpret_cast< QPoint(*)>(_a[1]))); break;
        case 7: _t->drawStatistics((*reinterpret_cast< Statistics(*)>(_a[1])),(*reinterpret_cast< StatisticsRenderItem(*)>(_a[2]))); break;
        case 8: { int _r = _t->getRenderMode();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = _r; }  break;
        case 9: _t->setRenderMode((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 10: _t->setViewInterpolationMode((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 11: { interpolation_mode_t _r = _t->getViewInterpolationMode();
            if (_a[0]) *reinterpret_cast< interpolation_mode_t*>(_a[0]) = _r; }  break;
        case 12: _t->setViewInterpolationFactor((*reinterpret_cast< float(*)>(_a[1]))); break;
        case 13: _t->setEyeDistance((*reinterpret_cast< double(*)>(_a[1]))); break;
        case 14: _t->setGridParameters((*reinterpret_cast< bool(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< unsigned char(*)[4]>(_a[3]))); break;
        case 15: _t->setStatisticsParameters((*reinterpret_cast< bool(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< unsigned char(*)[3]>(_a[3]))); break;
        case 16: _t->clear(); break;
        case 17: _t->zoomIn((*reinterpret_cast< QPoint*(*)>(_a[1])),(*reinterpret_cast< float(*)>(_a[2]))); break;
        case 18: _t->zoomIn((*reinterpret_cast< QPoint*(*)>(_a[1]))); break;
        case 19: _t->zoomIn(); break;
        case 20: _t->zoomOut((*reinterpret_cast< QPoint*(*)>(_a[1])),(*reinterpret_cast< float(*)>(_a[2]))); break;
        case 21: _t->zoomOut((*reinterpret_cast< QPoint*(*)>(_a[1]))); break;
        case 22: _t->zoomOut(); break;
        case 23: _t->zoomToFit(); break;
        case 24: _t->zoomToStandard(); break;
        case 25: _t->setZoomFactor((*reinterpret_cast< float(*)>(_a[1]))); break;
        case 26: _t->setZoomBoxEnabled((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 27: { bool _r = _t->getRenderSize((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3])),(*reinterpret_cast< int(*)>(_a[4])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 28: { bool _r = _t->isReady();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 29: _t->setCurrentMousePosition((*reinterpret_cast< QPoint(*)>(_a[1]))); break;
        case 30: _t->setNeighborRenderWidget((*reinterpret_cast< RenderWidget*(*)>(_a[1]))); break;
        case 31: { RenderWidget* _r = _t->getNeighborRenderWidget();
            if (_a[0]) *reinterpret_cast< RenderWidget**>(_a[0]) = _r; }  break;
        case 32: { Renderer* _r = _t->currentRenderer();
            if (_a[0]) *reinterpret_cast< Renderer**>(_a[0]) = _r; }  break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 1:
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< YUVObject* >(); break;
            }
            break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 1:
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< YUVObject* >(); break;
            }
            break;
        case 30:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< RenderWidget* >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (RenderWidget::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&RenderWidget::isFocus)) {
                *result = 0;
            }
        }
    }
}

const QMetaObject RenderWidget::staticMetaObject = {
    { &QGLWidget::staticMetaObject, qt_meta_stringdata_RenderWidget.data,
      qt_meta_data_RenderWidget,  qt_static_metacall, 0, 0}
};


const QMetaObject *RenderWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *RenderWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_RenderWidget.stringdata))
        return static_cast<void*>(const_cast< RenderWidget*>(this));
    return QGLWidget::qt_metacast(_clname);
}

int RenderWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QGLWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 33)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 33;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 33)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 33;
    }
    return _id;
}

// SIGNAL 0
void RenderWidget::isFocus()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}
QT_END_MOC_NAMESPACE
