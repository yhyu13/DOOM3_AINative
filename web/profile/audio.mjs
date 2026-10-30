// Q7b audio bridge.
//
// Browsers create Web Audio in a suspended state and require a user gesture to
// resume it. EAX has no Web Audio equivalent; this bridge only exposes the
// gesture-gated lifecycle and a synthetic cue used by the procedural harness.

export const AUDIO_GESTURE_REASON =
  "Web Audio requires a user gesture; no gesture was supplied, so audio stays locked";

/**
 * @param {object} options
 * @param {() => any} [options.createContext] AudioContext factory
 */
export function createAudioBridge({ createContext } = {}) {
  const factory =
    createContext ||
    (() => {
      const Ctor = globalThis.AudioContext || globalThis.webkitAudioContext;
      if (!Ctor) return null;
      return new Ctor();
    });

  let context = null;
  let master = null;
  let state = "locked";
  let lastError = null;
  let cues = 0;

  async function unlock() {
    try {
      if (!context) {
        context = factory();
        if (!context) {
          state = "unsupported";
          return { status: "UNSUPPORTED", reason: "no AudioContext implementation is available" };
        }
        master = context.createGain();
        master.gain.value = 0.25;
        master.connect(context.destination);
      }
      if (context.state === "suspended") await context.resume();
      if (context.state !== "running") {
        state = "blocked";
        return { status: "BLOCKED", reason: `AudioContext state is ${context.state}` };
      }
      state = "running";
      return { status: "OK" };
    } catch (error) {
      lastError = String(error?.message ?? error);
      state = "blocked";
      return { status: "BLOCKED", reason: lastError };
    }
  }

  /**
   * Play a short synthetic cue. Refuses with NOT RUN until unlock() succeeded.
   */
  function playCue({ frequency = 440, durationMs = 80, gain = 0.05, type = "sine" } = {}) {
    if (state !== "running" || !context) {
      return { status: "NOT RUN", reason: AUDIO_GESTURE_REASON };
    }
    const now = context.currentTime;
    const oscillator = context.createOscillator();
    const envelope = context.createGain();
    oscillator.type = type;
    oscillator.frequency.value = frequency;
    envelope.gain.setValueAtTime(0.0001, now);
    envelope.gain.linearRampToValueAtTime(gain, now + 0.005);
    envelope.gain.exponentialRampToValueAtTime(0.0001, now + Math.max(0.01, durationMs / 1000));
    oscillator.connect(envelope);
    envelope.connect(master);
    oscillator.start(now);
    oscillator.stop(now + Math.max(0.01, durationMs / 1000));
    cues += 1;
    return { status: "OK", frequency, durationMs, cues };
  }

  async function suspend() {
    if (context && context.state === "running") await context.suspend();
    state = "suspended";
    return { status: "OK" };
  }

  return {
    unlock,
    playCue,
    suspend,
    getState: () => ({
      state,
      contextState: context?.state ?? "none",
      cues,
      lastError,
    }),
    getContext: () => context,
  };
}
