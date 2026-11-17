// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "remote/config.hpp"
namespace remote {
// Caller serializes poll with all store access and the navigation start gate.
// Callbacks are nonblocking: read returns one byte or -1; write accepts a
// complete line or returns false without consuming it. No dynamic allocation.
class AtCli {
public:
  using Read = int (*)();
  using Write = bool (*)(const char *, std::size_t);
  void poll(Store *store, bool idle, Read read, Write write);

private:
  void execute(Store *store, bool idle);
  void status(Status result, uint32_t revision);
  void table_border();
  char input_[96]{};
  char output_[256]{};
  std::size_t used_ = 0, pending_ = 0, list_index_ = 0;
  bool overflow_ = false, listing_ = false;
  bool human_list_ = false;
  uint8_t table_stage_ = 0;
};
} // namespace remote
