// Engine registration for the portable Q2 scene consumer (spec 004 T085).
//
// This is the only translation unit that includes the engine's RenderApi.h.
// It bridges the engine's value-only q2SceneSink_t to the portable
// EngineQ2SinkConsumer and installs it before R_InitRenderApi runs. The
// installer is a static object in the engine object library, so it is linked
// into DOOM3.exe/DOOM3XP.exe even though no fork call site names it yet.
//
// With no classic frame backend configured, initialization is refused with an
// actionable reason instead of drawing an empty frame. A future engine-fork
// call site supplies the geometry source and pipelines through the consumer.

#include "renderer/RenderApi.h"

#include "rhi/engine_q2_sink.h"

#include <cstring>

namespace {

using doom3::rhi::EngineQ2SinkConsumer;
using doom3::rhi::Q2SinkLightFacts;
using doom3::rhi::Q2SinkMaterialFacts;

EngineQ2SinkConsumer g_consumer;

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

bool BridgeBeginView(void* user, unsigned int viewId, int x, int y, int width,
                     int height, bool overlay, bool worldGui,
                     const float projection[16]) {
    return g_consumer.callbacks()->beginView(user, viewId, x, y, width, height,
                                             overlay, worldGui, projection);
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
    &g_consumer,        BridgeInitialize,  BridgeShutdown,
    BridgeBeginView,    BridgeDrawPacket,  BridgeLightPacket,
    BridgeEndView,      BridgeSetColorTarget, BridgeCopyToImage,
    BridgeSwapBuffers,
};

struct Q2SceneSinkInstaller {
    Q2SceneSinkInstaller() { R_SetQ2SceneSink(&g_sink); }
};

Q2SceneSinkInstaller g_installer;

}  // namespace
