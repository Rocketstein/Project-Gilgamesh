#include "Renderer.h"
#include "D3D11/D3D11Device.h"

bool Renderer::Initialize(RendererDesc desc)
{
	


	return true; // Return true if initialization is successful
}

bool Renderer::Resize(Extent2D extent)
{
	// Handle resizing logic here, not WndProc.
	if (extent.width == 0 || extent.height == 0) return true;

	// Unbind the old render target.
	// Release RTV and back-buffer references.
	// Resize swap-chain buffers.
	// Recreate the RTV.
	// Update the viewport.

	return true;
}