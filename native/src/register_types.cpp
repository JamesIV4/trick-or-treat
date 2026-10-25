#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "gen/neighborhood_generator.h"
#include "play/candy_field.h"
#include "play/dragon_controller.h"
#include "play/festival_camera.h"
#include "play/festival_game.h"
#include "play/storm_sky.h"
#include "play/combat.h"
#include "play/loose_candy.h"
#include "play/monsters.h"
#include "play/ride_on_car.h"
#include "play/vfx.h"
#include "world/world_streamer.h"

using namespace godot;

void initialize_totnative(ModuleInitializationLevel level) {
	if (level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(tot::CandyField);
	GDREGISTER_CLASS(tot::NeighborhoodGenerator);
	GDREGISTER_CLASS(tot::DragonController);
	GDREGISTER_CLASS(tot::FestivalCamera);
	GDREGISTER_CLASS(tot::StormSky);
	GDREGISTER_CLASS(tot::WorldStreamer);
	GDREGISTER_CLASS(tot::Combat);
	GDREGISTER_CLASS(tot::LooseCandy);
	GDREGISTER_CLASS(tot::Monster);
	GDREGISTER_CLASS(tot::MonsterNest);
	GDREGISTER_CLASS(tot::ThornVine);
	GDREGISTER_CLASS(tot::VineTendril);
	GDREGISTER_CLASS(tot::RideOnCar);
	GDREGISTER_CLASS(tot::Vfx);
	GDREGISTER_CLASS(tot::FestivalGame);
}

void uninitialize_totnative(ModuleInitializationLevel level) {
	if (level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C" {
GDExtensionBool GDE_EXPORT totnative_library_init(GDExtensionInterfaceGetProcAddress get_proc_address,
		GDExtensionClassLibraryPtr library, GDExtensionInitialization *initialization) {
	GDExtensionBinding::InitObject init_obj(get_proc_address, library, initialization);
	init_obj.register_initializer(initialize_totnative);
	init_obj.register_terminator(uninitialize_totnative);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
	return init_obj.init();
}
}
