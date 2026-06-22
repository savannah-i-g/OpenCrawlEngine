#!/usr/bin/env bash
# One-shot helper: copy the curated set of game-icons.net SVGs used by the
# frontend from a local game-icons.net SVG export into app/ui/assets/icons/.
# The icons are licensed CC BY 3.0 — see NOTICE.md for attribution. Re-run only
# to add or refresh icons; the copied SVGs are committed to the repository.
# (Keeping here because if you want to add icons, the game-icons package is huge, change the folders around if it isn't in your downloads)
# Usage: scripts/copy_icons.sh [path-to-game-icons-export]
#   default export path: $HOME/Downloads/game-icons.net.svg
set -euo pipefail

SRC="${1:-$HOME/Downloads/game-icons.net.svg}"
DEST="$(cd "$(dirname "$0")/.." && pwd)/app/ui/assets/icons"

if [ ! -d "$SRC" ]; then
    echo "error: game-icons export not found at: $SRC" >&2
    echo "pass the export directory as the first argument" >&2
    exit 1
fi
mkdir -p "$DEST"

# Curated icons: classes, vitals, attributes, message senders, actions, item
# kinds, menu/asset glyphs. Names are game-icons.net icon ids.
ICONS=(
    # classes
    broadsword hooded-assassin wizard-staff angel-wings high-shot lyre
    # vitals + currency
    health-normal lightning-arc laurel-crown two-coins
    # attributes
    fist running-ninja spell-book health-increase owl public-speaker spyglass hood
    pay-money dice-six-faces-five
    # message senders
    pointy-hat pointy-sword console-controller crown
    # actions
    run scroll-unfurled scroll-quill magnifying-glass knapsack bloody-sword shield footsteps
    # item kinds / equipment
    breastplate potion-ball chest-armor swordman bottled-bolt
    # menu + assets
    briefcase castle book-cover crystal-wand cog gears open-book hearts sword-clash
    # factions / business / mounts / misc
    spectre cultist hooded-figure pentacle crossed-swords swords-emblem temple-gate church
    gavel anvil mortar shop wheat coins-pile cash dragon-head sailboat horse-head campfire
    chest trophy heart-plus
    # dice faces (skill/combat roll panels)
    dice-six-faces-one dice-six-faces-two dice-six-faces-three dice-six-faces-four
    dice-six-faces-five dice-six-faces-six
)

copied=0
missing=0
for name in "${ICONS[@]}"; do
    src="$(find "$SRC" -name "$name.svg" -print -quit 2>/dev/null || true)"
    if [ -n "$src" ]; then
        cp "$src" "$DEST/$name.svg"
        copied=$((copied + 1))
    else
        echo "missing: $name" >&2
        missing=$((missing + 1))
    fi
done
echo "copied $copied icons to $DEST ($missing missing)"
