#pragma once
#include "EditorViewport.h"
#include "Editor/EditorInput/EditorController.h"
#include "Editor/EditorCamera/EditorCamera.h"
#include "Engine/Viewport/ViewportClient.h"

class EditorViewportClient : public IViewportClient
{
public:
	EditorViewportClient() = default;
	void Initialize(ID3D11Device* device) override;
	void Shutdown() noexcept override;
	void Update(const InputFrame& frame, float DeltaTime) override;

	// Accessors
	EditorViewport& GetEditorViewport() noexcept { return viewport_; }
	EditorCamera& GetEditorCamera() noexcept { return camera_; }
	EditorController& GetEditorController() noexcept { return controller_; }

private:
	EditorViewport viewport_;
	EditorCamera camera_;
	EditorController controller_;
};
