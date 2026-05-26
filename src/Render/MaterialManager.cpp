#include "MaterialManager.h"


MaterialManager::MaterialManager()
{
}

MaterialManager::~MaterialManager()
{
}

uint32_t MaterialManager::CreateUnlitMaterial(uint32_t pipelineIndex, glm::vec4 color)
{
    Material newMat{};
    newMat.type = MaterialType::Unlit;
    newMat.pipeline = pipelineIndex;
    newMat.baseColor = color;

    _materials.push_back(newMat);
    return _materials.size() - 1;
}

uint32_t MaterialManager::CreateBlinnPhongMaterial(uint32_t pipelineIndex, std::vector<uint32_t> texIndices, glm::vec4 color)
{
    Material newMat{};
    newMat.type = MaterialType::BlinnPhong;
    newMat.pipeline = pipelineIndex;
    for (size_t i = 0; i < texIndices.size(); i++)
    {
        newMat.texIndices[i] = texIndices[i];
    }
    newMat.baseColor = newMat.baseColor;

    _materials.push_back(newMat);
    return _materials.size()-1;
}

std::vector<MaterialGPU> MaterialManager::ConvertMaterialsToGPU()
{
    std::vector<MaterialGPU> matGPUs;
    matGPUs.reserve(_materials.size());
    for (auto& mat : _materials)
    {
        MaterialGPU matGPU;
        memcpy(matGPU.texIndices, mat.texIndices, sizeof(uint32_t)*4);
        matGPU.baseColor = mat.baseColor;
        matGPUs.push_back(matGPU);
    }

    return matGPUs;
}
