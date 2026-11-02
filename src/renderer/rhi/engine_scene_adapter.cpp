// Ordered scene adapter for the classic path (spec 004 T035/T080-T082).
//
// The adapter owns four things and nothing else:
//
//   1. the backend-neutral command queue and its exact FIFO order (O-02),
//   2. the mid-frame split: CaptureRenderToFile executes the pending stream and clears it,
//      CaptureRenderToImage appends a copy and keeps recording (engine-integration §6.6),
//   3. the view/draw extraction counts and the display-overlay split (O-11/O-12),
//   4. when a device and a geometry source are supplied, the ordered submission itself:
//      one surface frame in stream order, then one stencil shadow-volume + shade frame per
//      light packet. Without them the adapter stays extraction-only and says so through
//      SceneExecuteResult::drawSubmissionUnavailable instead of presenting a plan as a
//      rendered frame.
//
// Geometry reaches this file already translated: the engine glue uploads buffers and
// textures through Device and hands over portable handles. No GL enum, idImage* or
// vertex-cache pointer appears here.

#include "rhi/engine_scene_adapter.h"

#include <algorithm>
#include <cstring>

namespace doom3 {
namespace rhi {

const char* ToString(SceneCommandKind value) {
    switch (value) {
        case SceneCommandKind::BeginView: return "BeginView";
        case SceneCommandKind::EndView: return "EndView";
        case SceneCommandKind::DrawPacket: return "DrawPacket";
        case SceneCommandKind::LightPacket: return "LightPacket";
        case SceneCommandKind::SetColorTarget: return "SetColorTarget";
        case SceneCommandKind::CopyToImage: return "CopyToImage";
        case SceneCommandKind::SwapBuffers: return "SwapBuffers";
    }
    return "Unknown";
}

Status TranslateColorTarget(uint32_t engineBufferValue, ColorTargetSelector* target,
                            std::string* error) {
    if (target == nullptr) {
        return Status::InvalidArgument;
    }
    switch (engineBufferValue) {
        case kEngineBufferFront:
            *target = ColorTargetSelector::Current;
            return Status::Ok;
        case kEngineBufferBack:
            *target = ColorTargetSelector::Offscreen;
            return Status::Ok;
        default:
            *target = ColorTargetSelector::Unsupported;
            if (error != nullptr) {
                *error = "unsupported set-buffer value " + std::to_string(engineBufferValue) +
                         ": the classic path accepts only the front/current and back/offscreen "
                         "targets";
            }
            return Status::Unsupported;
    }
}

ViewDesc MakeDisplayOverlayView(uint32_t identity, uint32_t width, uint32_t height) {
    ViewDesc view;
    view.identity = identity;
    view.width = width;
    view.height = height;
    view.overlay = true;
    view.worldGui = false;
    view.depthHack = DepthHackKind::None;
    // 640x480 ortho mapped onto the display, top-left origin (O-11). The virtual size is the
    // classic GUI space, not the display size, so HUD layout is resolution independent.
    for (int i = 0; i < 16; ++i) {
        view.projection[i] = 0.0f;
    }
    view.projection[0] = 2.0f / static_cast<float>(kOverlayVirtualWidth);
    view.projection[5] = -2.0f / static_cast<float>(kOverlayVirtualHeight);
    view.projection[10] = 1.0f;
    view.projection[12] = -1.0f;
    view.projection[13] = 1.0f;
    view.projection[15] = 1.0f;
    return view;
}

void ApplyWeaponDepthHack(float projection[16]) {
    if (projection == nullptr) {
        return;
    }
    // O-13: the usable depth interval becomes 0..0.5 and the projection z term is scaled by
    // 0.25. x and y NDC are unchanged, which is what keeps the weapon at the same place on
    // screen while it stops poking through walls.
    projection[10] *= 0.25f;
    projection[14] *= 0.25f;
}

void ApplyModelDepthHack(float projection[16], float distance) {
    if (projection == nullptr) {
        return;
    }
    // O-13: the model hack only subtracts from the z offset; x/y NDC is untouched.
    projection[14] -= distance;
}

bool SceneCommandQueue::Push(const SceneCommand& command) {
    commands_.push_back(command);
    return true;
}

bool SceneCommandQueue::BeginView(const ViewDesc& view) {
    if (view.identity == 0 || view.width == 0 || view.height == 0) {
        return false;
    }
    SceneCommand command;
    command.kind = SceneCommandKind::BeginView;
    command.view = view;
    return Push(command);
}

bool SceneCommandQueue::EndView() {
    // Balance is checked by the executor, not here: an unbalanced stream must reach the
    // validation pass so it is reported as fatal instead of silently dropped at the boundary.
    SceneCommand command;
    command.kind = SceneCommandKind::EndView;
    return Push(command);
}

bool SceneCommandQueue::Draw(const DrawSurfPacket& packet) {
    if (packet.surfaceId == 0 || packet.vertexCount == 0) {
        return false;
    }
    SceneCommand command;
    command.kind = SceneCommandKind::DrawPacket;
    command.draw = packet;
    return Push(command);
}

bool SceneCommandQueue::DrawLight(const LightPacket& packet) {
    if (packet.lightId == 0) {
        return false;
    }
    SceneCommand command;
    command.kind = SceneCommandKind::LightPacket;
    command.light = packet;
    return Push(command);
}

bool SceneCommandQueue::SetColorTarget(ColorTargetSelector target) {
    if (target == ColorTargetSelector::Unsupported) {
        return false;
    }
    SceneCommand command;
    command.kind = SceneCommandKind::SetColorTarget;
    command.target = target;
    return Push(command);
}

bool SceneCommandQueue::CopyToImage(uint32_t imageId) {
    if (imageId == 0) {
        return false;
    }
    SceneCommand command;
    command.kind = SceneCommandKind::CopyToImage;
    command.imageId = imageId;
    return Push(command);
}

bool SceneCommandQueue::CaptureToFile(uint32_t imageId) {
    if (!CopyToImage(imageId)) {
        return false;
    }
    // The copy is inside the flushed prefix, so it keeps the mid-frame position it was
    // recorded at; commands appended afterwards stay queued for the next execution.
    flushMark_ = commands_.size();
    flushPending_ = true;
    return true;
}

bool SceneCommandQueue::SwapBuffers() {
    SceneCommand command;
    command.kind = SceneCommandKind::SwapBuffers;
    return Push(command);
}

void SceneCommandQueue::Clear() {
    commands_.clear();
    flushMark_ = 0;
    flushPending_ = false;
}

void SceneCommandQueue::ConsumeFront(size_t count) {
    if (count > commands_.size()) {
        count = commands_.size();
    }
    commands_.erase(commands_.begin(), commands_.begin() + static_cast<std::ptrdiff_t>(count));
    flushMark_ = flushMark_ > count ? flushMark_ - count : 0;
    flushPending_ = false;
}

namespace {

DrawItem MakeItem(const SceneMeshHandles& mesh, const SceneSurfaceGeometry& geometry) {
    DrawItem item;
    item.vertexBuffer = mesh.vertexBuffer;
    item.vertexCount = mesh.vertexCount;
    if (mesh.indexCount != 0) {
        item.indexBuffer = mesh.indexBuffer;
        item.indexCount = mesh.indexCount;
    }
    item.texture = geometry.texture;
    std::memcpy(item.transform, geometry.transform, sizeof(item.transform));
    std::memcpy(item.tint, geometry.tint, sizeof(item.tint));
    return item;
}

}  // namespace

struct SceneAdapter::Impl {
    std::map<uint32_t, SceneMeshHandles> surfaces;
    std::map<uint32_t, SceneMeshHandles> volumes;
    std::map<uint32_t, SceneMeshHandles> shades;

    bool EnsureMesh(SceneGeometryBackend* backend, uint32_t key,
                    const SceneSurfaceGeometry& geometry,
                    std::map<uint32_t, SceneMeshHandles>* cache, std::string* error) {
        if (cache->find(key) != cache->end()) {
            return true;
        }
        if (geometry.vertices.empty()) {
            if (error != nullptr) {
                *error = "surface geometry has no vertices";
            }
            return false;
        }
        SceneMeshHandles handles;
        if (!backend->Upload(key, geometry, &handles, error)) {
            return false;
        }
        cache->emplace(key, handles);
        return true;
    }
};

SceneAdapter::SceneAdapter() : impl_(new Impl()) {}
SceneAdapter::~SceneAdapter() = default;

Status SceneAdapter::Initialize(const SceneAdapterDesc& desc, std::string* error) {
    if (desc.displayWidth == 0 || desc.displayHeight == 0) {
        if (error != nullptr) {
            *error = "scene adapter needs a non-zero display size";
        }
        return Status::InvalidArgument;
    }
    desc_ = desc;
    impl_->surfaces.clear();
    impl_->volumes.clear();
    impl_->shades.clear();
    initialized_ = true;
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

namespace {

bool ValidatePrefix(const std::vector<SceneCommand>& commands, size_t count, std::string* error) {
    int openViews = 0;
    for (size_t i = 0; i < count; ++i) {
        const SceneCommand& command = commands[i];
        switch (command.kind) {
            case SceneCommandKind::BeginView:
                if (command.view.identity == 0 || command.view.width == 0 ||
                    command.view.height == 0) {
                    if (error != nullptr) {
                        *error = "BeginView without an identity or size";
                    }
                    return false;
                }
                ++openViews;
                break;
            case SceneCommandKind::EndView:
                if (openViews == 0) {
                    if (error != nullptr) {
                        *error = "EndView without a matching BeginView";
                    }
                    return false;
                }
                --openViews;
                break;
            case SceneCommandKind::DrawPacket:
                if (command.draw.surfaceId == 0 || command.draw.vertexCount == 0) {
                    if (error != nullptr) {
                        *error = "DrawPacket without a surface identity or vertices";
                    }
                    return false;
                }
                break;
            case SceneCommandKind::LightPacket:
                if (command.light.lightId == 0) {
                    if (error != nullptr) {
                        *error = "LightPacket without a light identity";
                    }
                    return false;
                }
                break;
            case SceneCommandKind::SetColorTarget:
                if (command.target == ColorTargetSelector::Unsupported) {
                    if (error != nullptr) {
                        *error = "SetColorTarget carries an unsupported selector";
                    }
                    return false;
                }
                break;
            case SceneCommandKind::CopyToImage:
                if (command.imageId == 0) {
                    if (error != nullptr) {
                        *error = "CopyToImage without a destination identity";
                    }
                    return false;
                }
                break;
            case SceneCommandKind::SwapBuffers:
                break;
            default:
                if (error != nullptr) {
                    *error = "unknown scene command";
                }
                return false;
        }
    }
    return true;
}

}  // namespace

Status SceneAdapter::Execute(SceneCommandQueue* queue, SceneExecuteResult* result,
                             std::string* error) {
    if (!initialized_) {
        if (error != nullptr) {
            *error = "scene adapter is not initialized";
        }
        return Status::NotReady;
    }
    if (queue == nullptr || result == nullptr) {
        if (error != nullptr) {
            *error = "Execute needs a queue and a result";
        }
        return Status::InvalidArgument;
    }

    *result = SceneExecuteResult();
    const std::vector<SceneCommand>& commands = queue->commands();
    const size_t count =
        queue->HasPendingFlush() ? std::min(queue->FlushMark(), commands.size()) : commands.size();

    // Validation happens before any consumption, so an invalid stream is refused whole.
    std::string validationError;
    if (!ValidatePrefix(commands, count, &validationError)) {
        if (error != nullptr) {
            *error = validationError;
        }
        return Status::InvalidArgument;
    }

    const bool submitGeometry =
        !desc_.structuralOnly && desc_.backend != nullptr && desc_.geometry != nullptr;
    std::vector<DrawItem> pendingItems;

    auto flushSurfaces = [&]() -> Status {
        if (pendingItems.empty()) {
            return Status::Ok;
        }
        const uint32_t colourIndex = desc_.surfaceAttachmentIndex;
        ClearRow clear;
        clear.red = 0;
        clear.green = 0;
        clear.blue = 0;
        clear.alpha = 255;
        DrawPassDesc pass;
        pass.pipeline = desc_.surfacePipeline;
        pass.colorIndices = &colourIndex;
        pass.colorCount = 1;
        pass.colorClears = &clear;
        pass.items = pendingItems.data();
        pass.itemCount = static_cast<uint32_t>(pendingItems.size());
        DrawFrameDesc frame;
        frame.width = desc_.displayWidth;
        frame.height = desc_.displayHeight;
        frame.attachmentCount =
            std::max(desc_.surfaceAttachmentIndex, desc_.lightAttachmentIndex) + 1;
        frame.depthEnabled = true;
        frame.passCount = 1;
        frame.passes = &pass;
        Submission submission;
        const Status status = desc_.backend->Submit(frame, &submission, error);
        if (status == Status::Ok) {
            result->submissions.push_back(submission);
            result->submissionAttachments.push_back(desc_.surfaceAttachmentIndex);
        }
        pendingItems.clear();
        return status;
    };

    for (size_t i = 0; i < count; ++i) {
        const SceneCommand& command = commands[i];
        result->executed.push_back(command.kind);
        switch (command.kind) {
            case SceneCommandKind::BeginView:
                ++result->views;
                result->viewIds.push_back(command.view.identity);
                if (command.view.overlay) {
                    ++result->overlays;
                    ++stats_.overlays;
                } else {
                    ++stats_.views;
                }
                break;
            case SceneCommandKind::DrawPacket: {
                ++result->drawsExtracted;
                ++stats_.draws;
                if (command.draw.stageCount == 0) {
                    // Every stage was refused, so there is nothing this packet could draw.
                    ++result->drawsRefused;
                    ++stats_.drawsRefused;
                    break;
                }
                if (!submitGeometry) {
                    result->drawSubmissionUnavailable = true;
                    break;
                }
                SceneSurfaceGeometry geometry;
                if (!desc_.geometry->ResolveSurface(command.draw.surfaceId, &geometry) ||
                    !impl_->EnsureMesh(desc_.backend, command.draw.surfaceId, geometry,
                                       &impl_->surfaces, error)) {
                    ++result->drawsRefused;
                    ++stats_.drawsRefused;
                    break;
                }
                pendingItems.push_back(MakeItem(impl_->surfaces[command.draw.surfaceId],
                                                geometry));
                ++result->drawsSubmitted;
                ++stats_.drawsSubmitted;
                break;
            }
            case SceneCommandKind::LightPacket: {
                const Status flushed = flushSurfaces();
                if (flushed != Status::Ok) {
                    queue->ConsumeFront(count);
                    return flushed;
                }
                ++result->lightsExtracted;
                ++stats_.lights;
                LightPlan lightPlan;
                if (PlanLight(command.light.light, command.light.material,
                              command.light.sameEntity, &lightPlan, error) != Status::Ok) {
                    queue->ConsumeFront(count);
                    return Status::InvalidArgument;
                }
                ShadowVolumePlan volumePlan;
                if (PlanShadowVolume(lightPlan, &volumePlan, error) != Status::Ok) {
                    queue->ConsumeFront(count);
                    return Status::InvalidArgument;
                }
                if (!volumePlan.enabled) {
                    // CP-07 exclusion: no chain, no stencil work.
                    ++result->lightsExcluded;
                    ++stats_.lightsExcluded;
                    break;
                }
                if (volumePlan.chainCount != 1) {
                    if (error != nullptr) {
                        *error =
                            "light packet carries " +
                            std::to_string(volumePlan.chainCount) +
                            " shadow chains; emit one packet per chain instead of "
                            "dropping a chain";
                    }
                    queue->ConsumeFront(count);
                    return Status::Unsupported;
                }
                if (!submitGeometry) {
                    break;
                }
                SceneLightGeometry lightGeometry;
                const uint32_t backKey = command.light.lightId * 2u;
                if (!desc_.geometry->ResolveLight(command.light, &lightGeometry) ||
                    !impl_->EnsureMesh(desc_.backend, backKey, lightGeometry.volumes[0],
                                       &impl_->volumes, error) ||
                    !impl_->EnsureMesh(desc_.backend, backKey + 1u, lightGeometry.volumes[1],
                                       &impl_->volumes, error)) {
                    ++result->lightsRefused;
                    ++stats_.lightsRefused;
                    break;
                }

                DrawItem backItem =
                    MakeItem(impl_->volumes[backKey], lightGeometry.volumes[0]);
                DrawItem frontItem =
                    MakeItem(impl_->volumes[backKey + 1u], lightGeometry.volumes[1]);
                DrawItem shadeItem;
                bool shadeFromGeometry = !lightGeometry.shade.vertices.empty();
                if (shadeFromGeometry) {
                    if (!impl_->EnsureMesh(desc_.backend, backKey + 2u, lightGeometry.shade,
                                           &impl_->shades, error)) {
                        ++result->lightsRefused;
                        ++stats_.lightsRefused;
                        break;
                    }
                    shadeItem = MakeItem(impl_->shades[backKey + 2u], lightGeometry.shade);
                }

                const uint32_t colourIndex = desc_.lightAttachmentIndex;
                ClearRow clear;
                clear.red = 0;
                clear.green = 0;
                clear.blue = 0;
                clear.alpha = 255;
                // Zero chains were handled above, so the volume plan always has
                // the back-increment / front-decrement pair here.
                DrawPassDesc passes[3];
                passes[0].pipeline = desc_.shadowBackPipeline;
                passes[0].items = &backItem;
                passes[0].itemCount = 1;
                passes[1].pipeline = desc_.shadowFrontPipeline;
                passes[1].items = &frontItem;
                passes[1].itemCount = 1;
                passes[2].pipeline = desc_.lightShadePipeline;
                passes[2].colorIndices = &colourIndex;
                passes[2].colorCount = 1;
                passes[2].colorClears = &clear;
                for (int channel = 0; channel < 4; ++channel) {
                    passes[2].tint[channel] = lightGeometry.tint[channel];
                }
                if (shadeFromGeometry) {
                    passes[2].items = &shadeItem;
                    passes[2].itemCount = 1;
                }
                DrawFrameDesc frame;
                frame.width = desc_.displayWidth;
                frame.height = desc_.displayHeight;
                frame.attachmentCount =
                    std::max(desc_.surfaceAttachmentIndex, desc_.lightAttachmentIndex) + 1;
                frame.depthEnabled = true;
                frame.passCount = 3;
                frame.passes = passes;
                Submission submission;
                const Status status =
                    desc_.backend->Submit(frame, &submission, error);
                if (status != Status::Ok) {
                    queue->ConsumeFront(count);
                    return status;
                }
                result->submissions.push_back(submission);
                result->submissionAttachments.push_back(desc_.lightAttachmentIndex);
                result->lightPassesSubmitted += 3;
                stats_.lightPassesSubmitted += 3;
                break;
            }
            default:
                break;
        }
    }

    if (submitGeometry) {
        const Status flushed = flushSurfaces();
        if (flushed != Status::Ok) {
            queue->ConsumeFront(count);
            return flushed;
        }
    }

    if (!submitGeometry && !desc_.structuralOnly) {
        if (error != nullptr) {
            *error =
                "geometry submission needs a device and a geometry source; no classic frame "
                "was drawn";
        }
        queue->ConsumeFront(count);
        return Status::Unsupported;
    }

    if (!submitGeometry) {
        // Nothing was submitted for any extracted packet, whether it was refused or merely
        // extracted. Disclosing that is not a formality: without it an extraction-only run
        // could be read as a rendered frame.
        result->drawSubmissionUnavailable =
            result->drawsExtracted > result->drawsSubmitted;
    }

    queue->ConsumeFront(count);
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

}  // namespace rhi
}  // namespace doom3

