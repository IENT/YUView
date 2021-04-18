#include "YUViewInstanceInfo.h"
#include <QCoreApplication>
#include <QProcess>
#include <QSettings>


const QString YUViewInstanceInfo::instanceInfoKey = QString("YUViewInstances");

void YUViewInstanceInfo::initializeAsNewInstance()
{
  QSettings settings;
  uuid = QUuid::createUuid();
  pid = QCoreApplication::applicationPid();
  // append instance to recorded ones
  QList<QVariant> listOfQSettingInstances = YUViewInstanceInfo::getYUViewInstancesInQSettings();
  listOfQSettingInstances.append(QVariant::fromValue(*this));
  listOfQSettingInstances.append(QVariant::fromValue(*this));
  listOfQSettingInstances.append(QVariant::fromValue(*this));
  settings.setValue(instanceInfoKey, listOfQSettingInstances);
  settings.sync();
  // debug
  QList<QVariant> test = YUViewInstanceInfo::getYUViewInstancesInQSettings();
  YUViewInstanceInfo tmp = test.at(0).value<YUViewInstanceInfo>();
  int a = 42;
  a++;
}

void YUViewInstanceInfo::autoSavePlaylist(const QByteArray &newCompressedPlaylist)
{
  QSettings settings;
  // set new playlist
  this->compressedPlaylist = newCompressedPlaylist;

  // change the one stored
  QList<QVariant> listOfQSettingInstances = YUViewInstanceInfo::getYUViewInstancesInQSettings();
  QVariant thisInstance = QVariant::fromValue(*this);
  QMutableListIterator<QVariant> it(listOfQSettingInstances);
  while (it.hasNext())
  {
    YUViewInstanceInfo otherInstance = it.next().value<YUViewInstanceInfo>();
    if(this->uuid == otherInstance.uuid &&
       this->pid == otherInstance.pid )
    {
      // already had a instance, replace its playlist
      it.setValue(thisInstance);
      settings.setValue(instanceInfoKey, listOfQSettingInstances);
      settings.sync();
      return;
    }
  }

  // no instance yet, save new one
  // should never be reached, as we create the instance in initializeAsNewInstance,
  // thus there always should be one
  listOfQSettingInstances.append(thisInstance);
  settings.setValue(instanceInfoKey, listOfQSettingInstances);
  return;
}

//void YUViewInstanceInfo::loadAutosavedPlaylist()
//{
//  QSettings settings;

//  for( auto instance : crashedInstances)
//  {
//    // take the first in the list
//    QString playlist_name = "Autosaveplaylist-" + instance;
//    if (settings.contains(playlist_name))
//    {
//      QByteArray compressedPlaylist   = settings.value(playlist_name).toByteArray();
//      QByteArray uncompressedPlaylist = qUncompress(compressedPlaylist);
//      loadPlaylistFromByteArray(uncompressedPlaylist, QDir::current().absolutePath());
//      dropAutosavedPlaylist(instance);
//      break;
//    }

//  }
//}

void YUViewInstanceInfo::dropAutosavedPlaylist()
{
  // replace saved on with emtpy playlist
  this->autoSavePlaylist(QByteArray());
}

void YUViewInstanceInfo::removeInstanceFromQSettings()
{
  QSettings settings;

  // find and remove current instance from stored ones
  QList<QVariant> listOfQSettingInstances = YUViewInstanceInfo::getYUViewInstancesInQSettings();
  QVariant thisInstance = QVariant::fromValue(*this);
  QMutableListIterator<QVariant> it(listOfQSettingInstances);
  while (it.hasNext())
  {
    YUViewInstanceInfo otherInstance = it.next().value<YUViewInstanceInfo>();
    if(this->uuid == otherInstance.uuid &&
       this->pid == otherInstance.pid )
    {
      // found it, now remove it
      it.remove();
      settings.setValue(instanceInfoKey, listOfQSettingInstances);
      return;
    }
  }
}

YUViewInstanceInfo YUViewInstanceInfo::getAutosavedPlaylist()
{
  QSettings settings;
  QList<qint64> listOfPids = YUViewInstanceInfo::getRunningYUViewInstances();
  QList<QVariant> listOfQSettingInstances = YUViewInstanceInfo::getYUViewInstancesInQSettings();

  YUViewInstanceInfo candidateForRestore;
  QMutableListIterator<QVariant> it(listOfQSettingInstances);
  while (it.hasNext())
  {
    YUViewInstanceInfo instanceInSettings = it.next().value<YUViewInstanceInfo>();
    bool still_running = false;
    // get known instances, if instance no longer runs, remove it from QSettings
    for( auto pid_running : listOfPids )
    {
      if( pid_running == instanceInSettings.pid )  still_running = true;
    }
    if(!still_running)
    {
      // found an instance which is no longer running: candidate for restore
      candidateForRestore = instanceInSettings;
      it.remove();
    }
  }

  return candidateForRestore;
}

void YUViewInstanceInfo::cleanupRecordedInstances()
{
  QSettings settings;
  QList<qint64> listOfPids = YUViewInstanceInfo::getRunningYUViewInstances();
  QList<QVariant> listOfQSettingInstances = YUViewInstanceInfo::getYUViewInstancesInQSettings();

  QMutableListIterator<QVariant> it(listOfQSettingInstances);
  while (it.hasNext())
  {
    YUViewInstanceInfo instanceInSettings = it.next().value<YUViewInstanceInfo>();
    bool still_running = false;
    // get known instances, if instance no longer runs, remove it from QSettings
    for( auto pid_running : listOfPids )
    {
      if( pid_running == instanceInSettings.pid )  still_running = true;
    }
    if(!still_running)
    {
      // can remove this one
      it.remove();
    }
  }

  settings.setValue(instanceInfoKey, listOfQSettingInstances);
}

bool YUViewInstanceInfo::isValid()
{
  return !uuid.isNull();
}

QList<qint64> YUViewInstanceInfo::getRunningYUViewInstances()
{

  QList<qint64> listOfPids;

  // code for getting process ids adopted from https://www.qtcentre.org/threads/44489-Get-Process-ID-for-a-running-application
  // also mentions checking /proc. However there is no proc on macos, while pgrep should be available.

#if defined(Q_OS_WIN)
    // Get the list of process identifiers.
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    unsigned int i;

    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
    {
        return 0;
    }

    // Calculate how many process identifiers were returned.
    cProcesses = cbNeeded / sizeof(DWORD);

    // Search for a matching name for each process
    for (i = 0; i < cProcesses; i++)
    {
        if (aProcesses[i] != 0)
        {
            char szProcessName[MAX_PATH] = {0};

            DWORD processID = aProcesses[i];

            // Get a handle to the process.
            HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                PROCESS_VM_READ,
                FALSE, processID);

            // Get the process name
            if (NULL != hProcess)
            {
                HMODULE hMod;
                DWORD cbNeeded;

                if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
                {
                    GetModuleBaseNameA(hProcess, hMod, szProcessName, sizeof(szProcessName)/sizeof(char));
                }

                // Release the handle to the process.
                CloseHandle(hProcess);

                if (*szProcessName != 0 && strcmp(processName, szProcessName) == 0)
                {
                    listOfPids.append(QString::number(processID));
                }
            }
        }
    }

    return listOfPids.count();

#else

  // Run pgrep, which looks through the currently running processses and lists the process IDs
  // which match the selection criteria to stdout.
  QProcess process;
  process.start("pgrep",  QStringList() << "YUView");
  process.waitForReadyRead();

  QByteArray bytes = process.readAllStandardOutput();

  process.terminate();
  process.waitForFinished();
  process.kill();

  // Output is something like "2472\n2323" for multiple instances
  if (bytes.isEmpty())
      return QList<qint64>();

  // Remove trailing CR
  if (bytes.endsWith("\n"))
      bytes.resize(bytes.size() - 1);

  QStringList strListOfPids = QString(bytes).split("\n");
  for( auto strPid : strListOfPids )
  {
    listOfPids.append(strPid.toLongLong());
  }
#endif

  return  listOfPids;
}

QList<QVariant> YUViewInstanceInfo::getYUViewInstancesInQSettings()
{
  QSettings settings;
  QList<QVariant> instancesInQSettings;
  if (settings.contains(instanceInfoKey))
  {
    instancesInQSettings = settings.value(instanceInfoKey).toList();
  }
  return instancesInQSettings;
}

//const QString YUViewInstanceInfo::getKeyOfInstance()
//{
//  return this->uuid.toString() + ";" + QString::number(this->pid);
//}

//YUViewInstanceInfo YUViewInstanceInfo::fromQList(const QList<QVariant> list)
//{
//  YUViewInstanceInfo instanceInfo;
//  instanceInfo.uuid = list.at(0).toUuid();
//  instanceInfo.pid = list.at(1).toLongLong();
//  instanceInfo.compressedPlaylist = list.at(2).toByteArray();
//  return instanceInfo;
//}

//QList<QVariant> YUViewInstanceInfo::toQList()
//{
//  QList<QVariant> list;
//  list.append(this->uuid);
//  list.append(this->pid);
//  list.append(this->compressedPlaylist);
//  return list;
//}

QByteArray YUViewInstanceInfo::getCompressedPlaylist() const
{
  return compressedPlaylist;
}

//void YUViewInstanceInfo::keyToUuidAndPid(const QString key, QUuid &uuid, qint64 &pid)
//{
//  QStringList instance_uuid_and_pid =  key.split(';');
//  uuid =  QUuid(instance_uuid_and_pid.at(0));
//  QString instance_pid_str = instance_uuid_and_pid.at(1);
//  pid = instance_pid_str.toLongLong();
//}

