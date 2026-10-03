#pragma once
#include <cmath>
#include <cstdint>
#include <limits>

namespace encore {

enum class LoadingTaskPlanState { Unplanned, Running, Failed, Succeeded };
enum class LoadingTaskUnit { Unknown, Bytes, Items };
enum class LoadingTaskPlanError {
    None, InvalidTotal, NotRunning, UnexpectedTask, AlreadyDeclared,
    InvalidUnit, UndeclaredLocalProgress, MismatchedLocalProgress,
    BackwardsLocalProgress, LocalOutOfRange, LocalIncomplete,
    IncompletePlan, TaskFailed
};

// A fixed, sequential plan for one load transaction. Each completed task has
// equal weight; this measures task completion, never elapsed time or cost.
// No game data, clocks, platform APIs, allocations or observer globals live here.
class LoadingTaskPlan {
public:
    // Build cache-conditioned task counts before begin(), without wrapping.
    // Zero additions are useful for cached groups; begin(0) is still invalid.
    static bool add_task_count(uint64_t& count, uint64_t addition) {
        if (addition > std::numeric_limits<uint64_t>::max() - count) return false;
        count += addition;
        return true;
    }

    // Explicitly starts a NEW plan, clearing previous success/failure/local state.
    // The denominator cannot change within a running plan.
    bool begin(uint64_t task_count) {
        total_ = task_count;
        completed_ = 0;
        clear_local();
        state_ = LoadingTaskPlanState::Running;
        error_ = LoadingTaskPlanError::None;
        return task_count ? true : reject(LoadingTaskPlanError::InvalidTotal);
    }

    // Call only after this task's operation and validation have succeeded.
    // The index makes skipped, repeated and out-of-order completions detectable.
    bool complete_task(uint64_t index) {
        if (!check_task(index)) return false;
        if (local_declared_ && local_completed_ != local_total_)
            return reject(LoadingTaskPlanError::LocalIncomplete);
        ++completed_; // check_task proves completed_ < total_, so cannot overflow.
        clear_local();
        return true;
    }

    // Publish success only AFTER the load transaction has committed successfully.
    // Completing the final task alone deliberately does not expose 100%.
    bool finish() {
        if (!check_running()) return false;
        if (completed_ != total_) return reject(LoadingTaskPlanError::IncompletePlan);
        state_ = LoadingTaskPlanState::Succeeded;
        return true;
    }

    // Loading/validation/commit failure freezes the last aggregate fraction.
    bool fail() {
        if (!check_running()) return false;
        return reject(LoadingTaskPlanError::TaskFailed);
    }

    // Optional counters belong ONLY to the next task, with a known immutable
    // total and unit. They are not included in the aggregate task fraction.
    // Without this declaration local progress remains indeterminate. Existing
    // LoadProgress events may restart or mix operations: do not forward them
    // unless their operation identity, total and unit match this declaration.
    bool declare_local(uint64_t index, uint64_t total, LoadingTaskUnit unit) {
        if (!check_task(index)) return false;
        if (local_declared_) return reject(LoadingTaskPlanError::AlreadyDeclared);
        if (!total) return reject(LoadingTaskPlanError::InvalidTotal);
        if (!known_unit(unit)) return reject(LoadingTaskPlanError::InvalidUnit);
        local_declared_ = true;
        local_total_ = total;
        local_unit_ = unit;
        return true;
    }

    bool report_local(uint64_t index, uint64_t completed, uint64_t total,
                      LoadingTaskUnit unit) {
        if (!check_task(index)) return false;
        if (!local_declared_)
            return reject(LoadingTaskPlanError::UndeclaredLocalProgress);
        if (total != local_total_ || unit != local_unit_)
            return reject(LoadingTaskPlanError::MismatchedLocalProgress);
        if (completed > total) return reject(LoadingTaskPlanError::LocalOutOfRange);
        if (completed < local_completed_)
            return reject(LoadingTaskPlanError::BackwardsLocalProgress);
        local_completed_ = completed; // Identical repeated observations are valid.
        return true;
    }

    LoadingTaskPlanState state() const { return state_; }
    LoadingTaskPlanError error() const { return error_; }
    uint64_t total_tasks() const { return total_; }
    uint64_t completed_tasks() const { return completed_; }

    double fraction() const {
        if (state_ == LoadingTaskPlanState::Succeeded) return 1.0;
        if (!total_) return 0.0;
        const double value = double(completed_) / double(total_);
        // Reserve a visible final interval for successful commit. Merely using
        // nextafter(1, 0) can round to the final pixel in a platform renderer.
        return value < 0.99 ? value : 0.99;
    }

    // Exact integer floor of 100 * completed / total, capped until commit.
    // Threshold decomposition avoids overflowing the uint64_t multiplication.
    unsigned percent() const {
        if (state_ == LoadingTaskPlanState::Succeeded) return 100;
        if (!total_) return 0;
        unsigned low = 0, high = 99;
        while (low < high) {
            const unsigned middle = (low + high + 1) / 2;
            const uint64_t threshold = (total_ / 100) * middle +
                ((total_ % 100) * middle + 99) / 100;
            if (completed_ >= threshold) low = middle;
            else high = middle - 1;
        }
        return low;
    }

    bool local_determinate() const { return local_declared_; }
    uint64_t local_completed() const { return local_completed_; }
    uint64_t local_total() const { return local_total_; }
    LoadingTaskUnit local_unit() const { return local_unit_; }
    double local_fraction() const {
        if (!local_declared_) return 0.0;
        if (local_completed_ == local_total_) return 1.0;
        const double value = double(local_completed_) / double(local_total_);
        return value < 1.0 ? value : std::nextafter(1.0, 0.0);
    }

private:
    static bool known_unit(LoadingTaskUnit unit) {
        return unit == LoadingTaskUnit::Bytes || unit == LoadingTaskUnit::Items;
    }
    bool reject(LoadingTaskPlanError error) {
        // Preserve the first failure and a previously committed success. A bad
        // call never regresses a finished plan into an apparently active load.
        if (state_ != LoadingTaskPlanState::Failed &&
            state_ != LoadingTaskPlanState::Succeeded) {
            state_ = LoadingTaskPlanState::Failed;
            error_ = error;
        }
        return false;
    }
    bool check_running() {
        return state_ == LoadingTaskPlanState::Running ? true :
            reject(LoadingTaskPlanError::NotRunning);
    }
    bool check_task(uint64_t index) {
        if (!check_running()) return false;
        return index == completed_ && completed_ < total_ ? true :
            reject(LoadingTaskPlanError::UnexpectedTask);
    }
    void clear_local() {
        local_declared_ = false;
        local_completed_ = local_total_ = 0;
        local_unit_ = LoadingTaskUnit::Unknown;
    }

    LoadingTaskPlanState state_ = LoadingTaskPlanState::Unplanned;
    LoadingTaskPlanError error_ = LoadingTaskPlanError::None;
    uint64_t total_ = 0, completed_ = 0;
    bool local_declared_ = false;
    uint64_t local_completed_ = 0, local_total_ = 0;
    LoadingTaskUnit local_unit_ = LoadingTaskUnit::Unknown;
};

} // namespace encore
