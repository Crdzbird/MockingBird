// tests/test_degrade_policy.cpp
//
// The adaptive-degradation state machine in isolation: escalation is
// immediate, capped, and drop-triggered; de-escalation needs sustained calm.

#include "doctest.h"
#include "degrade.hpp"

using mockingbird::DegradationPolicy;

TEST_CASE("degrade: defaults and derived calm threshold") {
    DegradationPolicy p;
    p.configure(0);                       // 0 => 70% busy
    CHECK(p.level() == 0);
    CHECK(!p.update(699999, false));      // just under: hold
    CHECK(p.level() == 0);
    CHECK(p.update(700001, false));       // just over: escalate
    CHECK(p.level() == 1);
}

TEST_CASE("degrade: escalates one level per hot window, capped at 3") {
    DegradationPolicy p;
    p.configure(500000);
    CHECK(p.update(900000, false)); CHECK(p.level() == 1);
    CHECK(p.update(900000, false)); CHECK(p.level() == 2);
    CHECK(p.update(900000, false)); CHECK(p.level() == 3);
    CHECK(!p.update(900000, false));      // already at max: no change
    CHECK(p.level() == DegradationPolicy::kMaxLevel);
    CHECK(p.inhibit());
    CHECK(p.cadence_multiplier() == 4);
}

TEST_CASE("degrade: capture drops escalate regardless of busy share") {
    DegradationPolicy p;
    p.configure(700000);
    CHECK(p.update(1000, true));          // idle worker, but frames were lost
    CHECK(p.level() == 1);
}

TEST_CASE("degrade: de-escalation needs three consecutive calm windows") {
    DegradationPolicy p;
    p.configure(500000);                  // calm below 200000
    p.update(900000, false);
    p.update(900000, false);
    REQUIRE(p.level() == 2);
    CHECK(!p.update(100000, false));      // calm 1
    CHECK(!p.update(100000, false));      // calm 2
    CHECK(p.update(100000, false));       // calm 3 -> level 1
    CHECK(p.level() == 1);
    CHECK(!p.update(100000, false));
    CHECK(!p.update(300000, false));      // in the hold band: resets the calm run
    CHECK(!p.update(100000, false));
    CHECK(!p.update(100000, false));
    CHECK(p.update(100000, false));       // three calm again -> level 0
    CHECK(p.level() == 0);
    CHECK(!p.update(100000, false));      // nothing below 0
}

TEST_CASE("degrade: cadence multiplier per level") {
    DegradationPolicy p;
    p.configure(100);
    CHECK(p.cadence_multiplier() == 1); CHECK(!p.inhibit());
    p.update(1000, false); CHECK(p.cadence_multiplier() == 2);
    p.update(1000, false); CHECK(p.cadence_multiplier() == 4);
    p.update(1000, false); CHECK(p.inhibit());
}

TEST_CASE("degrade: reconfigure keeps the current level") {
    DegradationPolicy p;
    p.configure(100);
    p.update(1000, false);
    REQUIRE(p.level() == 1);
    p.configure(900000);
    CHECK(p.level() == 1);
    CHECK(!p.update(800000, false));      // under the new threshold: hold
}
