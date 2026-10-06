#include "encore/player_ready.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
float distance(Vec2 a, Vec2 b) {
  return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}
} // namespace
const PlayerGraphState *
PlayerAnimationGraph::state(std::string_view name) const {
  if (!data_)
    return nullptr;
  auto it = std::find_if(data_->states().begin(), data_->states().end(),
                         [&](const auto &s) { return s.name == name; });
  return it == data_->states().end() ? nullptr : &*it;
}
bool PlayerAnimationGraph::initialize(const PlayerReadyData &d,
                                      PlayerGraphHost h, std::string &e) {
  if (data_ || !d.valid() || !h.blend_clip || !h.begin_frame || !h.apply_frame)
    return fail(e, "Player AnimationTree actual track consumer pending");
  data_ = &d;
  host_ = std::move(h);
  for (const auto &s : d.states()) {
    if (!s.parameter.empty())
      blends_[s.parameter] = s.initial;
    if (!s.scale_parameter.empty())
      scales_[s.scale_parameter] = s.scale;
    closest_[s.name] = -1;
    for (const auto &p : s.points)
      times_[p.node_id] = 0;
  }
  return true;
}
bool PlayerAnimationGraph::set_active(bool v, std::string &e) {
  if (!data_ || poisoned_)
    return fail(e, "Player graph unbound/poisoned");
  if (active_ == v)
    return true;
  active_ = v;
  started_ = v;
  return true;
}
bool PlayerAnimationGraph::set_blend(std::string_view name, Vec2 v,
                                     std::string &e) {
  auto it = blends_.find(std::string(name));
  if (!data_ || poisoned_ || it == blends_.end() || !std::isfinite(v.x) ||
      !std::isfinite(v.y))
    return fail(e, "Player graph blend source parameter/value rejected");
  it->second = v;
  return true;
}
bool PlayerAnimationGraph::set_scale(std::string_view name, float v,
                                     std::string &e) {
  auto it = scales_.find(std::string(name));
  if (!data_ || poisoned_ || it == scales_.end() || !std::isfinite(v))
    return fail(e, "Player graph scale source parameter/value rejected");
  it->second = v;
  return true;
}
bool PlayerAnimationGraph::travel(std::string_view name, std::string &e) {
  if (!data_ || poisoned_ || !state(name))
    return fail(e, "Player graph travel unknown source state");
  request_ = name;
  request_travel_ = true;
  stop_ = false;
  return true;
}
bool PlayerAnimationGraph::start(std::string_view name, std::string &e) {
  if (!data_ || poisoned_ || !state(name))
    return fail(e, "Player graph start unknown source state");
  request_ = name;
  request_travel_ = false;
  stop_ = false;
  return true;
}
bool PlayerAnimationGraph::stop(std::string &e) {
  if (!data_ || poisoned_)
    return fail(e, "Player graph stop unbound");
  stop_ = true;
  return true;
}
bool PlayerAnimationGraph::travel_route(std::string_view target) {
  path_.clear();
  if (current_ == target)
    return true;
  auto from = state(current_), to = state(target);
  if (!from || !to)
    return false;
  struct Cost {
    std::string previous;
    float distance = 0;
  };
  std::map<std::string, Cost> costs;
  std::vector<size_t> open;
  const auto &edges = data_->edges();
  for (size_t i = 0; i < edges.size(); ++i)
    if (edges[i].source == current_) {
      auto next = state(edges[i].target);
      if (!next)
        return false;
      open.push_back(i);
      costs[edges[i].target] = {current_,
                                distance(next->position, from->position) *
                                    edges[i].priority};
      if (edges[i].target == target) {
        path_.emplace_back(target);
        return true;
      }
    }
  bool found = false;
  while (!found) {
    if (open.empty())
      return false;
    size_t best = 0;
    float least = 1e20f;
    for (size_t j = 0; j < open.size(); ++j) {
      auto &edge = edges[open[j]];
      float cost = costs[edge.target].distance +
                   distance(state(edge.target)->position, to->position);
      if (cost < least) {
        least = cost;
        best = j;
      }
    }
    const auto edge = edges[open[best]];
    for (size_t i = 0; i < edges.size(); ++i) {
      const auto &next = edges[i];
      if (next.source != edge.target || next.target == edge.source)
        continue;
      float cost =
          distance(state(next.source)->position, state(next.target)->position) *
              next.priority +
          costs[next.source].distance;
      auto it = costs.find(next.target);
      if (it != costs.end()) {
        if (cost < it->second.distance)
          it->second = {next.source, cost};
      } else {
        costs[next.target] = {next.source, cost};
        open.push_back(i);
        if (next.target == target) {
          found = true;
          break;
        }
      }
    }
    if (!found)
      open.erase(open.begin() + best);
  }
  std::string at(target);
  std::set<std::string> seen;
  while (at != current_) {
    if (!seen.insert(at).second || !costs.count(at))
      return false;
    path_.push_back(at);
    at = costs.at(at).previous;
  }
  std::reverse(path_.begin(), path_.end());
  return true;
}
bool PlayerAnimationGraph::evaluate(const std::string &name, float delta,
                                    bool seek, float weight, float &remaining,
                                    std::string &e) {
  auto s = state(name);
  if (!s)
    return fail(e, "Player graph current source state unavailable");
  float input = seek ? delta
                     : delta * (s->scale_parameter.empty()
                                    ? 1
                                    : scales_.at(s->scale_parameter));
  if (!std::isfinite(input))
    return fail(e, "Player graph scaled delta overflow");
  int point = 0;
  if (!s->parameter.empty()) {
    Vec2 v = blends_.at(s->parameter);
    float best = 1e20f;
    point = -1;
    for (size_t i = 0; i < s->points.size(); ++i) {
      auto p = s->points[i].position;
      float dx = p.x - v.x, dy = p.y - v.y;
      float d = dx * dx + dy * dy;
      if (d < best) {
        best = d;
        point = int(i);
      }
    }
    if (point < 0)
      return fail(e, "Player graph discrete blend has no finite nearest point");
    if (closest_.at(name) != point) {
      closest_[name] = point;
      input = 0;
      seek = true;
    }
  }
  const auto &p = s->points[size_t(point)];
  auto &stored = times_.at(p.node_id);
  float previous = stored, time = seek ? input : std::max(0.0f, stored + input),
        step = seek ? 0 : input;
  if (!std::isfinite(time))
    return fail(e, "Player graph leaf time overflow");
  if (p.loop) {
    time = std::fmod(time, p.length);
    if (time < 0)
      time += p.length;
  } else if (time > p.length) {
    time = p.length;
    step = p.length - previous;
  }
  if (!host_.blend_clip(p, time, step, seek, weight, e))
    return false;
  stored = time;
  remaining = p.length - time;
  return true;
}
bool PlayerAnimationGraph::process_graph(float delta, bool seek,
                                         std::string &e) {
  if (!playing_ && request_.empty()) {
    if (!stop_ && !data_->start().empty()) {
      request_ = data_->start();
      request_travel_ = false;
      stop_ = false;
    } else
      return true;
  }
  if (playing_ && stop_) {
    stop_ = false;
    playing_ = false;
    return true;
  }
  bool play_start = false;
  if (!request_.empty()) {
    if (request_travel_) {
      if (!playing_) {
        if (stop_ || data_->start().empty())
          return fail(e, "Player graph stopped travel rejected");
        path_.clear();
        current_ = data_->start();
        playing_ = true;
        play_start = true;
      } else {
        if (!travel_route(request_)) {
          path_.clear();
          current_ = request_;
        }
        request_.clear();
      }
    } else {
      path_.clear();
      current_ = request_;
      playing_ = true;
      play_start = true;
      request_.clear();
    }
  }
  if ((seek && delta == 0) || play_start || current_.empty()) {
    if (seek && delta == 0 && !data_->start().empty())
      current_ = data_->start();
    if (!evaluate(current_, 0, true, 1, length_, e))
      return false;
    position_ = 0;
  }
  float rem = 0;
  if (!evaluate(current_, delta, seek, 1, rem, e))
    return false;
  if (rem > length_)
    length_ = rem;
  float next_position = length_ - rem;
  bool end_loop = next_position < position_;
  position_ = next_position;
  const PlayerGraphEdge *next = nullptr;
  if (!path_.empty()) {
    for (const auto &edge : data_->edges())
      if (edge.source == current_ && edge.target == path_.front())
        next = &edge;
  } else {
    uint32_t priority = std::numeric_limits<uint32_t>::max();
    for (const auto &edge : data_->edges())
      if (edge.source == current_ && edge.auto_advance &&
          edge.priority <= priority) {
        next = &edge;
        priority = edge.priority;
      }
  }
  if (next && (next->mode == 0 || length_ - position_ <= 0 || end_loop)) {
    if (!path_.empty())
      path_.erase(path_.begin());
    current_ = next->target;
    if (!evaluate(current_, 0, true, 0, length_, e))
      return false;
    position_ = 0;
  }
  return true;
}
bool PlayerAnimationGraph::process(float delta, std::string &e) {
  if (!data_ || poisoned_ || !std::isfinite(delta) || delta < 0)
    return fail(e, "Player AnimationTree actual internal delta rejected");
  if (!active_)
    return true;
  if (!host_.begin_frame(e)) {
    poisoned_ = true;
    return false;
  }
  if (started_) {
    if (!process_graph(0, true, e)) {
      poisoned_ = true;
      return false;
    }
    started_ = false;
  }
  if (!process_graph(delta, false, e) || !host_.apply_frame(e)) {
    poisoned_ = true;
    return false;
  }
  return true;
}
} // namespace encore::upstream
