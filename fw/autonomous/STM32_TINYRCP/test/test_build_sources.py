# SPDX-License-Identifier: GPL-3.0-only

"""Validate the PlatformIO source graph without downloading its target SDK."""
import importlib.util
from pathlib import Path
import shlex
import subprocess
import sys
import unittest
from unittest.mock import patch

project = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("sources", project / "scripts/platformio_sources.py")
sources = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sources)


class Env(dict):
    def __init__(self, flags=""):
        super().__init__(BUILD_FLAGS=flags, PIOENV="test")
        self.paths, self.builds = [], []
        self.options = {}

    def ParseFlags(self, flags):
        defines = []
        for flag in shlex.split(flags):
            if flag.startswith("-D"):
                name, equal, value = flag[2:].partition("=")
                defines.append((name, value) if equal else name)
        return {"CPPDEFINES": defines}

    def Replace(self, **values):
        self.update(values)

    def subst(self, name):
        assert name == "$PROJECT_DIR"
        return str(project)

    def Append(self, **values):
        self.paths.extend(values["CPPPATH"])

    def BuildSources(self, target, root, src_filter):
        self.builds.append((target, root, src_filter))

    def GetProjectOption(self, name, default):
        if name=="custom_cmsis_dsp_root":
            return str(project / "__test_missing_cmsis__")
        return self.options.get(name, default)

    def GetProjectConfig(self):
        return self

    def set(self, section, name, value):
        assert section == "env:test"
        self.options[name] = value


class SourceGraph(unittest.TestCase):
    def test_base(self):
        env = Env()
        sources.configure(env)
        self.assertEqual(len(env.builds), 4)
        target, root, filters = env.builds[0]
        self.assertEqual(target, "$BUILD_DIR/robot_sf")
        self.assertEqual(len(filters), len(set(filters)))
        self.assertIn("+<navigation/right_hand_rule.cpp>", filters)
        self.assertEqual(len(filters), 12)
        self.assertTrue(all(Path(p).is_dir() for p in env.paths))
        self.assertTrue(all("fakes" not in p and "share_sf" not in p for p in env.paths))
        self.assertEqual(sum("vendor" in p for p in env.paths), 1)
        self.assertTrue((Path(root).parents[0] / "include").is_dir())

    def test_missing_cmsis_is_explicit(self):
        with self.assertRaisesRegex(RuntimeError, "custom_cmsis_dsp_root"):
            sources.configure(Env("-DLOC_USE_CMSIS_DSP"))

    def test_demo_excludes_sensor_navigation_and_store_sources(self):
        env = Env("-DAXI_OPENLOOP_DEMO=1 -DAXI_BUP_IMU_DIAGNOSTIC=1")
        sources.configure(env)
        self.assertEqual(len(env.builds), 1)
        self.assertEqual(env.builds[0][2], ["+<motor_control/wheel_controller.cpp>"])
        self.assertIn("+<tasks/openloop_demo.cpp>", env["SRC_FILTER"])
        self.assertIn("+<tasks/bup_tasks.cpp>", env["SRC_FILTER"])
        self.assertNotIn("+<tasks/localization_tasks.cpp>", env["SRC_FILTER"])
        self.assertNotIn("+<hal/hal_sensors.cpp>", env["SRC_FILTER"])

    def test_demo_zero_preserves_production_sources(self):
        env = Env("-DAXI_OPENLOOP_DEMO=0")
        sources.configure(env)
        self.assertEqual(len(env.builds), 4)

    def test_required_remote_sources_are_checked_in_every_profile(self):
        real_is_file=Path.is_file
        with patch.object(Path,"is_file",lambda p: False if p.name=="codec.cpp" else real_is_file(p)):
            with self.assertRaisesRegex(RuntimeError, "Required remote configuration"):
                sources.configure(Env())

    def test_lsm_excludes_mpu_and_uses_hal_vendor_sources(self):
        env = Env("-DAXI_IMU_LSM303DLHC=1")
        sources.configure(env)
        self.assertIn("-<hal/mpu9250_hal.cpp>", env["SRC_FILTER"])
        self.assertIn("LibDriver_MPU9250", env.options["lib_ignore"])
        self.assertNotIn("-<hal/vendor/lsm303dlhc/>", env["SRC_FILTER"])
        env = Env()
        sources.configure(env)
        self.assertIn("-<hal/lsm303dlhc_hal.cpp>", env["SRC_FILTER"])
        self.assertIn("-<hal/vendor/lsm303dlhc/>", env["SRC_FILTER"])

    def test_missing_lsm_submodules_are_explicit(self):
        real_is_file = Path.is_file
        with patch.object(Path, "is_file", lambda p: False if p.name == "accelero.h" else real_is_file(p)):
            with self.assertRaisesRegex(RuntimeError, "submodules"):
                sources.configure(Env("-DAXI_IMU_LSM303DLHC=1"))

    def test_backend_selection_rejected_by_build_and_compiler(self):
        cases = (("", True), ("-DAXI_IMU_LSM303DLHC=1", True),
                 ("-DAXI_IMU_MPU9250=1 -DAXI_IMU_LSM303DLHC=0", True),
                 ("-DAXI_IMU_MPU9250=1 -DAXI_IMU_LSM303DLHC=1", False),
                 ("-DAXI_IMU_MPU9250=0 -DAXI_IMU_LSM303DLHC=0", False),
                 ("-DAXI_IMU_LSM303DLHC=2", False))
        for flags, valid in cases:
            with self.subTest(flags=flags):
                result = subprocess.run(["c++", "-x", "c++", "-fsyntax-only",
                    "-I", str(project / "include"), *shlex.split(flags), "-"],
                    input='#include "hal/imu_selection.hpp"\n', text=True,
                    capture_output=True)
                self.assertEqual(result.returncode == 0, valid, result.stderr)
                if valid:
                    sources.configure(Env(flags))
                else:
                    with self.assertRaises(RuntimeError):
                        sources.configure(Env(flags))


if __name__ == "__main__":
    unittest.main()
