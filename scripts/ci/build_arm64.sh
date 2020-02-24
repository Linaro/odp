#!/bin/bash
set -e

export TARGET_ARCH=aarch64-linux-gnu
if [ "${CC#clang}" != "${CC}" ] ; then
	export CC="clang --target=${TARGET_ARCH}"
	export CXX="clang++ --target=${TARGET_ARCH}"
else
	export CC="${TARGET_ARCH}-gcc"
	export CXX="${TARGET_ARCH}-g++"
fi
export CPPFLAGS="-I/usr/include/${TARGET_ARCH}/dpdk"

# Use target libraries
export PKG_CONFIG_PATH=
export PKG_CONFIG_LIBDIR=/usr/lib/${TARGET_ARCH}/pkgconfig

exec "$(dirname "$0")"/build.sh
