#include "MaterialManager.h"
#include <stdexcept>

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
    std::vector<TextureHandle> texIndices, glm::vec4 baseColorFactor, glm::vec3 ormFactor,
    glm::vec3 emissiveFactor, float normalScale)
{
    Material newMat{};
    newMat.type = MaterialType::PBR;
    newMat.pipeline = pipelineIndex;

    // init with default textures for albedo/orm/normal/emissive
    newMat.texIndices[0] = 0;
    newMat.texIndices[1] = 1;
    newMat.texIndices[2] = 2;
    newMat.texIndices[3] = 3; 

    //TODO: All materials default to use one sampler for now
    newMat.samplerIndices[0] = newMat.samplerIndices[1] = newMat.samplerIndices[2] = newMat.samplerIndices[3] = 0;

    for (size_t i = 0; i < texIndices.size(); i++)
    {
        newMat.texIndices[i] = texIndices[i].index;
    }
    newMat.baseColorFactor = baseColorFactor;
    newMat.ormFactor = ormFactor;
    newMat.emissiveFactor = emissiveFactor;
    newMat.normalScale = normalScale;

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
        memcpy(matGPU.texIndices, mat.texIndices, sizeof(uint32_t)*4);
        memcpy(matGPU.samplerIndices, mat.samplerIndices, sizeof(uint32_t) * 4);

        matGPU.baseColorFactor = mat.baseColorFactor;
        matGPU.ormFactor = mat.ormFactor;
        matGPU.emissiveFactor = mat.emissiveFactor;
        matGPU.normalScale = mat.normalScale;

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
