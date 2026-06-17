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
#include "network.h"
#include "network_proto.pb.h"
// Full class definitions for lobby_state_t and EOSSDK_LobbyDetails.
// lobby_t inherits EOSSDK_LobbyDetails so the complete type must be visible.
#include "eossdk_lobbydetails.h"

namespace sdk
{

// Forward-declare EOSSDK_LobbySearch (used by pointer only, no inheritance).
class EOSSDK_LobbySearch;

struct lobby_invite_t
{
    EOS_ProductUserId peer_id;
    Lobby_Infos_pb infos;
};

// lobby_t inherits _state from EOSSDK_LobbyDetails; do NOT redeclare it here.
struct lobby_t : public EOSSDK_LobbyDetails
{
};

struct lobby_join_t
{
    pFrameResult_t cb;
};

class EOSSDK_Lobby :
    public IRunCallback,
    public IRunNetwork
{
    static std::chrono::milliseconds join_timeout;
    static int64_t join_id;

    std::unordered_map<std::string, lobby_t>         _lobbies;
    std::unordered_map<std::string, lobby_invite_t>  _lobby_invites;
    std::unordered_map<int64_t,    lobby_join_t>     _joins_requests;
    std::vector<EOSSDK_LobbySearch*>                 _lobbies_searchs;

public:
    EOSSDK_Lobby();
    ~EOSSDK_Lobby();

    lobby_state_t* get_lobby_by_id(std::string const& lobby_id);
    bool add_member_to_lobby   (std::string const& member, lobby_state_t* lobby);
    bool remove_member_from_lobby(std::string const& member, lobby_state_t* lobby);
    bool is_member_in_lobby    (std::string const& member, lobby_state_t* lobby);
    bool i_am_owner            (lobby_state_t* lobby);
    std::vector<lobby_state_t*> get_lobbies_from_attributes(google::protobuf::Map<std::string, Lobby_Search_Parameter> const& parameters);

    void notify_lobby_update(lobby_state_t* lobby);
    void notify_lobby_member_status_update(std::string const& member, EOS_ELobbyMemberStatus new_status, lobby_state_t* lobby);
    void notify_lobby_member_update(std::string const& member, lobby_state_t* lobby);
    void notify_lobby_invite_received(std::string const& invite_id, EOS_ProductUserId from_id);

    // Network send helpers
    bool send_to_all_members(Network_Message_pb& msg, lobby_state_t* lobby);
    bool send_to_all_members_or_owner(Network_Message_pb& msg, lobby_state_t* lobby);
    bool send_lobby_update(lobby_state_t* pLobby);
    bool send_lobbies_search_response(Network::peer_t const& peerid, Lobbies_Search_response_pb* resp);
    bool send_lobby_join_request(Network::peer_t const& peerid, Lobby_Join_Request_pb* req);
    bool send_lobby_join_response(Network::peer_t const& peerid, Lobby_Join_Response_pb* resp);
    bool send_lobby_invite(Network::peer_t const& peerid, Lobby_Invite_pb* invite);
    bool send_lobby_member_update(Network::peer_t const& member_id, lobby_state_t* pLobby);
    bool send_lobby_member_join(Network::peer_t const& member_id, lobby_state_t* lobby);
    bool send_lobby_member_leave(Network::peer_t const& member_id, lobby_state_t* lobby, EOS_ELobbyMemberStatus reason);
    bool send_lobby_member_promote(Network::peer_t const& member_id, lobby_state_t* lobby);

    // Network receive handlers
    bool on_peer_disconnect(Network_Message_pb const& msg, Network_Peer_Disconnect_pb const& peer);
    bool on_lobby_update(Network_Message_pb const& msg, Lobby_Update_pb const& update);
    bool on_lobby_member_update(Network_Message_pb const& msg, Lobby_Member_Update_pb const& update);
    bool on_lobbies_search(Network_Message_pb const& msg, Lobbies_Search_pb const& search);
    bool on_lobby_join_request(Network_Message_pb const& msg, Lobby_Join_Request_pb const& req);
    bool on_lobby_join_response(Network_Message_pb const& msg, Lobby_Join_Response_pb const& resp);
    bool on_lobby_invite(Network_Message_pb const& msg, Lobby_Invite_pb const& invite);
    bool on_lobby_member_join(Network_Message_pb const& msg, Lobby_Member_Join_pb const& join);
    bool on_lobby_member_leave(Network_Message_pb const& msg, Lobby_Member_Leave_pb const& leave);
    bool on_lobby_member_promote(Network_Message_pb const& msg, Lobby_Member_Promote_pb const& promote);

    virtual bool CBRunFrame();
    virtual bool RunNetwork(Network_Message_pb const& msg);
    virtual bool RunCallbacks(pFrameResult_t res);
    virtual void FreeCallback(pFrameResult_t res);

    void CreateLobby(const EOS_Lobby_CreateLobbyOptions* Options, void* ClientData, const EOS_Lobby_OnCreateLobbyCallback CompletionDelegate);
    void DestroyLobby(const EOS_Lobby_DestroyLobbyOptions* Options, void* ClientData, const EOS_Lobby_OnDestroyLobbyCallback CompletionDelegate);
    void JoinLobby(const EOS_Lobby_JoinLobbyOptions* Options, void* ClientData, const EOS_Lobby_OnJoinLobbyCallback CompletionDelegate);
    void LeaveLobby(const EOS_Lobby_LeaveLobbyOptions* Options, void* ClientData, const EOS_Lobby_OnLeaveLobbyCallback CompletionDelegate);
    EOS_EResult UpdateLobbyModification(const EOS_Lobby_UpdateLobbyModificationOptions* Options, EOS_HLobbyModification* OutLobbyModificationHandle);
    void UpdateLobby(const EOS_Lobby_UpdateLobbyOptions* Options, void* ClientData, const EOS_Lobby_OnUpdateLobbyCallback CompletionDelegate);
    void PromoteMember(const EOS_Lobby_PromoteMemberOptions* Options, void* ClientData, const EOS_Lobby_OnPromoteMemberCallback CompletionDelegate);
    void KickMember(const EOS_Lobby_KickMemberOptions* Options, void* ClientData, const EOS_Lobby_OnKickMemberCallback CompletionDelegate);
    EOS_NotificationId AddNotifyLobbyUpdateReceived(const EOS_Lobby_AddNotifyLobbyUpdateReceivedOptions* Options, void* ClientData, const EOS_Lobby_OnLobbyUpdateReceivedCallback NotificationFn);
    void RemoveNotifyLobbyUpdateReceived(EOS_NotificationId InId);
    EOS_NotificationId AddNotifyLobbyMemberUpdateReceived(const EOS_Lobby_AddNotifyLobbyMemberUpdateReceivedOptions* Options, void* ClientData, const EOS_Lobby_OnLobbyMemberUpdateReceivedCallback NotificationFn);
    void RemoveNotifyLobbyMemberUpdateReceived(EOS_NotificationId InId);
    EOS_NotificationId AddNotifyLobbyMemberStatusReceived(const EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions* Options, void* ClientData, const EOS_Lobby_OnLobbyMemberStatusReceivedCallback NotificationFn);
    void RemoveNotifyLobbyMemberStatusReceived(EOS_NotificationId InId);
    void SendInvite(const EOS_Lobby_SendInviteOptions* Options, void* ClientData, const EOS_Lobby_OnSendInviteCallback CompletionDelegate);
    void RejectInvite(const EOS_Lobby_RejectInviteOptions* Options, void* ClientData, const EOS_Lobby_OnRejectInviteCallback CompletionDelegate);
    void QueryInvites(const EOS_Lobby_QueryInvitesOptions* Options, void* ClientData, const EOS_Lobby_OnQueryInvitesCallback CompletionDelegate);
    uint32_t GetInviteCount(const EOS_Lobby_GetInviteCountOptions* Options);
    EOS_EResult GetInviteIdByIndex(const EOS_Lobby_GetInviteIdByIndexOptions* Options, char* OutBuffer, int32_t* InOutBufferLength);
    EOS_EResult CreateLobbySearch(const EOS_Lobby_CreateLobbySearchOptions* Options, EOS_HLobbySearch* OutLobbySearchHandle);
    EOS_NotificationId AddNotifyLobbyInviteReceived(const EOS_Lobby_AddNotifyLobbyInviteReceivedOptions* Options, void* ClientData, const EOS_Lobby_OnLobbyInviteReceivedCallback NotificationFn);
    void RemoveNotifyLobbyInviteReceived(EOS_NotificationId InId);
    EOS_NotificationId AddNotifyLobbyInviteAccepted(const EOS_Lobby_AddNotifyLobbyInviteAcceptedOptions* Options, void* ClientData, const EOS_Lobby_OnLobbyInviteAcceptedCallback NotificationFn);
    void RemoveNotifyLobbyInviteAccepted(EOS_NotificationId InId);
    EOS_NotificationId AddNotifyJoinLobbyAccepted(const EOS_Lobby_AddNotifyJoinLobbyAcceptedOptions* Options, void* ClientData, const EOS_Lobby_OnJoinLobbyAcceptedCallback NotificationFn);
    void RemoveNotifyJoinLobbyAccepted(EOS_NotificationId InId);
    EOS_EResult CopyLobbyDetailsHandleByInviteId(const EOS_Lobby_CopyLobbyDetailsHandleByInviteIdOptions* Options, EOS_HLobbyDetails* OutLobbyDetailsHandle);
    EOS_EResult CopyLobbyDetailsHandleByUiEventId(const EOS_Lobby_CopyLobbyDetailsHandleByUiEventIdOptions* Options, EOS_HLobbyDetails* OutLobbyDetailsHandle);
    EOS_EResult CopyLobbyDetailsHandle(const EOS_Lobby_CopyLobbyDetailsHandleOptions* Options, EOS_HLobbyDetails* OutLobbyDetailsHandle);

    // RTC room stubs (SDK 1.12+)
    EOS_EResult GetRTCRoomName(const EOS_Lobby_GetRTCRoomNameOptions* Options, char* OutBuffer, uint32_t* InOutBufferLength);
    EOS_EResult IsRTCRoomConnected(const EOS_Lobby_IsRTCRoomConnectedOptions* Options, EOS_Bool* bOutIsConnected);
    EOS_NotificationId AddNotifyRTCRoomConnectionChanged(const EOS_Lobby_AddNotifyRTCRoomConnectionChangedOptions* Options, void* ClientData, const EOS_Lobby_OnRTCRoomConnectionChangedCallback NotificationFn);
    void RemoveNotifyRTCRoomConnectionChanged(EOS_NotificationId InId);
};

} // namespace sdk
