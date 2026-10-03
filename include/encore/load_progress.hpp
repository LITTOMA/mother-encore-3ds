#pragma once
#include <cstdint>

namespace encore {
// Synchronous, thread-local loading notifications. The platform owns time,
// throttling and drawing; the shared runtime never creates a thread or a frame.
enum class LoadPhase { FileRead, Checksum, Texture, Scene };
struct LoadProgress {
    LoadPhase phase;
    uint64_t completed, total;
};
using LoadProgressCallback = void (*)(void*, const LoadProgress&);

namespace load_progress_detail {
struct Observer {
    LoadProgressCallback callback = nullptr;
    void* context = nullptr;
};
inline thread_local Observer observer{};
inline thread_local bool reporting = false;
inline thread_local uint64_t activity_revision=0;
}

inline void set_load_progress_observer(LoadProgressCallback callback, void* context = nullptr) {
    load_progress_detail::observer = {callback, context};
}
inline void clear_load_progress_observer() {
    load_progress_detail::observer = {};
}

// Counts describe work within one operation, not validation success or the whole
// load. A new operation can restart at zero. total == 0 means unknown, so callers
// must not infer a percentage. Known totals bound the reported completed count.
// Callbacks may render loading UI, but must not mutate the loader/session. Nested
// reports are suppressed, including reports from a nested scoped observer.
inline uint64_t load_activity_revision(){return load_progress_detail::activity_revision;}
inline void report_load_progress(LoadPhase phase, uint64_t completed, uint64_t total) {
    if(phase!=LoadPhase::Scene)++load_progress_detail::activity_revision;
    const auto observer = load_progress_detail::observer;
    if (!observer.callback || load_progress_detail::reporting) return;
    if (total && completed > total) completed = total;
    struct DispatchGuard {
        DispatchGuard() { load_progress_detail::reporting = true; }
        ~DispatchGuard() { load_progress_detail::reporting = false; }
    } guard;
    observer.callback(observer.context, {phase, completed, total});
}

class ScopedLoadProgress {
public:
    explicit ScopedLoadProgress(LoadProgressCallback callback, void* context = nullptr)
        : previous_(load_progress_detail::observer) {
        set_load_progress_observer(callback, context);
    }
    ~ScopedLoadProgress() { load_progress_detail::observer = previous_; }
    ScopedLoadProgress(const ScopedLoadProgress&) = delete;
    ScopedLoadProgress& operator=(const ScopedLoadProgress&) = delete;
private:
    load_progress_detail::Observer previous_;
};
}
