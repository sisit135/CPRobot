# SPDX-License-Identifier: GPL-3.0-only

"""Validate the PlatformIO source graph without downloading its target SDK."""
import importlib.util
from pathlib import Path
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
        self.assertEqual(len(filters), 11)
        self.assertTrue(all(Path(p).is_dir() for p in env.paths))
        self.assertTrue(all("fakes" not in p and "share_sf" not in p for p in env.paths))
        self.assertEqual(sum("vendor" in p for p in env.paths), 1)
        self.assertTrue((Path(root).parents[0] / "include").is_dir())

    def test_missing_cmsis_is_explicit(self):
        with self.assertRaisesRegex(RuntimeError, "custom_cmsis_dsp_root"):
            sources.configure(Env("-DLOC_USE_CMSIS_DSP"))

    def test_required_remote_sources_are_checked_in_every_profile(self):
        real_is_file=Path.is_file
        with patch.object(Path,"is_file",lambda p: False if p.name=="codec.cpp" else real_is_file(p)):
            with self.assertRaisesRegex(RuntimeError, "Required remote configuration"):
                sources.configure(Env())


if __name__ == "__main__":
    unittest.main()
