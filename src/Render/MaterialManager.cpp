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
    newMat.baseColor = color;

    _materials.push_back(newMat);
    return RegisterName(name);
}

MaterialHandle MaterialManager::CreateBlinnPhongMaterial(const std::string& name, uint32_t pipelineIndex, std::vector<TextureHandle> texIndices, glm::vec4 color, float shininess)
{
    Material newMat{};
    newMat.type = MaterialType::BlinnPhong;
    newMat.pipeline = pipelineIndex;

    //TODO: Change this to something more readable
    newMat.texIndices[0] = newMat.texIndices[1] = newMat.texIndices[2] = newMat.texIndices[3] = 0; // Defaults to white 1x1 texture

    //TODO: All materials default to use one sampler for now
    newMat.samplerIndices[0] = newMat.samplerIndices[1] = newMat.samplerIndices[2] = newMat.samplerIndices[3] = 0;

    for (size_t i = 0; i < texIndices.size(); i++)
    {
        newMat.texIndices[i] = texIndices[i].index;
    }
    newMat.baseColor = color;
    newMat.shininess = shininess;

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
        matGPU.baseColor = mat.baseColor;
        matGPU.shininess = mat.shininess;
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
