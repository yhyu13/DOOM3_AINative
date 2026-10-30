// Q7b canvas / swapchain adapter.
//
// WebGPU has no Present() call: the canvas is configured once, each frame asks
// for its current texture, and compositing is implicit. This module keeps that
// host detail behind a small seam and clamps the drawing buffer to the device
// limit and a declared DPR fallback.

export const DEFAULT_FORMAT = "bgra8unorm";
export const RENDER_ATTACHMENT = 0x10;
export const COPY_SRC = 0x01;
export const DEFAULT_USAGE = RENDER_ATTACHMENT | COPY_SRC;
export const DEFAULT_MAX_DPR = 2;

/**
 * Compute a clamped integer drawing-buffer size.
 * `maxDpr` is an explicit quality fallback: above it the canvas renders at a
 * lower density rather than exceeding memory or fill-rate budgets.
 */
export function computeCanvasSize({
  cssWidth = 1,
  cssHeight = 1,
  devicePixelRatio = 1,
  maxDimension = 8192,
  maxDpr = DEFAULT_MAX_DPR,
  minDimension = 1,
} = {}) {
  const dpr = Math.min(Math.max(Number(devicePixelRatio) || 1, 1), Math.max(1, Number(maxDpr) || 1));
  const maxSide = Math.max(1, Math.floor(Number(maxDimension) || 8192));
  const minSide = Math.max(1, Math.floor(Number(minDimension) || 1));
  let width = Math.max(minSide, Math.round((Number(cssWidth) || minSide) * dpr));
  let height = Math.max(minSide, Math.round((Number(cssHeight) || minSide) * dpr));
  let clamped = false;
  let reason = "none";
  if (width > maxSide || height > maxSide) {
    const scale = maxSide / Math.max(width, height);
    width = Math.max(minSide, Math.floor(width * scale));
    height = Math.max(minSide, Math.floor(height * scale));
    clamped = true;
    reason = "maxTextureDimension2D";
  }
  return { width, height, dpr, clamped, reason };
}

/**
 * Configure a WebGPU canvas context and expose frame acquisition.
 */
export function createSwapchain({
  canvas,
  device,
  context,
  format = DEFAULT_FORMAT,
  usage = DEFAULT_USAGE,
  maxDpr = DEFAULT_MAX_DPR,
  maxDimension,
} = {}) {
  if (!canvas) throw new Error("canvas is required");
  if (!device) throw new Error("device is required");
  const ctx = context || canvas.getContext?.("webgpu");
  if (!ctx) throw new Error('canvas.getContext("webgpu") returned null');
  const limit = Number(maxDimension ?? device.limits?.maxTextureDimension2D ?? 8192);
  let size = null;
  let configured = false;
  let acquisitions = 0;

  function measure() {
    const cssWidth = Number(canvas.clientWidth ?? canvas.width ?? 1);
    const cssHeight = Number(canvas.clientHeight ?? canvas.height ?? 1);
    const dpr = globalThis.devicePixelRatio ?? canvas.ownerDocument?.defaultView?.devicePixelRatio ?? 1;
    return computeCanvasSize({ cssWidth, cssHeight, devicePixelRatio: dpr, maxDimension: limit, maxDpr });
  }

  function configure() {
    size = measure();
    canvas.width = size.width;
    canvas.height = size.height;
    ctx.configure({ device, format, alphaMode: "opaque", usage });
    configured = true;
    return { ...size };
  }

  function resize() {
    const next = measure();
    if (!size || next.width !== size.width || next.height !== size.height) {
      size = next;
      canvas.width = next.width;
      canvas.height = next.height;
    }
    return { ...size };
  }

  function acquire() {
    if (!configured) configure();
    const current = measure();
    if (current.width !== size.width || current.height !== size.height) {
      size = current;
      canvas.width = current.width;
      canvas.height = current.height;
    }
    const texture = ctx.getCurrentTexture();
    acquisitions += 1;
    return { texture, view: texture.createView({ label: "q7-canvas-view" }), size: { ...size } };
  }

  return {
    configure,
    resize,
    acquire,
    getSize: () => ({ ...size }),
    getConfig: () => ({ format, alphaMode: "opaque", usage }),
    getStats: () => ({ configured, acquisitions, size: size ? { ...size } : null }),
  };
}
