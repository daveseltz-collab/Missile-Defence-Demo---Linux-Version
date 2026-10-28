import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
GTK_VARIABLES = (
    "GSETTINGS_SCHEMA_DIR", "GTK_PATH", "GTK_EXE_PREFIX", "GTK_IM_MODULE_FILE",
    "GIO_MODULE_DIR", "GIO_EXTRA_MODULES", "GDK_PIXBUF_MODULE_FILE", "GDK_PIXBUF_MODULEDIR",
)


class LauncherEnvironmentTest(unittest.TestCase):
    def launch(self, overrides):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            shutil.copy2(ROOT / "run_missile_defense.sh", root)
            runtime = root / "runtime"
            runtime.mkdir()
            build = root / "build"
            build.mkdir()
            (build / "dds-library-path.txt").write_text(f"{runtime}\n")
            for app in ("sensor", "effector", "command_control"):
                executable = build / "apps" / app / app
                executable.parent.mkdir(parents=True)
                executable.write_text(
                    "#!/usr/bin/env python3\n"
                    "import json, os, sys, time\n"
                    "if sys.argv[1:] == ['--check-dds']:\n"
                    "    sys.exit(0)\n"
                    + ("print(json.dumps(dict(os.environ)))\n" if app == "command_control"
                       else "time.sleep(30)\n")
                )
                executable.chmod(0o755)
            environment = os.environ.copy()
            for key in (*GTK_VARIABLES, "SNAP_NAME", "XDG_DATA_DIRS", "XDG_CONFIG_DIRS",
                        "XDG_DATA_DIRS_VSCODE_SNAP_ORIG", "XDG_CONFIG_DIRS_VSCODE_SNAP_ORIG"):
                environment.pop(key, None)
            environment.update(overrides)
            result = subprocess.run(
                ["bash", str(root / "run_missile_defense.sh")],
                cwd="/", env=environment, capture_output=True, text=True, timeout=10,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            return json.loads(result.stdout.splitlines()[-1])

    def test_snap_restores_host_paths_and_removes_private_gtk_paths(self):
        settings = {key: "/home/test/snap/code/common/private" for key in GTK_VARIABLES}
        settings.update(
            SNAP_NAME="code",
            XDG_DATA_DIRS="/snap/code/268/usr/share:/usr/share",
            XDG_CONFIG_DIRS="/snap/code/268/etc/xdg:/etc/xdg",
            XDG_DATA_DIRS_VSCODE_SNAP_ORIG="/usr/local/share:/usr/share",
            XDG_CONFIG_DIRS_VSCODE_SNAP_ORIG="/etc/xdg",
        )
        actual = self.launch(settings)
        for key in GTK_VARIABLES:
            self.assertNotIn(key, actual)
        self.assertEqual(actual["XDG_DATA_DIRS"], "/usr/local/share:/usr/share")
        self.assertEqual(actual["XDG_CONFIG_DIRS"], "/etc/xdg")
        self.assertTrue(actual["LD_LIBRARY_PATH"].split(":")[0].endswith("/runtime"))

    def test_snap_without_originals_filters_only_snap_paths(self):
        actual = self.launch({
            "SNAP_NAME": "code",
            "XDG_DATA_DIRS": "/home/test/snap/code/268:/opt/site/share:/snap/code/268/usr/share:/usr/share",
            "XDG_CONFIG_DIRS": "/snap/code/268/etc/xdg",
            "GSETTINGS_SCHEMA_DIR": "/opt/site/schemas",
            "GTK_PATH": "/opt/site/gtk",
        })
        self.assertEqual(actual["XDG_DATA_DIRS"], "/opt/site/share:/usr/share")
        self.assertNotIn("XDG_CONFIG_DIRS", actual)
        self.assertEqual(actual["GSETTINGS_SCHEMA_DIR"], "/opt/site/schemas")
        self.assertEqual(actual["GTK_PATH"], "/opt/site/gtk")

    def test_normal_terminal_environment_is_preserved(self):
        settings = {
            "GSETTINGS_SCHEMA_DIR": "/opt/site/schemas",
            "GTK_PATH": "/opt/site/gtk",
            "XDG_DATA_DIRS": "/opt/site/share:/usr/share",
            "XDG_CONFIG_DIRS": "/etc/xdg",
        }
        actual = self.launch(settings)
        for key, value in settings.items():
            self.assertEqual(actual[key], value)

    def test_snap_paths_are_removed_without_snap_name(self):
        settings = {key: "/home/test/snap/code/common/private" for key in GTK_VARIABLES}
        settings.update(
            XDG_DATA_DIRS="/snap/code/current/usr/share:/usr/share",
            XDG_CONFIG_DIRS="/snap/code/current/etc/xdg:/etc/xdg",
        )
        actual = self.launch(settings)
        for key in GTK_VARIABLES:
            self.assertNotIn(key, actual)
        self.assertEqual(actual["XDG_DATA_DIRS"], "/usr/share")
        self.assertEqual(actual["XDG_CONFIG_DIRS"], "/etc/xdg")

    def test_shared_snap_runtime_and_polluted_originals_are_removed(self):
        actual = self.launch({
            "SNAP_NAME": "another-app",
            "GSETTINGS_SCHEMA_DIR": "/snap/gnome-42-2204/current/usr/share/glib-2.0/schemas",
            "GTK_PATH": "/snap/core22/current/usr/lib/gtk-3.0",
            "XDG_DATA_DIRS": "/snap/code/current/usr/share",
            "XDG_DATA_DIRS_VSCODE_SNAP_ORIG": "/snap/gnome-42-2204/current/usr/share:/usr/share",
            "XDG_CONFIG_DIRS": "/snap/core22/current/etc/xdg:/etc/xdg",
        })
        self.assertNotIn("GSETTINGS_SCHEMA_DIR", actual)
        self.assertNotIn("GTK_PATH", actual)
        self.assertEqual(actual["XDG_DATA_DIRS"], "/usr/share")
        self.assertEqual(actual["XDG_CONFIG_DIRS"], "/etc/xdg")

    def test_clean_paths_are_not_replaced_by_stale_originals(self):
        actual = self.launch({
            "SNAP_NAME": "code",
            "XDG_DATA_DIRS": "/opt/site/share:/usr/share",
            "XDG_DATA_DIRS_VSCODE_SNAP_ORIG": "/snap/code/current/usr/share",
        })
        self.assertEqual(actual["XDG_DATA_DIRS"], "/opt/site/share:/usr/share")


if __name__ == "__main__":
    unittest.main()
