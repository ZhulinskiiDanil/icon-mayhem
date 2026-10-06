# Icon Mayhem

<img src="logo.png" width="150" alt="Icon Mayhem logo" />

A [Geode](https://geode-sdk.org) mod for Geometry Dash with cute and crazy customizations for your icons.

## Features

- **Physics-based hair** in every game mode: three hairstyles, face locks and bangs, ponytail and twin tails, braids, an ahoge, gusty wind
- **Accessories**: ears, bows, hair clips, a scarf, a headband, flowers, a halo, wings, hats and a little pet that follows you
- **Effects**: blush, face stickers, hearts and sparkles, weather (sakura, snow, leaves, stars), a sleepy icon, reactions to deaths, checkpoints and level completes
- **Customizer** with a live icon preview, opened from the pause menu: hitbox helpers, undo, random looks and colors
- **Presets**: built-in looks, your own saved looks, sharing through the clipboard or files

See [about.md](about.md) for the full description and every setting, and [changelog.md](changelog.md) for what changed.

## Building

You need the [Geode SDK](https://docs.geode-sdk.org/getting-started/) and its CLI.

```sh
geode build
```

Or with CMake directly:

```sh
cmake -B build -G Ninja
cmake --build build
```

The build packages the mod into `build/zhulis.icon-mayhem.geode` and installs it into your Geode profile.

## Project layout

- `src/hair/`: the icon rig
  - `HairSim`: verlet strand simulation, head collider, wind
  - `HairNode`: the rig attached to an icon, hair drawing
  - `Extras.cpp`, `Decor.cpp`, `Wings.cpp`, `Pet.cpp`, `Effects.cpp`: tails, accessories, wings, the pet, particles and reactions
  - `HairConfig`: settings read from the mod settings
- `src/hooks/`: game hooks (level, garage, pause menu, icon previews)
- `src/ui/`: the customizer and presets popups
- `src/presets/`: saving, loading and sharing presets
- `resources/`: sprites and the built-in presets
