/****************************************************************************
** Meta object code from reading C++ file 'settingswindow.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.4.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../settingswindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'settingswindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.4.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_SettingsWindow_t {
    QByteArrayData data[10];
    char stringdata[209];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_SettingsWindow_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_SettingsWindow_t qt_meta_stringdata_SettingsWindow = {
    {
QT_MOC_LITERAL(0, 0, 14), // "SettingsWindow"
QT_MOC_LITERAL(1, 15, 15), // "settingsChanged"
QT_MOC_LITERAL(2, 31, 0), // ""
QT_MOC_LITERAL(3, 32, 21), // "on_saveButton_clicked"
QT_MOC_LITERAL(4, 54, 36), // "on_cacheThresholdSlider_value..."
QT_MOC_LITERAL(5, 91, 5), // "value"
QT_MOC_LITERAL(6, 97, 29), // "on_cacheCheckBox_stateChanged"
QT_MOC_LITERAL(7, 127, 26), // "on_gridColorButton_clicked"
QT_MOC_LITERAL(8, 154, 23), // "on_cancelButton_clicked"
QT_MOC_LITERAL(9, 178, 30) // "on_simplifyColorButton_clicked"

    },
    "SettingsWindow\0settingsChanged\0\0"
    "on_saveButton_clicked\0"
    "on_cacheThresholdSlider_valueChanged\0"
    "value\0on_cacheCheckBox_stateChanged\0"
    "on_gridColorButton_clicked\0"
    "on_cancelButton_clicked\0"
    "on_simplifyColorButton_clicked"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_SettingsWindow[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   49,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       3,    0,   50,    2, 0x08 /* Private */,
       4,    1,   51,    2, 0x08 /* Private */,
       6,    1,   54,    2, 0x08 /* Private */,
       7,    0,   57,    2, 0x08 /* Private */,
       8,    0,   58,    2, 0x08 /* Private */,
       9,    0,   59,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, QMetaType::Int,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void SettingsWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        SettingsWindow *_t = static_cast<SettingsWindow *>(_o);
        switch (_id) {
        case 0: _t->settingsChanged(); break;
        case 1: _t->on_saveButton_clicked(); break;
        case 2: _t->on_cacheThresholdSlider_valueChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 3: _t->on_cacheCheckBox_stateChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 4: _t->on_gridColorButton_clicked(); break;
        case 5: _t->on_cancelButton_clicked(); break;
        case 6: _t->on_simplifyColorButton_clicked(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (SettingsWindow::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&SettingsWindow::settingsChanged)) {
                *result = 0;
            }
        }
    }
}

const QMetaObject SettingsWindow::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_SettingsWindow.data,
      qt_meta_data_SettingsWindow,  qt_static_metacall, 0, 0}
};


const QMetaObject *SettingsWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SettingsWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_SettingsWindow.stringdata))
        return static_cast<void*>(const_cast< SettingsWindow*>(this));
    return QWidget::qt_metacast(_clname);
}

int SettingsWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void SettingsWindow::settingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}
QT_END_MOC_NAMESPACE
