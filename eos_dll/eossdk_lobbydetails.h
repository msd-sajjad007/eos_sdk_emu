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
#include "network_proto.pb.h"

namespace sdk
{

struct lobby_state_t;

class EOSSDK_LobbyDetails
{
public:
    lobby_state_t _state;

    EOSSDK_LobbyDetails();
    virtual ~EOSSDK_LobbyDetails();

    EOS_ProductUserId  GetLobbyOwner(const EOS_LobbyDetails_GetLobbyOwnerOptions* Options);
    EOS_EResult        CopyInfo(const EOS_LobbyDetails_CopyInfoOptions* Options, EOS_LobbyDetails_Info** OutLobbyDetailsInfo);
    uint32_t           GetAttributeCount(const EOS_LobbyDetails_GetAttributeCountOptions* Options);
    EOS_EResult        CopyAttributeByIndex(const EOS_LobbyDetails_CopyAttributeByIndexOptions* Options, EOS_Lobby_Attribute** OutAttribute);
    EOS_EResult        CopyAttributeByKey(const EOS_LobbyDetails_CopyAttributeByKeyOptions* Options, EOS_Lobby_Attribute** OutAttribute);
    uint32_t           GetMemberCount(const EOS_LobbyDetails_GetMemberCountOptions* Options);
    EOS_ProductUserId  GetMemberByIndex(const EOS_LobbyDetails_GetMemberByIndexOptions* Options);
    uint32_t           GetMemberAttributeCount(const EOS_LobbyDetails_GetMemberAttributeCountOptions* Options);
    EOS_EResult        CopyMemberAttributeByIndex(const EOS_LobbyDetails_CopyMemberAttributeByIndexOptions* Options, EOS_Lobby_Attribute** OutAttribute);
    EOS_EResult        CopyMemberAttributeByKey(const EOS_LobbyDetails_CopyMemberAttributeByKeyOptions* Options, EOS_Lobby_Attribute** OutAttribute);
    void               Release();
};

} // namespace sdk
