#include "YUViewInstanceInfo.h"
#include <QCoreApplication>
#include <QProcess>
#include <QSettings>


const QString YUViewInstanceInfo::instanceInfoKey = QString("YUViewInstances");


QDataStream& operator<<(QDataStream &ds, const YUViewInstanceInfo &inObj)
{
  ds << inObj.uuid;
  // conversion to string should not be necessary.
  // but without I get ambiguous overload for operator
  ds << QString::number(inObj.pid);
  ds << inObj.compressedPlaylist;
  return ds;
}

QDataStream& operator>>(QDataStream &ds, YUViewInstanceInfo &outObj)
{
  ds >> outObj.uuid;
  QString tmp;
  ds >> tmp;
  outObj.pid = tmp.toLongLong();
  ds >> outObj.compressedPlaylist;
  return ds;
}

QDataStream& operator<<(QDataStream &ds, const YUViewInstanceInfoList &inObj)
{
  const qint64 list_length = inObj.length();
  // conversion to string should not be necessary.
  // but without I get ambiguous overload for operator
  ds <<  QString::number(list_length);
  for( auto instance : inObj )
  {
    ds << instance;
  }
  return ds;
}

QDataStream& operator>>(QDataStream &ds, YUViewInstanceInfoList &outObj)
{
  outObj.clear();
  QString tmp;
  ds >> tmp;
  const qint64 list_length = tmp.toLongLong();
  for(unsigned i = 0; i<list_length; i++)
  {
    YUViewInstanceInfo tmp;
    ds >> tmp;
    outObj.push_back(tmp);
  }
  return ds;
}

void YUViewInstanceInfo::initializeAsNewInstance()
{
  QSettings settings;
  uuid = QUuid::createUuid();
  pid = QCoreApplication::applicationPid();
  // append instance to recorded ones
  YUViewInstanceInfoList listOfQSettingInstances = YUViewInstanceInfo::getYUViewInstancesInQSettings();
  listOfQSettingInstances.append(*this);
  settings.setValue(instanceInfoKey, QVariant::fromValue(listOfQSettingInstances));
}

void YUViewInstanceInfo::autoSavePlaylist(const QByteArray &newCompressedPlaylist)
{
  QSettings settings;
  // set new playlist
  this->compressedPlaylist = newCompressedPlaylist;

  // change the one stored
  YUViewInstanceInfoList listOfQSettingInstances = YUViewInstanceInfo::getYUViewInstancesInQSettings();
  QMutableListIterator<YUViewInstanceInfo> it(listOfQSettingInstances);
  while (it.hasNext())
  {
    YUViewInstanceInfo otherInstance = it.next();
    if(this->uuid == otherInstance.uuid &&
       this->pid == otherInstance.pid )
    {
      // already had a instance, replace its playlist
      it.setValue(*this);
      settings.setValue(instanceInfoKey, QVariant::fromValue(listOfQSettingInstances));      
      return;
    }
  }

  // no instance yet, save new one
  // should never be reached, as we create the instance in initializeAsNewInstance,
  // thus there always should be one
  listOfQSettingInstances.append(*this);
  settings.setValue(instanceInfoKey, QVariant::fromValue(listOfQSettingInstances));
  return;
}

void YUViewInstanceInfo::dropAutosavedPlaylist()
{
  // replace saved on with emtpy playlist
  this->autoSavePlaylist(QByteArray());
}

void YUViewInstanceInfo::removeInstanceFromQSettings()
{
  QSettings settings;

  // find and remove current instance from stored ones
  YUViewInstanceInfoList listOfQSettingInstances = YUViewInstanceInfo::getYUViewInstancesInQSettings();
  QMutableListIterator<YUViewInstanceInfo> it(listOfQSettingInstances);
  while (it.hasNext())
  {
    YUViewInstanceInfo otherInstance = it.next();
    if(this->uuid == otherInstance.uuid &&
       this->pid == otherInstance.pid )
    {
      // found it, now remove it
      it.remove();
      settings.setValue(instanceInfoKey, QVariant::fromValue(listOfQSettingInstances));
      return;
    }
  }
}

YUViewInstanceInfo YUViewInstanceInfo::getAutosavedPlaylist()
{
  QSettings settings;
  QList<qint64> listOfPids = YUViewInstanceInfo::getRunningYUViewInstances();
  YUViewInstanceInfoList listOfQSettingInstances = YUViewInstanceInfo::getYUViewInstancesInQSettings();

  YUViewInstanceInfo candidateForRestore;
  QMutableListIterator<YUViewInstanceInfo> it(listOfQSettingInstances);
  while (it.hasNext())
  {
    YUViewInstanceInfo instanceInSettings = it.next();
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
      settings.setValue(instanceInfoKey, QVariant::fromValue(listOfQSettingInstances));
      break; // keep playlist from potential other crashed instances for other new instances
    }
  }

  return candidateForRestore;
}

void YUViewInstanceInfo::cleanupRecordedInstances()
{
  QSettings settings;
  QList<qint64> listOfPids = YUViewInstanceInfo::getRunningYUViewInstances();
  YUViewInstanceInfoList listOfQSettingInstances = YUViewInstanceInfo::getYUViewInstancesInQSettings();

  QMutableListIterator<YUViewInstanceInfo> it(listOfQSettingInstances);
  while (it.hasNext())
  {
    YUViewInstanceInfo instanceInSettings = it.next();
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

  settings.setValue(instanceInfoKey, QVariant::fromValue(listOfQSettingInstances));
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

YUViewInstanceInfoList YUViewInstanceInfo::getYUViewInstancesInQSettings()
{
  QSettings settings;
  YUViewInstanceInfoList instancesInQSettings;
  if (settings.contains(instanceInfoKey))
  {
    instancesInQSettings = settings.value(instanceInfoKey).value<YUViewInstanceInfoList>();
  }
  return instancesInQSettings;
}

QByteArray YUViewInstanceInfo::getCompressedPlaylist() const
{
  return compressedPlaylist;
}


