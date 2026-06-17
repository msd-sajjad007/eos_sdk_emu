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
#include "callback_manager.h"

namespace sdk
{
    // Stub implementation of EOS_Sanctions interface (SDK ~1.13+)
    class EOSSDK_Sanctions :
        public IRunCallback
    {
    public:
        EOSSDK_Sanctions()  = default;
        ~EOSSDK_Sanctions() = default;

        virtual bool CBRunFrame()                           { return true; }
        virtual bool RunCallbacks(pFrameResult_t res)       { return true; }
        virtual void FreeCallback(pFrameResult_t res)       {}

        void QueryActivePlayerSanctions(const EOS_Sanctions_QueryActivePlayerSanctionsOptions* Options, void* ClientData, const EOS_Sanctions_OnQueryActivePlayerSanctionsCallback CompletionDelegate);
        uint32_t    GetPlayerSanctionCount(const EOS_Sanctions_GetPlayerSanctionCountOptions* Options);
        EOS_EResult CopyPlayerSanctionByIndex(const EOS_Sanctions_CopyPlayerSanctionByIndexOptions* Options, EOS_Sanctions_PlayerSanction** OutSanction);
        void        PlayerSanction_Release(EOS_Sanctions_PlayerSanction* Sanction);
    };
}
