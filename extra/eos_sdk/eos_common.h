// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "eos_common1.16.4.h"
#include "eos_common1.14.1.h"
#include "eos_common1.3.1.h"

// Backward compatibility: pre-1.5 SDK used EOS_AccountId instead of EOS_EpicAccountId.
// Old versioned headers (eos_auth_types1.3.1.h, eos_ecom_types1.1.0.h, etc.) still reference it.
#ifndef EOS_AccountId
typedef EOS_EpicAccountId EOS_AccountId;
#endif
