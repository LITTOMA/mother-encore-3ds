#pragma once
namespace encore {
// Optional cooperative cancellation for deterministic CPU preparation only.
// The callback may inspect cancellation state; it must not advance gameplay.
struct PreparationControl {
    bool (*cancelled)(void*)=nullptr;void* context=nullptr;
    bool stopped()const{return cancelled&&cancelled(context);}
};
}
