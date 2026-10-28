# Missile Defense Demo - Linux

A real-time ship-defense simulation built with SDL2 and RTI Connext DDS. The
`command_control`, `sensor`, and `effector` programs communicate through DDS
topics, with optional runtime QoS editing and monitoring.

## Features

- SDL2 command-and-control display with the Arleigh Burke ship, radar tracks,
  incoming threat types, launch effects, and weapon status.
- Separate sensor and effector processes connected to the GUI by DDS.
- Runtime QoS editing for threat publishers, sensor and effector readers, and
  individual weapon endpoints. Click **Spawn** to configure a threat writer;
  click a weapon mount to view or change that weapon's QoS.
- RTI Monitoring Library configuration, Prometheus scraping, and a provisioned
  Grafana dashboard.
- An optional no-DDS build for GUI-only development; the demo uses DDS by default.

## Requirements

On Debian/Ubuntu, install the SDL2 development packages and build tools:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libsdl2-dev libsdl2-image-dev libgtk-3-dev
```

For DDS operation, install RTI Connext DDS 7.7.0 for Linux and generate the
type-support sources from `idl/ShipThreat.idl`. See [`idl/README.md`](idl/README.md)
for `rtiddsgen` instructions. Set `NDDSHOME` to the installation directory and
`CONNEXTDDS_ARCH` to the architecture installed by RTI (for example
`x64Linux4gcc8.5.0`).

## Build

Build the DDS demo (the default). Use the same RTI installation for code
generation, compilation, and runtime:

```bash
export NDDSHOME=/path/to/rti_connext_dds-7.7.0
export CONNEXTDDS_ARCH=x64Linux4gcc8.5.0
./scripts/generate_rtiddsgen.sh
cmake -S . -B build \
  -DUSE_CONNEXT=ON \
  -DRTI_CONNEXTDDS_DIR="$NDDSHOME"
cmake --build build --parallel
```

For manual launches, load the included monitoring profile and matching runtime:

```bash
export NDDS_QOS_PROFILES="$PWD/USER_QOS_PROFILES.xml"
export LD_LIBRARY_PATH="$NDDSHOME/lib/$CONNEXTDDS_ARCH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

The RTI installation must be licensed and its shared-library dependencies
available to the runtime loader. CMake supports `CONNEXTDDS_ARCH` overrides for
RTI installations using a different Linux architecture directory.

For GUI-only development, explicitly configure a separate no-DDS build:

```bash
cmake -S . -B build/offline -DUSE_CONNEXT=OFF
cmake --build build/offline --parallel
./build/offline/apps/command_control/command_control
```

These offline applications do not communicate and are not used by the demo
launcher.

## Run

On Linux, run [`run_missile_defense.sh`](run_missile_defense.sh) from any
directory after building the project. It locates the build relative to the
script, selects the RTI libraries recorded by CMake in
`build/dds-library-path.txt`, and checks that all three binaries have DDS enabled.
It then switches to the repository root so Connext finds `USER_QOS_PROFILES.xml`
regardless of the caller's working directory, starts the sensor and effector,
and runs the GUI. When the GUI exits, it stops the other two processes.
The launcher refuses no-DDS builds and reports early service startup failures.
The launcher removes Snap-specific GTK overrides and Snap entries in desktop
data/configuration search paths for the demo processes. It detects the paths
even if `SNAP_NAME` is missing or names a different application, and also
filters shared Snap runtimes and inherited VS Code original paths.
This prevents incompatible Snap settings schemas and
plugins from crashing the native QoS dialog; no desktop settings are changed.

All three applications use DDS domain **0**. Each executable supports
`--check-dds` to report compile-time DDS support and exit; this is not a
connectivity test. The checks below exercise actual DDS communication.

To start the processes manually, open three terminals in the repository root:

```bash
./build/apps/command_control/command_control
./build/apps/sensor/sensor
./build/apps/effector/effector
```

In the display, use the threat controls to select and spawn a threat. In a DDS
build, the spawn action opens the threat writer QoS controls. Select a weapon
mount to configure its publisher and associated reader endpoints. In builds
without RTI Connext, inter-process DDS communication and QoS editing are
disabled. The right-hand status panel and bottom-right spawn controls remain
visible: sensor and effector entries show `NO DDS`, while the threat list shows
locally spawned threats. Type selection, **Spawn One**, **Auto**, and the interval
buttons work without DDS; live sensor detections and effector actions require a
DDS-enabled build.

### Effector publisher QoS

Click an effector mount to open the Linux GTK publisher form. It uses the same
two-column layout and fields as the Windows dialog: profile, partitions,
ownership/strength, liveliness/lease duration, durability, history, reliability,
deadline, and lifespan. The form initially displays the selected effector's
current DDS settings; the profile list includes all available profiles.

Enter durations as nonnegative milliseconds (up to six decimal places) or
`INF`, and history as a positive integer or `ALL`. Separate partitions with
semicolons; leave the field blank for the default partition. **Action Reader
QoS...** opens a matching native reader form and stages its changes. It includes
reader profiles, partitions, ownership, durability, liveliness, history,
reliability, deadline, and time-based filtering (minimum separation), rather
than writer-only strength and lifespan fields. Minimum separation cannot exceed
the deadline. **Match publisher policies** copies shared policies and partitions.
Reopening the reader form retains staged edits; cancelling it leaves previously
staged edits unchanged. Reader forms opened during threat setup use this same
native editor.

**OK** validates and applies changes through DDS to the selected effector.
Invalid input or a rejected update leaves the form open with an error.
**Cancel**, Escape, or closing the window discards the form's edits, including
staged reader changes. Selecting a profile only fills the form; it does not
apply anything until **OK**.

The SDL loop continues servicing native GTK events after dialogs close, including
Wayland window teardown. A desktop regression test exercises publisher/reader
OK, Cancel, and reopening against a test effector on isolated DDS domain 132:

```bash
python3 tests/run_qos_lifecycle_test.py
```

This test requires the existing DDS-enabled Makefiles build and a desktop
display. It tests Wayland when available and X11, without changing domain 0.

## QoS checks

With the DDS-enabled applications running, the command-and-control executable
can exercise endpoint QoS updates and validation:

```bash
./build/apps/command_control/command_control --check-effector-qos
./build/apps/command_control/command_control --check-reader-qos
```

The first check targets the running effector service. The second requires both
the sensor and effector services and checks reader updates and filtered action
delivery.

## Observability

The monitoring-enabled participant profile publishes telemetry on DDS domain
101. With RTI Connext installed and the collector executable available, run the
collector in one terminal:

```bash
export NDDSHOME=/path/to/rti_connext_dds-7.7.0
export CONNEXTDDS_ARCH=x64Linux4gcc8.5.0
./scripts/run_collector_linux.sh
```

In another terminal, launch the Prometheus and Grafana containers:

```bash
docker compose -f observability/docker-compose.yml up -d
```

Open Grafana at <http://localhost:3000> and Prometheus at
<http://localhost:9090>. The dashboard is provisioned automatically. For
container runtime requirements and RTI collector details, see the RTI Connext
Monitoring and Collector Service documentation.

## Project layout

```text
apps/command_control/   SDL2 GUI and Linux QoS controls
apps/sensor/            sensor suite DDS process
apps/effector/          weapon systems DDS process
apps/effector_qos.h     shared runtime endpoint QoS support
idl/                    DDS IDL and generated type-support sources
observability/          Prometheus and Grafana configuration
scripts/                Linux collector launcher and RTI utilities
```

## License and disclaimer

This project is a demonstration and is not affiliated with or endorsed by the
U.S. Navy, RTI, Raytheon, or Lockheed Martin. RTI Connext DDS is a product of
Real-Time Innovations, Inc. and requires a separate license.
