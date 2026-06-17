/*
 * EOS_Connect identity token stubs — SDK 1.14+
 *
 * CopyIdToken  : fills an EOS_Connect_IdToken with a fake-but-valid JWT.
 * VerifyIdToken: always fires success — the emulator self-validates.
 */

#include "eossdk_connect.h"
#include "eossdk_platform.h"
#include "eos_client_api.h"
#include "settings.h"

namespace sdk
{

/**
 * EOS_Connect_CopyIdToken
 *
 * Produces a minimal EOS_Connect_IdToken keyed to the local ProductUserId.
 * The JsonWebToken field is a placeholder that is structurally a valid
 * (but unsigned) JWT so games that parse header/payload do not crash.
 * The caller must release with EOS_Connect_IdToken_Release.
 */
EOS_EResult EOSSDK_Connect::CopyIdToken(
    const EOS_Connect_CopyIdTokenOptions* Options,
    EOS_Connect_IdToken**                 OutIdToken)
{
    TRACE_FUNC();

    if (OutIdToken == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    *OutIdToken = nullptr;

    if (Options == nullptr || Options->LocalUserId == nullptr)
        return EOS_EResult::EOS_InvalidParameters;

    auto it = get_user_by_productid(Options->LocalUserId);
    if (it == get_end_users())
        return EOS_EResult::EOS_NotFound;

    // Same deterministic fake JWT used by Auth::CopyIdToken
    static const char* k_fake_jwt =
        "eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0"
        "."
        "eyJzdWIiOiJlbXVsYXRvciIsImlzcyI6ImVvc19lbXVsYXRvciJ9"
        "."
        "emulator_signature";

    EOS_Connect_IdToken* token = new EOS_Connect_IdToken;
    token->ApiVersion   = EOS_CONNECT_IDTOKEN_API_LATEST;
    token->ProductUserId = Options->LocalUserId;  // caller owns lifetime

    char* jwt_str = new char[strlen(k_fake_jwt) + 1];
    strcpy(jwt_str, k_fake_jwt);
    token->JsonWebToken = jwt_str;

    *OutIdToken = token;
    return EOS_EResult::EOS_Success;
}

/**
 * EOS_Connect_VerifyIdToken
 *
 * Always fires EOS_Success.  For LAN / offline use the emulator trusts its
 * own tokens unconditionally — the same policy as Auth::VerifyIdToken.
 */
void EOSSDK_Connect::VerifyIdToken(
    const EOS_Connect_VerifyIdTokenOptions* Options,
    void*                                   ClientData,
    const EOS_Connect_OnVerifyIdTokenCallback CompletionDelegate)
{
    TRACE_FUNC();

    if (CompletionDelegate == nullptr)
        return;

    pFrameResult_t res(new FrameResult);
    EOS_Connect_VerifyIdTokenCallbackInfo& vitci =
        res->CreateCallback<EOS_Connect_VerifyIdTokenCallbackInfo>((CallbackFunc)CompletionDelegate);

    vitci.ClientData = ClientData;

    if (Options == nullptr || Options->IdToken == nullptr)
    {
        vitci.ResultCode = EOS_EResult::EOS_InvalidParameters;
    }
    else
    {
        vitci.ResultCode         = EOS_EResult::EOS_Success;
        vitci.ProductUserId      = Options->IdToken->ProductUserId;
        vitci.bIsApplicationOwner = EOS_TRUE;
        // String fields: static literals, valid for callback duration
        vitci.Platform            = "emulator";
        vitci.DeviceType          = "PC";
        vitci.ClientId            = "eos_emulator";
        vitci.ProductId           = Settings::Inst().productid.c_str();
        vitci.SandboxId           = Settings::Inst().sandboxid.c_str();
        vitci.DeploymentId        = Settings::Inst().deploymentid.c_str();
    }

    res->done = true;
    GetCB_Manager().add_callback(this, res);
}

} // namespace sdk
