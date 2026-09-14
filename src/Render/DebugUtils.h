#pragma once
#include "VulkanContext.h"
#include <type_traits>

#if defined(UTAH_USE_GPU_MARKERS) && UTAH_USE_GPU_MARKERS
inline constexpr bool kDebugMarkers = true;
#elif !defined(NDEBUG)
inline constexpr bool kDebugMarkers = true;
#else
inline constexpr bool kDebugMarkers = false;
#endif


inline void SetDebugName(const vk::raii::Device& device, vk::ObjectType type, uint64_t handle, const char* name)
{
    if (!kDebugMarkers) 
        return;

    vk::DebugUtilsObjectNameInfoEXT info{};
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = name;

    device.setDebugUtilsObjectNameEXT(info);
}


template <typename RAII>
void SetDebugName(const vk::raii::Device& device, const RAII& obj, const char* name)
{
    using Hpp = std::decay_t<decltype(*obj)>;
    SetDebugName(
        device, 
        Hpp::objectType,
        reinterpret_cast<uint64_t>(static_cast<typename Hpp::CType>(*obj)),
        name);
}

inline void SetDebugName(const vk::raii::Device& device, const AllocatedImage& img, const char* name)
{
    SetDebugName(device, vk::ObjectType::eImage, reinterpret_cast<uint64_t>(img.image), name);
}

inline void SetDebugName(const vk::raii::Device& device, const AllocatedBuffer& buf, const char* name)
{
    SetDebugName(device, vk::ObjectType::eBuffer, reinterpret_cast<uint64_t>(buf.buffer), name);
}

class ScopedGPULabel
{
public:
    ScopedGPULabel(const vk::raii::CommandBuffer& cmd, const char* name) : _cmd(cmd)
    {
        if (!kDebugMarkers) return;
        vk::DebugUtilsLabelEXT label{};
        label.pLabelName = name;               // (optional: set label.color[0..3] to color-code)
        _cmd.beginDebugUtilsLabelEXT(label);
    }
    ~ScopedGPULabel() { if (kDebugMarkers) _cmd.endDebugUtilsLabelEXT(); }
    ScopedGPULabel(const ScopedGPULabel&) = delete;
    ScopedGPULabel& operator=(const ScopedGPULabel&) = delete;
private:
    const vk::raii::CommandBuffer& _cmd;
};