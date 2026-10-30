// Q7b browser virtual filesystem abstraction.
//
// The engine's synchronous fopen/unzOpen path cannot be used in a browser. This
// module is the async host-shaped seam: mounts are explicit, paths are resolved
// against a mount prefix, and every unavailable capability returns a typed
// NOT RUN or UNSUPPORTED result instead of synthetic bytes.
//
// It deliberately does not parse retail pak files. Retail data is EULA-bound and
// absent from this repository; a future worker-side reader must be supplied with
// legal user data and its own evidence.

export const NO_GAME_DATA_REASON =
  "no legal DOOM 3 game data is present in this repository; retail pk4/pk3 assets are EULA-bound and were not supplied for this slice";

export const PAK_UNSUPPORTED_REASON =
  "pak/zip directory parsing is not implemented in the Q7b host slice; legal user data must be mounted by a future worker-side reader";

export class VfsError extends Error {
  constructor(code, path, message) {
    super(message || `${code}: ${path}`);
    this.name = "VfsError";
    this.code = code;
    this.path = path;
  }
}

/**
 * Normalize a virtual path to a leading-slash POSIX form. Rejects traversal.
 */
export function normalizeVirtualPath(input) {
  if (typeof input !== "string" || input.length === 0) {
    throw new VfsError("ERR_INVALID_PATH", String(input), "path must be a non-empty string");
  }
  const raw = input.replace(/\\/g, "/");
  const parts = [];
  for (const part of raw.split("/")) {
    if (part === "" || part === ".") continue;
    if (part === "..") {
      throw new VfsError("ERR_PATH_TRAVERSAL", input, `path traversal is not allowed: ${input}`);
    }
    parts.push(part);
  }
  return `/${parts.join("/")}`;
}

function toBytes(value) {
  if (value instanceof Uint8Array) return value;
  if (typeof value === "string") return new TextEncoder().encode(value);
  if (value instanceof ArrayBuffer) return new Uint8Array(value);
  throw new VfsError("ERR_INVALID_CONTENT", "", "mount content must be bytes or a string");
}

/**
 * In-memory mount used by the procedural/test scene and unit tests.
 * Files are keyed by absolute virtual path.
 */
export function createMemoryMount(name, files = {}, { prefix = "/" } = {}) {
  if (!name) throw new VfsError("ERR_INVALID_MOUNT", "", "mount name is required");
  const prefixPath = normalizeVirtualPath(prefix);
  const contents = new Map();
  for (const [path, value] of Object.entries(files)) {
    let key = normalizeVirtualPath(path);
    // Keys may be written with the mount prefix or relative to it.
    if (prefixPath !== "/" && (key === prefixPath || key.startsWith(prefixPath + "/"))) {
      key = key.slice(prefixPath.length) || "/";
    }
    contents.set(key, toBytes(value));
  }
  return {
    name,
    kind: "memory",
    prefix: prefixPath,
    has(path) {
      return contents.has(normalizeVirtualPath(path));
    },
    read(path) {
      const key = normalizeVirtualPath(path);
      const bytes = contents.get(key);
      if (!bytes) throw new VfsError("ERR_NOT_FOUND", key);
      return bytes.slice();
    },
    size(path) {
      const key = normalizeVirtualPath(path);
      const bytes = contents.get(key);
      if (!bytes) return null;
      return bytes.byteLength;
    },
    list() {
      return [...contents.keys()].sort();
    },
  };
}

/**
 * HTTP mount for a legal, user-supplied data root. The fetcher is injected so
 * unit tests stay offline and the browser can pass its own fetch.
 */
export function createHttpMount(name, baseUrl, fetcher = globalThis.fetch, { prefix = "/" } = {}) {
  if (!name) throw new VfsError("ERR_INVALID_MOUNT", "", "mount name is required");
  if (typeof fetcher !== "function") throw new VfsError("ERR_INVALID_MOUNT", "", "fetcher is required");
  const base = String(baseUrl || "").replace(/\/+$/, "");
  return {
    name,
    kind: "http",
    prefix: normalizeVirtualPath(prefix),
    async read(path) {
      const key = normalizeVirtualPath(path);
      const response = await fetcher(base + key);
      if (!response) throw new VfsError("ERR_FETCH", key, "fetcher returned no response");
      if (response.status === 404) throw new VfsError("ERR_NOT_FOUND", key);
      if (!response.ok) throw new VfsError("ERR_FETCH", key, `HTTP ${response.status}`);
      return new Uint8Array(await response.arrayBuffer());
    },
    size() {
      return null;
    },
    list() {
      return [];
    },
  };
}

/**
 * Create the host VFS. Mounts are searched by longest matching prefix.
 */
export function createVfs({ mounts = [], fetcher = globalThis.fetch } = {}) {
  const active = [];
  let userData = null;

  function mount(entry) {
    if (!entry || typeof entry.read !== "function" || !entry.name) {
      throw new VfsError("ERR_INVALID_MOUNT", "", "a mount needs a name and a read() function");
    }
    active.push(entry);
    return entry.name;
  }

  for (const entry of mounts) mount(entry);

  function resolve(path) {
    const key = normalizeVirtualPath(path);
    let best = null;
    for (const entry of active) {
      const prefix = entry.prefix ? normalizeVirtualPath(entry.prefix) : "/";
      if (prefix === "/" || key === prefix || key.startsWith(prefix + "/")) {
        if (!best || prefix.length > best.prefix.length) best = { entry, prefix };
      }
    }
    if (!best) throw new VfsError("ERR_NO_MOUNT", key, `no mount owns ${key}`);
    const relative = key.slice(best.prefix.length) || "/";
    return { entry: best.entry, key, relative };
  }

  async function readFile(path) {
    const { entry, relative } = resolve(path);
    return entry.read(relative);
  }

  async function readText(path) {
    return new TextDecoder().decode(await readFile(path));
  }

  async function readRange(path, offset, length) {
    if (!Number.isInteger(offset) || offset < 0 || !Number.isInteger(length) || length < 0) {
      throw new VfsError("ERR_INVALID_RANGE", String(path), "offset and length must be non-negative integers");
    }
    const bytes = await readFile(path);
    if (offset + length > bytes.byteLength) {
      throw new VfsError("ERR_RANGE", String(path), `range ${offset}+${length} exceeds ${bytes.byteLength} bytes`);
    }
    return bytes.slice(offset, offset + length);
  }

  function openGameData() {
    if (!userData) {
      return { status: "NOT RUN", reason: NO_GAME_DATA_REASON };
    }
    return { status: "OK", mount: userData.name, note: "user-supplied legal data root; not exercised by this slice" };
  }

  function mountUserData(entry) {
    if (!entry || !entry.name || typeof entry.read !== "function") {
      throw new VfsError("ERR_INVALID_MOUNT", "", "user data mount needs a name and a read() function");
    }
    mount(entry);
    userData = entry;
    return openGameData();
  }

  function readPakDirectory() {
    return { status: "UNSUPPORTED", reason: PAK_UNSUPPORTED_REASON };
  }

  return {
    mount,
    resolve,
    readFile,
    readText,
    readRange,
    openGameData,
    mountUserData,
    readPakDirectory,
    listMounts: () => active.map((entry) => ({ name: entry.name, kind: entry.kind || "custom", prefix: entry.prefix || "/" })),
    fetcher,
  };
}
