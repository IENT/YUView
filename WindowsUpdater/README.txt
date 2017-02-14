This is the windows updater executable. It has one very simple task: 
Update the existing files in one directory and it's subdirectories.

When called, it will look for files that start with "update_". For all files it will then:
1. rename the old file to "old_"
2. rename the "update_" file to the original file name
3. delete the "old_" file. 

If this does not work with normal user rights, the program can restart itself with elevated rights.
It will also wait for the old files to be accessible if they are currently still used. 

If everything worked successfully, it will restart YUView.

The executable is statically linked and should run on all systems.