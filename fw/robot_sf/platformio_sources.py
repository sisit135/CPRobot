# SPDX-License-Identifier: GPL-3.0-only

from pathlib import Path
Import("env")
root = Path(env.subst("$PROJECT_DIR")).resolve().parents[1] / "robot_sf"
defines = env.ParseFlags(env.GetProjectOption("build_flags")).get("CPPDEFINES", [])
if any((x[0] if isinstance(x, (tuple, list)) else x) == "LOC_USE_CMSIS_DSP" for x in defines):
    # Current ststm32 installs framework-cmsis-dsp alongside the Arduino core.
    dsp = env.PioPlatform().get_package_dir("framework-cmsis-dsp")
    if not dsp:
        raise RuntimeError("Install platformio/framework-cmsis-dsp or use the scalar environment")
    dsp = Path(dsp)
    env.Append(CPPPATH=[str(dsp / "Include"), str(dsp / "PrivateInclude")])
    env.BuildSources("$BUILD_DIR/cmsis_matrix", str(dsp / "Source/MatrixFunctions"),
                     src_filter=["+<arm_mat_mult_f32.c>", "+<arm_mat_trans_f32.c>"])

env.BuildSources("$BUILD_DIR/robot_sf", str(root / "src"))
