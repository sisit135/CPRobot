// SPDX-License-Identifier: GPL-3.0-only
#include "lifecycle.hpp"
#include "navigation/motion_experiment.hpp"
#include <cstdio>
#include <stdexcept>
#include <string>
#if defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
__attribute__((target("rdseed"))) static uint32_t hardware_seed() {
  if (!__builtin_cpu_supports("rdseed"))
    throw std::runtime_error("RDSEED unavailable; provide --seed for replay");
  for (unsigned attempt = 0; attempt < 1024; ++attempt) {
    unsigned seed;
    if (_rdseed32_step(&seed))
      return seed;
    _mm_pause();
  }
  throw std::runtime_error("RDSEED exhausted its retry bound");
}
#else
static uint32_t hardware_seed() {
  throw std::runtime_error("RDSEED requires x86; provide --seed for replay");
}
#endif
#ifdef ROBOT_VULKAN_GUI
int run_gui(lifecycle::Session &, bool exit_on_result, float speed);
#endif
int main(int argc, char **argv) {
  try {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    lifecycle::Options options;
    if constexpr (nav::motion_test::enabled)
      options.scrape_allowance_m = .003f;
    bool headless = false, exit_on_result = false, expect_failure = false,
         seed_supplied = false;
    float speed = 1;
    for (int i = 1; i < argc; ++i) {
      const std::string a = argv[i];
      auto value = [&]() -> std::string {
        if (++i == argc)
          throw std::runtime_error("missing value for " + a);
        return argv[i];
      };
      if (a == "--seed") {
        options.seed = std::stoul(value());
        seed_supplied = true;
      } else if (a == "--entrance")
        options.entrance = std::stoi(value());
      else if (a == "--heading")
        options.heading = std::stoi(value());
      else if (a == "--variant")
        options.variant = std::stoul(value());
      else if (a == "--variant-mask")
        options.variant_mask = std::stoul(value());
      else if (a == "--map") {
        const auto map = value();
        if (map != "q1" && map != "open")
          throw std::runtime_error("map must be q1 or open");
        options.open_arena = map == "open";
      } else if (a == "--goal") {
        options.goal.x = std::stoi(value());
        options.goal.y = std::stoi(value());
      } else if (a == "--noise")
        options.noise_sigma = std::stof(value());
      else if (a == "--surface") {
        const auto surface = value();
        if (surface != "whiteboard" && surface != "legacy")
          throw std::runtime_error("surface must be whiteboard or legacy");
        options.surface.enabled = surface == "whiteboard";
      } else if (a == "--friction-static")
        options.surface.mu_static = std::stof(value());
      else if (a == "--friction-kinetic")
        options.surface.mu_kinetic = std::stof(value());
      else if (a == "--rolling-resistance")
        options.surface.rolling_resistance_coefficient = std::stof(value());
      else if (a == "--contact-fraction") {
        options.surface.contact_left_fraction = std::stof(value());
        options.surface.contact_right_fraction = std::stof(value());
      } else if (a == "--speed")
        speed = std::stof(value());
      else if (a == "--headless")
        headless = true;
      else if (a == "--quiet")
        options.quiet = true;
      else if (a == "--scrape-mm")
        options.scrape_allowance_m = std::stof(value()) * .001f;
      else if (a == "--exit-on-result")
        exit_on_result = true;
      else if (a == "--no-tape")
        options.no_tape = true;
      else if (a == "--nearest-return")
        options.nearest_return = true;
      else if (a == "--expect-failure")
        expect_failure = true;
      else if (a == "--startup-only")
        options.startup_only = true;
      else if (a == "--force-rhr")
        options.force_rhr = true;
      else if (a == "--rhr-test")
        options.rhr_test = true;
      else if (a == "--range-override") {
        options.range_override = std::stoi(value());
        options.range_override_m = std::stof(value());
      } else if (a == "--help") {
        std::puts(
            "FEATURE lifecycle: [--headless] [--seed N] [--entrance 0|1|2] "
            "[--variant 0..3]\n"
            "  [--map q1|open] [--variant-mask 1..15]\n"
            "  [--heading 0..3] [--goal X Y] (diagnostic overrides; fixed end "
            "0 0)\n"
            "  [--noise sigma_m]\n"
            "  [--quiet] (WARN/ERROR logs plus machine-readable metrics)\n"
            "  [--scrape-mm 0..5] (rolling-test build only; default 3 mm)\n"
            "  Default seed uses RDSEED; --seed N replays a saved draw.\n"
            "  [--surface whiteboard|legacy] (default whiteboard)\n"
            "  [--friction-static N] [--friction-kinetic N] "
            "[--rolling-resistance N]\n"
            "  [--contact-fraction L R] (available normal-load fractions, "
            "0..1)\n"
            "  [--nearest-return] (nearest of 9 rays across 15 deg)\n"
            "  [--speed 1..20] [--exit-on-result] [--startup-only] "
            "[--force-rhr [--rhr-test]] "
            "[--range-override channel meters] [--no-tape "
            "--expect-failure]\n"
            "GUI keys: R rays, P path, D diagnostics, Space pause, N single "
            "step, Esc quit.");
        return 0;
      } else
        throw std::runtime_error("unknown option: " + a);
    }
    if (!(speed >= 1 && speed <= 20))
      throw std::runtime_error("speed must be 1..20");
    if (options.rhr_test && !options.force_rhr)
      throw std::runtime_error("--rhr-test requires --force-rhr");
    if (options.rhr_test && options.startup_only)
      throw std::runtime_error(
          "--rhr-test cannot be combined with --startup-only");
#ifndef ROBOT_VULKAN_GUI
    (void)exit_on_result;
    if (!headless)
      throw std::runtime_error(
          "GUI disabled; build with ROBOT_LIFECYCLE_GUI=ON or use --headless");
#endif
    if (!seed_supplied)
      options.seed = hardware_seed();
    std::printf("SEED source=%s value=%u\n",
                seed_supplied ? "explicit" : "rdseed", options.seed);
    lifecycle::Session session(options);
    if (headless)
      while (!session.finished())
        session.tick();
#ifdef ROBOT_VULKAN_GUI
    else if (run_gui(session, exit_on_result, speed) != 0)
      return 2;
#endif
    if (expect_failure)
      return session.finished() && !session.passed &&
                     session.navigation.run.state == nav::RunState::TIMEOUT &&
                     !session.navigation.end_tape_seen &&
                     session.duty_left == 0 && session.duty_right == 0
                 ? 0
                 : 1;
    if (options.startup_only)
      return session.result == "STARTUP REPORT" ? 0 : 1;
    return session.passed ? 0 : 1;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "lifecycle: %s\n", e.what());
    return 2;
  }
}
