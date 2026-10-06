/* Static-convex subset adapted from Godot Engine 3.6.2-stable:
 * servers/physics_2d/{collision_solver_2d_sat,shape_2d_sw,space_2d_sw}.cpp
 * scene/2d/physics_body_2d.cpp; commit
 * 3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8. Copyright (c) 2014-present Godot
 * Engine contributors (see AUTHORS.md). Copyright (c) 2007-2014 Juan Linietsky,
 * Ariel Manzur.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "encore/podunk_player_kinematic.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
namespace encore::upstream {
namespace {
constexpr float epsilon = 0.00001f;
Vec2 add(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
Vec2 sub(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
Vec2 mul(Vec2 a, float b) { return {a.x * b, a.y * b}; }
float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
float length(Vec2 a) { return std::sqrt(dot(a, a)); }
Vec2 normalized(Vec2 a) {
  const float d = length(a);
  return d == 0 ? Vec2{} : Vec2{a.x / d, a.y / d};
}
Vec2 div(Vec2 a, float b) { return {a.x / b, a.y / b}; }
Vec2 tangent(Vec2 a) { return {a.y, -a.x}; }
bool zero(Vec2 a) { return a.x == 0 && a.y == 0; }
bool bounded(Vec2 a) {
  return std::isfinite(a.x) && std::isfinite(a.y) && std::abs(a.x) <= 1000000 &&
         std::abs(a.y) <= 1000000;
}
Vec2 projected_slide(Vec2 a, Vec2 normal) {
  return sub(a, mul(normal, dot(a, normal)));
}
struct Contact {
  Vec2 actor, obstacle;
};
struct Contacts {
  std::array<Contact, 32> data{};
  unsigned size = 0;
  bool overflow = false;
  void append(Vec2 a, Vec2 b) {
    // Refuse unsupported contact density instead of silently losing shapes.
    if (size == data.size()) {
      overflow = true;
      return;
    }
    data[size++] = {a, b};
  }
};

template <class Shape>
void project(const Shape &shape, Vec2 position, Vec2 axis, float &lo,
             float &hi) {
  if (shape.circle) {
    const float center = dot(axis, add(position, shape.center));
    const float radius = shape.radius * length(axis);
    lo = center - radius;
    hi = center + radius;
    return;
  }
  lo = hi = dot(axis, add(position, shape.vertices[0]));
  for (std::size_t i = 1; i < shape.vertices.size(); ++i) {
    const float d = dot(axis, add(position, shape.vertices[i]));
    lo = std::min(lo, d);
    hi = std::max(hi, d);
  }
}
template <class Shape>
unsigned supports(const Shape &shape, Vec2 position, Vec2 normal,
                  Vec2 *output) {
  if (shape.circle) {
    output[0] =
        add(add(position, shape.center), mul(normalized(normal), shape.radius));
    return 1;
  }
  if (shape.segment) {
    if (std::abs(dot(shape.normals[0], normal)) > 0.99998f) {
      output[0] = add(position, shape.vertices[0]);
      output[1] = add(position, shape.vertices[1]);
      return 2;
    }
    const unsigned index =
        dot(normal, sub(shape.vertices[1], shape.vertices[0])) > 0 ? 1 : 0;
    output[0] = add(position, shape.vertices[index]);
    return 1;
  }
  std::size_t index = 0;
  float best = -std::numeric_limits<float>::infinity();
  for (std::size_t i = 0; i < shape.vertices.size(); ++i) {
    const float d = dot(normal, shape.vertices[i]);
    if (d > best) {
      best = d;
      index = i;
    }
    if (dot(shape.normals[i], normal) > 0.99998f) {
      output[0] = add(position, shape.vertices[i]);
      output[1] =
          add(position, shape.vertices[(i + 1) % shape.vertices.size()]);
      return 2;
    }
  }
  output[0] = add(position, shape.vertices[index]);
  return 1;
}
Vec2 closest_on_line(Vec2 point, Vec2 a, Vec2 b) {
  const Vec2 delta = sub(b, a);
  return add(a, mul(delta, dot(sub(point, a), delta) / dot(delta, delta)));
}
void generate_contacts(Vec2 *a, unsigned na, Vec2 *b, unsigned nb, Vec2 normal,
                       Contacts &out) {
  if (na == 1 && nb == 1) {
    out.append(a[0], b[0]);
    return;
  }
  if (na == 1) {
    out.append(a[0], closest_on_line(a[0], b[0], b[1]));
    return;
  }
  if (nb == 1) {
    out.append(closest_on_line(b[0], a[0], a[1]), b[0]);
    return;
  }
  const Vec2 t = tangent(normal);
  const float da = dot(normal, a[0]), db = dot(normal, b[0]);
  struct Endpoint {
    float distance;
    bool actor;
    unsigned index;
  };
  std::array<Endpoint, 4> points{{{dot(t, a[0]), true, 0},
                                  {dot(t, a[1]), true, 1},
                                  {dot(t, b[0]), false, 0},
                                  {dot(t, b[1]), false, 1}}};
  // Stable order for ties. Ordering among multiple shapes remains input order.
  for (unsigned i = 1; i < points.size(); ++i) {
    const Endpoint value = points[i];
    unsigned j = i;
    while (j > 0 && value.distance < points[j - 1].distance) {
      points[j] = points[j - 1];
      --j;
    }
    points[j] = value;
  }
  for (unsigned i = 1; i <= 2; ++i) {
    Vec2 pa, pb;
    if (points[i].actor) {
      pa = a[points[i].index];
      pb = sub(pa, mul(normal, dot(normal, pa) - db));
    } else {
      pb = b[points[i].index];
      pa = sub(pb, mul(normal, dot(normal, pb) - da));
    }
    if (dot(normal, pa) > dot(normal, pb) - epsilon)
      continue;
    out.append(pa, pb);
  }
}

// The cast tests the Minkowski sweep, then uses Godot's eight adaptive fraction
// refinements. Margin is used for recovery/rest contacts, not the motion cast.
template <class Shape>
bool solve(const Shape &a, Vec2 position, Vec2 motion, const Shape &b,
           float safe_margin, Contacts *contacts = nullptr) {
  float best_depth = 1e15f;
  Vec2 best_axis{};
  const bool cast = !zero(motion);
  const auto test_axis = [&](Vec2 axis) {
    if (std::abs(axis.x) < epsilon && std::abs(axis.y) < epsilon)
      axis = {0, 1};
    float amin, amax, bmin, bmax;
    project(a, position, axis, amin, amax);
    if (cast) {
      float lo, hi;
      project(a, add(position, motion), axis, lo, hi);
      amin = std::min(amin, lo);
      amax = std::max(amax, hi);
    }
    project(b, {}, axis, bmin, bmax);
    amin -= safe_margin;
    amax += safe_margin;
    bmin -= (amax - amin) * 0.5f;
    bmax += (amax - amin) * 0.5f;
    float dmin = bmin - (amin + amax) * 0.5f,
          dmax = bmax - (amin + amax) * 0.5f;
    if (dmin > 0 || dmax < 0)
      return false;
    dmin = std::abs(dmin);
    if (dmax < dmin) {
      if (dmax < best_depth) {
        best_depth = dmax;
        best_axis = axis;
      }
    } else if (dmin < best_depth) {
      best_depth = dmin;
      best_axis = mul(axis, -1);
    }
    return true;
  };
  if (cast) {
    const Vec2 n = normalized(motion);
    if (!test_axis(n) || !test_axis(tangent(n)))
      return false;
  }
  if (b.circle) {
    // Official circle/convex SAT tests each poly point versus the analytic
    // circle centre, followed by that poly edge, in source vertex order.
    for (size_t i = 0; i < a.vertices.size(); ++i) {
      const Vec2 delta = sub(add(position, a.vertices[i]), b.center);
      if (!test_axis(normalized(delta)))
        return false;
      if (cast && !test_axis(normalized(add(delta, motion))))
        return false;
      if (!test_axis(a.normals[i]))
        return false;
    }
  } else if (b.segment) {
    // Godot segment/convex SAT visits the segment axis before each convex
    // edge, then its two endpoint margin axes in that same edge order.
    if (!test_axis(b.normals[0]))
      return false;
    for (size_t i = 0; i < a.normals.size(); ++i) {
      if (!test_axis(a.normals[i]))
        return false;
      if (safe_margin > 0)
        for (Vec2 bv : b.vertices) {
          const Vec2 delta = sub(add(position, a.vertices[i]), bv);
          if (!test_axis(normalized(delta)))
            return false;
          if (cast && !test_axis(normalized(add(delta, motion))))
            return false;
        }
    }
  } else {
    for (Vec2 n : a.normals)
      if (!test_axis(n))
        return false;
    for (Vec2 n : b.normals)
      if (!test_axis(n))
        return false;
  }
  if (safe_margin > 0 && !b.segment && !b.circle) {
    for (Vec2 av : a.vertices)
      for (Vec2 bv : b.vertices) {
        const Vec2 delta = sub(add(position, av), bv);
        if (!test_axis(normalized(delta)))
          return false;
        if (cast && !test_axis(normalized(add(delta, motion))))
          return false;
      }
  }
  if (zero(best_axis))
    return false;
  if (contacts) {
    // This backend only requests contacts for stationary shape tests.
    Vec2 av[2], bv[2];
    const unsigned na = supports(a, position, mul(best_axis, -1), av),
                   nb = supports(b, {}, best_axis, bv);
    for (unsigned i = 0; i < na; ++i)
      av[i] = sub(av[i], mul(best_axis, safe_margin));
    generate_contacts(av, na, bv, nb, best_axis, *contacts);
  }
  return true;
}

template <class Shape>
bool nearby(const Shape &actor, Vec2 position, Vec2 motion,
            const Shape &obstacle, float grow) {
  const Vec2 lo{position.x + actor.minimum.x + std::min(0.0f, motion.x) - grow,
                position.y + actor.minimum.y + std::min(0.0f, motion.y) - grow};
  const Vec2 hi{position.x + actor.maximum.x + std::max(0.0f, motion.x) + grow,
                position.y + actor.maximum.y + std::max(0.0f, motion.y) + grow};
  return lo.x <= obstacle.maximum.x && hi.x >= obstacle.minimum.x &&
         lo.y <= obstacle.maximum.y && hi.y >= obstacle.minimum.y;
}
struct Motion {
  Vec2 position{}, remainder{}, normal{};
  bool collided = false;
};
template <class Shape>
void record_query(const Shape &actor, Vec2 position, Vec2 motion, float grow,
                  MotionQueryBounds *bounds) {
  if (!bounds)
    return;
  const Vec2 lo{position.x + actor.minimum.x + std::min(0.0f, motion.x) - grow,
                position.y + actor.minimum.y + std::min(0.0f, motion.y) - grow};
  const Vec2 hi{position.x + actor.maximum.x + std::max(0.0f, motion.x) + grow,
                position.y + actor.maximum.y + std::max(0.0f, motion.y) + grow};
  if (!bounds->initialized) {
    *bounds = {lo, hi, true};
    return;
  }
  bounds->minimum.x = std::min(bounds->minimum.x, lo.x);
  bounds->minimum.y = std::min(bounds->minimum.y, lo.y);
  bounds->maximum.x = std::max(bounds->maximum.x, hi.x);
  bounds->maximum.y = std::max(bounds->maximum.y, hi.y);
}
template <class Shape>
bool move(const Shape &actor, const std::vector<Shape> &obstacles, Vec2 from,
          Vec2 motion, Motion &result, float margin,
          MotionQueryBounds *queries) {
  const float minimum_depth = margin * 0.05f;
  Vec2 position = from;
  bool recovered = false;
  for (unsigned attempt = 0; attempt < 4; ++attempt) {
    record_query(actor, position, {}, margin, queries);
    Contacts contacts;
    for (const auto &obstacle : obstacles)
      if (nearby(actor, position, {}, obstacle, margin))
        solve(actor, position, {}, obstacle, margin, &contacts);
    if (contacts.overflow)
      return false;
    if (!contacts.size)
      break;
    recovered = true;
    Vec2 recovery{};
    for (unsigned i = 0; i < contacts.size; ++i) {
      const auto &c = contacts.data[i];
      const Vec2 n = normalized(sub(c.actor, c.obstacle));
      const float depth = dot(n, add(c.actor, recovery)) - dot(n, c.obstacle);
      if (depth > minimum_depth + epsilon)
        recovery = sub(recovery, mul(n, (depth - minimum_depth) * 0.4f));
    }
    if (zero(recovery))
      break;
    position = add(position, recovery);
    if (!bounded(position))
      return false;
  }
  float safe = 1, unsafe = 1;
  record_query(actor, position, motion, margin, queries);
  for (const auto &obstacle : obstacles) {
    if (!nearby(actor, position, motion, obstacle, margin) ||
        !solve(actor, position, motion, obstacle, 0))
      continue;
    if (solve(actor, position, {}, obstacle, 0)) {
      safe = unsafe = 0;
      break;
    }
    float low = 0, high = 1, coefficient = 0.5f;
    for (unsigned iteration = 0; iteration < 8; ++iteration) {
      const float fraction = low + (high - low) * coefficient;
      if (solve(actor, position, mul(motion, fraction), obstacle, 0)) {
        high = fraction;
        coefficient = (iteration == 0 || low > 0) ? 0.5f : 0.25f;
      } else {
        low = fraction;
        coefficient = (iteration == 0 || high < 1) ? 0.5f : 0.75f;
      }
    }
    if (low < safe) {
      safe = low;
      unsafe = high;
    }
  }
  Vec2 normal{};
  float best_length = 0;
  if (recovered || safe < 1) {
    const Vec2 contact_position = add(position, mul(motion, unsafe));
    record_query(actor, contact_position, {}, margin, queries);
    const float allowed_depth = std::min(length(motion), minimum_depth);
    for (const auto &obstacle : obstacles) {
      if (!nearby(actor, contact_position, {}, obstacle, margin))
        continue;
      Contacts contacts;
      solve(actor, contact_position, {}, obstacle, margin, &contacts);
      if (contacts.overflow)
        return false;
      for (unsigned i = 0; i < contacts.size; ++i) {
        const Vec2 relative =
            sub(contacts.data[i].obstacle, contacts.data[i].actor);
        const float d = length(relative);
        if (d < allowed_depth || d <= best_length)
          continue;
        best_length = d;
        normal = div(relative, d);
      }
    }
  }
  const bool collided = best_length != 0;
  result.position = add(position, mul(motion, collided ? safe : 1.0f));
  result.remainder = collided ? sub(motion, mul(motion, safe)) : Vec2{};
  result.normal = normal;
  result.collided = collided;
  return bounded(result.position) && bounded(result.remainder) &&
         bounded(normal);
}
} // namespace

bool podunk_prepare_shape(const FieldGeometryActor &a, PodunkSolidShape &out,
                          std::string &e) {
  auto xf = [&](Vec2 p) {
    return add(add(mul(a.transform.x, p.x), mul(a.transform.y, p.y)),
               a.transform.origin);
  };
  PodunkSolidShape s;
  if (a.kind == FieldGeometryKind::Circle) {
    const float xx = dot(a.transform.x, a.transform.x),
                yy = dot(a.transform.y, a.transform.y);
    if (a.radius <= 0 || !std::isfinite(a.radius) || xx <= 0 || xx != yy ||
        dot(a.transform.x, a.transform.y) != 0) {
      e = "Kinematic analytic circle transform rejected";
      return false;
    }
    s.circle = true;
    s.center = a.transform.origin;
    s.radius = a.radius * std::sqrt(xx);
    s.minimum = sub(s.center, {s.radius, s.radius});
    s.maximum = add(s.center, {s.radius, s.radius});
    out = std::move(s);
    return true;
  }
  auto points = a.points;
  if (a.kind == FieldGeometryKind::Rectangle) {
    if (a.extents.x <= 0 || a.extents.y <= 0) {
      e = "Kinematic rectangle extents rejected";
      return false;
    }
    points = {{-a.extents.x, -a.extents.y},
              {a.extents.x, -a.extents.y},
              {a.extents.x, a.extents.y},
              {-a.extents.x, a.extents.y}};
  } else if (a.kind != FieldGeometryKind::Convex &&
             a.kind != FieldGeometryKind::Segment) {
    e = "Kinematic unsupported source solid primitive";
    return false;
  }
  if (points.size() < 2 || points.size() > 4096) {
    e = "Kinematic source solid point count rejected";
    return false;
  }
  s.segment = a.kind == FieldGeometryKind::Segment;
  float area = 0;
  for (auto p : points) {
    auto v = xf(p);
    if (!bounded(v)) {
      e = "Kinematic solid transform overflow";
      return false;
    }
    s.vertices.push_back(v);
  }
  for (size_t i = 0; i < s.vertices.size(); ++i)
    area += cross(s.vertices[i], s.vertices[(i + 1) % s.vertices.size()]);
  if (area < 0)
    std::reverse(s.vertices.begin(), s.vertices.end());
  for (size_t i = 0; i < s.vertices.size(); ++i) {
    auto edge = sub(s.vertices[(i + 1) % s.vertices.size()], s.vertices[i]);
    s.normals.push_back(normalized({-edge.y, edge.x}));
  }
  s.minimum = s.maximum = s.vertices[0];
  for (auto p : s.vertices) {
    s.minimum.x = std::min(s.minimum.x, p.x);
    s.minimum.y = std::min(s.minimum.y, p.y);
    s.maximum.x = std::max(s.maximum.x, p.x);
    s.maximum.y = std::max(s.maximum.y, p.y);
  }
  if (s.segment && s.vertices.size() != 2) {
    e = "Kinematic native segment count rejected";
    return false;
  }
  out = std::move(s);
  return true;
}
bool podunk_solve_motion(const PodunkSolidShape &actor,
                         const std::vector<PodunkSolidShape> &obstacles,
                         Vec2 position, Vec2 velocity, float delta,
                         float margin, SlideResult &result,
                         MotionQueryBounds &bounds, std::string &e) {
  if (actor.circle || actor.vertices.size() < 3 || !bounded(position) ||
      !bounded(velocity) || !std::isfinite(delta) || delta < 0 ||
      !std::isfinite(margin) || margin < 0) {
    e = "Kinematic actual motion inputs rejected";
    return false;
  }
  MotionQueryBounds queries;
  Vec2 motion = mul(velocity, delta);
  for (unsigned iteration = 0; iteration < 4; ++iteration) {
    Motion moved;
    if (!move(actor, obstacles, position, motion, moved, margin, &queries)) {
      e = "Kinematic source recovery/contact capacity rejected";
      return false;
    }
    position = moved.position;
    if (!moved.collided)
      break;
    motion = projected_slide(moved.remainder, moved.normal);
    velocity = projected_slide(velocity, moved.normal);
    if (zero(motion))
      break;
  }
  if (!bounded(position) || !bounded(velocity)) {
    e = "Kinematic source motion result overflow";
    return false;
  }
  result = {position, velocity};
  bounds = queries;
  return true;
}
} // namespace encore::upstream
