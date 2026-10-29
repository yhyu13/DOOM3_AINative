// Q7a WebGPU conformance runner.
//
// Dependency-free by design: Node's built-in fetch and WebSocket drive the
// DevTools protocol of an installed, named browser. No npm install, no
// downloaded SDK, no game data.
//
// Usage:
//   node tests/webgpu/run-conformance.mjs [--browser <path>] [--repeat <n>]
//        [--out <dir>] [--evidence <dir>] [--headed] [--timeout <ms>]
//
// Exit codes: 0 all required checks passed on every repetition; 1 a check
// failed, was not run, or repetitions disagreed; 2 setup failed (no browser,
// page never reported).

import { spawn } from "node:child_process";
import http from "node:http";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, "..", "..");

const REQUIRED = [
  "C1-device",
  "C2-buffer",
  "C3-bind-group",
  "C4-mrt",
  "C5-compute",
  "C6-pitch256",
  "C7-surface",
  "C8-device-loss",
  "N1-negative-comparator",
  "N2-negative-pitch",
  "N3-negative-liveness",
  "E-uncaptured",
  "A1-software-adapter",
];
const OPTIONAL_NOT_RUN = new Set(["A1-software-adapter"]);

function parseArgs(argv) {
  const args = { repeat: 1, out: path.join(ROOT, "build", "webgpu-q7a"), evidence: null, headed: false, timeout: 90000, browser: null, page: "conformance.html", root: HERE };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    const next = () => {
      const v = argv[++i];
      if (v === undefined) throw new Error(`missing value for ${a}`);
      return v;
    };
    if (a === "--browser") args.browser = next();
    else if (a === "--repeat") args.repeat = Math.max(1, parseInt(next(), 10));
    else if (a === "--out") args.out = path.resolve(next());
    else if (a === "--evidence") args.evidence = path.resolve(next());
    else if (a === "--headed") args.headed = true;
    else if (a === "--timeout") args.timeout = parseInt(next(), 10);
    else if (a === "--page") args.page = next();
    else if (a === "--root") args.root = path.resolve(next());
    else if (a === "--help" || a === "-h") {
      console.log("node tests/webgpu/run-conformance.mjs [--browser <path>] [--repeat <n>] [--out <dir>] [--evidence <dir>] [--headed] [--timeout <ms>]");
      process.exit(0);
    } else throw new Error(`unknown argument: ${a}`);
  }
  return args;
}

function findBrowser(explicit) {
  const candidates = [
    explicit,
    process.env.WGPU_BROWSER,
    path.join(process.env.ProgramFiles || "", "Google/Chrome/Application/chrome.exe"),
    path.join(process.env["ProgramFiles(x86)"] || "", "Google/Chrome/Application/chrome.exe"),
    path.join(process.env.LOCALAPPDATA || "", "Google/Chrome/Application/chrome.exe"),
    path.join(process.env.ProgramFiles || "", "Microsoft/Edge/Application/msedge.exe"),
    path.join(process.env["ProgramFiles(x86)"] || "", "Microsoft/Edge/Application/msedge.exe"),
  ].filter(Boolean);
  for (const c of candidates) if (fs.existsSync(c)) return c;
  return null;
}

function serveStatic(root) {
  const types = { ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".mjs": "text/javascript; charset=utf-8", ".css": "text/css; charset=utf-8", ".json": "application/json", ".wasm": "application/wasm", ".data": "application/octet-stream", ".png": "image/png" };
  const server = http.createServer((req, res) => {
    const url = new URL(req.url, "http://127.0.0.1");
    const rel = decodeURIComponent(url.pathname === "/" ? "/conformance.html" : url.pathname);
    const file = path.resolve(root, "." + rel);
    if (!file.startsWith(root) || !fs.existsSync(file) || !fs.statSync(file).isFile()) {
      res.writeHead(404, { "content-type": "text/plain" });
      res.end("not found");
      return;
    }
    res.writeHead(200, { "content-type": types[path.extname(file)] || "application/octet-stream", "cache-control": "no-store" });
    res.end(fs.readFileSync(file));
  });
  return new Promise((resolve) => server.listen(0, "127.0.0.1", () => resolve({ server, port: server.address().port })));
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function waitFor(fn, { timeout, interval = 200, label }) {
  const deadline = Date.now() + timeout;
  let lastError = null;
  while (Date.now() < deadline) {
    try {
      const v = await fn();
      if (v) return v;
    } catch (e) {
      lastError = e;
    }
    await sleep(interval);
  }
  throw new Error(`timed out waiting for ${label}${lastError ? `: ${lastError.message}` : ""}`);
}

function connectCdp(url) {
  const ws = new WebSocket(url);
  const listeners = new Set();
  let nextId = 0;
  const pending = new Map();
  ws.addEventListener("message", (event) => {
    const msg = JSON.parse(event.data);
    if (msg.id && pending.has(msg.id)) {
      const { resolve, reject } = pending.get(msg.id);
      pending.delete(msg.id);
      if (msg.error) reject(new Error(`${msg.error.message} (${msg.error.code})`));
      else resolve(msg.result);
      return;
    }
    for (const fn of listeners) fn(msg);
  });
  const open = new Promise((resolve, reject) => {
    ws.addEventListener("open", resolve, { once: true });
    ws.addEventListener("error", () => reject(new Error(`websocket error connecting to ${url}`)), { once: true });
  });
  return {
    open,
    send(method, params = {}) {
      const id = ++nextId;
      return new Promise((resolve, reject) => {
        pending.set(id, { resolve, reject });
        ws.send(JSON.stringify({ id, method, params }));
      });
    },
    on(fn) { listeners.add(fn); },
    close() { try { ws.close(); } catch {} },
  };
}

async function runOnce({ browserPath, args, httpPort, logs, attempt, nonce }) {
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), "q7a-webgpu-"));
  const chromeArgs = [
    args.headed ? "--window-size=1180,980" : "--headless=new",
    "--remote-debugging-port=0",
    `--user-data-dir=${profile}`,
    "--no-first-run",
    "--no-default-browser-check",
    "--disable-extensions",
    "--enable-unsafe-webgpu",
    "--hide-scrollbars",
    `http://127.0.0.1:${httpPort}/${args.page}?run=${nonce}`,
  ];
  logs.push(`[attempt ${attempt}] launch: "${browserPath}" ${chromeArgs.filter((a) => !a.startsWith("--user-data-dir")).join(" ")}`);
  const child = spawn(browserPath, chromeArgs, { stdio: "ignore" });
  const cleanup = () => { try { child.kill(); } catch {} };
  try {
    const activePort = path.join(profile, "DevToolsActivePort");
    const portText = await waitFor(() => (fs.existsSync(activePort) ? fs.readFileSync(activePort, "utf8") : null), { timeout: 30000, label: "DevToolsActivePort" });
    const cdpPort = parseInt(portText.split(/\r?\n/)[0], 10);
    const version = await waitFor(async () => {
      const r = await fetch(`http://127.0.0.1:${cdpPort}/json/version`);
      return r.ok ? r.json() : null;
    }, { timeout: 20000, label: "browser version" });
    logs.push(`[attempt ${attempt}] ${version.Browser} (protocol ${version["Protocol-Version"]})`);

    const target = await waitFor(async () => {
      const r = await fetch(`http://127.0.0.1:${cdpPort}/json/list`);
      const list = await r.json();
      return list.find((t) => t.type === "page" && t.url.includes("/" + args.page)) || null;
    }, { timeout: 20000, label: "conformance page target" });

    const cdp = connectCdp(target.webSocketDebuggerUrl);
    await cdp.open;
    cdp.on((msg) => {
      if (msg.method === "Runtime.consoleAPICalled") {
        logs.push(`[page] ${msg.params.type}: ${msg.params.args.map((a) => a.value ?? a.description ?? a.type).join(" ")}`);
      } else if (msg.method === "Runtime.exceptionThrown") {
        logs.push(`[page] exception: ${msg.params.exceptionDetails.text} ${msg.params.exceptionDetails.exception?.description || ""}`);
      } else if (msg.method === "Log.entryAdded") {
        logs.push(`[page] log ${msg.params.entry.level}: ${msg.params.entry.text}`);
      }
    });
    await cdp.send("Runtime.enable");
    await cdp.send("Log.enable");
    await cdp.send("Page.enable");
    await cdp.send("Emulation.setDeviceMetricsOverride", { width: 1180, height: 980, deviceScaleFactor: 1, mobile: false });

    const result = await waitFor(async () => {
      const r = await cdp.send("Runtime.evaluate", { expression: "window.__conformanceResult || null", returnByValue: true });
      return r.result && r.result.value ? r.result.value : null;
    }, { timeout: args.timeout, label: "conformance result" });

    if (!result.env || result.env.runNonce !== nonce) {
      cdp.close();
      throw new Error(`the page did not echo the run nonce ${nonce}; a static page cannot stand in for an executed run`);
    }
    const shot = await cdp.send("Page.captureScreenshot", { format: "png", captureBeyondViewport: true });
    cdp.close();
    return { result, version: version.Browser, protocol: version["Protocol-Version"], png: Buffer.from(shot.data, "base64") };
  } finally {
    cleanup();
    await sleep(300);
    try { fs.rmSync(profile, { recursive: true, force: true }); } catch {}
  }
}

function summarize(result) {
  const byId = new Map((result.checks || []).map((c) => [c.id, c]));
  const missing = REQUIRED.filter((id) => !byId.has(id));
  const failed = REQUIRED.filter((id) => { const c = byId.get(id); if (!c) return true; if (c.status === "PASS") return false; return !(c.status === "NOT RUN" && OPTIONAL_NOT_RUN.has(id)); });
  return { byId, missing, failed };
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const browserPath = findBrowser(args.browser);
  if (!browserPath) {
    console.error("SETUP: no browser found. Pass --browser <path to chrome.exe|msedge.exe> or set WGPU_BROWSER.");
    process.exit(2);
  }
  fs.mkdirSync(args.out, { recursive: true });
  const logs = [];
  const { server, port } = await serveStatic(args.root);
  const runs = [];
  let exitCode = 0;
  try {
    for (let attempt = 1; attempt <= args.repeat; attempt++) {
      let run;
      try {
        run = await runOnce({ browserPath, args, httpPort: port, logs, attempt, nonce: `q7a-${Date.now()}-${attempt}` });
      } catch (e) {
        logs.push(`[attempt ${attempt}] FAILED: ${e.stack || e.message}`);
        console.error(`attempt ${attempt} failed: ${e.message}`);
        exitCode = 2;
        break;
      }
      runs.push(run);
      const { failed, missing } = summarize(run.result);
      console.log(`attempt ${attempt}: ${run.version}  ${run.result.summary.pass} pass / ${run.result.summary.fail} fail / ${run.result.summary.notRun} not run  (${run.result.elapsedMs ?? "n/a"} ms)`);
      for (const c of run.result.checks) console.log(`  ${c.status === "PASS" ? "PASS" : c.status === "FAIL" ? "FAIL" : "NOTRUN"}  ${c.id}  ${c.observed ? JSON.stringify(c.observed).slice(0, 160) : c.detail.slice(0, 160)}`);
      if (missing.length) console.log(`  missing required checks: ${missing.join(", ")}`);
      if (failed.length) console.log(`  required checks not PASS: ${failed.join(", ")}`);
      if (missing.length || failed.length || run.result.summary.fail > 0) exitCode = 1;
    }
  } finally {
    server.close();
  }

  if (runs.length === 0) {
    fs.writeFileSync(path.join(args.out, "run.log"), logs.join("\n") + "\n", "utf8");
    process.exit(exitCode || 2);
  }

  const first = runs[0];
  const statusSignature = (r) => REQUIRED.map((id) => `${id}=${(r.result.checks.find((c) => c.id === id) || {}).status}`).join("|");
  const repeatable = runs.every((r) => statusSignature(r) === statusSignature(first));
  if (!repeatable) {
    console.log("repetitions disagree on per-check status, so the run is not repeatable");
    exitCode = 1;
  }

  const report = {
    schema: "q7a-webgpu-conformance/1",
    browser: { name: first.version, protocol: first.protocol, executableName: path.basename(browserPath), headless: !args.headed },
    page: first.result.env,
    repetitions: runs.length,
    repeatable,
    requiredChecks: REQUIRED,
    attempts: runs.map((r) => ({ summary: r.result.summary, checks: r.result.checks, elapsedMs: r.result.elapsedMs })),
    summary: first.result.summary,
  };
  fs.writeFileSync(path.join(args.out, "results.json"), JSON.stringify(report, null, 2) + "\n", "utf8");
  fs.writeFileSync(path.join(args.out, "conformance.png"), first.png);
  fs.writeFileSync(path.join(args.out, "run.log"), logs.join("\n") + "\n", "utf8");

  if (args.evidence) {
    fs.mkdirSync(args.evidence, { recursive: true });
    fs.writeFileSync(path.join(args.evidence, "q7a-conformance.json"), JSON.stringify(report, null, 2) + "\n", "utf8");
    fs.writeFileSync(path.join(args.evidence, "q7a-conformance.png"), first.png);
  }

  console.log(`\nartifacts: ${path.join(args.out, "results.json")}, ${path.join(args.out, "conformance.png")}, ${path.join(args.out, "run.log")}`);
  console.log(`repeatable across ${runs.length} attempt(s): ${repeatable}`);
  process.exit(exitCode);
}

main().catch((e) => {
  console.error(e.stack || String(e));
  process.exit(2);
});
