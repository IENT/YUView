#!/usr/bin/env bash

set -e

pacman -Syu --noconfirm
pacman -S --noconfirm gcc \
                      make \
                      git \
                      qt5-base
