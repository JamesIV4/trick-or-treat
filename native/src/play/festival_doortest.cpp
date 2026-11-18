// --door-test: headless check of doorbells, house clearing and the area goals (WORLD_REWORK.md, "House
// goals and candy bug"). Run with --seed=N --stream-fps=60 --no-lightning. It drives the real game loop:
// a door is rung by standing the kid on its bell (the game's own doorbell code rings it), candy goes to
// the bucket through CandyField's own pickup path (collect_lot_now: same signals and counters), areas are
// left and re-entered through the streamer (cached re-attach, evicted rebuild) and the save goes through
// save_state / load_state. Prints "DOOR_TEST" lines: the treats every door hands out, each check (OK /
// FAIL), and a summary, then quits.

#include "play/festival_game.h"

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "gen/context.h"
#include "gen/neighborhood_generator.h"
#include "play/candy_field.h"
#include "play/dragon_controller.h"
#include "play/festival_camera.h"
#include "world/world_streamer.h"

using namespace godot;

namespace tot {

struct FestivalGame::DoorTest {
	int phase = 0, wait = 0, sub = 0;
	uint64_t home = 0, east = 0;
	std::map<std::pair<uint64_t, int>, int> announced; // "House cleared" per house
	int complete_calls = 0;                              // "all clear" per home area
	int s[4] = { -1, -1, -1, -1 };                       // the four rule lots
	std::vector<int> queue;                              // lots still to ring in the area being rung
	size_t qi = 0;
	int ringing = -1, before = 0;
	uint64_t ring_key = 0;
	int doors = 0, treats = 0, short_doors = 0, least = 1 << 30, most = 0;
	int failures = 0, checks = 0;
	int total_announced_before = 0;
};

void FestivalGame::start_door_test() {
	if (!door_test_) {
		door_test_ = new DoorTest();
		door_test_->home = WorldStreamer::area_key(0, 0);
		for (const String &a : OS::get_singleton()->get_cmdline_user_args()) {
			if (a.begins_with("--candy-reserve=")) {
				CandyField::set_reserve_per_lot(a.substr(16).to_int()); // small: every field has to grow
			}
		}
	}
}

void FestivalGame::door_test_announced(uint64_t key, int lot) {
	if (!door_test_) {
		return;
	}
	const int n = ++door_test_->announced[{ key, lot }];
	UtilityFunctions::print(vformat("DOOR_TEST announce lot %d (count %d)", lot, n));
}

void FestivalGame::door_test_completed(uint64_t key) {
	if (door_test_ && key == door_test_->home) {
		++door_test_->complete_calls;
		UtilityFunctions::print(vformat("DOOR_TEST all clear announced (count %d)", door_test_->complete_calls));
	}
}

void FestivalGame::door_test_step() {
	DoorTest &t = *door_test_;
	if (t.wait > 0) {
		--t.wait;
		return;
	}
	const auto check = [&](bool ok, const String &what) {
		++t.checks;
		t.failures += ok ? 0 : 1;
		UtilityFunctions::print("DOOR_TEST check ", ok ? "OK   " : "FAIL ", what);
	};
	const auto ann = [&](uint64_t key, int lot) {
		auto it = t.announced.find({ key, lot });
		return it == t.announced.end() ? 0 : it->second;
	};
	const auto total_ann = [&]() {
		int n = 0;
		for (const auto &kv : t.announced) {
			n += kv.second;
		}
		return n;
	};
	const auto placed_left = [&](Area *a, int lot) {
		int n = 0;
		const GenContext *ctx = a->gen->context();
		for (size_t i = 0; i < ctx->candy.size(); ++i) {
			if (ctx->candy[i].lot == lot && !(i < a->state->candy_taken.size() && a->state->candy_taken[i])) {
				++n;
			}
		}
		return n;
	};
	const auto park = [&]() {
		// Out of reach of every bell and candy (no pickups): high above the home spawn.
		if (Area *h = streamer_->area(t.home)) {
			player_->teleport(h->plan.spawn + Vector3(0, 40.0f, 0), 0.0f);
		}
	};
	const auto go = [&](const Vector3 &p) {
		streamer_->update(p, true);
		player_->teleport(Vector3(p.x, streamer_->ground(p.x, p.z) + 0.4f, p.z), 0.0f);
		camera_->snap();
	};
	const auto ring = [&](Area *a, int lot) {
		const PackedVector3Array bells = a->gen->get_doorbells();
		t.ringing = lot;
		t.ring_key = a->key;
		t.before = a->gen->candy_field() ? a->gen->candy_field()->total() : 0;
		player_->teleport(a->to_world(bells[lot]), 0.0f); // the doorbell loop later this frame rings it
	};
	// The frame after a ring: what the door handed out.
	const auto measure = [&]() {
		Area *a = streamer_->area(t.ring_key);
		if (t.ringing < 0 || !a) {
			return;
		}
		const int got = (a->gen->candy_field() ? a->gen->candy_field()->total() : 0) - t.before;
		const bool rung = t.ringing < int(a->state->rung.size()) && a->state->rung[t.ringing];
		UtilityFunctions::print(vformat("DOOR_TEST ring %s door %d: rung %d, treats %d", String(a->plan.name.c_str()), t.ringing, rung ? 1 : 0, got));
		t.doors++;
		t.treats += got;
		t.least = Math::min(t.least, got);
		t.most = Math::max(t.most, got);
		t.short_doors += (got < 12 || !rung) ? 1 : 0;
		t.ringing = -1;
		park();
	};
	const auto summary = [&](const char *what) {
		UtilityFunctions::print(vformat("DOOR_TEST summary %s: doors %d, treats %d (min %d, max %d), doors short of 12 %d, candy fields grown %d times so far", what, t.doors,
				t.treats, t.doors ? t.least : 0, t.most, t.short_doors, CandyField::grow_count()));
		check(t.short_doors == 0, vformat("%s: every door handed out 12 treats", what));
		t.doors = t.treats = t.short_doors = t.most = 0;
		t.least = 1 << 30;
	};
	const auto state_check = [&](const char *when) {
		Area *a = streamer_->area(t.home);
		if (!a) {
			check(false, vformat("%s: home attached", when));
			return;
		}
		const AreaState &st = *a->state;
		int bad = 0, closed = 0, twice = 0;
		for (int l = 0; l < a->gen->get_lot_count(); ++l) {
			const bool want_closed = l < int(st.lot_cleared.size()) && st.lot_cleared[l];
			const bool want_open = l < int(st.rung.size()) && st.rung[l];
			bad += a->gen->house_closed(l) != want_closed ? 1 : 0;
			bad += a->gen->door_open(l) != want_open ? 1 : 0;
			bad += want_closed && !want_open ? 1 : 0; // never cleared without a trick-or-treat
			closed += want_closed ? 1 : 0;
			twice += ann(t.home, l) > 1 ? 1 : 0;
		}
		check(bad == 0, vformat("%s: nodes match the saved state, no house cleared without a ring (%d wrong, %d cleared)", when, bad, closed));
		check(twice == 0, vformat("%s: no house announced twice", when));
		check(total_ann() == t.total_announced_before, vformat("%s: no re-announce (%d announced, %d before)", when, total_ann(), t.total_announced_before));
		check(st.lot_cleared[t.s[0]] && st.lot_cleared[t.s[1]] && !st.lot_cleared[t.s[2]] && !st.lot_cleared[t.s[3]],
				vformat("%s: rule lots cleared %d %d, not cleared %d %d", when, t.s[0], t.s[1], t.s[2], t.s[3]));
	};

	Area *h = streamer_->area(t.home);
	const bool needs_home = (t.phase >= 1 && t.phase <= 8) || (t.phase >= 15 && t.phase <= 20) || t.phase == 22;
	if (needs_home && (!h || !h->gen->candy_field())) {
		check(false, vformat("phase %d: home attached", t.phase));
		t.phase = 99;
		return;
	}
	switch (t.phase) {
		case 0: { // the home area, settled
			if (frame_ < 90 || !h || !h->gen->candy_field()) {
				return;
			}
			const GenContext *ctx = h->gen->context();
			std::vector<int> per(ctx->lots.size(), 0);
			for (const CandySpec &c : ctx->candy) {
				if (c.lot >= 0 && c.lot < int(per.size())) {
					per[c.lot]++;
				}
			}
			int k = 0;
			for (int l = 0; l < int(per.size()) && k < 4; ++l) {
				if (per[l] >= 3) {
					t.s[k++] = l;
				}
			}
			int empty = 0;
			for (int n : per) {
				empty += n == 0 ? 1 : 0;
			}
			UtilityFunctions::print(vformat("DOOR_TEST home %s: lots %d (%d with no candy), candy %d, rule lots %d %d %d %d", String(h->plan.name.c_str()), int(per.size()), empty,
					int(ctx->candy.size()), t.s[0], t.s[1], t.s[2], t.s[3]));
			if (k < 4) {
				check(false, "four lots with candy");
				t.phase = 99;
				return;
			}
			park();
			t.phase = 1;
			return;
		}
		case 1: // rule 1: all candy first (no ring): never cleared
			h->gen->candy_field()->collect_lot_now(t.s[0]);
			t.phase = 2;
			t.wait = 2;
			return;
		case 2:
			check(placed_left(h, t.s[0]) == 0 && ann(t.home, t.s[0]) == 0, vformat("lot %d: all candy, not rung: not cleared (announced %d)", t.s[0], ann(t.home, t.s[0])));
			ring(h, t.s[0]);
			t.phase = 3;
			return;
		case 3:
			measure();
			check(ann(t.home, t.s[0]) == 1, vformat("lot %d: then rung: cleared once, at the ring (announced %d)", t.s[0], ann(t.home, t.s[0])));
			ring(h, t.s[1]); // rule 2: ring first, then the candy
			t.phase = 4;
			return;
		case 4:
			measure();
			check(ann(t.home, t.s[1]) == 0, vformat("lot %d: rung, candy left: not cleared (announced %d)", t.s[1], ann(t.home, t.s[1])));
			t.phase = 5;
			t.wait = 90; // the treats land
			return;
		case 5:
			h->gen->candy_field()->collect_lot_now(t.s[1]);
			t.phase = 6;
			t.wait = 2;
			return;
		case 6:
			check(ann(t.home, t.s[1]) == 1, vformat("lot %d: then all its candy: cleared once (announced %d)", t.s[1], ann(t.home, t.s[1])));
			h->gen->candy_field()->collect_lot_now(t.s[1]); // nothing left: no second announce
			h->gen->candy_field()->collect_lot_now(t.s[0]); // its treats
			ring(h, t.s[2]); // rule 3: rung only
			t.phase = 7;
			return;
		case 7:
			measure();
			check(ann(t.home, t.s[1]) == 1 && ann(t.home, t.s[0]) == 1, vformat("lots %d %d: picking up their treats announces nothing more (%d %d)", t.s[0], t.s[1], ann(t.home, t.s[0]),
					ann(t.home, t.s[1])));
			check(ann(t.home, t.s[2]) == 0, vformat("lot %d: rung only: not cleared (announced %d)", t.s[2], ann(t.home, t.s[2])));
			h->gen->candy_field()->collect_lot_now(t.s[3]); // rule 4: candy only (rung after the save round trip)
			t.queue.clear();
			for (int l = 0; l < h->gen->get_lot_count(); ++l) {
				if (l != t.s[0] && l != t.s[1] && l != t.s[2] && l != t.s[3]) {
					t.queue.push_back(l);
				}
			}
			t.qi = 0;
			t.phase = 8;
			t.wait = 2;
			return;
		case 8: // every other door of the home area, one a frame
			measure();
			if (t.qi < t.queue.size()) {
				ring(h, t.queue[t.qi++]);
				return;
			}
			// The four rule doors count toward the totals too (they were measured as they rang).
			check(ann(t.home, t.s[3]) == 0, vformat("lot %d: all candy, not rung: not cleared (announced %d)", t.s[3], ann(t.home, t.s[3])));
			summary("home");
			t.phase = 9;
			return;
		case 9: { // across areas: the next settlement east, every door
			for (int i = 1; i < 8; ++i) {
				const RegionPlan &r = streamer_->plan().region(i, 0);
				if (r.settlement()) {
					t.east = WorldStreamer::area_key(i, 0);
					t.total_announced_before = total_ann();
					go(r.spawn);
					break;
				}
			}
			t.phase = 10;
			t.wait = 10;
			return;
		}
		case 10: {
			Area *e = streamer_->area(t.east);
			if (!e) {
				check(false, "east area attached");
				t.phase = 99;
				return;
			}
			t.queue.clear();
			int empty = 0;
			for (int l = 0; l < e->gen->get_lot_count(); ++l) {
				t.queue.push_back(l);
				empty += lot_candy_left(e, l) == 0 ? 1 : 0;
			}
			UtilityFunctions::print(vformat("DOOR_TEST east %s (%s): lots %d, %d with no candy of their own (cleared at the ring)", String(e->plan.name.c_str()),
					e->plan.kind == RegionKind::Town ? "town" : "hood", e->gen->get_lot_count(), empty));
			t.qi = 0;
			t.phase = 11;
			return;
		}
		case 11: {
			measure();
			Area *e = streamer_->area(t.east);
			if (e && t.qi < t.queue.size()) {
				ring(e, t.queue[t.qi++]);
				return;
			}
			summary(e ? e->plan.name.c_str() : "east");
			t.total_announced_before = total_ann();
			go(streamer_->plan().region(0, 0).spawn); // home again: a cached re-attach
			t.phase = 12;
			t.wait = 10;
			return;
		}
		case 12:
			state_check("cached re-attach");
			go(streamer_->plan().region(0, 0).spawn + Vector3(0, 0, 1400.0f)); // far away: home is evicted
			t.phase = 13;
			t.wait = 300;
			return;
		case 13:
			go(streamer_->plan().region(0, 0).spawn); // built again from the saved state
			t.phase = 14;
			t.wait = 10;
			return;
		case 14: {
			state_check("evicted and rebuilt");
			const Dictionary d = save_state();
			UtilityFunctions::print("DOOR_TEST save/load round trip");
			const bool ok = load_state(d);
			check(ok, "load_state accepted the save");
			t.phase = 15;
			t.wait = 30;
			return;
		}
		case 15:
			state_check("after save and load");
			ring(h, t.s[3]); // rule 4 finished: the candy was taken before the save, the ring comes after
			t.phase = 16;
			return;
		case 16:
			measure();
			check(ann(t.home, t.s[3]) == 1, vformat("lot %d: candy before the save, ring after: cleared once (announced %d)", t.s[3], ann(t.home, t.s[3])));
			h->gen->candy_field()->collect_lot_now(t.s[2]);
			t.phase = 17;
			t.wait = 2;
			return;
		case 17:
			check(ann(t.home, t.s[2]) == 1, vformat("lot %d: rung before the save, candy after: cleared once (announced %d)", t.s[2], ann(t.home, t.s[2])));
			t.phase = 18;
			return;
		case 18: { // the area goals: every house cleared and every nest down make the area all clear, once
			check(t.complete_calls == 0, vformat("not all clear yet (announced %d)", t.complete_calls));
			for (int l = 0; l < h->gen->get_lot_count(); ++l) {
				h->gen->candy_field()->collect_lot_now(l);
			}
			t.phase = 19;
			t.wait = 90;
			return;
		}
		case 19: {
			for (int l = 0; l < h->gen->get_lot_count(); ++l) {
				h->gen->candy_field()->collect_lot_now(l); // treats that were still flying
			}
			int cleared = 0;
			for (char c : h->state->lot_cleared) {
				cleared += c ? 1 : 0;
			}
			check(cleared == h->gen->get_lot_count(), vformat("every house cleared (%d of %d)", cleared, h->gen->get_lot_count()));
			check(t.complete_calls == 0 || h->state->freed, vformat("houses alone do not make it all clear while nests stand (announced %d)", t.complete_calls));
			std::vector<Damageable *> targets;
			if (infest_.count(t.home)) {
				infest_[t.home]->gather(targets);
			}
			for (Damageable *d : targets) {
				if (!d->is_monster()) {
					d->take_hit(1e6f, d->target_point());
				}
			}
			t.phase = 20;
			t.wait = 60;
			return;
		}
		case 20:
			check(h->state->freed, "home freed");
			check(h->state->complete && t.complete_calls == 1, vformat("home all clear, announced once (%d)", t.complete_calls));
			t.total_announced_before = total_ann();
			go(streamer_->plan().region(0, 0).spawn + Vector3(0, 0, 400.0f)); // home detached (kept in the cache)
			t.phase = 21;
			t.wait = 10;
			return;
		case 21:
			go(streamer_->plan().region(0, 0).spawn);
			t.phase = 22;
			t.wait = 10;
			return;
		case 22:
			check(h->state->complete && t.complete_calls == 1 && total_ann() == t.total_announced_before,
					vformat("after a re-attach: still all clear, nothing re-announced (%d, %d)", t.complete_calls, total_ann()));
			t.phase = 99;
			return;
		default:
			UtilityFunctions::print(vformat("DOOR_TEST done: %d checks, %d failures", t.checks, t.failures));
			delete door_test_;
			door_test_ = nullptr;
			get_tree()->quit();
			return;
	}
}

} // namespace tot
