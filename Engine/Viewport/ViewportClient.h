#pragma once

struct ID3D11Device;
class InputFrame;

class IViewportClient
{
public:
	IViewportClient() = default;
	virtual ~IViewportClient() = default;

	virtual void Initialize(ID3D11Device* device) = 0;
	virtual void Shutdown() noexcept = 0;
	virtual void Update(const InputFrame& frame, float DeltaTime) = 0;
};
