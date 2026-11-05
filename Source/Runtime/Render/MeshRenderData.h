#pragma once

#include "Containers/Array.h"
#include "Math/Box.h"
#include "Math/Sphere.h"

class UMaterial;
class FVertexBuffer;
class FIndexBuffer;

struct FCachedMeshSection
{
    UMaterial* Material;
    uint32 StartIndex;
    uint32 IndexCount;
};

struct FCachedMeshLOD
{
    FVertexBuffer* VertexBuffers[2];
    int32 VertexBufferCount = 0;
    FIndexBuffer* IndexBuffer = nullptr;
    uint32 FirstSection = 0;
    uint32 NumSections = 0;
};

// Render bindings resolved and owned by a scene proxy.
// Visibility and per-view LOD decisions are stored separately.
struct FMeshRenderState
{
    TArray<FCachedMeshSection> Sections;
    FCachedMeshLOD LODs[4];
    float LODThresholdSq[3]{};
    uint8 LODCount = 1;
    bool bUseSkinning = false;
};

struct FLODSelectionInput
{
    FLODSphere Sphere;
    const FMeshRenderState* State = nullptr;
};
