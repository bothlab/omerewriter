#!/bin/sh
set -e

#
# This script is supposed to run inside the CI Docker container
# on the CI system.
#

echo "C compiler: $CC"
echo "C++ compiler: $CXX"
export LANG=C.UTF-8

set -x
$CXX --version

# configure build with all flags enabled
mkdir build && cd build
cmake -GNinja \
    ..

# Build, Test & Install
ninja
DESTDIR=/tmp/install_root/ ninja install
