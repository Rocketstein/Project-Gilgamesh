#pragma once

#include "Engine/Render/D3D11/Shader/D3D11ShaderManager.h"

class IRenderPass
{
public:
    virtual bool Initialize(
        ID3D11Device* device,
        ShaderManager& shaders) = 0;

protected:

};