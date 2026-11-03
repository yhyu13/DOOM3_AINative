// Engine registration for the portable Q2 scene consumer (spec 004 T085).
//
// This is the only translation unit that includes the engine's RenderApi.h.
// It bridges the engine's value-only q2SceneSink_t to the portable
// EngineQ2SinkConsumer and installs it before R_InitRenderApi runs. The
// installer is a static object in the engine object library, so it is linked
// into DOOM3.exe/DOOM3XP.exe.
//
// R_InitRenderApi calls the installed initialize callback. That callback is
// where the missing Device/pipeline creation now happens: it asks
// EngineQ2DeviceRuntime to create the hardware device, compile the four classic
// pipelines and initialize EngineQ2DeviceConsumer, then switches the forwarding
// table to that consumer. The engine therefore creates real device objects from
// R_InitRenderApi without naming a single D3D12 type.
//
// The fork call sites (T080-T082/T044) fill the seam: ordered world geometry,
// per-stage texture identities and uploads, one light packet per shadow chain
// with its exclusion, one shadow-volume packet per chain, and a private skinned
// pose. This unit converts those values; the engine-side call sites live on the
// fork branch.
//
// With no device available, initialization is refused with an actionable reason
// instead of drawing an empty frame. The structural consumer remains the
// pre-initialize fallback, so a callback that arrives early still reports its
// own refusal rather than reaching a half-created device consumer.

#include "renderer/RenderApi.h"

#include "rhi/engine_q2_device_bootstrap.h"
#include "rhi/engine_q2_sink.h"

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

using doom3::rhi::EngineQ2DeviceRuntime;
using doom3::rhi::Q2SinkCallbacks;
using doom3::rhi::Q2SinkImageUpload;
using doom3::rhi::Q2SinkLightFacts;
using doom3::rhi::Q2SinkMaterialFacts;
using doom3::rhi::Q2SinkPrivatePose;
using doom3::rhi::Q2SinkShadowVolumePacket;
using doom3::rhi::Q2SinkSurfaceGeometry;
using doom3::rhi::Q2SinkTextureStageFacts;
using doom3::rhi::EngineQ2SinkConsumer;
using doom3::rhi::MotionVec3;
using doom3::rhi::Vertex;

// The structural consumer is the fallback a caller gets before a device exists.
// The device consumer replaces it during initialize and is shut down again on
// R_ShutdownRenderApi.
EngineQ2SinkConsumer g_structural;
std::unique_ptr<EngineQ2DeviceRuntime> g_runtime;
EngineQ2SinkConsumer* g_active = &g_structural;

const Q2SinkCallbacks* ActiveCallbacks() { return g_active->callbacks(); }


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

// Appends one engine surface to a portable mesh, baking its column-major
// model-to-world transform into the positions. The engine sends one snapshot
// per chain surface; the portable volume packet carries one concatenated mesh
// per pass, so the per-surface transform cannot be deferred.
bool AppendGeometry(const q2SurfaceGeometry_t& geometry,
                    Q2SinkSurfaceGeometry* out, std::string* error) {
    if (out == nullptr) {
        return false;
    }
    Q2SinkSurfaceGeometry one;
    if (!ToPortable(geometry, &one)) {
        if (error != nullptr) {
            *error = "a shadow-volume surface has an unsupported vertex or index layout";
        }
        return false;
    }
    const size_t base = out->vertices.size();
    if (base + one.vertices.size() > 0xFFFFu) {
        if (error != nullptr) {
            *error = "a shadow-volume chain exceeds the portable 16-bit index range";
        }
        return false;
    }
    const float* m = geometry.transform;
    for (Vertex vertex : one.vertices) {
        const float x = vertex.position[0];
        const float y = vertex.position[1];
        const float z = vertex.position[2];
        vertex.position[0] = m[0] * x + m[4] * y + m[8] * z + m[12];
        vertex.position[1] = m[1] * x + m[5] * y + m[9] * z + m[13];
        vertex.position[2] = m[2] * x + m[6] * y + m[10] * z + m[14];
        out->vertices.push_back(vertex);
    }
    out->indices.reserve(out->indices.size() + one.indices.size());
    for (uint16_t index : one.indices) {
        out->indices.push_back(static_cast<uint16_t>(base + index));
    }
    return true;
}

#if defined( Q2_SEAM_SHADOW_VOLUME )
bool ToPortable(const q2ShadowVolumePacket_t& packet,
                Q2SinkShadowVolumePacket* portable, std::string* error) {
    if (portable == nullptr) {
        return false;
    }
    *portable = Q2SinkShadowVolumePacket();
    portable->chainKind = packet.chainKind;
    std::memcpy(portable->tint, packet.tint, sizeof(portable->tint));

    if (packet.backCount <= 0 || packet.back == nullptr ||
        packet.frontCount <= 0 || packet.front == nullptr) {
        if (error != nullptr) {
            *error = "a shadow-volume packet carries no back or front geometry";
        }
        return false;
    }
    for (int i = 0; i < packet.backCount; ++i) {
        if (!AppendGeometry(packet.back[i], &portable->back, error)) {
            return false;
        }
    }
    for (int i = 0; i < packet.frontCount; ++i) {
        if (!AppendGeometry(packet.front[i], &portable->front, error)) {
            return false;
        }
    }
    for (int i = 0; i < packet.shadeCount && packet.shade != nullptr; ++i) {
        if (!AppendGeometry(packet.shade[i], &portable->shade, error)) {
            return false;
        }
    }
    return true;
}
#endif  // Q2_SEAM_SHADOW_VOLUME

void CopyReason(const std::string& text, char* reason, int reasonSize) {
    if (reason == nullptr || reasonSize <= 0) {
        return;
    }
    const size_t limit = static_cast<size_t>(reasonSize) - 1;
    const size_t count = text.size() < limit ? text.size() : limit;
    std::memcpy(reason, text.data(), count);
    reason[count] = '\0';
}

// The engine's initialize callback. This is the Device/pipeline creation that
// R_InitRenderApi performs on the D3D12 path; the concrete work lives in the
// portable EngineQ2DeviceRuntime because the engine cannot name D3D12 types.
bool BridgeInitialize(void* /*user*/, int width, int height, char* reason,
                      int reasonSize) {
    // A second initialize without a shutdown (a partial restart) retires the
    // previous device first rather than leaking it.
    if (g_runtime != nullptr) {
        g_runtime->Shutdown();
        g_runtime.reset();
        g_active = &g_structural;
    }

    std::string error;
    auto runtime = std::make_unique<EngineQ2DeviceRuntime>();
    if (!runtime->Create(width, height, &error)) {
        CopyReason(error.empty()
                       ? "the D3D12 classic device could not be created"
                       : error,
                   reason, reasonSize);
        return false;
    }
    g_runtime = std::move(runtime);
    g_active = &g_runtime->sink();
    if (reason != nullptr && reasonSize > 0) {
        reason[0] = '\0';
    }
    return true;
}

void BridgeShutdown(void* /*user*/) {
    if (g_runtime != nullptr) {
        g_runtime->Shutdown();
        g_runtime.reset();
    }
    g_active = &g_structural;
}

bool BridgeImageUpload(void* /*user*/, const q2ImageUpload_t* image) {
    if (image == nullptr) {
        return false;
    }
    const Q2SinkImageUpload portable =
        ToPortable(*image, g_active->desc().captureImageUploads);
    const Q2SinkCallbacks* callbacks = ActiveCallbacks();
    return callbacks->imageUpload(callbacks->user, &portable);
}

bool BridgeBeginView(void* /*user*/, unsigned int viewId, int x, int y,
                     int width, int height, bool overlay, bool worldGui,
                     const float projection[16]) {
    const Q2SinkCallbacks* callbacks = ActiveCallbacks();
    return callbacks->beginView(callbacks->user, viewId, x, y, width, height,
                                overlay, worldGui, projection);
}

bool BridgeSurfacePacket(void* /*user*/, unsigned int surfaceId,
                         const q2SurfaceGeometry_t* geometry) {
    if (geometry == nullptr) {
        return false;
    }
    Q2SinkSurfaceGeometry portable;
    if (!ToPortable(*geometry, &portable)) {
        return false;
    }
    const Q2SinkCallbacks* callbacks = ActiveCallbacks();
    return callbacks->surfacePacket(callbacks->user, surfaceId, &portable);
}

bool BridgeDrawPacket(void* /*user*/, unsigned int surfaceId,
                      const q2MaterialFacts_t* material, int vertexCount,
                      int indexCount, int depthHackKind, float modelDepthHack,
                      bool worldGui, bool postProcess) {
    if (material == nullptr) {
        return false;
    }
    const Q2SinkMaterialFacts portable = ToPortable(*material);
    const Q2SinkCallbacks* callbacks = ActiveCallbacks();
    return callbacks->drawPacket(callbacks->user, surfaceId, &portable,
                                 vertexCount, indexCount, depthHackKind,
                                 modelDepthHack, worldGui, postProcess);
}

bool BridgeLightPacket(void* /*user*/, const q2LightFacts_t* light) {
    if (light == nullptr) {
        return false;
    }
    const Q2SinkLightFacts portable = ToPortable(*light);
    const Q2SinkCallbacks* callbacks = ActiveCallbacks();
    return callbacks->lightPacket(callbacks->user, &portable);
}

#if defined( Q2_SEAM_SHADOW_VOLUME )
bool BridgeShadowVolumePacket(void* /*user*/,
                              const q2ShadowVolumePacket_t* packet) {
    if (packet == nullptr) {
        return false;
    }
    Q2SinkShadowVolumePacket portable;
    std::string error;
    if (!ToPortable(*packet, &portable, &error)) {
        return false;
    }
    return g_active->ShadowVolumePacket(portable);
}
#endif  // Q2_SEAM_SHADOW_VOLUME

bool BridgePrivatePose(void* /*user*/, const q2PrivatePose_t* pose) {
    if (pose == nullptr || pose->positions == nullptr || pose->vertexCount <= 0) {
        return false;
    }
    const Q2SinkPrivatePose portable = ToPortable(*pose);
    const Q2SinkCallbacks* callbacks = ActiveCallbacks();
    return callbacks->privatePose(callbacks->user, &portable);
}

bool BridgeEndView(void* /*user*/) {
    const Q2SinkCallbacks* callbacks = ActiveCallbacks();
    return callbacks->endView(callbacks->user);
}

bool BridgeSetColorTarget(void* /*user*/, int selector) {
    const Q2SinkCallbacks* callbacks = ActiveCallbacks();
    return callbacks->setColorTarget(callbacks->user, selector);
}

bool BridgeCopyToImage(void* /*user*/, unsigned int imageId,
                       const char* imageName) {
    const Q2SinkCallbacks* callbacks = ActiveCallbacks();
    return callbacks->copyToImage(callbacks->user, imageId, imageName);
}

bool BridgeSwapBuffers(void* /*user*/) {
    const Q2SinkCallbacks* callbacks = ActiveCallbacks();
    return callbacks->swapBuffers(callbacks->user);
}

q2SceneSink_t g_sink = {
    &g_structural,        BridgeInitialize,      BridgeShutdown,
    BridgeImageUpload,    BridgeBeginView,       BridgeSurfacePacket,
    BridgeDrawPacket,     BridgeLightPacket,
#if defined( Q2_SEAM_SHADOW_VOLUME )
    BridgeShadowVolumePacket,
#endif
    BridgePrivatePose,    BridgeEndView,         BridgeSetColorTarget,
    BridgeCopyToImage,    BridgeSwapBuffers,
};

struct Q2SceneSinkInstaller {
    Q2SceneSinkInstaller() { R_SetQ2SceneSink(&g_sink); }
};

Q2SceneSinkInstaller g_installer;

}  // namespace
