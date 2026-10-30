"""Build the portable core and bundled ST vendor once, independent of shell cwd."""
from pathlib import Path

CORE_SOURCES = (
    "localization/robot_ekf.cpp", "localization/maze_map.cpp",
    "localization/fusion_engine.cpp", "localization/color_tracker.cpp",
    "navigation/AStar.cpp", "navigation/map_resolver.cpp",
    "navigation/end_cell.cpp", "navigation/feature_navigation.cpp", "navigation/path_executor.cpp", "navigation/right_hand_rule.cpp",
    "motor_control/wheel_controller.cpp",
)


def configure(env):
    project = Path(env.subst("$PROJECT_DIR")).resolve()
    fw = project.parents[1]
    core = fw / "robot_sf"
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
    flags = str(env.get("BUILD_FLAGS", ""))
    if "AXI_TELEMETRY_SPI_ENABLE" in flags:
        embedded = fw.parent / "third_party/EmbeddedProto/src"
        env.Append(CPPPATH=[str(embedded), str(fw.parent / "generic_field_tool/esp32c6/generated")])
        env.BuildSources("$BUILD_DIR/embedded_proto", str(embedded),
                         src_filter=["+<Fields.cpp>", "+<MessageInterface.cpp>", "+<ReadBufferSection.cpp>"])
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
    if "AXI_REMOTE_CONFIG_ENABLE" in flags:
        remote = core / "src/remote"
        if not remote.is_dir() or not any(remote.glob("*.cpp")) or not (remote / "codec.c").is_file():
            raise RuntimeError("Remote profile requires robot_sf/src/remote implementations (missing from source-only archive)")
        littlefs = fw.parent / "third_party/littlefs"
        if not (littlefs / "lfs.c").is_file():
            raise RuntimeError("Remote profile requires third_party/littlefs/lfs.c")
        env.Append(CPPPATH=[str(littlefs)])
        env.BuildSources("$BUILD_DIR/remote", str(remote), src_filter=["+<*.cpp>", "+<*.c>"])
        env.BuildSources("$BUILD_DIR/littlefs", str(littlefs), src_filter=["+<lfs.c>", "+<lfs_util.c>"])


if "Import" in globals():
    Import("env")
    configure(env)
