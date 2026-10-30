add_library(robot_core
    src/localization/robot_ekf.cpp src/localization/maze_map.cpp
    src/localization/fusion_engine.cpp src/localization/color_tracker.cpp
    src/navigation/AStar.cpp src/navigation/map_resolver.cpp src/navigation/path_executor.cpp src/navigation/right_hand_rule.cpp src/navigation/feature_navigation.cpp src/navigation/end_cell.cpp
    src/motor_control/wheel_controller.cpp)
target_include_directories(robot_core PUBLIC include include/motor_control)
target_compile_options(robot_core PRIVATE -Wall -Wextra -Wpedantic)
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
    target_compile_options(robot_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(robot_core PUBLIC -fsanitize=address,undefined)
endif()
