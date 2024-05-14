#! /bin/sh

set -e

# Smoke-test random-abort as part of running "make check".

if [ -n "$1" ]
then
    # If the test binary is passed in manually.
    test_bin=$1
else
    # If $binary_dir isn't set, default to using the build directory
    # this script resides under. Our CMake build will sync a copy of this
    # script to the build directory. Note this assumes we are executing a
    # copy of the script that lives under the build directory. Otherwise
    # passing the binary path is required.
    binary_dir=${binary_dir:-`dirname $0`}
    test_bin=$binary_dir/test_random_abort
fi
# $TEST_WRAPPER $test_bin -t 10 -T 5 -s workload -h /home/shauncl8/wiredtiger/logs/WT_TEST.random-abort
$TEST_WRAPPER $test_bin -t 10 -T 5 -s checker -h /home/shauncl8/wiredtiger/logs/WT_TEST.random-abort
# $TEST_WRAPPER $test_bin -t 10 -T 5
# $TEST_WRAPPER $test_bin -m -t 10 -T 5
# $TEST_WRAPPER $test_bin -C -t 10 -T 5
# $TEST_WRAPPER $test_bin -C -m -t 10 -T 5
