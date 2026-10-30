#include "AStar.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
namespace AStar {
bool Generator::setMaze(const loc::MazeMap &m) {
  if (!m.valid())
    return false;
  map_ = m;
  return true;
}
void Generator::addCollision(Vec2i c) {
  if (loc::valid_cell(c))
    blocked_[c.y * 5 + c.x] = true;
}
void Generator::removeCollision(Vec2i c) {
  if (loc::valid_cell(c))
    blocked_[c.y * 5 + c.x] = false;
}
void Generator::clearCollisions() {
  for (bool &b : blocked_)
    b = false;
}
CoordinateList Generator::findPath(Vec2i source, Vec2i target) {
  CoordinateList path;
  if (!map_.valid()) {
    status_ = PathStatus::NO_MAP;
    return path;
  }
  if (!loc::valid_cell(source) || !loc::valid_cell(target) ||
      blocked_[source.y * 5 + source.x] || blocked_[target.y * 5 + target.x]) {
    status_ = PathStatus::INVALID_INPUT;
    return path;
  }
  unsigned g[25];
  int parent[25];
  bool open[25]{}, closed[25]{};
  for (int i = 0; i < 25; ++i) {
    g[i] = std::numeric_limits<unsigned>::max();
    parent[i] = -1;
  }
  const int src = source.y * 5 + source.x, dst = target.y * 5 + target.x;
  g[src] = 0;
  open[src] = true;
  bool reached = false;
  const Vec2i directions[] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
  auto heuristic = [&](int index) {
    return unsigned(
        10 * (std::abs(index % 5 - target.x) + std::abs(index / 5 - target.y)));
  };
  for (int iteration = 0; iteration < 25; ++iteration) {
    int current = -1;
    unsigned score = std::numeric_limits<unsigned>::max();
    for (int i = 0; i < 25; ++i)
      if (open[i] && (g[i] + heuristic(i) < score ||
                      (g[i] + heuristic(i) == score && current >= 0 &&
                       heuristic(i) < heuristic(current)))) {
        current = i;
        score = g[i] + heuristic(i);
      }
    if (current < 0)
      break;
    if (current == dst) {
      reached = true;
      break;
    }
    open[current] = false;
    closed[current] = true;
    const Vec2i c{current % 5, current / 5};
    for (const auto &d : directions) {
      const Vec2i next{c.x + d.x, c.y + d.y};
      if (!map_.can_step(c, next))
        continue;
      const int j = next.y * 5 + next.x;
      if (blocked_[j] || closed[j])
        continue;
      const unsigned cost = g[current] + 10;
      if (!open[j] || cost < g[j]) {
        g[j] = cost;
        parent[j] = current;
        open[j] = true;
      }
    }
  }
  if (!reached) {
    status_ = PathStatus::NO_PATH;
    return path;
  }
  for (int j = dst; j >= 0 && path.count < 25; j = parent[j])
    path.cells[path.count++] = {j % 5, j / 5};
  if (path.cells[path.count - 1] != source) {
    path.count = 0;
    status_ = PathStatus::NO_PATH;
    return path;
  }
  std::reverse(path.cells, path.cells + path.count);
  status_ = PathStatus::FOUND;
  return path;
}
} // namespace AStar
