// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// The installed TCS module is disconnected. Keep its register path compiled
// out by default; explicit driver fixtures can still exercise the retained HAL.
#ifndef AXI_TCS3472_ENABLE
#define AXI_TCS3472_ENABLE 0
#endif
#if AXI_TCS3472_ENABLE != 0 && AXI_TCS3472_ENABLE != 1
#error "AXI_TCS3472_ENABLE must be 0 or 1"
#endif
