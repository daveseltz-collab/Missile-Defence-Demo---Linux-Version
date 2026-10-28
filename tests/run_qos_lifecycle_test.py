"""Run against the existing DDS Makefiles build; uses isolated DDS domain 132."""
import os
from pathlib import Path
import shlex
import subprocess


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/apps/command_control"
flags = {
    line.split(" = ", 1)[0]: shlex.split(line.split(" = ", 1)[1])
    for line in (BUILD / "CMakeFiles/command_control.dir/flags.make").read_text().splitlines()
    if " = " in line
}
link = shlex.split((BUILD / "CMakeFiles/command_control.dir/link.txt").read_text())
for service in (False, True):
    subprocess.run(
        ["c++", *flags["CXX_DEFINES"], *flags["CXX_INCLUDES"],
         *[flag for flag in flags["CXX_FLAGS"] if flag not in ("-DNDEBUG", "-O3")],
         *(["-DTEST_EFFECTOR_SERVICE"] if service else []),
         str(ROOT / "tests/qos_dialog_lifecycle_test.cpp"), "-o",
         str(BUILD / ("qos_test_effector" if service else "qos_lifecycle_test")),
         *link[link.index("-o") + 2:]],
        cwd=BUILD, check=True,
    )

environment = os.environ.copy()
runtime = (ROOT / "build/dds-library-path.txt").read_text().strip()
environment["LD_LIBRARY_PATH"] = runtime + ":" + environment.get("LD_LIBRARY_PATH", "")
for key, value in list(environment.items()):
    if key.startswith(("GTK_", "GDK_", "GIO_", "GSETTINGS_")) and "/snap/" in value:
        environment.pop(key)
for key, default in (("XDG_DATA_DIRS", "/usr/local/share:/usr/share"),
                     ("XDG_CONFIG_DIRS", "/etc/xdg")):
    environment[key] = ":".join(
        item for item in environment.get(key, default).split(":")
        if item and "/snap/" not in item
    ) or default

backends = ["x11"]
if environment.get("WAYLAND_DISPLAY"):
    backends.insert(0, "wayland")
for backend in backends:
    environment.update(GDK_BACKEND=backend, SDL_VIDEODRIVER=backend)
    service = subprocess.Popen(
        [str(BUILD / "qos_test_effector")], cwd=ROOT, env=environment,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
    )
    try:
        result = subprocess.run(
            [str(BUILD / "qos_lifecycle_test")], cwd=ROOT, env=environment,
            capture_output=True, text=True, timeout=30,
        )
        print(backend, result.stdout, result.stderr)
        result.check_returncode()
    finally:
        service.terminate()
        try:
            output = service.communicate(timeout=5)[0]
        except subprocess.TimeoutExpired:
            service.kill()
            output = service.communicate()[0]
        print(output)
