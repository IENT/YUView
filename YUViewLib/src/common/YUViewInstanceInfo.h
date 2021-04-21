#ifndef YUVIEWINSTANCEINFO_H
#define YUVIEWINSTANCEINFO_H

#include <QMetaType>
#include <QStringList>
#include <QUuid>
#include <QList>

class YUViewInstanceInfo;
typedef QList<YUViewInstanceInfo> YUViewInstanceInfoList;

class YUViewInstanceInfo
{
public:
    YUViewInstanceInfo() = default;
    ~YUViewInstanceInfo() = default;
    YUViewInstanceInfo(const YUViewInstanceInfo &) = default;
    YUViewInstanceInfo &operator=(const YUViewInstanceInfo &) = default;

    void initializeAsNewInstance();
    void autoSavePlaylist(const QByteArray &newCompressedPlaylist);
    void dropAutosavedPlaylist();
    void removeInstanceFromQSettings();
    YUViewInstanceInfo getAutosavedPlaylist();
    void cleanupRecordedInstances();
    bool isValid();

    QByteArray getCompressedPlaylist() const;

    friend QDataStream& (operator<<) (QDataStream &ds, const YUViewInstanceInfo &inObj);
    friend QDataStream& (operator>>) (QDataStream &ds, YUViewInstanceInfo &outObj);
private:
    static QList<qint64> getRunningYUViewInstances();
    static YUViewInstanceInfoList getYUViewInstancesInQSettings();

    qint64 pid;
    QUuid uuid;
    QByteArray compressedPlaylist;

    static const QString instanceInfoKey; // key used for QSettings
};

QDataStream& operator<<(QDataStream &ds, const YUViewInstanceInfo &inObj);
QDataStream& operator>>(QDataStream &ds, YUViewInstanceInfo &outObj);
QDataStream& operator<<(QDataStream &ds, const YUViewInstanceInfoList &inObj);
QDataStream& operator>>(QDataStream &ds, YUViewInstanceInfoList &outObj);

Q_DECLARE_METATYPE(YUViewInstanceInfo);
Q_DECLARE_METATYPE(YUViewInstanceInfoList);


#endif // YUVIEWINSTANCEINFO_H
