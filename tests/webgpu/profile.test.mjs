// Q7b host-slice unit tests.
//
// These tests cover the deterministic seams only. They do not replace a browser
// run and make no WebGPU support claim; the browser harness is run-profile.mjs.

import { test } from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

import {
  OPTIONAL_FEATURES,
  REQUIRED_LIMITS,
  WEB_PROFILE_ID,
  evaluateCapability,
} from "../../web/profile/capability.mjs";
import {
  NO_GAME_DATA_REASON,
  VfsError,
  createMemoryMount,
  createVfs,
  normalizeVirtualPath,
} from "../../web/profile/vfs.mjs";
import { advanceLoop, createLoopState, createMainLoop } from "../../web/profile/main-loop.mjs";
import {
  COPY_SRC,
  RENDER_ATTACHMENT,
  computeCanvasSize,
  createSwapchain,
} from "../../web/profile/swapchain.mjs";
import { createInputBridge } from "../../web/profile/input.mjs";
import { AUDIO_GESTURE_REASON, createAudioBridge } from "../../web/profile/audio.mjs";
import {
  SCENE_UNIFORM_BYTES,
  sceneUniformBytes,
} from "../../web/profile/scene.mjs";
import {
  PROFILE_EXPECTED_NOT_RUN,
  PROFILE_REQUIRED_CHECKS,
} from "../../web/profile/profile.mjs";
import { capabilityProblems, summarize } from "./run-profile.mjs";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, "..", "..");

function fullLimits() {
  return Object.fromEntries(REQUIRED_LIMITS.map((row) => [row.name, row.required]));
}

function allFeatures() {
  return OPTIONAL_FEATURES.map((feature) => feature.name);
}

function makeTarget() {
  const listeners = new Map();
  return {
    addEventListener(type, fn) {
      if (!listeners.has(type)) listeners.set(type, new Set());
      listeners.get(type).add(fn);
    },
    removeEventListener(type, fn) {
      listeners.get(type)?.delete(fn);
    },
    dispatch(type, event = {}) {
      for (const fn of listeners.get(type) ?? []) fn(event);
    },
    listenerCount(type) {
      return listeners.get(type)?.size ?? 0;
    },
  };
}

function makeDocument() {
  return makeTarget();
}

class FakeParam {
  constructor() {
    this.value = 0;
  }
  setValueAtTime(value) {
    this.value = value;
  }
  linearRampToValueAtTime(value) {
    this.value = value;
  }
  exponentialRampToValueAtTime(value) {
    this.value = value;
  }
}

class FakeNode {
  connect() {
    return this;
  }
}

class FakeOscillator extends FakeNode {
  constructor() {
    super();
    this.frequency = new FakeParam();
    this.type = "sine";
  }
  start() {}
  stop() {}
}

class FakeGain extends FakeNode {
  constructor() {
    super();
    this.gain = new FakeParam();
  }
}

class FakeAudioContext {
  constructor() {
    this.state = "suspended";
    this.currentTime = 0;
    this.destination = new FakeNode();
    this.oscillators = 0;
  }
  createGain() {
    return new FakeGain();
  }
  createOscillator() {
    this.oscillators += 1;
    return new FakeOscillator();
  }
  async resume() {
    this.state = "running";
  }
  async suspend() {
    this.state = "suspended";
  }
}

test("capability: complete adapter is READY", () => {
  const table = evaluateCapability({
    targetId: WEB_PROFILE_ID,
    adapter: { description: "synthetic", vendor: "test", vendorId: 0, deviceID: 0 },
    limits: fullLimits(),
    features: allFeatures(),
  });
  assert.equal(table.status, "READY");
  assert.equal(table.unsupported.length, 0);
  assert.equal(table.rows.length, REQUIRED_LIMITS.length + OPTIONAL_FEATURES.length);
  assert.ok(table.rows.every((row) => row.status === "PASS"));
  assert.equal(table.adapter.reportedFacts, "0x0");
});

test("capability: missing quality feature selects an explicit fallback", () => {
  const table = evaluateCapability({
    targetId: WEB_PROFILE_ID,
    adapter: { description: "synthetic" },
    limits: fullLimits(),
    features: allFeatures().filter((name) => name !== "texture-compression-bc"),
  });
  assert.equal(table.status, "DEGRADED");
  assert.ok(table.fallbacks.some((fallback) => fallback.id === "bc-transcode-required"));
  const row = table.rows.find((entry) => entry.name === "texture-compression-bc");
  assert.equal(row.status, "FALLBACK");
  assert.equal(row.fallback, "bc-transcode-required");
});

test("capability: a short required limit is UNSUPPORTED, not a fallback", () => {
  const table = evaluateCapability({
    targetId: WEB_PROFILE_ID,
    adapter: { description: "synthetic" },
    limits: { ...fullLimits(), maxBindGroups: 3 },
    features: allFeatures(),
  });
  assert.equal(table.status, "UNSUPPORTED");
  assert.ok(table.unsupported.some((entry) => entry.id === "limit:maxBindGroups"));
});

test("capability: a Windows target cannot stand in for the browser profile", () => {
  const table = evaluateCapability({
    targetId: "win11-amd64",
    adapter: { description: "synthetic" },
    limits: fullLimits(),
    features: allFeatures(),
  });
  assert.equal(table.status, "UNSUPPORTED");
  assert.ok(table.unsupported.some((entry) => entry.id === "target-identity"));
});

test("capability: the contract document names every executable row", () => {
  const text = fs.readFileSync(path.join(ROOT, "specs", "004-quality-renderer", "portable-profile-contract.md"), "utf8");
  for (const row of REQUIRED_LIMITS) assert.match(text, new RegExp(`\\b${row.name}\\b`), row.name);
  for (const feature of OPTIONAL_FEATURES) {
    assert.match(text, new RegExp(`\\b${feature.name}\\b`), feature.name);
    assert.match(text, new RegExp(`\\b${feature.fallback}\\b`), feature.fallback);
  }
});

test("vfs: memory mount reads, misses with a typed error, and refuses traversal", async () => {
  const vfs = createVfs();
  vfs.mount(createMemoryMount("demo", { "/demo/hello.txt": "hello q7\n" }, { prefix: "/demo" }));
  assert.equal(await vfs.readText("/demo/hello.txt"), "hello q7\n");
  await assert.rejects(() => vfs.readText("/demo/missing.txt"), (error) => error.code === "ERR_NOT_FOUND");
  assert.throws(() => normalizeVirtualPath("/demo/../secret"), (error) => error.code === "ERR_PATH_TRAVERSAL");
  assert.throws(() => normalizeVirtualPath(""), (error) => error instanceof VfsError);
});

test("vfs: legal game data is NOT RUN and pak parsing is explicitly UNSUPPORTED", () => {
  const vfs = createVfs();
  const game = vfs.openGameData();
  assert.equal(game.status, "NOT RUN");
  assert.equal(game.reason, NO_GAME_DATA_REASON);
  const pak = vfs.readPakDirectory();
  assert.equal(pak.status, "UNSUPPORTED");
  assert.match(pak.reason, /not implemented/);
});

test("vfs: a user-supplied legal mount changes only the game-data result", async () => {
  const vfs = createVfs();
  const result = vfs.mountUserData(createMemoryMount("user-data", { "/data/pak0.pk4": "not-a-real-pak" }, { prefix: "/data" }));
  assert.equal(result.status, "OK");
  assert.equal(result.mount, "user-data");
  assert.equal((await vfs.readFile("/data/pak0.pk4")).byteLength, 14);
  assert.equal(vfs.openGameData().status, "OK");
});

test("loop: fixed ticks, bounded catch-up and no negative accumulator", () => {
  const state = createLoopState();
  assert.equal(advanceLoop(state, 0).steps, 0);
  assert.equal(advanceLoop(state, 1000 / 60).steps, 1);
  const slow = advanceLoop(state, 1000);
  assert.equal(slow.steps, 4);
  assert.ok(state.droppedMs > 0);
  assert.ok(state.accumulatorMs >= 0);
});

test("loop: pause discards elapsed time instead of replaying it", () => {
  const state = createLoopState();
  advanceLoop(state, 0);
  state.paused = true;
  const paused = advanceLoop(state, 5000);
  assert.equal(paused.steps, 0);
  state.paused = false;
  state.lastTimestampMs = null;
  assert.equal(advanceLoop(state, 5000).steps, 0);
  assert.equal(advanceLoop(state, 5000 + 1000 / 60).steps, 1);
});

test("loop: the scheduler drives one render per host frame", () => {
  let callback = null;
  const steps = [];
  const renders = [];
  const loop = createMainLoop({
    step: (dt, tick) => steps.push({ dt, tick }),
    render: (alpha, frame) => renders.push({ alpha, steps: frame.steps }),
    schedule: (fn) => {
      callback = fn;
      return 1;
    },
    cancel: () => {},
    fixedStepMs: 10,
    maxTicksPerFrame: 2,
  });
  assert.equal(loop.start(), true);
  callback(0);
  callback(20);
  assert.deepEqual(steps.map((entry) => entry.tick), [1, 2]);
  assert.equal(renders.length, 2);
  assert.equal(loop.getStats().frames, 2);
  assert.equal(loop.stop(), true);
  assert.equal(loop.isRunning(), false);
});

test("swapchain: canvas size clamps DPR and the device dimension", () => {
  assert.deepEqual(computeCanvasSize({ cssWidth: 800, cssHeight: 600, devicePixelRatio: 3, maxDpr: 2 }), {
    width: 1600,
    height: 1200,
    dpr: 2,
    clamped: false,
    reason: "none",
  });
  const clamped = computeCanvasSize({ cssWidth: 6000, cssHeight: 4000, devicePixelRatio: 2, maxDimension: 8192 });
  assert.equal(Math.max(clamped.width, clamped.height), 8192);
  assert.equal(clamped.clamped, true);
  assert.equal(clamped.reason, "maxTextureDimension2D");
});

test("swapchain: configure and acquire pass the declared format and usage", () => {
  const calls = [];
  const view = { label: "fake-view" };
  const canvas = {
    width: 0,
    height: 0,
    clientWidth: 800,
    clientHeight: 600,
    getContext: () => ({
      configure: (config) => calls.push(config),
      getCurrentTexture: () => ({ createView: () => view }),
    }),
  };
  const device = { limits: { maxTextureDimension2D: 8192 } };
  const swapchain = createSwapchain({ canvas, device, format: "bgra8unorm" });
  const size = swapchain.configure();
  assert.equal(size.width, 800);
  assert.equal(size.height, 600);
  assert.equal(calls.length, 1);
  assert.equal(calls[0].format, "bgra8unorm");
  assert.equal(calls[0].alphaMode, "opaque");
  assert.equal(calls[0].usage, RENDER_ATTACHMENT | COPY_SRC);
  const acquired = swapchain.acquire();
  assert.equal(acquired.view, view);
  assert.equal(swapchain.getStats().acquisitions, 1);
});

test("input: keys, pointer lock and frame deltas are explicit", async () => {
  const target = makeTarget();
  const doc = makeDocument();
  const element = {
    requestPointerLock() {
      doc.pointerLockElement = element;
      doc.dispatch("pointerlockchange", {});
      return undefined;
    },
  };
  const bridge = createInputBridge({ target, element, document: doc });

  target.dispatch("keydown", { code: "KeyW", preventDefault() {} });
  assert.deepEqual(bridge.snapshot().keys, ["KeyW"]);
  target.dispatch("keyup", { code: "KeyW", preventDefault() {} });
  assert.deepEqual(bridge.snapshot().keys, []);

  target.dispatch("mousemove", { movementX: 5, movementY: 6 });
  assert.equal(bridge.snapshot().dx, 0, "movement is ignored without pointer lock");

  assert.equal((await bridge.requestPointerLock()).status, "REQUESTED");
  assert.equal(bridge.snapshot().locked, true);
  target.dispatch("mousemove", { movementX: 5, movementY: 6 });
  assert.equal(bridge.snapshot().dx, 5);
  const frame = bridge.consumeFrame();
  assert.equal(frame.dx, 5);
  assert.equal(bridge.snapshot().dx, 0);

  target.dispatch("blur", {});
  assert.equal(bridge.snapshot().focused, false);
  bridge.dispose();
  assert.equal(target.listenerCount("keydown"), 0);
});

test("audio: playback is refused before a gesture and works after unlock", async () => {
  const context = new FakeAudioContext();
  const bridge = createAudioBridge({ createContext: () => context });
  assert.equal(bridge.getState().state, "locked");
  const refused = bridge.playCue();
  assert.equal(refused.status, "NOT RUN");
  assert.equal(refused.reason, AUDIO_GESTURE_REASON);
  assert.equal((await bridge.unlock()).status, "OK");
  assert.equal(context.state, "running");
  assert.equal(bridge.playCue({ frequency: 220, durationMs: 40 }).status, "OK");
  assert.equal(bridge.getState().cues, 1);
  assert.equal(context.oscillators, 1);
  await bridge.suspend();
  assert.equal(bridge.getState().state, "suspended");
});

test("audio: a context that stays suspended is BLOCKED, not a pass", async () => {
  class StuckAudioContext extends FakeAudioContext {
    async resume() {}
  }
  const bridge = createAudioBridge({ createContext: () => new StuckAudioContext() });
  const result = await bridge.unlock();
  assert.equal(result.status, "BLOCKED");
  assert.match(result.reason, /suspended/);
});

test("scene: uniform layout is stable", () => {
  const bytes = sceneUniformBytes(1.5, 640, 360);
  assert.equal(SCENE_UNIFORM_BYTES, 16);
  assert.equal(bytes.byteLength, SCENE_UNIFORM_BYTES);
  assert.deepEqual([...bytes], [1.5, 640, 360, 0]);
});

test("profile: required and expected-NOT-RUN checks are declared", () => {
  assert.ok(PROFILE_REQUIRED_CHECKS.includes("P1-device"));
  assert.ok(PROFILE_REQUIRED_CHECKS.includes("P4b-canvas-readback"));
  assert.ok(PROFILE_REQUIRED_CHECKS.includes("E-uncaptured"));
  assert.match(PROFILE_EXPECTED_NOT_RUN["R1-real-scene"], /no legal DOOM 3 game data/i);
  assert.match(PROFILE_EXPECTED_NOT_RUN["P7b-pointer-lock"], /gesture/i);
  assert.match(PROFILE_EXPECTED_NOT_RUN["P8b-browser-audio"], /gesture/i);
});

test("browser pin is explicit and carries the unsafe-webgpu flag", () => {
  const pin = JSON.parse(fs.readFileSync(path.join(ROOT, "tests", "webgpu", "browser-pin.json"), "utf8"));
  assert.equal(pin.schema, "q7-pinned-browser/1");
  assert.equal(typeof pin.major, "number");
  assert.equal(typeof pin.evidenceVersion, "string");
  assert.ok(pin.flags.includes("--enable-unsafe-webgpu"));
  assert.match(pin.name, /Chrome/);
});

function completeProfileResult() {
  const checks = [
    ...PROFILE_REQUIRED_CHECKS.map((id) => ({ id, status: "PASS", detail: "executed" })),
    ...Object.entries(PROFILE_EXPECTED_NOT_RUN).map(([id, detail]) => ({ id, status: "NOT RUN", detail })),
  ];
  return {
    targetId: WEB_PROFILE_ID,
    capability: evaluateCapability({
      targetId: WEB_PROFILE_ID,
      adapter: { description: "synthetic target adapter" },
      limits: fullLimits(),
      features: allFeatures(),
    }),
    scene: { realScene: false },
    realScene: { status: "NOT RUN" },
    checks,
  };
}

test("harness report: a complete target-adapter result has no findings", () => {
  const { missing, failed } = summarize(completeProfileResult());
  assert.deepEqual(missing, []);
  assert.deepEqual(failed, []);
  assert.deepEqual(capabilityProblems(completeProfileResult()), []);
});

test("harness report: a result without the live capability table is refused", () => {
  const result = completeProfileResult();
  result.capability = null;
  assert.ok(summarize(result).failed.includes("capability:missing"));
});

test("harness report: a capability table for another target is refused", () => {
  const result = completeProfileResult();
  result.capability = evaluateCapability({
    targetId: "win11-amd64",
    adapter: { description: "synthetic native adapter" },
    limits: fullLimits(),
    features: allFeatures(),
  });
  assert.ok(summarize(result).failed.some((entry) => entry.startsWith("capability:target-mismatch")));
});

test("harness report: a capability table missing a required limit row is refused", () => {
  const result = completeProfileResult();
  result.capability.rows = result.capability.rows.filter((row) => row.name !== "maxBufferSize");
  assert.ok(summarize(result).failed.includes("capability:missing-limit(maxBufferSize)"));
});
