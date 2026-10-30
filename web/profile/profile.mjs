// Q7b WebGPU browser-profile host slice.
//
// This orchestrates the target-specific pieces behind one explicit profile id:
// adapter/device creation, capability table, canvas swapchain, procedural test
// scene, cooperative loop, input bridge, audio bridge and VFS self-test.
//
// It is a host slice, not a doom3::rhi backend and not a real-scene acceptance
// run. Every unavailable path returns NOT RUN/UNSUPPORTED with a reason.

import {
  CAPABILITY_SCHEMA,
  OPTIONAL_FEATURES,
  REQUIRED_LIMITS,
  WEB_PROFILE_ID,
  evaluateCapability,
} from "./capability.mjs";
import {
  NO_GAME_DATA_REASON,
  normalizeVirtualPath,
  createMemoryMount,
  createVfs,
} from "./vfs.mjs";
import { createMainLoop } from "./main-loop.mjs";
import { createSwapchain, DEFAULT_FORMAT } from "./swapchain.mjs";
import { createInputBridge } from "./input.mjs";
import { createAudioBridge } from "./audio.mjs";
import { PROCEDURAL_SCENE_ID, createProceduralScene } from "./scene.mjs";

export const PROFILE_SCHEMA = "q7-webgpu-profile/1";

export const PROFILE_REQUIRED_CHECKS = Object.freeze([
  "P1-device",
  "P2-capability-table",
  "P3-swapchain",
  "P4-procedural-frame",
  "P4b-canvas-readback",
  "P5-vfs",
  "P6-main-loop",
  "P7-input",
  "P8-audio-gate",
  "P9-capability-fallback",
  "E-uncaptured",
]);

export const PROFILE_EXPECTED_NOT_RUN = Object.freeze({
  "P7b-pointer-lock":
    "headless capture has no trusted user gesture, so pointer lock is not requested; the request path is covered by the input unit tests",
  "P8b-browser-audio":
    "headless capture has no trusted user gesture, so AudioContext.resume() is not asserted; the unlock path is covered by the fake-AudioContext unit tests",
  "R1-real-scene": NO_GAME_DATA_REASON,
});

const BUFFER_USAGE_MAP_READ = 0x01;
const BUFFER_USAGE_COPY_DST = 0x08;
const MAP_MODE_READ = 0x01;
const CHECK_ORDER = [
  ...PROFILE_REQUIRED_CHECKS,
  "P7b-pointer-lock",
  "P8b-browser-audio",
  "E-device-loss",
  "R1-real-scene",
];

function adapterInfoOf(adapter) {
  const info = adapter.info ?? {};
  return {
    description: String(info.description ?? ""),
    vendor: String(info.vendor ?? ""),
    architecture: String(info.architecture ?? ""),
    device: String(info.device ?? ""),
    fallbackAdapter: Boolean(adapter.isFallbackAdapter ?? info.isFallbackAdapter),
    vendorId: Number(info.vendorId ?? 0) || 0,
    deviceID: Number(info.deviceId ?? info.deviceID ?? 0) || 0,
  };
}

function featureList(features) {
  if (!features) return [];
  if (typeof features.has === "function" && typeof features[Symbol.iterator] === "function") return [...features];
  if (Array.isArray(features)) return features;
  return [];
}

function fullLimits() {
  const limits = {};
  for (const row of REQUIRED_LIMITS) limits[row.name] = row.required;
  return limits;
}

function summarize(checks) {
  return {
    pass: checks.filter((c) => c.status === "PASS").length,
    fail: checks.filter((c) => c.status === "FAIL").length,
    notRun: checks.filter((c) => c.status === "NOT RUN").length,
    total: checks.length,
  };
}

/**
 * Create the browser profile. Call `init()` before `start()`.
 */
export function createWebProfile({ canvas, page = globalThis, options = {} } = {}) {
  if (!canvas) throw new Error("canvas is required");

  const checks = new Map();
  const errors = [];
  let adapter = null;
  let device = null;
  let capability = null;
  let swapchain = null;
  let scene = null;
  let input = null;
  let audio = null;
  let loop = null;
  let captureRequest = null;
  let deviceLost = null;
  let frames = 0;
  let startedAtMs = null;

  function recordCheck(id, status, detail, observed) {
    if (!["PASS", "FAIL", "NOT RUN"].includes(status)) throw new Error(`invalid check status ${status}`);
    const row = { id, status, detail: String(detail ?? ""), observed: observed ?? null };
    checks.set(id, row);
    return row;
  }

  function elapsedSeconds() {
    if (startedAtMs === null) startedAtMs = page.performance?.now?.() ?? Date.now();
    const now = page.performance?.now?.() ?? Date.now();
    return (now - startedAtMs) / 1000;
  }

  function renderFrame() {
    if (!device || !swapchain || !scene) return null;
    const acquired = swapchain.acquire();
    const encoder = device.createCommandEncoder({ label: "q7-profile-frame" });
    const pass = encoder.beginRenderPass({
      label: "q7-profile-pass",
      colorAttachments: [
        {
          view: acquired.view,
          clearValue: { r: 0.01, g: 0.01, b: 0.02, a: 1 },
          loadOp: "clear",
          storeOp: "store",
        },
      ],
    });
    scene.render(pass, elapsedSeconds(), acquired.size.width, acquired.size.height);
    pass.end();
    frames += 1;

    const pending = captureRequest;
    if (!pending) {
      device.queue.submit([encoder.finish()]);
      return acquired;
    }

    captureRequest = null;
    const size = acquired.size;
    const bytesPerRow = Math.ceil((size.width * 4) / 256) * 256;
    const buffer = device.createBuffer({
      label: "q7-canvas-readback",
      size: bytesPerRow * size.height,
      usage: BUFFER_USAGE_COPY_DST | BUFFER_USAGE_MAP_READ,
    });
    encoder.copyTextureToBuffer(
      { texture: acquired.texture, mipLevel: 0, origin: { x: 0, y: 0, z: 0 } },
      { buffer, bytesPerRow, rowsPerImage: size.height },
      { width: size.width, height: size.height, depthOrArrayLayers: 1 },
    );
    device.queue.submit([encoder.finish()]);
    buffer
      .mapAsync(MAP_MODE_READ)
      .then(() => {
        const data = new Uint8Array(buffer.getMappedRange());
        let nonBlack = 0;
        for (let i = 0; i + 3 < data.length; i += 4) {
          if (data[i] !== 0 || data[i + 1] !== 0 || data[i + 2] !== 0) nonBlack += 1;
        }
        const pixels = size.width * size.height;
        buffer.unmap();
        buffer.destroy?.();
        pending({
          status: nonBlack > 0 ? "OK" : "FAIL",
          width: size.width,
          height: size.height,
          nonBlack,
          pixels,
        });
      })
      .catch((error) => {
        buffer.destroy?.();
        pending({ status: "FAIL", reason: String(error?.message ?? error) });
      });
    return acquired;
  }

  async function runVfsSelfTest() {
    const vfs = createVfs();
    vfs.mount(createMemoryMount("synthetic", { "/demo/hello.txt": "q7 synthetic vfs\n" }, { prefix: "/demo" }));
    const text = await vfs.readText("/demo/hello.txt");
    let missingCode = null;
    try {
      await vfs.readText("/demo/missing.txt");
    } catch (error) {
      missingCode = error.code;
    }
    let traversalCode = null;
    try {
      normalizeVirtualPath("/demo/../secret");
    } catch (error) {
      traversalCode = error.code;
    }
    const game = vfs.openGameData();
    const pak = vfs.readPakDirectory();
    const ok =
      text === "q7 synthetic vfs\n" &&
      missingCode === "ERR_NOT_FOUND" &&
      traversalCode === "ERR_PATH_TRAVERSAL" &&
      game.status === "NOT RUN" &&
      game.reason === NO_GAME_DATA_REASON &&
      pak.status === "UNSUPPORTED";
    recordCheck(
      "P5-vfs",
      ok ? "PASS" : "FAIL",
      ok
        ? "synthetic read, typed miss and traversal refusal passed; game data is NOT RUN and pak parsing is UNSUPPORTED"
        : "VFS self-test did not meet its declared result",
      { text, missingCode, traversalCode, game, pak },
    );
  }

  function runCapabilitySelfTest() {
    const all = OPTIONAL_FEATURES.map((feature) => feature.name);
    const ready = evaluateCapability({
      targetId: WEB_PROFILE_ID,
      adapter: { description: "synthetic ready adapter" },
      limits: fullLimits(),
      features: all,
    });
    const degraded = evaluateCapability({
      targetId: WEB_PROFILE_ID,
      adapter: { description: "synthetic degraded adapter" },
      limits: fullLimits(),
      features: all.filter((name) => name !== "texture-compression-bc"),
    });
    const unsupported = evaluateCapability({
      targetId: WEB_PROFILE_ID,
      adapter: { description: "synthetic short adapter" },
      limits: { ...fullLimits(), maxBindGroups: 3 },
      features: all,
    });
    const wrongTarget = evaluateCapability({
      targetId: "win11-amd64",
      adapter: { description: "synthetic native adapter" },
      limits: fullLimits(),
      features: all,
    });
    const ok =
      ready.status === "READY" &&
      degraded.status === "DEGRADED" &&
      degraded.fallbacks.some((fallback) => fallback.id === "bc-transcode-required") &&
      unsupported.status === "UNSUPPORTED" &&
      wrongTarget.status === "UNSUPPORTED";
    recordCheck(
      "P9-capability-fallback",
      ok ? "PASS" : "FAIL",
      ok
        ? "ready, quality fallback, short-limit unsupported and wrong-target unsupported all classified"
        : "capability fallback self-test did not meet its declared result",
      {
        ready: ready.status,
        degraded: degraded.status,
        degradedFallbacks: degraded.fallbacks,
        unsupported: unsupported.status,
        wrongTarget: wrongTarget.status,
      },
    );
  }

  function selfTestInput() {
    if (!input) {
      recordCheck("P7-input", "NOT RUN", "input bridge was not created");
      return { status: "NOT RUN" };
    }
    const Keyboard = page.KeyboardEvent;
    if (!Keyboard || typeof page.dispatchEvent !== "function") {
      recordCheck("P7-input", "NOT RUN", "KeyboardEvent or dispatchEvent is unavailable in this host");
      return { status: "NOT RUN" };
    }
    try {
      page.dispatchEvent(new Keyboard("keydown", { code: "KeyW" }));
      const down = input.snapshot().keys.includes("KeyW");
      page.dispatchEvent(new Keyboard("keyup", { code: "KeyW" }));
      const up = !input.snapshot().keys.includes("KeyW");
      const ok = down && up;
      recordCheck("P7-input", ok ? "PASS" : "FAIL", "synthetic keydown/keyup exercised the input bridge", { down, up });
      return { status: ok ? "PASS" : "FAIL", down, up };
    } catch (error) {
      recordCheck("P7-input", "FAIL", `synthetic key events threw: ${String(error?.message ?? error)}`);
      return { status: "FAIL", reason: String(error?.message ?? error) };
    }
  }

  async function init() {
    startedAtMs = page.performance?.now?.() ?? Date.now();
    const gpu = page.navigator?.gpu;
    if (!gpu) {
      recordCheck("P1-device", "NOT RUN", "navigator.gpu is unavailable; WebGPU is not exposed in this browser");
      return { status: "UNSUPPORTED", reason: "WebGPU unavailable" };
    }

    try {
      adapter = await gpu.requestAdapter({ powerPreference: "high-performance" });
    } catch (error) {
      recordCheck("P1-device", "NOT RUN", `requestAdapter threw: ${String(error?.message ?? error)}`);
      return { status: "UNSUPPORTED", reason: "adapter request failed" };
    }
    if (!adapter) {
      recordCheck("P1-device", "NOT RUN", "requestAdapter returned null; no hardware adapter is exposed");
      return { status: "UNSUPPORTED", reason: "no adapter" };
    }

    try {
      device = await adapter.requestDevice({ label: "q7-profile-device" });
    } catch (error) {
      recordCheck("P1-device", "NOT RUN", `requestDevice threw: ${String(error?.message ?? error)}`);
      return { status: "UNSUPPORTED", reason: "device request failed" };
    }

    device.lost?.then((info) => {
      deviceLost = { reason: info?.reason ?? "unknown", message: String(info?.message ?? "") };
      loop?.stop();
    });
    device.onuncapturederror = (event) => {
      errors.push(String(event?.error?.message ?? event?.error ?? event));
    };

    const info = adapterInfoOf(adapter);
    const limits = device.limits ?? {};
    capability = evaluateCapability({
      targetId: WEB_PROFILE_ID,
      adapter: info,
      limits,
      features: featureList(device.features),
    });

    recordCheck("P1-device", "PASS", "adapter and device created", {
      adapter: info,
      features: featureList(device.features),
      limits: { maxBindGroups: limits.maxBindGroups, maxTextureDimension2D: limits.maxTextureDimension2D },
    });

    const capabilityOk = capability.status !== "UNSUPPORTED";
    recordCheck(
      "P2-capability-table",
      capabilityOk ? "PASS" : "FAIL",
      `capability status ${capability.status}; ${capability.unsupported.length} unsupported row(s)`,
      { status: capability.status, unsupported: capability.unsupported, fallbacks: capability.fallbacks },
    );
    if (!capabilityOk) return { status: "UNSUPPORTED", reason: "capability table refused the target" };

    try {
      swapchain = createSwapchain({
        canvas,
        device,
        format: options.format ?? DEFAULT_FORMAT,
        maxDimension: Number(limits.maxTextureDimension2D) || 8192,
      });
      swapchain.configure();
      scene = createProceduralScene({ device, format: swapchain.getConfig().format });
      input = createInputBridge({ target: page, element: canvas, document: page.document });
      audio = createAudioBridge({
        createContext: () => {
          const Ctor = page.AudioContext || page.webkitAudioContext;
          return Ctor ? new Ctor() : null;
        },
      });
      loop = createMainLoop({
        step: () => {},
        render: () => renderFrame(),
        now: () => page.performance?.now?.() ?? Date.now(),
        schedule: (callback) => page.requestAnimationFrame(callback),
        cancel: (handle) => page.cancelAnimationFrame?.(handle),
      });
      renderFrame();
      recordCheck("P3-swapchain", "PASS", "canvas configured and a frame texture acquired", {
        config: swapchain.getConfig(),
        size: swapchain.getSize(),
        stats: swapchain.getStats(),
      });
      recordCheck("P4-procedural-frame", "PASS", `procedural field rendered ${scene.getDraws()} draw(s)`, {
        scene: PROCEDURAL_SCENE_ID,
        draws: scene.getDraws(),
      });
    } catch (error) {
      recordCheck("P3-swapchain", "FAIL", `swapchain/scene setup failed: ${String(error?.message ?? error)}`);
      return { status: "FAIL", reason: String(error?.message ?? error) };
    }

    await runVfsSelfTest();
    runCapabilitySelfTest();

    const cue = audio.playCue();
    const gateOk = cue.status === "NOT RUN" && String(cue.reason).includes("gesture");
    recordCheck(
      "P8-audio-gate",
      gateOk ? "PASS" : "FAIL",
      gateOk ? "audio refused to play before a user gesture" : "audio did not enforce the gesture gate",
      cue,
    );

    recordCheck("P7b-pointer-lock", "NOT RUN", PROFILE_EXPECTED_NOT_RUN["P7b-pointer-lock"]);
    recordCheck("P8b-browser-audio", "NOT RUN", PROFILE_EXPECTED_NOT_RUN["P8b-browser-audio"]);
    recordCheck("R1-real-scene", "NOT RUN", PROFILE_EXPECTED_NOT_RUN["R1-real-scene"]);
    return { status: capability.status };
  }

  function start() {
    if (!loop) return false;
    startedAtMs = page.performance?.now?.() ?? Date.now();
    return loop.start();
  }

  function stop() {
    return loop?.stop() ?? false;
  }

  function requestCapture(timeoutMs = 5000) {
    if (!device) return Promise.resolve({ status: "FAIL", reason: "device was not created" });
    return new Promise((resolve) => {
      let settled = false;
      const finish = (value) => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        resolve(value);
      };
      const timer = setTimeout(() => finish({ status: "FAIL", reason: "canvas readback timed out" }), timeoutMs);
      captureRequest = finish;
      if (!loop?.isRunning()) renderFrame();
    });
  }

  function getResult({ runNonce = null, capture = null, inputTest = null, mode = "procedural" } = {}) {
    selfTestInput();

    if (loop) {
      const stats = loop.getStats();
      recordCheck(
        "P6-main-loop",
        stats.ticks > 0 && stats.frames > 0 ? "PASS" : "FAIL",
        `loop ran ${stats.ticks} fixed tick(s) over ${stats.frames} host frame(s)`,
        stats,
      );
    } else {
      recordCheck("P6-main-loop", "NOT RUN", "loop was not created");
    }

    if (capture) {
      const ok = capture.status === "OK" && capture.nonBlack > 0;
      recordCheck(
        "P4b-canvas-readback",
        ok ? "PASS" : "FAIL",
        ok ? `canvas readback found ${capture.nonBlack} non-black pixel(s)` : "canvas readback failed or was blank",
        capture,
      );
    } else {
      recordCheck("P4b-canvas-readback", "FAIL", "no canvas readback was requested");
    }

    if (deviceLost) {
      recordCheck("E-device-loss", "FAIL", `device was lost: ${deviceLost.reason} ${deviceLost.message}`.trim(), deviceLost);
    } else {
      recordCheck("E-device-loss", "NOT RUN", "no device loss was induced in this harness run; Q7a covers the loss path");
    }

    recordCheck(
      "E-uncaptured",
      errors.length === 0 ? "PASS" : "FAIL",
      `${errors.length} uncaptured WebGPU error(s)`,
      { errors: [...errors] },
    );

    for (const id of [...PROFILE_REQUIRED_CHECKS, ...Object.keys(PROFILE_EXPECTED_NOT_RUN)]) {
      if (!checks.has(id)) {
        recordCheck(id, "NOT RUN", `check ${id} did not execute; initialization stopped before it`);
      }
    }

    const ordered = [...checks.values()].sort((a, b) => {
      const ai = CHECK_ORDER.indexOf(a.id);
      const bi = CHECK_ORDER.indexOf(b.id);
      return (ai < 0 ? 999 : ai) - (bi < 0 ? 999 : bi);
    });

    return {
      schema: PROFILE_SCHEMA,
      stage: "Q7b WebGPU browser profile host slice",
      targetId: WEB_PROFILE_ID,
      contract: CAPABILITY_SCHEMA,
      scene: { id: PROCEDURAL_SCENE_ID, kind: "procedural", realScene: false, mode },
      realScene: { status: "NOT RUN", reason: NO_GAME_DATA_REASON },
      env: {
        runNonce,
        userAgent: String(page.navigator?.userAgent ?? "unknown"),
        page: String(page.location?.href ?? ""),
      },
      capability,
      checks: ordered,
      summary: summarize(ordered),
      errors: [...errors],
      loop: loop ? loop.getStats() : null,
      input: inputTest ?? null,
    };
  }

  function dispose() {
    loop?.stop();
    input?.dispose();
    scene?.dispose();
    device?.destroy?.();
  }

  return {
    init,
    start,
    stop,
    dispose,
    requestCapture,
    getResult,
    selfTestInput,
    getChecks: () => [...checks.values()],
    getCapability: () => capability,
    getDevice: () => device,
    getAdapter: () => adapter,
    getLoopStats: () => loop?.getStats() ?? null,
  };
}
