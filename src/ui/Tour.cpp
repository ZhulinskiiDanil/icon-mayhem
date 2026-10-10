#include "Tour.hpp"

#include "Buttons.hpp"

#include <Geode/ui/NineSlice.hpp>

#include <algorithm>

using namespace geode::prelude;

namespace
{
  // Over everything of the popup, whatever priority a popup forces on what it adds
  constexpr int kTouchPriority = -2000;
  constexpr float kPadding = 4.f;
  constexpr float kBubbleWidth = 210.f;
  constexpr float kTextScale = .72f;
  constexpr GLubyte kDim = 150;
  constexpr ccColor3B kFrame = {255, 215, 90};

  // A button drawn by the tour itself: the popup's force priority would put a menu under it
  class TourButton : public CCNode
  {
  public:
    static TourButton *create(char const *text, char const *texture, float width, std::function<void()> action)
    {
      auto node = new TourButton();
      node->init();
      node->autorelease();
      auto sprite = textButton(text, width, texture, 22.f);
      node->setContentSize(sprite->getScaledContentSize());
      node->setAnchorPoint({.5f, .5f});
      sprite->setPosition(node->getContentSize() / 2.f);
      node->addChild(sprite);
      node->m_action = std::move(action);
      return node;
    }

    bool hit(CCPoint const &world)
    {
      CCPoint const local = this->convertToNodeSpace(world);
      return local.x >= 0.f && local.y >= 0.f && local.x <= this->getContentWidth() && local.y <= this->getContentHeight();
    }

    std::function<void()> m_action;
  };
}

TourLayer *TourLayer::create(std::vector<TourStep> steps, std::function<void()> done)
{
  auto layer = new TourLayer();
  if (layer->initTour(std::move(steps), std::move(done)))
  {
    layer->autorelease();
    return layer;
  }
  delete layer;
  return nullptr;
}

bool TourLayer::initTour(std::vector<TourStep> steps, std::function<void()> done)
{
  if (!CCLayer::init() || steps.empty())
    return false;
  m_steps = std::move(steps);
  m_done = std::move(done);
  this->setID("tour"_spr);
  this->setContentSize(CCDirector::sharedDirector()->getWinSize());
  return true;
}

void TourLayer::onEnter()
{
  CCLayer::onEnter();
  CCDirector::sharedDirector()->getTouchDispatcher()->addPrioTargetedDelegate(this, kTouchPriority, true);
  this->showStep();
}

void TourLayer::onExit()
{
  CCDirector::sharedDirector()->getTouchDispatcher()->removeDelegate(this);
  CCLayer::onExit();
}

CCRect TourLayer::frameOf(TourStep const &step)
{
  // Every node of the step, framed together, in this layer
  float left = 1e9f, bottom = 1e9f, right = -1e9f, top = -1e9f;
  for (auto node : step.nodes)
  {
    if (!node || !nodeIsVisible(node))
      continue;
    auto const size = node->getContentSize();
    for (CCPoint corner : {CCPoint{0.f, 0.f}, CCPoint{size.width, 0.f}, CCPoint{0.f, size.height}, CCPoint{size.width, size.height}})
    {
      CCPoint const point = this->convertToNodeSpace(node->convertToWorldSpace(corner));
      left = std::min(left, point.x);
      right = std::max(right, point.x);
      bottom = std::min(bottom, point.y);
      top = std::max(top, point.y);
    }
  }
  if (left > right)
    return {0.f, 0.f, 0.f, 0.f};
  return {left - kPadding, bottom - kPadding, right - left + 2.f * kPadding, top - bottom + 2.f * kPadding};
}

void TourLayer::showStep()
{
  if (m_content)
    m_content->removeFromParent();
  m_content = CCNode::create();
  this->addChild(m_content);

  auto const screen = this->getContentSize();
  auto const &step = m_steps[m_index];
  CCRect const frame = this->frameOf(step);

  // Dark all around the frame
  auto dim = [&](float x, float y, float w, float h)
  {
    if (w <= 0.f || h <= 0.f)
      return;
    auto layer = CCLayerColor::create({0, 0, 0, kDim}, w, h);
    layer->setPosition({x, y});
    m_content->addChild(layer);
  };
  dim(0.f, frame.getMaxY(), screen.width, screen.height - frame.getMaxY());
  dim(0.f, 0.f, screen.width, frame.getMinY());
  dim(0.f, frame.getMinY(), frame.getMinX(), frame.size.height);
  dim(frame.getMaxX(), frame.getMinY(), screen.width - frame.getMaxX(), frame.size.height);

  auto border = CCDrawNode::create();
  CCPoint const corners[4] = {{frame.getMinX(), frame.getMinY()}, {frame.getMaxX(), frame.getMinY()},
                              {frame.getMaxX(), frame.getMaxY()}, {frame.getMinX(), frame.getMaxY()}};
  for (int i = 0; i < 4; ++i)
    border->drawSegment(corners[i], corners[(i + 1) % 4], 1.f, ccc4FFromccc3B(kFrame));
  m_content->addChild(border);

  // The bubble: title, text, the step count and the buttons
  auto title = CCLabelBMFont::create(step.title.c_str(), "goldFont.fnt");
  title->limitLabelWidth(kBubbleWidth - 50.f, .55f, .1f);
  auto text = CCLabelBMFont::create(step.text.c_str(), "chatFont.fnt", (kBubbleWidth - 16.f) / kTextScale, kCCTextAlignmentLeft);
  text->setScale(kTextScale);
  auto count = CCLabelBMFont::create(fmt::format("{}/{}", m_index + 1, m_steps.size()).c_str(), "bigFont.fnt");
  count->setScale(.3f);
  count->setOpacity(170);

  float const textHeight = text->getScaledContentHeight();
  float const height = 18.f + 8.f + textHeight + 34.f;
  auto bubble = NineSlice::create("square02_small.png");
  bubble->setColor({0, 0, 0});
  bubble->setOpacity(225);
  bubble->setContentSize({kBubbleWidth, height});
  bubble->setAnchorPoint({0.f, 0.f});

  // Under the frame when there is room, else over it, else beside it
  CCPoint at;
  if (frame.getMinY() - height - 8.f > 4.f)
    at = CCPoint{frame.getMidX() - kBubbleWidth / 2.f, frame.getMinY() - height - 8.f};
  else if (frame.getMaxY() + height + 8.f < screen.height - 4.f)
    at = CCPoint{frame.getMidX() - kBubbleWidth / 2.f, frame.getMaxY() + 8.f};
  else if (frame.getMaxX() + kBubbleWidth + 8.f < screen.width)
    at = CCPoint{frame.getMaxX() + 8.f, frame.getMidY() - height / 2.f};
  else
    at = CCPoint{frame.getMinX() - kBubbleWidth - 8.f, frame.getMidY() - height / 2.f};
  at.x = std::clamp(at.x, 4.f, screen.width - kBubbleWidth - 4.f);
  at.y = std::clamp(at.y, 4.f, screen.height - height - 4.f);
  bubble->setPosition(at);
  m_content->addChild(bubble);

  title->setAnchorPoint({0.f, .5f});
  title->setPosition({8.f, height - 12.f});
  bubble->addChild(title);
  count->setAnchorPoint({1.f, .5f});
  count->setPosition({kBubbleWidth - 8.f, height - 12.f});
  bubble->addChild(count);
  text->setAnchorPoint({0.f, 1.f});
  text->setPosition({8.f, height - 24.f});
  bubble->addChild(text);

  bool const last = m_index + 1 == m_steps.size();
  auto skip = TourButton::create("Skip", "GJ_button_04.png", 50.f, [this]
                                 { this->finish(); });
  skip->setPosition({8.f + 25.f, 16.f});
  skip->setTag(1);
  bubble->addChild(skip);
  auto next = TourButton::create(last ? "Done" : "Next", "GJ_button_01.png", 56.f, [this, last]
                                 {
                                   if (last)
                                   {
                                     this->finish();
                                     return;
                                   }
                                   ++m_index;
                                   this->showStep();
                                 });
  next->setPosition({kBubbleWidth - 8.f - 28.f, 16.f});
  next->setTag(2);
  bubble->addChild(next);
  if (last)
    skip->setVisible(false);
}

bool TourLayer::ccTouchBegan(CCTouch *touch, CCEvent *)
{
  // Every touch stays here; a tap on a button of the bubble presses it
  if (!m_content)
    return true;
  std::vector<TourButton *> buttons;
  std::function<void(CCNode *)> collect = [&](CCNode *node)
  {
    for (auto child : CCArrayExt<CCNode *>(node->getChildren()))
    {
      if (auto button = typeinfo_cast<TourButton *>(child); button && button->isVisible())
        buttons.push_back(button);
      collect(child);
    }
  };
  collect(m_content);
  for (auto button : buttons)
  {
    if (button->hit(touch->getLocation()))
    {
      // Run after this touch, the step is built again
      auto action = button->m_action;
      Loader::get()->queueInMainThread([self = Ref(this), action]
                                       {
                                         if (self->getParent() && action)
                                           action();
                                       });
      break;
    }
  }
  return true;
}

void TourLayer::finish()
{
  auto done = m_done;
  this->removeFromParent();
  if (done)
    done();
}
