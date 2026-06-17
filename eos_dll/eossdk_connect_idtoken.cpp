/*
 * EOS_Connect identity token stubs — SDK 1.14+
 *
 * CopyIdToken  : fills an EOS_Connect_IdToken with a fake-but-valid JWT.
 * VerifyIdToken: always fires success — the emulator self-validates.
 */

#include "eossdk_connect.h"
#include "settings.h"

namespace sdk
{

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

    static const char* k_fake_jwt =
        "eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0"
        "."
        "eyJzdWIiOiJlbXVsYXRvciIsImlzcyI6ImVvc19lbXVsYXRvciJ9"
        "."
        "emulator_signature";

    EOS_Connect_IdToken* token = new EOS_Connect_IdToken;
    token->ApiVersion    = EOS_CONNECT_IDTOKEN_API_LATEST;
    token->ProductUserId = Options->LocalUserId;

    char* jwt_str = new char[strlen(k_fake_jwt) + 1];
    strcpy(jwt_str, k_fake_jwt);
    token->JsonWebToken = jwt_str;

    *OutIdToken = token;
    return EOS_EResult::EOS_Success;
}

void EOSSDK_Connect::VerifyIdToken(
    const EOS_Connect_VerifyIdTokenOptions*    Options,
    void*                                      ClientData,
    const EOS_Connect_OnVerifyIdTokenCallback  CompletionDelegate)
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
        vitci.ResultCode          = EOS_EResult::EOS_Success;
        vitci.ProductUserId       = Options->IdToken->ProductUserId;
        vitci.bIsApplicationOwner = EOS_TRUE;
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
