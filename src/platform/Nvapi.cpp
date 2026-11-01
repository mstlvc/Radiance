#include "platform/Nvapi.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>

namespace radiance::platform
{
    namespace
    {
        // NVAPI hands out its entry points by id through nvapi_QueryInterface.
        constexpr uint32_t kInitialize = 0x0150E828;
        constexpr uint32_t kUnload = 0xD22BDD7E;
        constexpr uint32_t kEnumNvidiaDisplayHandle = 0x9ABDD40D;
        constexpr uint32_t kGetAssociatedNvidiaDisplayName = 0x22A78B05;
        constexpr uint32_t kGetDVCInfoEx = 0x0E45002D;
        constexpr uint32_t kSetDVCLevelEx = 0x4A82C2B1;

        constexpr int kOk = 0;
        constexpr int kEndEnumeration = -7;

        struct DvcInfoEx
        {
            uint32_t version;
            int32_t currentLevel;
            int32_t minLevel;
            int32_t maxLevel;
            int32_t defaultLevel;
        };
        constexpr uint32_t kDvcInfoExVersion = static_cast<uint32_t>(sizeof(DvcInfoEx)) | (1u << 16);

        using QueryInterface = void*(__cdecl*)(uint32_t);
        using InitializeFn = int(__cdecl*)();
        using UnloadFn = int(__cdecl*)();
        using EnumDisplayFn = int(__cdecl*)(uint32_t index, void** handle);
        using DisplayNameFn = int(__cdecl*)(void* handle, char name[64]);
        using GetDvcFn = int(__cdecl*)(void* handle, uint32_t outputId, DvcInfoEx* info);
        using SetDvcFn = int(__cdecl*)(void* handle, uint32_t outputId, DvcInfoEx* info);
    }

    struct NvidiaVibrance::Functions
    {
        UnloadFn unload = nullptr;
        EnumDisplayFn enumDisplay = nullptr;
        DisplayNameFn displayName = nullptr;
        GetDvcFn getDvc = nullptr;
        SetDvcFn setDvc = nullptr;
    };

    NvidiaVibrance::~NvidiaVibrance()
    {
        if (fn_ && fn_->unload)
            fn_->unload();
        delete fn_;
        if (module_)
            FreeLibrary(static_cast<HMODULE>(module_));
    }

    NvidiaVibrance::State NvidiaVibrance::Initialise()
    {
        displays_.clear();
        if (!module_)
        {
            module_ = LoadLibraryExW(L"nvapi64.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!module_)
                return state_ = State::NoDriver;
            const auto query = reinterpret_cast<QueryInterface>(GetProcAddress(static_cast<HMODULE>(module_), "nvapi_QueryInterface"));
            if (!query)
                return state_ = State::NoDriver;
            const auto initialize = reinterpret_cast<InitializeFn>(query(kInitialize));
            fn_ = new Functions{};
            fn_->unload = reinterpret_cast<UnloadFn>(query(kUnload));
            fn_->enumDisplay = reinterpret_cast<EnumDisplayFn>(query(kEnumNvidiaDisplayHandle));
            fn_->displayName = reinterpret_cast<DisplayNameFn>(query(kGetAssociatedNvidiaDisplayName));
            fn_->getDvc = reinterpret_cast<GetDvcFn>(query(kGetDVCInfoEx));
            fn_->setDvc = reinterpret_cast<SetDvcFn>(query(kSetDVCLevelEx));
            if (!initialize || !fn_->enumDisplay || !fn_->getDvc || !fn_->setDvc || initialize() != kOk)
                return state_ = State::NoDriver;
        }
        if (!fn_)
            return state_ = State::NoDriver;
        for (uint32_t i = 0; i < 16; ++i)
        {
            void* handle = nullptr;
            const int status = fn_->enumDisplay(i, &handle);
            if (status == kEndEnumeration || status != kOk || !handle)
                break;
            displays_.push_back(handle);
        }
        return state_ = displays_.empty() ? State::NoDisplays : State::Ready;
    }

    std::string NvidiaVibrance::DeviceName(size_t display) const
    {
        if (display >= displays_.size() || !fn_ || !fn_->displayName)
            return {};
        char name[64]{};
        if (fn_->displayName(displays_[display], name) != kOk)
            return {};
        return name;
    }

    std::optional<VibranceLevel> NvidiaVibrance::Read(size_t display) const
    {
        if (display >= displays_.size() || !fn_)
            return std::nullopt;
        DvcInfoEx info{};
        info.version = kDvcInfoExVersion;
        if (fn_->getDvc(displays_[display], 0, &info) != kOk)
            return std::nullopt;
        return VibranceLevel{ info.currentLevel, info.minLevel, info.maxLevel, info.defaultLevel };
    }

    bool NvidiaVibrance::Write(size_t display, int level)
    {
        if (display >= displays_.size() || !fn_)
            return false;
        DvcInfoEx info{};
        info.version = kDvcInfoExVersion;
        if (fn_->getDvc(displays_[display], 0, &info) != kOk)
            return false;
        info.currentLevel = level < info.minLevel ? info.minLevel : (level > info.maxLevel ? info.maxLevel : level);
        return fn_->setDvc(displays_[display], 0, &info) == kOk;
    }
}
