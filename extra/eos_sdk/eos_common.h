// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "eos_base.h"

// [FIX] Guard EOS_PAGINATION_API_LATEST to suppress redefinition warning
// when both eos_common.h and eos_common1.16.4.h are included in the same TU.
#ifndef EOS_PAGINATION_API_LATEST
#define EOS_PAGINATION_API_LATEST 1
#endif
