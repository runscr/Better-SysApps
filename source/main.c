#include <string.h>

#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>

#include <whb/log.h>
#include <whb/log_udp.h>
#include <coreinit/title.h>
#include <sysapp/title.h>
#include <sysapp/switch.h>
#include <gx2/surface.h>
#include <memory/mappedmemory.h>

#include "buttons.h"

WUPS_PLUGIN_NAME("Better Settings");
WUPS_PLUGIN_DESCRIPTION("An aroma plugin that improves the System Settings user experience!");
WUPS_PLUGIN_VERSION("v1.1");
WUPS_PLUGIN_AUTHOR("Fangal");
WUPS_PLUGIN_LICENSE("GPLv3");

#define MIRROR_SCREENS_CONFIG_ID "mirrorScreensConfig"
#define INPUT_REDIRECTION_CONFIG_ID "inputRedirectionConfig"

WUPS_USE_STORAGE("Better_Settings");

bool mirrorScreensConfig = true;
bool inputRedirectionConfig = true;
bool patchDisplay = false;
bool patchInput = false;
bool noDRC = false;

uint64_t hsTID = 0;
uint64_t currentTID = 0;

// Determines which patches are active at runtime
void calcRuntimePatches(uint64_t titleID) {
    // Checks if title is a system app or the H&S app.
    // Patches are disabled for H&S as homebrew runs under that title id.
    bool isHSApp = (titleID == hsTID);
    bool isSysApp = (titleID & 0xFFFFFFFF'00000000ULL) == 0x00050010'00000000ULL;

    if (isSysApp && !isHSApp) {
        if (mirrorScreensConfig) {
            patchDisplay = true;
        } else {
            patchDisplay = false;
        }

        if (inputRedirectionConfig) {
            patchInput = true;
        } else {
            patchInput = false;
        }
    } else {
        patchDisplay = false;
        patchInput = false;
    }
}

// Uses KPAD to poll the specified WPAD channel and runs calcKPAD if data is valid
void pollKPAD(WPADChan chan) {
    if (patchInput) {
        KPADStatus kStatus;
        KPADError kErr;
        KPADReadEx(chan, &kStatus, 1, &kErr);
        if (kErr == KPAD_ERROR_OK) {
            calcKPAD(&kStatus);
        }
    }
}

void mirrorScreensConfigChanged(ConfigItemBoolean *item, bool newVal) {
    mirrorScreensConfig = newVal;
    WUPSStorageAPI_StoreBool(NULL, MIRROR_SCREENS_CONFIG_ID, mirrorScreensConfig);
}

void inputRedirectionConfigChanged(ConfigItemBoolean *item, bool newVal) {
    inputRedirectionConfig = newVal;
    WUPSStorageAPI_StoreBool(NULL, INPUT_REDIRECTION_CONFIG_ID, inputRedirectionConfig);
}

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle root) {
    patchDisplay = false;
    patchInput = false;

    if (WUPSConfigItemBoolean_AddToCategory(root, MIRROR_SCREENS_CONFIG_ID, "Mirror Gamepad screen to the TV", true, mirrorScreensConfig, &mirrorScreensConfigChanged) != WUPSCONFIG_API_RESULT_SUCCESS) {
        WHBLogPrintf("Failed to add item");
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    if (WUPSConfigItemBoolean_AddToCategory(root, INPUT_REDIRECTION_CONFIG_ID, "Redirect inputs", true, inputRedirectionConfig, &inputRedirectionConfigChanged) != WUPSCONFIG_API_RESULT_SUCCESS) {
        WHBLogPrintf("Failed to add item");
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    } 

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void ConfigMenuClosedCallback() {
    WUPSStorageAPI_SaveStorage(false);

    clearButtons();
    calcRuntimePatches(currentTID);
}

INITIALIZE_PLUGIN() {
    WHBLogUdpInit();
    WHBLogPrintf("Hola from Better Settings!");

    hsTID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY);

    WUPSConfigAPIOptionsV1 configOptions = {.name = "Better Settings"};
    if (WUPSConfigAPI_Init(configOptions, ConfigMenuOpenedCallback, ConfigMenuClosedCallback) != WUPSCONFIG_API_RESULT_SUCCESS) {
        WHBLogPrintf("Failed to init config api");
    }

    WUPSStorageError storageRes;
    if ((storageRes = WUPSStorageAPI_GetBool(NULL, MIRROR_SCREENS_CONFIG_ID, &mirrorScreensConfig)) == WUPS_STORAGE_ERROR_NOT_FOUND) {
        if (WUPSStorageAPI_StoreBool(NULL, MIRROR_SCREENS_CONFIG_ID, mirrorScreensConfig) != WUPS_STORAGE_ERROR_SUCCESS) {
            WHBLogPrintf("Failed to store bool");
        }
    }
    else if (storageRes != WUPS_STORAGE_ERROR_SUCCESS) {
        WHBLogPrintf("Failed to get bool %s (%d)", WUPSConfigAPI_GetStatusStr(storageRes), storageRes);
    }
    else {
        WHBLogPrintf("Successfully read the value from storage: %d %s (%d)", mirrorScreensConfig, WUPSConfigAPI_GetStatusStr(storageRes), storageRes);
    }    

    if ((storageRes = WUPSStorageAPI_GetBool(NULL, INPUT_REDIRECTION_CONFIG_ID, &inputRedirectionConfig)) == WUPS_STORAGE_ERROR_NOT_FOUND) {
        if (WUPSStorageAPI_StoreBool(NULL, INPUT_REDIRECTION_CONFIG_ID, inputRedirectionConfig) != WUPS_STORAGE_ERROR_SUCCESS) {
            WHBLogPrintf("Failed to store bool");
        }
    }
    else if (storageRes != WUPS_STORAGE_ERROR_SUCCESS) {
        WHBLogPrintf("Failed to get bool %s (%d)", WUPSConfigAPI_GetStatusStr(storageRes), storageRes);
    }
    else {
        WHBLogPrintf("Successfully read the value from storage: %d %s (%d)", inputRedirectionConfig, WUPSConfigAPI_GetStatusStr(storageRes), storageRes);
    }        

    WUPSStorageAPI_SaveStorage(false);    
}

ON_APPLICATION_START() {
    currentTID = OSGetTitleID();
    calcRuntimePatches(currentTID);
}

DECL_FUNCTION(void, GX2CopyColorBufferToScanBuffer, GX2ColorBuffer *colorBuffer, GX2ScanTarget scan_target) {
    if (patchDisplay) {
        if (scan_target == GX2_SCAN_TARGET_DRC) {
            scan_target = GX2_SCAN_TARGET_TV | GX2_SCAN_TARGET_DRC;
        }
        
        // Proper image handling HEAVILY inspired from SwipSwapMe
        if (colorBuffer->surface.aa != GX2_AA_MODE1X) {
            // If AA is enabled, we need to resolve the AA buffer.
            GX2Surface tempSurface;
            tempSurface = colorBuffer->surface;
            tempSurface.aa = GX2_AA_MODE1X;
            GX2CalcSurfaceSizeAndAlignment(&tempSurface);

            tempSurface.image = MEMAllocFromMappedMemoryForGX2Ex(tempSurface.imageSize, tempSurface.alignment);
            if (tempSurface.image != NULL) {
                GX2ResolveAAColorBuffer(colorBuffer, &tempSurface, 0, 0);

                GX2Surface surfaceCpy = colorBuffer->surface;
                colorBuffer->surface = tempSurface;
                real_GX2CopyColorBufferToScanBuffer(colorBuffer, scan_target);
                colorBuffer->surface = surfaceCpy;

                MEMFreeToMappedMemory(tempSurface.image);
                tempSurface.image = NULL;
                return;
            }
            else {
                WHBLogPrintf("Failed to allocate %d bytes for resolving AA", tempSurface.imageSize);
            }
        }
    }
    real_GX2CopyColorBufferToScanBuffer(colorBuffer, scan_target);
}

DECL_FUNCTION(int32_t, VPADRead, VPADChan chan, VPADStatus *vStatus, uint32_t size, VPADReadError *err) {
    int32_t res = real_VPADRead(chan, vStatus, size, err);

    // For when a GamePad is not connected
    // Special thanks to Lynx64, Maschell, and the Cemu project for this!
    if (patchInput && err && *err != VPAD_READ_SUCCESS) {
        noDRC = true;
        *err = VPAD_READ_SUCCESS;
        res = 1;
        memset(vStatus, 0x00, sizeof(VPADStatus));

        // full battery
        vStatus->battery = 0xC0;

        // maximum volume
        vStatus->slideVolume = vStatus->slideVolumeEx = 0xFF;

        // default touch
        vStatus->tpNormal.validity = VPAD_INVALID_X | VPAD_INVALID_Y;
        vStatus->tpFiltered1.validity = VPAD_INVALID_X | VPAD_INVALID_Y;
        vStatus->tpFiltered2.validity = VPAD_INVALID_X | VPAD_INVALID_Y;
    } else {
        noDRC = false;
    }

    if (patchInput) {
        calcVPAD(vStatus);
    }

    return res;
}

DECL_FUNCTION(void, KPADInitEx, KPADUnifiedWpadStatus *buffer, uint32_t count) {
    real_KPADInitEx(buffer, count);

    // Setup KPAD polling for when new KPADStatus data is stored in the ring buffer
    KPADSetSamplingCallback(WPAD_CHAN_0, pollKPAD);
}

DECL_FUNCTION(int32_t, SYSDirectlySwitchTo, SysAppPFID pfid) {
    // Prevent amiibo settings from loading
    if (noDRC && pfid == SYSAPP_PFID_CABINETU) {
        return 0;
    }
    return real_SYSDirectlySwitchTo(pfid);
}

WUPS_MUST_REPLACE(GX2CopyColorBufferToScanBuffer, WUPS_LOADER_LIBRARY_GX2, GX2CopyColorBufferToScanBuffer);
WUPS_MUST_REPLACE_FOR_PROCESS(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead, WUPS_FP_TARGET_PROCESS_ALL);
WUPS_MUST_REPLACE_FOR_PROCESS(KPADInitEx, WUPS_LOADER_LIBRARY_PADSCORE, KPADInitEx, WUPS_FP_TARGET_PROCESS_ALL);
WUPS_MUST_REPLACE(SYSDirectlySwitchTo, WUPS_LOADER_LIBRARY_SYSAPP, _SYSDirectlySwitchTo);