# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(SRCS "main.c" "ble_nus.c" "wire.c" "protocol_codec.cpp" "remote_codec.cpp"
  INCLUDE_DIRS "." "../generated" "../../common" "../../../fw/telemetry_bridge/common" "../../../fw/robot_sf/include"
  EMBED_TXTFILES "../certs/server.crt" "../certs/server.key"
  REQUIRES esp_wifi esp_netif esp_event nvs_flash bt driver mbedtls)
add_subdirectory(hal/vendor)
target_link_libraries(${COMPONENT_LIB} PRIVATE tool_embedded_proto)
