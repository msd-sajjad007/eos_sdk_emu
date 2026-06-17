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

using namespace sdk;

EOS_DECLARE_FUNC(void) EOS_Sanctions_QueryActivePlayerSanctions(EOS_HSanctions Handle, const EOS_Sanctions_QueryActivePlayerSanctionsOptions* Options, void* ClientData, const EOS_Sanctions_OnQueryActivePlayerSanctionsCallback CompletionDelegate)
{
    if (Handle == nullptr)
    {
        if (CompletionDelegate)
        {
            EOS_Sanctions_QueryActivePlayerSanctionsCallbackInfo info = {};
            info.ResultCode = EOS_EResult::EOS_InvalidParameters;
            info.ClientData = ClientData;
            CompletionDelegate(&info);
        }
        return;
    }

    auto pInst = reinterpret_cast<EOSSDK_Sanctions*>(Handle);
    pInst->QueryActivePlayerSanctions(Options, ClientData, CompletionDelegate);
}

EOS_DECLARE_FUNC(uint32_t) EOS_Sanctions_GetPlayerSanctionCount(EOS_HSanctions Handle, const EOS_Sanctions_GetPlayerSanctionCountOptions* Options)
{
    if (Handle == nullptr)
        return 0;

    auto pInst = reinterpret_cast<EOSSDK_Sanctions*>(Handle);
    return pInst->GetPlayerSanctionCount(Options);
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Sanctions_CopyPlayerSanctionByIndex(EOS_HSanctions Handle, const EOS_Sanctions_CopyPlayerSanctionByIndexOptions* Options, EOS_Sanctions_PlayerSanction** OutSanction)
{
    if (Handle == nullptr)
    {
        if (OutSanction != nullptr) *OutSanction = nullptr;
        return EOS_EResult::EOS_InvalidParameters;
    }

    auto pInst = reinterpret_cast<EOSSDK_Sanctions*>(Handle);
    return pInst->CopyPlayerSanctionByIndex(Options, OutSanction);
}

EOS_DECLARE_FUNC(void) EOS_Sanctions_PlayerSanction_Release(EOS_Sanctions_PlayerSanction* Sanction)
{
    // Static release — no handle needed
    // Emulator never allocates a real sanction struct so this is a safe no-op
    (void)Sanction;
}
