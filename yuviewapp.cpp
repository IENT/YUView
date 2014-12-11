#include "yuviewapp.h"

#include "version.h"

YUViewApp::YUViewApp( int & argc, char **argv ) : QApplication(argc, argv)
{
    QString versionString = QString::fromUtf8(YUVIEW_VERSION);

    QApplication::setApplicationName("YUView");
    if(versionString.contains("debug"))
        QApplication::setApplicationVersion("42424242");    // no updates for debug version
    else
        QApplication::setApplicationVersion(versionString);
    QApplication::setOrganizationName("Institut für Nachrichtentechnik, RWTH Aachen University");
    QApplication::setOrganizationDomain("ient.rwth-aachen.de");

    w = new MainWindow();

    QStringList fileList;
    for ( int i = 1; i < argc; ++i )
    {
        fileList.append( QString(argv[i]) );
    }

    w->loadFiles(fileList);

    w->show();
}

bool YUViewApp::event(QEvent *event)
{
    QStringList fileList;

    switch (event->type())
    {
    case QEvent::FileOpen:
        fileList.append((static_cast<QFileOpenEvent *>(event))->file());
        w->loadFiles( fileList );
        return true;
    default:
        return QApplication::event(event);
    }
}
