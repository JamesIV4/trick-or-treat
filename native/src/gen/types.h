#pragma once
// Plain data shared by the generator stages. All positions are world space unless noted.

#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <string>
#include <vector>

#include "core/consts.h"

namespace tot {

using godot::Basis;
using godot::Color;
using godot::Transform3D;
using godot::Vector3;

enum class Persona {
	PumpkinFanatics,
	GreenThumbs,
	YoungFamily,
	CozyGrandparents,
	CraftyArtists,
	Minimalists,
	SpookyCute,
	Bakers,
	Sporty,
	PetLovers,
	Stargazers,
	Tinkerers,
	Count
};

enum class Landmark {
	None,
	PumpkinPatch,
	Scarecrow,
	Sunflower,
	TreeHouse,
	HayRide,
	Cauldron,
	Inflatables,
	LanternPorch,
	CarvingContest,
	Count
};

const char *persona_name(Persona p);
const char *landmark_name(Landmark l);

enum class SettlementKind { Neighborhood, Town };

// Town shops (sign text and storefront dressing).
enum class ShopKind { Bakery, Candy, Toys, Books, Diner, Hardware, Florist, Costumes, Barber, Pets, Cinema, Pharmacy, Count };
const char *shop_sign(ShopKind k);

enum CandyKind {
	CANDY_PINK,
	CANDY_MINT,
	CANDY_GRAPE,
	CANDY_LEMON,
	CANDY_CHOCOLATE,
	CANDY_LOLLIPOP,
	CANDY_GOLDEN,
	CANDY_KIND_COUNT
};

inline int candy_value(int kind) {
	switch (kind) {
		case CANDY_CHOCOLATE:
			return 5;
		case CANDY_LOLLIPOP:
			return 25;
		case CANDY_GOLDEN:
			return 100;
		default:
			return 1;
	}
}

// Oriented box: collision volume and, when walkable, a platform whose top the player can stand on.
struct OBox {
	Transform3D xf; // centre and orientation
	Vector3 half;
	bool solid = true;
	bool walkable = false;
	bool bouncy = false;
	bool cylinder = false; // physics shape: cylinder (radius half.x, height 2*half.y) instead of box
	bool boundary = false; // playground edge (own collision layer, ignored by the camera)
	int lot = -1;
	float top() const { return xf.origin.y + half.y; }
};

struct PropPlacement {
	std::string id;
	Transform3D xf;
	int lot = -1;
	float wind = 0.0f; // sway amount for foliage
	bool collide = true;
	bool bouncy = false; // landing on top springs the player up
};

enum LightKind { LIGHT_PORCH, LIGHT_LAMP, LIGHT_JACK, LIGHT_LANTERN };

struct LightSpec {
	Vector3 pos;
	Color color;
	float energy = 1.0f;
	float range = 6.0f;
	int lot = -1;
	LightKind kind = LIGHT_PORCH;
};

struct CandySpec {
	Vector3 pos;
	int kind = CANDY_PINK;
	int lot = -1;
	int group = -1; // rhythm group: a run of candy laid along one jump or hop sequence
	int id = -1; // index in the settlement's candy list (persists which pieces were taken)
};

struct Circle {
	float x, z, r;
};

struct HouseSpec {
	float width = 8.0f, depth = 8.0f;
	int storeys = 1;
	float storey_h = 3.0f;
	float pitch_deg = 50.0f;
	bool ridge_along_x = true;
	float house_x = 0.0f; // lot-local centre x of the main body
	float front_z = -8.0f; // lot-local z of the front wall
	float porch_x = 0.0f, porch_w = 4.0f, porch_d = 2.4f;
	float deck_h = 0.6f;
	float door_x = 0.0f;
	bool garage = false;
	int garage_side = 1; // +1 right, -1 left
	bool chimney = true;
	int chimney_side = 1;
	bool roof_steps = false; // summit route (landmarks)
	int wall_cell = 0, roof_cell = 0, trim_cell = 0, door_cell = 0, garage_cell = 0;
	int fence_type = 0; // 0 none, 1 picket, 2 rail
	int fence_cell = 0;
	// Look and detail (all cells below are mat_cell-packed palette + material).
	int wall_mat = 0, upper_mat = 0, roof_mat = 0;
	int upper_cell = 0; // second-storey wall colour (Victorian two-tone)
	bool shutters = false;
	int shutter_cell = 0;
	bool flower_boxes = false;
	bool dormer = false;
	bool gingerbread = false;
	bool pediment = false;
	int curtain_cell = 0;
	int drive_mat = 0;
};

struct Lot {
	int index = 0;
	int side = 1; // +1 north (+z) of the main street, -1 south
	Transform3D xf; // lot frame: origin front-centre on the sidewalk edge, +Z toward the street
	float width = 16.0f, depth = 22.0f;
	float center_x = 0.0f; // world x along the street
	Persona persona = Persona::CozyGrandparents;
	Landmark landmark = Landmark::None;
	HouseSpec house;
	float drive_x = 0.0f; // lot-local driveway centre
	int passage_side = -1; // side passage (no driveway) side
	// Lot-local key points (y = height above ground).
	std::vector<Vector3> loop_route; // ground loop, two entrances
	std::vector<Vector3> roof_route; // porch-roof branch
	std::vector<Vector3> summit_route; // landmark summit branch
	Vector3 doorbell; // world
	Vector3 porch_light; // world
	int faces_used = 0; // bitmask of jack-o'-lantern faces (neighbors never match)
	int shop = -1; // ShopKind for town lots, -1 for houses
	Vector3 to_world(float x, float y, float z) const { return xf.xform(Vector3(x, y, z)); }
	Vector3 to_world(const Vector3 &p) const { return xf.xform(p); }
	Vector3 to_local(const Vector3 &p) const { return xf.affine_inverse().xform(p); }
};

struct StreetPlan {
	std::vector<Vector3> center; // main street centreline, 1 m steps along x
	std::vector<Vector3> left; // unit normal toward +z side
	float half_road = 4.0f;
	float curb = 0.3f;
	float walk = 2.7f; // grass verge + sidewalk
	float verge = 1.0f;
	float length = 96.0f;
	float lane_x = 0.0f;
	int lane_dir = 1; // lane runs toward +z (1) or -z (-1)
	float lane_len = 30.0f;
	Vector3 lane_origin;
	float edge() const { return half_road + curb + walk; } // sidewalk outer edge offset
	Vector3 at(float x) const;
	Vector3 left_at(float x) const;
};

} // namespace tot
