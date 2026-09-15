#pragma once

#include <cstdint>

#include "Engine/Render/Types/Buffers/VertexBuffer.h"
#include "Engine/Render/Types/Buffers/IndexBuffer.h"

class StaticMesh final
{
public:
    ~StaticMesh() = default;
    StaticMesh(const StaticMesh&) = delete;
    StaticMesh& operator=(const StaticMesh&) = delete;
    StaticMesh(StaticMesh&&) noexcept = default;
    StaticMesh& operator=(StaticMesh&&) noexcept = default;

    void Bind(ID3D11DeviceContext* context) const noexcept;

    [[nodiscard]] std::uint32_t GetIndexCount() const noexcept { return indexBuffer_.GetCount(); }

private:
    StaticMesh(VertexBuffer&& vertexBuffer, IndexBuffer&& indexBuffer) noexcept;

    VertexBuffer vertexBuffer_;
    IndexBuffer indexBuffer_;

    friend class MeshManager;
};
