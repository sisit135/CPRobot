# SPDX-License-Identifier: GPL-3.0-only

"""Validate the PlatformIO source graph without downloading its target SDK."""
import importlib.util
from pathlib import Path
import shlex
import sys
import unittest
from unittest.mock import patch

project = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("sources", project / "scripts/platformio_sources.py")
sources = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sources)


class Env(dict):
    def __init__(self, flags=""):
        super().__init__(BUILD_FLAGS=flags)
        self.paths, self.builds = [], []

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
        return default


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


if __name__ == "__main__":
    unittest.main()
