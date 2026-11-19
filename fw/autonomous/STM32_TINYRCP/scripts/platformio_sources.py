# SPDX-License-Identifier: GPL-3.0-only

"""Build the portable core and bundled ST vendor once, independent of shell cwd."""
from pathlib import Path

CORE_SOURCES = (
    "localization/robot_ekf.cpp", "localization/maze_map.cpp",
    "localization/fusion_engine.cpp", "localization/color_tracker.cpp",
    "navigation/AStar.cpp", "navigation/map_resolver.cpp",
    "navigation/end_cell.cpp", "navigation/feature_navigation.cpp", "navigation/path_executor.cpp", "navigation/right_hand_rule.cpp",
    "motor_control/wheel_controller.cpp",
    "motor_control/wheel_controller_settings.cpp",
)


def configure(env):
    project = Path(env.subst("$PROJECT_DIR")).resolve()
    fw = project.parents[1]
    core = fw / "robot_sf"
    flags = str(env.get("BUILD_FLAGS", ""))
    defines = env.ParseFlags(env.get("BUILD_FLAGS", "")).get("CPPDEFINES", [])
    demo = any(define == "AXI_OPENLOOP_DEMO" or
               (isinstance(define, (list, tuple)) and
                define[0] == "AXI_OPENLOOP_DEMO" and str(define[1]) == "1")
               for define in defines)
    if demo:
        # Compile no sensor, production navigation, telemetry or flash code.
        env.Append(CPPPATH=[str(core / "include"),
                            str(core / "include/motor_control"),
                            str(project / "include")])
        env.Replace(SRC_FILTER=["-<*>", "+<main.cpp>", "+<logging/>",
                                "+<tasks/bup_tasks.cpp>",
                                "+<tasks/openloop_demo.cpp>",
                                "+<hal/hal_motor.cpp>", "+<hal/hal_platform.cpp>",
                                "+<hal/rtt_log.cpp>", "+<hal/newlib_stdio.cpp>"])
        env.BuildSources("$BUILD_DIR/robot_sf", str(core / "src"),
                         src_filter=["+<motor_control/wheel_controller.cpp>"])
        return
    selection = {}
    for define in defines:
        name, value = define if isinstance(define, (list, tuple)) else (define, "1")
        if name in ("AXI_IMU_MPU9250", "AXI_IMU_LSM303DLHC"):
            if str(value) not in ("0", "1"):
                raise RuntimeError("IMU backend flags must be 0 or 1")
            selection[name] = int(value)
    lsm = selection.get("AXI_IMU_LSM303DLHC", 0)
    mpu = selection.get("AXI_IMU_MPU9250", 1 - lsm)
    if lsm + mpu != 1:
        raise RuntimeError("Select exactly one IMU backend")
    if lsm:
        vendor = project / "src/hal/vendor"
        required_lsm = [vendor / "lsm303dlhc/lsm303dlhc.c",
                        vendor / "lsm303dlhc/lsm303dlhc.h",
                        vendor / "Common/accelero.h"]
        if not all(path.is_file() for path in required_lsm):
            raise RuntimeError("Missing ST LSM303DLHC/Common submodules; run git submodule update --init")
        env.Replace(SRC_FILTER=["+<*>", "-<hal/mpu9250_hal.cpp>"])
        # Also honor direct build-flag selection outside the named profile.
        ignored = list(env.GetProjectOption("lib_ignore", []))
        if "LibDriver_MPU9250" not in ignored:
            env.GetProjectConfig().set("env:" + env["PIOENV"], "lib_ignore",
                                       ignored + ["LibDriver_MPU9250"])
    else:
        env.Replace(SRC_FILTER=["+<*>", "-<hal/lsm303dlhc_hal.cpp>",
                                "-<hal/vendor/lsm303dlhc/>"])
    missing = [str(core / "src" / name) for name in CORE_SOURCES
               if not (core / "src" / name).is_file()]
    required = [project / "src/hal/vendor/vl53l0x_class.cpp",
                project / "include/vl53l0x_api.h",
                project / "src/hal/vl53l0x_vendor_api.cpp"]
    missing += [str(p) for p in required if not p.is_file()]
    if missing:
        raise RuntimeError("Missing firmware sources: " + ", ".join(missing))
    env.Append(CPPPATH=[str(core / "include"), str(core / "include/motor_control"),
                        str(project / "include"), str(project / "src/hal/vendor")])
    env.BuildSources("$BUILD_DIR/robot_sf", str(core / "src"),
                     src_filter=["+<" + name + ">" for name in CORE_SOURCES])
    # Project src/ is compiled by PlatformIO, including vl53l0x_class.cpp,
    # the narrow facade and the polling port. Never add an external ST C core.
    embedded = fw.parent / "third_party/EmbeddedProto/src"
    remote = core / "src/remote"
    littlefs = fw.parent / "third_party/littlefs"
    required_remote = [remote / name for name in ("config.cpp", "codec.cpp", "service.cpp", "at_cli.cpp")]
    required_remote += [littlefs / "lfs.c", littlefs / "lfs_util.c"]
    required_remote += [core / "include/remote/catalog.hpp", core / "include/remote/schema_blob.hpp"]
    if any(not path.is_file() for path in required_remote):
        raise RuntimeError("Required remote configuration sources/assets are missing; regenerate schemas and restore littlefs")
    env.Append(CPPPATH=[str(embedded), str(littlefs),
                        str(core / "include/remote/protocol")])
    env.BuildSources("$BUILD_DIR/embedded_proto", str(embedded),
                     src_filter=["+<Fields.cpp>", "+<MessageInterface.cpp>", "+<ReadBufferSection.cpp>"])
    env.BuildSources("$BUILD_DIR/remote", str(remote), src_filter=["+<*.cpp>"])
    env.BuildSources("$BUILD_DIR/littlefs", str(littlefs), src_filter=["+<lfs.c>", "+<lfs_util.c>"])
    if "LOC_USE_CMSIS_DSP" in flags:
        root = Path(env.GetProjectOption("custom_cmsis_dsp_root",
                                        str(fw.parent / "third_party/CMSIS-DSP")))
        if not root.is_absolute():
            root = project / root
        matrix = root / "Source/MatrixFunctions"
        names = ["arm_mat_mult_f32.c", "arm_mat_trans_f32.c"]
        if not all((matrix / n).is_file() for n in names):
            raise RuntimeError("CMSIS profile requires custom_cmsis_dsp_root pointing to CMSIS-DSP")
        env.Append(CPPPATH=[str(root / "Include"), str(root / "PrivateInclude")])
        env.BuildSources("$BUILD_DIR/cmsis_matrix", str(matrix),
                         src_filter=["+<" + n + ">" for n in names])


if "Import" in globals():
    Import("env")
    configure(env)
