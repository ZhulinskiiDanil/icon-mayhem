#include "HairNode.hpp"
#include "HairShared.hpp"
#include "LevelQuery.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

using namespace geode::prelude;
using namespace hair;

// ! --- Face --- !
// Detailed anime eyes (textures from tools/make_eyes.py) and a drawn mouth that see the level: they look where you go, at the next
// spike or orb ahead, get scared right before a spike, squint at high speed, blink, look up in
// jumps, smile on checkpoints and cross out on death. Sizes are in icon units, the look is kept in
// the icon frame: x along the face to the right, y up.

namespace
{
  constexpr float kEyeRadius = 3.2f;
  constexpr float kLookRange = 180.f;  // units ahead the eyes watch (6 blocks)
  constexpr float kLookReach = 90.f;   // units to the sides
  constexpr float kScareDistance = 75.f; // closer than this a spike is scary
  constexpr float kQueryEvery = .05f;  // s between level queries
  constexpr float kLookSpeed = 9.f;    // 1 / s, how fast the eyes turn
  constexpr float kBlinkTime = .12f;   // s
  constexpr float kDeadEyesTime = .8f; // s the X eyes stay after a death
  constexpr float kSpeedSquint = .2f;        // how much the eyes narrow at high speed

  // The eye textures (tools/make_eyes.py): the right eye on a 256 x 176 canvas, the white 170 px
  // wide, the iris on its own canvas resting a bit above the middle
  constexpr bool kEyesMove = false;          // looking, blinking and moods are off for now
  constexpr float kEyeWidth = 11.f;          // icon units, the white of one eye
  constexpr float kCanvasPixels = 256.f;

  // Every type in every mood has its own shape (tools/make_eyes.py): how wide the white is on the
  // canvas and where the iris rests, in texture pixels from the middle of the canvas, y up
  struct EyeShape
  {
    float scleraPixels;
    float irisX, irisY;
  };

#include "EyeShapes.inc"

  int eyeDesign(FaceLook look)
  {
    switch (look)
    {
    case FaceLook::Sapphire:
    case FaceLook::SapphireHearts:
      return 1;
    case FaceLook::Crimson:
      return 2;
    default:
      return 0;
    }
  }

  EyeShape eyeShape(FaceLook look, FaceShape shape)
  {
    return kEyeShapes[eyeDesign(look)][static_cast<int>(shape)];
  }

  // Where an eye sits from the middle of the face: the spread along the eye line turned by the
  // tilt, the height, then both shifted to the front. `side` +1 is the front eye
  CCPoint eyeOffset(HairConfig const &config, float side, CCPoint const &right, CCPoint const &up, float scale)
  {
    float const tilt = radians(config.faceTilt);
    CCPoint const line = right * std::cos(tilt) + up * std::sin(tilt);
    return line * (side * config.faceX * scale) + up * (config.faceY * scale) + right * (config.faceShiftX * scale);
  }

  // The texture names of a mood: none for the default shape
  constexpr std::array<char const *, 9> kShapeSuffixes = {"", "-flirty", "-sultry", "-angry", "-kind",
                                                          "-cheerful", "-judging", "-sad", "-surprised"};

  // One number for the built style and shape
  int eyeBuildKey(FaceLook look, FaceShape shape)
  {
    return static_cast<int>(look) * 16 + static_cast<int>(shape);
  }
  constexpr float kIrisTravelX = 26.f;       // texture pixels the iris moves looking around
  constexpr float kIrisTravelY = 10.f;
  constexpr float kMouthDrop = 8.f;          // icon units from the eyes down to the mouth
  constexpr ccColor3B kMouth = {90, 30, 45};
  constexpr ccColor3B kTongue = {255, 128, 150};

}

// ! --- Update --- !

void HairNode::updateFace(float dt, CCPoint const &headCenter, bool tookOff)
{
  // The eyes don't move for now: no looking, blinking or moods until they are drawn for it
  if (m_config.face == FaceStyle::None || !kEyesMove)
    return;

  std::uniform_real_distribution<float> random(0.f, 1.f);
  CCPoint const right = m_iconBack * -1.f;

  // Where to look: where you go, or at the next spike or orb ahead
  CCPoint look{std::sin(m_time * .7f) * .35f, std::sin(m_time * .53f) * .2f};
  float const speed = m_headVelocity.getLength();
  if (speed > 20.f * m_simScale)
  {
    CCPoint const dir = m_headVelocity / speed;
    look = CCPoint{dir.dot(right), dir.dot(m_iconUp)} * .7f;
  }

  m_faceQueryTimer -= dt;
  if (m_config.faceWatch && m_faceQueryTimer <= 0.f)
  {
    m_faceQueryTimer = kQueryEvery;
    CCPoint const ahead = {m_facing >= 0.f ? 1.f : -1.f, 0.f};
    m_watchTarget = level::nearestHazard(headCenter, ahead, kLookRange, kLookReach);
    m_watchIsHazard = m_watchTarget.has_value();
    if (auto booster = level::nearestBooster(headCenter, ahead, kLookRange * .6f, kLookReach))
    {
      if (!m_watchTarget || booster->getDistance(headCenter) < m_watchTarget->getDistance(headCenter))
      {
        m_watchTarget = booster;
        m_watchIsHazard = false;
      }
    }
  }

  float scare = 0.f;
  if (m_config.faceWatch && m_watchTarget)
  {
    CCPoint const toTarget = *m_watchTarget - headCenter;
    float const distance = toTarget.getLength();
    if (distance > 1.f)
    {
      CCPoint const dir = toTarget / distance;
      look = CCPoint{dir.dot(right), dir.dot(m_iconUp)};
    }
    if (m_watchIsHazard)
      scare = std::clamp((kScareDistance - distance) / (kScareDistance * .6f), 0.f, 1.f);
  }

  // Jumps look up for a moment
  if (tookOff)
    m_faceJump = .3f;
  m_faceJump = std::max(0.f, m_faceJump - dt);
  if (m_faceJump > 0.f)
    look = look + CCPoint{0.f, .5f};

  float const length = look.getLength();
  if (length > 1.f)
    look = look / length;
  float const turn = std::min(1.f, kLookSpeed * dt);
  m_faceLook = m_faceLook + (look - m_faceLook) * turn;
  m_faceScare = approach(m_faceScare, scare, (scare > m_faceScare ? 8.f : 2.f) * dt);

  // Blinks every few seconds, sometimes twice
  m_faceBlink -= dt;
  if (m_faceBlink < -kBlinkTime)
  {
    bool const twice = random(m_random) < .2f;
    m_faceBlink = twice ? .15f : 2.f + 3.f * random(m_random);
  }

  m_faceMoodTime = std::max(0.f, m_faceMoodTime - dt);
  if (m_faceMoodTime <= 0.f)
    m_faceMood = FaceMood::Normal;
}

void HairNode::setFaceMood(FaceMood mood, float duration)
{
  m_faceMood = mood;
  m_faceMoodTime = duration;
}

// ! --- Drawing --- !

void HairNode::buildEyeSprites()
{
  if (!m_eyes)
    return;
  m_eyes->removeAllChildren();
  for (auto &eye : m_eyeSprites)
    eye = EyeSprites{};

  auto const style = m_config.faceStyle;
  char const *kind = style == FaceLook::Crimson ? "crimson" : style == FaceLook::Onyx ? "onyx" : "sapphire";
  std::string const irisName = style == FaceLook::SapphireHearts ? "eye-sapphire-heart-iris.png" : fmt::format("eye-{}-iris.png", kind);
  std::string const shaped = fmt::format("eye-{}{}", kind, kShapeSuffixes[static_cast<int>(m_config.faceShape)]);
  auto sprite = [](std::string const &name)
  { return CCSprite::create(Mod::get()->expandSpriteName(name).data()); };

  for (auto &eye : m_eyeSprites)
  {
    auto sclera = sprite(shaped + "-sclera.png");
    auto stencil = sprite(shaped + "-sclera.png");
    auto iris = sprite(irisName);
    auto lashes = sprite(shaped + "-lashes.png");
    if (!sclera || !stencil || !iris || !lashes)
    {
      log::warn("Eye textures of '{}' are missing", shaped);
      return;
    }

    // The iris only shows inside the white of the eye
    auto clip = CCClippingNode::create(stencil);
    clip->setAlphaThreshold(.05f);
    clip->addChild(sclera);
    clip->addChild(iris);

    eye.root = CCNode::create();
    eye.open = CCNode::create();
    eye.open->addChild(clip);
    eye.open->addChild(lashes);
    eye.root->addChild(eye.open);
    eye.iris = iris;
    eye.sprites = {sclera, iris, lashes};
    m_eyes->addChild(eye.root);
    m_eyeTexel = sclera->getContentSize().width / kCanvasPixels;
  }
  m_eyeStyleBuilt = eyeBuildKey(style, m_config.faceShape);
}

void HairNode::hideEyes()
{
  if (m_eyes)
    m_eyes->setVisible(false);
}

void HairNode::drawFace(CCDrawNode *node)
{
  // The eyes are detailed textures: only on High quality
  if (m_config.face == FaceStyle::None || m_config.quality != Quality::High || !m_eyes)
  {
    this->hideEyes();
    return;
  }
  if (m_eyeStyleBuilt != eyeBuildKey(m_config.faceStyle, m_config.faceShape))
    this->buildEyeSprites();
  if (!m_eyeSprites[0].root)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const alpha = this->drawAlpha();
  float const outline = m_config.outline ? kOutlineWidth * scale * .8f : 0.f;
  auto const black = this->ink(alpha);

  // On the face in the icon's own frame, like the blush
  CCPoint const right = normalized(applyVec(m_iconBack * -1.f, simToNode), {1.f, 0.f});
  CCPoint const up = normalized(applyVec(m_iconUp, simToNode), {0.f, 1.f});
  CCPoint const center = CCPointApplyAffineTransform(m_frameParams.headCenter, simToNode);
  float const faceSize = (m_config.faceScaleX + m_config.faceScaleY) * .5f;
  float const r = kEyeRadius * faceSize * scale;
  // The eye layer shares the parent of the front node, at the same place
  CCPoint const shift = m_front->getPosition() - m_eyes->getPosition();

  auto const shape = eyeShape(m_config.faceStyle, m_config.faceShape);
  // Node units per texture pixel, sideways and up: the eye width and height settings stretch it
  float const pixel = kEyeWidth * scale / shape.scleraPixels / std::max(m_eyeTexel, .0001f);
  // The eyes turn with the eye line
  float const tilt = radians(m_config.faceTilt);
  CCPoint const line = right * std::cos(tilt) + up * std::sin(tilt);
  float const angle = -std::atan2(line.y, line.x) * 180.f / kPi;
  GLubyte const opacity = static_cast<GLubyte>(std::clamp(alpha, 0.f, 1.f) * 255.f);

  m_eyes->setVisible(true);
  for (int i = 0; i < 2; ++i)
  {
    auto &eye = m_eyeSprites[i];
    float const side = i == 0 ? -1.f : 1.f;
    CCPoint const position = center + eyeOffset(m_config, side, right, up, scale) + shift;

    // The textures are the right eye: the left one is mirrored
    eye.root->setPosition(position);
    eye.root->setRotation(angle);
    eye.root->setScaleX(side * pixel * m_config.faceScaleX);
    eye.root->setScaleY(pixel * m_config.faceScaleY);
    eye.iris->setPosition(CCPoint{shape.irisX, shape.irisY} * m_eyeTexel);

    for (auto sprite : eye.sprites)
      sprite->setOpacity(opacity);
  }

  if (m_config.face != FaceStyle::EyesAndMouth)
    return;

  // Mouth: a smile, an "o" when scared or surprised, a grin when happy, a little line asleep
  CCPoint const mouth = center + up * ((m_config.faceY - kMouthDrop * m_config.faceScaleY) * scale) + right * (m_config.faceShiftX * scale);
  bool const happy = false;
  float const fright = 0.f;
  float const m = r * .42f;
  auto const mouthInk = premultiplied(kMouth, alpha);
  if (fright > .5f)
  {
    if (outline > 0.f)
      fillEllipse(node, mouth, up, m * .6f + outline, m * .45f + outline, black);
    fillEllipse(node, mouth, up, m * .6f, m * .45f, mouthInk);
  }
  else if (happy)
  {
    std::vector<CCPoint> grin;
    for (int i = 0; i <= 8; ++i)
    {
      float const t = kPi * static_cast<float>(i) / 8.f;
      grin.push_back(mouth + right * (std::cos(t) * m) - up * (std::sin(t) * m * .8f));
    }
    node->drawPolygon(grin.data(), static_cast<unsigned>(grin.size()), mouthInk, outline * .6f, black);
    fillEllipse(node, mouth - up * (m * .5f), right, m * .35f, m * .2f, premultiplied(kTongue, alpha));
  }
  else if (m_sleepy > .6f)
    node->drawSegment(mouth - right * (m * .4f), mouth + right * (m * .4f), r * .1f, mouthInk);
  else
  {
    CCPoint previous = mouth + right * (-m * .7f) + up * (m * .15f);
    for (int i = 1; i <= 6; ++i)
    {
      float const t = static_cast<float>(i) / 6.f;
      CCPoint const point = mouth + right * ((t * 2.f - 1.f) * m * .7f) + up * (m * .15f - std::sin(kPi * t) * m * .35f);
      node->drawSegment(previous, point, r * .1f, mouthInk);
      previous = point;
    }
  }
}

void HairNode::drawDeadFace(CCDrawNode *node, float age)
{
  if (m_config.face == FaceStyle::None || age > kDeadEyesTime || !kEyesMove)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const fade = std::clamp((kDeadEyesTime - age) / .3f, 0.f, 1.f);
  auto const ink = this->ink(fade);

  CCPoint const right = normalized(applyVec(m_iconBack * -1.f, simToNode), {1.f, 0.f});
  CCPoint const up = normalized(applyVec(m_iconUp, simToNode), {0.f, 1.f});
  CCPoint const center = CCPointApplyAffineTransform(m_frameParams.headCenter, simToNode);
  float const r = kEyeRadius * (m_config.faceScaleX + m_config.faceScaleY) * .5f * scale * .7f;
  for (float side : {-1.f, 1.f})
  {
    CCPoint const eye = center + eyeOffset(m_config, side, right, up, scale);
    node->drawSegment(eye - right * r - up * r, eye + right * r + up * r, r * .25f, ink);
    node->drawSegment(eye - right * r + up * r, eye + right * r - up * r, r * .25f, ink);
  }
}
