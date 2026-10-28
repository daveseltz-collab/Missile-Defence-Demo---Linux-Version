# Missile Defense applications

This directory contains the SDL2 command-and-control display and the two
independent DDS processes:

- `command_control/`: threat publisher, sensor/effector subscribers, and live
  endpoint QoS controls.
- `sensor/`: threat subscriber and sensor-detection publisher.
- `effector/`: threat subscriber and weapon-action publishers.

Generate Connext C++ type support from the repository root with:

```bash
./scripts/generate_rtiddsgen.sh
```

The script writes files to `idl/generated/` by default. Set `NDDSHOME` or
`RTI_CONNEXTDDS_DIR` to the RTI Connext DDS installation root, and set
`RTIDDSGEN_OUTPUT_DIR` to choose another output directory.

SDL2 and SDL2_image must be installed and discoverable by CMake. Linux DDS
builds also require GTK 3 development files and pkg-config for the native
effector publisher QoS form (`libgtk-3-dev` and `pkg-config` on Debian/Ubuntu).
DDS is enabled
by default. For DDS builds,
configure CMake with `-DUSE_CONNEXT=ON -DRTI_CONNEXTDDS_DIR="$NDDSHOME"`.
For GUI-only development, `-DUSE_CONNEXT=OFF` explicitly selects standalone
fallback behavior, but the programs do not communicate with one another.
The demo launcher rejects these offline binaries.
