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

#include "eossdk_custominvites.h"

namespace sdk
{

EOS_EResult EOSSDK_CustomInvites::SetCustomInvite(const EOS_CustomInvites_SetCustomInviteOptions* Options)
{
    if (Options == nullptr || Options->Payload == nullptr)
        return EOS_EResult::EOS_InvalidParameters;
    // Emulator: accept the payload silently; no real invite system
    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_CustomInvites::SendCustomInvite(
    const EOS_CustomInvites_SendCustomInviteOptions* Options,
    void* ClientData,
    const EOS_CustomInvites_OnSendCustomInviteCallback CompletionDelegate)
{
    if (Options == nullptr)
    {
        if (CompletionDelegate)
        {
            EOS_CustomInvites_SendCustomInviteCallbackInfo info = {};
            info.ResultCode  = EOS_EResult::EOS_InvalidParameters;
            info.ClientData  = ClientData;
            CompletionDelegate(&info);
        }
        return EOS_EResult::EOS_InvalidParameters;
    }
    // Emulator: immediately succeed; no real network send
    if (CompletionDelegate)
    {
        EOS_CustomInvites_SendCustomInviteCallbackInfo info = {};
        info.ResultCode  = EOS_EResult::EOS_Success;
        info.ClientData  = ClientData;
        info.LocalUserId = Options->LocalUserId;
        CompletionDelegate(&info);
    }
    return EOS_EResult::EOS_Success;
}

EOS_NotificationId EOSSDK_CustomInvites::AddNotifyCustomInviteReceived(
    const EOS_CustomInvites_AddNotifyCustomInviteReceivedOptions* Options,
    void* ClientData,
    const EOS_CustomInvites_OnCustomInviteReceivedCallback NotificationFn)
{
    // Emulator: no incoming invites; return a valid-looking but inactive ID
    return EOS_INVALID_NOTIFICATIONID;
}

void EOSSDK_CustomInvites::RemoveNotifyCustomInviteReceived(EOS_NotificationId NotificationId)
{
    // no-op
}

EOS_NotificationId EOSSDK_CustomInvites::AddNotifyCustomInviteAccepted(
    const EOS_CustomInvites_AddNotifyCustomInviteAcceptedOptions* Options,
    void* ClientData,
    const EOS_CustomInvites_OnCustomInviteAcceptedCallback NotificationFn)
{
    return EOS_INVALID_NOTIFICATIONID;
}

void EOSSDK_CustomInvites::RemoveNotifyCustomInviteAccepted(EOS_NotificationId NotificationId)
{
    // no-op
}

EOS_EResult EOSSDK_CustomInvites::FinalizeInvite(const EOS_CustomInvites_FinalizeInviteOptions* Options)
{
    return EOS_EResult::EOS_Success;
}

} // namespace sdk
