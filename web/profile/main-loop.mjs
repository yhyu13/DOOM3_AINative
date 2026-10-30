// Q7b cooperative main loop.
//
// The native engine runs `while (1) { Win_Frame(); common->Frame(); }`. A browser
// tab cannot. This module inverts that loop: the host schedules one frame callback
// (requestAnimationFrame in the browser), the loop advances at a fixed 60 Hz tic
// with a bounded catch-up budget, and rendering happens once per host frame.
//
// The pure `createLoopState`/`advanceLoop` pair is deterministic and covered by
// Node tests; `createMainLoop` only adds scheduling.

export const DEFAULT_FIXED_STEP_MS = 1000 / 60;
export const DEFAULT_MAX_CATCH_UP_TICKS = 4;
export const DEFAULT_MAX_FRAME_MS = 250;

export function createLoopState() {
  return {
    lastTimestampMs: null,
    accumulatorMs: 0,
    ticks: 0,
    frames: 0,
    droppedMs: 0,
    paused: false,
  };
}

/**
 * Advance the deterministic loop state by one host frame.
 * Returns the number of fixed steps to run and the interpolation alpha.
 */
export function advanceLoop(state, timestampMs, options = {}) {
  const fixedStepMs = options.fixedStepMs ?? DEFAULT_FIXED_STEP_MS;
  const maxTicksPerFrame = options.maxTicksPerFrame ?? DEFAULT_MAX_CATCH_UP_TICKS;
  const maxFrameMs = options.maxFrameMs ?? DEFAULT_MAX_FRAME_MS;
  if (!(fixedStepMs > 0)) throw new Error("fixedStepMs must be positive");
  if (!Number.isInteger(maxTicksPerFrame) || maxTicksPerFrame < 1) throw new Error("maxTicksPerFrame must be a positive integer");
  if (!(maxFrameMs > 0)) throw new Error("maxFrameMs must be positive");

  const timestamp = Number(timestampMs);
  if (!Number.isFinite(timestamp)) throw new Error("timestampMs must be finite");

  state.frames += 1;
  if (state.paused) {
    state.lastTimestampMs = timestamp;
    return { steps: 0, alpha: 0, deltaMs: 0 };
  }

  let deltaMs = 0;
  if (state.lastTimestampMs !== null) {
    deltaMs = Math.min(Math.max(0, timestamp - state.lastTimestampMs), maxFrameMs);
  }
  state.lastTimestampMs = timestamp;

  let accumulatorMs = state.accumulatorMs + deltaMs;
  let steps = Math.floor(accumulatorMs / fixedStepMs);
  if (steps > maxTicksPerFrame) {
    state.droppedMs += (steps - maxTicksPerFrame) * fixedStepMs;
    steps = maxTicksPerFrame;
    accumulatorMs = Math.min(accumulatorMs, fixedStepMs * maxTicksPerFrame);
  }
  accumulatorMs -= steps * fixedStepMs;
  if (accumulatorMs < 0) accumulatorMs = 0;

  state.accumulatorMs = accumulatorMs;
  state.ticks += steps;
  return { steps, alpha: accumulatorMs / fixedStepMs, deltaMs };
}

/**
 * Schedule a loop through injected host functions.
 *
 * @param {object} options
 * @param {(fixedDtSeconds:number, tick:number)=>void} options.step
 * @param {(alpha:number, frame:object)=>void} [options.render]
 * @param {()=>number} [options.now]
 * @param {(cb:(t:number)=>void)=>unknown} [options.schedule]
 * @param {(handle:unknown)=>void} [options.cancel]
 */
export function createMainLoop({
  step,
  render,
  now = () => performance.now(),
  schedule = (cb) => globalThis.requestAnimationFrame(cb),
  cancel = (handle) => globalThis.cancelAnimationFrame?.(handle),
  fixedStepMs = DEFAULT_FIXED_STEP_MS,
  maxTicksPerFrame = DEFAULT_MAX_CATCH_UP_TICKS,
  maxFrameMs = DEFAULT_MAX_FRAME_MS,
} = {}) {
  if (typeof step !== "function") throw new Error("step callback is required");
  const state = createLoopState();
  let running = false;
  let handle = null;

  function frame(timestamp) {
    if (!running) return;
    handle = null;
    const result = advanceLoop(state, Number.isFinite(timestamp) ? timestamp : now(), {
      fixedStepMs,
      maxTicksPerFrame,
      maxFrameMs,
    });
    for (let i = 0; i < result.steps; i++) {
      step(fixedStepMs / 1000, state.ticks - result.steps + i + 1);
    }
    if (typeof render === "function") render(result.alpha, { ...state, steps: result.steps, dropped: result.deltaMs > maxFrameMs });
    if (running) handle = schedule(frame);
  }

  return {
    start() {
      if (running) return false;
      running = true;
      state.paused = false;
      state.lastTimestampMs = null;
      handle = schedule(frame);
      return true;
    },
    stop() {
      if (!running) return false;
      running = false;
      state.paused = false;
      if (handle !== null) cancel(handle);
      handle = null;
      return true;
    },
    pause() {
      state.paused = true;
    },
    resume() {
      state.paused = false;
      state.lastTimestampMs = null;
    },
    isRunning: () => running,
    getStats: () => ({ ...state }),
  };
}

/**
 * Give the host a chance to run input/audio/paint work between long setup steps.
 */
export function yieldToHost() {
  return new Promise((resolve) => setTimeout(resolve, 0));
}
