#include "encore/loading_task_plan.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
unsigned checks = 0;
void check(bool ok, const char* why) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); }
}
using Plan = encore::LoadingTaskPlan;
using State = encore::LoadingTaskPlanState;
using Error = encore::LoadingTaskPlanError;
using Unit = encore::LoadingTaskUnit;

void failed(const Plan& plan, Error expected, uint64_t completed) {
    check(plan.state() == State::Failed && plan.error() == expected,
          "failure is explicit and records its cause");
    check(plan.completed_tasks() == completed && plan.fraction() < 1.0 &&
          plan.percent() < 100, "failure cannot manufacture completed tasks or 100 percent");
}

void cache_conditioned_plans() {
    // Standalone mechanism fixture, not an inventory of real game resources.
    // Decide optional uncached groups before the first task, including all hits.
    for (unsigned cache_mask = 0; cache_mask != 8; ++cache_mask) {
        uint64_t total = 2; // Required preparation and validation.
        if (!(cache_mask & 1)) check(Plan::add_task_count(total, 3), "plan metadata misses");
        if (!(cache_mask & 2)) check(Plan::add_task_count(total, 5), "plan texture misses");
        if (!(cache_mask & 4)) check(Plan::add_task_count(total, 2), "plan audio misses");
        Plan plan;
        check(plan.begin(total) && plan.total_tasks() == total && plan.fraction() == 0 &&
              plan.percent() == 0, "every cache-conditioned plan starts at zero");
        double previous = 0;
        for (uint64_t index = 0; index != total; ++index) {
            check(plan.complete_task(index), "planned task completes in source order");
            check(plan.total_tasks() == total, "cache denominator remains fixed");
            check(plan.fraction() > previous && plan.fraction() < 1.0,
                  "each completed task advances and never publishes early success");
            check(plan.percent() == (index + 1 == total ? 99 : (index + 1) * 100 / total),
                  "percent counts completed planned tasks");
            previous = plan.fraction();
        }
        check(plan.state() == State::Running && plan.fraction() == .99,
              "all tasks done reserves visible final one percent for successful commit");
        check(plan.finish() && plan.state() == State::Succeeded && plan.fraction() == 1 &&
              plan.percent() == 100, "successful final commit publishes one hundred");
        check(!plan.finish() && !plan.complete_task(total) && !plan.fail(),
              "finished plan rejects duplicate completion, commit and failure");
        check(plan.state() == State::Succeeded && plan.fraction() == 1,
              "rejected post-commit calls do not undo committed success");
    }
}

void failures_and_resets() {
    Plan plan;
    check(plan.state() == State::Unplanned && plan.total_tasks() == 0 &&
          plan.completed_tasks() == 0 && plan.fraction() == 0 && plan.percent() == 0,
          "unplanned progress is not successful completion");
    check(!plan.finish(), "unplanned finish rejected");
    failed(plan, Error::NotRunning, 0);
    check(!plan.begin(0), "unknown or zero aggregate denominator rejected");
    failed(plan, Error::InvalidTotal, 0);

    check(plan.begin(3) && plan.complete_task(0), "begin resets invalid plan");
    const double one_third = plan.fraction();
    check(!plan.complete_task(0), "double completion rejected");
    failed(plan, Error::UnexpectedTask, 1);
    check(!plan.complete_task(1) && !plan.finish() && !plan.fail(),
          "failed plan stays terminal");
    check(plan.error() == Error::UnexpectedTask && plan.fraction() == one_third,
          "terminal failure preserves first cause and aggregate fraction");

    check(plan.begin(3) && !plan.complete_task(1), "out-of-order completion rejected");
    failed(plan, Error::UnexpectedTask, 0);
    check(plan.begin(1) && !plan.complete_task(1), "out-of-range completion rejected");
    failed(plan, Error::UnexpectedTask, 0);
    check(plan.begin(3) && plan.complete_task(0) && !plan.finish(),
          "incomplete plan cannot commit");
    failed(plan, Error::IncompletePlan, 1);
    check(plan.begin(2) && plan.complete_task(0) && !plan.fail(),
          "failed task does not count as complete");
    failed(plan, Error::TaskFailed, 1);
    check(plan.begin(1) && plan.complete_task(0) && !plan.fail(),
          "final commit may fail after all individual tasks succeeded");
    failed(plan, Error::TaskFailed, 1);
    check(plan.fraction() == .99, "failed final commit does not reach renderer endpoint");

    check(plan.begin(2) && plan.declare_local(0, 10, Unit::Items) &&
          plan.report_local(0, 4, 10, Unit::Items), "partial local task fixture");
    check(plan.begin(1) && plan.completed_tasks() == 0 && plan.fraction() == 0 &&
          !plan.local_determinate() && plan.local_total() == 0 &&
          plan.local_completed() == 0 && plan.local_unit() == Unit::Unknown &&
          plan.error() == Error::None, "explicit new plan clears all previous counters and errors");
    check(plan.complete_task(0) && plan.finish() && plan.begin(4) && plan.percent() == 0,
          "new transaction may reset a successful plan to zero");
}

void local_progress() {
    Plan plan;
    check(plan.begin(2) && !plan.local_determinate() && plan.local_fraction() == 0,
          "local progress remains indeterminate without explicit contract");
    check(plan.declare_local(0, 4096, Unit::Bytes), "declare checked local byte operation");
    for (uint64_t completed : {0u, 1024u, 1024u, 4096u, 4096u}) {
        check(plan.report_local(0, completed, 4096, Unit::Bytes),
              "repeated and increasing local reports accepted");
        check(plan.fraction() == 0 && plan.percent() == 0 && plan.completed_tasks() == 0,
              "local byte progress does not invent aggregate task completion");
    }
    check(plan.local_determinate() && plan.local_completed() == 4096 &&
          plan.local_total() == 4096 && plan.local_unit() == Unit::Bytes &&
          plan.local_fraction() == 1, "local completion is distinct from task success");
    check(plan.complete_task(0) && plan.percent() == 50 && !plan.local_determinate(),
          "explicit success advances aggregate and clears old local operation");
    check(plan.declare_local(1, 2, Unit::Items) &&
          plan.report_local(1, 1, 2, Unit::Items) && plan.local_fraction() == .5 &&
          plan.fraction() == .5, "new task may declare a different unit without mixing counters");
    check(plan.report_local(1, 2, 2, Unit::Items) && plan.complete_task(1) && plan.finish(),
          "separately declared local operation and aggregate both complete");

    check(plan.begin(1) && !plan.report_local(0, 0, 4, Unit::Bytes),
          "undeclared local callback rejected");
    failed(plan, Error::UndeclaredLocalProgress, 0);
    check(plan.begin(1) && !plan.declare_local(0, 0, Unit::Bytes),
          "unknown local total rejected");
    failed(plan, Error::InvalidTotal, 0);
    for (const auto unit : {Unit::Unknown, static_cast<Unit>(999)}) {
        check(plan.begin(1) && !plan.declare_local(0, 4, unit), "unknown local unit rejected");
        failed(plan, Error::InvalidUnit, 0);
    }
    check(plan.begin(1) && plan.declare_local(0, 4, Unit::Bytes) &&
          !plan.declare_local(0, 4, Unit::Bytes), "local contract cannot be restarted in same task");
    failed(plan, Error::AlreadyDeclared, 0);
    for (const uint64_t total : {0u, 3u, 5u}) {
        check(plan.begin(1) && plan.declare_local(0, 4, Unit::Bytes) &&
              !plan.report_local(0, 1, total, Unit::Bytes), "changed report total rejected");
        failed(plan, Error::MismatchedLocalProgress, 0);
    }
    check(plan.begin(1) && plan.declare_local(0, 4, Unit::Bytes) &&
          !plan.report_local(0, 1, 4, Unit::Items), "changed report unit rejected");
    failed(plan, Error::MismatchedLocalProgress, 0);
    check(plan.begin(1) && plan.declare_local(0, 4, Unit::Bytes) &&
          !plan.report_local(0, 5, 4, Unit::Bytes), "over-range local report rejected, not clamped");
    failed(plan, Error::LocalOutOfRange, 0);
    check(plan.begin(1) && plan.declare_local(0, 4, Unit::Bytes) &&
          plan.report_local(0, 2, 4, Unit::Bytes) && !plan.report_local(0, 1, 4, Unit::Bytes),
          "backwards or restarted local operation rejected");
    failed(plan, Error::BackwardsLocalProgress, 0);
    check(plan.local_completed() == 2, "bad local report preserves last accepted count");
    check(plan.begin(1) && plan.declare_local(0, 4, Unit::Bytes) && !plan.complete_task(0),
          "declared incomplete local work cannot be marked successful");
    failed(plan, Error::LocalIncomplete, 0);
    check(plan.begin(2) && !plan.declare_local(1, 4, Unit::Bytes),
          "local contract for future task rejected");
    failed(plan, Error::UnexpectedTask, 0);
    check(plan.begin(2) && plan.declare_local(0, 4, Unit::Bytes) &&
          !plan.report_local(1, 1, 4, Unit::Bytes), "local report for wrong task rejected");
    failed(plan, Error::UnexpectedTask, 0);
}

void numeric_boundaries() {
    const uint64_t maximum = std::numeric_limits<uint64_t>::max();
    uint64_t count = maximum - 2;
    check(Plan::add_task_count(count, 2) && count == maximum, "largest task count sum accepted");
    check(!Plan::add_task_count(count, 1) && count == maximum,
          "overflowing plan sum rejected without mutation");
    count = 1;
    check(!Plan::add_task_count(count, maximum) && count == 1,
          "large addition cannot wrap to zero");
    check(Plan::add_task_count(count, 0) && count == 1, "cached empty group adds no task");
    Plan plan;
    check(plan.begin(maximum) && plan.complete_task(0) && plan.percent() == 0 &&
          std::isfinite(plan.fraction()) && plan.fraction() > 0,
          "largest denominator handles low progress without overflow");
    check(!plan.complete_task(maximum), "largest out-of-range index rejected");
    failed(plan, Error::UnexpectedTask, 1);
    check(plan.begin(1) && plan.declare_local(0, maximum, Unit::Bytes) &&
          plan.report_local(0, maximum - 1, maximum, Unit::Bytes) &&
          plan.local_fraction() < 1, "floating conversion cannot publish local completion early");
    check(plan.report_local(0, maximum, maximum, Unit::Bytes) &&
          plan.local_fraction() == 1 && plan.complete_task(0) && plan.percent() == 99 &&
          plan.fraction() == .99 && plan.finish(), "largest local counter can complete safely");

    // Exhaustive small ratios independently verify exact percentage boundaries.
    for (uint64_t total = 1; total <= 257; ++total) {
        check(plan.begin(total), "small ratio plan");
        for (uint64_t completed = 0; completed != total; ++completed) {
            check(plan.percent() == completed * 100 / total, "integer floor matches ratio oracle");
            check(plan.complete_task(completed), "ratio fixture task completion");
        }
        check(plan.percent() == 99 && plan.finish() && plan.percent() == 100,
              "all ratio plans reserve one hundred for final commit");
    }
}
}

int main() {
    cache_conditioned_plans();
    failures_and_resets();
    local_progress();
    numeric_boundaries();
    std::cout << "Loading task plan: " << checks << " checks passed\n";
}
