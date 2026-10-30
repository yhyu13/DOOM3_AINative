// Q7b WebGPU browser-profile capability contract.
//
// This module is the executable side of
// specs/004-quality-renderer/portable-profile-contract.md. It evaluates the
// advertised adapter/device facts of one explicitly named target and produces a
// table of exercised, fallback and unsupported rows. It never infers the target
// from the host OS and never turns an absent check into a pass.

export const WEB_PROFILE_ID = "webgpu-browser";
export const WEB_PROFILE_CONTRACT_VERSION = 1;
export const CAPABILITY_SCHEMA = "q7-webgpu-capability/1";

const MiB = 1024 * 1024;

// Required rows: a missing or short value is an explicit unsupported result for
// this profile, not a silent downgrade. `direction` says how the advertised
// value must compare with the requirement.
export const REQUIRED_LIMITS = Object.freeze([
  { name: "maxBufferSize", required: 256 * MiB, direction: "at-least", reason: "chunked staging and uploads need 256 MiB" },
  { name: "maxStorageBufferBindingSize", required: 128 * MiB, direction: "at-least", reason: "GI/reflection buffers must fit one binding or be split" },
  { name: "maxTextureDimension2D", required: 8192, direction: "at-least", reason: "display and offscreen targets up to 8192 px" },
  { name: "maxBindGroups", required: 4, direction: "at-least", reason: "frame / view-light / material / per-draw split" },
  { name: "maxSampledTexturesPerShaderStage", required: 16, direction: "at-least", reason: "seven interaction textures plus headroom" },
  { name: "maxStorageBuffersPerShaderStage", required: 8, direction: "at-least", reason: "budgeted compute GI/reflection jobs" },
  { name: "maxUniformBuffersPerShaderStage", required: 12, direction: "at-least", reason: "classic path uniform blocks" },
  { name: "maxUniformBufferBindingSize", required: 64 * 1024, direction: "at-least", reason: "per-light and per-draw data packing" },
  { name: "minUniformBufferOffsetAlignment", required: 256, direction: "at-most", reason: "uniform ring slots are 256-byte aligned" },
  { name: "maxVertexBuffers", required: 8, direction: "at-least", reason: "single interleaved DOOM 3 vertex stream fits" },
  { name: "maxVertexAttributes", required: 16, direction: "at-least", reason: "position/ST/normal/tangents/colour fit" },
  { name: "maxColorAttachments", required: 8, direction: "at-least", reason: "G-buffer plus auxiliary targets" },
  { name: "maxColorAttachmentBytesPerSample", required: 32, direction: "at-least", reason: "28-byte G-buffer plus headroom" },
  { name: "maxComputeInvocationsPerWorkgroup", required: 256, direction: "at-least", reason: "compute tile sizes must respect the workgroup cap" },
]);

// Optional rows carry an explicit fallback. `impact` decides whether a missing
// feature degrades the declared quality profile or is informational only.
export const OPTIONAL_FEATURES = Object.freeze([
  {
    name: "timestamp-query",
    fallback: "cpu-frame-timing",
    impact: "info",
    reason: "GPU timestamps unavailable; use CPU frame timing and make no GPU performance claim",
  },
  {
    name: "texture-compression-bc",
    fallback: "bc-transcode-required",
    impact: "quality",
    reason: "BC textures must be transcoded before upload; no BC asset is exercised in this slice",
  },
  {
    name: "depth-clip-control",
    fallback: "no-clip-plane",
    impact: "info",
    reason: "DOOM 3 does not require clip planes; alphaNotch is already shader-side",
  },
  {
    name: "float32-filterable",
    fallback: "nearest-or-half-float",
    impact: "info",
    reason: "use nearest filtering or a half-float format for float textures",
  },
]);

function hasFeature(features, name) {
  if (features instanceof Set) return features.has(name);
  if (Array.isArray(features)) return features.includes(name);
  if (features && typeof features === "object") return features[name] === true;
  return false;
}

function finiteNumber(value) {
  const n = typeof value === "number" ? value : Number(value);
  return Number.isFinite(n) ? n : null;
}

function normalizeAdapter(adapter = {}) {
  const vendorId = finiteNumber(adapter.vendorId) ?? 0;
  const deviceID = finiteNumber(adapter.deviceID) ?? 0;
  return {
    description: String(adapter.description ?? ""),
    vendor: String(adapter.vendor ?? ""),
    architecture: String(adapter.architecture ?? ""),
    device: String(adapter.device ?? ""),
    fallbackAdapter: Boolean(adapter.fallbackAdapter),
    // A browser may hide adapter identifiers. Selection must not use them.
    reportedFacts: vendorId === 0 && deviceID === 0 ? "0x0" : `${vendorId}/${deviceID}`,
  };
}

/**
 * Evaluate capability facts for one explicitly named target.
 *
 * @param {object} input
 * @param {string} input.targetId            must equal WEB_PROFILE_ID
 * @param {object} input.adapter             normalized adapter facts
 * @param {object} input.limits              GPUDevice.limits-shaped object
 * @param {Set|string[]|object} input.features advertised/device features
 * @returns {object} capability table
 */
export function evaluateCapability(input = {}) {
  const targetId = String(input.targetId ?? "");
  const adapter = normalizeAdapter(input.adapter);
  const limits = input.limits ?? {};
  const rows = [];
  const fallbacks = [];
  const unsupported = [];

  if (targetId !== WEB_PROFILE_ID) {
    unsupported.push({
      id: "target-identity",
      reason: `target "${targetId || "(missing)"}" is not the declared ${WEB_PROFILE_ID} profile; a Windows or native pass cannot stand in`,
    });
  }

  for (const requirement of REQUIRED_LIMITS) {
    const advertised = finiteNumber(limits[requirement.name]);
    let status = "PASS";
    let reason = "advertised value meets the requirement";
    if (advertised === null) {
      status = "UNSUPPORTED";
      reason = "advertised limit is missing";
    } else if (requirement.direction === "at-least" && advertised < requirement.required) {
      status = "UNSUPPORTED";
      reason = `advertised ${advertised} is below the required ${requirement.required}`;
    } else if (requirement.direction === "at-most" && advertised > requirement.required) {
      status = "UNSUPPORTED";
      reason = `advertised ${advertised} is above the allowed ${requirement.required}`;
    }
    const row = {
      id: `limit:${requirement.name}`,
      kind: "limit",
      name: requirement.name,
      requirement: requirement.required,
      direction: requirement.direction,
      advertised,
      status,
      fallback: null,
      reason,
    };
    rows.push(row);
    if (status === "UNSUPPORTED") {
      unsupported.push({ id: row.id, reason });
    }
  }

  for (const feature of OPTIONAL_FEATURES) {
    const present = hasFeature(input.features, feature.name);
    const row = {
      id: `feature:${feature.name}`,
      kind: "feature",
      name: feature.name,
      requirement: "optional",
      direction: "present",
      advertised: present,
      status: present ? "PASS" : "FALLBACK",
      fallback: present ? null : feature.fallback,
      reason: present ? "advertised feature is present" : feature.reason,
      impact: feature.impact,
    };
    rows.push(row);
    if (!present) {
      fallbacks.push({ id: feature.fallback, feature: feature.name, impact: feature.impact, reason: feature.reason });
    }
  }

  const qualityFallback = fallbacks.some((f) => f.impact === "quality");
  const status = unsupported.length > 0 ? "UNSUPPORTED" : qualityFallback ? "DEGRADED" : "READY";

  return {
    schema: CAPABILITY_SCHEMA,
    contractVersion: WEB_PROFILE_CONTRACT_VERSION,
    targetId: targetId || "(missing)",
    status,
    adapter,
    rows,
    fallbacks,
    unsupported,
    notes: [
      "status READY means every required row passed and no quality fallback was selected",
      "status DEGRADED still requires explicit fallback behaviour and must not be reported as full support",
      "status UNSUPPORTED means the profile must refuse to claim the target",
      "an adapter-reported identifier of 0x0 is recorded, never used for selection",
    ],
  };
}
