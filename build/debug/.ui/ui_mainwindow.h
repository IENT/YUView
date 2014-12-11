/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.4.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "playlisttreewidget.h"
#include "renderwidget.h"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionOpen;
    QWidget *centralWidget;
    QGridLayout *centralLayout;
    QSplitter *splitView;
    RenderWidget *renderWidget;
    RenderWidget *renderWidget2;
    QMenuBar *menuBar;
    QStatusBar *statusBar;
    QDockWidget *playlistDockWidget;
    QWidget *dockWidgetContents;
    QVBoxLayout *verticalLayout_3;
    PlaylistTreeWidget *playListTreeWidget;
    QHBoxLayout *horizontalLayout;
    QPushButton *openButton;
    QPushButton *groupButton;
    QPushButton *deleteButton;
    QPushButton *statisticsButton;
    QDockWidget *fileDockWidget;
    QWidget *dockWidgetContents_2;
    QVBoxLayout *verticalLayout_5;
    QHBoxLayout *horizontalLayout_7;
    QGridLayout *gridLayout;
    QDoubleSpinBox *rateSpinBox;
    QLabel *heightLabel;
    QSpinBox *widthSpinBox;
    QLabel *widthLabel;
    QLabel *PresetLabel;
    QSpinBox *offsetSpinBox;
    QGridLayout *gridLayout_2;
    QLabel *label_2;
    QSpinBox *framesSpinBox;
    QSpinBox *heightSpinBox;
    QLabel *label_3;
    QLabel *label_4;
    QSpinBox *samplingSpinBox;
    QGridLayout *gridLayout_3;
    QLabel *label_5;
    QComboBox *sizeComboBox;
    QLabel *label_6;
    QComboBox *colorFormatComboBox;
    QGridLayout *gridLayout_4;
    QLineEdit *modifiedLineEdit;
    QLabel *label_7;
    QLineEdit *createdLineEdit;
    QLabel *label_8;
    QLabel *label_11;
    QTextBrowser *filePathEdit;
    QDockWidget *statsDockWidget;
    QWidget *dockWidgetContents_3;
    QVBoxLayout *verticalLayout_6;
    QListView *statsListView;
    QGroupBox *statsGroupBox;
    QHBoxLayout *horizontalLayout_4;
    QFormLayout *formLayout_2;
    QLabel *label_15;
    QSlider *opacitySlider;
    QCheckBox *gridCheckBox;
    QDockWidget *displayDockWidget;
    QWidget *dockWidgetContents_4;
    QVBoxLayout *verticalLayout_4;
    QGroupBox *infoComponentsBox;
    QGridLayout *gridLayout_10;
    QPushButton *UButton;
    QPushButton *VButton;
    QPushButton *YButton;
    QGroupBox *gridBox;
    QGridLayout *gridLayout_13;
    QCheckBox *regularGridCheckBox;
    QSpinBox *gridSizeBox;
    QCheckBox *zoomBoxCheckBox;
    QGroupBox *infoChromaBox;
    QGridLayout *gridLayout_9;
    QHBoxLayout *horizontalLayout_5;
    QLabel *label_10;
    QComboBox *interpolationComboBox;
    QDockWidget *stereoDockWidget;
    QWidget *dockWidgetContents_5;
    QVBoxLayout *verticalLayout_2;
    QFormLayout *formLayout;
    QLabel *label;
    QComboBox *stereoModeBox;
    QLabel *label_12;
    QSlider *viewSlider;
    QLabel *label_14;
    QComboBox *ViewInterpolationBox;
    QLabel *label_13;
    QDoubleSpinBox *eyeDistanceBox;
    QCheckBox *switchLR;
    QDockWidget *controlsDockWidget;
    QWidget *dockWidgetContents_6;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *playButton;
    QPushButton *stopButton;
    QSlider *frameSlider;
    QSpinBox *frameCounter;
    QLabel *frameRateLabel;
    QLabel *label_9;
    QToolButton *repeatButton;
    QDockWidget *compareDockWidget;
    QWidget *dockWidgetContents_15;
    QVBoxLayout *verticalLayout;
    QCheckBox *comparisonToggle;
    QTreeView *playListTreeWidget2;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QStringLiteral("MainWindow"));
        MainWindow->resize(1164, 1236);
        QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(1);
        sizePolicy.setHeightForWidth(MainWindow->sizePolicy().hasHeightForWidth());
        MainWindow->setSizePolicy(sizePolicy);
        MainWindow->setUnifiedTitleAndToolBarOnMac(false);
        actionOpen = new QAction(MainWindow);
        actionOpen->setObjectName(QStringLiteral("actionOpen"));
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName(QStringLiteral("centralWidget"));
        QSizePolicy sizePolicy1(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        sizePolicy1.setHorizontalStretch(1);
        sizePolicy1.setVerticalStretch(1);
        sizePolicy1.setHeightForWidth(centralWidget->sizePolicy().hasHeightForWidth());
        centralWidget->setSizePolicy(sizePolicy1);
        centralWidget->setSizeIncrement(QSize(1, 1));
        centralLayout = new QGridLayout(centralWidget);
        centralLayout->setSpacing(6);
        centralLayout->setContentsMargins(11, 11, 11, 11);
        centralLayout->setObjectName(QStringLiteral("centralLayout"));
        centralLayout->setContentsMargins(0, 0, 0, 0);
        splitView = new QSplitter(centralWidget);
        splitView->setObjectName(QStringLiteral("splitView"));
        QSizePolicy sizePolicy2(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(splitView->sizePolicy().hasHeightForWidth());
        splitView->setSizePolicy(sizePolicy2);
        splitView->setOrientation(Qt::Horizontal);
        splitView->setOpaqueResize(true);
        splitView->setHandleWidth(1);
        splitView->setChildrenCollapsible(true);
        renderWidget = new RenderWidget(splitView);
        renderWidget->setObjectName(QStringLiteral("renderWidget"));
        renderWidget->setEnabled(true);
        sizePolicy2.setHeightForWidth(renderWidget->sizePolicy().hasHeightForWidth());
        renderWidget->setSizePolicy(sizePolicy2);
        renderWidget->setMinimumSize(QSize(50, 0));
        renderWidget->setMouseTracking(true);
        renderWidget->setFocusPolicy(Qt::ClickFocus);
        splitView->addWidget(renderWidget);
        renderWidget2 = new RenderWidget(splitView);
        renderWidget2->setObjectName(QStringLiteral("renderWidget2"));
        renderWidget2->setEnabled(true);
        sizePolicy2.setHeightForWidth(renderWidget2->sizePolicy().hasHeightForWidth());
        renderWidget2->setSizePolicy(sizePolicy2);
        renderWidget2->setMinimumSize(QSize(50, 0));
        renderWidget2->setMouseTracking(true);
        renderWidget2->setFocusPolicy(Qt::ClickFocus);
        splitView->addWidget(renderWidget2);

        centralLayout->addWidget(splitView, 0, 0, 1, 1);

        MainWindow->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(MainWindow);
        menuBar->setObjectName(QStringLiteral("menuBar"));
        menuBar->setGeometry(QRect(0, 0, 1164, 22));
        MainWindow->setMenuBar(menuBar);
        statusBar = new QStatusBar(MainWindow);
        statusBar->setObjectName(QStringLiteral("statusBar"));
        MainWindow->setStatusBar(statusBar);
        playlistDockWidget = new QDockWidget(MainWindow);
        playlistDockWidget->setObjectName(QStringLiteral("playlistDockWidget"));
        QSizePolicy sizePolicy3(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(playlistDockWidget->sizePolicy().hasHeightForWidth());
        playlistDockWidget->setSizePolicy(sizePolicy3);
        playlistDockWidget->setMinimumSize(QSize(298, 300));
        playlistDockWidget->setMaximumSize(QSize(524287, 524287));
        playlistDockWidget->setFloating(false);
        dockWidgetContents = new QWidget();
        dockWidgetContents->setObjectName(QStringLiteral("dockWidgetContents"));
        verticalLayout_3 = new QVBoxLayout(dockWidgetContents);
        verticalLayout_3->setSpacing(6);
        verticalLayout_3->setContentsMargins(11, 11, 11, 11);
        verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));
        playListTreeWidget = new PlaylistTreeWidget(dockWidgetContents);
        QTreeWidgetItem *__qtreewidgetitem = new QTreeWidgetItem();
        __qtreewidgetitem->setText(1, QStringLiteral("Stats"));
        playListTreeWidget->setHeaderItem(__qtreewidgetitem);
        playListTreeWidget->setObjectName(QStringLiteral("playListTreeWidget"));
        playListTreeWidget->setMinimumSize(QSize(100, 100));
        playListTreeWidget->setAcceptDrops(true);
        playListTreeWidget->setDragEnabled(true);
        playListTreeWidget->setDragDropMode(QAbstractItemView::InternalMove);
        playListTreeWidget->setDefaultDropAction(Qt::MoveAction);
        playListTreeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
        playListTreeWidget->setIconSize(QSize(16, 16));
        playListTreeWidget->setAnimated(true);
        playListTreeWidget->setColumnCount(2);
        playListTreeWidget->header()->setDefaultSectionSize(190);
        playListTreeWidget->header()->setMinimumSectionSize(25);

        verticalLayout_3->addWidget(playListTreeWidget);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(6);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        openButton = new QPushButton(dockWidgetContents);
        openButton->setObjectName(QStringLiteral("openButton"));
        openButton->setMaximumSize(QSize(16777215, 27));
        QIcon icon;
        icon.addFile(QStringLiteral(":/images/img_television.png"), QSize(), QIcon::Normal, QIcon::Off);
        openButton->setIcon(icon);
        openButton->setIconSize(QSize(16, 16));

        horizontalLayout->addWidget(openButton);

        groupButton = new QPushButton(dockWidgetContents);
        groupButton->setObjectName(QStringLiteral("groupButton"));
        groupButton->setEnabled(true);
        groupButton->setMaximumSize(QSize(16777215, 27));
        QIcon icon1;
        icon1.addFile(QStringLiteral(":/images/img_folder_add.png"), QSize(), QIcon::Normal, QIcon::Off);
        groupButton->setIcon(icon1);
        groupButton->setIconSize(QSize(16, 16));

        horizontalLayout->addWidget(groupButton);

        deleteButton = new QPushButton(dockWidgetContents);
        deleteButton->setObjectName(QStringLiteral("deleteButton"));
        deleteButton->setMaximumSize(QSize(16777215, 27));
        QIcon icon2;
        icon2.addFile(QStringLiteral(":/images/img_delete.png"), QSize(), QIcon::Normal, QIcon::Off);
        deleteButton->setIcon(icon2);
        deleteButton->setIconSize(QSize(14, 14));

        horizontalLayout->addWidget(deleteButton);

        statisticsButton = new QPushButton(dockWidgetContents);
        statisticsButton->setObjectName(QStringLiteral("statisticsButton"));
        statisticsButton->setMaximumSize(QSize(16777215, 27));
        QIcon icon3;
        icon3.addFile(QStringLiteral(":/images/stats_add.png"), QSize(), QIcon::Normal, QIcon::Off);
        statisticsButton->setIcon(icon3);

        horizontalLayout->addWidget(statisticsButton);


        verticalLayout_3->addLayout(horizontalLayout);

        playlistDockWidget->setWidget(dockWidgetContents);
        MainWindow->addDockWidget(static_cast<Qt::DockWidgetArea>(1), playlistDockWidget);
        fileDockWidget = new QDockWidget(MainWindow);
        fileDockWidget->setObjectName(QStringLiteral("fileDockWidget"));
        QSizePolicy sizePolicy4(QSizePolicy::Fixed, QSizePolicy::Preferred);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(fileDockWidget->sizePolicy().hasHeightForWidth());
        fileDockWidget->setSizePolicy(sizePolicy4);
        fileDockWidget->setMinimumSize(QSize(280, 350));
        fileDockWidget->setMaximumSize(QSize(280, 350));
        fileDockWidget->setFloating(false);
        dockWidgetContents_2 = new QWidget();
        dockWidgetContents_2->setObjectName(QStringLiteral("dockWidgetContents_2"));
        verticalLayout_5 = new QVBoxLayout(dockWidgetContents_2);
        verticalLayout_5->setSpacing(6);
        verticalLayout_5->setContentsMargins(11, 11, 11, 11);
        verticalLayout_5->setObjectName(QStringLiteral("verticalLayout_5"));
        horizontalLayout_7 = new QHBoxLayout();
        horizontalLayout_7->setSpacing(6);
        horizontalLayout_7->setObjectName(QStringLiteral("horizontalLayout_7"));
        gridLayout = new QGridLayout();
        gridLayout->setSpacing(6);
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        rateSpinBox = new QDoubleSpinBox(dockWidgetContents_2);
        rateSpinBox->setObjectName(QStringLiteral("rateSpinBox"));
        rateSpinBox->setMinimumSize(QSize(65, 0));
        rateSpinBox->setDecimals(2);
        rateSpinBox->setMaximum(300);
        rateSpinBox->setSingleStep(0.1);
        rateSpinBox->setValue(0);

        gridLayout->addWidget(rateSpinBox, 2, 1, 1, 1);

        heightLabel = new QLabel(dockWidgetContents_2);
        heightLabel->setObjectName(QStringLiteral("heightLabel"));
        heightLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(heightLabel, 0, 0, 1, 1);

        widthSpinBox = new QSpinBox(dockWidgetContents_2);
        widthSpinBox->setObjectName(QStringLiteral("widthSpinBox"));
        widthSpinBox->setMaximum(100000);
        widthSpinBox->setValue(0);

        gridLayout->addWidget(widthSpinBox, 0, 1, 1, 1);

        widthLabel = new QLabel(dockWidgetContents_2);
        widthLabel->setObjectName(QStringLiteral("widthLabel"));
        widthLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(widthLabel, 1, 0, 1, 1);

        PresetLabel = new QLabel(dockWidgetContents_2);
        PresetLabel->setObjectName(QStringLiteral("PresetLabel"));
        PresetLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(PresetLabel, 2, 0, 1, 1);

        offsetSpinBox = new QSpinBox(dockWidgetContents_2);
        offsetSpinBox->setObjectName(QStringLiteral("offsetSpinBox"));
        offsetSpinBox->setMaximum(100000);
        offsetSpinBox->setValue(0);

        gridLayout->addWidget(offsetSpinBox, 1, 1, 1, 1);


        horizontalLayout_7->addLayout(gridLayout);

        gridLayout_2 = new QGridLayout();
        gridLayout_2->setSpacing(6);
        gridLayout_2->setObjectName(QStringLiteral("gridLayout_2"));
        label_2 = new QLabel(dockWidgetContents_2);
        label_2->setObjectName(QStringLiteral("label_2"));
        label_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_2->addWidget(label_2, 0, 0, 1, 1);

        framesSpinBox = new QSpinBox(dockWidgetContents_2);
        framesSpinBox->setObjectName(QStringLiteral("framesSpinBox"));
        framesSpinBox->setMinimum(0);
        framesSpinBox->setMaximum(9999);
        framesSpinBox->setValue(0);

        gridLayout_2->addWidget(framesSpinBox, 1, 1, 1, 1);

        heightSpinBox = new QSpinBox(dockWidgetContents_2);
        heightSpinBox->setObjectName(QStringLiteral("heightSpinBox"));
        heightSpinBox->setMaximum(9999);

        gridLayout_2->addWidget(heightSpinBox, 0, 1, 1, 1);

        label_3 = new QLabel(dockWidgetContents_2);
        label_3->setObjectName(QStringLiteral("label_3"));
        label_3->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_2->addWidget(label_3, 1, 0, 1, 1);

        label_4 = new QLabel(dockWidgetContents_2);
        label_4->setObjectName(QStringLiteral("label_4"));
        label_4->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_2->addWidget(label_4, 2, 0, 1, 1);

        samplingSpinBox = new QSpinBox(dockWidgetContents_2);
        samplingSpinBox->setObjectName(QStringLiteral("samplingSpinBox"));
        samplingSpinBox->setMaximum(9999);

        gridLayout_2->addWidget(samplingSpinBox, 2, 1, 1, 1);


        horizontalLayout_7->addLayout(gridLayout_2);


        verticalLayout_5->addLayout(horizontalLayout_7);

        gridLayout_3 = new QGridLayout();
        gridLayout_3->setSpacing(6);
        gridLayout_3->setObjectName(QStringLiteral("gridLayout_3"));
        label_5 = new QLabel(dockWidgetContents_2);
        label_5->setObjectName(QStringLiteral("label_5"));
        label_5->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_3->addWidget(label_5, 0, 0, 1, 1);

        sizeComboBox = new QComboBox(dockWidgetContents_2);
        sizeComboBox->setObjectName(QStringLiteral("sizeComboBox"));
        QSizePolicy sizePolicy5(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy5.setHorizontalStretch(0);
        sizePolicy5.setVerticalStretch(0);
        sizePolicy5.setHeightForWidth(sizeComboBox->sizePolicy().hasHeightForWidth());
        sizeComboBox->setSizePolicy(sizePolicy5);

        gridLayout_3->addWidget(sizeComboBox, 0, 1, 1, 1);

        label_6 = new QLabel(dockWidgetContents_2);
        label_6->setObjectName(QStringLiteral("label_6"));
        label_6->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_3->addWidget(label_6, 1, 0, 1, 1);

        colorFormatComboBox = new QComboBox(dockWidgetContents_2);
        colorFormatComboBox->setObjectName(QStringLiteral("colorFormatComboBox"));
        sizePolicy5.setHeightForWidth(colorFormatComboBox->sizePolicy().hasHeightForWidth());
        colorFormatComboBox->setSizePolicy(sizePolicy5);

        gridLayout_3->addWidget(colorFormatComboBox, 1, 1, 1, 1);


        verticalLayout_5->addLayout(gridLayout_3);

        gridLayout_4 = new QGridLayout();
        gridLayout_4->setSpacing(6);
        gridLayout_4->setObjectName(QStringLiteral("gridLayout_4"));
        modifiedLineEdit = new QLineEdit(dockWidgetContents_2);
        modifiedLineEdit->setObjectName(QStringLiteral("modifiedLineEdit"));
        modifiedLineEdit->setFocusPolicy(Qt::NoFocus);
        modifiedLineEdit->setAcceptDrops(false);
        modifiedLineEdit->setLayoutDirection(Qt::LeftToRight);
        modifiedLineEdit->setAutoFillBackground(false);
        modifiedLineEdit->setStyleSheet(QStringLiteral("background-color: rgba(255, 255, 255, 0);"));
        modifiedLineEdit->setFrame(false);
        modifiedLineEdit->setDragEnabled(true);

        gridLayout_4->addWidget(modifiedLineEdit, 1, 1, 1, 1);

        label_7 = new QLabel(dockWidgetContents_2);
        label_7->setObjectName(QStringLiteral("label_7"));
        label_7->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_4->addWidget(label_7, 0, 0, 1, 1);

        createdLineEdit = new QLineEdit(dockWidgetContents_2);
        createdLineEdit->setObjectName(QStringLiteral("createdLineEdit"));
        createdLineEdit->setFocusPolicy(Qt::NoFocus);
        createdLineEdit->setAcceptDrops(false);
        createdLineEdit->setAutoFillBackground(false);
        createdLineEdit->setStyleSheet(QStringLiteral("background-color: rgba(255, 255, 255, 0);"));
        createdLineEdit->setFrame(false);
        createdLineEdit->setDragEnabled(true);

        gridLayout_4->addWidget(createdLineEdit, 0, 1, 1, 1);

        label_8 = new QLabel(dockWidgetContents_2);
        label_8->setObjectName(QStringLiteral("label_8"));
        label_8->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_4->addWidget(label_8, 1, 0, 1, 1);

        label_11 = new QLabel(dockWidgetContents_2);
        label_11->setObjectName(QStringLiteral("label_11"));
        QSizePolicy sizePolicy6(QSizePolicy::Preferred, QSizePolicy::Expanding);
        sizePolicy6.setHorizontalStretch(0);
        sizePolicy6.setVerticalStretch(0);
        sizePolicy6.setHeightForWidth(label_11->sizePolicy().hasHeightForWidth());
        label_11->setSizePolicy(sizePolicy6);
        label_11->setMinimumSize(QSize(70, 50));
        label_11->setAlignment(Qt::AlignRight|Qt::AlignTop|Qt::AlignTrailing);

        gridLayout_4->addWidget(label_11, 2, 0, 1, 1);

        filePathEdit = new QTextBrowser(dockWidgetContents_2);
        filePathEdit->setObjectName(QStringLiteral("filePathEdit"));
        QSizePolicy sizePolicy7(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy7.setHorizontalStretch(0);
        sizePolicy7.setVerticalStretch(0);
        sizePolicy7.setHeightForWidth(filePathEdit->sizePolicy().hasHeightForWidth());
        filePathEdit->setSizePolicy(sizePolicy7);
        filePathEdit->setMinimumSize(QSize(0, 0));
        filePathEdit->viewport()->setProperty("cursor", QVariant(QCursor(Qt::IBeamCursor)));
        filePathEdit->setMouseTracking(true);
        filePathEdit->setAutoFillBackground(false);
        filePathEdit->setStyleSheet(QStringLiteral("background-color: rgba(255, 255, 255, 0);"));
        filePathEdit->setFrameShape(QFrame::NoFrame);
        filePathEdit->setFrameShadow(QFrame::Plain);
        filePathEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        filePathEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        gridLayout_4->addWidget(filePathEdit, 2, 1, 1, 1);


        verticalLayout_5->addLayout(gridLayout_4);

        fileDockWidget->setWidget(dockWidgetContents_2);
        MainWindow->addDockWidget(static_cast<Qt::DockWidgetArea>(2), fileDockWidget);
        statsDockWidget = new QDockWidget(MainWindow);
        statsDockWidget->setObjectName(QStringLiteral("statsDockWidget"));
        statsDockWidget->setMinimumSize(QSize(200, 300));
        statsDockWidget->setMaximumSize(QSize(524287, 524287));
        statsDockWidget->setFloating(false);
        dockWidgetContents_3 = new QWidget();
        dockWidgetContents_3->setObjectName(QStringLiteral("dockWidgetContents_3"));
        verticalLayout_6 = new QVBoxLayout(dockWidgetContents_3);
        verticalLayout_6->setSpacing(6);
        verticalLayout_6->setContentsMargins(11, 11, 11, 11);
        verticalLayout_6->setObjectName(QStringLiteral("verticalLayout_6"));
        statsListView = new QListView(dockWidgetContents_3);
        statsListView->setObjectName(QStringLiteral("statsListView"));
        statsListView->setEditTriggers(QAbstractItemView::AllEditTriggers);
        statsListView->setDragEnabled(true);
        statsListView->setDragDropOverwriteMode(false);
        statsListView->setDragDropMode(QAbstractItemView::InternalMove);
        statsListView->setDefaultDropAction(Qt::MoveAction);
        statsListView->setSelectionMode(QAbstractItemView::SingleSelection);
        statsListView->setSelectionBehavior(QAbstractItemView::SelectRows);

        verticalLayout_6->addWidget(statsListView);

        statsGroupBox = new QGroupBox(dockWidgetContents_3);
        statsGroupBox->setObjectName(QStringLiteral("statsGroupBox"));
        QSizePolicy sizePolicy8(QSizePolicy::Minimum, QSizePolicy::Minimum);
        sizePolicy8.setHorizontalStretch(0);
        sizePolicy8.setVerticalStretch(0);
        sizePolicy8.setHeightForWidth(statsGroupBox->sizePolicy().hasHeightForWidth());
        statsGroupBox->setSizePolicy(sizePolicy8);
        statsGroupBox->setMaximumSize(QSize(16777215, 50));
        statsGroupBox->setCheckable(false);
        horizontalLayout_4 = new QHBoxLayout(statsGroupBox);
        horizontalLayout_4->setSpacing(6);
        horizontalLayout_4->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_4->setObjectName(QStringLiteral("horizontalLayout_4"));
        horizontalLayout_4->setContentsMargins(0, 0, 0, 0);
        formLayout_2 = new QFormLayout();
        formLayout_2->setSpacing(6);
        formLayout_2->setObjectName(QStringLiteral("formLayout_2"));
        formLayout_2->setSizeConstraint(QLayout::SetMaximumSize);
        formLayout_2->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        formLayout_2->setContentsMargins(-1, -1, 10, -1);
        label_15 = new QLabel(statsGroupBox);
        label_15->setObjectName(QStringLiteral("label_15"));
        label_15->setEnabled(false);

        formLayout_2->setWidget(0, QFormLayout::LabelRole, label_15);

        opacitySlider = new QSlider(statsGroupBox);
        opacitySlider->setObjectName(QStringLiteral("opacitySlider"));
        opacitySlider->setEnabled(false);
        opacitySlider->setOrientation(Qt::Horizontal);

        formLayout_2->setWidget(0, QFormLayout::FieldRole, opacitySlider);

        gridCheckBox = new QCheckBox(statsGroupBox);
        gridCheckBox->setObjectName(QStringLiteral("gridCheckBox"));
        gridCheckBox->setEnabled(false);

        formLayout_2->setWidget(1, QFormLayout::FieldRole, gridCheckBox);


        horizontalLayout_4->addLayout(formLayout_2);


        verticalLayout_6->addWidget(statsGroupBox);

        statsDockWidget->setWidget(dockWidgetContents_3);
        MainWindow->addDockWidget(static_cast<Qt::DockWidgetArea>(1), statsDockWidget);
        displayDockWidget = new QDockWidget(MainWindow);
        displayDockWidget->setObjectName(QStringLiteral("displayDockWidget"));
        QSizePolicy sizePolicy9(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy9.setHorizontalStretch(0);
        sizePolicy9.setVerticalStretch(0);
        sizePolicy9.setHeightForWidth(displayDockWidget->sizePolicy().hasHeightForWidth());
        displayDockWidget->setSizePolicy(sizePolicy9);
        displayDockWidget->setMinimumSize(QSize(280, 239));
        displayDockWidget->setMaximumSize(QSize(280, 239));
        displayDockWidget->setFloating(false);
        dockWidgetContents_4 = new QWidget();
        dockWidgetContents_4->setObjectName(QStringLiteral("dockWidgetContents_4"));
        verticalLayout_4 = new QVBoxLayout(dockWidgetContents_4);
        verticalLayout_4->setSpacing(6);
        verticalLayout_4->setContentsMargins(11, 11, 11, 11);
        verticalLayout_4->setObjectName(QStringLiteral("verticalLayout_4"));
        infoComponentsBox = new QGroupBox(dockWidgetContents_4);
        infoComponentsBox->setObjectName(QStringLiteral("infoComponentsBox"));
        infoComponentsBox->setEnabled(false);
        QSizePolicy sizePolicy10(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy10.setHorizontalStretch(0);
        sizePolicy10.setVerticalStretch(0);
        sizePolicy10.setHeightForWidth(infoComponentsBox->sizePolicy().hasHeightForWidth());
        infoComponentsBox->setSizePolicy(sizePolicy10);
        infoComponentsBox->setMinimumSize(QSize(200, 70));
        gridLayout_10 = new QGridLayout(infoComponentsBox);
        gridLayout_10->setSpacing(6);
        gridLayout_10->setContentsMargins(11, 11, 11, 11);
        gridLayout_10->setObjectName(QStringLiteral("gridLayout_10"));
        gridLayout_10->setContentsMargins(0, 0, 0, 0);
        UButton = new QPushButton(infoComponentsBox);
        UButton->setObjectName(QStringLiteral("UButton"));
        UButton->setMaximumSize(QSize(16777215, 35));
        QFont font;
        font.setPointSize(20);
        font.setBold(true);
        font.setWeight(75);
        UButton->setFont(font);
        QIcon icon4;
        icon4.addFile(QStringLiteral(":/images/U_on.png"), QSize(), QIcon::Normal, QIcon::Off);
        UButton->setIcon(icon4);
        UButton->setIconSize(QSize(20, 20));

        gridLayout_10->addWidget(UButton, 0, 1, 1, 1);

        VButton = new QPushButton(infoComponentsBox);
        VButton->setObjectName(QStringLiteral("VButton"));
        VButton->setMaximumSize(QSize(16777215, 35));
        VButton->setFont(font);
        QIcon icon5;
        icon5.addFile(QStringLiteral(":/images/V_on.png"), QSize(), QIcon::Normal, QIcon::Off);
        VButton->setIcon(icon5);
        VButton->setIconSize(QSize(20, 20));

        gridLayout_10->addWidget(VButton, 0, 2, 1, 1);

        YButton = new QPushButton(infoComponentsBox);
        YButton->setObjectName(QStringLiteral("YButton"));
        YButton->setMaximumSize(QSize(16777215, 35));
        QFont font1;
        font1.setPointSize(20);
        font1.setBold(true);
        font1.setWeight(75);
        font1.setStyleStrategy(QFont::PreferDefault);
        YButton->setFont(font1);
        QIcon icon6;
        icon6.addFile(QStringLiteral(":/images/Y_on.png"), QSize(), QIcon::Normal, QIcon::Off);
        YButton->setIcon(icon6);
        YButton->setIconSize(QSize(20, 20));
        YButton->setFlat(false);

        gridLayout_10->addWidget(YButton, 0, 0, 1, 1);


        verticalLayout_4->addWidget(infoComponentsBox);

        gridBox = new QGroupBox(dockWidgetContents_4);
        gridBox->setObjectName(QStringLiteral("gridBox"));
        gridBox->setEnabled(false);
        sizePolicy10.setHeightForWidth(gridBox->sizePolicy().hasHeightForWidth());
        gridBox->setSizePolicy(sizePolicy10);
        gridLayout_13 = new QGridLayout(gridBox);
        gridLayout_13->setSpacing(6);
        gridLayout_13->setContentsMargins(11, 11, 11, 11);
        gridLayout_13->setObjectName(QStringLiteral("gridLayout_13"));
        gridLayout_13->setContentsMargins(0, 0, 0, 0);
        regularGridCheckBox = new QCheckBox(gridBox);
        regularGridCheckBox->setObjectName(QStringLiteral("regularGridCheckBox"));

        gridLayout_13->addWidget(regularGridCheckBox, 0, 0, 1, 1);

        gridSizeBox = new QSpinBox(gridBox);
        gridSizeBox->setObjectName(QStringLiteral("gridSizeBox"));
        gridSizeBox->setMinimum(1);
        gridSizeBox->setMaximum(512);
        gridSizeBox->setValue(64);

        gridLayout_13->addWidget(gridSizeBox, 0, 1, 1, 1);

        zoomBoxCheckBox = new QCheckBox(gridBox);
        zoomBoxCheckBox->setObjectName(QStringLiteral("zoomBoxCheckBox"));

        gridLayout_13->addWidget(zoomBoxCheckBox, 1, 0, 1, 1);


        verticalLayout_4->addWidget(gridBox);

        infoChromaBox = new QGroupBox(dockWidgetContents_4);
        infoChromaBox->setObjectName(QStringLiteral("infoChromaBox"));
        infoChromaBox->setEnabled(false);
        sizePolicy10.setHeightForWidth(infoChromaBox->sizePolicy().hasHeightForWidth());
        infoChromaBox->setSizePolicy(sizePolicy10);
        gridLayout_9 = new QGridLayout(infoChromaBox);
        gridLayout_9->setSpacing(6);
        gridLayout_9->setContentsMargins(11, 11, 11, 11);
        gridLayout_9->setObjectName(QStringLiteral("gridLayout_9"));
        gridLayout_9->setContentsMargins(0, 0, 0, 0);
        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setSpacing(6);
        horizontalLayout_5->setObjectName(QStringLiteral("horizontalLayout_5"));
        label_10 = new QLabel(infoChromaBox);
        label_10->setObjectName(QStringLiteral("label_10"));
        label_10->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        horizontalLayout_5->addWidget(label_10);

        interpolationComboBox = new QComboBox(infoChromaBox);
        interpolationComboBox->setObjectName(QStringLiteral("interpolationComboBox"));
        interpolationComboBox->setEnabled(false);
        interpolationComboBox->setMaxVisibleItems(4);

        horizontalLayout_5->addWidget(interpolationComboBox);


        gridLayout_9->addLayout(horizontalLayout_5, 0, 0, 1, 1);


        verticalLayout_4->addWidget(infoChromaBox);

        displayDockWidget->setWidget(dockWidgetContents_4);
        MainWindow->addDockWidget(static_cast<Qt::DockWidgetArea>(2), displayDockWidget);
        stereoDockWidget = new QDockWidget(MainWindow);
        stereoDockWidget->setObjectName(QStringLiteral("stereoDockWidget"));
        sizePolicy9.setHeightForWidth(stereoDockWidget->sizePolicy().hasHeightForWidth());
        stereoDockWidget->setSizePolicy(sizePolicy9);
        stereoDockWidget->setMinimumSize(QSize(280, 215));
        stereoDockWidget->setMaximumSize(QSize(280, 215));
        stereoDockWidget->setFloating(false);
        dockWidgetContents_5 = new QWidget();
        dockWidgetContents_5->setObjectName(QStringLiteral("dockWidgetContents_5"));
        verticalLayout_2 = new QVBoxLayout(dockWidgetContents_5);
        verticalLayout_2->setSpacing(6);
        verticalLayout_2->setContentsMargins(11, 11, 11, 11);
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
        formLayout = new QFormLayout();
        formLayout->setSpacing(6);
        formLayout->setObjectName(QStringLiteral("formLayout"));
        label = new QLabel(dockWidgetContents_5);
        label->setObjectName(QStringLiteral("label"));
        label->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        formLayout->setWidget(0, QFormLayout::LabelRole, label);

        stereoModeBox = new QComboBox(dockWidgetContents_5);
        stereoModeBox->setObjectName(QStringLiteral("stereoModeBox"));
        QSizePolicy sizePolicy11(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        sizePolicy11.setHorizontalStretch(0);
        sizePolicy11.setVerticalStretch(0);
        sizePolicy11.setHeightForWidth(stereoModeBox->sizePolicy().hasHeightForWidth());
        stereoModeBox->setSizePolicy(sizePolicy11);
        stereoModeBox->setMaximumSize(QSize(170, 16777215));

        formLayout->setWidget(0, QFormLayout::FieldRole, stereoModeBox);

        label_12 = new QLabel(dockWidgetContents_5);
        label_12->setObjectName(QStringLiteral("label_12"));

        formLayout->setWidget(1, QFormLayout::LabelRole, label_12);

        viewSlider = new QSlider(dockWidgetContents_5);
        viewSlider->setObjectName(QStringLiteral("viewSlider"));
        viewSlider->setEnabled(false);
        QSizePolicy sizePolicy12(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
        sizePolicy12.setHorizontalStretch(0);
        sizePolicy12.setVerticalStretch(0);
        sizePolicy12.setHeightForWidth(viewSlider->sizePolicy().hasHeightForWidth());
        viewSlider->setSizePolicy(sizePolicy12);
        viewSlider->setMaximumSize(QSize(165, 16777215));
        viewSlider->setMaximum(3);
        viewSlider->setValue(0);
        viewSlider->setOrientation(Qt::Horizontal);
        viewSlider->setTickInterval(1);

        formLayout->setWidget(1, QFormLayout::FieldRole, viewSlider);

        label_14 = new QLabel(dockWidgetContents_5);
        label_14->setObjectName(QStringLiteral("label_14"));

        formLayout->setWidget(3, QFormLayout::LabelRole, label_14);

        ViewInterpolationBox = new QComboBox(dockWidgetContents_5);
        ViewInterpolationBox->setObjectName(QStringLiteral("ViewInterpolationBox"));
        sizePolicy11.setHeightForWidth(ViewInterpolationBox->sizePolicy().hasHeightForWidth());
        ViewInterpolationBox->setSizePolicy(sizePolicy11);
        ViewInterpolationBox->setMaximumSize(QSize(170, 16777215));

        formLayout->setWidget(3, QFormLayout::FieldRole, ViewInterpolationBox);

        label_13 = new QLabel(dockWidgetContents_5);
        label_13->setObjectName(QStringLiteral("label_13"));

        formLayout->setWidget(4, QFormLayout::LabelRole, label_13);

        eyeDistanceBox = new QDoubleSpinBox(dockWidgetContents_5);
        eyeDistanceBox->setObjectName(QStringLiteral("eyeDistanceBox"));
        eyeDistanceBox->setMinimumSize(QSize(0, 25));
        eyeDistanceBox->setMinimum(-10);
        eyeDistanceBox->setMaximum(10);
        eyeDistanceBox->setSingleStep(0.2);

        formLayout->setWidget(4, QFormLayout::FieldRole, eyeDistanceBox);

        switchLR = new QCheckBox(dockWidgetContents_5);
        switchLR->setObjectName(QStringLiteral("switchLR"));
        switchLR->setMinimumSize(QSize(0, 20));

        formLayout->setWidget(2, QFormLayout::FieldRole, switchLR);


        verticalLayout_2->addLayout(formLayout);

        stereoDockWidget->setWidget(dockWidgetContents_5);
        MainWindow->addDockWidget(static_cast<Qt::DockWidgetArea>(2), stereoDockWidget);
        controlsDockWidget = new QDockWidget(MainWindow);
        controlsDockWidget->setObjectName(QStringLiteral("controlsDockWidget"));
        sizePolicy10.setHeightForWidth(controlsDockWidget->sizePolicy().hasHeightForWidth());
        controlsDockWidget->setSizePolicy(sizePolicy10);
        controlsDockWidget->setMinimumSize(QSize(287, 85));
        controlsDockWidget->setMaximumSize(QSize(524287, 85));
        dockWidgetContents_6 = new QWidget();
        dockWidgetContents_6->setObjectName(QStringLiteral("dockWidgetContents_6"));
        sizePolicy10.setHeightForWidth(dockWidgetContents_6->sizePolicy().hasHeightForWidth());
        dockWidgetContents_6->setSizePolicy(sizePolicy10);
        horizontalLayout_2 = new QHBoxLayout(dockWidgetContents_6);
        horizontalLayout_2->setSpacing(6);
        horizontalLayout_2->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        playButton = new QPushButton(dockWidgetContents_6);
        playButton->setObjectName(QStringLiteral("playButton"));
        playButton->setEnabled(false);
        QSizePolicy sizePolicy13(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy13.setHorizontalStretch(1);
        sizePolicy13.setVerticalStretch(1);
        sizePolicy13.setHeightForWidth(playButton->sizePolicy().hasHeightForWidth());
        playButton->setSizePolicy(sizePolicy13);
        playButton->setMinimumSize(QSize(16, 16));
        playButton->setFocusPolicy(Qt::NoFocus);
        QIcon icon7;
        icon7.addFile(QStringLiteral(":/images/img_play.png"), QSize(), QIcon::Normal, QIcon::Off);
        playButton->setIcon(icon7);
        playButton->setIconSize(QSize(16, 16));
        playButton->setDefault(false);
        playButton->setFlat(false);

        horizontalLayout_2->addWidget(playButton);

        stopButton = new QPushButton(dockWidgetContents_6);
        stopButton->setObjectName(QStringLiteral("stopButton"));
        stopButton->setEnabled(false);
        sizePolicy9.setHeightForWidth(stopButton->sizePolicy().hasHeightForWidth());
        stopButton->setSizePolicy(sizePolicy9);
        stopButton->setFocusPolicy(Qt::NoFocus);
        QIcon icon8;
        icon8.addFile(QStringLiteral(":/images/img_stop.png"), QSize(), QIcon::Normal, QIcon::Off);
        stopButton->setIcon(icon8);
        stopButton->setIconSize(QSize(14, 14));

        horizontalLayout_2->addWidget(stopButton);

        frameSlider = new QSlider(dockWidgetContents_6);
        frameSlider->setObjectName(QStringLiteral("frameSlider"));
        frameSlider->setEnabled(false);
        frameSlider->setFocusPolicy(Qt::NoFocus);
        frameSlider->setMaximum(100);
        frameSlider->setOrientation(Qt::Horizontal);
        frameSlider->setInvertedControls(false);
        frameSlider->setTickPosition(QSlider::TicksBelow);

        horizontalLayout_2->addWidget(frameSlider);

        frameCounter = new QSpinBox(dockWidgetContents_6);
        frameCounter->setObjectName(QStringLiteral("frameCounter"));
        frameCounter->setEnabled(false);
        frameCounter->setMaximum(9999);

        horizontalLayout_2->addWidget(frameCounter);

        frameRateLabel = new QLabel(dockWidgetContents_6);
        frameRateLabel->setObjectName(QStringLiteral("frameRateLabel"));
        frameRateLabel->setMinimumSize(QSize(23, 0));
        frameRateLabel->setMaximumSize(QSize(23, 16777215));

        horizontalLayout_2->addWidget(frameRateLabel);

        label_9 = new QLabel(dockWidgetContents_6);
        label_9->setObjectName(QStringLiteral("label_9"));
        label_9->setMinimumSize(QSize(20, 0));

        horizontalLayout_2->addWidget(label_9);

        repeatButton = new QToolButton(dockWidgetContents_6);
        repeatButton->setObjectName(QStringLiteral("repeatButton"));
        QIcon icon9;
        icon9.addFile(QStringLiteral(":/images/img_repeat_on.png"), QSize(), QIcon::Normal, QIcon::Off);
        repeatButton->setIcon(icon9);
        repeatButton->setIconSize(QSize(16, 16));

        horizontalLayout_2->addWidget(repeatButton);

        controlsDockWidget->setWidget(dockWidgetContents_6);
        MainWindow->addDockWidget(static_cast<Qt::DockWidgetArea>(8), controlsDockWidget);
        compareDockWidget = new QDockWidget(MainWindow);
        compareDockWidget->setObjectName(QStringLiteral("compareDockWidget"));
        compareDockWidget->setMinimumSize(QSize(200, 300));
        compareDockWidget->setFloating(false);
        dockWidgetContents_15 = new QWidget();
        dockWidgetContents_15->setObjectName(QStringLiteral("dockWidgetContents_15"));
        verticalLayout = new QVBoxLayout(dockWidgetContents_15);
        verticalLayout->setSpacing(6);
        verticalLayout->setContentsMargins(11, 11, 11, 11);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        comparisonToggle = new QCheckBox(dockWidgetContents_15);
        comparisonToggle->setObjectName(QStringLiteral("comparisonToggle"));

        verticalLayout->addWidget(comparisonToggle);

        playListTreeWidget2 = new QTreeView(dockWidgetContents_15);
        playListTreeWidget2->setObjectName(QStringLiteral("playListTreeWidget2"));
        playListTreeWidget2->setFocusPolicy(Qt::ClickFocus);
        playListTreeWidget2->setAcceptDrops(false);
        playListTreeWidget2->setEditTriggers(QAbstractItemView::AllEditTriggers);
        playListTreeWidget2->setDragEnabled(true);
        playListTreeWidget2->setDragDropMode(QAbstractItemView::InternalMove);
        playListTreeWidget2->setDefaultDropAction(Qt::MoveAction);
        playListTreeWidget2->setSelectionMode(QAbstractItemView::ExtendedSelection);
        playListTreeWidget2->setIconSize(QSize(16, 16));
        playListTreeWidget2->setAnimated(true);

        verticalLayout->addWidget(playListTreeWidget2);

        compareDockWidget->setWidget(dockWidgetContents_15);
        MainWindow->addDockWidget(static_cast<Qt::DockWidgetArea>(2), compareDockWidget);

        retranslateUi(MainWindow);
        QObject::connect(playListTreeWidget, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), MainWindow, SLOT(setSelectedYUV(QTreeWidgetItem*)));
        QObject::connect(groupButton, SIGNAL(clicked()), MainWindow, SLOT(addGroup()));
        QObject::connect(deleteButton, SIGNAL(clicked()), MainWindow, SLOT(deleteItem()));
        QObject::connect(colorFormatComboBox, SIGNAL(currentIndexChanged(int)), MainWindow, SLOT(on_colorFormatComboBox_currentIndexChanged(int)));
        QObject::connect(interpolationComboBox, SIGNAL(currentIndexChanged(int)), MainWindow, SLOT(on_interpolationComboBox_currentIndexChanged(int)));
        QObject::connect(ViewInterpolationBox, SIGNAL(currentIndexChanged(int)), MainWindow, SLOT(setViewInterpolationMode(int)));
        QObject::connect(stereoModeBox, SIGNAL(currentIndexChanged(int)), MainWindow, SLOT(setRenderMode(int)));
        QObject::connect(viewSlider, SIGNAL(sliderMoved(int)), MainWindow, SLOT(setCurrentLeftView(int)));
        QObject::connect(switchLR, SIGNAL(toggled(bool)), MainWindow, SLOT(switchLR()));
        QObject::connect(playButton, SIGNAL(clicked()), MainWindow, SLOT(togglePlayback()));
        QObject::connect(stopButton, SIGNAL(clicked()), MainWindow, SLOT(stop()));
        QObject::connect(frameSlider, SIGNAL(valueChanged(int)), MainWindow, SLOT(setCurrentFrame(int)));
        QObject::connect(frameCounter, SIGNAL(valueChanged(int)), MainWindow, SLOT(setCurrentFrame(int)));
        QObject::connect(repeatButton, SIGNAL(clicked()), MainWindow, SLOT(toggleRepeat()));
        QObject::connect(openButton, SIGNAL(clicked()), MainWindow, SLOT(openFile()));
        QObject::connect(opacitySlider, SIGNAL(valueChanged(int)), MainWindow, SLOT(updateStatsOpacity(int)));
        QObject::connect(gridCheckBox, SIGNAL(toggled(bool)), MainWindow, SLOT(updateStatsGrid(bool)));
        QObject::connect(statsListView, SIGNAL(clicked(QModelIndex)), MainWindow, SLOT(setSelectedStats()));
        QObject::connect(eyeDistanceBox, SIGNAL(valueChanged(double)), MainWindow, SLOT(setEyeDistance(double)));
        QObject::connect(regularGridCheckBox, SIGNAL(clicked()), MainWindow, SLOT(updateGrid()));
        QObject::connect(gridSizeBox, SIGNAL(valueChanged(int)), MainWindow, SLOT(updateGrid()));
        QObject::connect(zoomBoxCheckBox, SIGNAL(toggled(bool)), renderWidget, SLOT(setZoomBoxEnabled(bool)));
        QObject::connect(comparisonToggle, SIGNAL(stateChanged(int)), MainWindow, SLOT(enableComparison(int)));
        QObject::connect(VButton, SIGNAL(clicked()), MainWindow, SLOT(toggleV()));
        QObject::connect(UButton, SIGNAL(clicked()), MainWindow, SLOT(toggleU()));
        QObject::connect(YButton, SIGNAL(clicked()), MainWindow, SLOT(toggleY()));
        QObject::connect(zoomBoxCheckBox, SIGNAL(toggled(bool)), renderWidget2, SLOT(setZoomBoxEnabled(bool)));

        interpolationComboBox->setCurrentIndex(1);
        stereoModeBox->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "YUView", 0));
        actionOpen->setText(QApplication::translate("MainWindow", "Open...", 0));
        playlistDockWidget->setWindowTitle(QApplication::translate("MainWindow", "Playlist", 0));
        QTreeWidgetItem *___qtreewidgetitem = playListTreeWidget->headerItem();
        ___qtreewidgetitem->setText(0, QApplication::translate("MainWindow", "Video File", 0));
        openButton->setText(QString());
        groupButton->setText(QString());
        deleteButton->setText(QString());
        statisticsButton->setText(QString());
        fileDockWidget->setWindowTitle(QApplication::translate("MainWindow", "File Options", 0));
        heightLabel->setText(QApplication::translate("MainWindow", "Width:", 0));
        widthLabel->setText(QApplication::translate("MainWindow", "Start:", 0));
        PresetLabel->setText(QApplication::translate("MainWindow", "Rate:", 0));
        label_2->setText(QApplication::translate("MainWindow", "Height:", 0));
        label_3->setText(QApplication::translate("MainWindow", "Frames:", 0));
        label_4->setText(QApplication::translate("MainWindow", "Sampling:", 0));
        label_5->setText(QApplication::translate("MainWindow", "Frame Size:", 0));
        sizeComboBox->clear();
        sizeComboBox->insertItems(0, QStringList()
         << QApplication::translate("MainWindow", "Custom Size", 0)
         << QApplication::translate("MainWindow", "QCIF - (176x144)", 0)
         << QApplication::translate("MainWindow", "QVGA - (320x240)", 0)
         << QApplication::translate("MainWindow", "WQVGA - (416x240)", 0)
         << QApplication::translate("MainWindow", "CIF - (352x288)", 0)
         << QApplication::translate("MainWindow", "VGA - (640x480)", 0)
         << QApplication::translate("MainWindow", "WVGA - (832x480)", 0)
         << QApplication::translate("MainWindow", "4CIF - (704x576)", 0)
         << QApplication::translate("MainWindow", "ITU R.BT601 - (720x576)", 0)
         << QApplication::translate("MainWindow", "720i/p - (1280x720)", 0)
         << QApplication::translate("MainWindow", "1080i/p - (1920x1080)", 0)
         << QApplication::translate("MainWindow", "4k x 2k - (3840x2160)", 0)
         << QApplication::translate("MainWindow", "XGA - (1024x768)", 0)
         << QApplication::translate("MainWindow", "XGA+ - (1280x960)", 0)
        );
        label_6->setText(QApplication::translate("MainWindow", "Color Format:", 0));
        colorFormatComboBox->clear();
        colorFormatComboBox->insertItems(0, QStringList()
         << QApplication::translate("MainWindow", "4:0:0 8-bit ", 0)
         << QApplication::translate("MainWindow", "4:1:1 Y'CbCr 8-bit planar", 0)
         << QApplication::translate("MainWindow", "4:2:0 Y'CbCr 8-bit planar", 0)
         << QApplication::translate("MainWindow", "4:2:0 Y'CbCr 10-bit LE planar", 0)
         << QApplication::translate("MainWindow", "4:2:2 Y'CbCr 8-bit planar", 0)
         << QApplication::translate("MainWindow", "4:4:4 Y'CbCr 8-bit planar", 0)
        );
        label_7->setText(QApplication::translate("MainWindow", "Created:", 0));
        label_8->setText(QApplication::translate("MainWindow", "Modified:", 0));
        label_11->setText(QApplication::translate("MainWindow", "File Path:", 0));
        filePathEdit->setHtml(QApplication::translate("MainWindow", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'.Helvetica Neue DeskInterface'; font-size:13pt; font-weight:400; font-style:normal;\">\n"
"<p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px; font-family:'Lucida Grande'; vertical-align:top;\"><br /></p></body></html>", 0));
        statsDockWidget->setWindowTitle(QApplication::translate("MainWindow", "Statistics", 0));
        statsGroupBox->setTitle(QString());
        label_15->setText(QApplication::translate("MainWindow", "Opacity", 0));
        gridCheckBox->setText(QApplication::translate("MainWindow", "Show Grid", 0));
        displayDockWidget->setWindowTitle(QApplication::translate("MainWindow", "Display Options", 0));
        infoComponentsBox->setTitle(QApplication::translate("MainWindow", "YUV Components", 0));
        UButton->setText(QString());
        VButton->setText(QString());
        YButton->setText(QString());
        gridBox->setTitle(QApplication::translate("MainWindow", "Regular Grid / Zoom Box", 0));
        regularGridCheckBox->setText(QApplication::translate("MainWindow", "Draw grid", 0));
        zoomBoxCheckBox->setText(QApplication::translate("MainWindow", "Zoom Box", 0));
        infoChromaBox->setTitle(QApplication::translate("MainWindow", "YUV to YUV 4:4:4", 0));
        label_10->setText(QApplication::translate("MainWindow", "Interpolation:", 0));
        interpolationComboBox->clear();
        interpolationComboBox->insertItems(0, QStringList()
         << QApplication::translate("MainWindow", "Nearest Neighbor", 0)
         << QApplication::translate("MainWindow", "Bilinear", 0)
        );
        stereoDockWidget->setWindowTitle(QApplication::translate("MainWindow", "3D Options", 0));
        label->setText(QApplication::translate("MainWindow", "Stereo Mode:", 0));
        stereoModeBox->clear();
        stereoModeBox->insertItems(0, QStringList()
         << QApplication::translate("MainWindow", "No Stereo", 0)
         << QApplication::translate("MainWindow", "Shutter", 0)
         << QApplication::translate("MainWindow", "Anaglyph", 0)
         << QApplication::translate("MainWindow", "Line Interleaving", 0)
        );
        label_12->setText(QApplication::translate("MainWindow", "View:", 0));
        label_14->setText(QApplication::translate("MainWindow", "Interpolation:", 0));
        ViewInterpolationBox->clear();
        ViewInterpolationBox->insertItems(0, QStringList()
         << QApplication::translate("MainWindow", "No View Interpolation", 0)
         << QApplication::translate("MainWindow", "Full View Interpolation", 0)
        );
        label_13->setText(QApplication::translate("MainWindow", "Eye Distance:", 0));
        switchLR->setText(QApplication::translate("MainWindow", "Switch L/R", 0));
        controlsDockWidget->setWindowTitle(QApplication::translate("MainWindow", "Playback Controls", 0));
        playButton->setText(QString());
        stopButton->setText(QString());
        frameRateLabel->setText(QApplication::translate("MainWindow", " 0", 0));
        label_9->setText(QApplication::translate("MainWindow", "fps", 0));
        repeatButton->setText(QApplication::translate("MainWindow", "...", 0));
        compareDockWidget->setWindowTitle(QApplication::translate("MainWindow", "Comparison", 0));
        comparisonToggle->setText(QApplication::translate("MainWindow", "Enable Comparison View", 0));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
