#!/bin/sh
#deploy the zip file to the YUViewReleases repo
git config --global user.email "blaeser@ient.rwth-aachen.de"
git config --global user.name "Travis CI"
git clone -b master --single-branch https://${GH_TOKEN}@github.com/IENT/YUViewReleases.git --depth 1
cd YUViewReleases
# delete old file from cache just in case
git rm --ignore-unmatch mac/*
# copy the new file
mkdir -p mac
cp -f ../YUView-MacOs.zip mac/.
git add mac/*
git commit --allow-empty --message "Travis build ${TRAVIS_BUILD_NUMBER}, ${TRAVIS_BUILD_ID} based on ${TRAVIS_COMMIT}"
git push