#include "HairNode.hpp"
#include "HairShared.hpp"

#include <algorithm>
#include <array>
#include <cmath>

using namespace geode::prelude;
using namespace hair;

// ! --- Cape --- !
// A real cloth: a cape pinned along the back of the head, a short cape, or a flag on a little pole.
// It streams behind when you move, hangs when you stand, turns over and shows its lining when you
// spin. Sizes are in icon units, the pinned edge is in the hairstyle frame like the headband.

namespace
{
  constexpr int kClothCols = 6;
  constexpr int kMaxClothRows = 18;
  constexpr float kClothGravity = .7f; // of the hair gravity, cloth is light
  constexpr float kClothDrag = 3.f;    // 1 / s at wind multiplier 1
  constexpr float kClothDamping = .6f;
  constexpr float kPoleLength = 20.f;
  constexpr ccColor3B kPoleColor = {120, 90, 70};

  struct CapeShape
  {
    float top;    // where the pinned edge starts on the back of the head, up from the middle
    float lengthScale;
  };

  CapeShape capeShape(CapeStyle style)
  {
    switch (style)
    {
    case CapeStyle::ShortCape:
      return {8.f, .55f};
    case CapeStyle::Flag:
      return {0.f, .5f};
    default:
      return {10.f, 1.f};
    }
  }
}

// ! --- Update --- !

void HairNode::capePins(CCPoint const &headCenter, std::vector<CCPoint> &pins, CCPoint &hang) const
{
  pins.clear();
  CCPoint const up = m_frameUp;
  CCPoint const back = normalized(m_frameBack, {-up.y, up.x});
  CCPoint const down = up * -1.f;
  float const unit = m_simScale;
  float const width = m_config.capeWidth;

  CCPoint from;
  CCPoint along;
  if (m_config.cape == CapeStyle::Flag)
  {
    // Down the top of a little pole standing on the back of the head
    CCPoint const base = headCenter + back * (this->headEdge(back) * .55f) + up * (this->headEdge(up) * .9f);
    CCPoint const poleDir = normalized(up + back * .15f, up);
    from = base + poleDir * (kPoleLength * unit);
    along = poleDir * -1.f;
    hang = back;
  }
  else
  {
    // Down the back of the head, from near the top
    auto const shape = capeShape(m_config.cape);
    from = headCenter + back * (this->headEdge(back) - .5f * unit) + up * (shape.top * unit);
    along = down;
    hang = normalized(back + down * .4f, back);
  }

  float const edge = std::min(width, m_config.cape == CapeStyle::Flag ? kPoleLength * .7f : width);
  for (int c = 0; c < kClothCols; ++c)
    pins.push_back(from + along * (edge * unit * static_cast<float>(c) / static_cast<float>(kClothCols - 1)));
}

void HairNode::updateCape(float dt, CCPoint const &headCenter)
{
  if (m_config.cape == CapeStyle::None)
    return;

  std::vector<CCPoint> pins;
  CCPoint hang;
  this->capePins(headCenter, pins, hang);

  // Square cells: the spacing comes from the pinned edge, the rows from the length
  float const spacing = pins.front().getDistance(pins.back()) / static_cast<float>(kClothCols - 1);
  if (spacing <= .0001f)
    return;
  float const length = m_config.capeLength * capeShape(m_config.cape).lengthScale * m_simScale;
  int const rows = std::clamp(static_cast<int>(std::round(length / spacing)) + 1, 3, kMaxClothRows);
  if (m_cloth.cols() != kClothCols || m_cloth.rows() != rows || std::abs(m_clothSpacing - spacing) > spacing * .05f)
  {
    m_cloth.setup(kClothCols, rows, spacing);
    m_clothSpacing = spacing;
  }

  m_cloth.setStepRate(m_config.simRate);
  if (m_needsReset || !m_cloth.ready())
  {
    m_cloth.reset(pins, hang);
    return;
  }

  CCPoint const down = m_gravityDir ? m_gravityDir() : CCPoint{0.f, -1.f};
  ClothParams params;
  params.gravity = down * (kBaseGravity * m_config.gravity * kClothGravity * m_simScale);
  params.drag = kClothDrag * std::max(m_config.windMultiplier, .1f);
  params.damping = kClothDamping;
  params.teleportDistance = m_frameParams.teleportDistance;
  m_cloth.step(dt, pins, hang, params);
}

// ! --- Drawing --- !

void HairNode::drawCape(CCDrawNode *node)
{
  if (m_config.cape == CapeStyle::None || !m_cloth.ready())
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const alpha = this->drawAlpha();
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;
  auto const outlineColor = this->ink(alpha);

  auto const color = m_config.capeColorSource == HairColorSource::Hair ? this->hairColor()
                                                                       : this->sourceColor(m_config.capeColorSource, m_config.capeColor);
  auto const lining = premultiplied(m_config.capeLining, alpha);
  int const cols = m_cloth.cols();
  int const rows = m_cloth.rows();
  auto at = [&](int c, int r)
  { return CCPointApplyAffineTransform(m_cloth.at(c, r), simToNode); };

  // The flag's pole first
  if (m_config.cape == CapeStyle::Flag)
  {
    CCPoint const up = m_frameUp;
    CCPoint const back = normalized(m_frameBack, {-up.y, up.x});
    CCPoint const base = CCPointApplyAffineTransform(
        m_frameParams.headCenter + back * (this->headEdge(back) * .55f) + up * (this->headEdge(up) * .9f), simToNode);
    CCPoint const tip = at(0, 0);
    auto const pole = premultiplied(kPoleColor, alpha);
    if (outline > 0.f)
      node->drawSegment(base, tip, .6f * scale + outline, outlineColor);
    node->drawSegment(base, tip, .6f * scale, pole);
    fillCircle(node, tip, 1.1f * scale + outline, outlineColor);
    fillCircle(node, tip, 1.1f * scale, lining);
  }

  // Outline along the free edges, under the sheet
  if (outline > 0.f)
  {
    for (int r = 1; r < rows; ++r)
    {
      node->drawSegment(at(0, r - 1), at(0, r), outline, outlineColor);
      node->drawSegment(at(cols - 1, r - 1), at(cols - 1, r), outline, outlineColor);
    }
    for (int c = 1; c < cols; ++c)
      node->drawSegment(at(c - 1, rows - 1), at(c, rows - 1), outline, outlineColor);
  }

  // The side facing us decides the color: a quad wound the other way shows the lining
  CCPoint const a0 = at(0, 0);
  CCPoint const a1 = at(cols - 1, 0);
  CCPoint const a2 = at(0, std::min(1, rows - 1));
  float const restSide = (a1 - a0).cross(a2 - a0) >= 0.f ? 1.f : -1.f;
  float const cell = m_clothSpacing / std::max(m_simScale, .0001f) * scale;

  for (int r = 1; r < rows; ++r)
  {
    for (int c = 1; c < cols; ++c)
    {
      CCPoint const p00 = at(c - 1, r - 1);
      CCPoint const p10 = at(c, r - 1);
      CCPoint const p11 = at(c, r);
      CCPoint const p01 = at(c - 1, r);
      bool const front = (p10 - p00).cross(p01 - p00) * restSide >= 0.f;
      auto fill = front ? color : lining;
      if (front && m_config.capePattern == CapePattern::Stripes && r % 4 >= 2)
        fill = lining;
      // Lower rows a bit darker, like folds in shade
      fill = shaded(fill, 1.f - .12f * static_cast<float>(r) / static_cast<float>(rows));

      std::array<CCPoint, 4> quad = {p00, p10, p11, p01};
      node->drawPolygon(quad.data(), 4, fill, 0.f, fill);

      // A little star or heart on every few front cells
      if (front && (c + r) % 3 == 0 &&
          (m_config.capePattern == CapePattern::Stars || m_config.capePattern == CapePattern::Hearts))
      {
        CCPoint const middle = (p00 + p10 + p11 + p01) * .25f;
        CCPoint const up = normalized(p00 - p01, {0.f, 1.f});
        if (m_config.capePattern == CapePattern::Stars)
          drawStar(node, middle, up, cell * .38f, cell * .17f, lining);
        else
          drawHeart(node, middle, up, cell * .3f, lining);
      }
    }
  }
}
