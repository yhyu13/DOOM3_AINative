// Engine registration for the portable Q2 scene consumer (spec 004 T085).
//
// This is the only translation unit that includes the engine's RenderApi.h.
// It bridges the engine's value-only q2SceneSink_t to the portable
// EngineQ2SinkConsumer and installs it before R_InitRenderApi runs. The
// installer is a static object in the engine object library, so it is linked
// into DOOM3.exe/DOOM3XP.exe.
//
// The fork call sites (T080-T082/T044) now fill the seam: ordered world
// geometry, per-stage texture identities and uploads, one light packet per
// shadow chain with its exclusion, and a private skinned pose. This unit only
// converts those values; the engine-side call sites live on the fork branch.
//
// With no classic frame backend configured, initialization is refused with an
// actionable reason instead of drawing an empty frame. The concrete backend is
// `rhi/d3d12/engine_q2_device_consumer.h` (EngineQ2D3D12Consumer): it owns the
// Device-backed SceneGeometryBackend, the per-image texture cache and the
// shadow-volume packet path. It is linked into doom3_rhi; the fork's
// R_InitRenderApi is the call site that will create the Device and the four
// pipelines and configure this installer with them.

#include "renderer/RenderApi.h"

#include "rhi/engine_q2_sink.h"

#include <cstring>
#include <vector>

namespace {

using doom3::rhi::Q2SinkImageUpload;
using doom3::rhi::Q2SinkLightFacts;
using doom3::rhi::Q2SinkMaterialFacts;
using doom3::rhi::Q2SinkPrivatePose;
using doom3::rhi::Q2SinkSurfaceGeometry;
using doom3::rhi::Q2SinkTextureStageFacts;
using doom3::rhi::EngineQ2SinkConsumer;
using doom3::rhi::MotionVec3;
using doom3::rhi::Vertex;

EngineQ2SinkConsumer g_consumer;

Q2SinkTextureStageFacts ToPortable(const q2TextureStageFacts_t& facts) {
    Q2SinkTextureStageFacts portable;
    portable.numStages = facts.numStages;
    const int count = facts.numStages < doom3::rhi::kQ2MaxStageTextures
                          ? facts.numStages
                          : doom3::rhi::kQ2MaxStageTextures;
    for (int i = 0; i < count; ++i) {
        portable.imageId[i] = facts.imageId[i];
        portable.filter[i] = facts.filter[i];
        portable.repeat[i] = facts.repeat[i];
        portable.hasImage[i] = facts.hasImage[i];
    }
    return portable;
}

Q2SinkMaterialFacts ToPortable(const q2MaterialFacts_t& facts) {
    Q2SinkMaterialFacts portable;
    portable.sort = facts.sort;
    portable.numStages = facts.numStages;
    portable.coverage = facts.coverage;
    portable.hasTexture = facts.hasTexture;
    portable.hasProgram = facts.hasProgram;
    portable.alphaTest = facts.alphaTest;
    portable.translucent = facts.translucent;
    portable.fogLight = facts.fogLight;
    portable.blendLight = facts.blendLight;
    portable.noShadows = facts.noShadows;
    portable.noSelfShadow = facts.noSelfShadow;
    portable.castsShadow = facts.castsShadow;
    portable.textures = ToPortable(facts.textures);
    return portable;
}

Q2SinkLightFacts ToPortable(const q2LightFacts_t& facts) {
    Q2SinkLightFacts portable;
    portable.fogLight = facts.fogLight;
    portable.blendLight = facts.blendLight;
    portable.pointLight = facts.pointLight;
    portable.parallel = facts.parallel;
    portable.hasProjection = facts.hasProjection;
    portable.noShadows = facts.noShadows;
    portable.falloffRadius = facts.falloffRadius;
    portable.numShadowFrustums = facts.numShadowFrustums;
    portable.globalShadows = facts.globalShadows;
    portable.localInteractions = facts.localInteractions;
    portable.localShadows = facts.localShadows;
    portable.globalInteractions = facts.globalInteractions;
    portable.translucentInteractions = facts.translucentInteractions;
    portable.chainKind = facts.chainKind;
    portable.chainSurfaceCount = facts.chainSurfaceCount;
    portable.chainMaterialNoShadows = facts.chainMaterialNoShadows;
    portable.chainMaterialNoSelfShadow = facts.chainMaterialNoSelfShadow;
    portable.chainSameEntity = facts.chainSameEntity;
    portable.excluded = facts.excluded;
    portable.exclusionKind = facts.exclusionKind;
    return portable;
}

bool ToPortable(const q2SurfaceGeometry_t& geometry,
                Q2SinkSurfaceGeometry* portable) {
    if (portable == nullptr || geometry.vertexCount <= 0 ||
        geometry.vertexData == nullptr || geometry.vertexStride <= 0) {
        return false;
    }
    // The engine describes its idDrawVert layout by offset/stride. The portable
    // Vertex is position + one texture coordinate; unsupported offsets are a
    // refusal, not a silently zeroed vertex.
    if (geometry.positionOffset < 0 ||
        geometry.positionOffset + static_cast<int>(3 * sizeof(float)) >
            geometry.vertexStride) {
        return false;
    }
    if (geometry.texCoordOffset >= 0 &&
        geometry.texCoordOffset + static_cast<int>(2 * sizeof(float)) >
            geometry.vertexStride) {
        return false;
    }

    portable->vertices.resize(static_cast<size_t>(geometry.vertexCount));
    for (int i = 0; i < geometry.vertexCount; ++i) {
        const unsigned char* base =
            geometry.vertexData + static_cast<size_t>(i) * geometry.vertexStride;
        Vertex& vertex = portable->vertices[static_cast<size_t>(i)];
        std::memcpy(vertex.position, base + geometry.positionOffset,
                    sizeof(vertex.position));
        if (geometry.texCoordOffset >= 0) {
            std::memcpy(vertex.texCoord, base + geometry.texCoordOffset,
                        sizeof(vertex.texCoord));
        }
    }

    if (geometry.indexCount > 0) {
        if (geometry.indexes == nullptr ||
            (geometry.indexSize != 2 && geometry.indexSize != 4)) {
            return false;
        }
        portable->indices.reserve(static_cast<size_t>(geometry.indexCount));
        for (int i = 0; i < geometry.indexCount; ++i) {
            const unsigned char* source =
                geometry.indexes + static_cast<size_t>(i) * geometry.indexSize;
            uint32_t value = 0;
            if (geometry.indexSize == 2) {
                uint16_t narrow = 0;
                std::memcpy(&narrow, source, sizeof(narrow));
                value = narrow;
            } else {
                std::memcpy(&value, source, sizeof(value));
            }
            // The public RHI vertex layout uses uint16_t indices. A larger
            // index means the surface must be split by the caller, not
            // silently truncated here.
            if (value > 0xFFFFu) {
                return false;
            }
            portable->indices.push_back(static_cast<uint16_t>(value));
        }
    }
    std::memcpy(portable->transform, geometry.transform,
                sizeof(portable->transform));
    return true;
}

Q2SinkImageUpload ToPortable(const q2ImageUpload_t& image, bool copyPixels) {
    Q2SinkImageUpload portable;
    portable.imageId = image.imageId;
    if (image.name != nullptr) {
        portable.name = image.name;
    }
    portable.width = image.width;
    portable.height = image.height;
    portable.mipCount = image.mipCount;
    portable.format = image.format;
    portable.sRGB = image.sRGB;
    portable.minFilter = image.minFilter;
    portable.magFilter = image.magFilter;
    portable.wrapS = image.wrapS;
    portable.wrapT = image.wrapT;
    portable.bytesPerRow = image.bytesPerRow;
    if (copyPixels && image.pixels != nullptr && image.bytesPerRow > 0 &&
        image.height > 0) {
        const size_t count =
            static_cast<size_t>(image.bytesPerRow) * image.height;
        portable.pixels.assign(image.pixels, image.pixels + count);
    }
    return portable;
}

Q2SinkPrivatePose ToPortable(const q2PrivatePose_t& pose) {
    Q2SinkPrivatePose portable;
    portable.viewId = pose.viewId;
    portable.viewKind = pose.viewKind;
    portable.surfaceId = pose.surfaceId;
    portable.generation = pose.generation;
    portable.positions.resize(static_cast<size_t>(pose.vertexCount));
    for (int i = 0; i < pose.vertexCount; ++i) {
        MotionVec3& target = portable.positions[static_cast<size_t>(i)];
        target.x = pose.positions[i * 3 + 0];
        target.y = pose.positions[i * 3 + 1];
        target.z = pose.positions[i * 3 + 2];
    }
    return portable;
}

bool BridgeInitialize(void* user, int width, int height, char* reason,
                      int reasonSize) {
    return g_consumer.callbacks()->initialize(user, width, height, reason,
                                              reasonSize);
}

void BridgeShutdown(void* user) {
    g_consumer.callbacks()->shutdown(user);
}

bool BridgeImageUpload(void* user, const q2ImageUpload_t* image) {
    if (image == nullptr) {
        return false;
    }
    const Q2SinkImageUpload portable =
        ToPortable(*image, g_consumer.desc().captureImageUploads);
    return g_consumer.callbacks()->imageUpload(user, &portable);
}

bool BridgeBeginView(void* user, unsigned int viewId, int x, int y, int width,
                     int height, bool overlay, bool worldGui,
                     const float projection[16]) {
    return g_consumer.callbacks()->beginView(user, viewId, x, y, width, height,
                                             overlay, worldGui, projection);
}

bool BridgeSurfacePacket(void* user, unsigned int surfaceId,
                         const q2SurfaceGeometry_t* geometry) {
    if (geometry == nullptr) {
        return false;
    }
    Q2SinkSurfaceGeometry portable;
    if (!ToPortable(*geometry, &portable)) {
        return false;
    }
    return g_consumer.callbacks()->surfacePacket(user, surfaceId, &portable);
}

bool BridgeDrawPacket(void* user, unsigned int surfaceId,
                      const q2MaterialFacts_t* material, int vertexCount,
                      int indexCount, int depthHackKind, float modelDepthHack,
                      bool worldGui, bool postProcess) {
    if (material == nullptr) {
        return false;
    }
    const Q2SinkMaterialFacts portable = ToPortable(*material);
    return g_consumer.callbacks()->drawPacket(
        user, surfaceId, &portable, vertexCount, indexCount, depthHackKind,
        modelDepthHack, worldGui, postProcess);
}

bool BridgeLightPacket(void* user, const q2LightFacts_t* light) {
    if (light == nullptr) {
        return false;
    }
    const Q2SinkLightFacts portable = ToPortable(*light);
    return g_consumer.callbacks()->lightPacket(user, &portable);
}

bool BridgePrivatePose(void* user, const q2PrivatePose_t* pose) {
    if (pose == nullptr || pose->positions == nullptr || pose->vertexCount <= 0) {
        return false;
    }
    const Q2SinkPrivatePose portable = ToPortable(*pose);
    return g_consumer.callbacks()->privatePose(user, &portable);
}

bool BridgeEndView(void* user) {
    return g_consumer.callbacks()->endView(user);
}

bool BridgeSetColorTarget(void* user, int selector) {
    return g_consumer.callbacks()->setColorTarget(user, selector);
}

bool BridgeCopyToImage(void* user, unsigned int imageId, const char* imageName) {
    return g_consumer.callbacks()->copyToImage(user, imageId, imageName);
}

bool BridgeSwapBuffers(void* user) {
    return g_consumer.callbacks()->swapBuffers(user);
}

q2SceneSink_t g_sink = {
    &g_consumer,          BridgeInitialize,    BridgeShutdown,
    BridgeImageUpload,    BridgeBeginView,     BridgeSurfacePacket,
    BridgeDrawPacket,     BridgeLightPacket,   BridgePrivatePose,
    BridgeEndView,        BridgeSetColorTarget, BridgeCopyToImage,
    BridgeSwapBuffers,
};

struct Q2SceneSinkInstaller {
    Q2SceneSinkInstaller() { R_SetQ2SceneSink(&g_sink); }
};

Q2SceneSinkInstaller g_installer;

}  // namespace
