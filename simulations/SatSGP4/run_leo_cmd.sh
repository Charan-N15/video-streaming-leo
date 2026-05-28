#!/usr/bin/env bash
set -e

# Usage:
#   ./run_leo_cmd.sh [config-section] [config-file]
#
# Example:
#   ./run_leo_cmd.sh VideoUDP omnetpp.ini

CONFIG="${1:-Experiment1}"
INI_FILE="${2:-omnetpp.ini}"

if [ ! -f "$INI_FILE" ]; then
  echo "Error: config file '$INI_FILE' does not exist."
  exit 1
fi

unset LD_LIBRARY_PATH
unset LD_PRELOAD

opp_run -m -u Cmdenv -c "$CONFIG" \
  -n ../../src:..:../../../inet-4.5.4/src:../../../inet-4.5.4/examples:../../../inet-4.5.4/showcases:../../../inet-4.5.4/tutorials:../../../inet-4.5.4/tests/validation:../../../inet-4.5.4/tests/networks:../../../os3/src:../../../os3/simulations \
  -x 'inet.common.selfdoc;inet.linklayer.configurator.gatescheduling.z3;inet.emulation;inet.showcases.visualizer.osg;inet.examples.emulation;inet.showcases.emulation;inet.transportlayer.tcp_lwip;inet.applications.voipstream;inet.visualizer.osg;inet.examples.voipstream' \
  --image-path=../../../inet-4.5.4/images:../../../os3/images \
  -l ../../../inet-4.5.4/out/clang-release/src/libINET.so \
  -l ../../../os3/out/clang-release/src/libos3.so \
  -l ../../out/clang-release/src/libleosatellites.so \
  --debug-on-errors=true \
  "$INI_FILE"
