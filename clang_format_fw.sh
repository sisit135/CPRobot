# SPDX-License-Identifier: GPL-3.0-only

#/bin/bash
find ./fw -type f \( -name '*.cpp' -o -name '*.hpp' \) ! -path '*/.pio/*' -exec clang-format -i --style=file {} +