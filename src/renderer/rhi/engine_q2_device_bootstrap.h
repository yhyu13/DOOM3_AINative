#pragma once

// Concrete D3D12 bootstrap for the Q2 classic path (spec 004 T080-T082).
//
// R_InitRenderApi is the engine's call site, but it cannot name Device,
// Pipeline or D3D12 vocabulary: those live in the port. The engine registration
// unit (engine_q2_sink_install.cpp) calls this runtime from its initialize
// callback, so the "create the Device and the four classic pipelines" step that
// R_InitRenderApi performs happens here, in the superproject, exactly once per
// initialization.
//
// The runtime owns the Device and the EngineQ2DeviceConsumer. Creation order is
// Device -> pipelines -> consumer; destruction is consumer -> Device, because
// the texture cache must clear its textures while the Device is still valid.
//
// This is still not a retail engine frame: it creates real device objects and
// initializes the same consumer the synthetic fixtures exercise, but a frame
// also needs engine-emitted geometry from a legally loaded level.

#include "rhi/engine_q2_sink_device.h"
#include "rhi/rhi.h"

#include <memory>
#include <string>

namespace doom3 {
namespace rhi {

// Compiles the classic programs and creates the four pipelines the adapter
// submits to: surface, shadow back (increment), shadow front (decrement) and
// the shade pass. The pipeline states mirror the synthetic fixtures:
//   * surface: one colour attachment, vertex input, textured item;
//   * back: front-face cull, stencil increment-saturate, write mask 0xFF;
//   * front: back-face cull, stencil decrement-saturate, write mask 0xFF;
//   * shade: depth test, no depth write, stencil Equal with no writes.
// A compilation or creation failure is returned with its device error.
bool CreateEngineQ2ClassicPipelines(Device& device,
                                    EngineQ2DevicePipelines* pipelines,
                                    std::string* error);

// The engine's classic path, from device creation through an initialized
// consumer. Call Create() once per R_InitRenderApi; call Shutdown() before the
// process drops the Device.
class EngineQ2DeviceRuntime {
public:
    EngineQ2DeviceRuntime();
    ~EngineQ2DeviceRuntime();

    EngineQ2DeviceRuntime(const EngineQ2DeviceRuntime&) = delete;
    EngineQ2DeviceRuntime& operator=(const EngineQ2DeviceRuntime&) = delete;

    // Creates the hardware device at adapter index 0, compiles the four
    // pipelines and initializes the consumer at the display extent. On failure
    // every object created so far is retired and `error` names the stage.
    bool Create(int width, int height, std::string* error);

    // Safe to call more than once. Destroys the consumer's textures first, then
    // drops the consumer and the device, in that order.
    void Shutdown();

    bool created() const { return consumer_ != nullptr; }

    Device& device() { return *device_; }
    const Device& device() const { return *device_; }
    EngineQ2DeviceConsumer& consumer() { return *consumer_; }
    const EngineQ2DeviceConsumer& consumer() const { return *consumer_; }
    EngineQ2SinkConsumer& sink() { return consumer_->sink(); }
    const EngineQ2SinkConsumer& sink() const { return consumer_->sink(); }

private:
    std::unique_ptr<Device> device_;
    // Declared after device_ so it is destroyed first.
    std::unique_ptr<EngineQ2DeviceConsumer> consumer_;
    EngineQ2DevicePipelines pipelines_;
};

}  // namespace rhi
}  // namespace doom3
