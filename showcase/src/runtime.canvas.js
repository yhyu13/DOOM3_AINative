/* Procedural visual systems: ember portal, idTech4 code rain, workflow graph. */
(() => {
  'use strict';
  const W = 1920, H = 1080;
  const DPR = Math.min(2, window.devicePixelRatio || 1);

  const STAGES = [
    { n: 1, label: 'ISSUE', x: 210, y: 560, detail: 'acceptance criteria are commands, exit codes, byte counts or screenshots. If it cannot be checked mechanically, it is rewritten.' },
    { n: 2, label: 'WORKTREE', x: 480, y: 470, detail: 'one branch, one checkout, one writer. No shared build directory, no stashes, no cross-agent edits.' },
    { n: 3, label: 'SPEC', x: 745, y: 560, detail: 'spec.md, plan.md and tasks.md exist before code. The plan names the files, gates and risks.' },
    { n: 4, label: 'BUILD', x: 1010, y: 455, detail: 'work happens in the target copy. The pristine submodule stays read-only; engine changes land as a reviewed bump.' },
    { n: 5, label: 'VERIFY', x: 1275, y: 560, detail: 'run the contract command and keep the output. A green claim without a log is not evidence.' },
    { n: 6, label: 'REVIEW', x: 1535, y: 455, detail: 'a reviewer who did not write the code reruns the verification and checks the diff against the spec.' },
    { n: 7, label: 'MERGE', x: 1745, y: 560, detail: 'squash only. The PR carries Closes #N, the evidence block and the rollback path. No self-merge.' }
  ];

  const EDGES = [
    { a: 1, b: 2, kind: 'main', speed: .10, phase: .05 },
    { a: 2, b: 3, kind: 'main', speed: .12, phase: .32 },
    { a: 3, b: 4, kind: 'main', speed: .11, phase: .58 },
    { a: 4, b: 5, kind: 'main', speed: .13, phase: .18 },
    { a: 5, b: 6, kind: 'main', speed: .12, phase: .47 },
    { a: 6, b: 7, kind: 'main', speed: .10, phase: .74 },
    { a: 5, b: 4, kind: 'feedback', speed: .09, phase: .22 },
    { a: 6, b: 4, kind: 'feedback', speed: .07, phase: .66 },
    { a: 7, b: 1, kind: 'loop', speed: .05, phase: .12 }
  ];

  function mulberry32(a) {
    return function () {
      a |= 0; a = a + 0x6D2B79F5 | 0;
      let t = Math.imul(a ^ a >>> 15, 1 | a);
      t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
      return ((t ^ t >>> 14) >>> 0) / 4294967296;
    };
  }

  function setup(id) {
    const cv = document.getElementById(id);
    if (!cv) return null;
    cv.width = W * DPR; cv.height = H * DPR;
    const ctx = cv.getContext('2d');
    ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
    return ctx;
  }

  const ctx1 = setup('fx1');
  const ctx2 = setup('fx2');
  const ctx3 = setup('fx3');

  function glowSprite(r, g, b) {
    const c = document.createElement('canvas');
    c.width = c.height = 64;
    const x = c.getContext('2d');
    const gr = x.createRadialGradient(32, 32, 0, 32, 32, 32);
    gr.addColorStop(0, `rgba(${r},${g},${b},1)`);
    gr.addColorStop(.22, `rgba(${r},${g},${b},.72)`);
    gr.addColorStop(.55, `rgba(${r},${g},${b},.20)`);
    gr.addColorStop(1, `rgba(${r},${g},${b},0)`);
    x.fillStyle = gr; x.fillRect(0, 0, 64, 64);
    return c;
  }
  const SPARK = glowSprite(255, 242, 224);
  const EMBER = glowSprite(255, 92, 30);
  const DEEP = glowSprite(168, 32, 8);
  const ACID = glowSprite(184, 255, 90);

  /* ------------------------------------------------------------------ portal */
  const portal = { parts: [], embers: [], stars: [], mx: 0, my: 0 };
  function initPortal() {
    const rnd = mulberry32(1337);
    for (let i = 0; i < 620; i++) {
      const ring = i < 430;
      portal.parts.push({
        a: rnd() * Math.PI * 2,
        r: ring ? 188 + rnd() * 205 : 74 + rnd() * 430,
        sp: (ring ? .0016 + rnd() * .0022 : .0005 + rnd() * .0011) * (rnd() < .5 ? -1 : 1),
        size: ring ? .8 + rnd() * 2.2 : .5 + rnd() * 1.5,
        hot: rnd(),
        wob: rnd() * Math.PI * 2
      });
    }
    for (let i = 0; i < 78; i++) {
      portal.embers.push({ x: rnd() * W, y: rnd() * H, vy: 14 + rnd() * 54, s: .5 + rnd() * 1.7, a: rnd(), ph: rnd() * Math.PI * 2 });
    }
    for (let i = 0; i < 90; i++) {
      portal.stars.push({ x: rnd() * W, y: rnd() * H, s: .4 + rnd() * 1.4, a: .04 + rnd() * .18, ph: rnd() * Math.PI * 2 });
    }
  }

  function drawPortal(t, freeze) {
    if (!ctx1) return;
    const ctx = ctx1;
    ctx.globalCompositeOperation = 'source-over';
    ctx.globalAlpha = 1;
    if (freeze) { ctx.fillStyle = '#060607'; ctx.fillRect(0, 0, W, H); }
    else { ctx.fillStyle = 'rgba(6,6,7,.165)'; ctx.fillRect(0, 0, W, H); }

    const time = freeze ? 7.4 : t;
    const cx = 1330 + portal.mx * 24;
    const cy = 520 + portal.my * 18;

    // star field
    ctx.globalCompositeOperation = 'lighter';
    for (const s of portal.stars) {
      const tw = freeze ? 1 : (.55 + .45 * Math.sin(time * 1.1 + s.ph));
      ctx.globalAlpha = s.a * tw;
      ctx.fillStyle = '#f4efe6';
      ctx.fillRect(s.x, s.y, s.s, s.s);
    }

    // volumetric outer glow
    const g = ctx.createRadialGradient(cx, cy, 16, cx, cy, 650);
    g.addColorStop(0, 'rgba(255,196,120,.30)');
    g.addColorStop(.24, 'rgba(255,94,32,.16)');
    g.addColorStop(.58, 'rgba(255,77,23,.055)');
    g.addColorStop(1, 'rgba(255,77,23,0)');
    ctx.globalAlpha = 1; ctx.fillStyle = g;
    ctx.fillRect(cx - 680, cy - 680, 1360, 1360);

    // horizon flare
    const hg = ctx.createLinearGradient(cx - 760, 0, cx + 760, 0);
    hg.addColorStop(0, 'rgba(255,77,23,0)');
    hg.addColorStop(.5, 'rgba(255,130,60,.16)');
    hg.addColorStop(1, 'rgba(255,77,23,0)');
    ctx.fillStyle = hg;
    ctx.fillRect(cx - 760, cy - 1.2, 1520, 2.4);

    // orbital particles
    for (const p of portal.parts) {
      const a = p.a + time * p.sp;
      const wob = 1 + Math.sin(a * 5 + p.wob + time * .7) * .048;
      const rr = p.r * wob;
      const x = cx + Math.cos(a) * rr * 1.27;
      const y = cy + Math.sin(a) * rr * .73;
      const s = (p.size * 4.6) * (freeze ? 1.5 : 1);
      ctx.globalAlpha = freeze ? .30 + p.hot * .62 : .14 + p.hot * .40;
      const sprite = p.hot > .84 ? SPARK : p.hot > .52 ? EMBER : DEEP;
      ctx.drawImage(sprite, x - s, y - s, s * 2, s * 2);
    }

    // drifting embers
    for (const e of portal.embers) {
      const y = ((e.y - time * e.vy) % (H + 120) + H + 120) % (H + 120) - 60;
      const x = e.x + Math.sin(time * .34 + e.ph) * 26;
      const s = e.s * (freeze ? 5.2 : 4.0);
      ctx.globalAlpha = freeze ? .34 : .12 + (.5 + .5 * Math.sin(time * 2 + e.ph)) * .22;
      ctx.drawImage(EMBER, x - s, y - s, s * 2, s * 2);
    }

    // dark core with a hot rim
    ctx.globalCompositeOperation = 'source-over';
    ctx.globalAlpha = 1;
    ctx.save();
    ctx.translate(cx, cy);
    ctx.scale(1.34, .76);
    const core = ctx.createRadialGradient(0, 0, 18, 0, 0, 176);
    core.addColorStop(0, 'rgba(0,0,0,1)');
    core.addColorStop(.62, 'rgba(3,2,2,.98)');
    core.addColorStop(.92, 'rgba(40,8,2,.72)');
    core.addColorStop(1, 'rgba(80,16,4,0)');
    ctx.fillStyle = core;
    ctx.beginPath(); ctx.arc(0, 0, 176, 0, Math.PI * 2); ctx.fill();
    ctx.restore();

    ctx.globalCompositeOperation = 'lighter';
    ctx.globalAlpha = freeze ? .62 : .40;
    ctx.strokeStyle = '#ff6a2c';
    ctx.lineWidth = 1.4;
    ctx.save();
    ctx.translate(cx, cy);
    ctx.scale(1.34, .76);
    ctx.beginPath(); ctx.arc(0, 0, 174, 0, Math.PI * 2); ctx.stroke();
    ctx.globalAlpha *= .55;
    ctx.beginPath(); ctx.arc(0, 0, 194, 0, Math.PI * 2); ctx.stroke();
    ctx.restore();

    // vertical lens flare
    const vg = ctx.createLinearGradient(0, cy - 390, 0, cy + 390);
    vg.addColorStop(0, 'rgba(255,120,60,0)');
    vg.addColorStop(.5, 'rgba(255,190,140,.20)');
    vg.addColorStop(1, 'rgba(255,120,60,0)');
    ctx.globalAlpha = 1;
    ctx.fillStyle = vg;
    ctx.fillRect(cx - 1.6, cy - 390, 3.2, 780);

    // freeze frame gets a few orbital arcs for density
    if (freeze) {
      ctx.strokeStyle = 'rgba(255,150,90,.16)';
      ctx.lineWidth = 1;
      for (let i = 0; i < 3; i++) {
        ctx.save();
        ctx.translate(cx, cy);
        ctx.scale(1.27, .73);
        ctx.beginPath();
        ctx.arc(0, 0, 224 + i * 76, .3 + i * 1.1, 2.1 + i * 1.1);
        ctx.stroke();
        ctx.restore();
      }
    }

    ctx.globalCompositeOperation = 'source-over';
    ctx.globalAlpha = 1;
  }

  /* -------------------------------------------------------------------- rain */
  const rain = { cols: [] };
  const TOKENS = [
    'R_Init', 'idRenderSystem', 'GL_ArrayElement', 'fopen', 'unzOpen', 'fs_basepath',
    'Sys_AsyncThread', 'common->Frame', 'pak0.pk4', 'GLSL ES 3.00', 'MEMFS', 'wasm32',
    'emscripten', 'SDL2', 'PointerLock', 'net_serverDedicated', 'qglBegin', 'ARB2',
    'idTech4', '#include', 'idFile', 'idStr', 'R_GLSL', 'idVec3', 'CPUID'
  ];
  function initRain() {
    const rnd = mulberry32(4242);
    for (let x = 18; x < W; x += 25) {
      rain.cols.push({
        x,
        y0: rnd() * (H + 80),
        sp: 38 + rnd() * 132,
        tok: TOKENS[(rnd() * TOKENS.length) | 0],
        hot: rnd() < .12,
        a: .075 + rnd() * .14
      });
    }
  }

  function drawRain(t, freeze) {
    if (!ctx2) return;
    const ctx = ctx2;
    ctx.globalCompositeOperation = 'source-over';
    ctx.globalAlpha = 1;
    if (freeze) { ctx.fillStyle = '#060607'; ctx.fillRect(0, 0, W, H); }
    else { ctx.fillStyle = 'rgba(6,6,7,.16)'; ctx.fillRect(0, 0, W, H); }

    // measurement grid
    ctx.strokeStyle = 'rgba(244,239,230,.030)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let x = 0; x <= W; x += 120) { ctx.moveTo(x + .5, 0); ctx.lineTo(x + .5, H); }
    for (let y = 0; y <= H; y += 120) { ctx.moveTo(0, y + .5); ctx.lineTo(W, y + .5); }
    ctx.stroke();

    const time = freeze ? 5.2 : t;
    ctx.font = '12px "Cascadia Mono", Consolas, monospace';
    ctx.textBaseline = 'middle';
    for (const c of rain.cols) {
      const y = ((c.y0 + time * c.sp) % (H + 80)) - 40;
      ctx.globalAlpha = c.hot ? Math.min(1, c.a + .16) : c.a;
      ctx.fillStyle = c.hot ? '#ff7a3c' : '#f4efe6';
      ctx.fillText(c.tok, c.x, y);
      ctx.globalAlpha *= .30;
      ctx.fillStyle = '#ff4d17';
      ctx.fillText(c.tok, c.x, y - 17);
    }

    // scanning beam
    const by = freeze ? 438 : ((time * 118) % 1320) - 120;
    const bg = ctx.createLinearGradient(0, by - 46, 0, by + 46);
    bg.addColorStop(0, 'rgba(255,77,23,0)');
    bg.addColorStop(.5, 'rgba(255,77,23,.085)');
    bg.addColorStop(1, 'rgba(255,77,23,0)');
    ctx.globalAlpha = 1;
    ctx.fillStyle = bg;
    ctx.fillRect(0, by - 46, W, 92);
    ctx.strokeStyle = 'rgba(255,120,60,.38)';
    ctx.beginPath(); ctx.moveTo(0, by + .5); ctx.lineTo(W, by + .5); ctx.stroke();

    // a few highlighted blocks, like a diff
    ctx.globalAlpha = .055;
    ctx.fillStyle = '#b8ff5a';
    ctx.fillRect(1510, 214, 276, 22);
    ctx.fillRect(1640, 268, 146, 14);
    ctx.globalAlpha = .04;
    ctx.fillStyle = '#ff4d17';
    ctx.fillRect(804, 702, 322, 18);

    ctx.globalAlpha = 1;
  }

  /* ----------------------------------------------------------------- network */
  const NET = { pulses: [], active: 0 };
  function node(n) { return STAGES.find(s => s.n === n); }

  function controlPoint(e) {
    const A = node(e.a), B = node(e.b);
    const mx = (A.x + B.x) / 2, my = (A.y + B.y) / 2;
    if (e.kind === 'loop') return { x: 975, y: 702 };
    const dx = B.x - A.x, dy = B.y - A.y;
    const len = Math.hypot(dx, dy) || 1;
    const nx = -dy / len, ny = dx / len;
    const off = e.kind === 'feedback' ? 78 : -46;
    return { x: mx + nx * off, y: my + ny * off };
  }

  function quad(A, C, B, t) {
    const u = 1 - t;
    return {
      x: u * u * A.x + 2 * u * t * C.x + t * t * B.x,
      y: u * u * A.y + 2 * u * t * C.y + t * t * B.y
    };
  }

  function drawNetwork(t, freeze) {
    if (!ctx3) return;
    const ctx = ctx3;
    ctx.globalCompositeOperation = 'source-over';
    ctx.globalAlpha = 1;
    ctx.fillStyle = '#060607';
    ctx.fillRect(0, 0, W, H);

    const bg = ctx.createRadialGradient(960, 540, 40, 960, 540, 900);
    bg.addColorStop(0, 'rgba(255,77,23,.055)');
    bg.addColorStop(.55, 'rgba(255,77,23,.018)');
    bg.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = bg;
    ctx.fillRect(0, 0, W, H);

    const time = freeze ? 2.6 : t;

    // edges
    for (const e of EDGES) {
      const A = node(e.a), B = node(e.b), C = controlPoint(e);
      ctx.beginPath();
      ctx.moveTo(A.x, A.y);
      ctx.quadraticCurveTo(C.x, C.y, B.x, B.y);
      if (e.kind === 'main') {
        ctx.strokeStyle = 'rgba(244,239,230,.235)';
        ctx.lineWidth = 1.15;
        ctx.setLineDash([]);
      } else if (e.kind === 'feedback') {
        ctx.strokeStyle = 'rgba(255,77,23,.38)';
        ctx.lineWidth = 1;
        ctx.setLineDash([5, 7]);
      } else {
        ctx.strokeStyle = 'rgba(255,77,23,.27)';
        ctx.lineWidth = 1;
        ctx.setLineDash([2, 8]);
      }
      ctx.stroke();
      ctx.setLineDash([]);
    }

    // direction chevrons
    for (const e of EDGES.filter(x => x.kind === 'main')) {
      const A = node(e.a), B = node(e.b), C = controlPoint(e);
      const p1 = quad(A, C, B, .58), p2 = quad(A, C, B, .62);
      const ang = Math.atan2(p2.y - p1.y, p2.x - p1.x);
      ctx.save();
      ctx.translate(p1.x, p1.y); ctx.rotate(ang);
      ctx.fillStyle = 'rgba(255,154,92,.42)';
      ctx.beginPath(); ctx.moveTo(4, 0); ctx.lineTo(-4, -3.2); ctx.lineTo(-4, 3.2); ctx.closePath(); ctx.fill();
      ctx.restore();
    }

    // pulses
    ctx.globalCompositeOperation = 'lighter';
    for (const e of EDGES) {
      const A = node(e.a), B = node(e.b), C = controlPoint(e);
      const p = ((e.phase + time * e.speed) % 1);
      const pt = quad(A, C, B, p);
      const s = e.kind === 'loop' ? 6 : e.kind === 'feedback' ? 7 : 9;
      ctx.globalAlpha = e.kind === 'feedback' ? .72 : .95;
      ctx.drawImage(e.kind === 'feedback' ? EMBER : SPARK, pt.x - s, pt.y - s, s * 2, s * 2);
    }

    // node halos
    for (const s of STAGES) {
      const hot = NET.active === s.n;
      const gr = ctx.createRadialGradient(s.x, s.y, 4, s.x, s.y, hot ? 118 : 76);
      gr.addColorStop(0, hot ? 'rgba(255,120,50,.28)' : 'rgba(255,77,23,.10)');
      gr.addColorStop(.45, hot ? 'rgba(255,77,23,.10)' : 'rgba(255,77,23,.035)');
      gr.addColorStop(1, 'rgba(255,77,23,0)');
      ctx.globalAlpha = 1;
      ctx.fillStyle = gr;
      ctx.fillRect(s.x - 130, s.y - 130, 260, 260);
    }

    // loop label
    ctx.globalCompositeOperation = 'source-over';
    ctx.globalAlpha = 1;
    ctx.font = '9px "Cascadia Mono", Consolas, monospace';
    ctx.fillStyle = 'rgba(255,154,92,.42)';
    ctx.textAlign = 'center';
    ctx.fillText('FEEDBACK LOOP · REQUIRED', 975, 726);
    ctx.textAlign = 'left';
  }

  function init() {
    try { initPortal(); initRain(); } catch (e) { /* static gradients remain */ }
    for (const s of STAGES) {
      const el = document.querySelector(`[data-stage="${s.n}"]`);
      if (el) { el.style.left = s.x + 'px'; el.style.top = s.y + 'px'; }
    }
  }

  function drawActive(page, t, freeze) {
    if (page === 1) {
      drawPortal(t, freeze);
      if (window.__SHOWCASE_FRAME__) window.__SHOWCASE_FRAME__.draw(t, freeze);
    } else if (page === 2) drawRain(t, freeze);
    else if (page === 3) drawNetwork(t, freeze);
  }

  function drawAllStatic() {
    drawPortal(0, true);
    if (window.__SHOWCASE_FRAME__) window.__SHOWCASE_FRAME__.draw(0, true);
    drawRain(0, true);
    drawNetwork(0, true);
  }

  function setStageActive(n) { NET.active = n; }
  function setParallax(x, y) { portal.mx = x; portal.my = y; }

  window.__SHOWCASE_CANVAS__ = {
    stages: STAGES,
    init,
    drawActive,
    drawAllStatic,
    setStageActive,
    setParallax
  };
})();
