/* 2026 isometric level map: a Mars base cutaway, AI route, readable rooms. */
(() => {
  'use strict';
  const W = 1920, H = 1080;
  const DPR = Math.min(2, window.devicePixelRatio || 1);
  const cv = document.getElementById('fx2');
  if (!cv) return;
  cv.width = W * DPR; cv.height = H * DPR;
  const ctx = cv.getContext('2d');
  if (!ctx) return;
  ctx.setTransform(DPR, 0, 0, DPR, 0, 0);

  const TILE = 42, COS = .866, SIN = .5, OX = 1180, OY = 248;
  const iso = (x, y, h = 0) => ({ x: OX + (x - y) * TILE * COS, y: OY + (x + y) * TILE * SIN - h });

  const ROOMS = [
    { id: 'MEDLAB', x: 1.2, y: 1.2, w: 3.6, d: 2.6, color: '#57d7ff', floor: '#0e2630', label: 'MEDLAB', lx: 1.9, ly: 3.1 },
    { id: 'SECURITY', x: 6.4, y: 1.2, w: 3.0, d: 2.6, color: '#ffb347', floor: '#2a1d0c', label: 'SECURITY', lx: 6.7, ly: 3.2 },
    { id: 'CONTROL', x: 11.4, y: 1.2, w: 3.4, d: 2.6, color: '#8cc8ff', floor: '#12223a', label: 'CONTROL', lx: 11.8, ly: 3.4 },
    { id: 'REACTOR', x: 11.4, y: 6.0, w: 3.4, d: 3.0, color: '#b8ff5a', floor: '#142a12', label: 'REACTOR', lx: 11.8, ly: 8.6 },
    { id: 'HELL', x: 11.4, y: 10.0, w: 3.4, d: 1.8, color: '#ff4d17', floor: '#33110a', label: 'HELL GATE', lx: 11.9, ly: 10.3 },
    { id: 'LANDING', x: 1.2, y: 9.0, w: 3.0, d: 2.0, color: '#ff9a5c', floor: '#2a170c', label: 'LANDING', lx: 1.9, ly: 10.7 },
    { id: 'CARGO', x: 6.2, y: 8.4, w: 3.0, d: 2.2, color: '#9a8f80', floor: '#1a1712', label: 'CARGO', lx: 6.6, ly: 9.9 }
  ];

  const CORRIDORS = [
    { x: 1.2, y: 4.0, w: 13.6, d: 1.2, h: 'h' },
    { x: 5.2, y: 1.2, w: 1.2, d: 10.2, h: 'v' },
    { x: 10.0, y: 1.2, w: 1.2, d: 10.2, h: 'v' },
    { x: 1.2, y: 7.2, w: 5.2, d: 1.0, h: 'h' },
    { x: 10.0, y: 7.2, w: 4.8, d: 1.0, h: 'h' }
  ];

  const ROUTE = [
    [2.6, 10.0], [2.6, 8.1], [2.6, 4.6], [5.8, 4.6], [5.8, 2.7], [8.0, 2.7],
    [8.0, 4.6], [10.6, 4.6], [10.6, 7.5], [13.1, 7.5], [13.1, 10.6]
  ];

  function mulberry32(a) {
    return function () {
      a |= 0; a = a + 0x6D2B79F5 | 0;
      let t = Math.imul(a ^ a >>> 15, 1 | a);
      t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
      return ((t ^ t >>> 14) >>> 0) / 4294967296;
    };
  }
  const rnd = mulberry32(2026);
  const dust = Array.from({ length: 80 }, () => ({ x: rnd() * W, y: rnd() * H, s: .5 + rnd() * 1.8, ph: rnd() * 6.28, sp: 2 + rnd() * 12 }));

  function poly(pts, fill, stroke, lw) {
    ctx.beginPath();
    ctx.moveTo(pts[0].x, pts[0].y);
    for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i].x, pts[i].y);
    ctx.closePath();
    if (fill) { ctx.fillStyle = fill; ctx.fill(); }
    if (stroke) { ctx.strokeStyle = stroke; ctx.lineWidth = lw || 1; ctx.stroke(); }
  }
  function line(a, b, color, lw, dash) {
    ctx.strokeStyle = color; ctx.lineWidth = lw || 1;
    ctx.setLineDash(dash || []);
    ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.stroke();
    ctx.setLineDash([]);
  }
  function glow(x, y, r, rgba) {
    const g = ctx.createRadialGradient(x, y, 0, x, y, r);
    g.addColorStop(0, rgba); g.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = g; ctx.fillRect(x - r, y - r, r * 2, r * 2);
  }
  function rect(x, y, w, h, fill) { ctx.fillStyle = fill; ctx.fillRect(x, y, w, h); }

  function floorRect(x, y, w, d, fill, stroke, alpha) {
    const a = iso(x, y), b = iso(x + w, y), c = iso(x + w, y + d), e = iso(x, y + d);
    poly([a, b, c, e], fill, stroke, 1);
    if (alpha) {
      ctx.globalAlpha = alpha;
      ctx.strokeStyle = stroke; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(c.x, c.y); ctx.stroke();
      ctx.globalAlpha = 1;
    }
  }

  function wall(x1, y1, x2, y2, h, color, accent, panels) {
    const a = iso(x1, y1), b = iso(x2, y2);
    const at = { x: a.x, y: a.y - h }, bt = { x: b.x, y: b.y - h };
    poly([a, b, bt, at], color, 'rgba(244,239,230,.06)', 1);
    poly([at, bt, { x: bt.x, y: bt.y + 4 }, { x: at.x, y: at.y + 4 }], accent, null, 0);
    line(at, bt, accent, 1.4);
    if (panels) {
      for (let i = 1; i < panels; i++) {
        const t = i / panels;
        const p1 = { x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t };
        const p2 = { x: p1.x, y: p1.y - h };
        line(p1, p2, 'rgba(244,239,230,.055)', 1);
      }
    }
  }

  function box(x, y, w, d, h, body, top, accent) {
    const a = iso(x, y), b = iso(x + w, y), c = iso(x + w, y + d), e = iso(x, y + d);
    const at = { x: a.x, y: a.y - h }, bt = { x: b.x, y: b.y - h };
    const ct = { x: c.x, y: c.y - h }, et = { x: e.x, y: e.y - h };
    // shadow
    ctx.globalAlpha = .32; poly([a, b, c, e], '#000'); ctx.globalAlpha = 1;
    // right + front faces
    poly([c, b, bt, ct], body);
    poly([e, c, ct, et], top);
    poly([at, bt, ct, et], top);
    poly([at, et, ct, bt], 'rgba(255,255,255,.035)', accent, 1);
    line(at, bt, accent, 1);
    line(at, et, accent, 1);
  }

  function cylinder(x, y, r, h, body, accent) {
    const p = iso(x, y);
    const ry = r * .55;
    // body
    ctx.fillStyle = body;
    ctx.beginPath();
    ctx.ellipse(p.x, p.y - h * .5, r, ry, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillRect(p.x - r, p.y - h * .5, r * 2, h * .5);
    ctx.beginPath();
    ctx.ellipse(p.x, p.y, r, ry, 0, 0, Math.PI * 2);
    ctx.fill();
    // top
    ctx.fillStyle = 'rgba(255,255,255,.06)';
    ctx.beginPath(); ctx.ellipse(p.x, p.y - h, r, ry, 0, 0, Math.PI * 2); ctx.fill();
    ctx.strokeStyle = accent; ctx.lineWidth = 1.4;
    ctx.beginPath(); ctx.ellipse(p.x, p.y - h, r, ry, 0, 0, Math.PI * 2); ctx.stroke();
    ctx.globalAlpha = .5;
    ctx.beginPath(); ctx.ellipse(p.x, p.y - h * .55, r * .78, ry * .78, 0, 0, Math.PI * 2); ctx.stroke();
    ctx.globalAlpha = 1;
  }

  function label(x, y, text, color) {
    const p = iso(x, y, 5);
    ctx.font = '10px "Cascadia Mono", Consolas, monospace';
    const w = ctx.measureText(text).width;
    ctx.fillStyle = 'rgba(5,5,6,.86)';
    ctx.fillRect(p.x - w / 2 - 5, p.y - 13, w + 10, 17);
    ctx.strokeStyle = 'rgba(244,239,230,.08)'; ctx.lineWidth = 1;
    ctx.strokeRect(p.x - w / 2 - 5, p.y - 13, w + 10, 17);
    ctx.fillStyle = color; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    ctx.fillText(text, p.x, p.y - 5);
    ctx.textAlign = 'left'; ctx.textBaseline = 'alphabetic';
  }

  function door(x1, y1, x2, y2, color) {
    const a = iso(x1, y1), b = iso(x2, y2);
    const mx = (a.x + b.x) / 2, my = (a.y + b.y) / 2;
    ctx.save();
    ctx.translate(mx, my);
    const ang = Math.atan2(b.y - a.y, b.x - a.x);
    ctx.rotate(ang);
    ctx.fillStyle = 'rgba(5,5,6,.95)';
    ctx.fillRect(-16, -32, 32, 32);
    ctx.strokeStyle = color; ctx.lineWidth = 1.5;
    ctx.strokeRect(-16, -32, 32, 32);
    ctx.globalAlpha = .35; ctx.fillStyle = color; ctx.fillRect(-12, -27, 24, 22); ctx.globalAlpha = 1;
    ctx.restore();
  }

  function drawBase() {
    const corners = [iso(0, 0), iso(16, 0), iso(16, 12), iso(0, 12)];
    const lower = corners.map(p => ({ x: p.x, y: p.y + 18 }));
    poly([corners[3], corners[2], lower[2], lower[3]], '#09090b');
    poly([corners[2], corners[1], lower[1], lower[2]], '#0b0b0d');
    poly(corners, '#17151a', 'rgba(92,205,255,.32)', 1);
    // grid
    for (let x = 0; x <= 16; x++) line(iso(x, 0), iso(x, 12), 'rgba(244,239,230,.06)', 1);
    for (let y = 0; y <= 12; y++) line(iso(0, y), iso(16, y), 'rgba(244,239,230,.06)', 1);
    // rim
    line(corners[3], corners[2], 'rgba(92,205,255,.65)', 2);
    line(corners[2], corners[1], 'rgba(255,150,80,.20)', 2);
  }

  function drawRooms() {
    for (const r of ROOMS) {
      floorRect(r.x, r.y, r.w, r.d, r.floor, 'rgba(255,255,255,.045)', .10);
      // room light pool
      const c = iso(r.x + r.w / 2, r.y + r.d / 2);
      ctx.globalCompositeOperation = 'lighter';
      glow(c.x, c.y, 150, `rgba(${hexRgb(r.color)},.11)`);
      ctx.globalCompositeOperation = 'source-over';
      // high back walls
      wall(r.x, r.y, r.x + r.w, r.y, 36, '#111114', `rgba(${hexRgb(r.color)},.85)`, 4);
      wall(r.x, r.y, r.x, r.y + r.d, 36, '#101013', `rgba(${hexRgb(r.color)},.60)`, 3);
      // low front lip
      wall(r.x, r.y + r.d, r.x + r.w, r.y + r.d, 8, '#101013', `rgba(${hexRgb(r.color)},.55)`, 0);
      wall(r.x + r.w, r.y, r.x + r.w, r.y + r.d, 8, '#101013', `rgba(${hexRgb(r.color)},.55)`, 0);
    }
    for (const c of CORRIDORS) {
      floorRect(c.x, c.y, c.w, c.d, '#0d0d10', 'rgba(255,255,255,.035)', .06);
      if (c.h === 'h') {
        line(iso(c.x, c.y + c.d / 2), iso(c.x + c.w, c.y + c.d / 2), 'rgba(255,170,110,.10)', 1);
      } else {
        line(iso(c.x + c.w / 2, c.y), iso(c.x + c.w / 2, c.y + c.d), 'rgba(92,205,255,.09)', 1);
      }
    }
  }

  function drawProps(t, freeze) {
    // MEDLAB beds
    box(1.8, 1.8, .7, 1.4, 16, '#0d161a', '#123039', '#57d7ff');
    box(3.3, 1.8, .7, 1.4, 16, '#0d161a', '#123039', '#57d7ff');
    // SECURITY consoles
    for (let i = 0; i < 3; i++) box(6.9 + i * .8, 1.9, .55, .5, 18, '#141008', '#2a1b0a', '#ffb347');
    // CONTROL holo table
    cylinder(13.1, 2.4, 34, 24, '#0a1119', '#8cc8ff');
    ctx.globalCompositeOperation = 'lighter';
    const holo = iso(13.1, 2.4, 46);
    glow(holo.x, holo.y, 55, `rgba(140,200,255,${freeze ? .16 : .12 + .05 * Math.sin(t * 2)})`);
    ctx.strokeStyle = 'rgba(140,200,255,.55)'; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.ellipse(holo.x, holo.y, 23, 10, 0, 0, Math.PI * 2); ctx.stroke();
    ctx.globalCompositeOperation = 'source-over';
    // REACTOR
    cylinder(13.0, 7.4, 46, 64, '#0b130c', '#b8ff5a');
    ctx.globalCompositeOperation = 'lighter';
    const rp = iso(13.0, 7.4, 70);
    glow(rp.x, rp.y, 90, `rgba(184,255,90,${freeze ? .22 : .16 + .08 * Math.sin(t * 2.3)})`);
    line({ x: rp.x, y: rp.y }, { x: rp.x, y: rp.y - 80 }, 'rgba(184,255,90,.18)', 2);
    ctx.globalCompositeOperation = 'source-over';
    // HELL GATE
    const hp = iso(13.1, 10.8);
    ctx.globalCompositeOperation = 'lighter';
    glow(hp.x, hp.y - 34, 80, `rgba(255,60,20,${freeze ? .30 : .22 + .10 * Math.sin(t * 3.1)})`);
    ctx.globalCompositeOperation = 'source-over';
    ctx.save();
    ctx.translate(hp.x, hp.y - 34);
    ctx.strokeStyle = 'rgba(255,77,23,.85)'; ctx.lineWidth = 4;
    ctx.beginPath(); ctx.ellipse(0, 0, 34, 46, 0, 0, Math.PI * 2); ctx.stroke();
    ctx.strokeStyle = 'rgba(255,170,90,.55)'; ctx.lineWidth = 1.2;
    ctx.beginPath(); ctx.ellipse(0, 0, 27, 38, 0, 0, Math.PI * 2); ctx.stroke();
    ctx.fillStyle = '#050303'; ctx.beginPath(); ctx.ellipse(0, 0, 23, 33, 0, 0, Math.PI * 2); ctx.fill();
    for (let i = 0; i < 5; i++) {
      const a = (i / 5) * Math.PI * 2;
      poly([{ x: Math.cos(a) * 34, y: Math.sin(a) * 46 }, { x: Math.cos(a + .18) * 46, y: Math.sin(a + .18) * 58 }, { x: Math.cos(a + .36) * 34, y: Math.sin(a + .36) * 46 }], 'rgba(255,77,23,.25)');
    }
    ctx.restore();
    // LANDING pad
    const lp = iso(2.7, 10.0);
    ctx.strokeStyle = 'rgba(255,154,92,.65)'; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.ellipse(lp.x, lp.y, 82, 40, 0, 0, Math.PI * 2); ctx.stroke();
    ctx.strokeStyle = 'rgba(255,154,92,.20)'; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.ellipse(lp.x, lp.y, 55, 27, 0, 0, Math.PI * 2); ctx.stroke();
    for (let i = 0; i < 4; i++) {
      const a = i * Math.PI / 2;
      line({ x: lp.x + Math.cos(a) * 60, y: lp.y + Math.sin(a) * 29 }, { x: lp.x + Math.cos(a) * 75, y: lp.y + Math.sin(a) * 36 }, 'rgba(255,154,92,.55)', 2);
    }
    // CARGO
    box(6.5, 8.7, .9, .8, 28, '#12100d', '#1c1915', '#9a8f80');
    box(7.5, 9.0, .9, .8, 20, '#12100d', '#1c1915', '#9a8f80');
    box(7.0, 9.7, .7, .6, 14, '#12100d', '#1c1915', '#9a8f80');
    // corridor pipes / lights
    for (let i = 0; i < 5; i++) {
      const p = iso(2.0 + i * 2.4, 4.6, 8);
      ctx.fillStyle = 'rgba(255,180,120,.18)';
      ctx.fillRect(p.x - 8, p.y - 2, 16, 2);
    }
    // hostiles
    const hostiles = [[3.1, 2.8], [7.2, 2.0], [12.9, 7.7], [13.0, 10.4], [7.4, 9.0]];
    for (const [hx, hy] of hostiles) {
      const p = iso(hx, hy, 12);
      const pulse = freeze ? 1 : .7 + .3 * Math.sin(t * 3.4 + hx);
      ctx.globalCompositeOperation = 'lighter';
      glow(p.x, p.y, 20, `rgba(255,60,30,${.28 * pulse})`);
      ctx.fillStyle = `rgba(255,80,40,${.85 * pulse})`;
      ctx.beginPath(); ctx.arc(p.x, p.y, 3.2, 0, Math.PI * 2); ctx.fill();
      ctx.globalCompositeOperation = 'source-over';
    }
  }

  function drawLabels() {
    for (const r of ROOMS) {
      label(r.lx || (r.x + r.w / 2), r.ly || (r.y + r.d / 2), r.label, r.color);
    }
  }

  function drawRoute(t, freeze) {
    const pts = ROUTE.map(([x, y]) => iso(x, y, 6));
    ctx.globalCompositeOperation = 'lighter';
    ctx.strokeStyle = 'rgba(92,205,255,.38)';
    ctx.lineWidth = 2;
    ctx.setLineDash([8, 8]);
    ctx.lineDashOffset = freeze ? 0 : -t * 22;
    ctx.beginPath();
    ctx.moveTo(pts[0].x, pts[0].y);
    for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i].x, pts[i].y);
    ctx.stroke();
    ctx.setLineDash([]);
    // pulses
    for (let i = 0; i < 3; i++) {
      const p = ((t * .12 + i / 3) % 1);
      const seg = Math.min(pts.length - 2, Math.floor(p * (pts.length - 1)));
      const local = p * (pts.length - 1) - seg;
      const a = pts[seg], b = pts[seg + 1];
      const x = a.x + (b.x - a.x) * local, y = a.y + (b.y - a.y) * local;
      glow(x, y, 16, 'rgba(92,205,255,.5)');
    }
    ctx.globalCompositeOperation = 'source-over';
    // player marker
    const pl = iso(2.6, 10.0, 8);
    ctx.fillStyle = 'rgba(184,255,90,.9)';
    ctx.beginPath(); ctx.moveTo(pl.x, pl.y - 8); ctx.lineTo(pl.x - 6, pl.y + 4); ctx.lineTo(pl.x + 6, pl.y + 4); ctx.closePath(); ctx.fill();
    // AI node
    const ai = iso(13.1, 2.4, 52);
    ctx.strokeStyle = 'rgba(140,200,255,.7)'; ctx.lineWidth = 1.4;
    ctx.beginPath(); ctx.arc(ai.x, ai.y, 12, 0, Math.PI * 2); ctx.stroke();
    ctx.beginPath(); ctx.arc(ai.x, ai.y, 5, 0, Math.PI * 2); ctx.stroke();
    ctx.fillStyle = 'rgba(140,200,255,.85)';
    ctx.beginPath(); ctx.arc(ai.x, ai.y, 2, 0, Math.PI * 2); ctx.fill();
  }

  function hexRgb(hex) {
    const h = hex.replace('#', '');
    return `${parseInt(h.slice(0, 2), 16)},${parseInt(h.slice(2, 4), 16)},${parseInt(h.slice(4, 6), 16)}`;
  }

  function drawMapTitle() {
    ctx.font = '9px Consolas, monospace';
    ctx.fillStyle = 'rgba(92,205,255,.92)';
    ctx.fillText('MARS BASE / SECTOR 03', 800, 222);
    ctx.fillStyle = 'rgba(244,239,230,.58)';
    ctx.fillText('SEED 7F3A  ·  AI ROUTE ACTIVE  ·  12 HOSTILES', 1000, 222);
  }

  function drawPanel() {
    const x = 742, y = 512, w = 310, h = 110;
    ctx.fillStyle = 'rgba(5,5,6,.78)';
    ctx.fillRect(x, y, w, h);
    ctx.strokeStyle = 'rgba(244,239,230,.12)'; ctx.lineWidth = 1;
    ctx.strokeRect(x, y, w, h);
    ctx.strokeStyle = 'rgba(92,205,255,.65)'; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(x, y + h); ctx.stroke();
    ctx.font = '8px "Cascadia Mono", Consolas, monospace';
    ctx.fillStyle = '#57d7ff';
    ctx.fillText('MARS BASE / SECTOR 03', x + 12, y + 18);
    ctx.fillStyle = 'rgba(244,239,230,.55)';
    ctx.fillText('SEED 7F3A · 12 ROOMS', x + 12, y + 34);
    ctx.fillStyle = 'rgba(255,154,92,.8)';
    ctx.fillText('AI DIRECTOR / ROUTE ACTIVE', x + 12, y + 50);
    ctx.fillStyle = 'rgba(244,239,230,.35)';
    ctx.fillText('HOSTILES 12 · HELL GATE 1', x + 12, y + 66);
    ctx.fillStyle = 'rgba(184,255,90,.8)';
    ctx.fillText('● PLAYER', x + 12, y + 88);
    ctx.fillStyle = 'rgba(255,80,40,.8)';
    ctx.fillText('● HOSTILE', x + 86, y + 88);
    ctx.fillStyle = 'rgba(92,205,255,.8)';
    ctx.fillText('● AI NODE', x + 160, y + 88);
  }

  function draw(t, freeze) {
    const time = freeze ? 6.6 : t;
    const swayX = freeze ? 0 : Math.sin(time * .27) * 2.4;
    const swayY = freeze ? 0 : Math.cos(time * .37) * 1.4;
    ctx.globalCompositeOperation = 'source-over';
    ctx.globalAlpha = 1;
    ctx.fillStyle = '#050506';
    ctx.fillRect(0, 0, W, H);

    // void glow + radar rings behind the diorama
    const bg = ctx.createRadialGradient(1180 + swayX, 560 + swayY, 40, 1180, 560, 800);
    bg.addColorStop(0, 'rgba(255,110,50,.055)');
    bg.addColorStop(.45, 'rgba(92,205,255,.025)');
    bg.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = bg; ctx.fillRect(0, 0, W, H);
    for (let i = 1; i <= 3; i++) {
      ctx.strokeStyle = `rgba(92,205,255,${.035 / i})`; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.ellipse(1180, 560, 260 * i, 140 * i, 0, 0, Math.PI * 2); ctx.stroke();
    }

    ctx.save();
    ctx.translate(swayX, swayY);
    drawBase();
    drawRooms();
    drawProps(time, freeze);
    drawRoute(time, freeze);
    drawLabels();
    ctx.restore();

    drawMapTitle();

    // floating dust
    ctx.globalCompositeOperation = 'lighter';
    for (const d of dust) {
      const y = ((d.y - time * d.sp) % (H + 40) + H + 40) % (H + 40) - 20;
      const x = d.x + Math.sin(time * .3 + d.ph) * 10;
      ctx.globalAlpha = .05 + .10 * (.5 + .5 * Math.sin(time * 1.7 + d.ph));
      ctx.fillStyle = '#ffd0a0';
      ctx.fillRect(x, y, d.s, d.s);
    }
    ctx.globalCompositeOperation = 'source-over';
    ctx.globalAlpha = 1;

    // left falloff for the copy, bottom falloff for the cards
    const left = ctx.createLinearGradient(0, 0, 760, 0);
    left.addColorStop(0, 'rgba(5,5,6,.72)');
    left.addColorStop(1, 'rgba(5,5,6,0)');
    ctx.fillStyle = left; ctx.fillRect(0, 0, 760, H);
    const bottom = ctx.createLinearGradient(0, H - 420, 0, H);
    bottom.addColorStop(0, 'rgba(5,5,6,0)');
    bottom.addColorStop(1, 'rgba(5,5,6,.94)');
    ctx.fillStyle = bottom; ctx.fillRect(0, H - 420, W, 420);

    const vig = ctx.createRadialGradient(1180, 560, 320, 1180, 560, 1100);
    vig.addColorStop(0, 'rgba(0,0,0,0)');
    vig.addColorStop(1, 'rgba(0,0,0,.55)');
    ctx.fillStyle = vig; ctx.fillRect(0, 0, W, H);
  }

  window.__SHOWCASE_MAP__ = { draw, rooms: ROOMS.map(r => r.label) };
})();
