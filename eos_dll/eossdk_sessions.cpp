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

#include "eossdk_sessions.h"
#include "eossdk_platform.h"
#include "eos_client_api.h"
#include "settings.h"
#include "os_funcs.h"

namespace sdk
{

decltype(EOSSDK_Sessions::join_timeout) EOSSDK_Sessions::join_timeout;

EOSSDK_Sessions::EOSSDK_Sessions()
{
    GetNetwork().register_listener(this, 0, Network_Message_pb::MessagesCase::kSession);
    GetNetwork().register_listener(this, 0, Network_Message_pb::MessagesCase::kSessionsSearch);

    GetCB_Manager().register_callbacks(this);

    GetCB_Manager().register_frame(this);
    
}

EOSSDK_Sessions::~EOSSDK_Sessions()
{
    GetCB_Manager().unregister_frame(this);

    GetCB_Manager().unregister_callbacks(this);

    GetNetwork().unregister_listener(this, 0, Network_Message_pb::MessagesCase::kSessionsSearch);
    GetNetwork().unregister_listener(this, 0, Network_Message_pb::MessagesCase::kSession);

    GetCB_Manager().remove_all_notifications(this);
}

template<typename T>
bool compare_attribute_values(T&& v1, EOS_EOnlineComparisonOp op, T&& v2, std::string const& attr_name)
{
    bool res = false;
    try
    {
        switch (op)
        {
            case EOS_EOnlineComparisonOp::EOS_CO_EQUAL             : res = v1 == v2; break;
            case EOS_EOnlineComparisonOp::EOS_CO_NOTEQUAL          : res = v1 != v2; break;
            case EOS_EOnlineComparisonOp::EOS_CO_GREATERTHAN       : res = v1 >  v2; break;
            case EOS_EOnlineComparisonOp::EOS_CO_GREATERTHANOREQUAL: res = v1 >= v2; break;
            case EOS_EOnlineComparisonOp::EOS_CO_LESSTHAN          : res = v1 <  v2; break;
            case EOS_EOnlineComparisonOp::EOS_CO_LESSTHANOREQUAL   : res = v1 <= v2; break;
            default: res = true;
        }
    }
    catch (...)
    {}

    return res;
}

session_state_t* EOSSDK_Sessions::get_session_by_id(std::string const& session_id)
{
    auto it = std::find_if(_sessions.begin(), _sessions.end(), [&session_id]( std::pair<std::string const, session_state_t>& infos)
    {
        return session_id == infos.second.infos.session_id();
    });
    if (it == _sessions.end())
        return nullptr;

    return &it->second;
}

session_state_t* EOSSDK_Sessions::get_session_by_name(std::string const& session_name)
{
    auto it = _sessions.find(session_name);
    if (it == _sessions.end())
        return nullptr;

    return &it->second;
}

bool EOSSDK_Sessions::session_match_from_attributes(session_state_t* session, google::protobuf::Map<std::string, Session_Search_Parameter> const& parameters)
{
    for (auto& param : parameters)
    {
        if (param.first == "bucket")
        {
            auto& comparison = *param.second.param().begin();
            EOS_EOnlineComparisonOp comp = static_cast<EOS_EOnlineComparisonOp>(comparison.first);

            switch (comparison.second.value_case())
            {
                case Session_Attr_Value::ValueCase::kS:
                {
                    std::string const& s_session = session->infos.bucket_id();
                    std::string const& s_search = comparison.second.s();
                    if (!compare_attribute_values(s_session, comp, s_search, param.first))
                        return false;
                }
                break;
                default: return false;
            }
        }
        else
        {
            auto it = session->infos.attributes().find(param.first);
            if (it == session->infos.attributes().end())
            {
                return false;
            }
            else
            {
                for (auto& comparisons : param.second.param())
                {
                    if (comparisons.second.value_case() != it->second.value().value_case())
                        return false;

                    EOS_EOnlineComparisonOp comp = static_cast<EOS_EOnlineComparisonOp>(comparisons.first);

                    switch (comparisons.second.value_case())
                    {
                        case Session_Attr_Value::ValueCase::kB:
                        {
                            bool b_session = it->second.value().b();
                            bool b_search = comparisons.second.b();
                            if (!compare_attribute_values(b_session, comp, b_search, param.first))
                                return false;
                        }
                        break;
                        case Session_Attr_Value::ValueCase::kI:
                        {
                            int64_t i_session = it->second.value().i();
                            int64_t i_search = comparisons.second.i();
                            if (!compare_attribute_values(i_session, comp, i_search, param.first))
                                return false;
                        }
                        break;
                        case Session_Attr_Value::ValueCase::kD:
                        {
                            double d_session = it->second.value().d();
                            double d_search = comparisons.second.d();
                            if (!compare_attribute_values(d_session, comp, d_search, param.first))
                                return false;
                        }
                        break;
                        case Session_Attr_Value::ValueCase::kS:
                        {
                            std::string const& s_session = it->second.value().s();
                            std::string const& s_search = comparisons.second.s();
                            if (!compare_attribute_values(s_session, comp, s_search, param.first))
                                return false;
                        }
                    }
                }
            }
        }
    }

    return true;
}

std::vector<session_state_t*> EOSSDK_Sessions::get_sessions_from_attributes(google::protobuf::Map<std::string, Session_Search_Parameter> const& parameters)
{
    std::vector<session_state_t*> res;
    for (auto& session : _sessions)
    {
        bool found = session_match_from_attributes(&session.second, parameters);
        if (found)
            res.emplace_back(&session.second);
        else
            APP_LOG(Log::LogLevel::DEBUG, "This session didn't match: %s(%s)", session.second.infos.session_id().c_str(), session.first.c_str());
    }
    return res;
}

void EOSSDK_Sessions::add_player_to_session(std::string const& player, session_state_t* session)
{
    if (session != nullptr)
    {
        if(!is_player_in_session(player, session))
            *session->infos.add_players() = player;
    }
}

void EOSSDK_Sessions::remove_player_from_session(std::string const& player, session_state_t* session)
{
    if (session != nullptr)
    {
        auto it = std::find(session->infos.players().begin(), session->infos.players().end(), player);
        if (it != session->infos.players().end())
            session->infos.mutable_players()->erase(it);

        it = std::find(session->infos.registered_players().begin(), session->infos.registered_players().end(), player);
        if (it != session->infos.registered_players().end())
            session->infos.mutable_registered_players()->erase(it);
    }
}

bool EOSSDK_Sessions::register_player_to_session(std::string const& player, session_state_t* session)
{
    if (session != nullptr && !is_player_registered(player, session))
    {
        *session->infos.add_registered_players() = player;
        return true;
    }
    return false;
}

bool EOSSDK_Sessions::unregister_player_from_session(std::string const& player, session_state_t* session)
{
    if (session != nullptr)
    {
        auto it = std::find(session->infos.registered_players().begin(), session->infos.registered_players().end(), player);
        if (it != session->infos.registered_players().end())
        {
            session->infos.mutable_registered_players()->erase(it);
            return true;
        }
    }
    return false;
}

bool EOSSDK_Sessions::is_player_in_session(std::string const& player, session_state_t* session)
{
    if (session != nullptr)
    {
        auto it = std::find(session->infos.players().begin(), session->infos.players().end(), player);
        return it != session->infos.players().end();
    }
    return false;
}

bool EOSSDK_Sessions::is_player_registered(std::string const& player, session_state_t* session)
{
    if (session != nullptr)
    {
        auto it = std::find(session->infos.registered_players().begin(), session->infos.registered_players().end(), player);
        return it != session->infos.registered_players().end();
    }
    return false;
}

// ---------------------------------------------------------------------------
// FIX 1: Detect real local IP (works with Hamachi/ZeroTier/any VPN adapter)
// The custom_broadcast field in JSON can override with a specific IP if needed.
// ---------------------------------------------------------------------------
static std::string get_best_host_address()
{
    // If user set custom_broadcast in JSON, use that directly
    std::string const& custom = Settings::Inst().custom_broadcast;
    if (!custom.empty() && custom != "0.0.0.0")
    {
        APP_LOG(Log::LogLevel::INFO, "Using custom_broadcast as host address: %s", custom.c_str());
        return custom;
    }

    // Try to get a non-loopback local IP from all interfaces
    std::string best_ip;
    auto ips = get_local_ips(false); // false = skip loopback
    for (auto const& ip : ips)
    {
        if (ip.empty() || ip == "127.0.0.1")
            continue;
        // Prefer VPN ranges: 25.x (Hamachi), 10.x (ZeroTier/corporate VPN), 192.168.x
        if (ip.substr(0, 3) == "25." ||
            ip.substr(0, 3) == "10." ||
            ip.substr(0, 8) == "192.168." ||
            ip.substr(0, 8) == "172.16."  ||
            ip.substr(0, 8) == "172.17."  ||
            ip.substr(0, 8) == "172.31.")
        {
            best_ip = ip;
            break;
        }
    }

    if (best_ip.empty() && !ips.empty())
        best_ip = ips[0]; // fallback: first available

    if (best_ip.empty())
        best_ip = "127.0.0.1"; // last resort

    APP_LOG(Log::LogLevel::INFO, "Auto-detected host address: %s", best_ip.c_str());
    return best_ip;
}

EOS_EResult EOSSDK_Sessions::CreateSessionModification(const EOS_Sessions_CreateSessionModificationOptions* Options, EOS_HSessionModification* OutSessionModificationHandle)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr || Options->SessionName == nullptr || Options->BucketId == nullptr || OutSessionModificationHandle == nullptr)
    {
        set_nullptr(OutSessionModificationHandle);
        return EOS_EResult::EOS_InvalidParameters;
    }

    EOSSDK_SessionModification* modif = new EOSSDK_SessionModification;
    modif->_api_version = Options->ApiVersion;
    modif->_type = EOSSDK_SessionModification::modif_type::creation;
    // FIX 1 applied here: use real IP instead of hardcoded 127.0.0.1
    modif->_infos.set_host_address(get_best_host_address());

    switch (Options->ApiVersion)
    {
        case EOS_SESSIONS_CREATESESSIONMODIFICATION_API_003:
        {
            const EOS_Sessions_CreateSessionModificationOptions003* opts = reinterpret_cast<const EOS_Sessions_CreateSessionModificationOptions003*>(Options);
            if (opts->SessionId != nullptr)
            {
                APP_LOG(Log::LogLevel::DEBUG, "Overriding session id: %s", modif->_infos.session_id().c_str());
                modif->_infos.set_session_id(opts->SessionId);
                if (modif->_infos.session_id().length() < EOS_SESSIONMODIFICATION_MIN_SESSIONIDOVERRIDE_LENGTH ||
                    modif->_infos.session_id().length() > EOS_SESSIONMODIFICATION_MAX_SESSIONIDOVERRIDE_LENGTH)
                {
                    delete modif;
                    set_nullptr(OutSessionModificationHandle);
                    return EOS_EResult::EOS_InvalidParameters;
                }
            }
        }

        case EOS_SESSIONS_CREATESESSIONMODIFICATION_API_002:
        {
            const EOS_Sessions_CreateSessionModificationOptions002* opts = reinterpret_cast<const EOS_Sessions_CreateSessionModificationOptions002*>(Options);
            modif->_infos.set_presence_allowed(opts->bPresenceEnabled);
        }

        case EOS_SESSIONS_CREATESESSIONMODIFICATION_API_001:
        {
            const EOS_Sessions_CreateSessionModificationOptions001* opts = reinterpret_cast<const EOS_Sessions_CreateSessionModificationOptions001*>(Options);
            modif->_infos.set_bucket_id(opts->BucketId);
            modif->_infos.set_max_players(opts->MaxPlayers);
            modif->_session_name = opts->SessionName;

            APP_LOG(Log::LogLevel::DEBUG, "Starting session creation: session_name = %s, bucket_id = %s, presence_enabled: %d, host_address: %s",
                modif->_session_name.c_str(),
                modif->_infos.bucket_id().c_str(),
                (int)modif->_infos.presence_allowed(),
                modif->_infos.host_address().c_str());
        }
        break;

        default:
            APP_LOG(Log::LogLevel::FATAL, "Unmanaged API version %d", Options->ApiVersion);
            abort();
    }

    *OutSessionModificationHandle = reinterpret_cast<EOS_HSessionModification>(modif);
    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_Sessions::UpdateSessionModification(const EOS_Sessions_UpdateSessionModificationOptions* Options, EOS_HSessionModification* OutSessionModificationHandle)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr || Options->SessionName == nullptr || OutSessionModificationHandle == nullptr)
    {
        set_nullptr(OutSessionModificationHandle);
        return EOS_EResult::EOS_InvalidParameters;
    }

    EOSSDK_SessionModification* modif = new EOSSDK_SessionModification;
    modif->_api_version = Options->ApiVersion;
    modif->_type = EOSSDK_SessionModification::modif_type::update;

    session_state_t* session = get_session_by_name(Options->SessionName);
    modif->_session_name = Options->SessionName;

    if (session != nullptr)
    {
        modif->_infos = session->infos;
    }
    else
    {
        switch (Options->ApiVersion)
        {
            case EOS_SESSIONS_UPDATESESSIONMODIFICATION_API_001:
            {
                APP_LOG(Log::LogLevel::DEBUG, "Starting session modification: session_name = %s", modif->_session_name.c_str());
            }
            break;

            default:
                APP_LOG(Log::LogLevel::FATAL, "Unmanaged API version %d", Options->ApiVersion);
                abort();
        }
    }
    
    *OutSessionModificationHandle = reinterpret_cast<EOS_HSessionModification>(modif);
    return EOS_EResult::EOS_Success;
}

void EOSSDK_Sessions::UpdateSession(const EOS_Sessions_UpdateSessionOptions* Options, void* ClientData, const EOS_Sessions_OnUpdateSessionCallback CompletionDelegate)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (CompletionDelegate == nullptr)
        return;

    pFrameResult_t res(new FrameResult);

    EOS_Sessions_UpdateSessionCallbackInfo& usci = res->CreateCallback<EOS_Sessions_UpdateSessionCallbackInfo>((CallbackFunc)CompletionDelegate);
    usci.ClientData = ClientData;

    if (Options == nullptr || Options->SessionModificationHandle == nullptr)
    {
        usci.ResultCode = EOS_EResult::EOS_InvalidParameters;
        {
            std::string const& sess_id = GetInvalidProductUserId()->to_string();
            char* session_id = new char[sess_id.length() + 1];
            strncpy(session_id, sess_id.c_str(), sess_id.length() + 1);
            usci.SessionId = session_id;
        }
        {
            char* str = new char[1];
            *str = '\0';
            usci.SessionName = str;
        }
    }
    else
    {
        EOSSDK_SessionModification* modif = reinterpret_cast<EOSSDK_SessionModification*>(Options->SessionModificationHandle);
        usci.SessionId = nullptr;
        {
            std::string const& sess_name = modif->_session_name;
            char* name = new char[sess_name.length() + 1];
            strncpy(name, sess_name.c_str(), sess_name.length() + 1);
            usci.SessionName = name;
        }
        session_state_t* session = get_session_by_id(modif->_infos.session_id());

        switch (modif->_type)
        {
            case EOSSDK_SessionModification::modif_type::creation:
            {
                if (session != nullptr)
                {
                    usci.ResultCode = EOS_EResult::EOS_Sessions_SessionAlreadyExists;
                }
                else
                {
                    auto& session = _sessions[modif->_session_name];

                    if (modif->_infos.session_id().empty())
                        modif->_infos.set_session_id(generate_account_id());

                    {
                        std::string const& sess_id = modif->_infos.session_id();
                        char* session_id = new char[sess_id.length() + 1];
                        strncpy(session_id, sess_id.c_str(), sess_id.length() + 1);
                        usci.SessionId = session_id;
                    }
                    session.state = session_state_t::state_e::created;
                    session.infos = modif->_infos;

                    APP_LOG(Log::LogLevel::DEBUG, "Session created:\n"
                        "  session_name: %s\n"
                        "  session_id: %s\n"
                        "  bucket_id: %s\n"
                        "  host_address: %s\n",
                        modif->_session_name.c_str(),
                        modif->_infos.session_id().c_str(),
                        modif->_infos.bucket_id().c_str(),
                        modif->_infos.host_address().c_str()
                    );

                    session.infos.set_state(utils::GetEnumValue(EOS_EOnlineSessionState::EOS_OSS_Pending));
                    *session.infos.add_players() = Settings::Inst().productuserid->to_string();
                    *session.infos.add_registered_players() = Settings::Inst().productuserid->to_string();

                    usci.ResultCode = EOS_EResult::EOS_Success;
                }
            }
            break;

            case EOSSDK_SessionModification::modif_type::update:
            {
                // FIX 2: If session doesn't exist yet during an update call, return Success
                // instead of EOS_NotFound so games that call UpdateSession before the session
                // is fully committed don't get a fatal error (fixes "update session failed" UI).
                if (session == nullptr)
                {
                    APP_LOG(Log::LogLevel::DEBUG, "UpdateSession: session not found, treating as no-op success.");
                    // Provide a dummy session id so the callback data is valid
                    {
                        std::string dummy_id = modif->_session_name;
                        char* session_id = new char[dummy_id.length() + 1];
                        strncpy(session_id, dummy_id.c_str(), dummy_id.length() + 1);
                        usci.SessionId = session_id;
                    }
                    usci.ResultCode = EOS_EResult::EOS_Success;
                }
                else
                {
                    modif->_infos.set_session_id(session->infos.session_id());
                    modif->_infos.set_state(session->infos.state());
                    // Keep the real host address when updating
                    if (modif->_infos.host_address().empty() || modif->_infos.host_address() == "127.0.0.1")
                        modif->_infos.set_host_address(get_best_host_address());
                    session->infos = modif->_infos;
                    {
                        std::string const& sess_id = session->infos.session_id();
                        char* session_id = new char[sess_id.length() + 1];
                        strncpy(session_id, sess_id.c_str(), sess_id.length() + 1);
                        usci.SessionId = session_id;
                    }

                    APP_LOG(Log::LogLevel::DEBUG, "Session modified:\n"
                        "  session_name: %s\n"
                        "  session_id: %s\n"
                        "  bucket_id: %s\n"
                        "  host_address: %s\n",
                        modif->_session_name.c_str(),
                        modif->_infos.session_id().c_str(),
                        modif->_infos.bucket_id().c_str(),
                        modif->_infos.host_address().c_str()
                    );

                    usci.ResultCode = EOS_EResult::EOS_Success;
                    send_session_info(session);
                }
            }
            break;
        }
    }

    res->done = true;
    GetCB_Manager().add_callback(this, res);
}

void EOSSDK_Sessions::DestroySession(const EOS_Sessions_DestroySessionOptions* Options, void* ClientData, const EOS_Sessions_OnDestroySessionCallback CompletionDelegate)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (CompletionDelegate == nullptr)
        return;

    pFrameResult_t res(new FrameResult);

    EOS_Sessions_DestroySessionCallbackInfo& dsci = res->CreateCallback<EOS_Sessions_DestroySessionCallbackInfo>((CallbackFunc)CompletionDelegate);

    dsci.ClientData = ClientData;

    if (Options == nullptr || Options->SessionName == nullptr)
    {
        dsci.ResultCode = EOS_EResult::EOS_InvalidParameters;
    }
    else
    {
        auto it = _sessions.find(Options->SessionName);
        if (it != _sessions.end())
        {
            APP_LOG(Log::LogLevel::DEBUG, "Destroying session: name %s", Options->SessionName);

            dsci.ResultCode = EOS_EResult::EOS_Success;

            auto join_it = _sessions_join.find(Options->SessionName);
            if (join_it != _sessions_join.end())
            {
                EOS_Sessions_JoinSessionCallbackInfo& jsci = join_it->second->GetCallback<EOS_Sessions_JoinSessionCallbackInfo>();
                jsci.ResultCode = EOS_EResult::EOS_UnexpectedError;
                res->done = true;
                _sessions_join.erase(join_it);
            }

            it->second.infos.set_state(utils::GetEnumValue(EOS_EOnlineSessionState::EOS_OSS_Destroying));

            send_session_destroy(&it->second);
            _sessions.erase(it);
        }
        else
        {
            APP_LOG(Log::LogLevel::DEBUG, "Destroying session: name %s Not Found", Options->SessionName);
            dsci.ResultCode = EOS_EResult::EOS_NotFound;
        }
    }

    res->done = true;
    GetCB_Manager().add_callback(this, res);
}

void EOSSDK_Sessions::JoinSession(const EOS_Sessions_JoinSessionOptions* Options, void* ClientData, const EOS_Sessions_OnJoinSessionCallback CompletionDelegate)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (CompletionDelegate == nullptr)
        return;

    pFrameResult_t res(new FrameResult);
    EOS_Sessions_JoinSessionCallbackInfo& jsci = res->CreateCallback<EOS_Sessions_JoinSessionCallbackInfo>((CallbackFunc)CompletionDelegate);
    jsci.ClientData = ClientData;

    if (Options == nullptr || Options->SessionHandle == nullptr || Options->SessionName == nullptr)
    {
        jsci.ResultCode = EOS_EResult::EOS_InvalidParameters;
        res->done = true;
    }
    else
    {
        EOSSDK_SessionDetails* details = reinterpret_cast<EOSSDK_SessionDetails*>(Options->SessionHandle);
        if (_sessions.count(Options->SessionName) == 0)
        {
            APP_LOG(Log::LogLevel::DEBUG, "Joining session: name %s, session_id: %s", Options->SessionName, details->_infos.session_id().c_str());

            switch ((EOS_EOnlineSessionState)details->_infos.state())
            {
                case EOS_EOnlineSessionState::EOS_OSS_InProgress:
                    if (!details->_infos.join_in_progress_allowed())
                    {
                        jsci.ResultCode = EOS_EResult::EOS_Sessions_NotAllowed;
                        res->done = true;
                        break;
                    }
                
                case EOS_EOnlineSessionState::EOS_OSS_Pending:
                {
                    Session_Join_Request_pb* join = new Session_Join_Request_pb;
                    join->set_session_id(details->_infos.session_id());

                    session_state_t& session = _sessions[Options->SessionName];
                    session.state = session_state_t::state_e::joining;
                    session.infos = details->_infos;
                    _sessions_join[details->_infos.session_id()] = res;

                    jsci.ResultCode = EOS_EResult::EOS_UnexpectedError;
                    send_session_join_request(&session);
                }
                break;

                default:
                {
                    jsci.ResultCode = EOS_EResult::EOS_Sessions_NotAllowed;
                    res->done = true;
                }
            }
        }
        else
        {
            APP_LOG(Log::LogLevel::DEBUG, "joining session: name %s Already Exists", Options->SessionName);
            jsci.ResultCode = EOS_EResult::EOS_Sessions_SessionAlreadyExists;
            res->done = true;
        }
    }

    GetCB_Manager().add_callback(this, res);
}

void EOSSDK_Sessions::StartSession(const EOS_Sessions_StartSessionOptions* Options, void* ClientData, const EOS_Sessions_OnStartSessionCallback CompletionDelegate)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (CompletionDelegate == nullptr)
        return;

    pFrameResult_t res(new FrameResult);
    EOS_Sessions_StartSessionCallbackInfo& ssci = res->CreateCallback<EOS_Sessions_StartSessionCallbackInfo>((CallbackFunc)CompletionDelegate);

    ssci.ClientData = ClientData;

    if (Options == nullptr || Options->SessionName == nullptr)
    {
        ssci.ResultCode = EOS_EResult::EOS_InvalidParameters;
    }
    else
    {
        session_state_t* session = get_session_by_name(Options->SessionName);
        if (session != nullptr)
        {
            APP_LOG(Log::LogLevel::DEBUG, "Starting session: name %s", Options->SessionName);

            ssci.ResultCode = EOS_EResult::EOS_Success;
            switch ((EOS_EOnlineSessionState)session->infos.state())
            {
                case EOS_EOnlineSessionState::EOS_OSS_Destroying:
                case EOS_EOnlineSessionState::EOS_OSS_NoSession :
                case EOS_EOnlineSessionState::EOS_OSS_Ending    :
                case EOS_EOnlineSessionState::EOS_OSS_Creating  :
                case EOS_EOnlineSessionState::EOS_OSS_Starting  :
                case EOS_EOnlineSessionState::EOS_OSS_InProgress:
                    ssci.ResultCode = EOS_EResult::EOS_InvalidParameters;
                    break;

                case EOS_EOnlineSessionState::EOS_OSS_Ended:
                case EOS_EOnlineSessionState::EOS_OSS_Pending:
                    session->infos.set_state(utils::GetEnumValue(EOS_EOnlineSessionState::EOS_OSS_InProgress));
                    send_session_info(session);
            }
        }
        else
        {
            APP_LOG(Log::LogLevel::DEBUG, "Starting session: name %s Not Found", Options->SessionName);
            ssci.ResultCode = EOS_EResult::EOS_NotFound;
        }
    }

    res->done = true;
    GetCB_Manager().add_callback(this, res);
}

void EOSSDK_Sessions::EndSession(const EOS_Sessions_EndSessionOptions* Options, void* ClientData, const EOS_Sessions_OnEndSessionCallback CompletionDelegate)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (CompletionDelegate == nullptr)
        return;

    pFrameResult_t res(new FrameResult);
    EOS_Sessions_EndSessionCallbackInfo& esci = res->CreateCallback<EOS_Sessions_EndSessionCallbackInfo>((CallbackFunc)CompletionDelegate);

    esci.ClientData = ClientData;

    if (Options == nullptr || Options->SessionName == nullptr)
    {
        esci.ResultCode = EOS_EResult::EOS_InvalidParameters;
    }
    else
    {
        session_state_t* session = get_session_by_name(Options->SessionName);
        if (session != nullptr)
        {
            esci.ResultCode = EOS_EResult::EOS_Success;
            session->infos.set_state(utils::GetEnumValue(EOS_EOnlineSessionState::EOS_OSS_Ended));
        }
        else
        {
            esci.ResultCode = EOS_EResult::EOS_NotFound;
        }
    }

    res->done = true;
    GetCB_Manager().add_callback(this, res);
}

void EOSSDK_Sessions::RegisterPlayers(const EOS_Sessions_RegisterPlayersOptions* Options, void* ClientData, const EOS_Sessions_OnRegisterPlayersCallback CompletionDelegate)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (CompletionDelegate == nullptr)
        return;

    pFrameResult_t res(new FrameResult);

    EOS_Sessions_RegisterPlayersCallbackInfo& rpci = res->CreateCallback<EOS_Sessions_RegisterPlayersCallbackInfo>((CallbackFunc)CompletionDelegate);
    rpci.ClientData = ClientData;

    if (Options->SessionName == nullptr || Options->PlayersToRegister == nullptr || Options->PlayersToRegisterCount == 0)
    {
        rpci.ResultCode = EOS_EResult::EOS_InvalidParameters;
    }
    else
    {
        session_state_t* session = get_session_by_name(Options->SessionName);
        if (session == nullptr)
        {
            rpci.ResultCode = EOS_EResult::EOS_NotFound;
        }
        else
        {
            if (is_player_registered(Settings::Inst().productuserid->to_string(), session))
            {
                google::protobuf::RepeatedPtrField<std::string> registered;
                for (uint32_t i = 0; i < Options->PlayersToRegisterCount; ++i)
                {
                    if (register_player_to_session(Options->PlayersToRegister[i]->to_string(), session))
                        *registered.Add() = Options->PlayersToRegister[i]->to_string();
                }
                if (registered.empty())
                {
                    rpci.ResultCode = EOS_EResult::EOS_NoChange;
                }
                else
                {
                    rpci.ResultCode = EOS_EResult::EOS_Success;

                    Session_Register_pb* register_ = new Session_Register_pb;
                    register_->set_session_id(session->infos.session_id());
                    *register_->mutable_member_ids() = std::move(registered);
                    send_session_register(register_, session);
                }
            }
            else
            {
                rpci.ResultCode = EOS_EResult::EOS_Sessions_NotAllowed;
            }
        }
    }

    res->done = true;
    GetCB_Manager().add_callback(this, res);
}

void EOSSDK_Sessions::UnregisterPlayers(const EOS_Sessions_UnregisterPlayersOptions* Options, void* ClientData, const EOS_Sessions_OnUnregisterPlayersCallback CompletionDelegate)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (CompletionDelegate == nullptr)
        return;

    pFrameResult_t res(new FrameResult);

    EOS_Sessions_UnregisterPlayersCallbackInfo& upci = res->CreateCallback<EOS_Sessions_UnregisterPlayersCallbackInfo>((CallbackFunc)CompletionDelegate);
    upci.ClientData = ClientData;

    if (Options->SessionName == nullptr || Options->PlayersToUnregister == nullptr || Options->PlayersToUnregisterCount == 0)
    {
        upci.ResultCode = EOS_EResult::EOS_InvalidParameters;
    }
    else
    {
        session_state_t* session = get_session_by_name(Options->SessionName);
        if (session == nullptr)
        {
            upci.ResultCode = EOS_EResult::EOS_NotFound;
        }
        else
        {
            if (is_player_registered(Settings::Inst().productuserid->to_string(), session))
            {
                google::protobuf::RepeatedPtrField<std::string> unregistered;
                for (uint32_t i = 0; i < Options->PlayersToUnregisterCount; ++i)
                {
                    if (unregister_player_from_session(Options->PlayersToUnregister[i]->to_string(), session))
                        *unregistered.Add() = Options->PlayersToUnregister[i]->to_string();
                }
                if (unregistered.empty())
                {
                    upci.ResultCode = EOS_EResult::EOS_NoChange;
                }
                else
                {
                    upci.ResultCode = EOS_EResult::EOS_Success;

                    Session_Unregister_pb* unregister = new Session_Unregister_pb;
                    unregister->set_session_id(session->infos.session_id());
                    *unregister->mutable_member_ids() = std::move(unregistered);
                    send_session_unregister(unregister, session);
                }
            }
            else
            {
                upci.ResultCode = EOS_EResult::EOS_Sessions_NotAllowed;
            }
        }
    }

    res->done = true;
    GetCB_Manager().add_callback(this, res);
}

void EOSSDK_Sessions::SendInvite(const EOS_Sessions_SendInviteOptions* Options, void* ClientData, const EOS_Sessions_OnSendInviteCallback CompletionDelegate)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (CompletionDelegate == nullptr)
        return;

    pFrameResult_t res(new FrameResult);

    EOS_Sessions_SendInviteCallbackInfo& sici = res->CreateCallback<EOS_Sessions_SendInviteCallbackInfo>((CallbackFunc)CompletionDelegate);

    sici.ClientData = ClientData;

    if (Options == nullptr || Options->SessionName == nullptr || Options->TargetUserId == nullptr)
    {
        sici.ResultCode = EOS_EResult::EOS_InvalidParameters;
    }
    else
    {
        session_state_t* session = get_session_by_name(Options->SessionName);
        if (session == nullptr || GetEOS_Connect().get_user_by_productid(Options->TargetUserId) == GetEOS_Connect().get_end_users())
        {
            sici.ResultCode = EOS_EResult::EOS_NotFound;
        }
        else
        {
            Session_Invite_pb* invite = new Session_Invite_pb;
            *invite->mutable_infos() = session->infos;
            send_session_invite(Options->TargetUserId->to_string(), invite);
            sici.ResultCode = EOS_EResult::EOS_Success;
        }
    }

    res->done = true;
    GetCB_Manager().add_callback(this, res);
}

void EOSSDK_Sessions::RejectInvite(const EOS_Sessions_RejectInviteOptions* Options, void* ClientData, const EOS_Sessions_OnRejectInviteCallback CompletionDelegate)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (CompletionDelegate == nullptr)
        return;

    pFrameResult_t res(new FrameResult);

    EOS_Sessions_RejectInviteCallbackInfo& rici = res->CreateCallback<EOS_Sessions_RejectInviteCallbackInfo>((CallbackFunc)CompletionDelegate);

    rici.ClientData = ClientData;

    auto it = std::find_if(_session_invites.begin(), _session_invites.end(), [Options]( session_invite_t& invite)
    {
        return invite.invite_id == Options->InviteId;
    });

    if (it == _session_invites.end())
        rici.ResultCode = EOS_EResult::EOS_NotFound;
    else
        rici.ResultCode = EOS_EResult::EOS_Success;

    res->done = true;
    GetCB_Manager().add_callback(this, res);
}

void EOSSDK_Sessions::QueryInvites(const EOS_Sessions_QueryInvitesOptions* Options, void* ClientData, const EOS_Sessions_OnQueryInvitesCallback CompletionDelegate)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (CompletionDelegate == nullptr)
        return;

    pFrameResult_t res(new FrameResult);

    EOS_Sessions_QueryInvitesCallbackInfo& qici = res->CreateCallback<EOS_Sessions_QueryInvitesCallbackInfo>((CallbackFunc)CompletionDelegate);
    qici.LocalUserId = GetEOS_Connect().get_myself()->first;
    qici.ClientData = ClientData;

    if (Options == nullptr || Options->LocalUserId == nullptr)
        qici.ResultCode = EOS_EResult::EOS_InvalidParameters;
    else
        qici.ResultCode = EOS_EResult::EOS_Success;

    res->done = true;
    GetCB_Manager().add_callback(this, res);
}

uint32_t EOSSDK_Sessions::GetInviteCount(const EOS_Sessions_GetInviteCountOptions* Options)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr || Options->LocalUserId != GetEOS_Connect().get_myself()->first)
        return 0;

    return _session_invites.size();
}

EOS_EResult EOSSDK_Sessions::GetInviteIdByIndex(const EOS_Sessions_GetInviteIdByIndexOptions* Options, char* OutBuffer, int32_t* InOutBufferLength)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr || Options->LocalUserId != GetEOS_Connect().get_myself()->first ||
        Options->Index >= _session_invites.size() ||
        OutBuffer == nullptr || InOutBufferLength == nullptr)
    {
        return EOS_EResult::EOS_InvalidParameters;
    }
    
    auto it = _session_invites.begin();
    std::advance(it, Options->Index);

    strncpy(OutBuffer, it->invite_id.c_str(), *InOutBufferLength);
    *InOutBufferLength = std::min<int32_t>(it->invite_id.length() + 1, *InOutBufferLength);

    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_Sessions::CreateSessionSearch(const EOS_Sessions_CreateSessionSearchOptions* Options, EOS_HSessionSearch* OutSessionSearchHandle)
{
    TRACE_FUNC();

    if (Options == nullptr || Options->MaxSearchResults == 0 || OutSessionSearchHandle == nullptr)
    {
        set_nullptr(OutSessionSearchHandle);
        return EOS_EResult::EOS_InvalidParameters;
    }
    
    _session_searchs.emplace_back();
    EOSSDK_SessionSearch*& session_search = _session_searchs.back();
    session_search = new EOSSDK_SessionSearch;

    *OutSessionSearchHandle = reinterpret_cast<EOS_HSessionSearch>(session_search);

    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_Sessions::CopyActiveSessionHandle(const EOS_Sessions_CopyActiveSessionHandleOptions* Options, EOS_HActiveSession* OutSessionHandle)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr || Options->SessionName == nullptr || OutSessionHandle == nullptr)
    {
        set_nullptr(OutSessionHandle);
        return EOS_EResult::EOS_InvalidParameters;
    }

    session_state_t* session = get_session_by_name(Options->SessionName);
    if (session == nullptr)
    {
        APP_LOG(Log::LogLevel::DEBUG, "Didn't find Active Session %s", Options->SessionName);
        return EOS_EResult::EOS_NotFound;
    }

    APP_LOG(Log::LogLevel::DEBUG, "Found Active Session %s", Options->SessionName);
    EOSSDK_ActiveSession* active_session = new EOSSDK_ActiveSession;
    
    active_session->_session_name = Options->SessionName;
    active_session->_infos = session->infos;

    *OutSessionHandle = reinterpret_cast<EOS_HActiveSession>(active_session);

    return EOS_EResult::EOS_Success;
}

EOS_NotificationId EOSSDK_Sessions::AddNotifySessionInviteReceived(const EOS_Sessions_AddNotifySessionInviteReceivedOptions* Options, void* ClientData, const EOS_Sessions_OnSessionInviteReceivedCallback NotificationFn)
{
    TRACE_FUNC();
    
    if (NotificationFn == nullptr)
        return EOS_INVALID_NOTIFICATIONID;

    pFrameResult_t res(new FrameResult);
    
    EOS_Sessions_SessionInviteReceivedCallbackInfo& sirci = res->CreateCallback<EOS_Sessions_SessionInviteReceivedCallbackInfo>((CallbackFunc)NotificationFn);

    sirci.ClientData = ClientData;
    sirci.LocalUserId = GetEOS_Connect().get_myself()->first;
    sirci.InviteId = new char[max_accountid_length];

    return GetCB_Manager().add_notification(this, res);
}

void EOSSDK_Sessions::RemoveNotifySessionInviteReceived(EOS_NotificationId InId)
{
    TRACE_FUNC();
    GetCB_Manager().remove_notification(this, InId);
}

EOS_NotificationId EOSSDK_Sessions::AddNotifySessionInviteAccepted(const EOS_Sessions_AddNotifySessionInviteAcceptedOptions* Options, void* ClientData, const EOS_Sessions_OnSessionInviteAcceptedCallback NotificationFn)
{
    TRACE_FUNC();

    if (NotificationFn == nullptr)
        return EOS_INVALID_NOTIFICATIONID;

    pFrameResult_t res(new FrameResult);

    EOS_Sessions_SessionInviteAcceptedCallbackInfo& siaci = res->CreateCallback<EOS_Sessions_SessionInviteAcceptedCallbackInfo>((CallbackFunc)NotificationFn);

    siaci.ClientData = ClientData;
    siaci.LocalUserId = GetEOS_Connect().get_myself()->first;
    siaci.SessionId = new char[max_accountid_length];

    return GetCB_Manager().add_notification(this, res);
}

void EOSSDK_Sessions::RemoveNotifySessionInviteAccepted(EOS_NotificationId InId)
{
    TRACE_FUNC();
    GetCB_Manager().remove_notification(this, InId);
}

EOS_NotificationId EOSSDK_Sessions::AddNotifyJoinSessionAccepted(const EOS_Sessions_AddNotifyJoinSessionAcceptedOptions* Options, void* ClientData, const EOS_Sessions_OnJoinSessionAcceptedCallback NotificationFn)
{
    TRACE_FUNC();

    if (NotificationFn == nullptr)
        return EOS_INVALID_NOTIFICATIONID;

    pFrameResult_t res(new FrameResult);

    EOS_Sessions_JoinSessionAcceptedCallbackInfo& jsaci = res->CreateCallback<EOS_Sessions_JoinSessionAcceptedCallbackInfo>((CallbackFunc)NotificationFn);

    jsaci.ClientData = ClientData;
    jsaci.LocalUserId = GetEOS_Connect().get_myself()->first;
    jsaci.UiEventId = EOS_UI_EVENTID_INVALID;

    return GetCB_Manager().add_notification(this, res);
}

void EOSSDK_Sessions::RemoveNotifyJoinSessionAccepted(EOS_NotificationId InId)
{
    TRACE_FUNC();
    GetCB_Manager().remove_notification(this, InId);
}

EOS_EResult EOSSDK_Sessions::CopySessionHandleByInviteId(const EOS_Sessions_CopySessionHandleByInviteIdOptions* Options, EOS_HSessionDetails* OutSessionHandle)
{
    TRACE_FUNC();
    GLOBAL_LOCK();
    
    if (Options == nullptr || Options->InviteId == nullptr || OutSessionHandle == nullptr)
    {
        set_nullptr(OutSessionHandle);
        return EOS_EResult::EOS_InvalidParameters;
    }

    auto it = std::find_if(_session_invites.begin(), _session_invites.end(), [Options](session_invite_t const& invite)
    {
        return invite.invite_id == Options->InviteId;
    });

    if (it == _session_invites.end())
        return EOS_EResult::EOS_NotFound;
    
    EOSSDK_SessionDetails* details = new EOSSDK_SessionDetails;
    details->_infos = it->infos;
    *OutSessionHandle = reinterpret_cast<EOS_HSessionDetails>(details);

    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_Sessions::CopySessionHandleByUiEventId(const EOS_Sessions_CopySessionHandleByUiEventIdOptions* Options, EOS_HSessionDetails* OutSessionHandle)
{
    TRACE_FUNC();
    GLOBAL_LOCK();
    
    if (Options == nullptr || Options->UiEventId == EOS_UI_EVENTID_INVALID || OutSessionHandle == nullptr)
    {
        set_nullptr(OutSessionHandle);
        return EOS_EResult::EOS_InvalidParameters;
    }

    *OutSessionHandle = nullptr;
    return EOS_EResult::EOS_InvalidParameters;
}

EOS_EResult EOSSDK_Sessions::CopySessionHandleForPresence(const EOS_Sessions_CopySessionHandleForPresenceOptions* Options, EOS_HSessionDetails* OutSessionHandle)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr || OutSessionHandle == nullptr)
    {
        set_nullptr(OutSessionHandle);
        return EOS_EResult::EOS_InvalidParameters;
    }

    for (auto const& session : _sessions)
    {
        if (session.second.infos.presence_allowed())
        {
            APP_LOG(Log::LogLevel::DEBUG, "Found Session for presence");
            EOSSDK_SessionDetails *details = new EOSSDK_SessionDetails;
            details->_infos = session.second.infos;
            *OutSessionHandle = reinterpret_cast<EOS_HSessionDetails>(details);
            return EOS_EResult::EOS_Success;
        }
    }

    APP_LOG(Log::LogLevel::DEBUG, "Did not find Session for presence");
    *OutSessionHandle = nullptr;
    return EOS_EResult::EOS_NotFound;
}

EOS_EResult EOSSDK_Sessions::IsUserInSession(const EOS_Sessions_IsUserInSessionOptions* Options)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    if (Options == nullptr || Options->TargetUserId == nullptr || Options->SessionName == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    if (Options->TargetUserId == Settings::Inst().productuserid)
    {
        session_state_t* session = get_session_by_name(Options->SessionName);
        if (session != nullptr)
        {
            for (auto const& player : session->infos.players())
            {
                if (GetProductUserId(player) == Options->TargetUserId)
                    return EOS_EResult::EOS_Success;
            }
        }
        else
        {
            return EOS_EResult::EOS_Sessions_InvalidSession;
        }
    }
    else
    {
        auto user_infos = GetEOS_Connect().get_user_by_productid(Options->TargetUserId);
        if (user_infos != GetEOS_Connect().get_end_users())
        {
            for (auto const& session : user_infos->second.infos.sessions())
            {
                if (session.first == Options->SessionName)
                    return EOS_EResult::EOS_Success;
            }
        }
    }

    return EOS_EResult::EOS_NotFound;
}

EOS_EResult EOSSDK_Sessions::DumpSessionState(const EOS_Sessions_DumpSessionStateOptions* Options)
{
    TRACE_FUNC();
    GLOBAL_LOCK();
    return EOS_EResult::EOS_Success;
}

///////////////////////////////////////////////////////////////////////////////
//                           Network Send messages                           //
///////////////////////////////////////////////////////////////////////////////
bool EOSSDK_Sessions::send_to_all_members(Network_Message_pb & msg, session_state_t* session)
{
    TRACE_FUNC();
    assert(session != nullptr);

    for (auto const& player : session->infos.players())
    {
        if (player != msg.source_id())
        {
            msg.set_dest_id(player);
            GetNetwork().TCPSendTo(msg);
        }
    }
    return true;
}

bool EOSSDK_Sessions::send_session_info_request(Network::peer_t const& peerid, Session_Infos_Request_pb* req)
{
    TRACE_FUNC();
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    Session_Message_pb* session = new Session_Message_pb;

    session->set_allocated_sessions_request(req);
    msg.set_allocated_session(session);
    msg.set_source_id(user_id);
    msg.set_dest_id(peerid);

    return GetNetwork().TCPSendTo(msg);
}

bool EOSSDK_Sessions::send_session_info(session_state_t* session)
{
    TRACE_FUNC();
    assert(session != nullptr);
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    Session_Message_pb* session_pb = new Session_Message_pb;

    session_pb->set_allocated_session_infos(&session->infos);
    msg.set_allocated_session(session_pb);
    msg.set_source_id(user_id);
    msg.set_game_id(Settings::Inst().appid);

    bool res = send_to_all_members(msg, session);

    session_pb->release_session_infos();

    return res;
}

bool EOSSDK_Sessions::send_session_destroy(session_state_t *session)
{
    TRACE_FUNC();
    assert(session != nullptr);
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    Session_Message_pb* session_pb = new Session_Message_pb;
    Session_Destroy_pb* destr = new Session_Destroy_pb;

    destr->set_session_id(session->infos.session_id());

    session_pb->set_allocated_session_destroy(destr);
    msg.set_allocated_session(session_pb);
    msg.set_source_id(user_id);
    msg.set_game_id(Settings::Inst().appid);

    return send_to_all_members(msg, session);
}

bool EOSSDK_Sessions::send_sessions_search_response(Network::peer_t const& peerid, Sessions_Search_response_pb* resp)
{
    TRACE_FUNC();
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    Sessions_Search_Message_pb* search = new Sessions_Search_Message_pb;

    search->set_allocated_search_response(resp);
    msg.set_allocated_sessions_search(search);
    msg.set_source_id(user_id);
    msg.set_dest_id(peerid);
    msg.set_game_id(Settings::Inst().appid);

    return GetNetwork().TCPSendTo(msg);
}

bool EOSSDK_Sessions::send_session_join_request(session_state_t *session)
{
    TRACE_FUNC();
    assert(session != nullptr);
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    Session_Message_pb* session_pb = new Session_Message_pb;
    Session_Join_Request_pb* req = new Session_Join_Request_pb;

    session_pb->set_allocated_session_join_request(req);
    msg.set_allocated_session(session_pb);
    msg.set_source_id(user_id);
    msg.set_game_id(Settings::Inst().appid);

    req->set_session_id(session->infos.session_id());

    return send_to_all_members(msg, session);
}

bool EOSSDK_Sessions::send_session_join_response(Network::peer_t const& peerid, Session_Join_Response_pb* resp)
{
    TRACE_FUNC();
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    Session_Message_pb* session = new Session_Message_pb;

    session->set_allocated_session_join_response(resp);
    msg.set_allocated_session(session);
    msg.set_source_id(user_id);
    msg.set_dest_id(peerid);
    msg.set_game_id(Settings::Inst().appid);

    session_state_t* pSession = get_session_by_id(resp->session_id());
    if (pSession != nullptr)
        send_to_all_members(msg, pSession);

    return GetNetwork().TCPSendTo(msg);
}

bool EOSSDK_Sessions::send_session_invite(Network::peer_t const& peerid, Session_Invite_pb* invite)
{
    TRACE_FUNC();
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    Session_Message_pb* session = new Session_Message_pb;

    session->set_allocated_session_invite(invite);
    msg.set_allocated_session(session);
    msg.set_source_id(user_id);
    msg.set_dest_id(peerid);
    msg.set_game_id(Settings::Inst().appid);

    return GetNetwork().TCPSendTo(msg);
}

bool EOSSDK_Sessions::send_session_invite_response(Network::peer_t const& peerid, Session_Invite_Response_pb* resp)
{
    TRACE_FUNC();
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    Session_Message_pb* session = new Session_Message_pb;

    session->set_allocated_session_invite_response(resp);
    msg.set_allocated_session(session);
    msg.set_source_id(user_id);
    msg.set_dest_id(peerid);
    msg.set_game_id(Settings::Inst().appid);

    return GetNetwork().TCPSendTo(msg);
}

bool EOSSDK_Sessions::send_session_register(Session_Register_pb* register_, session_state_t* session)
{
    TRACE_FUNC();
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    Session_Message_pb* session_pb = new Session_Message_pb;

    session_pb->set_allocated_session_register(register_);
    msg.set_allocated_session(session_pb);
    msg.set_source_id(user_id);
    msg.set_game_id(Settings::Inst().appid);

    return send_to_all_members(msg, session);
}

bool EOSSDK_Sessions::send_session_unregister(Session_Unregister_pb* unregister, session_state_t* session)
{
    TRACE_FUNC();
    std::string const& user_id = Settings::Inst().productuserid->to_string();

    Network_Message_pb msg;
    Session_Message_pb* session_pb = new Session_Message_pb;

    session_pb->set_allocated_session_unregister(unregister);
    msg.set_allocated_session(session_pb);
    msg.set_source_id(user_id);
    msg.set_game_id(Settings::Inst().appid);

    return send_to_all_members(msg, session);
}

///////////////////////////////////////////////////////////////////////////////
//                          Network Receive messages                         //
///////////////////////////////////////////////////////////////////////////////
bool EOSSDK_Sessions::on_peer_disconnect(Network_Message_pb const& msg, Network_Peer_Disconnect_pb const& peer)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    for (auto& session : _sessions)
        remove_player_from_session(msg.source_id(), &session.second);

    return true;
}

bool EOSSDK_Sessions::on_session_info_request(Network_Message_pb const& msg, Session_Infos_Request_pb const& req)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    session_state_t* session = get_session_by_id(req.session_id());
    Session_Infos_pb* infos;

    if (session == nullptr)
        infos = new Session_Infos_pb();
    else
        infos = new Session_Infos_pb(session->infos);

    Network_Message_pb resp;
    Session_Message_pb* session_pb = new Session_Message_pb;

    session_pb->set_allocated_session_infos(infos);
    resp.set_allocated_session(session_pb);
    resp.set_source_id(Settings::Inst().productuserid->to_string());
    resp.set_dest_id(msg.source_id());

    return GetNetwork().TCPSendTo(resp);
}

bool EOSSDK_Sessions::on_session_info(Network_Message_pb const& msg, Session_Infos_pb const& infos)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    session_state_t *session = get_session_by_id(infos.session_id());
    if (session != nullptr)
        session->infos = infos;

    return true;
}

bool EOSSDK_Sessions::on_session_destroy(Network_Message_pb const& msg, Session_Destroy_pb const& destr)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    session_state_t* session = get_session_by_id(destr.session_id());
    if (session != nullptr)
        remove_player_from_session(msg.source_id(), session);

    return true;
}

bool EOSSDK_Sessions::on_sessions_search(Network_Message_pb const& msg, Sessions_Search_pb const& search)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    Sessions_Search_response_pb* resp = new Sessions_Search_response_pb;
    resp->set_search_id(search.search_id());

    if (msg.game_id() == Settings::Inst().appid)
    {
        if (!search.session_id().empty())
        {
            session_state_t* pSession = get_session_by_id(search.session_id());
            if (pSession != nullptr && session_match_from_attributes(pSession, search.parameters()))
            {
                APP_LOG(Log::LogLevel::DEBUG, "sessions found");
                *resp->mutable_sessions()->Add() = pSession->infos;
            }
        }
        else if (search.parameters_size() > 0)
        {
            std::vector<session_state_t*> sessions = std::move(get_sessions_from_attributes(search.parameters()));
            APP_LOG(Log::LogLevel::DEBUG, "sessions found: %d", sessions.size());
            for (auto& session : sessions)
                *resp->mutable_sessions()->Add() = session->infos;
        }
        else if (GetProductUserId(search.target_id()) == GetEOS_Connect().get_myself()->first)
        {
            for (auto& session : _sessions)
                *resp->mutable_sessions()->Add() = session.second.infos;
        }
    }

    return send_sessions_search_response(msg.source_id(), resp);
}

bool EOSSDK_Sessions::on_session_join_request(Network_Message_pb const& msg, Session_Join_Request_pb const& req)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    session_state_t* pSession = get_session_by_id(req.session_id());
    if (!is_player_registered(Settings::Inst().productuserid->to_string(), pSession))
        return true;

    Session_Join_Response_pb* resp = new Session_Join_Response_pb;
    resp->set_session_id(req.session_id());
    resp->set_user_id(msg.source_id());

    if (GetEOS_Connect().get_user_by_productid(GetProductUserId(msg.source_id())) != GetEOS_Connect().get_end_users())
    {
        if (pSession->infos.max_players() - pSession->infos.players_size())
        {
            APP_LOG(Log::LogLevel::DEBUG, "Join request accepted.");
            resp->set_reason(utils::GetEnumValue(EOS_EResult::EOS_Success));
            add_player_to_session(msg.source_id(), pSession);
        }
        else
        {
            APP_LOG(Log::LogLevel::DEBUG, "Join request rejected: This session is full.");
            resp->set_reason(utils::GetEnumValue(EOS_EResult::EOS_Sessions_TooManyPlayers));
        }
    }
    else
    {
        APP_LOG(Log::LogLevel::DEBUG, "Join request rejected: We don't know (yet?) the user.");
        resp->set_reason(utils::GetEnumValue(EOS_EResult::EOS_Sessions_NotAllowed));
    }

    return send_session_join_response(msg.source_id(), resp);
}

bool EOSSDK_Sessions::on_session_join_response(Network_Message_pb const& msg, Session_Join_Response_pb const& resp)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    std::string const& user_id = Settings::Inst().productuserid->to_string();
    auto session_it = std::find_if(_sessions.begin(), _sessions.end(), [&resp]( std::pair<const std::string, session_state_t>& item)
    {
        return item.second.infos.session_id() == resp.session_id();
    });

    auto reason = static_cast<EOS_EResult>(resp.reason());
    if (resp.user_id() == Settings::Inst().productuserid->to_string())
    {
        auto it = _sessions_join.find(resp.session_id());
        if (it != _sessions_join.end())
        {
            EOS_Sessions_JoinSessionCallbackInfo& jsci = it->second->GetCallback<EOS_Sessions_JoinSessionCallbackInfo>();
            jsci.ResultCode = static_cast<EOS_EResult>(resp.reason());

            switch (jsci.ResultCode)
            {
                case EOS_EResult::EOS_Sessions_NotAllowed:
                    APP_LOG(Log::LogLevel::DEBUG, "(%s) Join request rejected: We don't know (yet?) the user.", msg.source_id().c_str());
                    break;

                case EOS_EResult::EOS_Sessions_TooManyPlayers:
                    APP_LOG(Log::LogLevel::DEBUG, "(%s) Join rejected: This session is full.", msg.source_id().c_str());
                    it->second->done = true;
                    _sessions_join.erase(it);
                    _sessions.erase(session_it);
                    break;
                
                case EOS_EResult::EOS_Success:
                    APP_LOG(Log::LogLevel::DEBUG, "(%s) Join accepted.", msg.source_id().c_str());
                    it->second->done = true;
                    _sessions_join.erase(it);
                    session_it->second.state = session_state_t::state_e::joined;
                    add_player_to_session(user_id, &session_it->second);
                    break;
            }
        }
        else
        {
            APP_LOG(Log::LogLevel::DEBUG, "Join request not found.");
        }
    }
    else if(is_player_in_session(user_id, &session_it->second))
    {
        if (reason == EOS_EResult::EOS_Success)
        {
            APP_LOG(Log::LogLevel::DEBUG, "Add new player (%s) to session.", resp.user_id().c_str());
            add_player_to_session(resp.user_id(), &session_it->second);
        }
    }

    return true;
}

bool EOSSDK_Sessions::on_session_invite(Network_Message_pb const& msg, Session_Invite_pb const& invite)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    EOS_ProductUserId target_id = GetProductUserId(msg.source_id());
    
    session_invite_t invite_infos;
    invite_infos.infos = invite.infos();
    invite_infos.invite_id = std::move(generate_account_id());
    invite_infos.peer_id = target_id;

    _session_invites.emplace_back(std::move(invite_infos));
    std::string const& invite_id = _session_invites.back().invite_id;

    std::vector<pFrameResult_t> notifs = std::move(GetCB_Manager().get_notifications(this, EOS_Sessions_SessionInviteReceivedCallbackInfo::k_iCallback));
    for (auto& notif : notifs)
    {
        EOS_Sessions_SessionInviteReceivedCallbackInfo& sirci = notif->GetCallback<EOS_Sessions_SessionInviteReceivedCallbackInfo>();
        strncpy(const_cast<char*>(sirci.InviteId), invite_id.c_str(), max_accountid_length);
        sirci.TargetUserId = target_id;
        notif->GetFunc()(notif->GetFuncParam());
    }

    return true;
}

bool EOSSDK_Sessions::on_session_invite_response(Network_Message_pb const& msg, Session_Invite_Response_pb const& resp)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    std::vector<pFrameResult_t> notifs = std::move(GetCB_Manager().get_notifications(this, EOS_Sessions_SessionInviteAcceptedCallbackInfo::k_iCallback));
    for (auto& notif : notifs)
    {
        EOS_Sessions_SessionInviteAcceptedCallbackInfo& siacbi = notif->GetCallback<EOS_Sessions_SessionInviteAcceptedCallbackInfo>();
        siacbi.TargetUserId = GetProductUserId(msg.source_id());
        strncpy(const_cast<char*>(siacbi.SessionId), resp.session_id().c_str(), max_accountid_length);
        notif->GetFunc()(notif->GetFuncParam());
    }

    return true;
}

bool EOSSDK_Sessions::on_session_register(Network_Message_pb const& msg, Session_Register_pb const& register_)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    session_state_t* pSession = get_session_by_id(register_.session_id());

    if (is_player_registered(msg.source_id(), pSession))
    {
        for (auto const& member_id : register_.member_ids())
            register_player_to_session(member_id, pSession);
    }

    return true;
}

bool EOSSDK_Sessions::on_session_unregister(Network_Message_pb const& msg, Session_Unregister_pb const& unregister)
{
    TRACE_FUNC();
    GLOBAL_LOCK();

    session_state_t* pSession = get_session_by_id(unregister.session_id());

    if (is_player_registered(msg.source_id(), pSession))
    {
        for (auto const& member_id : unregister.member_ids())
            unregister_player_from_session(member_id, pSession);
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
//                                 IRunFrame                                 //
///////////////////////////////////////////////////////////////////////////////
bool EOSSDK_Sessions::CBRunFrame()
{
    GLOBAL_LOCK();

    for (auto it = _session_searchs.begin(); it != _session_searchs.end();)
    {
        if ((*it)->released())
        {
            delete* it;
            it = _session_searchs.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return true;
}

bool EOSSDK_Sessions::RunNetwork(Network_Message_pb const& msg)
{
    switch (msg.messages_case())
    {
        case Network_Message_pb::MessagesCase::kSession:
        {
            if (GetProductUserId(msg.source_id()) == GetEOS_Connect().get_myself()->first)
                return true;

            Session_Message_pb const& session = msg.session();

            switch (session.message_case())
            {
                case Session_Message_pb::MessageCase::kSessionsRequest      : return on_session_info_request(msg, session.sessions_request());
                case Session_Message_pb::MessageCase::kSessionInfos         : return on_session_info(msg, session.session_infos());
                case Session_Message_pb::MessageCase::kSessionDestroy       : return on_session_destroy(msg, session.session_destroy());
                case Session_Message_pb::MessageCase::kSessionJoinRequest   : return on_session_join_request(msg, session.session_join_request());
                case Session_Message_pb::MessageCase::kSessionJoinResponse  : return on_session_join_response(msg, session.session_join_response());
                case Session_Message_pb::MessageCase::kSessionInvite        : return on_session_invite(msg, session.session_invite());
                case Session_Message_pb::MessageCase::kSessionInviteResponse: return on_session_invite_response(msg, session.session_invite_response());
                case Session_Message_pb::MessageCase::kSessionRegister      : return on_session_register(msg, session.session_register());
                case Session_Message_pb::MessageCase::kSessionUnregister    : return on_session_unregister(msg, session.session_unregister());
                default: APP_LOG(Log::LogLevel::WARN, "Unhandled network message %d", session.message_case());
            }
        }
        break;

        case Network_Message_pb::MessagesCase::kSessionsSearch:
        {
            Sessions_Search_Message_pb const& search = msg.sessions_search();

            switch (search.message_case())
            {
                case Sessions_Search_Message_pb::MessageCase::kSearch: return on_sessions_search(msg, search.search());
            }
        }
    }

    return true;
}

bool EOSSDK_Sessions::RunCallbacks(pFrameResult_t res)
{
    GLOBAL_LOCK();

    switch (res->ICallback())
    {
        case EOS_Sessions_JoinSessionCallbackInfo::k_iCallback:
        {
            auto now = std::chrono::steady_clock::now();
            if ((now - res->created_time) > join_timeout)
            {
                EOS_Sessions_JoinSessionCallbackInfo& jsci = res->GetCallback<EOS_Sessions_JoinSessionCallbackInfo>();
                if (jsci.ResultCode == EOS_EResult::EOS_UnexpectedError)
                    jsci.ResultCode = EOS_EResult::EOS_TimedOut;

                auto join_it = std::find_if(_sessions_join.begin(), _sessions_join.end(), [&res]( std::pair<std::string const, pFrameResult_t> &join )
                {
                    return res == join.second;
                });
                if (join_it != _sessions_join.end())
                {
                    auto session_it = std::find_if(_sessions.begin(), _sessions.end(), [join_it]( std::pair<const std::string, session_state_t>& item)
                    {
                        return item.second.infos.session_id() == join_it->first;
                    });
                    if (session_it == _sessions.end())
                        _sessions.erase(session_it);

                    _sessions_join.erase(join_it);
                }

                res->done = true;
            }
        }
        break;
    }

    return res->done;
}

void EOSSDK_Sessions::FreeCallback(pFrameResult_t res)
{
    GLOBAL_LOCK();

    switch (res->ICallback())
    {
        case EOS_Sessions_UpdateSessionCallbackInfo::k_iCallback:
        {
            EOS_Sessions_UpdateSessionCallbackInfo& usci = res->GetCallback<EOS_Sessions_UpdateSessionCallbackInfo>();
            delete[]usci.SessionId;
            delete[]usci.SessionName;
        }
        break;
        case EOS_Sessions_SessionInviteReceivedCallbackInfo::k_iCallback:
        {
            EOS_Sessions_SessionInviteReceivedCallbackInfo& callback = res->GetCallback<EOS_Sessions_SessionInviteReceivedCallbackInfo>();
            delete[]callback.InviteId;
        }
        break;
        case EOS_Sessions_SessionInviteAcceptedCallbackInfo::k_iCallback:
        {
            EOS_Sessions_SessionInviteAcceptedCallbackInfo& callback = res->GetCallback<EOS_Sessions_SessionInviteAcceptedCallbackInfo>();
            delete[]callback.SessionId;
        }
        break;
    }
}

} // namespace sdk
