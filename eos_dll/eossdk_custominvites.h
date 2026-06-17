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
    // Stub implementation of EOS_CustomInvites interface (SDK ~1.14+)
    // Games that call these functions will get safe no-op returns.
    class EOSSDK_CustomInvites :
        public IRunCallback
    {
    public:
        EOSSDK_CustomInvites()  = default;
        ~EOSSDK_CustomInvites() = default;

        virtual bool CBRunFrame()                           { return true; }
        virtual bool RunCallbacks(pFrameResult_t res)       { return true; }
        virtual void FreeCallback(pFrameResult_t res)       {}

        EOS_EResult        SetCustomInvite(const EOS_CustomInvites_SetCustomInviteOptions* Options);
        EOS_EResult        SendCustomInvite(const EOS_CustomInvites_SendCustomInviteOptions* Options, void* ClientData, const EOS_CustomInvites_OnSendCustomInviteCallback CompletionDelegate);
        EOS_NotificationId AddNotifyCustomInviteReceived(const EOS_CustomInvites_AddNotifyCustomInviteReceivedOptions* Options, void* ClientData, const EOS_CustomInvites_OnCustomInviteReceivedCallback NotificationFn);
        void               RemoveNotifyCustomInviteReceived(EOS_NotificationId NotificationId);
        EOS_NotificationId AddNotifyCustomInviteAccepted(const EOS_CustomInvites_AddNotifyCustomInviteAcceptedOptions* Options, void* ClientData, const EOS_CustomInvites_OnCustomInviteAcceptedCallback NotificationFn);
        void               RemoveNotifyCustomInviteAccepted(EOS_NotificationId NotificationId);
        EOS_EResult        FinalizeInvite(const EOS_CustomInvites_FinalizeInviteOptions* Options);
    };
}
