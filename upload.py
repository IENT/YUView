import os
import subprocess
import filecmp
import hashlib
import argparse
from os import listdir
from os.path import isfile, join

upload_list = ["YUView.exe","versioninfo.txt","YUViewUpdater.exe"]
 
def getAllSubFiles(rootDir):    
    fileSet = set()
    for dir_, _, files in os.walk(rootDir):
        for fileName in files:
            relDir = os.path.relpath(dir_, rootDir)
            relFile = os.path.join(relDir, fileName)
            fileSet.add(relFile)
    return fileSet
 
def upload(user,token,repo,tag):
    onlyfiles = getAllSubFiles("release")
    for f in onlyfiles:
        filepathclean = os.path.join("release",f.lstrip(".\\"))
        filenameclean = os.path.basename(f)
        if filenameclean in upload_list:
        #print('github-release','-v','upload','--security-token',token,'--user',user,'--repo',repo,'--tag',tag,'--name',filenameclean,'--file',filepathclean)
            try:
                subproces.check_call('github-release','-v','upload','--security-token',token,'--user',user,'--repo',repo,'--tag',tag,'--name',filenameclean,'--file',filepathclean,shell=True)
            except subproces.CalledProcessError as e:
                print(str(e))

            
def main():
    parser = argparse.ArgumentParser(description="Uploads all files to Github")
    parser.add_argument('-u', '--user', help='Specifies the user.', type=str, required=True)
    parser.add_argument('-o', '--token', help='Specified the token.', type=str, required=True)
    parser.add_argument('-r', '--repo', help='Specified the repository.', type=str, required=True)
    parser.add_argument('-t', '--tag', help='Specified the tag.', type=str, required=True)
    args = parser.parse_args()
    if args.user is not None and args.token is not None and args.repo is not None and args.tag is not None:
        upload(args.user,args.token,args.repo,args.tag)
    else:
        print('An error occured. Did you specify everything correctly?')            

if __name__ == "__main__":
    main()
