/*
 * RTC Room stubs for EOS_Lobby — SDK 1.12+
 * GetRTCRoomName, IsRTCRoomConnected, AddNotifyRTCRoomConnectionChanged
 *
 * This emulator does not implement real voice/RTC.  These stubs return safe
 * values so games that query RTC state do not crash or null-deref.
 */

#include "eossdk_lobby.h"
#include "eossdk_platform.h"
#include "eos_client_api.h"
#include "settings.h"

namespace sdk
{

/**
 * EOS_Lobby_GetRTCRoomName
 * Writes the RTC room name string for a lobby into OutBuffer.
 * We return a deterministic fake name based on the lobby id so the game
 * can store it without crashing; the RTC subsystem itself is not emulated.
 */
EOS_EResult EOSSDK_Lobby::GetRTCRoomName(
    const EOS_Lobby_GetRTCRoomNameOptions* Options,
    char*     OutBuffer,
    uint32_t* InOutBufferLength)
{
    TRACE_FUNC();

    if (Options == nullptr || InOutBufferLength == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    if (Options->LobbyId == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    // Build a deterministic name: "RTC_<lobby_id>"
    std::string room_name = std::string("RTC_") + Options->LobbyId;

    uint32_t needed = static_cast<uint32_t>(room_name.size()) + 1u;

    if (OutBuffer == nullptr)
    {
        *InOutBufferLength = needed;
        return EOS_EResult::EOS_Success;
    }

    if (*InOutBufferLength < needed)
    {
        *InOutBufferLength = needed;
        return EOS_EResult::EOS_LimitExceeded;
    }

    strncpy(OutBuffer, room_name.c_str(), *InOutBufferLength);
    OutBuffer[*InOutBufferLength - 1] = '\0';
    *InOutBufferLength = needed;
    return EOS_EResult::EOS_Success;
}

/**
 * EOS_Lobby_IsRTCRoomConnected
 * Always reports NOT connected — the emulator has no real RTC.
 * Returns EOS_Success so callers do not treat it as an error; they will
 * simply see bOutIsConnected == EOS_FALSE and fall back gracefully.
 */
EOS_EResult EOSSDK_Lobby::IsRTCRoomConnected(
    const EOS_Lobby_IsRTCRoomConnectedOptions* Options,
    EOS_Bool* bOutIsConnected)
{
    TRACE_FUNC();

    if (Options == nullptr || bOutIsConnected == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    if (Options->LobbyId == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    // Lobby must exist
    if (get_lobby_by_id(Options->LobbyId) == nullptr)
        return EOS_EResult::EOS_NotFound;

    *bOutIsConnected = EOS_FALSE;
    return EOS_EResult::EOS_Success;
}

/**
 * EOS_Lobby_AddNotifyRTCRoomConnectionChanged
 * Registers a notification slot for future RTC connection-change events.
 * Because we never actually connect to RTC the callback will never fire,
 * but the handle must be non-zero so RemoveNotify does not assert.
 */
EOS_NotificationId EOSSDK_Lobby::AddNotifyRTCRoomConnectionChanged(
    const EOS_Lobby_AddNotifyRTCRoomConnectionChangedOptions* Options,
    void* ClientData,
    const EOS_Lobby_OnRTCRoomConnectionChangedCallback NotificationFn)
{
    TRACE_FUNC();

    if (Options == nullptr || NotificationFn == nullptr)
        return EOS_INVALID_NOTIFICATIONID;

    pFrameResult_t res(new FrameResult);
    EOS_Lobby_RTCRoomConnectionChangedCallbackInfo& rcci =
        res->CreateCallback<EOS_Lobby_RTCRoomConnectionChangedCallbackInfo>((CallbackFunc)NotificationFn);

    rcci.ClientData      = ClientData;
    rcci.bIsConnected    = EOS_FALSE;
    rcci.DisconnectReason = EOS_EResult::EOS_NoConnection;
    {
        char* str = new char[max_accountid_length];
        *str = '\0';
        rcci.LobbyId = str;
    }

    return GetCB_Manager().add_notification(this, res);
}

void EOSSDK_Lobby::RemoveNotifyRTCRoomConnectionChanged(EOS_NotificationId InId)
{
    TRACE_FUNC();
    GetCB_Manager().remove_notification(this, InId);
}

} // namespace sdk
