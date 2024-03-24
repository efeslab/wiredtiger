#!/bin/bash

set -euo pipefail

# Always restore the original paranoid level at the end of the script!!
orig_paranoid_level=$(cat /proc/sys/kernel/perf_event_paranoid)
trap 'echo "restoring orig paranoid level"; sudo sysctl kernel.perf_event_paranoid=${orig_paranoid_level}' EXIT

# Clean up test dirs
trap 'chmod -R u+w WT_*' 0 1 2 3 13 15

echo "Decreasing paranoid level. This is required for coz to run profiling"
sudo sysctl kernel.perf_event_paranoid=1

# Run coz on the test
coz run --- ./test_wt8963_insert_stress
