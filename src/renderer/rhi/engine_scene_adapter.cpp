// Ordered scene adapter for the classic path (spec 004 T035/T080).
//
// The adapter owns three things and nothing else:
//
//   1. the backend-neutral command queue and its exact FIFO order (O-02),
//   2. the mid-frame split: CaptureRenderToFile executes the pending stream and clears it,
//      CaptureRenderToImage appends a copy and keeps recording (engine-integration §6.6),
//   3. the view/draw extraction counts and the display-overlay split (O-11/O-12).
//
// It deliberately does not draw classic geometry. The Q1b portable seam composes a colour
// frame but accepts no vertex or index data, so a draw packet can be extracted, classified
// and refused, and cannot be submitted. The adapter says so through
// SceneExecuteResult::drawSubmissionUnavailable instead of presenting an extraction pass as
// a rendered frame. Extending the seam with geometry submission is the open T080 blocker
// recorded in specs/004-quality-renderer/evidence-q2.md.

#include "rhi/engine_scene_adapter.h"

#include <algorithm>

namespace doom3 {
namespace rhi {

const char* ToString(SceneCommandKind value) {
    switch (value) {
        case SceneCommandKind::BeginView: return "BeginView";
        case SceneCommandKind::EndView: return "EndView";
        case SceneCommandKind::DrawPacket: return "DrawPacket";
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

SceneAdapter::SceneAdapter() = default;
SceneAdapter::~SceneAdapter() = default;

Status SceneAdapter::Initialize(const SceneAdapterDesc& desc, std::string* error) {
    if (desc.displayWidth == 0 || desc.displayHeight == 0) {
        if (error != nullptr) {
            *error = "scene adapter needs a non-zero display size";
        }
        return Status::InvalidArgument;
    }
    desc_ = desc;
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
            case SceneCommandKind::DrawPacket:
                ++result->drawsExtracted;
                ++stats_.draws;
                if (command.draw.stageCount == 0) {
                    // Every stage was refused, so there is nothing this packet could draw.
                    ++result->drawsRefused;
                    ++stats_.drawsRefused;
                } else {
                    result->drawSubmissionUnavailable = true;
                }
                break;
            default:
                break;
        }
    }

    if (result->drawsExtracted != result->drawsRefused) {
        // The seam has no geometry submission, so a packet with a live stage run is not
        // drawn. Say that instead of reporting a frame.
        if (desc_.structuralOnly) {
            if (error != nullptr) {
                error->clear();
            }
        } else {
            if (error != nullptr) {
                *error =
                    "scene geometry submission is not available through the current portable "
                    "seam; no classic frame was drawn";
            }
            // The stream was still consumed in order, which is the part this slice proves.
            queue->ConsumeFront(count);
            return Status::Unsupported;
        }
    }

    // Nothing was submitted for any extracted packet, whether it was refused or merely
    // extracted. Disclosing that is not a formality: without it an extraction-only run could
    // be read as a rendered frame.
    result->drawSubmissionUnavailable = result->drawsExtracted > result->drawsSubmitted;

    queue->ConsumeFront(count);
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

}  // namespace rhi
}  // namespace doom3
