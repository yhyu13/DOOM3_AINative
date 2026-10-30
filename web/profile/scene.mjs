// Q7b procedural test scene.
//
// This is a deterministic full-screen field, not a game scene. It exists to
// exercise canvas configuration, a WGSL module, a bind group, a uniform update
// and a draw call without any retail asset. It is not real-scene acceptance.

export const PROCEDURAL_SCENE_ID = "q7-procedural-field/1";
export const SCENE_UNIFORM_BYTES = 16;

export const PROCEDURAL_WGSL = `
struct SceneUniforms {
  time: f32,
  width: f32,
  height: f32,
  _pad: f32,
};

@group(0) @binding(0) var<uniform> scene: SceneUniforms;

@vertex
fn vs(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f {
  var positions = array<vec2f, 3>(
    vec2f(-1.0, -1.0),
    vec2f( 3.0, -1.0),
    vec2f(-1.0,  3.0)
  );
  return vec4f(positions[vertexIndex], 0.0, 1.0);
}

@fragment
fn fs(@builtin(position) fragCoord: vec4f) -> @location(0) vec4f {
  let uv = fragCoord.xy / vec2f(max(scene.width, 1.0), max(scene.height, 1.0));
  let wave = 0.5 + 0.5 * sin(scene.time * 1.2 + uv.x * 8.0 + uv.y * 5.0);
  let cell = floor(uv.x * 24.0) + floor(uv.y * 14.0);
  let checker = select(0.055, 0.115, cell % 2.0 < 1.0);
  let scan = 1.0 - smoothstep(0.0, 0.02, abs(uv.y - 0.5 - 0.22 * sin(scene.time + uv.x * 4.0)));
  let base = vec3f(0.035 + checker, 0.05 + checker, 0.09 + checker);
  let tint = vec3f(0.12, 0.32, 0.55) * wave + vec3f(0.85, 0.45, 0.10) * scan;
  return vec4f(base + tint, 1.0);
}
`;

export function sceneUniformBytes(timeSeconds, width, height) {
  return new Float32Array([timeSeconds, width, height, 0]);
}

const BUFFER_USAGE_UNIFORM = 0x40;
const BUFFER_USAGE_COPY_DST = 0x08;
const SHADER_STAGE_VERTEX = 0x1;
const SHADER_STAGE_FRAGMENT = 0x2;

/**
 * Build the procedural pipeline. Caller owns disposal.
 */
export function createProceduralScene({ device, format } = {}) {
  if (!device || typeof device.createShaderModule !== "function") throw new Error("device is required");
  if (!format) throw new Error("color format is required");

  const module = device.createShaderModule({ label: "q7-procedural-field", code: PROCEDURAL_WGSL });
  const uniform = device.createBuffer({
    label: "q7-scene-uniforms",
    size: SCENE_UNIFORM_BYTES,
    usage: BUFFER_USAGE_UNIFORM | BUFFER_USAGE_COPY_DST,
  });
  const bindGroupLayout = device.createBindGroupLayout({
    label: "q7-scene-layout",
    entries: [
      {
        binding: 0,
        visibility: SHADER_STAGE_VERTEX | SHADER_STAGE_FRAGMENT,
        buffer: { type: "uniform" },
      },
    ],
  });
  const pipeline = device.createRenderPipeline({
    label: "q7-procedural-pipeline",
    layout: device.createPipelineLayout({ label: "q7-scene-pipeline-layout", bindGroupLayouts: [bindGroupLayout] }),
    vertex: { module, entryPoint: "vs" },
    fragment: { module, entryPoint: "fs", targets: [{ format }] },
    primitive: { topology: "triangle-list" },
  });
  const bindGroup = device.createBindGroup({
    label: "q7-scene-bind-group",
    layout: bindGroupLayout,
    entries: [{ binding: 0, resource: { buffer: uniform } }],
  });

  let draws = 0;

  return {
    id: PROCEDURAL_SCENE_ID,
    pipeline,
    bindGroup,
    uniform,
    render(pass, timeSeconds, width, height) {
      device.queue.writeBuffer(uniform, 0, sceneUniformBytes(timeSeconds, width, height));
      pass.setPipeline(pipeline);
      pass.setBindGroup(0, bindGroup);
      pass.draw(3);
      draws += 1;
    },
    getDraws: () => draws,
    dispose() {
      uniform.destroy?.();
    },
  };
}
