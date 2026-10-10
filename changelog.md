# Changelog

## v1.10.1

### Customizer
- Colors are picked right under their row: a palette, the hex code and hue, saturation and brightness sliders. The icon changes while you move them, nothing covers the preview anymore
- Presets: tap a preset twice quickly to load it
- Editing the look of an icon (Icon) no longer asks to save when you switch to Main or close the customizer: the changes wait unsaved, and your icon already wears them in levels, the garage and on Globed, so you can try them first. Save keeps them; loading the preset again drops them
- Main and Icon no longer touch the game mode switcher; − and + of Rows have room around the text

### Globed
- Looks go to the other players through the Icon Mayhem server: Globed carries only a short code, so a look of any size arrives at once and in full instead of in many small parts. Looks are fetched once and kept. When the server can't be reached, the look goes through Globed like before. Gifts go the same way
- Sharing your look needs you logged in to your Geometry Dash account (checked once with Argon, like for custom icons)

### Physics
- Calm jumps is off and Hitbox multiplier is 0.6 by default (the built-in presets too)

### Hair
- The base under the hair is smaller and stays hidden behind the icon

### Fixed
- The macOS build of v1.10.0 failed, so that release had no file to download

## v1.10.0

### Settings
- Geode's settings page of the mod is short now: the master switch, where the look shows, quality, focus mode, online and the emote keys. Your look (hair, accessories, effects, physics, game modes) is set in the customizer, "Your look" on the page opens it
- Your look lives in `look.json` in the config folder of the mod: only what differs from the defaults, easy to read. Edit it in a text editor and save, the icon changes in a moment (a mistake keeps the look and tells you what's wrong). The folder button in General opens it. Your current look moves there by itself

### Pause menu
- Icon Mayhem adds just one button to the pause menu. Players and gifts on Globed moved into the customizer (next to Save), and the button wears a little badge when players are around or a gift waits

### Customizer
- Tap a part of your icon in the preview (the bangs, a bow, the glasses, the pet...) and its settings open
- With a mouse, the part under the cursor glows and shows its name, so you see what a click opens
- Parts (left of the search): every part on one page, green when it's on, tap one to go there
- The customizer takes the whole screen: the list of settings is taller and wider
- Row size: − and + at the top left make the rows of settings smaller (down to 60%) to see more of them at once, or bigger

### Removed
- The face (anime eyes and the mouth) is gone. Looks and presets that had it simply show no face

## v1.9.0

### Custom icons on Globed
- Players see each other's custom icons, from More Icons or from a texture pack, even without More Icons themselves. Every mode: the cube, vehicles (with the UFO dome), robots and spiders.
- Your icons go up to the Icon Mayhem server once, and only the players you meet in a level get them. Switches in General (Online): share yours, see theirs.
- Report an icon in Players: it turns plain for you right away, and enough reports hide it for everyone.

### Builds
- Every release now has the mod for all five platforms: Windows and Android are built on the Icon Mayhem builder, macOS and iOS on GitHub.

## v1.8.0

### Looks of everyone
- Gallery: looks shared by everyone (Top, New, Mine, search), each on your own icon, alive. Wear one in a tap (it lands in your presets), keep it, like it, report it; share your own look with a name. Your Geometry Dash account is checked with Argon, the password never leaves the game. From the presets window (the globe) and General
- Players in the pause menu (on Globed): everybody in the level in their look. Save it to your presets, try it on until the level ends (the others see it on you too), like it (hearts burst on their icon and they are told), or gift them your look (it waits for them in their Players window)

### Customizer
- Every icon button has a little caption under it (Undo, Random, Colors, Same color, Jump, Run, Wind, Save, Hitboxes, Reset, New empty, Paste, Import, Folder, Gallery, Save here, Rename, Copy, To file, Delete...)

## v1.7.0

### Bangs
- Clumps: a few wide pointed clumps like in anime (three for Yor's), the side ones curling to the middle, with thin wisps sticking out beside them
- Locks between clumps: the hair under the clumps shows in the gaps, shorter and darker
- Bangs width: how wide each lock or clump is
- Bangs fan out: how much the bangs turn out to the sides with a wide spread and an arc, 0 makes them all fall straight down

### Customizer
- Wind view (the wind button left of the icon in the preview): air as strong as the air the hair feels, with every wind setting in it. Streaks at different depths (the near ones pass in front of the icon), flowing around the head with a calm wake behind it, motes of dust, curls in strong gusts. A meter under the button shows how strong the flow is and where it combs the hair fully
- The customizer opens on the look of your icon when a preset is linked to it (Icon in Main | Icon); Main switches to the main look

### Fixed
- Globed: when Globed drops a player for a moment and makes a new icon for them, it gets their look again (it stayed plain until you left the level). Players who come back get your look again

## v1.6.0

### On your icon
- Anime eyes: Onyx (big and round), Sapphire (long and sharp, also with heart pupils) and Crimson (wide and heavy-lidded), each in nine shapes (flirty, sultry, angry, kind, cheerful, judging, sad, surprised), with their own width, height, shift and tilt, on High quality
- Capes made of real cloth: a cape, a short cape or a flag on a little pole. It streams behind you, hangs in folds when you stand and shows its lining when it turns over. Stripes, stars or hearts
- The pet can run on the blocks of the level: it jumps over spikes and gaps, catches up and rides on your head in the ship, UFO, wave, swing and jetpack
- Bangs styles: straight, parted in the middle and combed to both sides (like Frieren), swept to one side, or on one side of the face only with a sharp or a soft edge
- Pendulum earrings: a long faceted gold drop under a little cap, like Yor's. Earrings move in and out with Earring inset X
- Face locks shift together, and tilt sets one lock higher than the other
- Outline color: the outline of the hair, accessories, the pet, the cape and the face in any color, not only black
- Focus mode: nothing covers your view in hard parts. At faster speeds and when you click a lot (Auto) or always, the trail, weather and particles hide and the pet, wings and halo fade, then come back when it calms down. The hair can fade too. Other players' looks on Globed follow your focus

### Looks and presets
- Linker: link a preset to any icon, not only the one you wear. Every icon of a mode in a grid like the garage, linked ones show their preset under them, the selected one wears it on the right. A linked icon wins over the look of its mode; ships, UFOs and jetpacks carry your cube, so they wear the preset of the cube
- More Icons in the Linker, in their own list: wear a More Icons icon and its linked preset comes along in levels, the garage, menus and on Globed
- Link many at once: pick a preset for the brush and every icon you tap gets it (tap again to unlink), or search More Icons by name or pack and link all found
- Main | Icon in the customizer preview: edit the look of the icon you wear right there. Save keeps it in its preset, the main look comes back when you switch back or close the customizer (even after a restart in the middle)
- The garage and the menus show the look of the icon you wear (they showed the main look with More Icons)
- Looks per mode: tap a mode to pick its preset from a list with search, no more paging with arrows
- Presets made over: the customizer and the presets window know which preset your look came from and whether it changed, and Save puts the changes back into it in one tap. Save as makes a new one, a built-in look saves as your own copy
- Your presets come first, the latest on top, built-in ones in their own tab, with search. The selected one is shown alive on your icon before you load it. Rename presets and save over one
- Empty look: start from nothing and build your own. Presets remember whether the hair is on

### Customizer
- Every part folds into one line with its switch in the header: turn a cape or glasses on and off without opening anything. A part that is off hides its settings, turning it on opens them
- Positions, insets and tilts wait under "Fine tuning" in each part
- "On now" next to the search lists everything you are wearing, from every tab
- Icons on the obvious buttons (undo, surprise, colors, match, jump, run, save, hitboxes, reset, paste, import, copy, export, rename, delete), so the windows are tidier and the preview has more room. Saving and other big changes ask first, and the icons that change your look explain themselves the first time
- Customization switch at the top of General: turns everything off at once, everywhere, and back on with your look as it was
- The customizer opens from the garage too, no level needed
- The preview shows your custom icon from More Icons
- Buttons keep their size with any texture pack

### Removed
- The built-in presets Kepochka and Twin Tails
- The star button in the pause menu (the next favorite look) and the favorite stars: the Linker and looks per mode do it better

## v1.5.2

- Globed: big looks reach the other players too. The Globed server drops events over 1024 bytes, so a detailed look was lost on the way; looks now travel in small parts. Tested with Globed 2.2.3 builds

## v1.5.1

- Globed: sharing looks and emotes waits for Globed 2.2.3. Globed 2.2.2 was released without the API other mods use, so nothing was sent. Icon Mayhem keeps asking and starts sharing as soon as Globed answers
- Globed: the look goes to everybody in the level when it changes or somebody joins, the log tells what was sent and received
- Player 2 in dual: the hair is attached on a timer of the level, it could miss the moment player 2 appears

## v1.5.0

- Trail: a long ribbon tied at the back of the head, or little hearts, stars or sparkles left behind
- A look for each icon: change your icon in the garage and the look changes with it. A button next to the garage icon picks it
- Your look on your own profile and on the profile button of the main menu ("Show in menus")
- Emotes: keys (Alt+One to Alt+Four by default) show a heart, a note, "!" or "?" above your icon, the pet answers
- Globed (experimental): players in the same level see each other's looks and emotes (both need the mod)
- Quality setting: Balanced and Low draw less for slower phones and computers
- Fixed: at high FPS the hair and everything hanging on it flickered and left a ghost behind while moving

## v1.4.0

- Game modes: turn the whole customization off in some game modes, for example keep it on the cube only
- General tab in the customizer
- Headphones (plain or with cat ears) whose lights glow to the level music
- Glasses: round, hearts or stars, with tinted lenses
- Earrings (drops, hearts, stars, pearls) and a cat bell on a collar, swinging when you move
- Colored streaks in the bangs, the face locks or the hair
- Pets: a bunny and a slime, and moods: the pet falls asleep with you, cheers on checkpoints and level completes, cries when you die
- Orbs and pads throw the hair up with a puff of sparkles
- Settings search in the customizer
- Star presets as favorites and switch between them with the new button in the pause menu
- Looks per mode: wear another preset in each game mode, separately for player 1 and player 2 in dual
- Player 2 in dual gets the hair and accessories too, it had none before
- Built-in presets wear the new accessories: Angel got its wings, Kitty a bell and a kitten, Sakura a bunny, glasses for Short and Ahoge and more

## v1.3.0

- Hair under a cap: "Cap gap" leaves the top of the head bare so the hair comes out from under a cap
- Extras tab: ponytail or twin tails with a scrunchie or a bow, ahoge, a bow on the head with fluttering ribbons
- Ears (cat, bunny, fox) that twitch and fold back in the wind, a scarf with fluttering ends
- Hair shine, dyed tips and hair clips on the bangs
- Effects tab: blush on the cheeks, hearts and sparkles
- Braids for the tails and face locks, a headband with a bow or ears, flowers and a flower crown, a halo
- Sakura petals, a sleepy icon with floating "Z"s, a Surprise button for a random cute look
- Wings that flap on jumps, a pet floating behind you, cute hats, face stickers
- Weather: snowflakes, autumn leaves and stars besides the sakura petals
- Reactions to death, level complete and checkpoints, a cute death burst
- Customizer: Undo, random Colors, Match hair colors
- Built-in presets: Kepochka, Kitty, Sakura, Angel, Fairy, Twin Tails, Ponytail, Short and Ahoge
- Physics tab with gusty wind (gusts, flutter, breeze), friction and calm jumps

## v1.2.0

- Presets: save your look under a name and load it in one tap
- Share presets through the clipboard or as a .json file, import them the same way
- Presets button in the customizer, next to the hitbox helpers

## v1.1.0

- Face locks: wavy locks framing the face, left and right can be shown separately, with their own width, position and color
- Bangs over the top of the face: spread, an arc for icons with a round top, position and their own color. They lie over the face locks
- Front tab in the customizer for the face locks and bangs
- Hitbox helpers in the customizer preview: the collider, the floor, the roots and the bangs hairline
- Wind multiplier: how much the air affects the hair while moving, 0 makes it look like you're standing still
- Hair length up to 200, gravity up to 10, damping up to 1

## v1.0.0

- Physics-based hair in every game mode
- Hairstyles: Flowing, Long and Spiky
- Hair glued to the top of the icon spins with it in jumps, or stays on top of the head with "Spin with icon" off
- Hair drapes over the head standing still, gets combed back and pressed to the head by the oncoming air while moving
- Hitbox multiplier to choose how far from the icon the hair rests
- Hair preview in the garage
- Customizer popup with a live icon preview, opened from the pause menu
