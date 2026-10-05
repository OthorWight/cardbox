# Cardbox

Cardbox is a lightweight, scriptable Solitaire and card game engine built in C++ using Dear ImGui. It allows you to play various card games and easily create your own custom rules using Lua scripts.

## Features

* **Scriptable Games**: Write your own card games using simple Lua scripts.
* **Smooth UI**: Fast, responsive, and animated card movements.
* **Game Themes**: Every bundled Lua game controls its window background, menus,
  toolbar, buttons, and card slots. Palettes range from Klondike's slate blue and
  FreeCell's teal to Pyramid's sandstone and Hold'em's burgundy and gold.
* **Quality of Life**: Built-in undo/redo stack, smart drag-and-drop with magnetic snapping, and auto-solve mechanics.
* **Game Controls**: Each game has a progress display and its own actions. Solitaire
  games include legal-move hints, move counts, and explicit safe foundation moves.
* **Particle Effects**: Soft move sparkles, motion-driven drag trails, and timed
  victory confetti with tumbling card suits. A separate particle system handles
  fixed-step physics, DPI scaling, and a reusable buffer capped at 1,024 particles.

## Crawler dungeon

Choose **Crawler** from the game list. Fight spades, equip clubs, drink hearts,
and collect diamonds. You start with a six-of-clubs weapon; weapons last three
strikes. Damage previews show what each fight will cost, including lethal attacks.
Fight bare-handed to preserve a weapon, carry a potion into the next room, spend
8 gold to heal at camp, or pay 2 HP to retreat. Retreat returns encounters to the
bottom of the deck and cannot be used twice in a row.

Drink at most one potion and camp once per room. Resolve unwanted potions and
weapons with their alternative buttons. Advance when at most one non-monster
remains, carrying that encounter forward. Clear the deck and final room to win.
Score counts gold carried, 5 points per monster defeated, and 2 per remaining HP
on victory. **Ctrl+Z / Ctrl+Y** undo and redo the entire dungeon state; **F2**
starts a new run. The **Advice** button suggests an encounter and explains its
cost; the board also provides Undo, Redo, and New run controls.

## Bundled games

| Script | Rules and controls |
| --- | --- |
| `klondike.lua` | Draw one, unlimited redeals; alternating-color runs, Kings in empty columns, optional safe foundation moves. |
| `freecell.lua` | Group capacity uses spare cells and columns, excluding an empty destination; foundations can be moved back. |
| `yukon.lua` | Any face-up group can move when its bottom card fits; exposed cards flip after moves. |
| `spider.lua` | One suit, 104 cards; ten-card stock rows require every column to be occupied, and complete same-suit runs are collected together. |
| `golf.lua` | Click or drag exposed cards; King/Ace wrap, no stock recycling, and a no-moves status when stuck. |
| `pyramid.lua` | Click pairs totaling 13 or drag onto a partner; click exposed Kings to remove them; two stock redeals. |
| `dungeon.lua` | Combat previews, alternative encounter actions, camp, retreat, room carry, and advice. |
| `hanoi.lua` | Choose 3–7 disks; drag or select source/destination pegs; track moves against the optimal solution. |
| `texas_holdem.lua` | Eight-seat tournament; legal minimum raises and all-ins, contribution-based side pots, clockwise odd-chip payouts, heads-up blinds, and pause/step bots. |
| `example.lua` | A solvable tutorial: archive all 52 cards, with stock draws, work columns, and batch archive controls. |
| `malicious.lua` | Fourteen individually protected sandbox probes, with a persistent, undoable pass/fail report. |

Every script saves its logical counters and state for undo/redo. Hold'em also
restores its random state so repeating a bot action produces the same result;
undo pauses the bots until Resume is selected. Hints and selections do not create
history entries. Next hand is explicit, so results remain available for review.
The Spider stock restriction follows [Bicycle's rules](https://bicyclecards.com/how-to-play/spider-solitaire);
Hold'em betting, heads-up play, and side pots follow
[PokerStars' rules](https://www.pokerstars.com/help/articles/poker-rules-master/229178/).

## Lua game hooks

Rules can add `SaveState()` and `LoadState(piles, data)` to preserve custom run
state in undo history. `SaveState` returns an immutable string; `LoadState`
restores it after the engine restores the piles and score. Both hooks are optional.
Use `PerformAction(name)` in drawing code to queue `HandleAction(piles, name)`
after drawing finishes. These actions share the history and rollback behavior of
card clicks. `cards:take_back()` removes and returns a card by value, so scripts
can safely move it between piles.

The engine loads a sibling `lib/solitaire.lua` when present. Bundled solitaire
scripts use it for shared layout, hints, and counters while keeping rule decisions
in each script. The build copies both games and their support library into
`build/rules/`, including after changes that only touch Lua files.
`HandleClick(piles, pileIdx, cardIdx)` receives the clicked card index (`-1` for an
empty slot), allowing scripts to reject clicks on covered cards. Older two-argument
callbacks still work. `CanUndo()` and `CanRedo()` report available history;
`PerformAction("engine:undo")`, `"engine:redo"`, and `"engine:restart"` queue board
controls that also work after victory.

`DrawBackground()` draws before the cards. `DrawBoardPanel`, `DrawBoardTooltip`,
and `EmitBoardParticles` use the same logical coordinates as the piles.
`DrawBoardText(x, y, text, fontSize, wrapWidth, r, g, b)` accepts optional text
size, wrapping, and RGB color arguments; existing three-argument calls still work.
`DrawBoardButton(x, y, w, h, label, enabled)` accepts an optional enabled flag.
Buttons at different logical positions can share the same label.

Games can define an optional `Theme` table to style the entire application client
area, including its menus, toolbar, buttons, tooltips, and empty card slots:

```lua
Theme = {
    Background = {13, 18, 27}, BackgroundBottom = {10, 14, 22},
    Toolbar = {19, 25, 36}, ButtonRounding = 6,
    EmptyPile = {19, 25, 36, 180}, EmptyPileBorder = {66, 80, 98},
    EmptyPileText = {155, 169, 184}, CardBack = {38, 54, 75},
    CardBorder = {97, 112, 131}, CardHover = {244, 201, 116},
    Colors = {
        Text = {235, 229, 213}, MenuBarBg = {19, 25, 36},
        PopupBg = {25, 32, 44}, Button = {43, 57, 76},
        ButtonHovered = {62, 80, 101}, ButtonActive = {79, 99, 121}
    }
}
```

Colors use `{r, g, b}` or `{r, g, b, a}` byte channels (0–255).
`Colors` accepts Dear ImGui color names without the `ImGuiCol_` prefix.
Setting `Background` also sets the bottom and toolbar colors unless overridden.
`CardBack` colors the procedural fallback when no card-back image is loaded;
card artwork keeps its original colors. `ButtonRounding` uses logical pixels.
Themes load at game initialization; Reload applies Lua changes. Missing or
malformed entries keep the defaults. Switching games or returning to the launcher
restores the appropriate palette without retaining the previous game's overrides.

## Dependencies

This project relies on the following excellent open-source libraries (included in the `extern/` directory):

* Dear ImGui - Bloat-free Graphical User interface for C++ with minimal dependencies.
* GLFW - A multi-platform library for OpenGL, OpenGL ES, Vulkan, window and input.
* stb - Single-file public domain (or MIT licensed) libraries for C/C++.
* glad / OpenGL Loader
* sol2 / Lua - C++ bindings for Lua scripting.

## Building

This project uses standard build tools. Assuming you are using CMake, you can build the project by running the following commands from the root directory:

```bash
# Create a build directory
mkdir build
cd build

# Configure the project
cmake ..

# Build the project
make
```

Once built, you can run the executable generated in the `build/` directory.

Run the headless engine regression tests with:

```bash
ctest --test-dir build --output-on-failure
```

Tests are enabled by default; use `-DBUILD_TESTING=OFF` when configuring to
build only the application. Lua rules may request 1–8 decks with `NumDecks`.
Tests cover all bundled games' rule decisions, card conservation, stock limits,
poker payouts and chip conservation, history, sandbox probes, particles, and
headless ImGui rendering with unique board button IDs.
Invalid vector accesses and card ranks/suits raise Lua errors. Rule files
must contain Lua source text.

## License

MIT
