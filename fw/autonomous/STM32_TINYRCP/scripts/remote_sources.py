# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
Import("env")
root=Path(env.subst("$PROJECT_DIR")).resolve().parents[1]
third=root.parent / "third_party" / "littlefs"
env.Append(CPPPATH=[str(third)])
env.BuildSources("$BUILD_DIR/littlefs",str(third),src_filter=["+<lfs.c>","+<lfs_util.c>"])
# This changes LD_MAX_SIZE in the STM32 Arduino linker script as well as upload limits.
env.BoardConfig().update("upload.maximum_size",131072)
