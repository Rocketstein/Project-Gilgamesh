#include "StaticMesh.h"

#include <utility>

StaticMesh::StaticMesh(
	VertexBuffer&& vertexBuffer,
	IndexBuffer&& indexBuffer) noexcept
	: vertexBuffer_(std::move(vertexBuffer)),
	  indexBuffer_(std::move(indexBuffer))
{
}

void StaticMesh::Bind(ID3D11DeviceContext* context) const noexcept
{
	vertexBuffer_.Bind(context);
	indexBuffer_.Bind(context);
}
