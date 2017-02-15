/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*
*
*   ####### The YUView Updater ######
*
*   This is the windows updater executable. It has one very simple task: 
*   Update the existing files in one directory and it's subdirectories.
*   
*   When called, it will look for files with the suffix ".update". For all files it will then:
*   1. rename the already present corresponding file to ".old" (if it exists)
*   2. rename the ".update" file to the original file name
*   3. delete the ".old" files.
*   
*   If this does not work with normal user rights, the program can restart itself with elevated rights.
*   It will also wait for the old files to be accessible if they are currently still used. 
*   
*   If everything worked successfully, it will restart YUView.
*   
*   The executable is statically linked and should run on all systems.
*/

#include <windows.h>
#include <Strsafe.h>
#include "stdafx.h"
#include <Shlobj.h>
#include <iostream>
#include <fstream>
#include <cstdarg>

class logFile
{
public:
  logFile() { fileOpened = false; }
  ~logFile() { if (fileOpened) logFileStream.close(); }
  void openLogFile(LPSTR name)
  {
    logFileStream.open(name);
    fileOpened = true;
  }
  template<typename T> void log(const T& c) { if (fileOpened) logFileStream << c; }
  template<typename T1, typename T2> void log(const T1& c1, const T2& c2) { if (fileOpened) logFileStream << c1 << c2; }
  template<typename T1, typename T2> void logN(const T1& c1, const T2& c2) { if (fileOpened) logFileStream << c1 << c2 << "\n"; }
  template<typename T1, typename T2, typename T3, typename T4> 
  void logN(const T1& c1, const T2& c2, const T3& c3, const T4& c4) { if (fileOpened) logFileStream << c1 << c2 << c3 << c4 << "\n"; }
  void logLastError()
  {
    WCHAR buf[256];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, NULL);
    logFileStream << L"  Error Code " << GetLastError() << "\n";
    logFileStream << L"  Error message: " << buf << "\n";
  }
private:
  std::wofstream logFileStream;
  bool fileOpened;
};

// Log messages to this log (if verbose)
logFile l;
// Was this programm launched with elevated rights?
bool elevated;

class updateFile
{
public:
  updateFile()
  {
    fileDir[0] = 0;
    updated = false;
    status = STATUS_INIT;
  };
  bool update(IProgressDialog *ppd)
  {
    // Concstruct the full file path to the file that we want to update
    TCHAR filePath[MAX_PATH];
    StringCchCopy(filePath, MAX_PATH, fileDir);
    StringCchCat(filePath, MAX_PATH, TEXT("\\"));
    StringCchCat(filePath, MAX_PATH, fileName);

    // Check if the file to update ("filename.xxx") exists
    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(filePath, &ffd);
    filePresent = (hFind != INVALID_HANDLE_VALUE);
    FindClose(hFind);

    // Construct the full path to the ".old" file
    TCHAR filePathToOld[MAX_PATH];
    StringCchCopy(filePathToOld, MAX_PATH, filePath);
    StringCchCat(filePathToOld, MAX_PATH, TEXT(".old"));

    // Check if an ".old" file already exists.
    hFind = FindFirstFile(filePathToOld, &ffd);
    bool fileOldPresent = (hFind != INVALID_HANDLE_VALUE);
    FindClose(hFind);

    // Construct the full path to the ".update" file
    TCHAR filePathToUpdate[MAX_PATH];
    StringCchCopy(filePathToUpdate, MAX_PATH, filePath);
    StringCchCat(filePathToUpdate, MAX_PATH, TEXT(".update"));
    
    l.logN("Updating file ", filePath);

    // Set the progress dialog message
    TCHAR msg[MAX_PATH + 20];
    StringCchCopy(msg, MAX_PATH, TEXT("Updating file "));
    StringCchCat(msg, MAX_PATH, fileName);
    ppd->SetLine(2, msg, true, NULL);

    // Now we start the update process. This is a multi step process where every step can go wrong:
    // 1. If it exists, remove the file "filename.xxx.old"
    // 1. If it exists, rename the file to update ("filename.xxx") to "filename.xxx.old"
    // 2. Rename the "filename.xxx.update" file to "filename.xxx".
    // We return false if any step fails. In this case we will revert the update (and possibly retry with elevated rights)

    
    if (filePresent && fileOldPresent)
    {
      // The file to update ("filename.xxx") exists and an old file ("filename.xxx.old") also exists.
      // We MUST delete the ".old" file before we perform the update. The update will fail otherwise.
      if (!DeleteFile(filePathToOld))
      {
        if (elevated)
        {
          l.logN("  Waiting for file to be deletable ", filePathToOld);

          // We will wait until the operation succeeds
          TCHAR msg[MAX_PATH + 25];
          StringCchCopy(msg, MAX_PATH, TEXT("Waiting to delete file "));
          StringCchCat(msg, MAX_PATH, filePathToOld);
          ppd->SetLine(2, msg, true, NULL);
          ppd->SetLine(3, L"Please close all running instances of YUView.", true, NULL);
          do
          {
            Sleep(100);
          } while (!DeleteFile(filePathToOld) && !ppd->HasUserCancelled());

          ppd->SetLine(3, L"", true, NULL);
          if (ppd->HasUserCancelled())
            return false;
        }
        else
        {
          l.logN("  Could not delete file ", filePathToOld);
          l.logLastError();
          return false;
        }
      }
      
      l.logN("  Deleted file ", filePathToOld);
    }
    
    if (filePresent)
    {
      // The file to update ("filename.xxx") exists. Rename it to "filename.xxx.old".
      if (!MoveFile(filePath, filePathToOld))
      {
        if (elevated)
        {
          l.logN("  Waiting for file to be movable ", filePath, " to ", filePathToOld);

          // We will wait until the operation succeeds
          TCHAR msg[MAX_PATH + 25];
          StringCchCopy(msg, MAX_PATH, TEXT("Waiting to move file "));
          StringCchCat(msg, MAX_PATH, filePath);
          ppd->SetLine(3, L"Please close all running instances of YUView.", true, NULL);
          ppd->SetLine(2, msg, true, NULL);
          do
          {
            Sleep(100);
          } while (!MoveFile(filePath, filePathToOld) && !ppd->HasUserCancelled());

          ppd->SetLine(3, L"", true, NULL);
          if (ppd->HasUserCancelled())
            return false;
        }
        else
        {
          // Something went wrong when renaming the old file.
          l.logN("  Error renaming file ", filePath, " to ", filePathToOld);
          l.logLastError();
          return false;
        }
      }
      
      l.logN("  Renamed file ", filePath, " to ", filePathToOld);
    }
    status = STATUS_OLDFILERENAMED;
    
    // Step 2: Rename the "filename.xxx.update" file to "filename.xxx".
    if (!MoveFile(filePathToUpdate, filePath))
    {
      if (elevated)
      {
        l.logN("Waiting for file to be movable ", filePathToUpdate, " to ", filePath);

        // We will wait until the operation succeeds
        TCHAR msg[MAX_PATH + 25];
        StringCchCopy(msg, MAX_PATH, TEXT("Waiting to move file "));
        StringCchCat(msg, MAX_PATH, filePathToUpdate);
        ppd->SetLine(2, msg, true, NULL);
        ppd->SetLine(3, L"Please close all running instances of YUView.", true, NULL);
        do
        {
          Sleep(100);
        } while (!MoveFile(filePathToUpdate, filePath) && !ppd->HasUserCancelled());

        ppd->SetLine(3, L"", true, NULL);
        if (ppd->HasUserCancelled())
          return false;
      }
      else
      {
        // Something went wrong when renaming the update_ file.
        l.logN("  Error renaming file ", filePathToUpdate, " to ", filePath);
        l.logLastError();
        return false;
      }
    }
    
    l.logN("  Renamed file ", filePathToUpdate, " to ", filePath);

    status = STATUS_UPDATEFILERENAMED;
    return true;
  }

  bool deleteOldFile(IProgressDialog *ppd)
  {
    l.logN("Deleing file ", fileName);

    // Update the progress bar info text
    TCHAR msg[MAX_PATH + 15];
    StringCchCopy(msg, MAX_PATH, TEXT("Claning file  "));
    StringCchCat(msg, MAX_PATH, fileName);
    ppd->SetLine(2, msg, true, NULL);

    // Get the full path to the file to delete
    TCHAR filePathToOld[MAX_PATH];
    StringCchCopy(filePathToOld, MAX_PATH, fileDir);
    StringCchCat(filePathToOld, MAX_PATH, TEXT("\\"));
    StringCchCat(filePathToOld, MAX_PATH, fileName);

    // Step 3: Remove the old file, which is now present because we created it.
    if (!DeleteFile(filePathToOld))
    {
      if (elevated)
      {
        l.logN("Waiting for file to be deletable ", filePathToOld);

        // We will wait until the operation succeeds
        TCHAR msg[MAX_PATH + 25];
        StringCchCopy(msg, MAX_PATH, TEXT("Waiting to delete file "));
        StringCchCat(msg, MAX_PATH, filePathToOld);
        ppd->SetLine(2, msg, true, NULL);
        ppd->SetLine(3, L"Please close all running instances of YUView.", true, NULL);
        do
        {
          Sleep(100);
        } while (!DeleteFile(filePathToOld) && !ppd->HasUserCancelled());

        ppd->SetLine(3, L"", true, NULL);
        if (ppd->HasUserCancelled())
          return false;
      }
      else 
      {
        l.logN("  Error deleting file ", filePathToOld);
        l.logLastError();
        return false;
      }
    }
    l.logN("  Deleted file ", filePathToOld);
    return true;
  }

  // Undo what we have done (as good as possible)
  bool revertUpdate()
  {
    // Concstruct the full file path to the file that we want to update
    TCHAR filePath[MAX_PATH];
    StringCchCopy(filePath, MAX_PATH, fileDir);
    StringCchCat(filePath, MAX_PATH, TEXT("\\"));
    StringCchCat(filePath, MAX_PATH, fileName);

    if (status == STATUS_UPDATEFILERENAMED)
    {
      // We renamed the update_ file to the normal file name. Undo this.
      
      // Construct the full path to the ".update" file
      TCHAR filePathToUpdate[MAX_PATH];
      StringCchCopy(filePathToUpdate, MAX_PATH, filePath);
      StringCchCat(filePathToUpdate, MAX_PATH, TEXT(".update"));

      if (!MoveFile(filePath, filePathToUpdate))
      {
        // Something went wrong when renaming the update_ file.
        l.logN("  Revert error renaming file ", filePath, " to ", filePathToUpdate);
        l.logLastError();
        return false;
      }
      else
        l.logN("  Revert renamed file ", filePath, " to ", filePathToUpdate);

      status = STATUS_OLDFILERENAMED;
    }

    if (status == STATUS_OLDFILERENAMED && filePresent)
    {
      // We renamed the preciously present file to .old Undo this.

      // Construct the full path to the ".old" file
      TCHAR filePathToOld[MAX_PATH];
      StringCchCopy(filePathToOld, MAX_PATH, filePath);
      StringCchCat(filePathToOld, MAX_PATH, TEXT(".old"));

      if (!MoveFile(filePathToOld, filePath))
      {
        // Something went wrong when renaming the old file.
        l.logN("  Revert error renaming file ", filePathToOld, " to ", filePath);
        l.logLastError();
        return false;
      }
      else
        l.logN("  Revert renamed file ", filePathToOld, " to ", filePath);
    }
    // We undid the update as much as possible
    return true;
  }

  TCHAR fileDir[MAX_PATH];
  TCHAR fileName[MAX_PATH];
  // Is the file to update present (the "filename.xxx" file without .old or .update)?
  bool filePresent;
  bool updated;
  enum updateStatus
  {
    STATUS_INIT,              // Nothing was done yet
    STATUS_OLDFILERENAMED,    // The old file was renamed
    STATUS_UPDATEFILERENAMED  // The update_ file was successfully renamed
  };
  updateStatus status;
};

// This is the maximum number of files that we can update in one update
#define MAX_FILES 100

class fileUpdater
{
public:
  fileUpdater()
  {
    numFiles = 0;
    updaterUpdateFilePresent = false;
    fileListUpdateFilePresent = false;
  }

  void findFilesInDirectoryRecursive(TCHAR curDir[], bool cleanupOnly=false, bool rootDir=true)
  {
    l.logN("Find files in dir: ", curDir);

    // Append '\*' to the directory name
    TCHAR searchDir[MAX_PATH];
    StringCchCopy(searchDir, MAX_PATH, curDir);
    StringCchCat(searchDir, MAX_PATH, TEXT("\\*"));
    
    // Find the first file
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA ffd;
    hFind = FindFirstFile(searchDir, &ffd);

    if (hFind == INVALID_HANDLE_VALUE)
    {
      l.log("No file found.\n");
      return;
    } 

    do
    {
      // Ignore "." and ".." entries.
      if (wcscmp(ffd.cFileName, TEXT(".")) == 0 || wcscmp(ffd.cFileName, TEXT("..")) == 0)
        continue;

      // Get the full file/directory path
      TCHAR fullDir[MAX_PATH];
      StringCchCopy(fullDir, MAX_PATH, curDir);
      StringCchCat(fullDir, MAX_PATH, TEXT("\\"));
      StringCchCat(fullDir, MAX_PATH, ffd.cFileName);

      if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
        // This is a directory. Parse all files in this directory.
        findFilesInDirectoryRecursive(fullDir, cleanupOnly, false);
      }
      else
      {
        int nameLen = wcsnlen(ffd.cFileName, MAX_PATH);
        if (cleanupOnly)
        {
          // We are only performing cleanup. We are only looking for files that end with ".old"
          // so that we can clean them up.
          if (nameLen > 4 && _wcsnicmp(ffd.cFileName + nameLen - 4, TEXT(".old"), 4) == 0)
          {
            // Add the file to the file list
            l.logN("Adding cleanup File: ", fullDir);
            // Save the filenames with the .old part
            StringCchCopy(fileList[numFiles].fileDir, MAX_PATH, curDir);
            StringCchCopy(fileList[numFiles].fileName, MAX_PATH, ffd.cFileName);
            numFiles++;
          }
        }
        else
        {
          // Normal update process. We are looking for files that end with ".update"
          // Save the file name without the ".update" extension
          if (_wcsnicmp(ffd.cFileName, TEXT("YUViewUpdater.exe.update"), 24) == 0 && rootDir)
          {
            // This is a new version of the updater itself. Add the file as the updater file.
            // This file is updated first.
            l.logN("Adding updater update File: ", fullDir);
            StringCchCopy(updaterUpdateFile.fileDir, MAX_PATH, curDir);
            StringCchCopy(updaterUpdateFile.fileName, MAX_PATH, TEXT("YUViewUpdater.exe"));
            updaterUpdateFilePresent = true;
          }
          if (_wcsnicmp(ffd.cFileName, TEXT("FileUpdateList.txt.update"), 25) == 0 && rootDir)
          {
            // This is a new version of the FileUpdateList. Add the file as the fileUpdateList file.
            // This file is updated last.
            l.logN("Adding updater update File: ", fullDir);
            StringCchCopy(fileListUpdate.fileDir, MAX_PATH, curDir);
            StringCchCopy(fileListUpdate.fileName, MAX_PATH, TEXT("FileUpdateList.txt"));
            fileListUpdateFilePresent = true;
          }
          else if (nameLen > 7 && _wcsnicmp(ffd.cFileName + nameLen - 7, TEXT(".update"), 7) == 0)
          {
            // Add the file to the file list
            l.logN("Adding update File: ", fullDir);
            StringCchCopy(fileList[numFiles].fileDir, MAX_PATH, curDir);
            StringCchCopy(fileList[numFiles].fileName, nameLen-6, ffd.cFileName);
            numFiles++;
          }
        }
      }
    }
    while (FindNextFile(hFind, &ffd) != 0 && numFiles < MAX_FILES);
    FindClose(hFind);
  }
  bool updateFiles(IProgressDialog *ppd)
  {
    // We now have a list of all files. Update them.
    // If something goes wrong, revert the update.
    l.logN("Update all files: #", numFiles);
    for (int i = 0; i < numFiles; i++)
    {
      ppd->SetProgress(i, numFiles*2);

      if (!fileList[i].update(ppd))
        return false;
    }
    return true;
  }
  bool updateUpdater(IProgressDialog *ppd)
  {
    if (!updaterUpdateFilePresent)
      return true;
    l.log("Update the updater...\n");
    return updaterUpdateFile.update(ppd);
  }
  bool updateFileListTxt(IProgressDialog *ppd)
  {
    if (!fileListUpdateFilePresent)
      return true;
    l.log("Update the FileUpdateList.txt file ...\n");
    return fileListUpdate.update(ppd);
  }
  bool cleanBackupedFiles(IProgressDialog *ppd)
  {
    // Cleanup the ".old" files from the old version
    l.logN("Cleanup the old files: #", numFiles);
    for (int i = 0; i < numFiles; i++)
    {
      ppd->SetProgress(i+numFiles, numFiles*2);
      if (!fileList[i].deleteOldFile(ppd))
        return false;
    }
    return true;
  }
  void revertUpdate()
  {
    // Revert the update (as good as possible)
    for (int i = 0; i < numFiles; i++)
      fileList[i].revertUpdate();
  }
  bool updateForUpdater() { return updaterUpdateFilePresent; }
private:
  // Fill a list with files that we will update
  updateFile fileList[MAX_FILES];
  // The updater itself is one of the possible update files:
  bool updaterUpdateFilePresent;
  updateFile updaterUpdateFile;
  bool fileListUpdateFilePresent;
  updateFile fileListUpdate;
  // How many files are in the list?
  int numFiles;
};

void restartUpdater(bool verbose, bool elevated)
{
  //Let's try to launch it again with elevated rights.
  TCHAR fullPathToExe[MAX_PATH];
  GetModuleFileName(NULL, fullPathToExe, MAX_PATH);

  l.log("Starting the updater with elevated rights\n");

  // This should trigger the UAC dialog to start the application with elevated rights.
  // The "updateElevated" parameter tells the new instance of YUView that it should have elevated rights now
  // and it should retry to update.
  HINSTANCE h;
  if (elevated)
  {
    if (verbose)
      h = ShellExecute(nullptr, L"runas", fullPathToExe, L"-uv", nullptr, SW_SHOWNORMAL);
    else
      h = ShellExecute(nullptr, L"runas", fullPathToExe, L"-u", nullptr, SW_SHOWNORMAL);
  }
  else
  {
    if (verbose)
      h = ShellExecute(nullptr, L"open", fullPathToExe, L"-v", nullptr, SW_SHOWNORMAL);
    else
      h = ShellExecute(nullptr, L"open", fullPathToExe, L"-", nullptr, SW_SHOWNORMAL);
  }
  INT_PTR retVal = (INT_PTR)h;
  if (retVal < 32)  // From MSDN: If the function succeeds, it returns a value greater than 32.
  {
    DWORD err = GetLastError();
    if (err == ERROR_CANCELLED)
      // The user did not allow the updater to run with elevated rights.
      l.log("The user did not allow to run the updater with elevated rights.\n");
    else
      l.log("Unknown error when restarting the updater: ", err);
  }
}

int WINAPI WinMain(
  HINSTANCE   hInstance,
  HINSTANCE   hPrevInstance,
  LPSTR       lpCmdLine,
  int         nCmdShow
  )
{
  // We can tell the application to be verbose...
  bool verbose = (lpCmdLine[0] == '-' && (lpCmdLine[1] == 'v' || ((lpCmdLine[1] == 'u') && lpCmdLine[2] == 'v')));
  elevated = lpCmdLine[0] == '-' && lpCmdLine[1] == 'u';
  
  // We will write the log to a file (if verbose)
  if (verbose)
  {
    if (elevated)
      l.openLogFile("update_elevated.log");
    else
      l.openLogFile("update.log");
  }
 
  if (verbose)
    l.log("Verbose mode activated.\n");
  if (elevated)
    l.log("Elevated mode.\n");

  // Now we start the update process. Every file that starts with _update_ will replace it's
  // counterpart. If a file already exists, we will try to delete it. If deleting it does not work,
  // we will wait for it to work.

  // Create a progress dialog
  CoInitialize(NULL);
  IProgressDialog *ppd;
  HRESULT res = CoCreateInstance(CLSID_ProgressDialog, NULL, CLSCTX_INPROC_SERVER, IID_IProgressDialog, (void **)&ppd);
  if (ppd)
  {
    ppd->SetTitle(L"Updating YUView...");
    ppd->SetCancelMsg(L"Do you really want to cancel the update?", NULL);
    ppd->StartProgressDialog(NULL, NULL, PROGDLG_NOTIME, NULL);
    ppd->SetProgress(0, 100);
    ppd->SetLine(1, L"Applying update", false, NULL);
  }

  // Get the current directory
  TCHAR curDir[MAX_PATH];
  GetCurrentDirectory(MAX_PATH, curDir);
  l.logN("Current dir: ", curDir);

  {
    // Step 1: Update files
    ppd->SetLine(2, L"Looking for files to update...", false, NULL);
    fileUpdater updater;
    updater.findFilesInDirectoryRecursive(curDir);

    // The first file that we update is the updater itself
    if (updater.updateForUpdater())
    {
      // There is an update for the updater itself. Do this first.
      if (!updater.updateUpdater(ppd))
      {
        if (ppd->HasUserCancelled())
        {
          l.log("Update process was canceled by the user.\n");
          return 0;
        }

        if (elevated)
          // The update failed, but we already have elevated rights.
          l.log("Updating the updater failed even with elevated rights.\n");
        else
          // Try again with elevated rights.
          restartUpdater(verbose, true);
        return 0;
      }

      // The updater itself was updated. Restart it!
      restartUpdater(verbose, elevated);
      return 0;
    }

    // Start the update process for all the other files
    if (!updater.updateFiles(ppd))
    {
      if (ppd->HasUserCancelled())
      {
        l.log("Update process was canceled by the user.\n");
        return 0;
      }

      if (elevated)
      {
        // The update failed, but we already have elevated rights.
        l.log("Update process failed even with elevated rights.\n");
        // Undo the update and quit.
        updater.revertUpdate();
      }
      else
        restartUpdater(verbose, true);
      return 0;
    }

    // Lastly, update the UpdateFileList.txt
    if (!updater.updateFileListTxt(ppd))
    {
      if (elevated)
        // The update failed, but we already have elevated rights.
        l.log("Update process of the UpdateFileList.txt file failed even with elevated rights.\n");
      else
        restartUpdater(verbose, true);
      return 0;
    }
  }
  
  // Step 2: Look for all ".old" files and clean them up.
  {
    // The update was successfull but the cleanup is retried with elevated rights
    fileUpdater updater;
    updater.findFilesInDirectoryRecursive(curDir, true);
    if (!updater.cleanBackupedFiles(ppd))
    {
      if (ppd->HasUserCancelled())
      {
        l.log("Cleanup process was canceled by the user.\n");
        return 0;
      }

      if (elevated)
        // The update failed, but we already have elevated rights.
        l.log("Update process failed even with elevated rights.\n");
      else
        restartUpdater(verbose, true);
      return 0;
    }
  }
  
  l.log("Done. Restarting YUView.exe.\n");
  
  // Find the YUView.exe file
  TCHAR yuviewDir[MAX_PATH];
  StringCchCopy(yuviewDir, MAX_PATH, curDir);
  StringCchCat(yuviewDir, MAX_PATH, TEXT("\\YUView.exe"));
  HANDLE hFind = INVALID_HANDLE_VALUE;
  WIN32_FIND_DATA ffd;
  hFind = FindFirstFile(yuviewDir, &ffd);
  if (hFind != INVALID_HANDLE_VALUE)
    // Launch YUView
    HINSTANCE h = ShellExecute(nullptr, L"open", yuviewDir, L"", nullptr, SW_SHOWNORMAL);
  FindClose(hFind);

  return 0;
}
