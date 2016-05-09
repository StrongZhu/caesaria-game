engine.log("CaesarIA: load modules started")

var modules = [
    "namespaces",
    "math",
    "common",
    "g_config",
    "g_config_goods",
    "vector",
    "if_session",
    "if_city",
    "if_picture",
    "if_datetime",
    "if_overlay",
    "if_house",
    "if_walker",
    "if_gui_widgets",
    "if_gui_window",
    "g_ui",
    "g_fs",
    "g_config_colors",
    "g_config_languages",
    "g_config_ranks",
    "g_config_movie",
    "g_config_entertainment",
    "g_config_steam",
    "g_config_minimap",
    "g_config_climate",
    "g_config_house",
    "g_config_religion",
    "g_config_metric",
    "g_config_service",
    "g_config_key",
    "g_config_tile",
    "g_config_walker",
    "g_config_tilemap",
    "g_config_widget",
    "g_config_construction",
    "init_render",
    "init_game",
    "game_events",
    "game_pantheon",
    "game_sound_player",
    "game_event_manager",
    "game_tutorials",
    "game_ui_dialogs",
    "game_ui_dialogs_audio_opts",
    "game_ui_dialogs_video_options",
    "game_ui_dialog_player_salary_settings",
    "game_ui_infobox_temple",
    "game_ui_infobox_house",
    "game_ui_infobox_manager",
    "game_ui_infobox_engineering",
    "game_ui_infobox_market",
    "game_ui_infobox_warehouse",
    "game_ui_infobox_walker",
    "game_ui_infobox_event",
    "mission_events",
    "mission_timescale",
    "splash_common",
    "sim_ui_advisor_religion",
    "sim_ui_advisor_education",
    "sim_ui_dialogs_savegame",
    "sim_ui_dialogs_mission_targets",
    "sim_ui_dialogs_city_settings",
    "sim_ui_dialogs_speed_settings",
    "sim_ui_dialogs_gift_emperor",
    "sim_ui_dialogs_scribesmessages",
    "sim_ui_dialogs_citydonation",
    "sim_ui_dialogs_festivalplaning",
    "sim_ui_topmenu_debug",
    "sim_ui_topmenu",
    "sim_ui_buildmenu",
    "sim_ui_overlaymenu",
    "sim_ui_rpanel",
    "sim_ui_menu",
    "sim_ui_shortmenu",
    "sim_ui_tutorial",
    "sim_hotkeys",
    "mission_common",
    "lobby",
    "lobby_steam",
    "lobby_ui_dialogs_package_options",
    "lobby_ui_loadgame_loadmission"
]

for (var i in modules) {
    engine.log("Loading module " + modules[i])
    engine.loadModule(":/system/" + modules[i] + ".js")
}
