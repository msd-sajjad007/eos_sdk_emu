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

#include "eossdk_platform.h"
#include "settings.h"

namespace sdk
{

EOSSDK_Platform::EOSSDK_Platform():
    _platform_init(false),
    _ticket_budget_in_milliseconds(0),

    _cb_manager       (nullptr),
    _network          (nullptr),
    _metrics          (nullptr),
    _auth             (nullptr),
    _connect          (nullptr),
    _ecom             (nullptr),
    _ui               (nullptr),
    _friends          (nullptr),
    _presence         (nullptr),
    _sessions         (nullptr),
    _lobby            (nullptr),
    _userinfo         (nullptr),
    _p2p              (nullptr),
    _playerdatastorage(nullptr),
    _achievements     (nullptr),
    _stats            (nullptr),
    _leaderboards     (nullptr)
{
    _cb_manager        = new Callback_Manager;
    _network           = new Network;
}

EOSSDK_Platform::~EOSSDK_Platform()
{
    Release();
    delete _network;
    delete _cb_manager;
}

EOSSDK_Platform& EOSSDK_Platform::Inst()
{
    static EOSSDK_Platform instance;
    return instance;
}

void EOSSDK_Platform::Init(const EOS_Platform_Options* Options)
{
    GLOBAL_LOCK();
    if(!_platform_init)
    {
        if (Options != nullptr)
        {
            _api_version = Options->ApiVersion;

            // For any API version newer than what we know about, treat it as
            // the highest version we support (API_014) and fall through safely.
            // This prevents abort() crashes on newer game builds.
            int32_t effective_version = Options->ApiVersion;
            if (effective_version > EOS_PLATFORM_OPTIONS_API_014)
            {
                APP_LOG(Log::LogLevel::WARN,
                    "EOS_Platform_Create: unknown API version %d, treating as %d",
                    Options->ApiVersion, EOS_PLATFORM_OPTIONS_API_014);
                effective_version = EOS_PLATFORM_OPTIONS_API_014;
            }

            switch (effective_version)
            {
                case EOS_PLATFORM_OPTIONS_API_014:
                {
                    auto pf = reinterpret_cast<const EOS_Platform_Options014*>(Options);
                    // TaskNetworkTimeoutSeconds is optional and may be null — guard before deref
                    if (pf->TaskNetworkTimeoutSeconds != nullptr)
                        APP_LOG(Log::LogLevel::DEBUG, "TaskNetworkTimeoutSeconds = '%d'", *pf->TaskNetworkTimeoutSeconds);
                    else
                        APP_LOG(Log::LogLevel::DEBUG, "TaskNetworkTimeoutSeconds = (null/default)");
                }
                case EOS_PLATFORM_OPTIONS_API_013:
                case EOS_PLATFORM_OPTIONS_API_012:
                case EOS_PLATFORM_OPTIONS_API_011:
                {
                    auto pf = reinterpret_cast<const EOS_Platform_Options011*>(Options);
                    // RTCOptions and its ApiVersion field are both optional — guard both
                    if (pf->RTCOptions != NULL && pf->RTCOptions->ApiVersion != 0)
                        APP_LOG(Log::LogLevel::DEBUG, "RTCOptions.ApiVersion = '%d'", pf->RTCOptions->ApiVersion);
                }
                case EOS_PLATFORM_OPTIONS_API_010:
                case EOS_PLATFORM_OPTIONS_API_009:
                case EOS_PLATFORM_OPTIONS_API_008:
                case EOS_PLATFORM_OPTIONS_API_007:
                {
                    auto pf = reinterpret_cast<const EOS_Platform_Options007*>(Options);
                    if (pf->CacheDirectory != nullptr)
                        _ticket_budget_in_milliseconds = pf->TickBudgetInMilliseconds;

                    APP_LOG(Log::LogLevel::DEBUG, "TickBudgetInMilliseconds = '%d'", _ticket_budget_in_milliseconds);
                }
                case EOS_PLATFORM_OPTIONS_API_006:
                {
                    auto pf = reinterpret_cast<const EOS_Platform_Options006*>(Options);
                    if (pf->CacheDirectory != nullptr)
                        _cache_directory = pf->CacheDirectory;

                    APP_LOG(Log::LogLevel::DEBUG, "CacheDirectory = '%s'", _cache_directory.c_str());
                }
                case EOS_PLATFORM_OPTIONS_API_005:
                {
                    auto pf = reinterpret_cast<const EOS_Platform_Options005*>(Options);
                    if (pf->EncryptionKey != nullptr)
                        _encryption_key = pf->EncryptionKey;

                    if (pf->OverrideCountryCode != nullptr)
                        _override_country_code = pf->OverrideCountryCode;

                    if (pf->OverrideLocaleCode != nullptr)
                        _override_locale_code = pf->OverrideLocaleCode;

                    if (pf->DeploymentId != nullptr)
                        _deployment_id = pf->DeploymentId;

                    _flags = pf->Flags;

                    APP_LOG(Log::LogLevel::DEBUG, "EncryptionKey = '%s'", _encryption_key.c_str());
                    APP_LOG(Log::LogLevel::DEBUG, "OverrideCountryCode = '%s'", _override_country_code.c_str());
                    APP_LOG(Log::LogLevel::DEBUG, "OverrideLocaleCode = '%s'", _override_locale_code.c_str());
                    APP_LOG(Log::LogLevel::DEBUG, "DeploymentId = '%s'", _deployment_id.c_str());
                    APP_LOG(Log::LogLevel::DEBUG, "Flags = %llu", _flags);
                }
                case EOS_PLATFORM_OPTIONS_API_001:
                {
                    auto pf = reinterpret_cast<const EOS_Platform_Options001*>(Options);
                    _reserved = pf->Reserved;

                    if (pf->ProductId != nullptr)
                        _product_id = pf->ProductId;

                    if (pf->SandboxId != nullptr)
                        _sandbox_id = pf->SandboxId;

                    if (pf->ClientCredentials.ClientId != nullptr)
                        _client_id = pf->ClientCredentials.ClientId;

                    if (pf->ClientCredentials.ClientSecret != nullptr)
                        _client_secret = pf->ClientCredentials.ClientSecret;

                    _is_server = pf->bIsServer;

                    APP_LOG(Log::LogLevel::DEBUG, "ProductId = '%s'", _product_id.c_str());
                    APP_LOG(Log::LogLevel::DEBUG, "SandboxId = '%s'", _sandbox_id.c_str());
                    APP_LOG(Log::LogLevel::DEBUG, "ClientId = '%s'", _client_id.c_str());
                    APP_LOG(Log::LogLevel::DEBUG, "ClientSecret = '%s'", _client_secret.c_str());
                    APP_LOG(Log::LogLevel::DEBUG, "ApiVersion = %u", pf->ApiVersion);
                }
                break;

                default:
                    // Should never reach here due to version clamping above,
                    // but log a warning instead of aborting.
                    APP_LOG(Log::LogLevel::WARN, "EOS_Platform_Create: unhandled version %d after clamp, skipping version-specific init", effective_version);
                    break;
            }
        }

        _auth              = new EOSSDK_Auth;
        _friends           = new EOSSDK_Friends;
        _presence          = new EOSSDK_Presence;
        _connect           = new EOSSDK_Connect;
        _metrics           = new EOSSDK_Metrics;
        _ecom              = new EOSSDK_Ecom;
        _ui                = new EOSSDK_UI;
        _sessions          = new EOSSDK_Sessions;
        _lobby             = new EOSSDK_Lobby;
        _userinfo          = new EOSSDK_UserInfo;
        _p2p               = new EOSSDK_P2P;
        _playerdatastorage = new EOSSDK_PlayerDataStorage;
        _achievements      = new EOSSDK_Achievements;
        _stats             = new EOSSDK_Stats;
        _titlestorage      = new EOSSDK_TitleStorage;
        _leaderboards      = new EOSSDK_Leaderboards;

        _presence->setup_myself();
        _userinfo->setup_myself();

        _platform_init = true;
    }
}

void EOSSDK_Platform::Release()
{
    GLOBAL_LOCK();

    if (_platform_init)
    {
        delete _leaderboards;
        delete _titlestorage;
        delete _stats;
        delete _achievements;
        delete _playerdatastorage;
        delete _p2p;
        delete _userinfo;
        delete _lobby;
        delete _sessions;
        delete _presence;
        delete _friends;
        delete _ui;
        delete _ecom;
        delete _connect;
        delete _auth;
        delete _metrics;

        _platform_init = false;
    }
}

void EOSSDK_Platform::Tick()
{
    GLOBAL_LOCK();
    GetCB_Manager().set_max_tick_budget(_ticket_budget_in_milliseconds);
    GetCB_Manager().tick();
}

EOS_HMetrics EOSSDK_Platform::GetMetricsInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HMetrics>(_metrics);
}

EOS_HAuth EOSSDK_Platform::GetAuthInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HAuth>(_auth);
}

EOS_HConnect EOSSDK_Platform::GetConnectInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HConnect>(_connect);
}

EOS_HEcom EOSSDK_Platform::GetEcomInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HEcom>(_ecom);
}

EOS_HUI EOSSDK_Platform::GetUIInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HUI>(_ui);
}

EOS_HFriends EOSSDK_Platform::GetFriendsInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HFriends>(_friends);
}

EOS_HPresence EOSSDK_Platform::GetPresenceInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HPresence>(_presence);
}

EOS_HSessions EOSSDK_Platform::GetSessionsInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HSessions>(_sessions);
}

EOS_HLobby EOSSDK_Platform::GetLobbyInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HLobby>(_lobby);
}

EOS_HUserInfo EOSSDK_Platform::GetUserInfoInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HUserInfo>(_userinfo);
}

EOS_HP2P EOSSDK_Platform::GetP2PInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HP2P>(_p2p);
}

EOS_HPlayerDataStorage EOSSDK_Platform::GetPlayerDataStorageInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HPlayerDataStorage>(_playerdatastorage);
}

EOS_HTitleStorage EOSSDK_Platform::GetTitleStorageInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HTitleStorage>(_titlestorage);
}

EOS_HAchievements EOSSDK_Platform::GetAchievementsInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HAchievements>(_achievements);
}

EOS_HStats EOSSDK_Platform::GetStatsInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HStats>(_stats);
}

EOS_HLeaderboards EOSSDK_Platform::GetLeaderboardsInterface()
{
    GLOBAL_LOCK();
    return reinterpret_cast<EOS_HLeaderboards>(_leaderboards);
}

EOS_EResult EOSSDK_Platform::GetActiveCountryCode(EOS_EpicAccountId LocalUserId, char* OutBuffer, int32_t* InOutBufferLength)
{
    TRACE_FUNC();

    if (OutBuffer == nullptr || InOutBufferLength == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    if (*InOutBufferLength < (utils::static_strlen("") + 1))
        return EOS_EResult::EOS_LimitExceeded;

    strncpy(OutBuffer, "", utils::static_strlen("") + 1);

    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_Platform::GetActiveLocaleCode(EOS_EpicAccountId LocalUserId, char* OutBuffer, int32_t* InOutBufferLength)
{
    TRACE_FUNC();

    if (OutBuffer == nullptr || InOutBufferLength == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    if (*InOutBufferLength < (utils::static_strlen("en") + 1))
        return EOS_EResult::EOS_LimitExceeded;

    strncpy(OutBuffer, "en", utils::static_strlen("en") + 1);

    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_Platform::GetOverrideCountryCode(char* OutBuffer, int32_t* InOutBufferLength)
{
    TRACE_FUNC();

    if (OutBuffer == nullptr || InOutBufferLength == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    if (*InOutBufferLength < (_override_country_code.length() + 1))
    {
        *InOutBufferLength = _override_country_code.length() + 1;
        return EOS_EResult::EOS_LimitExceeded;
    }

    strncpy(OutBuffer, _override_country_code.c_str(), _override_country_code.length() + 1);

    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_Platform::GetOverrideLocaleCode(char* OutBuffer, int32_t* InOutBufferLength)
{
    TRACE_FUNC();

    if (OutBuffer == nullptr || InOutBufferLength == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    if (*InOutBufferLength < (_override_locale_code.length() + 1))
    {
        *InOutBufferLength = _override_locale_code.length() + 1;
        return EOS_EResult::EOS_LimitExceeded;
    }

    strncpy(OutBuffer, _override_locale_code.c_str(), _override_locale_code.length() + 1);

    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_Platform::SetOverrideCountryCode(const char* NewCountryCode)
{
    TRACE_FUNC();

    if (NewCountryCode == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    APP_LOG(Log::LogLevel::DEBUG, "%s", NewCountryCode);

    _override_country_code = NewCountryCode;

    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_Platform::SetOverrideLocaleCode(const char* NewLocaleCode)
{
    TRACE_FUNC();

    if (NewLocaleCode == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    APP_LOG(Log::LogLevel::DEBUG, "%s", NewLocaleCode);

    _override_locale_code = NewLocaleCode;

    return EOS_EResult::EOS_Success;
}

EOS_EResult EOSSDK_Platform::CheckForLauncherAndRestart()
{
    TRACE_FUNC();

    return EOS_EResult::EOS_NoChange;
}

}
