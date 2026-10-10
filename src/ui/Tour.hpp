#pragma once

#include <Geode/Geode.hpp>

#include <functional>
#include <string>
#include <vector>

// ! --- Tour --- !
// A few steps over a window: everything dims but one thing, framed, with a bubble of text and
// Next / Skip. Touches don't reach the window while it shows.

struct TourStep
{
  std::vector<cocos2d::CCNode *> nodes; // framed together
  std::string title;
  std::string text;
};

class TourLayer : public cocos2d::CCLayer
{
public:
  // Over `owner` (a full-screen popup); `done` runs at the end or on Skip
  static TourLayer *create(std::vector<TourStep> steps, std::function<void()> done);

  bool ccTouchBegan(cocos2d::CCTouch *touch, cocos2d::CCEvent *event) override;
  void ccTouchMoved(cocos2d::CCTouch *, cocos2d::CCEvent *) override {}
  void ccTouchEnded(cocos2d::CCTouch *, cocos2d::CCEvent *) override {}
  void ccTouchCancelled(cocos2d::CCTouch *, cocos2d::CCEvent *) override {}
  void onEnter() override;
  void onExit() override;

private:
  bool initTour(std::vector<TourStep> steps, std::function<void()> done);
  void showStep();
  void finish();
  cocos2d::CCRect frameOf(TourStep const &step);

  std::vector<TourStep> m_steps;
  std::function<void()> m_done;
  size_t m_index = 0;
  cocos2d::CCNode *m_content = nullptr; // rebuilt for every step
};
