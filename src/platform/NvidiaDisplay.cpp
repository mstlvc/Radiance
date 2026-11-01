#include "platform/NvidiaDisplay.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <vector>

namespace radiance::platform
{
    namespace
    {
        // The layouts below are copied from NVIDIA's nvapi.h (MIT licence, github.com/NVIDIA/nvapi)
        // field for field, with its 8-byte packing, so the sizes the driver checks in each
        // struct's version match. Enums are 32-bit, as in the header.
#pragma pack(push, 8)
        using NvU8 = unsigned char;
        using NvU16 = unsigned short;
        using NvU32 = unsigned long;
        using NvS32 = signed long;

        struct NV_TIMINGEXT
        {
            NvU32 flag;
            NvU16 rr;
            NvU32 rrx1k;
            NvU32 aspect;
            NvU16 rep;
            NvU32 status;
            NvU8 name[40];
        };

        struct NV_TIMING
        {
            NvU16 HVisible;
            NvU16 HBorder;
            NvU16 HFrontPorch;
            NvU16 HSyncWidth;
            NvU16 HTotal;
            NvU8 HSyncPol;
            NvU16 VVisible;
            NvU16 VBorder;
            NvU16 VFrontPorch;
            NvU16 VSyncWidth;
            NvU16 VTotal;
            NvU8 VSyncPol;
            NvU16 interlaced;
            NvU32 pclk;
            NV_TIMINGEXT etc;
        };

        struct NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO
        {
            NvU32 version;
            NvU32 rotation;       // NV_ROTATE
            NvU32 scaling;        // NV_SCALING
            NvU32 refreshRate1K;
            NvU32 interlaced : 1;
            NvU32 primary : 1;
            NvU32 reservedBit1 : 1;
            NvU32 disableVirtualModeSupport : 1;
            NvU32 isPreferredUnscaledTarget : 1;
            NvU32 reserved : 27;
            NvU32 connector;      // NV_GPU_CONNECTOR_TYPE
            NvU32 tvFormat;       // NV_DISPLAY_TV_FORMAT
            NvU32 timingOverride; // NV_TIMING_OVERRIDE
            NV_TIMING timing;
        };

        struct NV_DISPLAYCONFIG_PATH_TARGET_INFO_V2
        {
            NvU32 displayId;
            NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO* details;
            NvU32 targetId;
        };

        struct NV_RESOLUTION
        {
            NvU32 width;
            NvU32 height;
            NvU32 colorDepth;
        };

        struct NV_POSITION
        {
            NvS32 x;
            NvS32 y;
        };

        struct NV_DISPLAYCONFIG_SOURCE_MODE_INFO_V1
        {
            NV_RESOLUTION resolution;
            NvU32 colorFormat;          // NV_FORMAT
            NV_POSITION position;
            NvU32 spanningOrientation;  // NV_DISPLAYCONFIG_SPANNING_ORIENTATION
            NvU32 bGDIPrimary : 1;
            NvU32 bSLIFocus : 1;
            NvU32 reserved : 30;
        };

        struct NV_DISPLAYCONFIG_PATH_INFO_V2
        {
            NvU32 version;
            union
            {
                NvU32 sourceId;
                NvU32 reserved_sourceId;
            };
            NvU32 targetInfoCount;
            NV_DISPLAYCONFIG_PATH_TARGET_INFO_V2* targetInfo;
            NV_DISPLAYCONFIG_SOURCE_MODE_INFO_V1* sourceModeInfo;
            NvU32 IsNonNVIDIAAdapter : 1;
            NvU32 reserved : 31;
            void* pOSAdapterID;
        };

        struct NV_TIMING_FLAG
        {
            NvU32 isInterlaced : 4;
            NvU32 reserved0 : 12;
            union
            {
                NvU32 tvFormat : 8;
                NvU32 ceaId : 8;
                NvU32 nvPsfId : 8;
            };
            NvU32 scaling : 8;
        };

        struct NV_TIMING_INPUT
        {
            NvU32 version;
            NvU32 width;
            NvU32 height;
            float rr;
            NV_TIMING_FLAG flag;
            NvU32 type;  // NV_TIMING_OVERRIDE
        };

        struct NV_VIEWPORTF
        {
            float x;
            float y;
            float w;
            float h;
        };

        struct NV_CUSTOM_DISPLAY
        {
            NvU32 version;
            NvU32 width;
            NvU32 height;
            NvU32 depth;
            NvU32 colorFormat;  // NV_FORMAT
            NV_VIEWPORTF srcPartition;
            float xRatio;
            float yRatio;
            NV_TIMING timing;
            NvU32 hwModeSetOnly : 1;
        };
#pragma pack(pop)

        static_assert(sizeof(NV_TIMING) == 96);
        static_assert(sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO) == 128);
        static_assert(sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO_V2) == 24);
        static_assert(sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO_V1) == 32);
        static_assert(sizeof(NV_DISPLAYCONFIG_PATH_INFO_V2) == 48);
        static_assert(sizeof(NV_CUSTOM_DISPLAY) == 144);

        template <typename T>
        constexpr NvU32 Version(NvU32 v)
        {
            return static_cast<NvU32>(sizeof(T)) | (v << 16);
        }

        constexpr NvU32 kAdvancedTargetVer = Version<NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO>(1);
        constexpr NvU32 kPathInfoVer = Version<NV_DISPLAYCONFIG_PATH_INFO_V2>(2);
        constexpr NvU32 kTimingInputVer = Version<NV_TIMING_INPUT>(1);
        constexpr NvU32 kCustomDisplayVer = Version<NV_CUSTOM_DISPLAY>(1);

        constexpr NvU32 kValidateOnly = 0x1;
        constexpr NvU32 kSaveToPersistence = 0x2;
        constexpr NvU32 kFormatA8R8G8B8 = 21;
        // Timing formulas to try for a custom resolution, as NVIDIA Control Panel offers them:
        // CVT reduced blanking first (what most panels want), then CVT, GTF and automatic.
        constexpr NvU32 kTimingFormulas[] = { 6 /* CVT_RB */, 5 /* CVT */, 7 /* GTF */, 1 /* AUTO */ };

        constexpr int kOk = 0;

        using QueryInterface = void*(__cdecl*)(NvU32);
        using InitializeFn = int(__cdecl*)();
        using UnloadFn = int(__cdecl*)();
        using GetDisplayIdByNameFn = int(__cdecl*)(const char*, NvU32*);
        using GetDisplayConfigFn = int(__cdecl*)(NvU32*, NV_DISPLAYCONFIG_PATH_INFO_V2*);
        using SetDisplayConfigFn = int(__cdecl*)(NvU32, NV_DISPLAYCONFIG_PATH_INFO_V2*, NvU32);
        using GetTimingFn = int(__cdecl*)(NvU32, NV_TIMING_INPUT*, NV_TIMING*);
        using TryCustomFn = int(__cdecl*)(NvU32*, NvU32, NV_CUSTOM_DISPLAY*);
        using SaveCustomFn = int(__cdecl*)(NvU32*, NvU32, NvU32, NvU32);
        using RevertCustomFn = int(__cdecl*)(NvU32*, NvU32);
        using EnumCustomFn = int(__cdecl*)(NvU32, NvU32, NV_CUSTOM_DISPLAY*);

        // Everything the driver reports about the active paths, with the buffers its
        // pointers point into (so it must not be copied).
        struct Config
        {
            std::vector<NV_DISPLAYCONFIG_PATH_INFO_V2> paths;
            std::vector<std::vector<NV_DISPLAYCONFIG_PATH_TARGET_INFO_V2>> targets;
            std::vector<std::vector<NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO>> details;
            std::vector<NV_DISPLAYCONFIG_SOURCE_MODE_INFO_V1> sources;

            Config() = default;
            Config(const Config&) = delete;
            Config& operator=(const Config&) = delete;
        };
    }

    struct NvidiaDisplay::Api
    {
        UnloadFn unload = nullptr;
        GetDisplayIdByNameFn displayIdByName = nullptr;
        GetDisplayConfigFn getConfig = nullptr;
        SetDisplayConfigFn setConfig = nullptr;
        GetTimingFn getTiming = nullptr;
        TryCustomFn tryCustom = nullptr;
        SaveCustomFn saveCustom = nullptr;
        RevertCustomFn revertCustom = nullptr;
        EnumCustomFn enumCustom = nullptr;

        // The documented three passes: how many paths, how many targets each, then all of it.
        bool Read(Config& c) const
        {
            NvU32 count = 0;
            if (getConfig(&count, nullptr) != kOk || count == 0)
                return false;
            c.paths.assign(count, NV_DISPLAYCONFIG_PATH_INFO_V2{});
            for (NV_DISPLAYCONFIG_PATH_INFO_V2& p : c.paths)
                p.version = kPathInfoVer;
            if (getConfig(&count, c.paths.data()) != kOk)
                return false;
            c.paths.resize(count);
            c.targets.assign(count, {});
            c.details.assign(count, {});
            c.sources.assign(count, NV_DISPLAYCONFIG_SOURCE_MODE_INFO_V1{});
            for (NvU32 i = 0; i < count; ++i)
            {
                const NvU32 targets = c.paths[i].targetInfoCount;
                c.targets[i].assign(targets, NV_DISPLAYCONFIG_PATH_TARGET_INFO_V2{});
                c.details[i].assign(targets, NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO{});
                for (NvU32 t = 0; t < targets; ++t)
                {
                    c.details[i][t].version = kAdvancedTargetVer;
                    c.targets[i][t].details = &c.details[i][t];
                }
                c.paths[i].targetInfo = c.targets[i].data();
                c.paths[i].sourceModeInfo = &c.sources[i];
            }
            return getConfig(&count, c.paths.data()) == kOk;
        }
    };

    NvidiaDisplay::~NvidiaDisplay()
    {
        if (api_ && api_->unload)
            api_->unload();
        delete api_;
        if (module_)
            FreeLibrary(static_cast<HMODULE>(module_));
    }

    bool NvidiaDisplay::Available()
    {
        if (tried_)
            return api_ != nullptr;
        tried_ = true;
        module_ = LoadLibraryExW(L"nvapi64.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module_)
            return false;
        const auto query = reinterpret_cast<QueryInterface>(GetProcAddress(static_cast<HMODULE>(module_), "nvapi_QueryInterface"));
        if (!query)
            return false;
        // Ids from nvapi_interface.h in the same repository.
        const auto initialize = reinterpret_cast<InitializeFn>(query(0x0150E828));
        auto* api = new Api{};
        api->unload = reinterpret_cast<UnloadFn>(query(0xD22BDD7E));
        api->displayIdByName = reinterpret_cast<GetDisplayIdByNameFn>(query(0xAE457190));
        api->getConfig = reinterpret_cast<GetDisplayConfigFn>(query(0x11ABCCF8));
        api->setConfig = reinterpret_cast<SetDisplayConfigFn>(query(0x5D8CF8DE));
        api->getTiming = reinterpret_cast<GetTimingFn>(query(0x175167E9));
        api->tryCustom = reinterpret_cast<TryCustomFn>(query(0x1F7DB630));
        api->saveCustom = reinterpret_cast<SaveCustomFn>(query(0x49882876));
        api->revertCustom = reinterpret_cast<RevertCustomFn>(query(0xCBBD40F0));
        api->enumCustom = reinterpret_cast<EnumCustomFn>(query(0xA2072D59));
        const bool complete = initialize && api->displayIdByName && api->getConfig && api->setConfig && api->getTiming &&
                              api->tryCustom && api->saveCustom && api->revertCustom && api->enumCustom;
        if (!complete || initialize() != kOk)
        {
            api->unload = nullptr;  // never initialised: nothing to unload
            delete api;
            return false;
        }
        api_ = api;
        return true;
    }

    std::optional<unsigned> NvidiaDisplay::DisplayId(const std::string& device)
    {
        if (!Available())
            return std::nullopt;
        NvU32 id = 0;
        if (api_->displayIdByName(device.c_str(), &id) != kOk)
            return std::nullopt;
        return static_cast<unsigned>(id);
    }

    bool NvidiaDisplay::Drives(const std::string& device)
    {
        return DisplayId(device).has_value();
    }

    std::optional<int> NvidiaDisplay::Scaling(const std::string& device)
    {
        const std::optional<unsigned> id = DisplayId(device);
        Config c;
        if (!id || !api_->Read(c))
            return std::nullopt;
        for (size_t i = 0; i < c.paths.size(); ++i)
        {
            for (const NV_DISPLAYCONFIG_PATH_TARGET_INFO_V2& t : c.targets[i])
            {
                if (t.displayId == *id)
                    return static_cast<int>(t.details->scaling);
            }
        }
        return std::nullopt;
    }

    bool NvidiaDisplay::SetScaling(const std::string& device, int nvScaling, bool validateOnly)
    {
        const std::optional<unsigned> id = DisplayId(device);
        Config c;
        if (!id || !api_->Read(c))
            return false;
        bool found = false;
        for (size_t i = 0; i < c.paths.size(); ++i)
        {
            for (NV_DISPLAYCONFIG_PATH_TARGET_INFO_V2& t : c.targets[i])
            {
                if (t.displayId == *id)
                {
                    t.details->scaling = static_cast<NvU32>(nvScaling);
                    found = true;
                }
            }
        }
        if (!found)
            return false;
        const NvU32 flags = validateOnly ? kValidateOnly : kSaveToPersistence;
        lastStatus_ = api_->setConfig(static_cast<NvU32>(c.paths.size()), c.paths.data(), flags);
        return lastStatus_ == kOk;
    }

    bool NvidiaDisplay::Timing(const std::string& device, int width, int height, int refreshHz)
    {
        const std::optional<unsigned> id = DisplayId(device);
        if (!id)
            return false;
        for (const NvU32 formula : kTimingFormulas)
        {
            NV_TIMING_INPUT input{};
            input.version = kTimingInputVer;
            input.width = static_cast<NvU32>(width);
            input.height = static_cast<NvU32>(height);
            input.rr = static_cast<float>(refreshHz);
            input.type = formula;
            NV_TIMING timing{};
            if (api_->getTiming(*id, &input, &timing) == kOk)
                return true;
        }
        return false;
    }

    bool NvidiaDisplay::TryCustom(const std::string& device, int width, int height, int refreshHz)
    {
        std::optional<unsigned> id = DisplayId(device);
        if (!id)
            return false;
        for (const NvU32 formula : kTimingFormulas)
        {
            NV_TIMING_INPUT input{};
            input.version = kTimingInputVer;
            input.width = static_cast<NvU32>(width);
            input.height = static_cast<NvU32>(height);
            input.rr = static_cast<float>(refreshHz);
            input.type = formula;
            NV_CUSTOM_DISPLAY custom{};
            if (api_->getTiming(*id, &input, &custom.timing) != kOk)
                continue;
            custom.version = kCustomDisplayVer;
            custom.width = static_cast<NvU32>(width);
            custom.height = static_cast<NvU32>(height);
            custom.depth = 32;
            custom.colorFormat = kFormatA8R8G8B8;
            custom.srcPartition = NV_VIEWPORTF{ 0.0f, 0.0f, 1.0f, 1.0f };
            custom.xRatio = 1.0f;
            custom.yRatio = 1.0f;
            NvU32 ids[1] = { static_cast<NvU32>(*id) };
            lastStatus_ = api_->tryCustom(ids, 1, &custom);
            if (lastStatus_ == kOk)
                return true;
        }
        return false;
    }

    bool NvidiaDisplay::SaveCustom(const std::string& device)
    {
        const std::optional<unsigned> id = DisplayId(device);
        if (!id)
            return false;
        NvU32 ids[1] = { static_cast<NvU32>(*id) };
        return api_->saveCustom(ids, 1, 0, 0) == kOk;
    }

    bool NvidiaDisplay::RevertCustom(const std::string& device)
    {
        const std::optional<unsigned> id = DisplayId(device);
        if (!id)
            return false;
        NvU32 ids[1] = { static_cast<NvU32>(*id) };
        return api_->revertCustom(ids, 1) == kOk;
    }

    int NvidiaDisplay::CustomCount(const std::string& device)
    {
        const std::optional<unsigned> id = DisplayId(device);
        if (!id)
            return 0;
        int count = 0;
        for (NvU32 index = 0; index < 64; ++index)
        {
            NV_CUSTOM_DISPLAY custom{};
            custom.version = kCustomDisplayVer;
            if (api_->enumCustom(*id, index, &custom) != kOk)
                break;
            ++count;
        }
        return count;
    }
}
