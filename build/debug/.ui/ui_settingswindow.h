/********************************************************************************
** Form generated from reading UI file 'settingswindow.ui'
**
** Created by: Qt User Interface Compiler version 5.4.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SETTINGSWINDOW_H
#define UI_SETTINGSWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SettingsWindow
{
public:
    QVBoxLayout *verticalLayout;
    QTabWidget *tabWidget;
    QWidget *general;
    QVBoxLayout *verticalLayout_2;
    QGroupBox *groupBox;
    QGridLayout *gridLayout;
    QCheckBox *cacheCheckBox;
    QSlider *cacheThresholdSlider;
    QCheckBox *cacheThresholdCheckBox;
    QLabel *cacheThresholdLabel;
    QGroupBox *groupBox_2;
    QPushButton *gridColorButton;
    QGroupBox *groupBox_4;
    QGridLayout *gridLayout_2;
    QLabel *label_3;
    QSpinBox *simplifySizeSpinBox;
    QLabel *label_2;
    QPushButton *simplifyColorButton;
    QCheckBox *simplifyCheckBox;
    QWidget *synthesis;
    QVBoxLayout *verticalLayout_3;
    QGroupBox *groupBox_3;
    QFormLayout *formLayout;
    QComboBox *blendingModeComboBox;
    QLabel *label;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer;
    QPushButton *cancelButton;
    QPushButton *saveButton;

    void setupUi(QWidget *SettingsWindow)
    {
        if (SettingsWindow->objectName().isEmpty())
            SettingsWindow->setObjectName(QStringLiteral("SettingsWindow"));
        SettingsWindow->setWindowModality(Qt::WindowModal);
        SettingsWindow->setEnabled(true);
        SettingsWindow->resize(579, 411);
        QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(SettingsWindow->sizePolicy().hasHeightForWidth());
        SettingsWindow->setSizePolicy(sizePolicy);
        verticalLayout = new QVBoxLayout(SettingsWindow);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        tabWidget = new QTabWidget(SettingsWindow);
        tabWidget->setObjectName(QStringLiteral("tabWidget"));
        general = new QWidget();
        general->setObjectName(QStringLiteral("general"));
        verticalLayout_2 = new QVBoxLayout(general);
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
        groupBox = new QGroupBox(general);
        groupBox->setObjectName(QStringLiteral("groupBox"));
        groupBox->setMinimumSize(QSize(450, 0));
        gridLayout = new QGridLayout(groupBox);
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        gridLayout->setSizeConstraint(QLayout::SetDefaultConstraint);
        cacheCheckBox = new QCheckBox(groupBox);
        cacheCheckBox->setObjectName(QStringLiteral("cacheCheckBox"));

        gridLayout->addWidget(cacheCheckBox, 0, 0, 1, 1);

        cacheThresholdSlider = new QSlider(groupBox);
        cacheThresholdSlider->setObjectName(QStringLiteral("cacheThresholdSlider"));
        cacheThresholdSlider->setEnabled(false);
        cacheThresholdSlider->setOrientation(Qt::Horizontal);
        cacheThresholdSlider->setInvertedAppearance(false);

        gridLayout->addWidget(cacheThresholdSlider, 1, 2, 1, 1);

        cacheThresholdCheckBox = new QCheckBox(groupBox);
        cacheThresholdCheckBox->setObjectName(QStringLiteral("cacheThresholdCheckBox"));
        cacheThresholdCheckBox->setEnabled(false);

        gridLayout->addWidget(cacheThresholdCheckBox, 1, 0, 1, 1);

        cacheThresholdLabel = new QLabel(groupBox);
        cacheThresholdLabel->setObjectName(QStringLiteral("cacheThresholdLabel"));
        cacheThresholdLabel->setEnabled(false);
        cacheThresholdLabel->setMinimumSize(QSize(120, 0));

        gridLayout->addWidget(cacheThresholdLabel, 1, 1, 1, 1);


        verticalLayout_2->addWidget(groupBox);

        groupBox_2 = new QGroupBox(general);
        groupBox_2->setObjectName(QStringLiteral("groupBox_2"));
        groupBox_2->setMinimumSize(QSize(0, 80));
        gridColorButton = new QPushButton(groupBox_2);
        gridColorButton->setObjectName(QStringLiteral("gridColorButton"));
        gridColorButton->setGeometry(QRect(20, 30, 171, 31));
        gridColorButton->setAutoFillBackground(false);
        gridColorButton->setFlat(false);

        verticalLayout_2->addWidget(groupBox_2);

        groupBox_4 = new QGroupBox(general);
        groupBox_4->setObjectName(QStringLiteral("groupBox_4"));
        gridLayout_2 = new QGridLayout(groupBox_4);
        gridLayout_2->setObjectName(QStringLiteral("gridLayout_2"));
        label_3 = new QLabel(groupBox_4);
        label_3->setObjectName(QStringLiteral("label_3"));
        label_3->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_2->addWidget(label_3, 1, 1, 1, 1);

        simplifySizeSpinBox = new QSpinBox(groupBox_4);
        simplifySizeSpinBox->setObjectName(QStringLiteral("simplifySizeSpinBox"));
        simplifySizeSpinBox->setMinimum(0);
        simplifySizeSpinBox->setValue(10);

        gridLayout_2->addWidget(simplifySizeSpinBox, 1, 2, 1, 1);

        label_2 = new QLabel(groupBox_4);
        label_2->setObjectName(QStringLiteral("label_2"));
        label_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        label_2->setWordWrap(true);

        gridLayout_2->addWidget(label_2, 0, 1, 1, 1);

        simplifyColorButton = new QPushButton(groupBox_4);
        simplifyColorButton->setObjectName(QStringLiteral("simplifyColorButton"));

        gridLayout_2->addWidget(simplifyColorButton, 0, 2, 1, 1);

        simplifyCheckBox = new QCheckBox(groupBox_4);
        simplifyCheckBox->setObjectName(QStringLiteral("simplifyCheckBox"));

        gridLayout_2->addWidget(simplifyCheckBox, 0, 0, 1, 1);


        verticalLayout_2->addWidget(groupBox_4);

        tabWidget->addTab(general, QString());
        synthesis = new QWidget();
        synthesis->setObjectName(QStringLiteral("synthesis"));
        verticalLayout_3 = new QVBoxLayout(synthesis);
        verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));
        groupBox_3 = new QGroupBox(synthesis);
        groupBox_3->setObjectName(QStringLiteral("groupBox_3"));
        formLayout = new QFormLayout(groupBox_3);
        formLayout->setObjectName(QStringLiteral("formLayout"));
        formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        blendingModeComboBox = new QComboBox(groupBox_3);
        blendingModeComboBox->setObjectName(QStringLiteral("blendingModeComboBox"));

        formLayout->setWidget(0, QFormLayout::FieldRole, blendingModeComboBox);

        label = new QLabel(groupBox_3);
        label->setObjectName(QStringLiteral("label"));

        formLayout->setWidget(0, QFormLayout::LabelRole, label);


        verticalLayout_3->addWidget(groupBox_3);

        tabWidget->addTab(synthesis, QString());

        verticalLayout->addWidget(tabWidget);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        cancelButton = new QPushButton(SettingsWindow);
        cancelButton->setObjectName(QStringLiteral("cancelButton"));

        horizontalLayout->addWidget(cancelButton);

        saveButton = new QPushButton(SettingsWindow);
        saveButton->setObjectName(QStringLiteral("saveButton"));
        saveButton->setAutoDefault(false);
        saveButton->setDefault(true);

        horizontalLayout->addWidget(saveButton);


        verticalLayout->addLayout(horizontalLayout);


        retranslateUi(SettingsWindow);
        QObject::connect(cacheThresholdCheckBox, SIGNAL(stateChanged(int)), SettingsWindow, SLOT(on_cacheCheckBox_stateChanged(int)));

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(SettingsWindow);
    } // setupUi

    void retranslateUi(QWidget *SettingsWindow)
    {
        SettingsWindow->setWindowTitle(QApplication::translate("SettingsWindow", "Form", 0));
        groupBox->setTitle(QApplication::translate("SettingsWindow", "Caching of video data", 0));
        cacheCheckBox->setText(QApplication::translate("SettingsWindow", "Use video cache", 0));
        cacheThresholdCheckBox->setText(QApplication::translate("SettingsWindow", "Use System RAM threshold: ", 0));
        cacheThresholdLabel->setText(QApplication::translate("SettingsWindow", "100% / 256 MB", 0));
        groupBox_2->setTitle(QApplication::translate("SettingsWindow", "Overlay Grid", 0));
        gridColorButton->setText(QApplication::translate("SettingsWindow", "Select Color", 0));
        groupBox_4->setTitle(QApplication::translate("SettingsWindow", "Statistics", 0));
        label_3->setText(QApplication::translate("SettingsWindow", "Simplify for blocks smaller than:", 0));
        simplifySizeSpinBox->setSuffix(QApplication::translate("SettingsWindow", " px", 0));
        label_2->setText(QApplication::translate("SettingsWindow", "Simplification will be indicated\n"
" by alternative grid color:", 0));
        simplifyColorButton->setText(QApplication::translate("SettingsWindow", "Select Color", 0));
        simplifyCheckBox->setText(QApplication::translate("SettingsWindow", "Simplify Vectors", 0));
        tabWidget->setTabText(tabWidget->indexOf(general), QApplication::translate("SettingsWindow", "General", 0));
        groupBox_3->setTitle(QApplication::translate("SettingsWindow", "Full Interpolation", 0));
        label->setText(QApplication::translate("SettingsWindow", "Blending Mode", 0));
        tabWidget->setTabText(tabWidget->indexOf(synthesis), QApplication::translate("SettingsWindow", "View Synthesis", 0));
        cancelButton->setText(QApplication::translate("SettingsWindow", "Cancel", 0));
        cancelButton->setShortcut(QApplication::translate("SettingsWindow", "Esc", 0));
        saveButton->setText(QApplication::translate("SettingsWindow", "Save", 0));
    } // retranslateUi

};

namespace Ui {
    class SettingsWindow: public Ui_SettingsWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SETTINGSWINDOW_H
