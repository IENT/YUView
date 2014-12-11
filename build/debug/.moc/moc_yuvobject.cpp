/****************************************************************************
** Meta object code from reading C++ file 'yuvobject.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.4.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../yuvobject.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'yuvobject.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.4.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_YUVObject_t {
    QByteArrayData data[26];
    char stringdata[294];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_YUVObject_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_YUVObject_t qt_meta_stringdata_YUVObject = {
    {
QT_MOC_LITERAL(0, 0, 9), // "YUVObject"
QT_MOC_LITERAL(1, 10, 18), // "informationChanged"
QT_MOC_LITERAL(2, 29, 0), // ""
QT_MOC_LITERAL(3, 30, 8), // "setWidth"
QT_MOC_LITERAL(4, 39, 8), // "newWidth"
QT_MOC_LITERAL(5, 48, 9), // "setHeight"
QT_MOC_LITERAL(6, 58, 9), // "newHeight"
QT_MOC_LITERAL(7, 68, 14), // "setColorFormat"
QT_MOC_LITERAL(8, 83, 9), // "newFormat"
QT_MOC_LITERAL(9, 93, 12), // "setFrameRate"
QT_MOC_LITERAL(10, 106, 7), // "newRate"
QT_MOC_LITERAL(11, 114, 12), // "setNumFrames"
QT_MOC_LITERAL(12, 127, 12), // "newNumFrames"
QT_MOC_LITERAL(13, 140, 7), // "setName"
QT_MOC_LITERAL(14, 148, 8), // "QString&"
QT_MOC_LITERAL(15, 157, 7), // "newName"
QT_MOC_LITERAL(16, 165, 13), // "setStartFrame"
QT_MOC_LITERAL(17, 179, 13), // "newStartFrame"
QT_MOC_LITERAL(18, 193, 11), // "setSampling"
QT_MOC_LITERAL(19, 205, 11), // "newSampling"
QT_MOC_LITERAL(20, 217, 14), // "setBitPerPixel"
QT_MOC_LITERAL(21, 232, 11), // "bitPerPixel"
QT_MOC_LITERAL(22, 244, 20), // "setInterpolationMode"
QT_MOC_LITERAL(23, 265, 7), // "newMode"
QT_MOC_LITERAL(24, 273, 15), // "setPlayUntilEnd"
QT_MOC_LITERAL(25, 289, 4) // "play"

    },
    "YUVObject\0informationChanged\0\0setWidth\0"
    "newWidth\0setHeight\0newHeight\0"
    "setColorFormat\0newFormat\0setFrameRate\0"
    "newRate\0setNumFrames\0newNumFrames\0"
    "setName\0QString&\0newName\0setStartFrame\0"
    "newStartFrame\0setSampling\0newSampling\0"
    "setBitPerPixel\0bitPerPixel\0"
    "setInterpolationMode\0newMode\0"
    "setPlayUntilEnd\0play"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_YUVObject[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   74,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       3,    1,   75,    2, 0x0a /* Public */,
       5,    1,   78,    2, 0x0a /* Public */,
       7,    1,   81,    2, 0x0a /* Public */,
       9,    1,   84,    2, 0x0a /* Public */,
      11,    1,   87,    2, 0x0a /* Public */,
      13,    1,   90,    2, 0x0a /* Public */,
      16,    1,   93,    2, 0x0a /* Public */,
      18,    1,   96,    2, 0x0a /* Public */,
      20,    1,   99,    2, 0x0a /* Public */,
      22,    1,  102,    2, 0x0a /* Public */,
      24,    1,  105,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void, QMetaType::Int,    6,
    QMetaType::Void, QMetaType::Int,    8,
    QMetaType::Void, QMetaType::Double,   10,
    QMetaType::Void, QMetaType::Int,   12,
    QMetaType::Void, 0x80000000 | 14,   15,
    QMetaType::Void, QMetaType::Int,   17,
    QMetaType::Void, QMetaType::Int,   19,
    QMetaType::Void, QMetaType::Int,   21,
    QMetaType::Void, QMetaType::Int,   23,
    QMetaType::Void, QMetaType::Bool,   25,

       0        // eod
};

void YUVObject::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        YUVObject *_t = static_cast<YUVObject *>(_o);
        switch (_id) {
        case 0: _t->informationChanged(); break;
        case 1: _t->setWidth((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->setHeight((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 3: _t->setColorFormat((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 4: _t->setFrameRate((*reinterpret_cast< double(*)>(_a[1]))); break;
        case 5: _t->setNumFrames((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 6: _t->setName((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 7: _t->setStartFrame((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 8: _t->setSampling((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 9: _t->setBitPerPixel((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 10: _t->setInterpolationMode((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 11: _t->setPlayUntilEnd((*reinterpret_cast< bool(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (YUVObject::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&YUVObject::informationChanged)) {
                *result = 0;
            }
        }
    }
}

const QMetaObject YUVObject::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_YUVObject.data,
      qt_meta_data_YUVObject,  qt_static_metacall, 0, 0}
};


const QMetaObject *YUVObject::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *YUVObject::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_YUVObject.stringdata))
        return static_cast<void*>(const_cast< YUVObject*>(this));
    return QObject::qt_metacast(_clname);
}

int YUVObject::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void YUVObject::informationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}
QT_END_MOC_NAMESPACE
