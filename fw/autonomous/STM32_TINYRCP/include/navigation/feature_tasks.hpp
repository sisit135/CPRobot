// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "navigation/feature_navigation.hpp"
namespace nav {
bool feature_task_start(const ResolverConfig &, const FeatureConfig &);
void feature_task_halt();
bool feature_task_snapshot(FeatureSnapshot *);
void feature_task_target(loc::Cell);
} // namespace nav
