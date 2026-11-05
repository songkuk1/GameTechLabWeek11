#pragma once
#include "Math/Transform.h"
#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include <array>
#include <utility>

class FPrimitiveSceneProxy;
inline constexpr uint32 InvalidObjectSlot = ~0u;

struct FGeometryBinding
{
    FVertexBuffer* VertexBuffers[2];
    int32 VertexBufferCount = 0;
    FIndexBuffer* IndexBuffer = nullptr;
    uint32 StartIndex = 0;
    uint32 IndexCount = 0; // Zero selects the full index buffer.

    void SetVertexBuffers(FVertexBuffer* const* InVertexBuffers, int32 InVertexBufferCount)
    {
        std::memcpy(VertexBuffers, InVertexBuffers, sizeof(FVertexBuffer*) * InVertexBufferCount);
        VertexBufferCount = InVertexBufferCount;
    }

    bool HasSameBuffers(FVertexBuffer* const* InVertexBuffers, int32 InVertexBufferCount, const FIndexBuffer* InIndexBuffer) const
    {
        return InVertexBufferCount == VertexBufferCount && IndexBuffer == InIndexBuffer && std::equal(VertexBuffers, VertexBuffers + VertexBufferCount, InVertexBuffers);
    }
};

struct FRenderPacket
{
    const FPrimitiveSceneProxy* Proxy = nullptr;
    const FMatrix* Model = nullptr;
    FGeometryBinding Geometry;
    UMaterial* Material = nullptr;
    const void* MaterialParamData = nullptr;
    float CameraToParticleDistance = 0.0f;
    uint32 MaterialParamDataSize = 0;
    uint32 Slot = InvalidObjectSlot;
    bool bOccludedByGpu = false;
};

// Shared bindings, with one ordinary indexed draw per item.
struct FStaticDrawItem
{
    const FPrimitiveSceneProxy* Proxy;
    uint32 Slot;
    uint32 StartIndex;
    uint32 IndexCount;
    uint32 bOccludedByGpu;
};

struct FStaticDrawGroup
{
    FGeometryBinding Geometry;
    UMaterial* Material = nullptr;
    std::vector<FStaticDrawItem> Items;
};

// Owns the current frame's billboard/particle matrices together with their packets.
// Blocks keep matrix addresses stable when either array grows.
class FRenderQueue : public TArray<FRenderPacket>
{
    using FPackets = TArray<FRenderPacket>;
    static constexpr uint32 MatricesPerBlock = 256;
    using FMatrixBlock = std::array<FMatrix, MatricesPerBlock>;
    TArray<TUniquePtr<FMatrixBlock>> MatrixBlocks;
    uint32 MatrixCount = 0;

public:
    FRenderQueue() = default;
    FRenderQueue(const FRenderQueue&) = delete;
    FRenderQueue& operator=(const FRenderQueue&) = delete;
    FRenderQueue(FRenderQueue&& Other) noexcept
        : FPackets(std::move(Other)), MatrixBlocks(std::move(Other.MatrixBlocks)),
          MatrixCount(std::exchange(Other.MatrixCount, 0)) 
    {
    }

    FRenderQueue& operator=(FRenderQueue&& Other) noexcept
    {
        if (this != &Other)
        {
            FPackets::operator=(std::move(Other));
            MatrixBlocks = std::move(Other.MatrixBlocks);
            MatrixCount = std::exchange(Other.MatrixCount, 0);
        }
        return *this;
    }

    const FMatrix* StoreWorldMatrix(const FMatrix& World)
    {
        const uint32 BlockIndex = MatrixCount / MatricesPerBlock;
        if (BlockIndex >= static_cast<uint32>(MatrixBlocks.Num()))
            MatrixBlocks.Add(MakeUnique<FMatrixBlock>());
        FMatrix& Stored = (*MatrixBlocks[BlockIndex])[MatrixCount % MatricesPerBlock];
        Stored = World;
        ++MatrixCount;
        return &Stored;
    }

    void Reset()
    {
        FPackets::Reset();
        MatrixCount = 0;
    }
};
