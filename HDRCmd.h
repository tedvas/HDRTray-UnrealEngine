// This is code taken from HDRCmd originally created by res2k, repository link: https://github.com/res2k/HDRTray

/*
 *  Copyright (C) 2005-2020 Team Kodi
 *
 *  This file is based on source code from Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>

UENUM(BlueprintType)
enum EWindowsHDRStatus
{
	WHDR_Unsupported = 0 UMETA(DisplayName = "Unsupported"), WHDR_Off = 1 UMETA(DisplayName = "Off"), WHDR_On = 2 UMETA(DisplayName = "On")
};

/// Display information
struct Display
{
	/// Display name
	std::wstring name;
	/// HDR status
	EWindowsHDRStatus status;
};

#if PLATFORM_WINDOWS && (!defined(NTDDI_WIN11_GA) || WDK_NTDDI_VERSION < NTDDI_WIN11_GA)
#ifndef HDR_H_
#define HDR_H_

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <tchar.h>

// Application instance
extern HINSTANCE hInst;

namespace WindowsHDR {
	EWindowsHDRStatus GetWindowsHDRStatus(bool GetOnlyPrimaryDisplay = true);
	EWindowsHDRStatus SetWindowsHDRStatus(bool enable, bool SetOnlyPrimaryDisplay = true);

} // namespace hdr

#endif // HDR_H_

#ifndef WINVERCHECK_HPP_
#define WINVERCHECK_HPP_

static bool IsWindows10BuildOrGreater(DWORD dwBuildNumber)
{
	OSVERSIONINFOEXW osvi = { sizeof(osvi), 0, 0, 0, 0, {0}, 0, 0 };
	DWORDLONG        const dwlConditionMask = VerSetConditionMask(
		VerSetConditionMask(
		VerSetConditionMask(
			0, VER_MAJORVERSION, VER_GREATER_EQUAL),
			   VER_MINORVERSION, VER_GREATER_EQUAL),
			   VER_BUILDNUMBER, VER_GREATER_EQUAL);

	osvi.dwMajorVersion = 10;
	osvi.dwMinorVersion = 0;
	osvi.dwBuildNumber = dwBuildNumber;

	return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, dwlConditionMask) != FALSE;
}

static bool IsWindows10_1709OrGreater ()
{
	return IsWindows10BuildOrGreater(16299);
}

static bool IsWindows10_1803OrGreater ()
{
	return IsWindows10BuildOrGreater(17134);
}

static bool IsWindows10_1903OrGreater ()
{
	return IsWindows10BuildOrGreater(18362);
}

static bool IsWindows11_24H2OrGreater ()
{
	return IsWindows10BuildOrGreater(26100);
}

#endif // WINVERCHECK_HPP_
#endif // PLATFORM_WINDOWS && (!defined(NTDDI_WIN11_GA) || WDK_NTDDI_VERSION < NTDDI_WIN11_GA)

#include "HDRCmd.generated.h"

UCLASS()
class UHDRCmd : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set Windows HDR Enabled"))
	static EWindowsHDRStatus SetWindowsHDREnabled(bool NewEnabled, bool SetOnlyPrimaryDisplay = true);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Toggle Windows HDR Enabled"))
	static EWindowsHDRStatus ToggleWindowsHDREnabled(bool SetOnlyPrimaryDisplay = true);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "Get Windows HDR Status"))
	static EWindowsHDRStatus GetWindowsHDRStatus(bool GetOnlyPrimaryDisplay = true);
};
