#include "EditorViewportClient.h"
#include "Engine/Input/InputFrame.h"

void EditorViewportClient::Initialize(ID3D11Device* device)
{
	viewport_.Initialize(device);
}

void EditorViewportClient::Shutdown() noexcept
{
	viewport_.Shutdown();
}

void EditorViewportClient::Update(const InputFrame& frame, float DeltaTime)
{
	const EditorCameraIntent intent = controller_.Update(frame);
	camera_.ApplyCameraIntent(intent, DeltaTime);
}