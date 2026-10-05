#pragma once
#include "encore/movement.hpp"
#include <string_view>

namespace encore::upstream {
// A scene host binds a checked, source-identified collision consumer before
// initializing its Room scheduler. Query caches may change during slide; game
// state and the caller's result remain untouched when a query is rejected.
// This capability admits static movement only, never an entire field scene.
class SceneMotionBackend : public MotionSolver {
public:
    virtual bool admitted() const=0;
    virtual std::string_view source_scene() const=0;
    virtual std::string_view reviewed_commit() const=0;
};
}
