#ifndef YUVIEWINSTANCEINFO_H
#define YUVIEWINSTANCEINFO_H

#include <QMetaType>
#include <QStringList>
#include <QUuid>
#include <QList>

class YUViewInstanceInfo
{
public:
    YUViewInstanceInfo() = default;
    ~YUViewInstanceInfo() = default;
    YUViewInstanceInfo(const YUViewInstanceInfo &) = default;
    YUViewInstanceInfo &operator=(const YUViewInstanceInfo &) = default;

//    YUViewInstanceInfo(const QString &body, const QStringList &headers);

    void initializeAsNewInstance();
    void autoSavePlaylist(const QByteArray &newCompressedPlaylist);
//    void loadAutosavedPlaylist();
    void dropAutosavedPlaylist();
    void removeInstanceFromQSettings();
    YUViewInstanceInfo getAutosavedPlaylist();
    void cleanupRecordedInstances();
    bool isValid();

    QByteArray getCompressedPlaylist() const;

private:
    static QList<qint64> getRunningYUViewInstances();
    static QList<QVariant> getYUViewInstancesInQSettings();
//    const QString getKeyOfInstance();
//    static YUViewInstanceInfo fromQList(const QList<QVariant> list);
//    QList<QVariant> toQList();

    qint64 pid;
    QUuid uuid;
    QByteArray compressedPlaylist;

//    static void keyToUuidAndPid( const QString key, QUuid &uuid, qint64 &pid);
    static const QString instanceInfoKey; // key used for QSettings
};



Q_DECLARE_METATYPE(YUViewInstanceInfo);



#endif // YUVIEWINSTANCEINFO_H
