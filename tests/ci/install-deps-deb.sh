#!/bin/sh
set -e
set -x

export DEBIAN_FRONTEND=noninteractive

# update caches
apt-get update -qq

# install build essentials
apt-get install -yq \
    eatmydata curl build-essential gdb gcc g++

# install build dependencies
eatmydata apt-get install -yq --no-install-recommends \
    git ca-certificates \
    cmake \
    ninja-build \
    gettext \
    libqt6opengl6-dev \
    libqt6svg6-dev \
    qt6-base-dev
