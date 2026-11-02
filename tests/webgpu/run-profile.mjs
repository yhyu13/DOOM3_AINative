// Q7b pinned-browser harness for the WebGPU profile host slice.
//
// Dependency-free: Node's built-in fetch and WebSocket drive the DevTools
// protocol of an explicitly named Chrome binary. No npm install, no downloaded
// browser, no game data.
//
// Usage:
//   node tests/webgpu/run-profile.mjs --browser <chrome.exe> [--repeat 2]
//        [--out <dir>] [--evidence <dir>] [--headed] [--timeout <ms>]
//        [--allow-version-drift] [--evidence-png]
//
// Exit codes: 0 every required check passed twice on a pinned-major browser;
// 1 a check failed, was not run, repetitions disagreed, or version drift was
// explicitly allowed; 2 setup failed (no browser, page crash, nonce missing,
// pinned major mismatch).

import { spawn } from "node:child_process";
import fs from "node:fs";
import http from "node:http";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

import {
  OPTIONAL_FEATURES,
  REQUIRED_LIMITS,
  WEB_PROFILE_ID,
} from "../../web/profile/capability.mjs";
import {
  PROFILE_EXPECTED_NOT_RUN,
  PROFILE_REQUIRED_CHECKS,
} from "../../web/profile/profile.mjs";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, "..", "..");
const PIN_PATH = path.join(HERE, "browser-pin.json");
const PAGE = "web/profile/index.html";
const SCHEMA = "q7-webgpu-profile-run/1";

function parseArgs(argv) {
  const args = {
    repeat: 2,
    out: path.join(ROOT, "build", "webgpu-profile"),
    evidence: null,
    evidencePng: false,
    headed: false,
    timeout: 90000,
    browser: null,
    allowVersionDrift: false,
  };
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    const next = () => {
      const value = argv[++i];
      if (value === undefined) throw new Error(`missing value for ${arg}`);
      return value;
    };
    if (arg === "--browser") args.browser = next();
    else if (arg === "--repeat") args.repeat = Math.max(1, parseInt(next(), 10));
    else if (arg === "--out") args.out = path.resolve(next());
    else if (arg === "--evidence") args.evidence = path.resolve(next());
    else if (arg === "--evidence-png") args.evidencePng = true;
    else if (arg === "--headed") args.headed = true;
    else if (arg === "--timeout") args.timeout = parseInt(next(), 10);
    else if (arg === "--allow-version-drift") args.allowVersionDrift = true;
    else if (arg === "--help" || arg === "-h") {
      console.log("node tests/webgpu/run-profile.mjs --browser <chrome.exe> [--repeat 2] [--out <dir>] [--evidence <dir>] [--headed] [--timeout <ms>] [--allow-version-drift] [--evidence-png]");
      process.exit(0);
    } else throw new Error(`unknown argument: ${arg}`);
  }
  return args;
}

function loadPin() {
  const pin = JSON.parse(fs.readFileSync(PIN_PATH, "utf8"));
  if (pin.schema !== "q7-pinned-browser/1" || !pin.name || !Number.isInteger(pin.major)) {
    throw new Error("tests/webgpu/browser-pin.json is not a q7-pinned-browser/1 record");
  }
  return pin;
}

function requireBrowser(explicit) {
  const candidate = explicit || process.env.WGPU_BROWSER || null;
  if (!candidate) {
    throw new Error("no browser selected. Pass --browser <path to chrome.exe> or set WGPU_BROWSER; the harness does not auto-select a different browser");
  }
  if (!fs.existsSync(candidate) || !fs.statSync(candidate).isFile()) {
    throw new Error(`browser executable does not exist: ${candidate}`);
  }
  return candidate;
}

function serveStatic(root) {
  const types = {
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".mjs": "text/javascript; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".json": "application/json",
    ".wasm": "application/wasm",
    ".data": "application/octet-stream",
    ".png": "image/png",
  };
  const server = http.createServer((request, response) => {
    const url = new URL(request.url, "http://127.0.0.1");
    const rel = decodeURIComponent(url.pathname === "/" ? `/${PAGE}` : url.pathname);
    const file = path.resolve(root, "." + rel);
    const relative = path.relative(root, file);
    if (relative.startsWith("..") || path.isAbsolute(relative) || !fs.existsSync(file) || !fs.statSync(file).isFile()) {
      response.writeHead(404, { "content-type": "text/plain" });
      response.end("not found");
      return;
    }
    response.writeHead(200, {
      "content-type": types[path.extname(file)] || "application/octet-stream",
      "cache-control": "no-store",
    });
    response.end(fs.readFileSync(file));
  });
  return new Promise((resolve) => server.listen(0, "127.0.0.1", () => resolve({ server, port: server.address().port })));
}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function waitFor(fn, { timeout, interval = 200, label }) {
  const deadline = Date.now() + timeout;
  let lastError = null;
  while (Date.now() < deadline) {
    try {
      const value = await fn();
      if (value) return value;
    } catch (error) {
      lastError = error;
    }
    await sleep(interval);
  }
  throw new Error(`timed out waiting for ${label}${lastError ? `: ${lastError.message}` : ""}`);
}

function connectCdp(url) {
  const socket = new WebSocket(url);
  const listeners = new Set();
  let nextId = 0;
  const pending = new Map();
  socket.addEventListener("message", (event) => {
    const message = JSON.parse(event.data);
    if (message.id && pending.has(message.id)) {
      const { resolve, reject } = pending.get(message.id);
      pending.delete(message.id);
      if (message.error) reject(new Error(`${message.error.message} (${message.error.code})`));
      else resolve(message.result);
      return;
    }
    for (const fn of listeners) fn(message);
  });
  const open = new Promise((resolve, reject) => {
    socket.addEventListener("open", resolve, { once: true });
    socket.addEventListener("error", () => reject(new Error(`websocket error connecting to ${url}`)), { once: true });
  });
  return {
    open,
    send(method, params = {}) {
      const id = ++nextId;
      return new Promise((resolve, reject) => {
        pending.set(id, { resolve, reject });
        socket.send(JSON.stringify({ id, method, params }));
      });
    },
    on(fn) {
      listeners.add(fn);
    },
    close() {
      try {
        socket.close();
      } catch {}
    },
  };
}

function browserMajor(browserName) {
  const match = /Chrome\/(\d+)\./.exec(String(browserName || ""));
  return match ? Number(match[1]) : null;
}

async function runOnce({ browserPath, args, pin, httpPort, logs, attempt, nonce }) {
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), "q7-profile-"));
  let launchFlags = [...pin.flags];
  if (args.headed) launchFlags = launchFlags.filter((flag) => !flag.startsWith("--headless"));
  const chromeArgs = [
    ...launchFlags,
    "--remote-debugging-port=0",
    `--user-data-dir=${profile}`,
    `http://127.0.0.1:${httpPort}/${PAGE}?run=${nonce}`,
  ];
  logs.push(`[attempt ${attempt}] launch: "${browserPath}" ${chromeArgs.filter((a) => !a.startsWith("--user-data-dir")).join(" ")}`);
  const child = spawn(browserPath, chromeArgs, { stdio: "ignore" });
  const cleanup = () => {
    try {
      child.kill();
    } catch {}
  };
  try {
    const activePort = path.join(profile, "DevToolsActivePort");
    const portText = await waitFor(() => (fs.existsSync(activePort) ? fs.readFileSync(activePort, "utf8") : null), {
      timeout: 30000,
      label: "DevToolsActivePort",
    });
    const cdpPort = parseInt(portText.split(/\r?\n/)[0], 10);
    const version = await waitFor(async () => {
      const response = await fetch(`http://127.0.0.1:${cdpPort}/json/version`);
      return response.ok ? response.json() : null;
    }, { timeout: 20000, label: "browser version" });
    logs.push(`[attempt ${attempt}] ${version.Browser} (protocol ${version["Protocol-Version"]})`);

    const target = await waitFor(async () => {
      const response = await fetch(`http://127.0.0.1:${cdpPort}/json/list`);
      const list = await response.json();
      return list.find((entry) => entry.type === "page" && entry.url.includes("/" + PAGE)) || null;
    }, { timeout: 20000, label: "profile page target" });

    const cdp = connectCdp(target.webSocketDebuggerUrl);
    await cdp.open;
    cdp.on((message) => {
      if (message.method === "Runtime.consoleAPICalled") {
        logs.push(`[page] ${message.params.type}: ${message.params.args.map((arg) => arg.value ?? arg.description ?? arg.type).join(" ")}`);
      } else if (message.method === "Runtime.exceptionThrown") {
        logs.push(`[page] exception: ${message.params.exceptionDetails.text} ${message.params.exceptionDetails.exception?.description || ""}`);
      } else if (message.method === "Log.entryAdded") {
        logs.push(`[page] log ${message.params.entry.level}: ${message.params.entry.text}`);
      }
    });
    await cdp.send("Runtime.enable");
    await cdp.send("Log.enable");
    await cdp.send("Page.enable");
    await cdp.send("Emulation.setDeviceMetricsOverride", { width: 1180, height: 980, deviceScaleFactor: 1, mobile: false });

    const result = await waitFor(async () => {
      const response = await cdp.send("Runtime.evaluate", { expression: "window.__profileResult || null", returnByValue: true });
      return response.result && response.result.value ? response.result.value : null;
    }, { timeout: args.timeout, label: "profile result" });

    if (!result.env || result.env.runNonce !== nonce) {
      cdp.close();
      throw new Error(`the page did not echo the run nonce ${nonce}; a static page cannot stand in for an executed run`);
    }
    const shot = await cdp.send("Page.captureScreenshot", { format: "png", captureBeyondViewport: true });
    cdp.close();
    return {
      result,
      version: version.Browser,
      protocol: version["Protocol-Version"],
      major: browserMajor(version.Browser),
      png: Buffer.from(shot.data, "base64"),
    };
  } finally {
    cleanup();
    await sleep(300);
    try {
      fs.rmSync(profile, { recursive: true, force: true });
    } catch {}
  }
}

// T073 target-adapter evidence: the run must carry the live capability table
// for the explicitly named target, not just a status string. A report that
// drops the adapter identity, limits or features is incomplete evidence.
export function capabilityProblems(result) {
  const problems = [];
  const capability = result?.capability;
  if (!capability || typeof capability !== "object") {
    problems.push("capability:missing");
    return problems;
  }
  if (capability.targetId !== WEB_PROFILE_ID) {
    problems.push(`capability:target-mismatch(${capability.targetId})`);
  }
  if (!capability.adapter || typeof capability.adapter !== "object") {
    problems.push("capability:adapter-missing");
  } else if (!String(capability.adapter.description || capability.adapter.vendor || "").length) {
    problems.push("capability:adapter-identity-blank");
  }
  if (!Number.isInteger(capability.contractVersion) || capability.contractVersion < 1) {
    problems.push("capability:contract-version-missing");
  }
  const rows = Array.isArray(capability.rows) ? capability.rows : [];
  for (const rule of REQUIRED_LIMITS) {
    if (!rows.some((row) => row.kind === "limit" && row.name === rule.name)) {
      problems.push(`capability:missing-limit(${rule.name})`);
    }
  }
  for (const feature of OPTIONAL_FEATURES) {
    if (!rows.some((row) => row.kind === "feature" && row.name === feature.name)) {
      problems.push(`capability:missing-feature(${feature.name})`);
    }
  }
  if (!Array.isArray(capability.fallbacks)) problems.push("capability:fallbacks-missing");
  if (!Array.isArray(capability.unsupported)) problems.push("capability:unsupported-missing");
  return problems;
}

export function summarize(result) {
  const byId = new Map((result.checks || []).map((check) => [check.id, check]));
  const missing = PROFILE_REQUIRED_CHECKS.filter((id) => !byId.has(id));
  const failed = PROFILE_REQUIRED_CHECKS.filter((id) => {
    const check = byId.get(id);
    return !check || check.status !== "PASS";
  });
  for (const [id, expectedReason] of Object.entries(PROFILE_EXPECTED_NOT_RUN)) {
    const check = byId.get(id);
    if (!check) {
      failed.push(`${id}:missing`);
      continue;
    }
    if (check.status !== "NOT RUN") {
      failed.push(`${id}:expected-not-run`);
      continue;
    }
    const detail = String(check.detail || "");
    if (detail.length < 10 || !detail.includes(expectedReason.split(";")[0].slice(0, 24))) {
      failed.push(`${id}:missing-or-mismatched-reason`);
    }
  }
  if (result.scene?.realScene !== false) failed.push("scene:realScene-must-be-false");
  if (result.realScene?.status !== "NOT RUN") failed.push("realScene:must-be-NOT-RUN");
  if (result.targetId !== WEB_PROFILE_ID) failed.push("target:not-webgpu-browser");
  failed.push(...capabilityProblems(result));
  return { byId, missing, failed };
}

function statusSignature(result) {
  return PROFILE_REQUIRED_CHECKS.map((id) => {
    const check = (result.checks || []).find((entry) => entry.id === id);
    return `${id}=${check ? check.status : "MISSING"}`;
  }).join("|");
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const pin = loadPin();
  const browserPath = requireBrowser(args.browser);
  fs.mkdirSync(args.out, { recursive: true });
  const logs = [];
  const { server, port } = await serveStatic(ROOT);
  const runs = [];
  let exitCode = 0;

  try {
    for (let attempt = 1; attempt <= args.repeat; attempt++) {
      let run;
      const nonce = `q7b-${Date.now()}-${attempt}`;
      try {
        run = await runOnce({ browserPath, args, pin, httpPort: port, logs, attempt, nonce });
      } catch (error) {
        logs.push(`[attempt ${attempt}] FAILED: ${error.stack || error.message}`);
        console.error(`attempt ${attempt} failed: ${error.message}`);
        exitCode = 2;
        break;
      }
      runs.push(run);
      const { failed, missing } = summarize(run.result);
      console.log(
        `attempt ${attempt}: ${run.version}  ${run.result.summary.pass} pass / ${run.result.summary.fail} fail / ${run.result.summary.notRun} not run  (${run.result.summary.total} checks)`,
      );
      for (const check of run.result.checks) {
        console.log(`  ${check.status.padEnd(7)} ${check.id}  ${String(check.detail).slice(0, 150)}`);
      }
      if (missing.length) console.log(`  missing required checks: ${missing.join(", ")}`);
      if (failed.length) console.log(`  required checks not PASS: ${failed.join(", ")}`);
      if (missing.length || failed.length) exitCode = 1;
    }
  } finally {
    server.close();
  }

  if (runs.length === 0) {
    fs.writeFileSync(path.join(args.out, "run.log"), logs.join("\n") + "\n", "utf8");
    process.exit(exitCode || 2);
  }

  const first = runs[0];
  const repeatable = runs.every((run) => statusSignature(run.result) === statusSignature(first.result));
  if (!repeatable) {
    console.log("repetitions disagree on required-check status, so the run is not repeatable");
    exitCode = 1;
  }

  const pinSatisfied = first.major === pin.major;
  if (!pinSatisfied) {
    const message = `pinned browser major ${pin.major}, got ${first.major ?? "unknown"} (${first.version})`;
    if (!args.allowVersionDrift) {
      logs.push(`[pin] FAILED: ${message}`);
      console.error(`SETUP: ${message}. Install the pinned major or pass --allow-version-drift to record a non-pinned run as a failure.`);
      fs.writeFileSync(path.join(args.out, "run.log"), logs.join("\n") + "\n", "utf8");
      process.exit(2);
    }
    logs.push(`[pin] drift allowed: ${message}`);
    console.log(`WARNING: ${message}; this run is not a pinned-browser pass`);
    exitCode = 1;
  }

  const report = {
    schema: SCHEMA,
    targetId: first.result.targetId,
    contract: first.result.contract ?? null,
    browser: {
      name: first.version,
      protocol: first.protocol,
      major: first.major,
      pinnedMajor: pin.major,
      evidenceVersion: pin.evidenceVersion,
      pinSatisfied,
      executableName: path.basename(browserPath),
      headless: !args.headed,
    },
    page: first.result.env,
    scene: first.result.scene,
    realScene: first.result.realScene,
    capability: first.result.capability ?? null,
    repetitions: runs.length,
    repeatable,
    requiredChecks: PROFILE_REQUIRED_CHECKS,
    expectedNotRun: PROFILE_EXPECTED_NOT_RUN,
    attempts: runs.map((run) => ({
      summary: run.result.summary,
      capabilityStatus: run.result.capability?.status ?? null,
      capability: run.result.capability ?? null,
      checks: run.result.checks,
    })),
    summary: first.result.summary,
  };

  fs.writeFileSync(path.join(args.out, "results.json"), JSON.stringify(report, null, 2) + "\n", "utf8");
  fs.writeFileSync(path.join(args.out, "profile.png"), first.png);
  fs.writeFileSync(path.join(args.out, "run.log"), logs.join("\n") + "\n", "utf8");

  if (args.evidence) {
    fs.mkdirSync(args.evidence, { recursive: true });
    fs.writeFileSync(path.join(args.evidence, "q7-profile.json"), JSON.stringify(report, null, 2) + "\n", "utf8");
    if (args.evidencePng) {
      fs.writeFileSync(path.join(args.evidence, "q7-profile-procedural.png"), first.png);
    }
  }

  console.log(`\nartifacts: ${path.join(args.out, "results.json")}, ${path.join(args.out, "profile.png")}, ${path.join(args.out, "run.log")}`);
  console.log(`repeatable across ${runs.length} attempt(s): ${repeatable}`);
  console.log(`pinned browser major ${pin.major}: ${pinSatisfied}`);
  process.exit(exitCode);
}

const isEntryPoint = process.argv[1]
  ? path.resolve(process.argv[1]).toLowerCase() === fileURLToPath(import.meta.url).toLowerCase()
  : false;

if (isEntryPoint) {
  main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(2);
  });
}
