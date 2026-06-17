/*
 * Copyright (C) 2020 Nemirtingas
 * This file is part of the Nemirtingas's Epic Emulator
 *
 * The Nemirtingas's Epic Emulator is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * The Nemirtingas's Epic Emulator is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the Nemirtingas's Epic Emulator; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "settings.h"
#include "os_funcs.h"
#include "Log.h"

#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

Settings& Settings::Inst()
{
    static Settings _inst;
    return _inst;
}

void Settings::load_settings()
{
    auto settings_path = GetExePath() / "eos_settings.json";

    std::ifstream settings_file(settings_path);
    if (!settings_file.is_open())
    {
        APP_LOG(Log::LogLevel::WARN, "Could not open settings file: %s", settings_path.string().c_str());
        return;
    }

    json settings_json;
    try
    {
        settings_file >> settings_json;
    }
    catch (std::exception& e)
    {
        APP_LOG(Log::LogLevel::ERR, "Failed to parse settings file: %s", e.what());
        return;
    }

    // Core identity
    if (settings_json.contains("username") && settings_json["username"].is_string())
        username = settings_json["username"].get<std::string>();

    if (settings_json.contains("userid") && settings_json["userid"].is_string())
        userid = settings_json["userid"].get<std::string>();

    if (settings_json.contains("language") && settings_json["language"].is_string())
        language = settings_json["language"].get<std::string>();

    // EOS Platform identity fields (required by SDK 1.8+ games)
    if (settings_json.contains("product_id") && settings_json["product_id"].is_string())
        product_id = settings_json["product_id"].get<std::string>();

    if (settings_json.contains("sandbox_id") && settings_json["sandbox_id"].is_string())
        sandbox_id = settings_json["sandbox_id"].get<std::string>();

    if (settings_json.contains("deployment_id") && settings_json["deployment_id"].is_string())
        deployment_id = settings_json["deployment_id"].get<std::string>();

    // Networking
    if (settings_json.contains("custom_broadcasts") && settings_json["custom_broadcasts"].is_string())
        custom_broadcasts = settings_json["custom_broadcasts"].get<std::string>();

    if (settings_json.contains("p2p_port") && settings_json["p2p_port"].is_number_unsigned())
        p2p_port = settings_json["p2p_port"].get<uint16_t>();

    if (settings_json.contains("max_p2p_ports_tried") && settings_json["max_p2p_ports_tried"].is_number_unsigned())
        max_p2p_ports_tried = settings_json["max_p2p_ports_tried"].get<uint16_t>();

    // [FIX] disable_online_networking flag
    if (settings_json.contains("disable_online_networking") && settings_json["disable_online_networking"].is_boolean())
        disable_online_networking = settings_json["disable_online_networking"].get<bool>();
    else
        disable_online_networking = false;

    // [FIX] gamename field
    if (settings_json.contains("gamename") && settings_json["gamename"].is_string())
        gamename = settings_json["gamename"].get<std::string>();
    else
        gamename = "DefaultGameName";

    APP_LOG(Log::LogLevel::INFO, "Settings loaded: user='%s' id='%s' product='%s' sandbox='%s' deployment='%s' gamename='%s' disable_online_networking=%d",
        username.c_str(), userid.c_str(), product_id.c_str(), sandbox_id.c_str(), deployment_id.c_str(),
        gamename.c_str(), (int)disable_online_networking);
}

// [FIX] save_settings() — writes the current gamename back to eos_settings.json
// so that a game-supplied ProductName persists across restarts.
void Settings::save_settings()
{
    auto settings_path = GetExePath() / "eos_settings.json";

    json settings_json;

    // Read existing file first so we don't clobber unrelated fields
    {
        std::ifstream in(settings_path);
        if (in.is_open())
        {
            try { in >> settings_json; }
            catch (...) { settings_json = json::object(); }
        }
        else
        {
            settings_json = json::object();
        }
    }

    settings_json["gamename"]                  = gamename;
    settings_json["disable_online_networking"] = disable_online_networking;

    std::ofstream out(settings_path);
    if (!out.is_open())
    {
        APP_LOG(Log::LogLevel::WARN, "Could not write settings file: %s", settings_path.string().c_str());
        return;
    }

    out << settings_json.dump(4);
    APP_LOG(Log::LogLevel::INFO, "Settings saved: gamename='%s'", gamename.c_str());
}
