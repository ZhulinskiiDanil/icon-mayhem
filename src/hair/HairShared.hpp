#pragma once

#include <Geode/Geode.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <numbers>

// ! --- Hair shared --- !
// Constants, small math and drawing helpers shared by HairNode.cpp, Extras.cpp and Decor.cpp

namespace hair
{
  constexpr float kPi = std::numbers::pi_v<float>;

  constexpr float kBaseGravity = 900.f; // units / s^2 for gravity = 1
  constexpr float kHeadRadius = 15.f;   // hair units, half of the cube
  constexpr float kSpringScale = 13.f;  // style spring vs gravity, see HairNode::buildTargets()

  constexpr float kTipTaper = .92f;    // how much thinner the tip is than the root
  constexpr float kOutlineWidth = .6f; // icon units
  constexpr float kOutlineTaper = .6f; // outline thins towards the tip too, no blobs on the ends
  constexpr float kBackShade = .72f;   // brightness of the deepest lock

  inline cocos2d::CCPoint applyVec(cocos2d::CCPoint const &v, cocos2d::CCAffineTransform const &t)
  {
    return {t.a * v.x + t.c * v.y, t.b * v.x + t.d * v.y};
  }

  inline cocos2d::CCPoint normalized(cocos2d::CCPoint const &v, cocos2d::CCPoint const &fallback)
  {
    float const len = v.getLength();
    return len > .0001f ? v / len : fallback;
  }

  inline float radians(float degrees)
  {
    return degrees * kPi / 180.f;
  }

  inline cocos2d::CCPoint rotated(cocos2d::CCPoint const &v, float radians)
  {
    float const c = std::cos(radians);
    float const s = std::sin(radians);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
  }

  inline float approach(float value, float target, float maxDelta)
  {
    return value + std::clamp(target - value, -maxDelta, maxDelta);
  }

  inline cocos2d::ccColor4F premultiplied(cocos2d::ccColor3B const &color, float alpha)
  {
    return {
        color.r / 255.f * alpha,
        color.g / 255.f * alpha,
        color.b / 255.f * alpha,
        alpha,
    };
  }

  inline cocos2d::ccColor4F shaded(cocos2d::ccColor4F const &color, float shade)
  {
    return {color.r * shade, color.g * shade, color.b * shade, color.a};
  }

  inline cocos2d::CCPoint catmullRom(cocos2d::CCPoint const &p0, cocos2d::CCPoint const &p1, cocos2d::CCPoint const &p2, cocos2d::CCPoint const &p3, float t)
  {
    float const t2 = t * t;
    float const t3 = t2 * t;
    return (p1 * 2.f + (p2 - p0) * t + (p0 * 2.f - p1 * 5.f + p2 * 4.f - p3) * t2 + (p1 * 3.f - p0 - p2 * 3.f + p3) * t3) * .5f;
  }

  // Smooth curve through the simulated points, already moved into the hair node space
  inline void buildCurve(std::vector<cocos2d::CCPoint> const &pts, cocos2d::CCAffineTransform const &toHair, int subdiv,
                         std::vector<cocos2d::CCPoint> &out)
  {
    int const n = static_cast<int>(pts.size());
    auto at = [&](int i)
    {
      if (i < 0)
        return pts[0] * 2.f - pts[1];
      if (i >= n)
        return pts[n - 1] * 2.f - pts[n - 2];
      return pts[i];
    };

    out.clear();
    for (int i = 0; i + 1 < n; ++i)
    {
      for (int j = 0; j < subdiv; ++j)
      {
        float const t = static_cast<float>(j) / static_cast<float>(subdiv);
        out.push_back(CCPointApplyAffineTransform(catmullRom(at(i - 1), at(i), at(i + 1), at(i + 2), t), toHair));
      }
    }
    out.push_back(CCPointApplyAffineTransform(pts[n - 1], toHair));
  }

  // Premultiplied color mixed towards white by `amount`, keeping its opacity
  inline cocos2d::ccColor4F mixedWhite(cocos2d::ccColor4F const &color, float amount)
  {
    return {color.r + (color.a - color.r) * amount, color.g + (color.a - color.g) * amount, color.b + (color.a - color.b) * amount, color.a};
  }

  // Premultiplied color with its opacity scaled
  inline cocos2d::ccColor4F faded(cocos2d::ccColor4F const &color, float alpha)
  {
    return {color.r * alpha, color.g * alpha, color.b * alpha, color.a * alpha};
  }

  inline cocos2d::CCPoint perpendicular(cocos2d::CCPoint const &v)
  {
    return {-v.y, v.x};
  }

  // A filled ellipse from a polygon
  inline void fillEllipse(cocos2d::CCDrawNode *node, cocos2d::CCPoint const &center, cocos2d::CCPoint const &axis, float radiusAlong,
                          float radiusAcross, cocos2d::ccColor4F const &color, float outline = 0.f,
                          cocos2d::ccColor4F const &outlineColor = {0.f, 0.f, 0.f, 1.f})
  {
    constexpr int kPoints = 16;
    std::array<cocos2d::CCPoint, kPoints> points;
    cocos2d::CCPoint const minor = perpendicular(axis);
    for (int i = 0; i < kPoints; ++i)
    {
      float const angle = 2.f * kPi * static_cast<float>(i) / static_cast<float>(kPoints);
      points[i] = center + axis * (std::cos(angle) * radiusAlong) + minor * (std::sin(angle) * radiusAcross);
    }
    node->drawPolygon(points.data(), kPoints, color, outline, outlineColor);
  }

  // A filled circle. CCDrawNode::drawDot draws a square in GD, so circles are polygons
  inline void fillCircle(cocos2d::CCDrawNode *node, cocos2d::CCPoint const &center, float radius, cocos2d::ccColor4F const &color)
  {
    constexpr int kPoints = 18;
    std::array<cocos2d::CCPoint, kPoints> points;
    for (int i = 0; i < kPoints; ++i)
    {
      float const angle = 2.f * kPi * static_cast<float>(i) / static_cast<float>(kPoints);
      points[i] = center + cocos2d::CCPoint{std::cos(angle), std::sin(angle)} * radius;
    }
    node->drawPolygon(points.data(), kPoints, color, 0.f, color);
  }

  // Filled heart pointing along -`up`: two round lobes and a triangle
  inline void drawHeart(cocos2d::CCDrawNode *node, cocos2d::CCPoint const &center, cocos2d::CCPoint const &up, float size,
                        cocos2d::ccColor4F const &color)
  {
    cocos2d::CCPoint const side = perpendicular(up) * -1.f;
    float const lobe = size * .52f;
    fillCircle(node, center + up * (size * .28f) + side * (size * .5f), lobe, color);
    fillCircle(node, center + up * (size * .28f) - side * (size * .5f), lobe, color);
    std::array<cocos2d::CCPoint, 3> tip = {
        center + up * (size * .15f) - side * (size * .98f),
        center + up * (size * .15f) + side * (size * .98f),
        center - up * size,
    };
    node->drawPolygon(tip.data(), 3, color, 0.f, color);
  }

  // A bow: two loops tilted a bit away from the head, a fold inside each, the knot on top.
  // `outward` points away from the head, `wobble` swings the loops in degrees
  inline void drawBowShape(cocos2d::CCDrawNode *node, cocos2d::CCPoint const &knot, cocos2d::CCPoint const &outward, float wobble,
                           float size, cocos2d::ccColor4F const &color, float outline, cocos2d::ccColor4F const &outlineColor)
  {
    cocos2d::CCPoint const along = rotated(perpendicular(outward), radians(wobble));

    for (float side : {-1.f, 1.f})
    {
      cocos2d::CCPoint const dir = normalized(along * side + outward * .35f, along);
      fillEllipse(node, knot + dir * (size * .6f), dir, size * .6f, size * .38f, color, outline, outlineColor);
      fillEllipse(node, knot + dir * (size * .32f), dir, size * .2f, size * .12f, shaded(color, .75f));
    }

    if (outline > 0.f)
      fillCircle(node, knot, size * .28f + outline, outlineColor);
    fillCircle(node, knot, size * .28f, shaded(color, .85f));
  }

  // Five pointed star: five tip triangles around a pentagon, all convex pieces
  inline void drawStar(cocos2d::CCDrawNode *node, cocos2d::CCPoint const &center, cocos2d::CCPoint const &up, float outer, float inner,
                       cocos2d::ccColor4F const &color)
  {
    std::array<cocos2d::CCPoint, 5> tips;
    std::array<cocos2d::CCPoint, 5> valleys;
    for (int i = 0; i < 5; ++i)
    {
      tips[i] = center + rotated(up, radians(72.f * static_cast<float>(i))) * outer;
      valleys[i] = center + rotated(up, radians(72.f * static_cast<float>(i) + 36.f)) * inner;
    }
    node->drawPolygon(valleys.data(), 5, color, 0.f, color);
    for (int i = 0; i < 5; ++i)
    {
      std::array<cocos2d::CCPoint, 3> spike = {tips[i], valleys[i], valleys[(i + 4) % 5]};
      node->drawPolygon(spike.data(), 3, color, 0.f, color);
    }
  }
  // A charm on a short chain: swings around the pull of gravity, kicked when what it hangs on
  // speeds up or stops. Angles are radians from straight down, counterclockwise
  struct Pendulum
  {
    float angle = 0.f;
    float speed = 0.f; // radians / s

    void reset()
    {
      angle = 0.f;
      speed = 0.f;
    }

    // `down` is a unit vector, `accel` the acceleration of the anchor, `length` from the anchor to the charm
    void update(float dt, cocos2d::CCPoint const &down, cocos2d::CCPoint const &accel, float gravity, float length,
                float damping)
    {
      if (dt <= 0.f || length <= 0.f)
        return;

      // Small steps keep a short chain stable after a lag spike
      int const steps = std::clamp(static_cast<int>(std::ceil(dt * 240.f)), 1, 16);
      float const h = dt / static_cast<float>(steps);
      // In the frame of the anchor gravity pulls along `down`, the acceleration pushes the other way
      cocos2d::CCPoint const pull = down * gravity - accel;
      for (int i = 0; i < steps; ++i)
      {
        cocos2d::CCPoint const dir = rotated(down, angle);
        float const torque = dir.x * pull.y - dir.y * pull.x;
        speed += (torque / length - speed * damping) * h;
        angle = std::remainder(angle + speed * h, 2.f * kPi);
      }
    }

    cocos2d::CCPoint direction(cocos2d::CCPoint const &down) const
    {
      return rotated(down, angle);
    }
  };
}
