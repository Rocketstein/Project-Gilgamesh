#include "PassRegistrar.h"
#include "Engine/Render/D3D11/Shader/D3D11ShaderManager.h"

bool PassRegistrar::Prepare(ID3D11Device* device, ShaderManager& shaders)
{
	if (!device || !shaders.IsInitialized()) return false;

	for (auto& entry : entries_)
	{
		const PassDescriptor& desc = entry.descriptor;

		const auto vertexShader = shaders.LoadVertex(desc.vertexShader);
		if (!vertexShader) return false;

		std::optional<PixelShaderHandle> pixelShader;
		if (desc.pixelShader.has_value())
		{
			const auto loaded = shaders.LoadPixel(*desc.pixelShader);
			if (!loaded) return false;
			pixelShader = loaded.resource;
		}

		GraphicsPipelineDesc pipelineDesc{};
		pipelineDesc.vertexShader = vertexShader.resource;
		pipelineDesc.pixelShader = pixelShader;
		pipelineDesc.drawState = desc.drawState;

		if (FAILED(entry.pipeline.Initialize(device, shaders, pipelineDesc)))
        {
            return false;
        }
	}

	return true;
}
