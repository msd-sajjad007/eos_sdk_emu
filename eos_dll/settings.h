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

#pragma once

#include "common_includes.h"

struct Settings
{
    std::string username;
    std::string userid;
    std::string language;

    // EOS Platform identity fields (SDK 1.8+)
    std::string product_id;
    std::string sandbox_id;
    std::string deployment_id;

    // Networking
    std::string custom_broadcasts;
    uint16_t    p2p_port;
    uint16_t    max_p2p_ports_tried;

    // [FIX] Members referenced by eos_client_api.cpp
    bool        disable_online_networking;
    std::string gamename;

    static Settings& Inst();

    void load_settings();
    // [FIX] save_settings() declared here; persists gamename back to JSON
    void save_settings();
};
