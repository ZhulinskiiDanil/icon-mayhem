#include "HairNode.hpp"

#include <array>

using namespace geode::prelude;

// ! --- Finding a part --- !
// Every part remembers the triangles it drew this frame (see drawPart in redraw()), so tapping the
// preview finds exactly what is under the finger, the topmost part first

namespace
{
  float cross(CCPoint const &a, CCPoint const &b, CCPoint const &p)
  {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
  }

  bool inTriangle(CCPoint const &p, CCPoint const &a, CCPoint const &b, CCPoint const &c)
  {
    float const ab = cross(a, b, p);
    float const bc = cross(b, c, p);
    float const ca = cross(c, a, p);
    bool const anyNegative = ab < 0.f || bc < 0.f || ca < 0.f;
    bool const anyPositive = ab > 0.f || bc > 0.f || ca > 0.f;
    return !(anyNegative && anyPositive);
  }

  CCPoint pointOf(ccV2F_C4B_T2F const &vertex)
  {
    return {vertex.vertices.x, vertex.vertices.y};
  }
}

std::string HairNode::partAtExactly(CCPoint const &world) const
{
  // Drawn later is on top
  for (auto drawn = m_drawn.rbegin(); drawn != m_drawn.rend(); ++drawn)
  {
    auto node = drawn->node;
    if (!node || !node->isVisible() || drawn->to > node->m_nBufferCount)
      continue;

    CCPoint const local = node->convertToNodeSpace(world);
    for (GLsizei i = drawn->from; i + 2 < drawn->to; i += 3)
    {
      auto const &a = node->m_pBuffer[i];
      auto const &b = node->m_pBuffer[i + 1];
      auto const &c = node->m_pBuffer[i + 2];
      // Invisible glow or a hidden piece
      if (a.colors.a == 0 && b.colors.a == 0 && c.colors.a == 0)
        continue;
      if (inTriangle(local, pointOf(a), pointOf(b), pointOf(c)))
        return drawn->part;
    }
  }
  return "";
}

std::string HairNode::partAt(CCPoint const &world, float slop) const
{
  if (auto part = this->partAtExactly(world); !part.empty())
    return part;
  if (slop <= 0.f)
    return "";

  // A thin lock is hard to hit: a little around the finger too
  static constexpr std::array<std::pair<float, float>, 8> kAround{
      std::pair{1.f, 0.f}, {-1.f, 0.f}, {0.f, 1.f}, {0.f, -1.f}, {.7f, .7f}, {-.7f, .7f}, {.7f, -.7f}, {-.7f, -.7f}};
  for (float reach : {.5f, 1.f})
  {
    for (auto [x, y] : kAround)
    {
      if (auto part = this->partAtExactly(world + CCPoint{x, y} * (slop * reach)); !part.empty())
        return part;
    }
  }
  return "";
}

void HairNode::partShape(std::string_view part, CCAffineTransform const &worldToTarget, std::vector<CCPoint> &triangles) const
{
  for (auto const &drawn : m_drawn)
  {
    auto node = drawn.node;
    if (part != drawn.part || !node || !node->isVisible() || drawn.to > node->m_nBufferCount)
      continue;
    auto const toTarget = CCAffineTransformConcat(node->nodeToWorldTransform(), worldToTarget);
    for (GLsizei i = drawn.from; i + 2 < drawn.to; i += 3)
    {
      auto const &a = node->m_pBuffer[i];
      auto const &b = node->m_pBuffer[i + 1];
      auto const &c = node->m_pBuffer[i + 2];
      if (a.colors.a == 0 && b.colors.a == 0 && c.colors.a == 0)
        continue;
      triangles.push_back(CCPointApplyAffineTransform(pointOf(a), toTarget));
      triangles.push_back(CCPointApplyAffineTransform(pointOf(b), toTarget));
      triangles.push_back(CCPointApplyAffineTransform(pointOf(c), toTarget));
    }
  }
}
