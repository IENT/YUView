// YUViewUpdater.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <Strsafe.h>
#include "stdafx.h"
#include <Shlobj.h>
#include <iostream>
#include <fstream>

// Are we verbose? If yes, log to the log File.
bool verbose = false;
std::wofstream logFile;

class updateFile
{
public:
  updateFile()
  {
    fileDir[0] = 0;
    updated = false;
    previousFilePresent = false;
    oldFilePresent = false;
    status = STATUS_INIT;
  };
  bool update(bool wait, IProgressDialog *ppd)
  {
    // Get the file name without the update part at the beginning.
    TCHAR fileNameToUpdate[MAX_PATH];
    StringCchCopy(fileNameToUpdate, MAX_PATH, fileName+7);

    // Concstruct the full file path to the old file
    TCHAR filePathToUpdate[MAX_PATH];
    StringCchCopy(filePathToUpdate, MAX_PATH, fileDir);
    StringCchCat(filePathToUpdate, MAX_PATH, TEXT("\\"));
    StringCchCat(filePathToUpdate, MAX_PATH, fileNameToUpdate);

    if (verbose)
      logFile << L"Updating file " << filePathToUpdate << "\n";

    // Find the file that we want to update
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA ffd;
    hFind = FindFirstFile(filePathToUpdate, &ffd);
    previousFilePresent = (hFind != INVALID_HANDLE_VALUE);
    FindClose(hFind);

    // Set the progress dialog message
    TCHAR msg[MAX_PATH + 20];
    StringCchCopy(msg, MAX_PATH, TEXT("Updating file "));
    StringCchCat(msg, MAX_PATH, filePathToOld);
    ppd->SetLine(2, msg, true, NULL);

    // Now we start the update process. This is a multi step process where every step can go wrong:
    // 1. Rename the old file (if it exists) to "old_filename.xxx"
    // 2. Rename the update file to the actual file name.
    // 3. Delete the old_filename.xxx file (if it exists).
    // We return false if 1 or 2 fails. In this case we will revert the update.

    // Before we rename the old file, check if an old_... file already exists.
    StringCchCopy(filePathToOld, MAX_PATH, fileDir);
    StringCchCat(filePathToOld, MAX_PATH, TEXT("\\old_"));
    StringCchCat(filePathToOld, MAX_PATH, fileNameToUpdate);
    hFind = FindFirstFile(filePathToOld, &ffd);
    oldFilePresent = (hFind != INVALID_HANDLE_VALUE);
    FindClose(hFind);
    if (oldFilePresent)
    {
      // Remove the old file first.
      if (!DeleteFile(filePathToOld))
      {
        if (wait)
        {
          if (verbose)
            logFile << L"Waiting for file to be deletable " << filePathToOld << "\n";

          // We will wait until the operation succeeds
          TCHAR msg[MAX_PATH + 25];
          StringCchCopy(msg, MAX_PATH, TEXT("Waiting to delete file "));
          StringCchCat(msg, MAX_PATH, filePathToOld);
          ppd->SetLine(2, msg, true, NULL);
          do
          {
            Sleep(100);
          } while (!DeleteFile(filePathToOld) && !ppd->HasUserCancelled());

          if (ppd->HasUserCancelled())
            return false;
        }
        else
        {
          if (verbose)
          {
            logFile << L"  Could not delete file " << filePathToOld << "\n";
            printLastError();
          }
          return false;
        }
      }
      
      if (verbose)
        logFile << L"  Deleted file " << filePathToOld << "\n";
    }
    
    if (previousFilePresent)
    {
      // 1. Rename the old file. 
      if (!MoveFile(filePathToUpdate, filePathToOld))
      {
        if (wait)
        {
          if (verbose)
            logFile << L"Waiting for file to be movable " << filePathToUpdate << " to " << filePathToOld << "\n";

          // We will wait until the operation succeeds
          TCHAR msg[MAX_PATH + 25];
          StringCchCopy(msg, MAX_PATH, TEXT("Waiting to move file "));
          StringCchCat(msg, MAX_PATH, filePathToUpdate);
          ppd->SetLine(2, msg, true, NULL);
          do
          {
            Sleep(100);
          } while (!MoveFile(filePathToUpdate, filePathToOld) && !ppd->HasUserCancelled());

          if (ppd->HasUserCancelled())
            return false;
        }
        else
        {
          // Something went wrong when renaming the old file.
          if (verbose)
          {
            logFile << L"  Error renaming file " << filePathToUpdate << " to " << filePathToOld << "\n";
            printLastError();
          }
          return false;
        }
      }
      
      if (verbose)
      {
        logFile << L"  Renamed file " << filePathToUpdate << " to " << filePathToOld << "\n";
        // Now a old_ files is present
        oldFilePresent = true;
      }
    }
    status = STATUS_OLDFILERENAMED;
    
    // Step 2: Rename the update_ file ...
    TCHAR filePathToUpdateFile[MAX_PATH];
    StringCchCopy(filePathToUpdateFile, MAX_PATH, fileDir);
    StringCchCat(filePathToUpdateFile, MAX_PATH, TEXT("\\"));
    StringCchCat(filePathToUpdateFile, MAX_PATH, fileName);
    if (!MoveFile(filePathToUpdateFile, filePathToUpdate))
    {
      if (wait)
      {
        if (verbose)
          logFile << L"Waiting for file to be movable " << filePathToUpdateFile << " to " << filePathToUpdate << "\n";

        // We will wait until the operation succeeds
        TCHAR msg[MAX_PATH + 25];
        StringCchCopy(msg, MAX_PATH, TEXT("Waiting to move file "));
        StringCchCat(msg, MAX_PATH, filePathToUpdateFile);
        ppd->SetLine(2, msg, true, NULL);
        do
        {
          Sleep(100);
        } while (!MoveFile(filePathToUpdateFile, filePathToUpdate) && !ppd->HasUserCancelled());

        if (ppd->HasUserCancelled())
          return false;
      }
      else
      {
        // Something went wrong when renaming the update_ file.
        if (verbose)
        {
          logFile << L"  Error renaming file " << filePathToUpdateFile << " to " << filePathToUpdate << "\n";
          printLastError();
        }
        return false;
      }
    }
    
    if (verbose)
      logFile << L"  Renamed file " << filePathToUpdateFile << " to " << filePathToUpdate << "\n";

    status = STATUS_UPDATEFILERENAMED;
    return true;
  }

  bool cleanBackupedFiles(bool wait, IProgressDialog *ppd)
  {
    if (!oldFilePresent)
      return true;

    if (verbose)
      logFile << L"Deleings file " << filePathToOld << "\n";

    // Update the progress bar info text
    TCHAR msg[MAX_PATH + 15];
    StringCchCopy(msg, MAX_PATH, TEXT("Claning file  "));
    StringCchCat(msg, MAX_PATH, filePathToOld);
    ppd->SetLine(2, msg, true, NULL);

    // Step 3: Remove the old file, which is now present because we created it.
    if (!DeleteFile(filePathToOld))
    {
      if (wait)
      {
        if (verbose)
          logFile << L"Waiting for file to be deletable " << filePathToOld << "\n";

        // We will wait until the operation succeeds
        TCHAR msg[MAX_PATH + 25];
        StringCchCopy(msg, MAX_PATH, TEXT("Waiting to delete file "));
        StringCchCat(msg, MAX_PATH, filePathToOld);
        ppd->SetLine(2, msg, true, NULL);
        do
        {
          Sleep(100);
        } while (!DeleteFile(filePathToOld) && !ppd->HasUserCancelled());

        if (ppd->HasUserCancelled())
          return false;
      }
      else 
      {
        if (verbose)
        {
          logFile << L"  Error deleting file " << filePathToOld << "\n";
          printLastError();
        }
        return false;
      }
    }
    
    if (verbose)
      logFile << L"  Deleted file " << filePathToOld << "\n";

    status = STATUS_OLDFILEDELETED;
    return true;
  }

  bool revertUpdate()
  {
    // Undo what we have done (as good as possible)
    if (status == STATUS_OLDFILEDELETED)
    {
      if (previousFilePresent)
      {
        // We deleted the old file which was there before. This should not happen.
        return false;
      }
      status = STATUS_UPDATEFILERENAMED;
    }

    // Get the file name without the update part at the beginning.
    TCHAR fileNameToUpdate[MAX_PATH];
    StringCchCopy(fileNameToUpdate, MAX_PATH, fileName+7);
    // Concstruct the full file path to the old file
    TCHAR filePathToUpdate[MAX_PATH];
    StringCchCopy(filePathToUpdate, MAX_PATH, fileDir);
    StringCchCat(filePathToUpdate, MAX_PATH, TEXT("\\"));
    StringCchCat(filePathToUpdate, MAX_PATH, fileNameToUpdate);

    TCHAR filePathToUpdateFile[MAX_PATH];
    StringCchCopy(filePathToUpdateFile, MAX_PATH, fileDir);
    StringCchCat(filePathToUpdateFile, MAX_PATH, TEXT("\\"));
    StringCchCat(filePathToUpdateFile, MAX_PATH, fileName);

    if (status == STATUS_UPDATEFILERENAMED)
    {
      // We renamed the update_ file to the normal file name. Undo this.
      if (!MoveFile(filePathToUpdate, filePathToUpdateFile))
      {
        // Something went wrong when renaming the update_ file.
        if (verbose)
        {
          logFile << L"  Revert error renaming file " << filePathToUpdate << " to " << filePathToUpdateFile << "\n";
          printLastError();
        }
        return false;
      }
      else if (verbose)
        logFile << L"  Revert renamed file " << filePathToUpdate << " to " << filePathToUpdateFile << "\n";

      status = STATUS_OLDFILERENAMED;
    }

    if (status == STATUS_OLDFILERENAMED)
    {
      // We renamed the preciously present file to old_. Undo this.
      if (!MoveFile(filePathToOld, filePathToUpdate))
      {
        // Something went wrong when renaming the old file.
        if (verbose)
        {
          logFile << L"  Revert error renaming file " << filePathToOld << " to " << filePathToUpdate << "\n";
          printLastError();
        }
        return false;
      }
      else if (verbose)
        logFile << L"  Revert renamed file " << filePathToOld << " to " << filePathToUpdate << "\n";
    }
    // We undid the update as much as possible
    return true;
  }

  void printLastError()
  {
    WCHAR buf[256];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, NULL);
    logFile << L"  Error Code " << GetLastError() << "\n";
    logFile << L"  Error message: " << buf << "\n";
  }
  TCHAR fileDir[MAX_PATH];
  TCHAR fileName[MAX_PATH];
  TCHAR filePathToOld[MAX_PATH];
  bool updated;
  bool previousFilePresent;
  bool oldFilePresent;      // Is a file with old_ of this update file present (now)?
  enum updateStatus
  {
    STATUS_INIT,              // Nothing was done yet
    STATUS_OLDFILERENAMED,    // The old file was renamed
    STATUS_UPDATEFILERENAMED, // The update_ file was successfully renamed
    STATUS_OLDFILEDELETED,    // The old file was successfully deleted. We are done.
  };
  updateStatus status;
};

// This is the maximum number of files that we can update in one update
#define MAX_FILES 100

class fileUpdater
{
public:
  fileUpdater() : numFiles(0) {}

  void findFilesInDirectoryRecursive(TCHAR curDir[], bool cleanupOnly=false)
  {
    if (verbose)
      logFile << L"Find files in dir: " << curDir << "\n";

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
      if (verbose)
        logFile << L"No file found.\n";
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
        findFilesInDirectoryRecursive(fullDir);
      }
      else
      {
        if (cleanupOnly)
        {
          // We are only performing cleanup. We are only looking for files that start with "old_"
          // so that we can clean them up.
          if (_wcsnicmp(ffd.cFileName, TEXT("old_"), 4) == 0)
          {
            // Add the file to the file list
            if (verbose)
              logFile << L"Adding cleanup File: " << fullDir << "\n";
            // Get the filename withoug the "old_" part
            StringCchCopy(fileList[numFiles].fileDir, MAX_PATH, curDir);
            StringCchCopy(fileList[numFiles].fileName, MAX_PATH, ffd.cFileName+4);
            StringCchCopy(fileList[numFiles].filePathToOld, MAX_PATH, fullDir);
            fileList[numFiles].oldFilePresent = true;
            numFiles++;
          }
        }
        else
        {
          // Normal update process. We are looking for files that start with "update_".
          if (_wcsnicmp(ffd.cFileName, TEXT("update_"), 7) == 0)
          {
            // Add the file to the file list
            if (verbose)
              logFile << L"Adding update File: " << fullDir << "\n";
            StringCchCopy(fileList[numFiles].fileDir, MAX_PATH, curDir);
            StringCchCopy(fileList[numFiles].fileName, MAX_PATH, ffd.cFileName);
            numFiles++;
          }
        }
      }
    }
    while (FindNextFile(hFind, &ffd) != 0 && numFiles < MAX_FILES);
    FindClose(hFind);
  }
  bool updateFiles(bool wait, IProgressDialog *ppd)
  {
    // We now have a list of all files. Update them.
    // If something goes wrong, revert the update.
    for (int i = 0; i < numFiles; i++)
    {
      ppd->SetProgress(i, numFiles*2);

      if (!fileList[i].update(wait, ppd))
        return false;
    }
    return true;
  }
  bool cleanBackupedFiles(bool wait, IProgressDialog *ppd)
  {
    // Cleanup the old_ files from the old version
    for (int i = 0; i < numFiles; i++)
    {
      ppd->SetProgress(i+numFiles, numFiles*2);
      if (!fileList[i].cleanBackupedFiles(wait, ppd))
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
private:
  // Fill a list with files that we will update
  updateFile fileList[MAX_FILES];
  // How many files are in the list?
  int numFiles;
};

void startElevated(bool cleanupStep)
{
  //Let's try to launch it again with elevated rights.
  TCHAR fullPathToExe[MAX_PATH];
  GetModuleFileName(NULL, fullPathToExe, MAX_PATH);

  // This should trigger the UAC dialog to start the application with elevated rights.
  // The "updateElevated" parameter tells the new instance of YUView that it should have elevated rights now
  // and it should retry to update.
  HINSTANCE h;
  if (cleanupStep)
  {
    if (verbose)
      h = ShellExecute(nullptr, L"runas", fullPathToExe, L"-cv", nullptr, SW_SHOWNORMAL);
    else
      h = ShellExecute(nullptr, L"runas", fullPathToExe, L"-c", nullptr, SW_SHOWNORMAL);
  }
  else
  {
    if (verbose)
      h = ShellExecute(nullptr, L"runas", fullPathToExe, L"-uv", nullptr, SW_SHOWNORMAL);
    else
      h = ShellExecute(nullptr, L"runas", fullPathToExe, L"-u", nullptr, SW_SHOWNORMAL);
  }
  INT_PTR retVal = (INT_PTR)h;
  if (retVal > 32)  // From MSDN: If the function succeeds, it returns a value greater than 32.
  {
    // The user allowed restarting YUView as admin. Quit this one. The other one will take over.
    return;
  }
  else
  {
    DWORD err = GetLastError();
    if (err == ERROR_CANCELLED)
    {
      // The user did not allow the updater to run with elevated rights.
      if (verbose)
        logFile << L"The user did not allow to run the updater with elevated rights.\n";
      return;
    }
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
  verbose = (lpCmdLine[0] == '-' && (lpCmdLine[1] == 'v' || ((lpCmdLine[1] == 'u' || lpCmdLine[1] == 'c') && lpCmdLine[2] == 'v')));
  bool cleanup = (lpCmdLine[0] == '-' && lpCmdLine[1] == 'c');
  bool elevated = cleanup || (lpCmdLine[0] == '-' && lpCmdLine[1] == 'u');

  // We will write the log to a file (if verbose)
  if (verbose)
  {
    if (elevated)
      logFile.open ("update_elevated.log");
    else
      logFile.open ("update.log");
  }
 
  if (verbose)
    logFile << L"Starting updater in verbose mode.\n";
  if (elevated)
    logFile << L"Starting updater in elevated mode.\n";

  // Now we start the update process. Every file that starts with _update_ will replace it's
  // counterpart. If a file already exists, we will try to delete it. If deleting it does not work,
  // we will wait for it to work.

  // Create a file updater and start parsing the current directory for files that need updating.
  fileUpdater updater;

  // DEBUG
  // Try to create a progress dialog
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
  if (verbose)
    logFile << L"Current dir: " << curDir << "\n";

  if (!cleanup)
  {
    // Start updating 
    ppd->SetLine(2, L"Looking for files to update...", false, NULL);
    updater.findFilesInDirectoryRecursive(curDir);

    // Start the update process
    if (!updater.updateFiles(elevated, ppd))
    {
      if (ppd->HasUserCancelled())
      {
        if (verbose)
          logFile << L"Update process was canceled by the user.\n";
        if (verbose)
          logFile.close();
        return 0;
      }

      // The update process failed. 
      if (elevated)
      {
        // The update failed, but we already have elevated rights.
        if (verbose)
          logFile << L"Update process failed even with elevated rights.\n";
        // Undo the update and quit.
        updater.revertUpdate();
        if (verbose)
          logFile.close();
        return 0;
      }
      else if (verbose)
        logFile << L"Update process failed. Let's try it with elevated rights.\n";
    
      startElevated(false);
    }
    else
    {
      // The update was successfull. Clean all the old_ files
      if (!updater.cleanBackupedFiles(elevated, ppd))
      {
        // The cleaning process failed
        if (elevated)
        {
          // The cleaning process failed with elevated rights. Nothing we can do.
          if (verbose)
            logFile << L"Cleanup process failed even with elevated rights.\n";
          if (verbose)
            logFile.close();
          return 0;
        }
        else if (verbose)
          logFile << L"Cleanup process failed. Let's try it with elevated rights.\n";

        startElevated(true);
      }
    }
  }
  else
  {
    // The update was successfull but the cleanup is retried with elevated rights
    updater.findFilesInDirectoryRecursive(curDir, true);
    if (!updater.cleanBackupedFiles(elevated, ppd))
    {
      // The cleaning process failed
      if (verbose)
        logFile << L"Cleanup process failed even with elevated rights.\n";
      if (verbose)
        logFile.close();
      return 0;
    }
  }
  


  if (verbose)
  {
    logFile << L"Done.\n";
    logFile.close();
  }
  return 0;
}
