# New Files Compile Checklist

This file documents the compile-readiness of the 3 new `.cpp` files added
to `eos_dll/` in the `partyvan` branch. It can be deleted after the first
successful CI green build.

## Files added

| File | Purpose |
|------|---------|
| `eossdk_auth_idtoken.cpp` | `EOS_Auth_CopyIdToken` + `EOS_Auth_VerifyIdToken` impl |
| `eossdk_connect_idtoken.cpp` | `EOS_Connect_CopyIdToken` + `EOS_Connect_VerifyIdToken` impl |
| `eossdk_lobby_rtc.cpp` | `EOS_Lobby_GetRTCRoomName` + `IsRTCRoomConnected` + `AddNotifyRTCRoomConnectionChanged` + `RemoveNotifyRTCRoomConnectionChanged` impl |

## CMake registration

All 3 files are auto-included via:
```cmake
file(GLOB emu_sources eos_dll/*.cpp ...)
```
No changes to `CMakeLists.txt` are required.

## Header declarations

- `eossdk_auth.h` — `CopyIdToken` + `VerifyIdToken` declared under `sdk::EOSSDK_Auth`
- `eossdk_connect.h` — `CopyIdToken` + `VerifyIdToken` declared under `sdk::EOSSDK_Connect`
- `eossdk_lobby.h` — `GetRTCRoomName`, `IsRTCRoomConnected`, `AddNotifyRTCRoomConnectionChanged`, `RemoveNotifyRTCRoomConnectionChanged` declared under `sdk::EOSSDK_Lobby`

## Flat dispatch

- `eos_auth_flat.cpp` — exports `EOS_Auth_CopyIdToken`, `EOS_Auth_IdToken_Release`, `EOS_Auth_VerifyIdToken`
- `eos_connect_flat.cpp` — exports `EOS_Connect_CopyIdToken`, `EOS_Connect_IdToken_Release`, `EOS_Connect_VerifyIdToken`
- `eos_lobby_flat.cpp` — exports `EOS_Lobby_GetRTCRoomName`, `EOS_Lobby_IsRTCRoomConnected`, `EOS_Lobby_AddNotifyRTCRoomConnectionChanged`, `EOS_Lobby_RemoveNotifyRTCRoomConnectionChanged`

## Known-safe patterns used

- All callbacks use `pFrameResult_t` + `res->CreateCallback<T>` + `GetCB_Manager().add_callback` — identical to all other subsystem impls
- `Settings::Inst().productid/sandboxid/deploymentid` accessed as `const std::string` — `.c_str()` is safe for callback duration because Settings is a singleton that outlives callbacks
- `rcci.LobbyId = ""` uses a static string literal — no allocation, no leak
- `get_lobby_by_id(std::string(Options->LobbyId))` — explicit `std::string` construction matches method signature `get_lobby_by_id(std::string const&)`
