# SPDX-License-Identifier: GPL-3.0-only
# Linux ucontext scheduler is isolated from the simpler one-task fixtures.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif()
add_executable(robot_feature_lifecycle main.cpp lifecycle.cpp scheduler.cpp
    ../../src/tasks/bup_tasks.cpp ../../src/tasks/localization_tasks.cpp
    ../../src/tasks/feature_navigation_tasks.cpp ../../src/tasks/navigation_tasks.cpp
    ../../src/tasks/motor_tasks.cpp ../../src/logging/log.cpp)
target_include_directories(robot_feature_lifecycle PRIVATE ../fakes ../../include)
target_link_libraries(robot_feature_lifecycle PRIVATE robot_sim)
target_compile_definitions(robot_feature_lifecycle PRIVATE ROBOT_LIFECYCLE_TEST)
target_compile_options(robot_feature_lifecycle PRIVATE -Wall -Wextra -Wpedantic)

# Reuse every lifecycle scenario/campaign with real ST/HAL register processing.
# Qualification flags apply only to the modeled environment, never firmware.
option(ROBOT_LIFECYCLE_LSM303DLHC "Run lifecycle through LSM303DLHC MAG+ACCEL HAL" OFF)
if(ROBOT_LIFECYCLE_LSM303DLHC)
    target_sources(robot_feature_lifecycle PRIVATE
        ../../src/hal/lsm303dlhc_hal.cpp
        ../../src/hal/vendor/lsm303dlhc/lsm303dlhc.c
        ../support/lsm303dlhc_device.cpp)
    target_include_directories(robot_feature_lifecycle PRIVATE
        ../support ../../src/hal/vendor)
    target_compile_definitions(robot_feature_lifecycle PRIVATE
        ROBOT_LIFECYCLE_LSM303DLHC AXI_IMU_LSM303DLHC=1
        SENSORS_LSM303DLHC_CALIBRATED=1
        SENSORS_LSM303DLHC_MAGNETIC_ENVIRONMENT_VERIFIED=1)
endif()

add_executable(robot_lifecycle_start_validation_tests test_start_validation.cpp)
target_link_libraries(robot_lifecycle_start_validation_tests PRIVATE robot_core)
target_compile_options(robot_lifecycle_start_validation_tests PRIVATE
    -UNDEBUG -Wall -Wextra -Wpedantic)
add_test(NAME robot_lifecycle_start_validation
    COMMAND robot_lifecycle_start_validation_tests)

foreach(variant RANGE 0 3)
    add_test(NAME robot_lifecycle_q${variant} COMMAND robot_feature_lifecycle
        --headless --seed 1234 --entrance 2 --heading 2 --variant ${variant} --goal 4 2)
    set_tests_properties(robot_lifecycle_q${variant} PROPERTIES
        TIMEOUT 30 PASS_REGULAR_EXPRESSION "PASS: end cell reached AND loc/run GOAL")
endforeach()
# Diagnostic route prefix through both corridor/intersection boundaries and
# the noisy turn at (2,3) that previously chattered in SETTLE. It requires the
# physical goal/tape proof and safe stop, but remains separate from fixed-goal
# campaign qualification. The production 86-second deadline is unchanged.
foreach(variant RANGE 0 3)
    add_test(NAME robot_lifecycle_feature_boundary_prefix_q${variant}
        COMMAND robot_feature_lifecycle --headless --seed 1234 --entrance 2
            --heading 2 --variant ${variant} --goal 2 4)
    set_tests_properties(robot_lifecycle_feature_boundary_prefix_q${variant}
        PROPERTIES TIMEOUT 30
        PASS_REGULAR_EXPRESSION "PASS: end cell reached AND loc/run GOAL"
        FAIL_REGULAR_EXPRESSION "right-hand fallback|FEATURE contradiction")
endforeach()
foreach(entrance RANGE 0 2)
    foreach(seed 1234 2026)
        if(entrance EQUAL 0)
            set(fixture_heading 0)
        else()
            set(fixture_heading 2)
        endif()
        add_test(NAME robot_lifecycle_open_${entrance}_${seed} COMMAND robot_feature_lifecycle
            --headless --map open --seed ${seed} --entrance ${entrance}
            --heading ${fixture_heading} --goal 2 2)
        set_tests_properties(robot_lifecycle_open_${entrance}_${seed} PROPERTIES
            TIMEOUT 30 PASS_REGULAR_EXPRESSION "PASS: end cell reached AND loc/run GOAL")
    endforeach()
endforeach()
add_test(NAME robot_lifecycle_no_tape COMMAND robot_feature_lifecycle --headless
    --seed 1234 --entrance 2 --heading 2 --goal 4 2 --no-tape)
set_tests_properties(robot_lifecycle_no_tape PROPERTIES TIMEOUT 30)
add_test(NAME robot_lifecycle_open_no_tape COMMAND robot_feature_lifecycle --headless
    --map open --seed 1234 --entrance 0 --heading 0 --goal 2 2 --no-tape)
set_tests_properties(robot_lifecycle_open_no_tape PROPERTIES TIMEOUT 30
    PASS_REGULAR_EXPRESSION "PASS: end cell reached AND loc/run GOAL"
    FAIL_REGULAR_EXPRESSION "tape=1")

add_test(NAME robot_lifecycle_resolver_e2e COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/run_resolver_e2e.py $<TARGET_FILE:robot_feature_lifecycle>
    --output ${CMAKE_CURRENT_BINARY_DIR}/resolver-e2e/summary.json)
set_tests_properties(robot_lifecycle_resolver_e2e PROPERTIES TIMEOUT 120)

foreach(variant RANGE 0 3)
    add_test(NAME robot_lifecycle_force_rhr_q${variant} COMMAND robot_feature_lifecycle
        --headless --seed 1234 --entrance 2 --heading 2 --variant ${variant}
        --variant-mask 15 --force-rhr --rhr-test)
    set_tests_properties(robot_lifecycle_force_rhr_q${variant} PROPERTIES
        TIMEOUT 45 PASS_REGULAR_EXPRESSION "PASS: forced RHR halted safely")
endforeach()

add_test(NAME robot_lifecycle_rhr_fixed_goal COMMAND robot_feature_lifecycle
    --headless --seed 1234 --entrance 2 --heading 2 --variant 0 --force-rhr)
set_tests_properties(robot_lifecycle_rhr_fixed_goal PROPERTIES TIMEOUT 30
    PASS_REGULAR_EXPRESSION "PASS: end cell reached AND loc/run GOAL"
    FAIL_REGULAR_EXPRESSION "RHR FAULT|RHR BLOCKED")

add_test(NAME robot_lifecycle_rhr_startup_uturn_goal COMMAND robot_feature_lifecycle
    --headless --seed 1234 --entrance 2 --heading 3 --variant 0 --force-rhr)
set_tests_properties(robot_lifecycle_rhr_startup_uturn_goal PROPERTIES TIMEOUT 30
    PASS_REGULAR_EXPRESSION "PASS: end cell reached AND loc/run GOAL"
    FAIL_REGULAR_EXPRESSION "RHR FAULT|RHR BLOCKED")

add_test(NAME robot_lifecycle_rhr_startup_center_goal COMMAND robot_feature_lifecycle
    --headless --seed 2 --entrance 2 --heading 3 --variant 0 --force-rhr)
set_tests_properties(robot_lifecycle_rhr_startup_center_goal PROPERTIES TIMEOUT 30
    PASS_REGULAR_EXPRESSION "PASS: end cell reached AND loc/run GOAL"
    FAIL_REGULAR_EXPRESSION "RHR FAULT|RHR BLOCKED")

# The settled-corner seed and arrival/front regressions started at the retired
# Q1 (0,0) POI. Their original sources, binaries and results are archived in
# archives/validation_pre_q1_poi_*.tar.gz; they are not new-POI validations.

# Fixed-seed full missions from the retained GDB failure investigation. These
# require real end-tape/goal/physical-stop proof under the unchanged run budget.
function(add_evidence_regression name seed variant entrance heading mask)
    add_test(NAME robot_lifecycle_evidence_${name} COMMAND robot_feature_lifecycle
        --headless --quiet --surface whiteboard --map q1 --seed ${seed}
        --variant ${variant} --entrance ${entrance} --heading ${heading}
        --variant-mask ${mask})
    set_tests_properties(robot_lifecycle_evidence_${name} PROPERTIES
        TIMEOUT 30 LABELS regression
        PASS_REGULAR_EXPRESSION "PASS: end cell reached AND loc/run GOAL"
        FAIL_REGULAR_EXPRESSION "physical wall collision|PATH FAULT|PATH BLOCKED")
endfunction()
add_evidence_regression(tape 1933008326 0 2 1 15)
add_evidence_regression(clearance 1494640011 1 2 0 15)
add_evidence_regression(progress 4062737702 1 2 0 2)
add_evidence_regression(pivot_alias_q1 2804585118 0 2 2 1)
add_evidence_regression(pivot_alias_unconfigured 2324046816 0 2 2 15)
add_evidence_regression(pivot_alias_q3 2220377848 2 2 2 4)
add_evidence_regression(pivot_alias_q4 3780416859 3 2 2 8)

add_test(NAME robot_lifecycle_rhr_test_requires_force COMMAND robot_feature_lifecycle
    --headless --rhr-test)
set_tests_properties(robot_lifecycle_rhr_test_requires_force PROPERTIES
    WILL_FAIL TRUE TIMEOUT 5)

add_test(NAME robot_lifecycle_rhr_test_rejects_startup_only COMMAND robot_feature_lifecycle
    --headless --force-rhr --rhr-test --startup-only)
set_tests_properties(robot_lifecycle_rhr_test_rejects_startup_only PROPERTIES
    WILL_FAIL TRUE TIMEOUT 5)

# Qualification cannot use pose/entrance/heading/goal overrides or expected
# failures. Each child process must reach the fixed end cell and GOAL.
add_test(NAME robot_lifecycle_campaign COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/run_campaign.py $<TARGET_FILE:robot_feature_lifecycle>
    --output-dir ${CMAKE_CURRENT_BINARY_DIR}/campaign)
set_tests_properties(robot_lifecycle_campaign PROPERTIES TIMEOUT 180 LABELS qualification)
add_test(NAME robot_lifecycle_open_campaign COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/run_campaign.py $<TARGET_FILE:robot_feature_lifecycle>
    --map open --output-dir ${CMAKE_CURRENT_BINARY_DIR}/open-campaign)
set_tests_properties(robot_lifecycle_open_campaign PROPERTIES TIMEOUT 180)

option(ROBOT_LIFECYCLE_GUI "Build SDL2 + Dear ImGui Vulkan lifecycle GUI" OFF)
if(ROBOT_LIFECYCLE_GUI)
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    find_package(SDL2 REQUIRED)
    find_package(Vulkan REQUIRED)
    FetchContent_Declare(imgui
        URL https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.91.9b
        URL_HASH SHA256=8e1bbc76c71d74fef2fb85db7e7ca8eba13d6a86623c54992b60162db554ffdb)
    FetchContent_MakeAvailable(imgui)
    add_library(robot_lifecycle_imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
    target_include_directories(robot_lifecycle_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(robot_lifecycle_imgui PUBLIC SDL2::SDL2 Vulkan::Vulkan)
    target_sources(robot_feature_lifecycle PRIVATE gui.cpp)
    target_compile_definitions(robot_feature_lifecycle PRIVATE ROBOT_VULKAN_GUI)
    target_link_libraries(robot_feature_lifecycle PRIVATE robot_lifecycle_imgui)
endif()
