// T037 — classic material/light/shadow compatibility fixtures (spec 004).
//
// Covers the classification half of CP-02, CP-03, CP-04, CP-05, CP-06, CP-07, CP-08, CP-09,
// CP-10, CP-11, CP-12, CP-14, CP-15, CP-16, CP-19, CP-20, CP-21, CP-24. The drawn half
// (image parity) needs legal demo data and an owned program; it is recorded separately in
// evidence-q2.md and is not claimed here.
//
// CP-14/CP-15/CP-16 are structural here: a mirror/xray subview is a separate non-overlay
// view in the ordered stream, and a portal sky is one environment stage. Mirror recursion,
// portal-sky content, xray surface indexing and every image comparison remain NOT RUN.
//
// The program gate is the merged spec 008 resolver reading the shipping manifest, so the
// refusal of every non-owned classic program is exercised against the real manifest rather
// than a test double.

#include "q2_test_support.h"

#include "shader_resolver.h"

#include <fstream>
#include <string>
#include <vector>

namespace {

using doom3::rhi::CoverageClass;
using doom3::rhi::DrawClass;
using doom3::rhi::DrawSurfPacket;
using doom3::rhi::LightInput;
using doom3::rhi::LightKind;
using doom3::rhi::LightPlan;
using doom3::rhi::MaterialInput;
using doom3::rhi::MaterialPlan;
using doom3::rhi::MaterialShadowInput;
using doom3::rhi::ProgramGate;
using doom3::rhi::SceneAdapter;
using doom3::rhi::SceneAdapterDesc;
using doom3::rhi::SceneCommandKind;
using doom3::rhi::SceneCommandQueue;
using doom3::rhi::SceneExecuteResult;
using doom3::rhi::ShadowPolicy;
using doom3::rhi::ShadowSuppression;
using doom3::rhi::StageClass;
using doom3::rhi::Status;

struct GateContext {
    const d3::shader::Resolver* resolver = nullptr;
    d3::shader::Target target = d3::shader::Target::Fragment;
};

uint32_t GateThunk(void* user, const char* stageKey) {
    if (user == nullptr || stageKey == nullptr) {
        return 0;
    }
    const GateContext* context = static_cast<const GateContext*>(user);
    if (context->resolver == nullptr) {
        return 0;
    }
    return context->resolver->MayDraw(stageKey, context->target) ? 1u : 0u;
}

bool LoadManifest(d3::shader::Manifest* manifest) {
    std::ifstream file(Q2_MANIFEST_PATH, std::ios::binary);
    if (!file) {
        q2_test::Log("MANIFEST open failed: %s", Q2_MANIFEST_PATH);
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    std::string error;
    if (!d3::shader::Manifest::Parse(text, *manifest, error)) {
        q2_test::Log("MANIFEST parse failed: %s", error.c_str());
        return false;
    }
    return true;
}

int RunMaterialOrder(const ProgramGate& openGate) {
    MaterialInput input;
    input.name = "textures/base/wall";
    input.hasDiffuseMap = true;
    input.hasBumpMap = true;
    input.hasSpecularMap = true;
    MaterialPlan plan;
    std::string error;
    if (doom3::rhi::PlanMaterial(input, openGate, &plan, &error) != Status::Ok) {
        q2_test::Log("PLAN material failed: %s", error.c_str());
        return 1;
    }
    if (!q2_test::RequireEq(plan.stageCount, 3, "three stage runs")) return 1;
    if (!q2_test::Require(plan.stages[0].kind == StageClass::Bump, "bump is first")) return 1;
    if (!q2_test::Require(plan.stages[1].kind == StageClass::Diffuse, "diffuse is second")) {
        return 1;
    }
    if (!q2_test::Require(plan.stages[2].kind == StageClass::Specular, "specular is third")) {
        return 1;
    }
    if (!q2_test::Require(plan.coverage == CoverageClass::Opaque, "no alpha test is opaque")) {
        return 1;
    }
    if (!q2_test::Require(plan.refusal == nullptr, "a gated material is not refused")) return 1;
    q2_test::Log("MATERIAL order: bump > diffuse > specular");
    return 0;
}

int RunAlphaAndBlend(const ProgramGate& openGate) {
    MaterialInput alpha;
    alpha.name = "textures/base/grate";
    alpha.hasDiffuseMap = true;
    alpha.alphaTest = true;
    MaterialPlan alphaPlan;
    std::string error;
    if (doom3::rhi::PlanMaterial(alpha, openGate, &alphaPlan, &error) != Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(alphaPlan.coverage == CoverageClass::AlphaTested,
                          "alpha test is AlphaTested coverage")) {
        return 1;
    }
    if (!q2_test::Require(alphaPlan.drawClass == DrawClass::Opaque,
                          "alpha test stays in the opaque draw class")) {
        return 1;
    }

    MaterialInput blend;
    blend.name = "textures/base/glass";
    blend.hasDiffuseMap = true;
    blend.blend = true;
    MaterialPlan blendPlan;
    if (doom3::rhi::PlanMaterial(blend, openGate, &blendPlan, &error) != Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(blendPlan.coverage == CoverageClass::Translucent,
                          "blend is Translucent coverage")) {
        return 1;
    }
    if (!q2_test::Require(blendPlan.drawClass == DrawClass::Translucent,
                          "blend is the translucent draw class")) {
        return 1;
    }
    q2_test::Log("MATERIAL coverage: alphaTest=%s blend=%s",
                 doom3::rhi::ToString(alphaPlan.coverage),
                 doom3::rhi::ToString(blendPlan.coverage));
    return 0;
}

int RunPostAndMegaTexture(const ProgramGate& openGate) {
    MaterialInput post;
    post.name = "postprocess";
    post.postProcess = true;
    MaterialPlan postPlan;
    std::string error;
    if (doom3::rhi::PlanMaterial(post, openGate, &postPlan, &error) != Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(postPlan.drawClass == DrawClass::PostProcess,
                          "post-process draw class")) {
        return 1;
    }
    if (!q2_test::RequireEq(postPlan.sortKey, doom3::rhi::kPostProcessSort,
                            "post-process sort key")) {
        return 1;
    }

    MaterialInput mega;
    mega.name = "mega";
    mega.hasDiffuseMap = true;
    mega.megaTexture = true;
    MaterialPlan megaPlan;
    if (doom3::rhi::PlanMaterial(mega, openGate, &megaPlan, &error) != Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(megaPlan.usesMegaTexture, "megatexture is reported")) return 1;
    if (!q2_test::Require(megaPlan.refusal != nullptr, "megatexture is refused")) return 1;
    if (!q2_test::RequireEq(megaPlan.stageCount, 0, "megatexture draws no stage")) return 1;

    // Negative control: the refusal must not be a constant. A material with no
    // unsupported feature and a satisfied gate is drawable, so "refused" carries meaning.
    MaterialInput plain;
    plain.name = "plain";
    plain.hasDiffuseMap = true;
    MaterialPlan plainPlan;
    if (doom3::rhi::PlanMaterial(plain, openGate, &plainPlan, &error) != Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(plainPlan.refusal == nullptr && plainPlan.stageCount == 1,
                          "CONTROL a supported material is not refused")) {
        return 1;
    }
    q2_test::Log("MATERIAL megatexture refused: %s", megaPlan.refusal);
    return 0;
}

int RunResolverGateRefusesUnowned(const ProgramGate& manifestGate,
                                  const d3::shader::Manifest& manifest) {
    // The shipping manifest is the source of truth: no classic program is owned yet, so a
    // classic material must be refused rather than drawn from a stub.
    if (!q2_test::RequireEq(static_cast<uint32_t>(manifest.CountOf(d3::shader::Status::Stub)), 6,
                            "manifest stub count")) {
        return 1;
    }
    if (!q2_test::RequireEq(
            static_cast<uint32_t>(manifest.CountOf(d3::shader::Status::Unsupported)), 1,
            "manifest unsupported count")) {
        return 1;
    }
    if (!q2_test::RequireEq(
            static_cast<uint32_t>(manifest.CountOf(d3::shader::Status::Absent)), 7,
            "manifest absent count")) {
        return 1;
    }

    MaterialInput classic;
    classic.name = "textures/base/classic_wall";
    classic.hasDiffuseMap = true;
    classic.hasBumpMap = true;
    classic.hasSpecularMap = true;
    MaterialPlan plan;
    std::string error;
    if (doom3::rhi::PlanMaterial(classic, manifestGate, &plan, &error) != Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(plan.refusal != nullptr,
                          "a stub program must refuse the material")) {
        return 1;
    }
    if (!q2_test::RequireEq(plan.stageCount, 0, "a refused material submits no stage")) {
        return 1;
    }
    if (!q2_test::RequireEq(GateThunk(const_cast<void*>(
                                          static_cast<const void*>(manifestGate.user)),
                                      "interaction"),
                            0, "resolver gate refuses the stub interaction program")) {
        return 1;
    }
    q2_test::Log("MATERIAL resolver refusal: %s", plan.refusal);
    return 0;
}

int RunLightClassification() {
    std::string error;
    LightInput ambient;
    ambient.kind = LightKind::Ambient;
    LightPlan ambientPlan;
    if (doom3::rhi::PlanLight(ambient, MaterialShadowInput(), false, &ambientPlan, &error) !=
        Status::Ok) {
        q2_test::Log("PLAN ambient failed: %s", error.c_str());
        return 1;
    }
    if (!q2_test::Require(ambientPlan.ambientPass && !ambientPlan.interactionPass,
                          "ambient is its own pass, not an interaction")) {
        return 1;
    }

    LightInput projected;
    projected.kind = LightKind::Projected;
    projected.hasProjection = true;
    projected.falloffRadius = 128.0f;
    LightPlan projectedPlan;
    if (doom3::rhi::PlanLight(projected, MaterialShadowInput(), false, &projectedPlan, &error) !=
        Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(projectedPlan.interactionPass, "projected light interacts")) return 1;

    LightInput point = projected;
    point.kind = LightKind::Point;
    point.pointLight = true;
    point.multiFrustum = true;
    LightPlan pointPlan;
    if (doom3::rhi::PlanLight(point, MaterialShadowInput(), false, &pointPlan, &error) !=
        Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(pointPlan.multiFrustum, "point light splits across frusta")) return 1;

    LightInput noFalloff;
    noFalloff.kind = LightKind::Projected;
    noFalloff.hasProjection = true;
    noFalloff.falloffRadius = 0.0f;
    LightPlan badPlan;
    if (doom3::rhi::PlanLight(noFalloff, MaterialShadowInput(), false, &badPlan, &error) !=
        Status::InvalidArgument) {
        q2_test::Log("CONTROL a projected light with no falloff was accepted");
        return 1;
    }

    LightInput fog;
    fog.kind = LightKind::Fog;
    LightPlan fogPlan;
    if (doom3::rhi::PlanLight(fog, MaterialShadowInput(), false, &fogPlan, &error) !=
        Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(fogPlan.fogOrBlendPass && !fogPlan.interactionPass,
                          "fog is a separate pass")) {
        return 1;
    }
    LightInput blendLight;
    blendLight.kind = LightKind::Blend;
    LightPlan blendPlan;
    if (doom3::rhi::PlanLight(blendLight, MaterialShadowInput(), false, &blendPlan, &error) !=
        Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(blendPlan.fogOrBlendPass && !blendPlan.interactionPass,
                          "blend light is a separate pass")) {
        return 1;
    }
    q2_test::Log("LIGHT ambient/fog/blend are separate passes; projected and point interact");
    return 0;
}

int RunShadowChains() {
    std::string error;
    LightInput light;
    light.kind = LightKind::Projected;
    light.hasProjection = true;
    light.falloffRadius = 96.0f;

    LightPlan other;
    if (doom3::rhi::PlanLight(light, MaterialShadowInput(), false, &other, &error) != Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(other.firstChain == ShadowPolicy::Global &&
                              other.secondChain == ShadowPolicy::Local,
                          "non-self casters get global then local chains")) {
        return 1;
    }

    LightPlan self;
    if (doom3::rhi::PlanLight(light, MaterialShadowInput(), true, &self, &error) != Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(self.firstChain == ShadowPolicy::Local &&
                              self.secondChain == ShadowPolicy::None,
                          "a self caster keeps only the local chain")) {
        return 1;
    }

    MaterialShadowInput noShadows;
    noShadows.noShadows = true;
    LightPlan suppressed;
    if (doom3::rhi::PlanLight(light, noShadows, false, &suppressed, &error) != Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(suppressed.suppressed == ShadowSuppression::MaterialNoShadows &&
                              suppressed.firstChain == ShadowPolicy::None &&
                              suppressed.secondChain == ShadowPolicy::None,
                          "material noShadows emits no chain")) {
        return 1;
    }

    MaterialShadowInput noSelfShadow;
    noSelfShadow.noSelfShadow = true;
    LightPlan selfOnly;
    if (doom3::rhi::PlanLight(light, noSelfShadow, false, &selfOnly, &error) != Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(selfOnly.suppressed == ShadowSuppression::MaterialNoSelfShadow &&
                              selfOnly.firstChain == ShadowPolicy::Local &&
                              selfOnly.secondChain == ShadowPolicy::None,
                          "noSelfShadow keeps the local chain only")) {
        return 1;
    }

    LightInput dark = light;
    dark.noShadows = true;
    LightPlan lightSuppressed;
    if (doom3::rhi::PlanLight(dark, MaterialShadowInput(), false, &lightSuppressed, &error) !=
        Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(lightSuppressed.suppressed == ShadowSuppression::LightNoShadows &&
                              lightSuppressed.firstChain == ShadowPolicy::None,
                          "light noShadows emits no chain")) {
        return 1;
    }
    q2_test::Log("SHADOW chains: global+local, local-only for self, explicit suppressions");
    return 0;
}

int RunDepthHacks() {
    float weapon[16] = {0.500f, 0.000f, 0.000f, 0.000f,
                        0.000f, 0.700f, 0.000f, 0.000f,
                        0.000f, 0.000f, 1.200f, 0.000f,
                        0.000f, 0.000f, 3.400f, 1.000f};
    doom3::rhi::ApplyWeaponDepthHack(weapon);
    if (!q2_test::RequireNear(weapon[0], 0.500f, 1e-6f, "weapon hack leaves x scale")) return 1;
    if (!q2_test::RequireNear(weapon[5], 0.700f, 1e-6f, "weapon hack leaves y scale")) return 1;
    if (!q2_test::RequireNear(weapon[10], 0.300f, 1e-6f, "weapon hack scales the z term")) {
        return 1;
    }
    if (!q2_test::RequireNear(weapon[14], 0.850f, 1e-6f, "weapon hack scales the z offset")) {
        return 1;
    }

    float model[16] = {0.500f, 0.000f, 0.000f, 0.000f,
                       0.000f, 0.700f, 0.000f, 0.000f,
                       0.000f, 0.000f, 1.200f, 0.000f,
                       0.000f, 0.000f, 3.400f, 1.000f};
    doom3::rhi::ApplyModelDepthHack(model, 0.500f);
    if (!q2_test::RequireNear(model[0], 0.500f, 1e-6f, "model hack leaves x scale")) return 1;
    if (!q2_test::RequireNear(model[5], 0.700f, 1e-6f, "model hack leaves y scale")) return 1;
    if (!q2_test::RequireNear(model[10], 1.200f, 1e-6f, "model hack keeps the z scale")) {
        return 1;
    }
    if (!q2_test::RequireNear(model[14], 2.900f, 1e-6f, "model hack subtracts from z offset")) {
        return 1;
    }
    q2_test::Log("DEPTH HACKS move only the z term; x/y NDC unchanged");
    return 0;
}

// CP-14: a mirror is a subview rendered as its own view before the main view. It must not
// be reclassified as a display overlay, and its packets must keep their stream position.
int RunMirrorSubview(SceneAdapter& adapter) {
    SceneCommandQueue queue;
    queue.BeginView(q2_test::MakeSceneView(41, 1280, 720));  // mirror subview
    queue.Draw(q2_test::MakePacket(140, 2));
    queue.EndView();
    queue.BeginView(q2_test::MakeSceneView(42, 1280, 720));  // main view
    queue.Draw(q2_test::MakePacket(141, 3));
    queue.EndView();

    SceneExecuteResult result;
    std::string error;
    if (adapter.Execute(&queue, &result, &error) != Status::Ok) {
        q2_test::Log("CP-14 mirror subview execute failed: %s", error.c_str());
        return 1;
    }
    const std::vector<SceneCommandKind> expected = {
        SceneCommandKind::BeginView, SceneCommandKind::DrawPacket, SceneCommandKind::EndView,
        SceneCommandKind::BeginView, SceneCommandKind::DrawPacket, SceneCommandKind::EndView};
    if (!q2_test::RequireSequence(result.executed, expected,
                                  "CP-14 mirror then main")) {
        return 1;
    }
    if (!q2_test::RequireEq(result.views, 2, "CP-14 subview and main are two views")) return 1;
    if (!q2_test::RequireEq(result.overlays, 0, "CP-14 a mirror subview is not an overlay")) {
        return 1;
    }
    if (!q2_test::Require(result.viewIds.size() == 2 && result.viewIds[0] == 41 &&
                              result.viewIds[1] == 42,
                          "CP-14 the subview executes before the main view")) {
        return 1;
    }
    if (!q2_test::RequireEq(result.drawsExtracted, 2,
                            "CP-14 both subview and main draws are extracted")) {
        return 1;
    }
    q2_test::Log("CP-14 mirror subview: separate view, not an overlay");
    return 0;
}

// CP-15: a portal sky is a single environment stage with no sampled-texture upload. The
// manifest gate refuses the stub `environment` program, so the sky is not drawn from a stub.
int RunPortalSky(const ProgramGate& openGate, const ProgramGate& manifestGate) {
    MaterialInput sky;
    sky.name = "textures/sfx/portal_sky";
    sky.skybox = true;

    MaterialPlan plan;
    std::string error;
    if (doom3::rhi::PlanMaterial(sky, openGate, &plan, &error) != Status::Ok) {
        q2_test::Log("CP-15 portal sky plan failed: %s", error.c_str());
        return 1;
    }
    if (!q2_test::RequireEq(plan.stageCount, 1, "CP-15 portal sky is one stage")) return 1;
    if (!q2_test::Require(plan.stages[0].kind == StageClass::Skybox,
                          "CP-15 the stage is the skybox stage")) {
        return 1;
    }
    if (!q2_test::RequireEq(plan.stages[0].programId, 1, "CP-15 skybox stage has a program")) {
        return 1;
    }
    if (!q2_test::Require(plan.coverage == CoverageClass::Opaque,
                          "CP-15 the sky is opaque")) {
        return 1;
    }
    if (!q2_test::Require(plan.drawClass == DrawClass::Opaque,
                          "CP-15 the sky stays in the opaque draw class")) {
        return 1;
    }
    if (!q2_test::Require(!plan.needsTexture[0],
                          "CP-15 the portal sky is texgen, not a sampled upload")) {
        return 1;
    }
    if (!q2_test::Require(plan.refusal == nullptr, "CP-15 an open gate draws the sky")) {
        return 1;
    }

    MaterialPlan refused;
    if (doom3::rhi::PlanMaterial(sky, manifestGate, &refused, &error) != Status::Ok) {
        return 1;
    }
    if (!q2_test::Require(refused.refusal != nullptr,
                          "CP-15 the manifest refuses the stub environment program")) {
        return 1;
    }
    if (!q2_test::RequireEq(refused.stageCount, 0,
                            "CP-15 a refused sky submits no stage")) {
        return 1;
    }
    q2_test::Log("CP-15 portal sky: one environment stage; manifest refuses the stub");
    return 0;
}

// CP-16: an xray subview is also a separate non-overlay view. The portable material plan
// classifies its blended surface; per-surface xray indexing is an engine/image behaviour and
// remains NOT RUN.
int RunXraySubview(SceneAdapter& adapter, const ProgramGate& openGate) {
    MaterialInput xray;
    xray.name = "textures/sfx/xray";
    xray.blend = true;
    MaterialPlan plan;
    std::string error;
    if (doom3::rhi::PlanMaterial(xray, openGate, &plan, &error) != Status::Ok) {
        q2_test::Log("CP-16 xray material plan failed: %s", error.c_str());
        return 1;
    }
    if (!q2_test::Require(plan.coverage == CoverageClass::Translucent,
                          "CP-16 an xray surface is blended")) {
        return 1;
    }
    if (!q2_test::Require(plan.drawClass == DrawClass::Translucent,
                          "CP-16 an xray surface is in the translucent draw class")) {
        return 1;
    }

    SceneCommandQueue queue;
    queue.BeginView(q2_test::MakeSceneView(51, 1280, 720));  // xray subview
    DrawSurfPacket xrayPacket = q2_test::MakePacket(160, 1);
    xrayPacket.translucent = true;
    queue.Draw(xrayPacket);
    queue.EndView();
    queue.BeginView(q2_test::MakeSceneView(52, 1280, 720));  // main view
    queue.Draw(q2_test::MakePacket(161, 2));
    queue.EndView();

    SceneExecuteResult result;
    if (adapter.Execute(&queue, &result, &error) != Status::Ok) {
        q2_test::Log("CP-16 xray subview execute failed: %s", error.c_str());
        return 1;
    }
    const std::vector<SceneCommandKind> expected = {
        SceneCommandKind::BeginView, SceneCommandKind::DrawPacket, SceneCommandKind::EndView,
        SceneCommandKind::BeginView, SceneCommandKind::DrawPacket, SceneCommandKind::EndView};
    if (!q2_test::RequireSequence(result.executed, expected, "CP-16 xray then main")) {
        return 1;
    }
    if (!q2_test::RequireEq(result.views, 2, "CP-16 xray subview and main are two views")) {
        return 1;
    }
    if (!q2_test::RequireEq(result.overlays, 0, "CP-16 an xray subview is not an overlay")) {
        return 1;
    }
    if (!q2_test::Require(result.viewIds.size() == 2 && result.viewIds[0] == 51 &&
                              result.viewIds[1] == 52,
                          "CP-16 the xray subview executes before the main view")) {
        return 1;
    }
    if (!q2_test::RequireEq(result.drawsExtracted, 2,
                            "CP-16 both xray and main draws are extracted")) {
        return 1;
    }
    q2_test::Log("CP-16 xray subview: separate view; xray surface indexing NOT RUN");
    return 0;
}

}  // namespace

int main() {
    d3::shader::Manifest manifest;
    if (!LoadManifest(&manifest)) {
        return 1;
    }

    // An always-open gate isolates the material-plan rules from program ownership.
    ProgramGate openGate;
    openGate.user = nullptr;
    openGate.resolve = nullptr;

    GateContext context;
    d3::shader::Resolver resolver(manifest);
    resolver.SetMode(d3::shader::Mode::Strict);
    context.resolver = &resolver;
    context.target = d3::shader::Target::Fragment;
    ProgramGate manifestGate;
    manifestGate.user = &context;
    manifestGate.resolve = &GateThunk;

    if (RunMaterialOrder(openGate) != 0) return 1;
    if (RunAlphaAndBlend(openGate) != 0) return 1;
    if (RunPostAndMegaTexture(openGate) != 0) return 1;
    if (RunResolverGateRefusesUnowned(manifestGate, manifest) != 0) return 1;
    if (RunLightClassification() != 0) return 1;
    if (RunShadowChains() != 0) return 1;
    if (RunDepthHacks() != 0) return 1;

    // CP-14/CP-15/CP-16. The subview cases only need the ordered stream, so the adapter
    // stays structural: no device, no frame and no image comparison is claimed.
    SceneAdapterDesc adapterDesc;
    adapterDesc.displayWidth = 1280;
    adapterDesc.displayHeight = 720;
    adapterDesc.structuralOnly = true;
    SceneAdapter adapter;
    std::string adapterError;
    if (adapter.Initialize(adapterDesc, &adapterError) != Status::Ok) {
        q2_test::Log("INIT subview adapter failed: %s", adapterError.c_str());
        return 1;
    }
    if (RunMirrorSubview(adapter) != 0) return 1;
    if (RunPortalSky(openGate, manifestGate) != 0) return 1;
    if (RunXraySubview(adapter, openGate) != 0) return 1;

    q2_test::Log("T037 PASS (structural/classification only; CP-14/15/16 structural; "
                 "image parity NOT RUN)");
    return 0;
}
