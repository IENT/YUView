#!/usr/bin/env bash

set -e

pacman -Syu --noconfirm
pacman -S base-devel
pacman -S git
pacman -S qt5
