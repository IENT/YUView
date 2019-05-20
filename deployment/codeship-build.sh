#!/bin/bash
set -eu

: "${SOURCE_FOLDER=$(realpath $(dirname $0)/..)}"

BUILD_FOLDER="${SOURCE_FOLDER}/build/docker-${LINUX_SHORT_NAME}"

pushd ${SOURCE_FOLDER}

: "${CI_BRANCH:=$(git rev-parse --abbrev-ref HEAD)}"
: "${CI_COMMIT_DESCRIPTION:=$(git describe)}"
: "${CI_TAG:=$(git rev-parse --short HEAD)}"

echo "Common variables:"
echo "  SOURCE_FOLDER:          ${SOURCE_FOLDER}"
echo "  BUILD_FOLDER:           ${BUILD_FOLDER}"
echo "  CI_BRANCH:              ${CI_BRANCH}"
echo "  CI_COMMIT_DESCRIPTION:  ${CI_COMMIT_DESCRIPTION}"
echo "  CI_TAG:                 ${CI_TAG}"
echo "  LINUX_SHORT_NAME        ${LINUX_SHORT_NAME}"

popd

mkdir -p ${BUILD_FOLDER}
cd ${BUILD_FOLDER}
qmake ${SOURCE_FOLDER}/YUView.pro
make -j$(nproc)
