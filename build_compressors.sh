#!/usr/bin/bash

# FIXME-WT-11845 - this script slightly deviates from the one attached to the Jira ticket, 
# namely that we save the built compressors (SRCDIR, DESTDIR) within the wiredtiger dir 
# instead of to /tmp since Evergreen doesn't clean the /tmp folder between runs and we hit 
# errors trying to extract to an existing dir.

set -euf -o pipefail

BASEDIR=$(pwd)

# Build dir.
SRCDIR=$BASEDIR/tmp_msan-src

# Staging root dir.
DSTDIR=$BASEDIR/tmp_msan-thirdparty

[[ -d $SRCDIR ]] || mkdir $SRCDIR
[[ -d $DSTDIR ]] || mkdir $DSTDIR

#
# Checkout source code.
#

REPOS="git@github.com:madler/zlib.git git@github.com:facebook/zstd.git git@github.com:lz4/lz4.git"
for repo in $REPOS; do
     (cd $SRCDIR; git clone $repo)
done

# Snappy uses submodules.
(cd $SRCDIR ; git clone --recurse-submodules git@github.com:google/snappy.git)

#
# Build and stage compression libraries with MSan support.
#

TOOLCHAIN=/opt/mongodbtoolchain/v4
PATH=$TOOLCHAIN/bin:$PATH
USE_CC=$TOOLCHAIN/bin/clang
USE_CXX=$TOOLCHAIN/bin/clang++
USE_LD=$TOOLCHAIN/bin/llv

(cd $SRCDIR/zstd;
 CC=$USE_CC CFLAGS='-fsanitize=memory -fPIE' LD=$USE_LD LDFLAGS='-fsanitize=memory -fPIE' make -j $(nproc);
 make PREFIX=$DSTDIR install)

(cd $SRCDIR/zlib;
 CC=$USE_CC CFLAGS='-fsanitize=memory -fPIE' LDFLAGS='-fsanitize=memory -fPIE' ./configure;
 make -j $(nproc);
 make prefix=$DSTDIR install)

(cd $SRCDIR/lz4;
 CC=$USE_CC CFLAGS='-fsanitize=memory -fPIE' LDFLAGS='-fsanitize=memory -fPIE' make -j $(nproc);
 make prefix=$DSTDIR install)

(cd $SRCDIR/snappy;
 cmake -E env CC=$USE_CC CXX=$USE_CXX CXXFLAGS="-fsanitize=memory -fPIE" LDXXFLAGS="-fsanitize=memory -fPIE" cmake -B build -DSNAPPY_BUILD_BENCHMARKS=FALSE -DBUILD_SHARED_LIBS=ON -DINSTALL_GTEST=OFF -DCMAKE_INSTALL_PREFIX="$DSTDIR";
 cd build;
 make -j $(nproc);
 make install)