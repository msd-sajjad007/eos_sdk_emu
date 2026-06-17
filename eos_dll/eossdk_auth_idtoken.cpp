/*
 * EOS_Auth identity token stubs — SDK 1.14+
 *
 * CopyIdToken  : fills an EOS_Auth_IdToken with a fake-but-valid-looking JWT
 *                so games that inspect the token structure do not null-deref.
 * VerifyIdToken: always reports the token as valid (emulator trusts itself).
 */

#include "eossdk_auth.h"
#include "eossdk_platform.h"
#include "eos_client_api.h"
#include "settings.h"

namespace sdk
{

/**
 * EOS_Auth_CopyIdToken
 *
 * Produces a minimal EOS_Auth_IdToken.  The JsonWebToken field is set to a
 * placeholder string that is structurally a valid (but unsigned) JWT so games
 * that parse the header/payload segments do not crash.  The caller must
 * release the result with EOS_Auth_IdToken_Release.
 */
EOS_EResult EOSSDK_Auth::CopyIdToken(
    const EOS_Auth_CopyIdTokenOptions* Options,
    EOS_Auth_IdToken**                 OutIdToken)
{
    TRACE_FUNC();

    if (OutIdToken == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    *OutIdToken = nullptr;

    if (Options == nullptr || Options->AccountId == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    if (!_logged_in)
        return EOS_EResult::EOS_Auth_NotLoggedIn;

    // Build a fake JWT: base64url(header).base64url(payload).fakesig
    // Header: {"alg":"none","typ":"JWT"}
    // Payload: {"sub":"<accountid>","iss":"eos_emulator"}
    // We use static base64url encoded versions for determinism.
    static const char* k_fake_jwt =
        "eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0"
        "."
        "eyJzdWIiOiJlbXVsYXRvciIsImlzcyI6ImVvc19lbXVsYXRvciJ9"
        "."
        "emulator_signature";

    EOS_Auth_IdToken* token = new EOS_Auth_IdToken;
    token->ApiVersion = EOS_AUTH_IDTOKEN_API_LATEST;
    token->AccountId  = Options->AccountId;   // caller owns AccountId lifetime

    char* jwt_str = new char[strlen(k_fake_jwt) + 1];
    strcpy(jwt_str, k_fake_jwt);
    token->JsonWebToken = jwt_str;

    *OutIdToken = token;
    return EOS_EResult::EOS_Success;
}

/**
 * EOS_Auth_VerifyIdToken
 *
 * Always fires the callback with EOS_Success — the emulator self-validates.
 * Games use this to confirm the local player's identity; for LAN / offline
 * use-cases a positive result is the correct and safe behaviour.
 */
void EOSSDK_Auth::VerifyIdToken(
    const EOS_Auth_VerifyIdTokenOptions* Options,
    void*                                ClientData,
    const EOS_Auth_OnVerifyIdTokenCallback CompletionDelegate)
{
    TRACE_FUNC();

    if (CompletionDelegate == nullptr)
        return;

    pFrameResult_t res(new FrameResult);
    EOS_Auth_VerifyIdTokenCallbackInfo& vitci =
        res->CreateCallback<EOS_Auth_VerifyIdTokenCallbackInfo>((CallbackFunc)CompletionDelegate);

    vitci.ClientData = ClientData;

    if (Options == nullptr || Options->IdToken == nullptr)
    {
        vitci.ResultCode = EOS_EResult::EOS_InvalidParameters;
    }
    else
    {
        vitci.ResultCode = EOS_EResult::EOS_Success;
        // Populate identity fields the game may read
        vitci.AccountId               = Options->IdToken->AccountId;
        vitci.bIsApplicationOwner     = EOS_TRUE;
        vitci.ProductUserId           = GetEOS_Connect().get_myself()->first;
        // String fields: point at static literals — safe for the callback duration
        vitci.Platform                = "emulator";
        vitci.DeviceType              = "PC";
        vitci.ClientId                = "eos_emulator";
        vitci.ProductId               = Settings::Inst().productid.c_str();
        vitci.SandboxId               = Settings::Inst().sandboxid.c_str();
        vitci.DeploymentId            = Settings::Inst().deploymentid.c_str();
    }

    res->done = true;
    GetCB_Manager().add_callback(this, res);
}

} // namespace sdk
