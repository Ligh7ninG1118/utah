#include "MaterialManager.h"
#include <stdexcept>
#include <optional>

MaterialManager::MaterialManager()
{
}

MaterialManager::~MaterialManager()
{
}

MaterialHandle MaterialManager::CreateUnlitMaterial(const std::string& name, uint32_t pipelineIndex, glm::vec4 color)
{
    Material newMat{};
    newMat.type = MaterialType::Unlit;
    newMat.pipeline = pipelineIndex;
    newMat.baseColorFactor = color;

    _materials.push_back(newMat);
    return RegisterName(name);
}

MaterialHandle MaterialManager::CreatePBRMaterial(const std::string& name, uint32_t pipelineIndex, 
    const PBRTextureSet& textures, glm::vec4 baseColorFactor, glm::vec3 ormFactor, glm::vec3 emissiveFactor,
    float normalScale, float alphaCutoff, bool isDoubleSided, AlphaMode alphaMode)
{
    Material newMat{};
    newMat.type = MaterialType::PBR;
    newMat.pipeline = pipelineIndex;

    // init with default textures for albedo/orm/normal/emissive
    newMat.texIndices[ToIdx(PBRSlot::BaseColor)] = 0;
    newMat.texIndices[ToIdx(PBRSlot::MetalRough)] = 1;
    newMat.texIndices[ToIdx(PBRSlot::Normal)] = 2;
    newMat.texIndices[ToIdx(PBRSlot::Emissive)] = 3;
    newMat.texIndices[ToIdx(PBRSlot::Occlusion)] = 4;

    // init to 0 for the empty tex slots
    for (uint32_t i = ToIdx(PBRSlot::Count); i < MAX_TEX_SLOTS; ++i)
        newMat.texIndices[i] = 0;

    // all materials default to use one sampler (repeat, aniso enabled)
    for (uint32_t& s : newMat.samplerIndices) 
        s = 0;

    auto set = [&](PBRSlot slot, const std::optional<TextureHandle>& h) 
        {
            if (h) 
                newMat.texIndices[ToIdx(slot)] = h->index;
        };
    set(PBRSlot::BaseColor, textures.baseColor);
    set(PBRSlot::MetalRough, textures.metalRough);
    set(PBRSlot::Normal, textures.normal);
    set(PBRSlot::Emissive, textures.emissive);
    set(PBRSlot::Occlusion, textures.occlusion);

    newMat.baseColorFactor = baseColorFactor;
    newMat.ormFactor = glm::vec4(ormFactor, 0.0f);
    newMat.emissiveFactor = glm::vec4(emissiveFactor, 0.0f);
    newMat.params = glm::vec4(
        normalScale, 
        alphaMode == AlphaMode::Mask ? alphaCutoff : 0.0f,  // currently Mask and Opaque shares one shader; guards against bad alpha channel
        0.0f, 0.0f); // reserved, empty fields
    newMat.isDoubleSided = isDoubleSided;
    newMat.alphaMode = alphaMode;

    _materials.push_back(newMat);
    return RegisterName(name);
}

std::vector<MaterialGPU> MaterialManager::ConvertMaterialsToGPU()
{
    std::vector<MaterialGPU> matGPUs;
    matGPUs.reserve(_materials.size());
    for (auto& mat : _materials)
    {
        MaterialGPU matGPU{};
        memcpy(matGPU.texIndices, mat.texIndices, sizeof(uint32_t) * MAX_TEX_SLOTS);
        memcpy(matGPU.samplerIndices, mat.samplerIndices, sizeof(uint32_t) * MAX_TEX_SLOTS);
        matGPU.baseColorFactor = mat.baseColorFactor;
        matGPU.ormFactor = mat.ormFactor;
        matGPU.emissiveFactor = mat.emissiveFactor;
        matGPU.params = mat.params;

        matGPUs.push_back(matGPU);
    }

    return matGPUs;
}

MaterialHandle MaterialManager::GetHandle(const std::string& name) const
{
    auto it = _nameMap.find(name);
    if (it == _nameMap.end())
        throw std::runtime_error("Unknown material name: " + name);
    return MaterialHandle{ it->second };
}

MaterialHandle MaterialManager::RegisterName(const std::string& name)
{
    uint32_t idx = static_cast<uint32_t>(_materials.size() - 1);
    auto [it, inserted] = _nameMap.emplace(name, idx);
    if (!inserted)
        throw std::runtime_error("Duplicate material name: " + name);
    return MaterialHandle{ idx };
}
