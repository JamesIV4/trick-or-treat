#pragma once
// Every kind of candy in the game: the nine classic kinds (CandyKind) first, then the ten most
// iconic variety candies from the catalog (candy_<name> with a candy_tier and a glow colour). The tier sets
// a candy's value and how big a fuss its pickup makes: 0 everyday, 1 chocolate-class, 2 lollipop-
// class, 3 golden. Placement picks the tier; the kind within the tier is chosen per piece.

#include <godot_cpp/variant/color.hpp>

#include <cstdint>
#include <string>

namespace tot {

class AssetLibrary;

// Reads the variety candies from the loaded catalog (once; later calls are no-ops).
void register_candy_kinds(AssetLibrary &lib);
int candy_kind_count();
const std::string &candy_kind_id(int kind);
godot::Color candy_glow(int kind);
// A kind of the given tier, chosen by `r` (any well-mixed random number).
int candy_kind_of_tier(int tier, uint64_t r);

} // namespace tot
