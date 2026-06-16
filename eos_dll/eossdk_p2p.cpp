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

#include "eossdk_p2p.h"
#include "eossdk_platform.h"
#include "eos_client_api.h"
#include "settings.h"

namespace sdk
{

decltype(EOSSDK_P2P::connecting_timeout) EOSSDK_P2P::connecting_timeout;
decltype(EOSSDK_P2P::connection_timeout) EOSSDK_P2P::connection_timeout;

EOSSDK_P2P::EOSSDK_P2P():
    next_requested_channel(-1),
    _relay_control(EOS_ERelayControl::EOS_RC_AllowRelays),
    _p2p_port(7777),
    _max_additional_ports_to_try(99),
    // Default 2 MB inbound queue — matches SDK 1.13 default
    _packet_queue_size_bytes(2 * 1024 * 1024),
    _packet_queue_used_bytes(0),
    _packet_queue_dropped_packets(0)
{
    GetCB_Manager().register_frame(this);
    GetCB_Manager().register_callbacks(this);

    GetNetwork().register_listener(this, 0, Network_Message_pb::MessagesCase::kP2P);
}

EOSSDK_P2P::~EOSSDK_P2P()
{
    GetNetwork().unregister_listener(this, 0, Network_Message_pb::MessagesCase::kP2P);

    GetCB_Manager().unregister_callbacks(this);
    GetCB_Manager().unregister_frame(this);

    GetCB_Manager().remove_all_notifications(this);
}


EOS_P2P_OnPeerConnectionEstablishedInfo* mCachedOpcei = NULL;

void EOSSDK_P2P::set_p2p_state_connected(EOS_ProductUserId remote_id, p2p_state_t& state)
{
    p2p_state_t::status_e oldStatus = state.status;
    state.status = p2p_state_t::status_e::connected;
    for (auto& out_msgs : state.p2p_out_messages)
    {// Send all previously stored messages
        send_p2p_data(remote_id->to_string(), &out_msgs);
    }
    state.p2p_out_messages.clear();


    std::vector<pFrameResult_t> notifs = std::move(GetCB_Manager().get_notifications(this, EOS_P2P_OnPeerConnectionEstablishedInfo::k_iCallback));
    if (notifs.empty()) {
        pFrameResult_t res(new FrameResult);
        mCachedOpcei = &res->CreateCallback<EOS_P2P_OnPeerConnectionEstablishedInfo>((CallbackFunc)NULL);
        if (oldStatus == p2p_state_t::status_e::connection_loss) {
            mCachedOpcei->ConnectionType = EOS_EConnectionEstablishedType::EOS_CET_Reconnection;
        }
        else mCachedOpcei->ConnectionType = EOS_EConnectionEstablishedType::EOS_CET_NewConnection;
        mCachedOpcei->RemoteUserId = remote_id;
        EOS_P2P_SocketId* socketId = new EOS_P2P_SocketId;
        socketId->ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
        strncpy(const_cast<char*>(socketId->SocketName), state.socket_name.c_str(), sizeof(EOS_P2P_SocketId::SocketName));
        mCachedOpcei->SocketId = socketId;
    }
    for (auto& notif : notifs)
    {
        EOS_P2P_OnPeerConnectionEstablishedInfo& opcei = notif->GetCallback<EOS_P2P_OnPeerConnectionEstablishedInfo>();
        if (oldStatus == p2p_state_t::status_e::connection_loss) {
            opcei.ConnectionType = EOS_EConnectionEstablishedType::EOS_CET_Reconnection;
        }
        else opcei.ConnectionType = EOS_EConnectionEstablishedType::EOS_CET_NewConnection;
        opcei.RemoteUserId = remote_id;
        EOS_P2P_SocketId * socketId = new EOS_P2P_SocketId;
        socketId->ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
        strncpy(const_cast<char*>(socketId->SocketName), state.socket_name.c_str(), sizeof(EOS_P2P_SocketId::SocketName));
        opcei.SocketId = socketId;
        mCachedOpcei = &opcei;
        notif->GetFunc()(notif->GetFuncParam());
    }
}

/**
 * Send a packet to a peer. Respects API version fields including API_003 reliability/discard options.
 */
EOS_EResult EOSSDK_P2P::SendPacket(const EOS_P2P_SendPacketOptions* Options)
{
    //TRACE_FUNC();
    GLOBAL_LOCK();
    
    if (Options == nullptr || Options->RemoteUserId == nullptr || Options->Data == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    p2p_state_t& p2p_state = _p2p_connections[Options->RemoteUserId];
    P2P_Data_Message_pb data;

    // -----------------------------------------------------------------------
    // API version dispatch — fall-through from highest to lowest is intentional
    // so each case enriches the base Options fields already accessible through
    // the latest struct layout.
    // API_003 adds: bAllowDelayedDelivery, bDisableAutoAcceptConnection,
    //               Channel (moved from socket options).
    // We read those extra flags here and honour them.
    // -----------------------------------------------------------------------
    bool allow_delayed_delivery    = true;  // SDK default
    bool disable_auto_accept       = false; // SDK default

    switch (Options->ApiVersion)
    {
        case EOS_P2P_SENDPACKET_API_003:
        {
            const EOS_P2P_SendPacketOptions003* opts = reinterpret_cast<const EOS_P2P_SendPacketOptions003*>(Options);
            allow_delayed_delivery  = (opts->bAllowDelayedDelivery == EOS_TRUE);
            disable_auto_accept     = (opts->bDisableAutoAcceptConnection == EOS_TRUE);
            // fall-through intentional
        }
        case EOS_P2P_SENDPACKET_API_002:
        {
            // API_002 has same layout as API_001 for the fields we read below
            // fall-through intentional
        }
        case EOS_P2P_SENDPACKET_API_001:
        {
            data.set_data(reinterpret_cast<const char*>(Options->Data), Options->DataLengthBytes);
            data.set_channel(Options->Channel);
            data.set_socket_name(Options->SocketId->SocketName);
            data.set_user_id(Options->LocalUserId->to_string());
        }
        break;

        default:
            // Unknown version — still try to read base fields rather than fail
            data.set_data(reinterpret_cast<const char*>(Options->Data), Options->DataLengthBytes);
            data.set_channel(Options->Channel);
            if (Options->SocketId != nullptr)
                data.set_socket_name(Options->SocketId->SocketName);
            if (Options->LocalUserId != nullptr)
                data.set_user_id(Options->LocalUserId->to_string());
            break;
    }

    switch(p2p_state.status)
    {
        case p2p_state_t::status_e::requesting:
        {
            // If auto-accept is disabled by the caller, we must not implicitly accept.
            if (!disable_auto_accept)
            {
                APP_LOG(Log::LogLevel::INFO, "Implicit P2P acceptation on send");
                set_p2p_state_connected(Options->RemoteUserId, p2p_state);
                // fall-through to connected case
            }
            else
            {
                // Queue the message; the app is expected to call AcceptConnection explicitly.
                if (!allow_delayed_delivery)
                    return EOS_EResult::EOS_P2P_ConnectionClosed; // can't send without accepting
                p2p_state.p2p_out_messages.emplace_back(std::move(data));
                break;
            }
        }
        // fall-through when auto-accept happened above

        case p2p_state_t::status_e::connected:
        {// We're connected, send the message now
            send_p2p_data(Options->RemoteUserId->to_string(), &data);
            if (mCachedOpcei != NULL) {
                set_p2p_state_connected(Options->RemoteUserId, p2p_state);
                mCachedOpcei = NULL;
            }
        }
        break;

        case p2p_state_t::status_e::connection_loss:
        case p2p_state_t::status_e::connecting:
        {
            if (allow_delayed_delivery)
            {
                // Save the message for later
                p2p_state.p2p_out_messages.emplace_back(std::move(data));
            }
            // If delayed delivery is disabled, silently discard (matches SDK behaviour).
        }
        break;

        case p2p_state_t::status_e::closed:
        {
            if (allow_delayed_delivery)
            {
                // Save the message for later, then start connecting
                p2p_state.p2p_out_messages.emplace_back(std::move(data));
            }

            p2p_state.status = p2p_state_t::status_e::connecting;
            p2p_state.socket_name = Options->SocketId->SocketName;
            p2p_state.connection_loss_start = std::chrono::steady_clock::now();

            P2P_Connect_Request_pb* req = new P2P_Connect_Request_pb;
            req->set_socket_name(p2p_state.socket_name);
            send_p2p_connection_request(Options->RemoteUserId->to_string(), req);
        }
        break;
    }

    return EOS_EResult::EOS_Success;
}

/**
 * Gets the size of the next available inbound packet.
 */
EOS_EResult EOSSDK_P2P::GetNextReceivedPacketSize(const EOS_P2P_GetNextReceivedPacketSizeOptions* Options, uint32_t* OutPacketSizeBytes)
{
    //TRACE_FUNC();
    std::lock_guard<std::recursive_mutex> lk(local_mutex);

    if (Options == nullptr || OutPacketSizeBytes == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    bool has_packet = false;
    if (Options->RequestedChannel == nullptr)
    {
        for (auto& in_msgs : _p2p_in_messages)
        {
            if (!in_msgs.second.empty())
            {
                auto& front = in_msgs.second.front();
                *OutPacketSizeBytes = static_cast<uint32_t>(front.data().length());
                next_requested_channel = front.channel();
                has_packet = true;
            }
        }
    }
    else
    {
        next_requested_channel = *Options->RequestedChannel;
        auto& in_msgs = _p2p_in_messages[next_requested_channel];
        if (!in_msgs.empty())
        {
            *OutPacketSizeBytes = static_cast<uint32_t>(in_msgs.front().data().length());
            has_packet = true;
        }
    }

    if (has_packet)
    {
        return EOS_EResult::EOS_Success;
    }
    
    *OutPacketSizeBytes = 0;
    return EOS_EResult::EOS_NotFound;
}

/**
 * Receive the next packet for the local user.
 */
EOS_EResult EOSSDK_P2P::ReceivePacket(const EOS_P2P_ReceivePacketOptions* Options, EOS_ProductUserId* OutPeerId, EOS_P2P_SocketId* OutSocketId, uint8_t* OutChannel, void* OutData, uint32_t* OutBytesWritten)
{
    std::lock_guard<std::recursive_mutex> lk(local_mutex);

    if (Options == nullptr || OutPeerId == nullptr || OutSocketId == nullptr ||
        OutChannel == nullptr || OutData == nullptr || OutBytesWritten == nullptr)
    {
        return EOS_EResult::EOS_InvalidParameters;
    }

    if (Options->RequestedChannel != nullptr)
        next_requested_channel = *Options->RequestedChannel;

    std::list<P2P_Data_Message_pb> *queue = nullptr;
    if (next_requested_channel == -1)
    {// No channel, get the next available message
        auto it = std::find_if(_p2p_in_messages.begin(), _p2p_in_messages.end(), []( std::pair<uint8_t const, std::list<P2P_Data_Message_pb>>& messages_queue)
        {
            return !messages_queue.second.empty();
        });
        if (it != _p2p_in_messages.end())
            queue = &it->second;
    }
    else
    {
        queue = &_p2p_in_messages[next_requested_channel];
        if (queue->empty())
        {
            queue = nullptr;
        }
    }
    if (queue == nullptr)
    {
        return EOS_EResult::EOS_NotFound;
    }

    auto& msg = queue->front();

    uint32_t msg_size = static_cast<uint32_t>(msg.data().size());

    *OutPeerId = GetProductUserId(msg.user_id());
    *OutBytesWritten = static_cast<uint32_t>(msg.data().copy(reinterpret_cast<char*>(OutData), Options->MaxDataSizeBytes));
    msg.socket_name().copy(OutSocketId->SocketName, sizeof(EOS_P2P_SocketId::SocketName));
    OutSocketId->SocketName[sizeof(EOS_P2P_SocketId::SocketName) - 1] = 0;
    *OutChannel = msg.channel();
    next_requested_channel = -1;

    // Update queue usage tracking
    if (_packet_queue_used_bytes >= msg_size)
        _packet_queue_used_bytes -= msg_size;
    else
        _packet_queue_used_bytes = 0;

    queue->pop_front();

    return EOS_EResult::EOS_Success;
}

/**
 * Listen for incoming connection requests on a particular Socket ID.
 */
EOS_NotificationId EOSSDK_P2P::AddNotifyPeerConnectionRequest(const EOS_P2P_AddNotifyPeerConnectionRequestOptions* Options, void* ClientData, EOS_P2P_OnIncomingConnectionRequestCallback ConnectionRequestHandler)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (ConnectionRequestHandler == nullptr)
        return EOS_INVALID_NOTIFICATIONID;

    pFrameResult_t res(new FrameResult);
    
    EOS_P2P_OnIncomingConnectionRequestInfo& oicri = res->CreateCallback<EOS_P2P_OnIncomingConnectionRequestInfo>((CallbackFunc)ConnectionRequestHandler);
    oicri.ClientData = ClientData;
    oicri.LocalUserId = Settings::Inst().productuserid;
    oicri.RemoteUserId = GetProductUserId(sdk::NULL_USER_ID);
    oicri.SocketId = new EOS_P2P_SocketId;

    return GetCB_Manager().add_notification(this, res);
}

void EOSSDK_P2P::RemoveNotifyPeerConnectionRequest(EOS_NotificationId NotificationId)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    GetCB_Manager().remove_notification(this, NotificationId);
}

EOS_NotificationId EOSSDK_P2P::AddNotifyPeerConnectionEstablished(const EOS_P2P_AddNotifyPeerConnectionEstablishedOptions* Options, void* ClientData, EOS_P2P_OnPeerConnectionEstablishedCallback ConnectionEstablishedHandler) {
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (ConnectionEstablishedHandler == nullptr)
        return EOS_INVALID_NOTIFICATIONID;

    pFrameResult_t res(new FrameResult);

    EOS_P2P_OnPeerConnectionEstablishedInfo& opcei = res->CreateCallback<EOS_P2P_OnPeerConnectionEstablishedInfo>((CallbackFunc)ConnectionEstablishedHandler);
    opcei.ClientData = ClientData;
    opcei.LocalUserId = Settings::Inst().productuserid;
    opcei.RemoteUserId = GetProductUserId(sdk::NULL_USER_ID);
    opcei.SocketId = new EOS_P2P_SocketId;
    opcei.ConnectionType = EOS_EConnectionEstablishedType::EOS_CET_Reconnection;
    opcei.NetworkType = EOS_ENetworkConnectionType::EOS_NCT_DirectConnection;

    if (mCachedOpcei != NULL) {
        opcei.RemoteUserId = mCachedOpcei->RemoteUserId;
        opcei.SocketId = mCachedOpcei->SocketId;
        opcei.ConnectionType = mCachedOpcei->ConnectionType;
        res->GetFunc()(res->GetFuncParam());
    }

    return GetCB_Manager().add_notification(this, res);
}

void EOSSDK_P2P::RemoveNotifyPeerConnectionEstablished(EOS_NotificationId NotificationId) {
    TRACE_FUNC();
    GLOBAL_LOCK();
    GetCB_Manager().remove_notification(this, NotificationId);
}

EOS_NotificationId EOSSDK_P2P::AddNotifyPeerConnectionInterrupted(const EOS_P2P_AddNotifyPeerConnectionInterruptedOptions* Options, void* ClientData, EOS_P2P_OnPeerConnectionInterruptedCallback ConnectionInterruptedHandler){
    TRACE_FUNC();
    GLOBAL_LOCK();
    if (ConnectionInterruptedHandler == nullptr)
        return EOS_INVALID_NOTIFICATIONID;

    pFrameResult_t res(new FrameResult);

    EOS_P2P_OnPeerConnectionInterruptedInfo& opcii = res->CreateCallback<EOS_P2P_OnPeerConnectionInterruptedInfo>((CallbackFunc)ConnectionInterruptedHandler);
    opcii.ClientData = ClientData;
    opcii.LocalUserId = Settings::Inst().productuserid;
    opcii.RemoteUserId = GetProductUserId(sdk::NULL_USER_ID);
    opcii.SocketId = new EOS_P2P_SocketId;

    return GetCB_Manager().add_notification(this, res);
}

void EOSSDK_P2P::RemoveNotifyPeerConnectionInterrupted(EOS_NotificationId NotificationId)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    GetCB_Manager().remove_notification(this, NotificationId);
}

EOS_NotificationId EOSSDK_P2P::AddNotifyPeerConnectionClosed(const EOS_P2P_AddNotifyPeerConnectionClosedOptions* Options, void* ClientData, EOS_P2P_OnRemoteConnectionClosedCallback ConnectionClosedHandler)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (ConnectionClosedHandler == nullptr)
        return EOS_INVALID_NOTIFICATIONID;

    pFrameResult_t res(new FrameResult);

    EOS_P2P_OnRemoteConnectionClosedInfo& orcci = res->CreateCallback<EOS_P2P_OnRemoteConnectionClosedInfo>((CallbackFunc)ConnectionClosedHandler);
    orcci.ClientData = ClientData;
    orcci.LocalUserId = Settings::Inst().productuserid;
    orcci.RemoteUserId = GetProductUserId(sdk::NULL_USER_ID);
    orcci.SocketId = new EOS_P2P_SocketId;
    orcci.Reason = EOS_EConnectionClosedReason::EOS_CCR_Unknown;

    return GetCB_Manager().add_notification(this, res);
}

void EOSSDK_P2P::RemoveNotifyPeerConnectionClosed(EOS_NotificationId NotificationId)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    GetCB_Manager().remove_notification(this, NotificationId);
}

EOS_EResult EOSSDK_P2P::AcceptConnection(const EOS_P2P_AcceptConnectionOptions* Options)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr || Options->RemoteUserId == nullptr || Options->SocketId == nullptr)
        return EOS_EResult::EOS_InvalidParameters;
    
    auto& conn = _p2p_connections[Options->RemoteUserId];

    if (conn.status == p2p_state_t::status_e::requesting)
    {
        P2P_Connect_Response_pb* resp = new P2P_Connect_Response_pb;
        resp->set_accepted(true);
        send_p2p_connection_response(Options->RemoteUserId->to_string(), resp);
    }
    
    set_p2p_state_connected(Options->RemoteUserId, conn);
    conn.connection_loss_start = {};
    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_P2P::CloseConnection(const EOS_P2P_CloseConnectionOptions* Options)
{
    TRACE_FUNC();
    APP_LOG(Log::LogLevel::DEBUG, "TODO");
    GLOBAL_LOCK();
    
    if (Options == nullptr || Options->RemoteUserId == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    if (Options->SocketId == nullptr)
    {
        auto& conn = _p2p_connections[Options->RemoteUserId];
        conn.p2p_out_messages.clear();
        for (auto& in_msgs : _p2p_in_messages)
        {
            in_msgs.second.erase(std::remove_if(in_msgs.second.begin(), in_msgs.second.end(), [&Options](P2P_Data_Message_pb& msg)
            {
                return msg.user_id() == Options->RemoteUserId->to_string();
            }), in_msgs.second.end());
        }

        if (conn.status != p2p_state_t::status_e::closed)
        {
            conn.status = p2p_state_t::status_e::closed;

            P2P_Connection_Close_pb* close = new P2P_Connection_Close_pb;
            send_p2p_connetion_close(Options->RemoteUserId->to_string(), close);
        }
    }
    else
    {
        std::string target_sock_name = Options->SocketId->SocketName;
        auto& conn = _p2p_connections[Options->RemoteUserId];
        conn.p2p_out_messages.clear();
        for (auto& in_msgs : _p2p_in_messages)
        {
            in_msgs.second.erase(std::remove_if(in_msgs.second.begin(), in_msgs.second.end(), [&Options](P2P_Data_Message_pb& msg)
            {
                return msg.user_id() == Options->RemoteUserId->to_string();
            }), in_msgs.second.end());
        }

        if (conn.status != p2p_state_t::status_e::closed)
        {
            if (conn.socket_name == target_sock_name)
            {
                conn.status = p2p_state_t::status_e::closed;

                P2P_Connection_Close_pb* close = new P2P_Connection_Close_pb;
                send_p2p_connetion_close(Options->RemoteUserId->to_string(), close);
            }
        }
    }
    
    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_P2P::CloseConnections(const EOS_P2P_CloseConnectionsOptions* Options)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr || Options->SocketId == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    std::string target_sock_name = Options->SocketId->SocketName;
    for (auto& conn : _p2p_connections)
    {
        if (conn.second.socket_name == target_sock_name)
        {
            conn.second.status = p2p_state_t::status_e::closed;

            P2P_Connection_Close_pb* close = new P2P_Connection_Close_pb;
            send_p2p_connetion_close(conn.first->to_string(), close);
        }
    }
    
    return EOS_EResult::EOS_Success;
}

void EOSSDK_P2P::QueryNATType(const EOS_P2P_QueryNATTypeOptions* Options, void* ClientData, const EOS_P2P_OnQueryNATTypeCompleteCallback NATTypeQueriedHandler)
{
    TRACE_FUNC();
    APP_LOG(Log::LogLevel::DEBUG, "TODO");
    GLOBAL_LOCK();

    if (NATTypeQueriedHandler == nullptr)
        return;

    pFrameResult_t res(new FrameResult);
    EOS_P2P_OnQueryNATTypeCompleteInfo& pqntci = res->CreateCallback<EOS_P2P_OnQueryNATTypeCompleteInfo>((CallbackFunc)NATTypeQueriedHandler, std::chrono::milliseconds(5000));
    pqntci.ClientData = ClientData;

    if (Options == nullptr)
    {
        pqntci.NATType = EOS_ENATType::EOS_NAT_Unknown;
        pqntci.ResultCode = EOS_EResult::EOS_InvalidParameters;
    }
    else
    {
        pqntci.NATType = EOS_ENATType::EOS_NAT_Open;
        pqntci.ResultCode = EOS_EResult::EOS_Success;
    }
    
    res->done = true;
    GetCB_Manager().add_callback(this, res);
}

EOS_EResult EOSSDK_P2P::GetNATType(const EOS_P2P_GetNATTypeOptions* Options, EOS_ENATType* OutNATType)
{
    TRACE_FUNC();
    APP_LOG(Log::LogLevel::DEBUG, "TODO");
    GLOBAL_LOCK();

    *OutNATType = EOS_ENATType::EOS_NAT_Moderate;
    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_P2P::SetRelayControl(const EOS_P2P_SetRelayControlOptions* Options)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if(Options == nullptr)
        return EOS_EResult::EOS_InvalidParameters;
    
    _relay_control = Options->RelayControl;
    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_P2P::GetRelayControl(const EOS_P2P_GetRelayControlOptions* Options, EOS_ERelayControl* OutRelayControl)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr || OutRelayControl == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    *OutRelayControl = _relay_control;
    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_P2P::SetPortRange(const EOS_P2P_SetPortRangeOptions* Options)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr || Options->Port <= 1024)
        return EOS_EResult::EOS_InvalidParameters;

    _p2p_port = Options->Port;
    _max_additional_ports_to_try = Options->MaxAdditionalPortsToTry;
    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_P2P::GetPortRange(const EOS_P2P_GetPortRangeOptions* Options, uint16_t* OutPort, uint16_t* OutNumAdditionalPortsToTry)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr || OutPort == nullptr || OutNumAdditionalPortsToTry == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    *OutPort = _p2p_port;
    *OutNumAdditionalPortsToTry = _max_additional_ports_to_try;
    return EOS_EResult::EOS_Success;
}

// ---------------------------------------------------------------------------
// SDK 1.13+ Packet Queue Management
// ---------------------------------------------------------------------------

/**
 * Set the maximum byte size of the inbound packet queue.
 * Packets received while the queue is full are dropped and the
 * AddNotifyIncomingPacketQueueFull callback fires.
 *
 * @param Options  Must contain a valid PacketQueueMaxSizeBytes (>= 4096).
 */
EOS_EResult EOSSDK_P2P::SetPacketQueueSize(const EOS_P2P_SetPacketQueueSizeOptions* Options)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    // SDK enforces a minimum of 4 KB
    if (Options->PacketQueueMaxSizeBytes < 4096)
        return EOS_EResult::EOS_InvalidParameters;

    _packet_queue_size_bytes = Options->PacketQueueMaxSizeBytes;
    APP_LOG(Log::LogLevel::INFO, "P2P packet queue size set to %llu bytes", (unsigned long long)_packet_queue_size_bytes);
    return EOS_EResult::EOS_Success;
}

/**
 * Get current packet queue metrics.
 */
EOS_EResult EOSSDK_P2P::GetPacketQueueInfo(const EOS_P2P_GetPacketQueueInfoOptions* Options, EOS_P2P_PacketQueueInfo* OutPacketQueueInfo)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr || OutPacketQueueInfo == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    // Compute current inbound queue byte usage from the in-message map
    uint64_t used = 0;
    for (auto& channel_queue : _p2p_in_messages)
    {
        for (auto& msg : channel_queue.second)
        {
            used += static_cast<uint64_t>(msg.data().size());
        }
    }
    _packet_queue_used_bytes = used;

    OutPacketQueueInfo->IncomingPacketQueueMaxSizeBytes  = _packet_queue_size_bytes;
    OutPacketQueueInfo->IncomingPacketQueueCurrentSizeBytes = _packet_queue_used_bytes;
    OutPacketQueueInfo->IncomingPacketQueueCurrentPacketCount = 0;
    for (auto& ch : _p2p_in_messages)
        OutPacketQueueInfo->IncomingPacketQueueCurrentPacketCount += static_cast<uint64_t>(ch.second.size());

    // Outbound queue is not bounded in this emulator — report as zero used / max capacity.
    OutPacketQueueInfo->OutgoingPacketQueueMaxSizeBytes          = _packet_queue_size_bytes;
    OutPacketQueueInfo->OutgoingPacketQueueCurrentSizeBytes      = 0;
    OutPacketQueueInfo->OutgoingPacketQueueCurrentPacketCount    = 0;

    return EOS_EResult::EOS_Success;
}

/**
 * Register a notification callback for when the inbound packet queue is full
 * and an incoming packet had to be dropped.
 */
EOS_NotificationId EOSSDK_P2P::AddNotifyIncomingPacketQueueFull(
    const EOS_P2P_AddNotifyIncomingPacketQueueFullOptions* Options,
    void* ClientData,
    EOS_P2P_OnIncomingPacketQueueFullCallback PacketQueueFullHandler)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (PacketQueueFullHandler == nullptr)
        return EOS_INVALID_NOTIFICATIONID;

    pFrameResult_t res(new FrameResult);
    EOS_P2P_OnIncomingPacketQueueFullInfo& info = res->CreateCallback<EOS_P2P_OnIncomingPacketQueueFullInfo>((CallbackFunc)PacketQueueFullHandler);
    info.ClientData                            = ClientData;
    info.PacketQueueMaxSizeBytes               = _packet_queue_size_bytes;
    info.PacketQueueCurrentSizeBytes           = _packet_queue_used_bytes;
    info.OverflowPacketLocalUserId             = Settings::Inst().productuserid;
    info.OverflowPacketChannel                 = 0;
    info.OverflowPacketSizeBytes               = 0;

    return GetCB_Manager().add_notification(this, res);
}

void EOSSDK_P2P::RemoveNotifyIncomingPacketQueueFull(EOS_NotificationId NotificationId)
{
    TRACE_FUNC();
    GLOBAL_LOCK();
    GetCB_Manager().remove_notification(this, NotificationId);
}

///////////////////////////////////////////////////////////////////////////////
//                           Network Send messages                           //
///////////////////////////////////////////////////////////////////////////////
bool EOSSDK_P2P::send_p2p_connection_request(Network::peer_t const& peerid, P2P_Connect_Request_pb* req) const
{
    TRACE_FUNC();
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    P2P_Message_pb* p2p = new P2P_Message_pb;

    p2p->set_allocated_connect_request(req);

    msg.set_source_id(user_id);
    msg.set_dest_id(peerid);
    msg.set_game_id(Settings::Inst().appid);

    msg.set_allocated_p2p(p2p);

    return GetNetwork().TCPSendTo(msg);
}

bool EOSSDK_P2P::send_p2p_connection_response(Network::peer_t const& peerid, P2P_Connect_Response_pb* resp) const
{
    TRACE_FUNC();
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    P2P_Message_pb* p2p = new P2P_Message_pb;

    p2p->set_allocated_connect_response(resp);

    msg.set_source_id(user_id);
    msg.set_dest_id(peerid);
    msg.set_game_id(Settings::Inst().appid);

    msg.set_allocated_p2p(p2p);

    return GetNetwork().TCPSendTo(msg);
}

bool EOSSDK_P2P::send_p2p_data(Network::peer_t const& peerid, P2P_Data_Message_pb* data) const
{
    //TRACE_FUNC();
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    P2P_Message_pb* p2p = new P2P_Message_pb;

    p2p->set_allocated_data_message(data);

    msg.set_source_id(user_id);
    msg.set_dest_id(peerid);
    msg.set_game_id(Settings::Inst().appid);

    msg.set_allocated_p2p(p2p);
    auto res = GetNetwork().UDPSendTo(msg);

    p2p->release_data_message();

    return res;
}

bool EOSSDK_P2P::send_p2p_data_ack(Network::peer_t const& peerid, P2P_Data_Acknowledge_pb* ack) const
{
    //TRACE_FUNC();
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    P2P_Message_pb* p2p = new P2P_Message_pb;

    p2p->set_allocated_data_acknowledge(ack);

    msg.set_source_id(user_id);
    msg.set_dest_id(peerid);
    msg.set_game_id(Settings::Inst().appid);

    msg.set_allocated_p2p(p2p);

    return GetNetwork().UDPSendTo(msg);
}

bool EOSSDK_P2P::send_p2p_connetion_close(Network::peer_t const& peerid, P2P_Connection_Close_pb* close) const
{
    //TRACE_FUNC();
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    P2P_Message_pb* p2p = new P2P_Message_pb;

    p2p->set_allocated_connection_close(close);

    msg.set_source_id(user_id);
    msg.set_dest_id(peerid);
    msg.set_game_id(Settings::Inst().appid);

    msg.set_allocated_p2p(p2p);

    return GetNetwork().TCPSendTo(msg);
}

///////////////////////////////////////////////////////////////////////////////
//                          Network Receive messages                         //
///////////////////////////////////////////////////////////////////////////////
bool EOSSDK_P2P::on_peer_connect(Network_Message_pb const& msg, Network_Peer_Connect_pb const& peer)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    auto peer_id = GetProductUserId(msg.source_id());
    auto it = _p2p_connections.find(peer_id);
    if (it != _p2p_connections.end() && it->second.status == p2p_state_t::status_e::connection_loss)
    {
        it->second.status = p2p_state_t::status_e::connected;

        for (auto& m : it->second.p2p_out_messages)
        {
            send_p2p_data(it->first->to_string(), &m);
        }
        it->second.p2p_out_messages.clear();
    }

    return true;
}

bool EOSSDK_P2P::on_peer_disconnect(Network_Message_pb const& msg, Network_Peer_Disconnect_pb const& peer)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    auto peer_id = GetProductUserId(msg.source_id());
    auto it = _p2p_connections.find(peer_id);
    if (it != _p2p_connections.end())
    {
        it->second.connection_loss_start = std::chrono::steady_clock::now();
    }

    std::vector<pFrameResult_t> notifs = std::move(GetCB_Manager().get_notifications(this, EOS_P2P_OnPeerConnectionInterruptedInfo::k_iCallback));
    for (auto& notif : notifs)
    {
        EOS_P2P_OnPeerConnectionInterruptedInfo& opcii = notif->GetCallback<EOS_P2P_OnPeerConnectionInterruptedInfo>();
        opcii.LocalUserId = Settings::Inst().productuserid;
        opcii.RemoteUserId = peer_id;
        notif->GetFunc()(notif->GetFuncParam());
    }

    return true;
}

bool EOSSDK_P2P::on_p2p_connection_request(Network_Message_pb const& msg, P2P_Connect_Request_pb const& req)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    auto peer_id = GetProductUserId(msg.source_id());
    auto& conn = _p2p_connections[peer_id];
    if (conn.status != p2p_state_t::status_e::connected)
    {
        conn.status = p2p_state_t::status_e::requesting;
        conn.connection_loss_start = std::chrono::steady_clock::now();
        conn.socket_name = req.socket_name();
        std::vector<pFrameResult_t> notifs = std::move(GetCB_Manager().get_notifications(this, EOS_P2P_OnIncomingConnectionRequestInfo::k_iCallback));
        for (auto& notif : notifs)
        {
            EOS_P2P_OnIncomingConnectionRequestInfo& oicrc = notif->GetCallback<EOS_P2P_OnIncomingConnectionRequestInfo>();
            oicrc.RemoteUserId = peer_id;
            strncpy(const_cast<char*>(oicrc.SocketId->SocketName), req.socket_name().c_str(), sizeof(EOS_P2P_SocketId::SocketName));

            notif->GetFunc()(notif->GetFuncParam());
        }
    }
    else
    {
        P2P_Connect_Response_pb* resp = new P2P_Connect_Response_pb;
        resp->set_accepted(true);
        send_p2p_connection_response(msg.source_id(), resp);
    }

    return true;
}

bool EOSSDK_P2P::on_p2p_connection_response(Network_Message_pb const& msg, P2P_Connect_Response_pb const& resp)
{
    TRACE_FUNC();
    GLOBAL_LOCK();
    
    EOS_ProductUserId remote_id = GetProductUserId(msg.source_id());
    if (resp.accepted())
    {
        set_p2p_state_connected(remote_id, _p2p_connections[remote_id]);
    }
    else
    {
        std::vector<pFrameResult_t> notifs = std::move(GetCB_Manager().get_notifications(this, EOS_P2P_OnRemoteConnectionClosedInfo::k_iCallback));
        for (auto& notif : notifs)
        {
            EOS_P2P_OnRemoteConnectionClosedInfo& orcci = notif->GetCallback<EOS_P2P_OnRemoteConnectionClosedInfo>();
            orcci.Reason = EOS_EConnectionClosedReason::EOS_CCR_ClosedByPeer;
            orcci.RemoteUserId = remote_id;

            notif->GetFunc()(notif->GetFuncParam());
        }
    }

    return true;
}

bool EOSSDK_P2P::on_p2p_data(Network_Message_pb const& msg, P2P_Data_Message_pb const& data)
{
    //TRACE_FUNC();
    std::lock_guard<std::recursive_mutex> lk(local_mutex);

    EOS_ProductUserId remote_id = GetProductUserId(msg.source_id());
    auto& p2p_state = _p2p_connections[remote_id];

    P2P_Data_Acknowledge_pb* ack = new P2P_Data_Acknowledge_pb;

    switch (p2p_state.status)
    {
        case p2p_state_t::status_e::connecting:
        {
            APP_LOG(Log::LogLevel::INFO, "Implicit P2P acceptation on receive");
            set_p2p_state_connected(remote_id, p2p_state);
        }
        // fall-through

        case p2p_state_t::status_e::connected:
        {
            uint64_t pkt_size = static_cast<uint64_t>(data.data().size());

            // Check if adding this packet would overflow the queue
            if ((_packet_queue_used_bytes + pkt_size) > _packet_queue_size_bytes)
            {
                // Drop packet and fire queue-full notifications
                ++_packet_queue_dropped_packets;
                APP_LOG(Log::LogLevel::WARN, "Inbound P2P packet queue full, dropping packet (%llu bytes)", (unsigned long long)pkt_size);

                std::vector<pFrameResult_t> notifs = std::move(GetCB_Manager().get_notifications(this, EOS_P2P_OnIncomingPacketQueueFullInfo::k_iCallback));
                for (auto& notif : notifs)
                {
                    EOS_P2P_OnIncomingPacketQueueFullInfo& info = notif->GetCallback<EOS_P2P_OnIncomingPacketQueueFullInfo>();
                    info.PacketQueueMaxSizeBytes       = _packet_queue_size_bytes;
                    info.PacketQueueCurrentSizeBytes   = _packet_queue_used_bytes;
                    info.OverflowPacketLocalUserId     = Settings::Inst().productuserid;
                    info.OverflowPacketChannel         = static_cast<uint8_t>(data.channel());
                    info.OverflowPacketSizeBytes       = static_cast<uint32_t>(pkt_size);
                    notif->GetFunc()(notif->GetFuncParam());
                }

                ack->set_channel(data.channel());
                ack->set_accepted(false);
                return send_p2p_data_ack(msg.source_id(), ack);
            }

            ack->set_channel(data.channel());
            ack->set_accepted(true);
            _packet_queue_used_bytes += pkt_size;
            _p2p_in_messages[data.channel()].emplace_back(data);
        }
        break;

        default:
            ack->set_accepted(false);
    }

    return send_p2p_data_ack(msg.source_id(), ack);
}

bool EOSSDK_P2P::on_p2p_data_ack(Network_Message_pb const& msg, P2P_Data_Acknowledge_pb const& ack)
{
    //TRACE_FUNC();
    GLOBAL_LOCK();

    return true;
}

bool EOSSDK_P2P::on_p2p_connection_close(Network_Message_pb const& msg, P2P_Connection_Close_pb const& close)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    std::vector<pFrameResult_t> notifs = std::move(GetCB_Manager().get_notifications(this, EOS_P2P_OnRemoteConnectionClosedInfo::k_iCallback));
    for (auto& notif : notifs)
    {
        EOS_P2P_OnRemoteConnectionClosedInfo& orcci = notif->GetCallback<EOS_P2P_OnRemoteConnectionClosedInfo>();
        orcci.Reason = EOS_EConnectionClosedReason::EOS_CCR_ClosedByPeer;
        orcci.RemoteUserId = GetProductUserId(msg.source_id());

        notif->GetFunc()(notif->GetFuncParam());
    }

    _p2p_connections[GetProductUserId(msg.source_id())].status = p2p_state_t::status_e::closed;

    return true;
}

///////////////////////////////////////////////////////////////////////////////
//                                 IRunFrame                                 //
///////////////////////////////////////////////////////////////////////////////
bool EOSSDK_P2P::CBRunFrame()
{
    GLOBAL_LOCK();

    for (auto it = _p2p_connections.begin(); it != _p2p_connections.end(); ++it)
    {
        switch(it->second.status)
        {
            case p2p_state_t::status_e::requesting:
            {
                auto now = std::chrono::steady_clock::now();
                if ((now - it->second.connection_loss_start) > connecting_timeout)
                {// We didn't accept the connection
                    it->second.status = p2p_state_t::status_e::closed;
                    it->second.p2p_out_messages.clear();
                }
            }
            break;

            case p2p_state_t::status_e::connecting:
            {
                auto now = std::chrono::steady_clock::now();
                if ((now - it->second.connection_loss_start) > connecting_timeout)
                {
                    it->second.status = p2p_state_t::status_e::closed;
                    it->second.p2p_out_messages.clear();

                    std::vector<pFrameResult_t> notifs(std::move(GetCB_Manager().get_notifications(this, EOS_P2P_OnRemoteConnectionClosedInfo::k_iCallback)));
                    for (auto& notif : notifs)
                    {
                        EOS_P2P_OnRemoteConnectionClosedInfo& orcci = notif->GetCallback<EOS_P2P_OnRemoteConnectionClosedInfo>();
                        orcci.RemoteUserId = it->first;
                        strncpy(const_cast<char*>(orcci.SocketId->SocketName), it->second.socket_name.c_str(), sizeof(orcci.SocketId->SocketName));
                        const_cast<char*>(orcci.SocketId->SocketName)[sizeof(orcci.SocketId->SocketName) - 1] = '\0';
                        orcci.Reason = EOS_EConnectionClosedReason::EOS_CCR_ConnectionFailed;
                    }
                }
            }
            break;

            case p2p_state_t::status_e::connection_loss:
            {
                auto now = std::chrono::steady_clock::now();
                if ((now - it->second.connection_loss_start) > connection_timeout)
                {
                    it->second.status = p2p_state_t::status_e::closed;
                    it->second.p2p_out_messages.clear();

                    std::vector<pFrameResult_t> notifs(std::move(GetCB_Manager().get_notifications(this, EOS_P2P_OnRemoteConnectionClosedInfo::k_iCallback)));
                    for (auto& notif : notifs)
                    {
                        EOS_P2P_OnRemoteConnectionClosedInfo& orcci = notif->GetCallback<EOS_P2P_OnRemoteConnectionClosedInfo>();
                        orcci.RemoteUserId = it->first;
                        strncpy(const_cast<char*>(orcci.SocketId->SocketName), it->second.socket_name.c_str(), sizeof(orcci.SocketId->SocketName));
                        const_cast<char*>(orcci.SocketId->SocketName)[sizeof(orcci.SocketId->SocketName) - 1] = '\0';
                        orcci.Reason = EOS_EConnectionClosedReason::EOS_CCR_TimedOut;
                    }
                }
            }
            break;

            default:
                break;
        }
    }

    return true;
}

bool EOSSDK_P2P::RunNetwork(Network_Message_pb const& msg)
{
    switch (msg.messages_case())
    {
        case Network_Message_pb::MessagesCase::kP2P:
        {
            P2P_Message_pb const& p2p = msg.p2p();
            switch (p2p.message_case())
            {
                case P2P_Message_pb::MessageCase::kConnectRequest : return on_p2p_connection_request(msg, p2p.connect_request());
                case P2P_Message_pb::MessageCase::kConnectResponse: return on_p2p_connection_response(msg, p2p.connect_response());
                case P2P_Message_pb::MessageCase::kDataMessage    : return on_p2p_data(msg, p2p.data_message());
                case P2P_Message_pb::MessageCase::kDataAcknowledge: return on_p2p_data_ack(msg, p2p.data_acknowledge());
                case P2P_Message_pb::MessageCase::kConnectionClose: return on_p2p_connection_close(msg, p2p.connection_close());
                default: APP_LOG(Log::LogLevel::WARN, "Unhandled network message %d", p2p.message_case());
            }
        }
        break;
        default:
            break;
    }

    return true;
}

bool EOSSDK_P2P::RunCallbacks(pFrameResult_t res)
{
    GLOBAL_LOCK();

    return res->done;
}

void EOSSDK_P2P::FreeCallback(pFrameResult_t res)
{
    GLOBAL_LOCK();

    switch (res->ICallback())
    {
        case EOS_P2P_OnIncomingConnectionRequestInfo::k_iCallback:
        {
            EOS_P2P_OnIncomingConnectionRequestInfo& callback = res->GetCallback<EOS_P2P_OnIncomingConnectionRequestInfo>();
            delete callback.SocketId;
        }
        break;
        case EOS_P2P_OnRemoteConnectionClosedInfo::k_iCallback:
        {
            EOS_P2P_OnRemoteConnectionClosedInfo& callback = res->GetCallback<EOS_P2P_OnRemoteConnectionClosedInfo>();
            delete callback.SocketId;
        }
        break;
        // EOS_P2P_OnIncomingPacketQueueFullInfo owns no heap pointers — nothing to free
        default:
            break;
    }
}

}
