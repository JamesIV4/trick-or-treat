#include "gen/candy_kinds.h"

#include <godot_cpp/core/math.hpp>

#include <vector>

#include "gen/asset_library.h"
#include "gen/types.h"

using namespace godot;

namespace tot {

namespace {
struct Kind {
	std::string id;
	int tier;
	Color glow;
};

std::vector<Kind> &kinds() {
	static std::vector<Kind> k = {
		{ "candy_wrapped_pink", 0, Color(1.0f, 0.45f, 0.75f) },
		{ "candy_wrapped_mint", 0, Color(0.4f, 1.0f, 0.8f) },
		{ "candy_wrapped_grape", 0, Color(0.7f, 0.5f, 1.0f) },
		{ "candy_wrapped_lemon", 0, Color(1.0f, 0.9f, 0.35f) },
		{ "candy_wrapped_orange", 0, Color(1.0f, 0.6f, 0.25f) },
		{ "candy_corn", 0, Color(1.0f, 0.78f, 0.3f) },
		{ "candy_chocolate_bar", 1, Color(1.0f, 0.55f, 0.35f) },
		{ "candy_lollipop", 2, Color(1.0f, 0.4f, 0.45f) },
		{ "candy_golden_bar", 3, Color(1.0f, 0.82f, 0.3f) },
	};
	return k;
}

// The variety candies in play, added alongside the nine classics (a missing or unpublished one is skipped).
// (The other published variety models stay in the catalog, unused.)
const char *ICONIC[] = { "candy_gummy_bear_red", "candy_gummy_worm", "candy_peppermint", "candy_swedish_fish", "candy_jellybean_trio",
	"candy_caramel_apple", "candy_pb_cups", "candy_cane_giant", "candy_lolly_jack", "candy_golden_twist" };

std::vector<int> by_tier[4];
bool registered = false;
} // namespace

int candy_tier(int kind) {
	const std::vector<Kind> &k = kinds();
	return kind >= 0 && kind < int(k.size()) ? k[kind].tier : 0;
}

void register_candy_kinds(AssetLibrary &lib) {
	if (registered) {
		return;
	}
	registered = true;
	std::vector<Kind> &k = kinds();
	for (const char *iconic : ICONIC) {
		const std::string id = iconic;
		const AssetInfo *a = lib.get(id);
		if (a->candy_tier < 0 || !a->published) {
			continue;
		}
		bool known = false;
		for (const Kind &e : k) {
			known = known || e.id == id;
		}
		if (!known) {
			k.push_back({ id, Math::clamp(a->candy_tier, 0, 3), a->glow });
		}
	}
	for (int i = 0; i < int(k.size()); ++i) {
		by_tier[k[i].tier].push_back(i);
	}
}

int candy_kind_count() {
	return int(kinds().size());
}

const std::string &candy_kind_id(int kind) {
	const std::vector<Kind> &k = kinds();
	return k[Math::clamp(kind, 0, int(k.size()) - 1)].id;
}

Color candy_glow(int kind) {
	const std::vector<Kind> &k = kinds();
	return k[Math::clamp(kind, 0, int(k.size()) - 1)].glow;
}

int candy_kind_of_tier(int tier, uint64_t r) {
	tier = Math::clamp(tier, 0, 3);
	const std::vector<int> &list = by_tier[tier];
	if (list.empty()) {
		static const int base[4] = { CANDY_PINK, CANDY_CHOCOLATE, CANDY_LOLLIPOP, CANDY_GOLDEN };
		return base[tier];
	}
	return list[size_t(r % list.size())];
}

} // namespace tot
