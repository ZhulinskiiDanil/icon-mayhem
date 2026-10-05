# Icon Mayhem

<img src="logo.png" width="150" alt="Icon Mayhem logo" />

A [Geode](https://geode-sdk.org) mod for Geometry Dash with crazy customizations for your icons.

## Features

- **Physics-based hair** in every game mode, with three hairstyles, a garage preview and a settings button in the pause menu

More features are coming. See [about.md](about.md) for the full description and settings.

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

- `src/hair/`: hair simulation (`HairSim`), drawing (`HairNode`) and settings (`HairConfig`)
- `src/hooks/`: game hooks (level, garage, pause menu)
- `resources/`: sprites
