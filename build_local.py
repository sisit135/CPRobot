#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Interactive builds with local configuration/output and live source links."""

import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys


REPO = Path(__file__).resolve().parent
THS = Path("fw/autonomous/STM32_TINYRCP")
BRIDGE = Path("generic_field_tool/esp32_multiboard")
ANDROID = Path("generic_field_tool/android")
CHOICES = {
    "1": ("ths", "THS STM32F411CE firmware"),
    "2": ("c6", "ESP32-C6 remote bridge firmware"),
    "3": ("android", "Android field toolkit (debug APK and checks)"),
    "4": ("host", "Native firmware libraries and tests"),
    "5": ("lifecycle", "Interactive lifecycle GUI"),
    "6": ("navigation", "Standalone navigation libraries and tests"),
    "7": ("ths-cmsis", "THS firmware with CMSIS-DSP"),
    "8": ("all", "All standard components (1, 2, 3, 4, 6)"),
    "9": ("ths-trace", "THS firmware with all vendor tracing"),
    "10": ("c3", "LuatOS CORE ESP32-C3 remote bridge firmware"),
}


def link(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_symlink():
        if destination.readlink() == source:
            return
        raise RuntimeError(f"Conflicting source link: {destination}")
    if destination.exists():
        raise RuntimeError(f"Refusing to replace existing path: {destination}")
    destination.symlink_to(source, target_is_directory=source.is_dir())


def copy_config(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_symlink():
        raise RuntimeError(f"Configuration must be local, not a link: {destination}")
    # Do not copy SMB permissions, ownership or timestamps.
    shutil.copyfile(source, destination)


class Builder:
    def __init__(self, root, jobs, dry_run):
        self.root = root
        self.workspace = root / "workspace"
        self.jobs = jobs
        self.dry_run = dry_run
        link(REPO, root / "source")
        # Preserve relative include paths and THS pre-build script repository lookup.
        for relative in ("third_party", "fw/robot_sf", "generic_field_tool/common"):
            link(REPO / relative, self.workspace / relative)
        link(REPO / "fw/telemetry_bridge/common",
             self.workspace / "fw/telemetry_bridge/common")
        self.env = os.environ.copy()
        self.env.update({
            "PLATFORMIO_CORE_DIR": str(root / "cache/platformio"),
            "GRADLE_USER_HOME": str(root / "cache/gradle"),
            "ANDROID_USER_HOME": str(root / "cache/android"),
            "XDG_CACHE_HOME": str(root / "cache"),
            "PIP_CACHE_DIR": str(root / "cache/pip"),
            "TMPDIR": str(root / "tmp"),
            "PYTHONDONTWRITEBYTECODE": "1",
        })
        (root / "tmp").mkdir(parents=True, exist_ok=True)

    def run(self, command, cwd=None, env=None):
        print(f"\n$ {shlex.join(str(arg) for arg in command)}", flush=True)
        if not self.dry_run:
            subprocess.run(command, cwd=cwd or self.root, env=env or self.env,
                           check=True)

    def stage_project(self, relative, source_dirs):
        source = REPO / relative
        local = self.workspace / relative
        local.mkdir(parents=True, exist_ok=True)
        for name in source_dirs:
            if (source / name).exists():
                link(source / name, local / name)
        for name in ("platformio.ini", "CMakeLists.txt", "sdkconfig.defaults",
                     "partitions.csv"):
            if (source / name).is_file():
                copy_config(source / name, local / name)
        # Seed once: retain later locally generated SDK configuration on rebuild.
        for config in source.glob("sdkconfig.*"):
            if config.name.startswith("sdkconfig.defaults.") or (config.name != "sdkconfig.defaults" and not (local / config.name).exists()):
                copy_config(config, local / config.name)
        return local

    def pio(self, relative, profile):
        project = self.stage_project(relative, ("src", "include", "lib", "scripts", "generated"))
        pio = os.environ.get("PIO") or shutil.which("pio")
        if not pio:
            candidate = Path.home() / ".platformio/penv/bin/pio"
            pio = str(candidate) if candidate.is_file() else "pio"
        env = self.env.copy()
        env.update({
            "PLATFORMIO_BUILD_DIR": str(self.root / "build" / relative.name),
            "PLATFORMIO_LIBDEPS_DIR": str(self.root / "libdeps" / relative.name),
        })
        self.run([pio, "run", "-d", str(project), "-e", profile, "-j", str(self.jobs)],
                 env=env)
        print(f"Firmware: {env['PLATFORMIO_BUILD_DIR']}/{profile}/firmware.bin")

    def bridge(self, profile):
        certs = self.workspace / BRIDGE / "certs"
        certs.mkdir(parents=True, exist_ok=True)
        # Reuse the provisioned identity when present; never copy or print its key.
        source_certs = REPO / BRIDGE / "certs"
        source_pair = [source_certs / name for name in ("server.crt", "server.key")]
        if any(path.exists() for path in source_pair):
            if not all(path.is_file() for path in source_pair):
                raise RuntimeError("Source certificate pair is incomplete (server.crt/server.key)")
            for path in source_pair:
                link(path, certs / path.name)
        if not all((certs / name).is_file() for name in ("server.crt", "server.key")):
            # The canonical helper's relative paths now address the local scaffold.
            helper = self.workspace / "fw/telemetry_bridge/tools/generate_cert.sh"
            copy_config(REPO / "fw/telemetry_bridge/tools/generate_cert.sh", helper)
            self.run(["sh", str(helper)])
        self.pio(BRIDGE, profile)

    def android(self):
        project = self.workspace / ANDROID
        # Keep module roots local so Gradle/Kotlin outputs cannot land on SMB.
        for relative in (Path(), Path("app"), Path("protocol"),
                         Path("gradle"), Path("gradle/wrapper")):
            source = REPO / ANDROID / relative
            local = project / relative
            local.mkdir(parents=True, exist_ok=True)
            for entry in source.iterdir():
                if entry.is_file() and (entry.suffix in (".kts", ".properties", ".jar")
                                        or entry.name == "gradlew"):
                    copy_config(entry, local / entry.name)
            if (source / "src").is_dir():
                link(source / "src", local / "src")
        self.run(["bash", str(project / "gradlew"), "--project-dir", str(project),
                  "--project-cache-dir", str(self.root / "cache/android-project"),
                  "--max-workers", str(self.jobs), ":protocol:test", ":app:testDebugUnitTest",
                  ":app:lintDebug", ":app:assembleDebug"], cwd=project)
        print(f"APK: {project}/app/build/outputs/apk/debug/app-debug.apk")

    def native(self, name):
        output = self.root / "build" / name
        source = self.root / "source/fw"
        options = ["-DCMAKE_BUILD_TYPE=Debug"]
        if name == "navigation":
            source /= "tools/navigation_host"
        elif name == "lifecycle":
            options.append("-DROBOT_LIFECYCLE_GUI=ON")
        cmake = os.environ.get("CMAKE", "cmake")
        self.run([cmake, "-S", str(source), "-B", str(output), *options])
        command = [cmake, "--build", str(output), "--parallel", str(self.jobs)]
        if name == "lifecycle":
            command += ["--target", "robot_feature_lifecycle"]
        self.run(command)
        if name != "lifecycle":
            self.run([os.environ.get("CTEST", "ctest"), "--test-dir", str(output),
                      "--output-on-failure"])

    def build(self, name):
        print(f"\nBuilding {name}; local root: {self.root}", flush=True)
        if name in ("ths", "ths-cmsis", "ths-trace"):
            # Shared generated headers are also used by THS remote configuration.
            link(REPO / BRIDGE / "generated", self.workspace / BRIDGE / "generated")
            profile = {"ths": "blackpill_f411ce", "ths-cmsis": "blackpill_f411ce_cmsis",
                       "ths-trace": "blackpill_f411ce_trace"}[name]
            self.pio(THS, profile)
        elif name in ("c6", "c3"):
            self.bridge("esp32c6" if name == "c6" else "luatos_esp32c3")
        elif name == "android":
            self.android()
        else:
            self.native(name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path,
                        default=Path(os.environ.get("CP_ROBOT_BUILD_ROOT", "~/cp_robot_build")))
    parser.add_argument("--component", choices=[value[0] for value in CHOICES.values()])
    parser.add_argument("--jobs", type=int, default=int(os.environ.get("BUILD_JOBS", "4")))
    parser.add_argument("--dry-run", action="store_true", help="prepare links/configs and print commands")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")
    root = args.root.expanduser().resolve()
    if root == REPO or REPO in root.parents:
        parser.error("--root must be outside the source repository")
    builder = Builder(root, args.jobs, args.dry_run)
    while True:
        selection = args.component
        if selection is None:
            print(f"\nCPRobot local builds: {root}\nSources: {REPO}")
            for number, (_, label) in CHOICES.items():
                print(f"  {number}. {label}")
            print("  0. Exit\nADR MicroPython has no compilation step.")
            try:
                number = input("Choose a component number: ").strip()
            except EOFError:
                return 0
            if number == "0":
                return 0
            if number not in CHOICES:
                print("Enter a number from the menu.")
                continue
            selection = CHOICES[number][0]
        components = ["ths", "c6", "android", "host", "navigation"] if selection == "all" else [selection]
        failed = []
        for component in components:
            try:
                builder.build(component)
            except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
                print(f"\nFAILED {component}: {error}", file=sys.stderr)
                failed.append(component)
        if failed:
            print("\nFailed: " + ", ".join(failed))
        elif args.dry_run:
            print("\nPreview complete; no build commands executed.")
        else:
            print("\nSelected builds completed successfully.")
        if args.component:
            return 1 if failed else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nBuild interrupted.", file=sys.stderr)
        sys.exit(130)
    except (OSError, RuntimeError) as error:
        print(f"Cannot prepare local build workspace: {error}\n"
              "Use --root with a writable local directory, or have the build root created "
              "and assigned to your user first.", file=sys.stderr)
        sys.exit(1)
