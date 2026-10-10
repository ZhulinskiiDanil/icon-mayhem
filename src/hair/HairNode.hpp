#pragma once

#include "HairConfig.hpp"
#include "HairShared.hpp"
#include "HairSim.hpp"
#include "ClothSim.hpp"

#include <Geode/Geode.hpp>
#include <array>
#include <functional>
#include <optional>
#include <random>

// ! --- Hair node --- !
// Draws the hair behind a "head" sprite (the cube, ball, wave, the cube riding a ship,
// a robot or spider head). Simulation runs in `simSpace`
// (the player's parent), so the hair reacts to the real movement of the icon.
// Everything is done in visit(), which runs after the game updated the player
// this frame, so the roots never lag behind the icon.
// The hairstyle is either glued to the top of the icon and spins with it, or oriented
// by gravity and movement direction so it always stays on top of the head.
// Face locks and bangs lie in front of the face, they share the simulation but are drawn
// by a second draw node above the icon. Extras (ponytails, braids, ahoge, bows, scarf, ears) live
// in Extras.cpp, decorations (clips, blush, stickers, headband, flowers, halo, hats) in Decor.cpp,
// particles (hearts, sparkles, weather, the sleepy Zzz, reactions) in Effects.cpp, wings in
// Wings.cpp and the pet in Pet.cpp.

class HairNode;

// Draws the particles in the sim space next to the player, so they stay visible when the player
// is hidden (after a death) and don't move with it
class HairEffectsNode : public cocos2d::CCDrawNode
{
public:
  static HairEffectsNode *create();
  void visit() override;

  HairNode *m_owner = nullptr; // cleared when the hair node leaves the scene
};

class HairNode : public cocos2d::CCDrawNode
{
public:
  // Hair grows from `head` and is drawn right behind `behind` (usually the head itself,
  // the whole body for robots and spiders). `primary` / `secondary` give the icon colors
  static HairNode *attach(cocos2d::CCSprite *head, cocos2d::CCNode *behind, cocos2d::CCSprite *primary,
                          cocos2d::CCSprite *secondary, cocos2d::CCNode *simSpace);

  void setShouldShow(std::function<bool()> fn) { m_shouldShow = std::move(fn); }
  // The game mode the icon is in, the rig hides in the modes turned off in the settings
  void setGameMode(std::function<GameMode()> fn) { m_gameModeFn = std::move(fn); }
  void setGravityDir(std::function<cocos2d::CCPoint()> fn) { m_gravityDir = std::move(fn); }
  // +1 when moving right in sim space, -1 when moving left
  void setFacing(std::function<float()> fn) { m_facingFn = std::move(fn); }
  // The head stands on the floor below it (a grounded cube or ball), hair can't go under it
  void setOnGround(std::function<bool()> fn) { m_onGround = std::move(fn); }
  // The head is a cube, the hair lies on its faces (a round collider otherwise)
  void setBoxHead(std::function<bool()> fn) { m_boxHead = std::move(fn); }
  void setIdleWind(bool enabled) { m_idleWind = enabled; }

  // The air the hair feels now, for the wind view of the customizer: where it flows (sim space),
  // how strong (the movement times Wind multiplier, plus the breeze, times the gust; 1 combs the
  // hair fully), the gust factor alone, the flutter setting and the clock of the gusts
  struct AirFlow
  {
    cocos2d::CCPoint toward;
    float strength = 0.f;
    float gust = 1.f;
    float flutter = 0.f;
    float time = 0.f;
  };
  AirFlow airFlow() const;
  void setGarage(bool garage) { m_isGarage = garage; }
  void resetSim() { m_needsReset = true; }
  // Draws the collider, the floor and where the locks grow over the icon, for tuning in the customizer
  void setDebugDraw(bool enabled) { m_debugDraw = enabled; }
  // The player died: death reactions keep playing after the hair is hidden
  void setIsDead(std::function<bool()> fn) { m_isDeadFn = std::move(fn); }
  // In a level: the headphones glow to the level music (a slow idle glow otherwise)
  void setMusicDriven(bool enabled) { m_musicDriven = enabled; }
  // The look to wear (a preset name, "" for the main look), asked every frame; see Looks.hpp.
  // Without it the rig wears the main look, like the customizer preview that edits it
  void setLook(std::function<std::string()> fn) { m_lookFn = std::move(fn); }
  // In a level: how hard the moment is, 0..1 (speed, busy clicking), for the focus mode.
  // Without it the rig is never focused (menus, previews)
  void setFocusSignal(std::function<float()> fn) { m_focusFn = std::move(fn); }
  // Profile and menu icons follow the "Show in menus" setting
  void setMenu(bool menu) { m_isMenu = menu; }

  // A little speech bubble above the head, from the emote keys (or another player on Globed)
  enum class Emote
  {
    Heart,
    Note,
    Exclaim,
    Question,
  };
  void emote(Emote emote);

  // Gameplay events for the reactions
  void celebrate();
  void checkpointReached();
  // An orb or a pad launched the icon, `up` is the direction it flies in (sim space)
  void boosted(cocos2d::CCPoint const &up);
  // The player this rig belongs to, for the hooks that only know the player
  void setPlayer(cocos2d::CCNode *player) { m_player = player; }
  cocos2d::CCNode *player() const { return m_player; }

  // The part of the look drawn at a point (world space) in the last frame: the title key of its
  // block in the customizer ("bangs-title"), "" for none. Points up to `slop` away count too
  std::string partAt(cocos2d::CCPoint const &world, float slop) const;
  // The shape `part` had in the last frame as triangles (three points each), `worldToTarget` maps
  // them into the space wanted (the hover highlight of the customizer)
  void partShape(std::string_view part, cocos2d::CCAffineTransform const &worldToTarget,
                 std::vector<cocos2d::CCPoint> &triangles) const;

  void visit() override;
  void onEnter() override;
  void onExit() override;
  // Called by the effects node every frame: updates the particles if nobody did, draws them
  void visitEffects(cocos2d::CCDrawNode *node);

private:
  enum class LockKind
  {
    Back,     // the hairstyle behind the head
    FaceLock, // framing the face, in front of it
    Bang,     // over the top of the face
    Tail,     // ponytail / twin tails, a bundle of locks tied together
    Ahoge,    // springy strand sticking up from the top
    Ribbon,   // ribbon tail hanging from a bow
    Ear,      // soft ear on top of the head
    ScarfEnd, // end of the scarf fluttering behind
    Trail,    // the long ribbon tied at the back of the head
  };

  // One lock of the hairstyle, generated once per config
  struct Lock
  {
    LockKind kind = LockKind::Back;
    float side = 0.f; // front locks: -1..1 across the face; extras: angle offset or side
    int group = 0;    // extras: which tail / bow the lock belongs to
    float angle;  // degrees from "up" towards the back where the lock grows
    float length; // icon units
    float width;  // icon units, at the root
    float curl;   // degrees per segment
    float depth;  // 0 = back layer (darker), 1 = front layer
    bool streak = false; // colored with the streak color
  };

  bool init(cocos2d::CCSprite *head, cocos2d::CCSprite *primary, cocos2d::CCSprite *secondary, cocos2d::CCNode *simSpace);
  // The head, the colors and the sim space all still exist
  bool anchorsAlive() const;

  // What each part drew this frame, for partAt(): its triangles in the buffer of a draw node
  struct DrawnPart
  {
    cocos2d::CCDrawNode *node = nullptr;
    GLsizei from = 0;
    GLsizei to = 0;
    char const *part = "";
  };
  template <class Draw>
  void drawPart(cocos2d::CCDrawNode *node, char const *part, Draw &&draw)
  {
    GLsizei const from = node->m_nBufferCount;
    draw();
    if (node->m_nBufferCount > from)
      m_drawn.push_back({node, from, node->m_nBufferCount, part});
  }
  std::string partAtExactly(cocos2d::CCPoint const &world) const;

  void reloadConfig();
  void generateLocks();
  void addFrontLocks(std::mt19937 &rng);
  // Clumps bangs: a few wide pointed clumps and thin wisps beside them
  void addBangClumps(std::mt19937 &rng);
  // Picks the locks that get the streak color
  void markStreaks();

  // ! --- Extras (Extras.cpp) --- !

  bool extrasActive() const;
  void addExtraLocks(std::mt19937 &rng);
  void addRibbonLocks();
  void addEarAndScarfLocks();
  void buildExtraTarget(Lock const &lock, HairStrandTarget &target, cocos2d::CCPoint const &headCenter,
                        cocos2d::CCPoint const &up, cocos2d::CCPoint const &across);
  // Distance from the head center to the visible edge of the icon along a unit direction
  float headEdge(cocos2d::CCPoint const &dir) const;
  // Where a tail is tied, on the edge of the head
  cocos2d::CCPoint tailRoot(int group, cocos2d::CCPoint const &headCenter, cocos2d::CCPoint const &up,
                            cocos2d::CCPoint const &across, cocos2d::CCPoint *outward = nullptr) const;
  // Bows: 0 is the one on the head, 1 and 2 tie the tails
  bool bowExists(int group) const;
  cocos2d::CCPoint bowAnchor(int group, cocos2d::CCPoint const &headCenter, cocos2d::CCPoint const &up,
                             cocos2d::CCPoint const &across, cocos2d::CCPoint *outward = nullptr) const;
  void updateBow(float dt);
  // How wide a lock is along its length, relative to its root width
  float lockWidthAt(Lock const &lock, float along) const;
  void drawTies(cocos2d::CCDrawNode *node);
  // Locks [from, to) drawn as braids instead of plain locks
  void drawBraids(cocos2d::CCDrawNode *node, size_t from, size_t to);
  void drawBows(cocos2d::CCDrawNode *node);
  // Scarf: where the knot is (back of the neck) and the band across the bottom of the head
  cocos2d::CCPoint scarfKnot(cocos2d::CCPoint const &headCenter) const;
  void drawScarfBand(cocos2d::CCDrawNode *node);
  void drawRibbon(cocos2d::CCDrawNode *node);
  void drawEarInners(cocos2d::CCDrawNode *node);
  void updateEars(float dt);

  // ! --- Decorations and effects (Decor.cpp) --- !

  bool decorActive() const;
  void updateDecor(float dt, cocos2d::CCPoint const &headCenter, cocos2d::CCPoint const &up, cocos2d::CCPoint const &across);
  void onLanded(cocos2d::CCPoint const &headCenter, cocos2d::CCPoint const &up, cocos2d::CCPoint const &across);
  enum class ParticleKind
  {
    Heart,
    Sparkle,
    Petal,
    Snow,
    Leaf,
    Star,
    Zzz,
    Drop,
    Trail, // left behind while moving: a heart, a star or a sparkle (the trail setting)
  };
  void updateParticles(float dt);
  void onDeath();
  void burst(ParticleKind kind, int count, cocos2d::CCPoint const &headCenter);
  void spawnParticle(ParticleKind kind, cocos2d::CCPoint const &headCenter, bool burst);
  // Where clip `index` sits and which way it points, in sim space
  bool clipPlacement(int index, cocos2d::CCPoint &position, cocos2d::CCPoint &direction) const;
  void drawClips(cocos2d::CCDrawNode *node);
  void drawHeadband(cocos2d::CCDrawNode *node);
  void drawFlowers(cocos2d::CCDrawNode *node);
  void updateHalo(float dt, cocos2d::CCPoint const &headCenter);
  cocos2d::CCPoint haloTarget(cocos2d::CCPoint const &headCenter) const;
  void drawHalo(cocos2d::CCDrawNode *node);
  void drawSticker(cocos2d::CCDrawNode *node);
  void drawHat(cocos2d::CCDrawNode *node);

  // ! --- Charms (Charms.cpp): headphones, glasses, earrings, the bell --- !

  bool charmsActive() const;
  void updateCharms(float dt);
  void drawHeadphones(cocos2d::CCDrawNode *node);
  void drawGlasses(cocos2d::CCDrawNode *node);
  void drawEarrings(cocos2d::CCDrawNode *node);
  void drawCollarAndBell(cocos2d::CCDrawNode *node);

  // ! --- Cape (Cloth.cpp) --- !

  // The pinned edge of the cloth and the way it hangs at rest, sim space
  void capePins(cocos2d::CCPoint const &headCenter, std::vector<cocos2d::CCPoint> &pins, cocos2d::CCPoint &hang) const;
  void updateCape(float dt, cocos2d::CCPoint const &headCenter);
  void drawCape(cocos2d::CCDrawNode *node);

  // ! --- Wings (Wings.cpp) and the pet (Pet.cpp) --- !

  void updateWings(float dt, bool tookOff);
  void drawWings(cocos2d::CCDrawNode *node);
  enum class PetMood
  {
    Normal,
    Happy, // a checkpoint or a level complete
    Sad,   // the icon died, until the respawn
  };
  cocos2d::CCPoint petTarget(cocos2d::CCPoint const &headCenter) const;
  void updatePet(float dt, cocos2d::CCPoint const &headCenter);
  // After a death the hair isn't simulated, the pet just floats where it was
  void updatePetAlone(float dt);
  // The running pet: on the blocks of the level, jumping spikes, riding on the head in flight
  void updatePetRunner(float dt, cocos2d::CCPoint const &headCenter);
  void petReact(PetMood mood, float duration);
  void drawPet(cocos2d::CCDrawNode *node);
  void drawEmote(cocos2d::CCDrawNode *node);
  void drawBlush(cocos2d::CCDrawNode *node);
  void drawParticles(cocos2d::CCDrawNode *node);
  // Fill color of a lock at a point along it: dyed tips and the shine are mixed in here
  cocos2d::ccColor4F lockColorAt(Lock const &lock, cocos2d::ccColor4F const &base, float along) const;
  float headUnit() const; // head-local units per hair unit
  bool isActive() const;
  // Opacity to draw with: the icon's own times the focus fade of the part being drawn
  float drawAlpha() const;
  // The outline color of the look at this opacity, premultiplied
  cocos2d::ccColor4F ink(float alpha) const;
  void updateFocus(float dt);
  void updateFrame(float dt, bool snap);
  void updateMotion(float dt, cocos2d::CCPoint const &headCenter, bool snap);
  void updateGust(float dt);
  void gravityAxes(cocos2d::CCPoint &up, cocos2d::CCPoint &back) const;
  void iconAxes(cocos2d::CCAffineTransform const &headToSim, cocos2d::CCPoint &up, cocos2d::CCPoint &back) const;
  void buildTargets(cocos2d::CCPoint const &headCenter, cocos2d::CCPoint const &up, cocos2d::CCPoint const &back);
  void buildFrontTarget(Lock const &lock, HairStrandTarget &target, cocos2d::CCPoint const &headCenter,
                        cocos2d::CCPoint const &up, cocos2d::CCPoint const &across);
  // Point of the bangs hairline, `side` goes -1..1 across the face
  cocos2d::CCPoint bangRoot(float side, cocos2d::CCPoint const &headCenter, cocos2d::CCPoint const &up,
                            cocos2d::CCPoint const &across) const;
  float bangArcExponent() const;
  void simulate(float dt);
  void redraw();
  void drawCap(cocos2d::CCAffineTransform const &simToHair, cocos2d::ccColor4F const &color, float outline,
               cocos2d::ccColor4F const &outlineColor);
  // One piece of the base, between two angles (degrees from "up" towards the back)
  void drawCapArc(cocos2d::CCAffineTransform const &simToHair, float from, float to, cocos2d::ccColor4F const &color,
                  float outline, cocos2d::ccColor4F const &outlineColor);
  // Strands [from, to) into `node`, all outlines first so overlapping locks merge into one shape
  void drawLocks(cocos2d::CCDrawNode *node, size_t from, size_t to, bool withCap);
  void drawDebug();
  cocos2d::ccColor4F hairColor() const;
  cocos2d::ccColor4F lockColor(Lock const &lock) const;
  cocos2d::ccColor4F sourceColor(HairColorSource source, cocos2d::ccColor3B const &custom) const;

  cocos2d::CCSprite *m_head = nullptr;
  cocos2d::CCSprite *m_primary = nullptr;
  cocos2d::CCSprite *m_secondary = nullptr;
  cocos2d::CCNode *m_simSpace = nullptr;
  // The nodes above may go away under the rig (a profile page builds its icons again, a preview
  // moves to another parent): checked every frame before they are touched
  geode::WeakRef<cocos2d::CCSprite> m_headAlive;
  geode::WeakRef<cocos2d::CCSprite> m_primaryAlive;
  geode::WeakRef<cocos2d::CCSprite> m_secondaryAlive;
  geode::WeakRef<cocos2d::CCNode> m_simSpaceAlive;

  std::function<bool()> m_shouldShow;
  std::function<GameMode()> m_gameModeFn;
  std::function<cocos2d::CCPoint()> m_gravityDir;
  std::function<float()> m_facingFn;
  std::function<bool()> m_onGround;
  std::function<bool()> m_boxHead;

  HairConfig m_config;
  unsigned m_configVersion = 0;
  std::vector<Lock> m_locks; // back hairstyle sorted back to front, then the face locks and bangs
  // Lock ranges, each drawn with its own outline: hairstyle, tails, ahoge (behind the icon),
  // face locks, bangs, bow ribbons (in front of it)
  size_t m_tailsStart = 0;
  size_t m_ahogeStart = 0;
  size_t m_earsStart = 0;
  size_t m_scarfStart = 0;
  size_t m_trailStart = 0; // the ribbon, drawn as a flat band
  size_t m_frontStart = 0;
  size_t m_bangsStart = 0;
  size_t m_ribbonsStart = 0;
  geode::Ref<cocos2d::CCDrawNode> m_front; // draws the front locks above the icon
  HairSim m_sim;
  std::vector<HairStrandTarget> m_targets;
  std::vector<cocos2d::CCPoint> m_curve; // scratch buffer for drawing

  // Last simulated frame, the base under the locks is drawn from it
  HairSimParams m_frameParams;
  cocos2d::CCPoint m_frameUp = {0.f, 1.f};
  cocos2d::CCPoint m_frameBack = {-1.f, 0.f};
  float m_capFrom = 0.f; // degrees, the arc the locks grow on
  float m_capTo = 0.f;

  float m_simScale = 1.f; // sim units per hair unit, a hair unit is 1/30 of the head size
  float m_upAngle = 0.f;  // radians, smoothed "away from gravity" direction
  float m_facing = 1.f;   // smoothed, -1..1
  float m_motion = 0.f;   // smoothed, 0 standing still .. 1 moving fast enough to comb the hair back
  float m_airFlow = 0.f;  // the flow before the clamp, see airFlow()
  cocos2d::CCPoint m_lastUp = {0.f, 1.f}; // hairstyle frame of the last frame, gives the spin speed
  float m_calm = 0.f;                      // current calm jumps strength, see HairSimParams::calm
  float m_gust = 1.f;                      // current wind strength factor, changes over time
  cocos2d::CCPoint m_lastHeadCenter;
  cocos2d::CCPoint m_headVelocity;     // sim units / s
  cocos2d::CCPoint m_lastHeadVelocity; // of the previous frame, gives the acceleration
  cocos2d::CCPoint m_headAxisX = {1.f, 0.f}; // icon axes in sim space, for the edge of a cube
  cocos2d::CCPoint m_headAxisY = {0.f, 1.f};
  bool m_isBox = false;
  float m_bowWobble = 0.f;      // degrees the bows swing
  float m_bowWobbleSpeed = 0.f; // degrees / s

  // The icon's own frame (not the hairstyle frame): the face is the texture, so the blush and the
  // scarf follow the sprite even when the hair stays upright
  cocos2d::CCPoint m_iconUp = {0.f, 1.f};
  cocos2d::CCPoint m_iconBack = {-1.f, 0.f};

  // Ears: a quick twitch now and then, one state per ear
  std::array<float, 2> m_earTwitch = {0.f, 0.f};      // degrees
  std::array<float, 2> m_earTwitchSpeed = {0.f, 0.f}; // degrees / s
  std::array<float, 2> m_earTimer = {1.f, 2.5f};      // s until the next twitch

  // Effects
  struct Particle
  {
    cocos2d::CCPoint position; // sim space
    cocos2d::CCPoint velocity; // sim units / s
    float age = 0.f;
    float life = 1.f;
    float size = 1.f;
    float phase = 0.f;
    float rotation = 0.f; // radians
    float spin = 0.f;     // radians / s
    ParticleKind kind = ParticleKind::Heart;
    bool tinted = false; // uses `color` instead of the setting's color
    cocos2d::ccColor4F color = {1.f, 1.f, 1.f, 1.f};
  };
  std::vector<Particle> m_particles;
  float m_sparkleTimer = 0.f;
  float m_blushPop = 0.f; // 1 right after a landing, fades to 0
  float m_petalTimer = 0.f;
  float m_trailTimer = 0.f;
  float m_idleTime = 0.f; // s standing still
  float m_sleepy = 0.f;   // 0 awake .. 1 asleep
  float m_zzzTimer = 0.f;
  float m_landPop = 0.f; // 1 right after a landing, fades fast: hats squash, the pet hops

  std::array<hair::Pendulum, 2> m_earringSwing; // left, right
  hair::Pendulum m_bellSwing;
  float m_beat = 0.f; // 0..1, the music pulse for the headphones
  bool m_musicDriven = false;
  cocos2d::CCNode *m_player = nullptr;
  std::function<std::string()> m_lookFn;
  std::function<float()> m_focusFn;
  float m_focus = 0.f;     // 0 relaxed .. 1 focused, smoothed
  float m_focusCalm = 0.f; // s since the moment stopped being hard
  float m_fadeAlpha = 1.f; // set around the parts that fade with the focus
  std::string m_lookName; // "" is the main look

  float m_wingAngle = 0.f; // degrees the wings are raised by a flap
  float m_wingSpeed = 0.f; // degrees / s

  cocos2d::CCPoint m_petPosition; // sim space
  cocos2d::CCPoint m_petVelocity;
  float m_petBlink = 3.f; // s until the next blink, negative while blinking
  float m_petClock = 0.f; // s, keeps running while the hair is hidden
  PetMood m_petMood = PetMood::Normal;
  float m_petMoodTime = 0.f; // s left of a happy mood
  unsigned m_petFrame = 0;   // last frame the pet was updated
  float m_petTilt = 0.f;     // s left of a puzzled head tilt (the "?" emote)
  bool m_petGrounded = false; // the running pet stands on a block
  bool m_petRiding = false;   // the running pet sits on the head
  float m_petRunPhase = 0.f;  // radians, the feet

  ClothSim m_cloth;
  float m_clothSpacing = 0.f; // sim units between cloth points

  std::optional<Emote> m_emote;
  float m_emoteAge = 0.f; // s
  bool m_diedActive = false; // the rig was showing when the icon died, so the pet and the reactions play

  std::function<bool()> m_isDeadFn;
  bool m_wasDead = false;
  unsigned m_aliveFrame = 0;     // last frame the hair was simulated
  unsigned m_particlesFrame = 0; // last frame the particles were updated
  geode::Ref<HairEffectsNode> m_effects;
  cocos2d::CCPoint m_haloPosition; // sim space, lags behind the head on a spring
  cocos2d::CCPoint m_haloVelocity;
  bool m_wasOnGround = false;
  std::minstd_rand m_random{20240611};
  unsigned m_lastFrame = 0;
  float m_time = 0.f;
  bool m_needsReset = true;
  bool m_idleWind = false;
  bool m_isGarage = false;
  bool m_isMenu = false;
  bool m_debugDraw = false;
  std::vector<DrawnPart> m_drawn;
};
