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

#include "eossdk_sanctions.h"

namespace sdk
{

void EOSSDK_Sanctions::QueryActivePlayerSanctions(
    const EOS_Sanctions_QueryActivePlayerSanctionsOptions* Options,
    void* ClientData,
    const EOS_Sanctions_OnQueryActivePlayerSanctionsCallback CompletionDelegate)
{
    // Emulator: player has no sanctions — always return success with 0 sanctions
    if (CompletionDelegate)
    {
        EOS_Sanctions_QueryActivePlayerSanctionsCallbackInfo info = {};
        info.ResultCode      = EOS_EResult::EOS_Success;
        info.ClientData      = ClientData;
        info.LocalUserId     = (Options != nullptr) ? Options->LocalUserId : nullptr;
        info.TargetUserId    = (Options != nullptr) ? Options->TargetUserId : nullptr;
        CompletionDelegate(&info);
    }
}

uint32_t EOSSDK_Sanctions::GetPlayerSanctionCount(const EOS_Sanctions_GetPlayerSanctionCountOptions* Options)
{
    return 0;
}

EOS_EResult EOSSDK_Sanctions::CopyPlayerSanctionByIndex(
    const EOS_Sanctions_CopyPlayerSanctionByIndexOptions* Options,
    EOS_Sanctions_PlayerSanction** OutSanction)
{
    if (OutSanction != nullptr)
        *OutSanction = nullptr;
    return EOS_EResult::EOS_NotFound;
}

void EOSSDK_Sanctions::PlayerSanction_Release(EOS_Sanctions_PlayerSanction* Sanction)
{
    // no-op: emulator never allocates a real sanction struct
}

} // namespace sdk
