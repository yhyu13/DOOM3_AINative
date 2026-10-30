// Q7b input bridge.
//
// Replaces the native DirectInput path with browser keyboard/mouse state. Pointer
// lock is never requested automatically: `requestPointerLock()` is an explicit
// call the host makes from a user gesture.

const GAME_KEYS = new Set([
  "KeyW", "KeyA", "KeyS", "KeyD",
  "ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight",
  "Space", "ShiftLeft", "ShiftRight", "ControlLeft", "ControlRight",
  "Escape", "Tab", "Enter",
  "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6", "Digit7", "Digit8", "Digit9", "Digit0",
]);

function removedListeners() {
  return [];
}

/**
 * @param {object} options
 * @param {EventTarget} options.target       window-shaped event source
 * @param {object} options.element           canvas-shaped element for pointer lock
 * @param {object} [options.document]        document-shaped object
 */
export function createInputBridge({ target, element, document: doc = globalThis.document } = {}) {
  if (!target || typeof target.addEventListener !== "function") {
    throw new Error("target with addEventListener is required");
  }
  const state = {
    keys: new Set(),
    buttons: 0,
    dx: 0,
    dy: 0,
    wheel: 0,
    focused: true,
    locked: false,
    events: 0,
  };
  const bound = removedListeners();

  function on(type, handler, options) {
    target.addEventListener(type, handler, options);
    bound.push([type, handler, options]);
  }

  function onKeyDown(event) {
    state.events += 1;
    if (event && GAME_KEYS.has(event.code)) event.preventDefault?.();
    if (event?.code) state.keys.add(event.code);
  }
  function onKeyUp(event) {
    state.events += 1;
    if (event && GAME_KEYS.has(event.code)) event.preventDefault?.();
    if (event?.code) state.keys.delete(event.code);
  }
  function onMouseMove(event) {
    state.events += 1;
    if (!state.locked) return;
    state.dx += Number(event?.movementX) || 0;
    state.dy += Number(event?.movementY) || 0;
  }
  function onMouseDown(event) {
    state.events += 1;
    const button = Number(event?.button);
    if (Number.isInteger(button) && button >= 0 && button < 8) state.buttons |= 1 << button;
  }
  function onMouseUp(event) {
    state.events += 1;
    const button = Number(event?.button);
    if (Number.isInteger(button) && button >= 0 && button < 8) state.buttons &= ~(1 << button);
  }
  function onWheel(event) {
    state.events += 1;
    state.wheel += Number(event?.deltaY) || 0;
  }
  function onBlur() {
    state.events += 1;
    state.keys.clear();
    state.buttons = 0;
    state.dx = 0;
    state.dy = 0;
    state.wheel = 0;
    state.focused = false;
  }
  function onFocus() {
    state.events += 1;
    state.focused = true;
  }
  function onPointerLockChange() {
    state.locked = Boolean(element && doc && doc.pointerLockElement === element);
    if (!state.locked) {
      state.dx = 0;
      state.dy = 0;
    }
  }

  on("keydown", onKeyDown);
  on("keyup", onKeyUp);
  on("mousemove", onMouseMove);
  on("mousedown", onMouseDown);
  on("mouseup", onMouseUp);
  on("wheel", onWheel, { passive: true });
  on("blur", onBlur);
  on("focus", onFocus);
  if (doc && typeof doc.addEventListener === "function") doc.addEventListener("pointerlockchange", onPointerLockChange);

  function snapshot() {
    return {
      keys: [...state.keys].sort(),
      buttons: state.buttons,
      dx: state.dx,
      dy: state.dy,
      wheel: state.wheel,
      focused: state.focused,
      locked: state.locked,
      events: state.events,
    };
  }

  return {
    snapshot,
    /** Return the frame's input and reset one-shot mouse deltas. */
    consumeFrame() {
      const frame = snapshot();
      state.dx = 0;
      state.dy = 0;
      state.wheel = 0;
      return frame;
    },
    /**
     * Explicit gesture-time pointer-lock request. Never called by the bridge
     * itself, so a page cannot silently capture the pointer on load.
     */
    async requestPointerLock() {
      if (!element || typeof element.requestPointerLock !== "function") {
        return { status: "UNSUPPORTED", reason: "element.requestPointerLock is unavailable" };
      }
      try {
        const result = element.requestPointerLock();
        if (result && typeof result.then === "function") await result;
        return { status: "REQUESTED" };
      } catch (error) {
        return { status: "BLOCKED", reason: String(error?.message ?? error) };
      }
    },
    dispose() {
      for (const [type, handler, options] of bound) target.removeEventListener?.(type, handler, options);
      bound.length = 0;
      if (doc && typeof doc.removeEventListener === "function") doc.removeEventListener("pointerlockchange", onPointerLockChange);
    },
    _state: state,
  };
}
