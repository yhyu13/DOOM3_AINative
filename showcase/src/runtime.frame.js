/* Procedural reference frame: an original industrial corridor, not a game capture. */
(() => {
  'use strict';
  const W = 1920, H = 1080;
  const DPR = Math.min(2, window.devicePixelRatio || 1);
  const cv = document.getElementById('fxFrame');
  if (!cv) return;
  cv.width = W * DPR; cv.height = H * DPR;
  const ctx = cv.getContext('2d');
  ctx.setTransform(DPR, 0, 0, DPR, 0, 0);

  const ZS = [0.24, 0.29, 0.35, 0.43, 0.53, 0.65, 0.79, 1.0];

  function mulberry32(a) {
    return function () {
      a |= 0; a = a + 0x6D2B79F5 | 0;
      let t = Math.imul(a ^ a >>> 15, 1 | a);
      t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
      return ((t ^ t >>> 14) >>> 0) / 4294967296;
    };
  }
  const rnd = mulberry32(90210);
  const dust = Array.from({ length: 64 }, () => ({
    x: rnd() * W, y: rnd() * H, s: .6 + rnd() * 1.8, ph: rnd() * 6.28, sp: 5 + rnd() * 16
  }));

  function ring(z, vx, vy) {
    const s = 1 / z;
    return { z, s, l: vx - 330 * s, r: vx + 330 * s, t: vy - 225 * s, b: vy + 225 * s };
  }
  function quad(a, b, c, d, fill) {
    ctx.fillStyle = fill;
    ctx.beginPath();
    ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.lineTo(c[0], c[1]); ctx.lineTo(d[0], d[1]);
    ctx.closePath(); ctx.fill();
  }
  function line(x1, y1, x2, y2, stroke, width) {
    ctx.strokeStyle = stroke; ctx.lineWidth = width || 1;
    ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke();
  }
  function glow(x, y, r, rgba) {
    const g = ctx.createRadialGradient(x, y, 0, x, y, r);
    g.addColorStop(0, rgba);
    g.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = g;
    ctx.fillRect(x - r, y - r, r * 2, r * 2);
  }

  function draw(t, freeze) {
    const time = freeze ? 3.4 : t;
    const pulse = freeze ? .88 : .74 + .26 * Math.sin(time * 2.35);
    const vx = 1238 + (freeze ? 0 : Math.sin(time * .61) * 3.2);
    const vy = 522 + (freeze ? 0 : Math.cos(time * .83) * 2.2);

    ctx.globalCompositeOperation = 'source-over';
    ctx.globalAlpha = 1;

    // Base air and distant heat.
    const base = ctx.createLinearGradient(0, 0, 0, H);
    base.addColorStop(0, '#08080a');
    base.addColorStop(.55, '#0e0b0b');
    base.addColorStop(1, '#140d09');
    ctx.fillStyle = base;
    ctx.fillRect(0, 0, W, H);

    const amb = ctx.createRadialGradient(vx, vy, 30, vx, vy, 1050);
    amb.addColorStop(0, 'rgba(255,94,34,.20)');
    amb.addColorStop(.30, 'rgba(255,60,20,.075)');
    amb.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = amb;
    ctx.fillRect(0, 0, W, H);

    const rings = ZS.map(z => ring(z, vx, vy));
    const near = rings[0], far = rings[rings.length - 1];

    // Walls, near to far, with a warm shift toward the door.
    for (let i = 0; i < rings.length - 1; i++) {
      const a = rings[i], b = rings[i + 1];
      const p = i / (rings.length - 1);
      const ceil = `rgb(${Math.round(13 + 30 * p)},${Math.round(12 + 17 * p)},${Math.round(14 + 10 * p)})`;
      const floor = `rgb(${Math.round(25 + 32 * p)},${Math.round(19 + 19 * p)},${Math.round(16 + 12 * p)})`;
      const left = `rgb(${Math.round(18 + 27 * p)},${Math.round(14 + 15 * p)},${Math.round(13 + 9 * p)})`;
      const right = `rgb(${Math.round(13 + 21 * p)},${Math.round(11 + 12 * p)},${Math.round(10 + 7 * p)})`;
      quad([a.l, a.t], [a.r, a.t], [b.r, b.t], [b.l, b.t], ceil);
      quad([a.l, a.b], [a.r, a.b], [b.r, b.b], [b.l, b.b], floor);
      quad([a.l, a.t], [a.l, a.b], [b.l, b.b], [b.l, b.t], left);
      quad([a.r, a.t], [a.r, a.b], [b.r, b.b], [b.r, b.t], right);

      // Inset wall panels.
      const inset = 0.16;
      const lx1 = a.l + (a.r - a.l) * inset, lx2 = b.l + (b.r - b.l) * inset;
      const rx1 = a.r - (a.r - a.l) * inset, rx2 = b.r - (b.r - b.l) * inset;
      const yt1 = a.t + (a.b - a.t) * inset, yt2 = b.t + (b.b - b.t) * inset;
      const yb1 = a.b - (a.b - a.t) * inset, yb2 = b.b - (b.b - b.t) * inset;
      quad([lx1, yt1], [lx1, yb1], [lx2, yb2], [lx2, yt2], `rgba(255,180,120,${0.018 + p * .012})`);
      quad([rx1, yt1], [rx1, yb1], [rx2, yb2], [rx2, yt2], `rgba(255,120,70,${0.014 + p * .010})`);
    }

    // Structural seams and rivets.
    for (const r of rings) {
      ctx.strokeStyle = 'rgba(244,239,230,.075)';
      ctx.lineWidth = 1;
      ctx.strokeRect(r.l, r.t, r.r - r.l, r.b - r.t);
      ctx.strokeStyle = 'rgba(244,239,230,.04)';
      ctx.beginPath();
      ctx.moveTo(r.l, (r.t + r.b) / 2); ctx.lineTo(r.r, (r.t + r.b) / 2);
      ctx.stroke();
      for (const x of [r.l, r.r]) {
        for (const y of [r.t, r.b]) {
          ctx.fillStyle = 'rgba(255,190,140,.16)';
          ctx.beginPath(); ctx.arc(x, y, 2.2, 0, Math.PI * 2); ctx.fill();
        }
      }
    }

    // Floor grating: cross seams and three depth lines.
    for (const r of rings) {
      line(r.l, r.b, r.r, r.b, 'rgba(255,150,95,.055)', 1);
    }
    for (const u of [0.18, 0.38, 0.62, 0.82]) {
      const nx = near.l + (near.r - near.l) * u;
      const fx = far.l + (far.r - far.l) * u;
      line(nx, near.b, fx, far.b, 'rgba(255,150,95,.07)', 1);
    }

    // Ceiling beams and rails.
    for (const r of rings) {
      line(r.l, r.t, r.r, r.t, 'rgba(255,190,140,.10)', 3);
      line(r.l, r.t + 10, r.r, r.t + 10, 'rgba(0,0,0,.28)', 2);
    }

    // Pipes along both ceiling corners.
    for (const side of [-1, 1]) {
      const cx1 = vx + side * ((near.r - near.l) / 2 - 24);
      const cx2 = vx + side * ((far.r - far.l) / 2 - 7);
      const cy1 = near.t + 24, cy2 = far.t + 7;
      line(cx1, cy1, cx2, cy2, 'rgba(120,104,92,.40)', 8);
      line(cx1, cy1 - 2, cx2, cy2 - 1, 'rgba(255,196,150,.16)', 2);
      line(cx1, cy1 + 19, cx2, cy2 + 5, 'rgba(70,58,50,.45)', 4);
      for (const r of rings) {
        const rr = r.r - r.l;
        const x = vx + side * (rr / 2 - 24 * r.s * .2 - 6);
        const y = r.t + 24 * r.s * .2 + 6;
        ctx.strokeStyle = 'rgba(230,200,170,.20)';
        ctx.lineWidth = 2;
        ctx.beginPath(); ctx.arc(x, y, 5, 0, Math.PI * 2); ctx.stroke();
      }
    }

    // Far door.
    const doorW = 122, doorH = 246;
    const dx = vx - doorW / 2, dy = vy - doorH / 2 + 22;
    ctx.globalCompositeOperation = 'lighter';
    glow(vx, vy + 30, 320, `rgba(255,64,18,${.22 * pulse})`);
    glow(vx, vy + 70, 140, `rgba(255,170,100,${.20 * pulse})`);
    ctx.globalCompositeOperation = 'source-over';
    ctx.fillStyle = '#0b0605';
    ctx.fillRect(dx, dy, doorW, doorH);
    ctx.strokeStyle = 'rgba(255,120,60,.55)';
    ctx.lineWidth = 2;
    ctx.strokeRect(dx, dy, doorW, doorH);
    ctx.fillStyle = `rgba(255,77,23,${.62 + .18 * pulse})`;
    ctx.fillRect(vx - 2.5, dy + 16, 5, doorH - 32);
    ctx.fillStyle = `rgba(255,224,190,${.55 * pulse})`;
    ctx.fillRect(vx - .8, dy + 28, 1.6, doorH - 56);
    for (let i = 0; i < 5; i++) {
      ctx.fillStyle = i % 2 ? 'rgba(255,150,80,.20)' : 'rgba(20,8,5,.55)';
      ctx.fillRect(dx + 8, dy + 16 + i * 46, 10, 22);
    }
    // Warning bar above the door.
    for (let i = 0; i < 7; i++) {
      ctx.fillStyle = i % 2 ? 'rgba(255,170,80,.20)' : 'rgba(10,6,5,.6)';
      ctx.fillRect(vx - 84 + i * 24, far.t + 18, 24, 10);
    }

    // Ceiling lights with a soft cone.
    ctx.globalCompositeOperation = 'lighter';
    for (const z of [0.36, 0.50, 0.66, 0.84]) {
      const r = ring(z, vx, vy);
      const w = Math.max(42, 150 * r.s * .22);
      const y = r.t + 15;
      glow(vx, y, 190 * r.s * .25, `rgba(255,168,98,${.25 * pulse})`);
      ctx.fillStyle = `rgba(255,206,150,${.72 * pulse})`;
      ctx.fillRect(vx - w / 2, y - 3, w, 6);
      ctx.fillStyle = `rgba(255,96,34,${.92 * pulse})`;
      ctx.fillRect(vx - w / 2, y - 1, w, 1.4);
      const cone = ctx.createLinearGradient(0, y, 0, y + 260 * r.s * .2);
      cone.addColorStop(0, `rgba(255,190,120,${.055 * pulse})`);
      cone.addColorStop(1, 'rgba(255,190,120,0)');
      ctx.fillStyle = cone;
      ctx.beginPath();
      ctx.moveTo(vx - w / 2, y); ctx.lineTo(vx + w / 2, y);
      ctx.lineTo(vx + w, y + 260 * r.s * .2); ctx.lineTo(vx - w, y + 260 * r.s * .2);
      ctx.closePath(); ctx.fill();
    }

    // Emergency strips on the walls, with floor pools.
    for (const z of [0.46, 0.64, 0.86]) {
      const r = ring(z, vx, vy);
      const h = Math.max(42, 160 * r.s * .15);
      for (const side of [-1, 1]) {
        const x = side < 0 ? r.l + 10 : r.r - 16;
        glow(x + 3, vy + 20, 70, `rgba(255,42,10,${.20 * pulse})`);
        ctx.fillStyle = `rgba(255,54,16,${.78 * pulse})`;
        ctx.fillRect(x, vy - h / 2, 6, h);
        ctx.fillStyle = `rgba(255,190,150,${.35 * pulse})`;
        ctx.fillRect(x + 1.5, vy - h / 2 + 4, 1.5, h - 8);
        const pool = ctx.createRadialGradient(x, r.b - 10, 4, x, r.b - 10, 90);
        pool.addColorStop(0, `rgba(255,46,12,${.12 * pulse})`);
        pool.addColorStop(1, 'rgba(255,46,12,0)');
        ctx.fillStyle = pool;
        ctx.fillRect(x - 100, r.b - 100, 200, 200);
      }
    }
    ctx.globalCompositeOperation = 'source-over';

    // Door reflection on the floor.
    ctx.globalCompositeOperation = 'lighter';
    const refl = ctx.createLinearGradient(0, far.b, 0, near.b);
    refl.addColorStop(0, `rgba(255,92,32,${.15 * pulse})`);
    refl.addColorStop(.45, `rgba(255,70,20,${.055 * pulse})`);
    refl.addColorStop(1, 'rgba(255,70,20,0)');
    ctx.fillStyle = refl;
    ctx.beginPath();
    ctx.moveTo(vx - 54, far.b); ctx.lineTo(vx + 54, far.b);
    ctx.lineTo(vx + 165, near.b); ctx.lineTo(vx - 165, near.b);
    ctx.closePath(); ctx.fill();
    ctx.globalCompositeOperation = 'source-over';

    // Depth fog.
    const fog = ctx.createRadialGradient(vx, vy, 20, vx, vy, 660);
    fog.addColorStop(0, `rgba(255,124,58,${.18 * pulse})`);
    fog.addColorStop(.32, `rgba(255,94,40,${.075 * pulse})`);
    fog.addColorStop(1, 'rgba(255,77,23,0)');
    ctx.fillStyle = fog;
    ctx.fillRect(0, 0, W, H);

    // Dust motes.
    ctx.globalCompositeOperation = 'lighter';
    for (const d of dust) {
      const y = ((d.y - time * d.sp) % (H + 60) + H + 60) % (H + 60) - 30;
      const x = d.x + Math.sin(time * .3 + d.ph) * 18;
      ctx.globalAlpha = .08 + .14 * (.5 + .5 * Math.sin(time * 1.7 + d.ph));
      ctx.fillStyle = '#ffbd8a';
      ctx.fillRect(x, y, d.s, d.s);
    }
    ctx.globalCompositeOperation = 'source-over';
    ctx.globalAlpha = 1;

    // Crosshair + HUD ticks.
    ctx.strokeStyle = 'rgba(255,166,106,.55)';
    ctx.lineWidth = 1.2;
    ctx.beginPath();
    ctx.moveTo(vx - 16, vy); ctx.lineTo(vx - 5, vy);
    ctx.moveTo(vx + 5, vy); ctx.lineTo(vx + 16, vy);
    ctx.moveTo(vx, vy - 16); ctx.lineTo(vx, vy - 5);
    ctx.moveTo(vx, vy + 5); ctx.lineTo(vx, vy + 16);
    ctx.stroke();
    ctx.strokeStyle = 'rgba(255,166,106,.18)';
    ctx.beginPath(); ctx.arc(vx, vy, 27, 0, Math.PI * 2); ctx.stroke();
    ctx.strokeStyle = 'rgba(255,166,106,.16)';
    for (const [ax, ay] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) {
      const bx = vx + ax * 70, by = vy + ay * 70;
      ctx.beginPath(); ctx.moveTo(bx, by); ctx.lineTo(bx - ax * 13, by); ctx.lineTo(bx, by - ay * 13); ctx.stroke();
    }

    // Foreground falloff.
    const fg = ctx.createLinearGradient(0, H - 340, 0, H);
    fg.addColorStop(0, 'rgba(6,6,7,0)');
    fg.addColorStop(1, 'rgba(6,6,7,.90)');
    ctx.fillStyle = fg;
    ctx.fillRect(0, H - 340, W, 340);
  }

  window.__SHOWCASE_FRAME__ = { draw };
})();
