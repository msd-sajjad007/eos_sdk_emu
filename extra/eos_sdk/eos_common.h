// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "eos_common1.16.4.h"

// [FIX] Guard EOS_PAGINATION_API_LATEST to suppress redefinition warning
// when both eos_common.h and eos_common1.16.4.h are included in the same TU.
// EOS_PAGINATION_API_LATEST is already defined inside eos_common1.16.4.h;
// this guard prevents a duplicate-macro warning on MSVC.
#ifndef EOS_PAGEQUERY_API_LATEST
#define EOS_PAGEQUERY_API_LATEST 1
#endif
