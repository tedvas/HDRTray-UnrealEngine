// This is code taken from HDRCmd originally created by res2k, repository link: https://github.com/res2k/HDRTray

/*
 *  Copyright (C) 2005-2018 Team Kodi
 *
 *  This file is based on source code from Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "HDRCmd.h"

#if PLATFORM_WINDOWS && (!defined(NTDDI_WIN11_GA) || WDK_NTDDI_VERSION < NTDDI_WIN11_GA)
#include <cstdint>
#include <vector>
#include <wingdi.h>

extern "C" {
// From msmcs; undocumented, but exported by name
BOOL WINAPI InternalRefreshCalibration(LPCWSTR /* display name?  */, uintptr_t /* ??? */, const GUID* /* ??? */, const GUID* /* ??? */);
}

namespace WindowsHDR
{
    template<typename F> static void ForEachDisplay(F func)
    {
        uint32_t pathCount = 0;
        uint32_t modeCount = 0;

        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
        {
            return;
        }

        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);

        if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), 0) != ERROR_SUCCESS)
        {
            return;
        }

        for (const auto& path : paths)
        {
            const auto& mode = modes.at(path.targetInfo.modeInfoIdx);
            func(mode);
        }
    }
    
    static bool GetPrimaryDisplay(DISPLAYCONFIG_MODE_INFO& OutModeInfo, DISPLAYCONFIG_PATH_INFO& OutPathInfo)
    {
        UINT32 numPathArrayElements = 0;
        UINT32 numModeInfoArrayElements = 0;

        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, &numModeInfoArrayElements) != ERROR_SUCCESS)
        {
            return false;
        }

        TArray<DISPLAYCONFIG_PATH_INFO> pathArray;
        TArray<DISPLAYCONFIG_MODE_INFO> modeArray;
        pathArray.SetNum(numPathArrayElements);
        modeArray.SetNum(numModeInfoArrayElements);

        if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, pathArray.GetData(), &numModeInfoArrayElements, modeArray.GetData(), nullptr) != ERROR_SUCCESS)
        {
            return false;
        }

        for (UINT32 i = 0; i < numPathArrayElements; ++i)
        {
            if (pathArray[i].sourceInfo.statusFlags & DISPLAYCONFIG_SOURCE_IN_USE)
            {
                DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
                sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
                sourceName.header.size = sizeof(DISPLAYCONFIG_SOURCE_DEVICE_NAME);
                sourceName.header.adapterId = pathArray[i].sourceInfo.adapterId;
                sourceName.header.id = pathArray[i].sourceInfo.id;

                if (DisplayConfigGetDeviceInfo(&sourceName.header) == ERROR_SUCCESS)
                {
                    MONITORINFOEXW monInfo;
                    monInfo.cbSize = sizeof(MONITORINFOEXW);
                    POINT pt = { 0, 0 };
                    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
                    
                    if (GetMonitorInfoW(hMon, &monInfo))
                    {
                        if (wcscmp(sourceName.viewGdiDeviceName, monInfo.szDevice) == 0)
                        {
                            OutPathInfo = pathArray[i];
                            UINT32 modeIdx = pathArray[i].targetInfo.modeInfoIdx;
                            
                            if (modeIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID && modeIdx < numModeInfoArrayElements)
                            {
                                OutModeInfo = modeArray[modeIdx];
                                return true;
                            }
                        }
                    }
                }
            }
        }
        
        return false;
    }

    static EWindowsHDRStatus GetDisplayHDRStatus(const DISPLAYCONFIG_MODE_INFO& mode)
    {
        DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO getColorInfo = {};
        getColorInfo.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
        getColorInfo.header.size = sizeof(getColorInfo);
        getColorInfo.header.adapterId.HighPart = mode.adapterId.HighPart;
        getColorInfo.header.adapterId.LowPart = mode.adapterId.LowPart;
        getColorInfo.header.id = mode.id;

        if (DisplayConfigGetDeviceInfo(&getColorInfo.header) != ERROR_SUCCESS)
            return EWindowsHDRStatus::WHDR_Unsupported;

        if (!getColorInfo.advancedColorSupported)
            return EWindowsHDRStatus::WHDR_Unsupported;

        return getColorInfo.advancedColorEnabled ? EWindowsHDRStatus::WHDR_On : EWindowsHDRStatus::WHDR_Off;
    }

    EWindowsHDRStatus GetWindowsHDRStatus(bool GetOnlyPrimaryDisplay)
    {
        bool anySupported = false;
        bool anyEnabled = false;

        if (GetOnlyPrimaryDisplay == true)
        {
            DISPLAYCONFIG_MODE_INFO mode;
            DISPLAYCONFIG_PATH_INFO pathInfo;
            
            GetPrimaryDisplay(mode, pathInfo);
            
            EWindowsHDRStatus displayStatus = GetDisplayHDRStatus(mode);
            anySupported |= displayStatus != EWindowsHDRStatus::WHDR_Unsupported;
            anyEnabled |= displayStatus == EWindowsHDRStatus::WHDR_On;
        }
        else
        {
            ForEachDisplay([&](const DISPLAYCONFIG_MODE_INFO& mode)
            {
               EWindowsHDRStatus displayStatus = GetDisplayHDRStatus(mode);
               anySupported |= displayStatus != EWindowsHDRStatus::WHDR_Unsupported;
               anyEnabled |= displayStatus == EWindowsHDRStatus::WHDR_On;
           });
        }

        if (anySupported)
            return anyEnabled ? EWindowsHDRStatus::WHDR_On : EWindowsHDRStatus::WHDR_Off;
        else
            return EWindowsHDRStatus::WHDR_Unsupported;
    }

    static std::optional<EWindowsHDRStatus> SetDisplayHDRStatus(const DISPLAYCONFIG_MODE_INFO& mode, bool enable)
    {
        if (GetDisplayHDRStatus(mode) == EWindowsHDRStatus::WHDR_Unsupported)
            return std::nullopt;

        DISPLAYCONFIG_SET_ADVANCED_COLOR_STATE setColorState = {};
        setColorState.header.type = DISPLAYCONFIG_DEVICE_INFO_SET_ADVANCED_COLOR_STATE;
        setColorState.header.size = sizeof(setColorState);
        setColorState.header.adapterId.HighPart = mode.adapterId.HighPart;
        setColorState.header.adapterId.LowPart = mode.adapterId.LowPart;
        setColorState.header.id = mode.id;
        setColorState.enableAdvancedColor = enable;

        if (DisplayConfigSetDeviceInfo(&setColorState.header) != ERROR_SUCCESS)
            return std::nullopt;
        // Don't assume changing the HDR mode was successful... re-query the status
        return GetDisplayHDRStatus(mode);
    }

    EWindowsHDRStatus SetWindowsHDRStatus(bool enable, bool SetOnlyPrimaryDisplay)
    {
        TEnumAsByte<EWindowsHDRStatus> status;
        
        if (SetOnlyPrimaryDisplay == true)
        {
            DISPLAYCONFIG_MODE_INFO mode;
            DISPLAYCONFIG_PATH_INFO pathInfo;
            
            GetPrimaryDisplay(mode, pathInfo);
            
            auto new_status = SetDisplayHDRStatus(mode, enable);
                
            if (!new_status)
            {
                return status;
            }
                
            if(!status)
            {
                status = *new_status;
            }
            else
            {
                status = static_cast<EWindowsHDRStatus>(std::max(static_cast<int>(status), static_cast<int>(*new_status)));
            }
        }
        else
        {
            ForEachDisplay([&](const DISPLAYCONFIG_MODE_INFO& mode)
            {
                auto new_status = SetDisplayHDRStatus(mode, enable);
                
                if (!new_status)
                {
                    return;
                }
                
                if(!status)
                {
                    status = *new_status;
                }
                else
                {
                    status = static_cast<EWindowsHDRStatus>(std::max(static_cast<int>(status), static_cast<int>(*new_status)));
                }
            });
        }

        if (status)
            InternalRefreshCalibration(nullptr, 0, nullptr, nullptr);

        return status;
    }

    std::optional<EWindowsHDRStatus> ToggleHDRStatus()
    {
        auto status = GetWindowsHDRStatus();
        if (status == EWindowsHDRStatus::WHDR_Unsupported)
            return EWindowsHDRStatus::WHDR_Unsupported;
        return SetWindowsHDRStatus(status == EWindowsHDRStatus::WHDR_Off ? true : false);
    }

    static const wchar_t* GetFallbackDisplayName(const DISPLAYCONFIG_MODE_INFO& mode)
    {
        DISPLAYCONFIG_TARGET_BASE_TYPE target_base = {};
        target_base.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_BASE_TYPE;
        target_base.header.size = sizeof(target_base);
        target_base.header.adapterId.HighPart = mode.adapterId.HighPart;
        target_base.header.adapterId.LowPart = mode.adapterId.LowPart;
        target_base.header.id = mode.id;

        if (ERROR_SUCCESS == DisplayConfigGetDeviceInfo(&target_base.header)) {
            if ((target_base.baseOutputTechnology != DISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER)
                && (target_base.baseOutputTechnology & DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL))
                return L"Internal Display";
        }

        return L"Unnamed";
    }
} // namespace hdr

#endif

EWindowsHDRStatus UHDRCmd::SetWindowsHDREnabled(bool NewEnabled, bool SetOnlyPrimaryDisplay)
{
#if PLATFORM_WINDOWS && (!defined(NTDDI_WIN11_GA) || WDK_NTDDI_VERSION < NTDDI_WIN11_GA)
    return WindowsHDR::SetWindowsHDRStatus(NewEnabled, SetOnlyPrimaryDisplay);
#else
    return WHDR_Unsupported;
#endif
}

EWindowsHDRStatus UHDRCmd::ToggleWindowsHDREnabled(bool SetOnlyPrimaryDisplay)
{
#if PLATFORM_WINDOWS && (!defined(NTDDI_WIN11_GA) || WDK_NTDDI_VERSION < NTDDI_WIN11_GA)
    if (WindowsHDR::GetWindowsHDRStatus(SetOnlyPrimaryDisplay) == WHDR_Unsupported)
    {
        return WHDR_Unsupported;
    }
    
    if (WindowsHDR::GetWindowsHDRStatus(SetOnlyPrimaryDisplay) == WHDR_Off)
    {
        return WindowsHDR::SetWindowsHDRStatus(true, SetOnlyPrimaryDisplay);
    }
    else
    {
        return WindowsHDR::SetWindowsHDRStatus(false, SetOnlyPrimaryDisplay);
    }
#else
    return WHDR_Unsupported;
#endif
}

EWindowsHDRStatus UHDRCmd::GetWindowsHDRStatus(bool GetOnlyPrimaryDisplay)
{
#if PLATFORM_WINDOWS && (!defined(NTDDI_WIN11_GA) || WDK_NTDDI_VERSION < NTDDI_WIN11_GA)
    return WindowsHDR::GetWindowsHDRStatus(GetOnlyPrimaryDisplay);
#else
    return WHDR_Unsupported;
#endif
}
