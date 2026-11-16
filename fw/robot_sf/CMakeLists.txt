# SPDX-License-Identifier: GPL-3.0-only

set(embedded_proto_root "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/EmbeddedProto/src")
set(littlefs_root "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/littlefs")
add_library(robot_embedded_proto ${embedded_proto_root}/Fields.cpp
    ${embedded_proto_root}/MessageInterface.cpp ${embedded_proto_root}/ReadBufferSection.cpp)
target_include_directories(robot_embedded_proto PUBLIC ${embedded_proto_root})
add_library(robot_littlefs ${littlefs_root}/lfs.c ${littlefs_root}/lfs_util.c)
target_include_directories(robot_littlefs PUBLIC ${littlefs_root})
target_compile_definitions(robot_littlefs PRIVATE LFS_NO_DEBUG LFS_NO_WARN LFS_NO_ERROR)
option(AXI_PLATFORM_V2 "Use the 135 x 110 mm v2 platform geometry (v1 deprecated)" ON)
add_library(robot_remote src/remote/config.cpp src/remote/codec.cpp src/remote/service.cpp)
target_include_directories(robot_remote PUBLIC include include/motor_control
    "${CMAKE_CURRENT_SOURCE_DIR}/../../generic_field_tool/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../generic_field_tool/esp32_multiboard/generated")
target_compile_definitions(robot_remote PUBLIC AXI_PLATFORM_V2=$<BOOL:${AXI_PLATFORM_V2}>)
target_link_libraries(robot_remote PUBLIC robot_littlefs robot_embedded_proto)
target_compile_options(robot_remote PRIVATE -Wall -Wextra -Wpedantic)
add_library(robot_core
    src/localization/robot_ekf.cpp src/localization/maze_map.cpp
    src/localization/fusion_engine.cpp src/localization/color_tracker.cpp
    src/navigation/AStar.cpp src/navigation/map_resolver.cpp src/navigation/path_executor.cpp src/navigation/right_hand_rule.cpp src/navigation/feature_navigation.cpp src/navigation/end_cell.cpp
    src/motor_control/wheel_controller.cpp)
target_include_directories(robot_core PUBLIC include include/motor_control)
target_link_libraries(robot_core PUBLIC robot_remote)
target_compile_options(robot_core PRIVATE -Wall -Wextra -Wpedantic)
option(NAV_TURN_GUARD "Check side clearance and correct posture before pivots" ON)
target_compile_definitions(robot_core PUBLIC NAV_TURN_GUARD=$<BOOL:${NAV_TURN_GUARD}>)
option(ROBOT_MAX_SPEED_ROLLING_TEST "Full-output rolling-turn test mode" OFF)
option(NAV_NONSTOP_TURN "Rolling turns with normal speed and slew limits" ON)
if(ROBOT_MAX_SPEED_ROLLING_TEST AND NAV_TURN_GUARD)
    message(FATAL_ERROR "ROBOT_MAX_SPEED_ROLLING_TEST requires NAV_TURN_GUARD=OFF")
endif()
target_compile_definitions(robot_core PUBLIC
    ROBOT_MAX_SPEED_ROLLING_TEST=$<BOOL:${ROBOT_MAX_SPEED_ROLLING_TEST}>
    NAV_NONSTOP_TURN=$<BOOL:${NAV_NONSTOP_TURN}>)
if(LOC_USE_CMSIS_DSP)
    get_filename_component(CMSIS_DSP_ROOT "${CMSIS_DSP_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    if(NOT EXISTS "${CMSIS_DSP_ROOT}/Source/MatrixFunctions/arm_mat_mult_f32.c")
        message(FATAL_ERROR "Set CMSIS_DSP_ROOT to a CMSIS-DSP v1.16.0 checkout")
    endif()
    add_library(robot_cmsis_matrix
        ${CMSIS_DSP_ROOT}/Source/MatrixFunctions/arm_mat_mult_f32.c
        ${CMSIS_DSP_ROOT}/Source/MatrixFunctions/arm_mat_trans_f32.c)
    target_include_directories(robot_cmsis_matrix PUBLIC ${CMSIS_DSP_ROOT}/Include PRIVATE ${CMSIS_DSP_ROOT}/PrivateInclude)
    # CMSIS host scalar build: avoid target CMSIS compiler intrinsics.
    target_compile_definitions(robot_cmsis_matrix PUBLIC __GNUC_PYTHON__)
    target_compile_definitions(robot_core PUBLIC LOC_USE_CMSIS_DSP)
    target_link_libraries(robot_core PUBLIC robot_cmsis_matrix)
endif()
if(ROBOT_SANITIZERS)
    foreach(target robot_remote robot_littlefs robot_embedded_proto)
        target_compile_options(${target} PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PUBLIC -fsanitize=address,undefined)
    endforeach()
    target_compile_options(robot_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(robot_core PUBLIC -fsanitize=address,undefined)
endif()
