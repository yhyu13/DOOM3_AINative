/* 2026 vision render: a cinematic Mars hangar, demon silhouette, modern light. */
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

  function mulberry32(a) {
    return function () {
      a |= 0; a = a + 0x6D2B79F5 | 0;
      let t = Math.imul(a ^ a >>> 15, 1 | a);
      t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
      return ((t ^ t >>> 14) >>> 0) / 4294967296;
    };
  }
  const rnd = mulberry32(7734);
  const dust = Array.from({ length: 110 }, () => ({
    x: rnd() * W, y: rnd() * H, s: .5 + rnd() * 2.2, ph: rnd() * 6.28, sp: 3 + rnd() * 18
  }));
  const stars = Array.from({ length: 44 }, () => ({
    x: 760 + rnd() * 1080, y: 130 + rnd() * 380, s: .4 + rnd() * 1.3, a: .15 + rnd() * .35
  }));

  const WIN = { l: 660, r: 1800, t: 112, b: 780, tl: 760, tr: 1680 };

  function poly(pts, fill) {
    ctx.fillStyle = fill;
    ctx.beginPath();
    ctx.moveTo(pts[0][0], pts[0][1]);
    for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
    ctx.closePath(); ctx.fill();
  }
  function line(x1, y1, x2, y2, color, width) {
    ctx.strokeStyle = color; ctx.lineWidth = width || 1;
    ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke();
  }
  function glow(x, y, r, rgba) {
    const g = ctx.createRadialGradient(x, y, 0, x, y, r);
    g.addColorStop(0, rgba);
    g.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = g;
    ctx.fillRect(x - r, y - r, r * 2, r * 2);
  }

  function drawWindow(time, freeze, sway) {
    // Clip to the trapezoid window and paint the Mars view.
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(WIN.tl, WIN.t); ctx.lineTo(WIN.tr, WIN.t);
    ctx.lineTo(WIN.r, WIN.b); ctx.lineTo(WIN.l, WIN.b);
    ctx.closePath(); ctx.clip();

    const sky = ctx.createLinearGradient(0, WIN.t, 0, WIN.b);
    sky.addColorStop(0, '#160f22');
    sky.addColorStop(.34, '#46202a');
    sky.addColorStop(.62, '#a53a2c');
    sky.addColorStop(.84, '#f07a42');
    sky.addColorStop(1, '#ffc08a');
    ctx.fillStyle = sky;
    ctx.fillRect(WIN.l, WIN.t, WIN.r - WIN.l, WIN.b - WIN.t);

    // faint stars, dust haze
    for (const s of stars) {
      ctx.globalAlpha = s.a * (freeze ? 1 : .7 + .3 * Math.sin(time * 1.2 + s.x));
      ctx.fillStyle = '#ffe8d4';
      ctx.fillRect(s.x, s.y, s.s, s.s);
    }
    ctx.globalAlpha = 1;

    // sun
    const sx = 1490 + sway.x * .3, sy = 300 + sway.y * .2;
    ctx.globalCompositeOperation = 'lighter';
    glow(sx, sy, 350, 'rgba(255,180,110,.30)');
    glow(sx, sy, 120, 'rgba(255,230,190,.45)');
    ctx.globalCompositeOperation = 'source-over';
    ctx.fillStyle = '#ffe3bc';
    ctx.beginPath(); ctx.arc(sx, sy, 44, 0, Math.PI * 2); ctx.fill();

    // Mars mountain layers
    poly([[660, 640], [820, 560], [960, 610], [1110, 540], [1270, 600], [1450, 520], [1640, 590], [1800, 545], [1800, 780], [660, 780]], '#3b1b16');
    poly([[660, 680], [840, 610], [1000, 660], [1160, 590], [1340, 650], [1520, 590], [1700, 650], [1800, 620], [1800, 780], [660, 780]], '#26100e');
    poly([[660, 720], [900, 680], [1140, 720], [1380, 670], [1620, 720], [1800, 690], [1800, 780], [660, 780]], '#130909');

    // dust clouds on the plain
    for (let i = 0; i < 5; i++) {
      const g = ctx.createRadialGradient(900 + i * 190, 690, 10, 900 + i * 190, 690, 220);
      g.addColorStop(0, 'rgba(255,140,90,.11)');
      g.addColorStop(1, 'rgba(255,140,90,0)');
      ctx.fillStyle = g;
      ctx.fillRect(650, 500, 1180, 320);
    }

    // window frame mullions
    ctx.globalCompositeOperation = 'source-over';
    ctx.fillStyle = 'rgba(5,5,6,.94)';
    for (const x of [840, 1040, 1240, 1440, 1640]) ctx.fillRect(x - 6, WIN.t, 12, WIN.b - WIN.t);
    for (const y of [300, 530]) ctx.fillRect(WIN.l, y - 6, WIN.r - WIN.l, 12);
    ctx.fillStyle = 'rgba(255,160,100,.13)';
    for (const x of [840, 1040, 1240, 1440, 1640]) ctx.fillRect(x + 6, WIN.t, 2, WIN.b - WIN.t);
    ctx.restore();

    // heavy frame
    ctx.strokeStyle = '#08080a';
    ctx.lineWidth = 32;
    ctx.beginPath();
    ctx.moveTo(WIN.tl, WIN.t); ctx.lineTo(WIN.tr, WIN.t);
    ctx.lineTo(WIN.r, WIN.b); ctx.lineTo(WIN.l, WIN.b);
    ctx.closePath(); ctx.stroke();
    ctx.strokeStyle = 'rgba(255,150,90,.25)';
    ctx.lineWidth = 3;
    ctx.stroke();
  }

  function drawInterior(time, freeze, sway) {
    // ceiling and side walls
    const ceil = ctx.createLinearGradient(0, 0, 0, 220);
    ceil.addColorStop(0, '#050506');
    ceil.addColorStop(1, '#121015');
    ctx.fillStyle = ceil;
    ctx.fillRect(0, 0, W, 220);

    const left = ctx.createLinearGradient(0, 0, 660, 0);
    left.addColorStop(0, '#070708');
    left.addColorStop(.7, '#111015');
    left.addColorStop(1, '#181316');
    ctx.fillStyle = left;
    ctx.fillRect(0, 0, 660, 820);
    const right = ctx.createLinearGradient(1800, 0, 1920, 0);
    right.addColorStop(0, '#1a1416');
    right.addColorStop(1, '#050506');
    ctx.fillStyle = right;
    ctx.fillRect(1800, 0, 120, 820);

    // wall panels / pipes
    for (let i = 0; i < 7; i++) {
      const y = 90 + i * 94;
      line(30, y, 640, y + 8, 'rgba(244,239,230,.055)', 1);
    }
    for (let i = 0; i < 4; i++) {
      const y = 180 + i * 150;
      line(80, y, 620, y + 20, 'rgba(255,150,90,.09)', 3);
    }
    // cyan edge light on the left, ember on the right
    line(78, 210, 78, 760, 'rgba(92,205,255,.34)', 3);
    line(1840, 190, 1840, 770, 'rgba(255,96,36,.40)', 3);
    glow(80, 440, 160, 'rgba(92,205,255,.075)');

    // floor
    const floor = ctx.createLinearGradient(0, 760, 0, H);
    floor.addColorStop(0, '#171014');
    floor.addColorStop(1, '#050506');
    ctx.fillStyle = floor;
    ctx.fillRect(0, 760, W, H - 760);
    for (let i = 0; i < 10; i++) {
      const x = 120 + i * 190;
      line(x, 1080, 960 + (i - 5) * 70, 780, 'rgba(255,150,100,.035)', 1);
    }
    for (const y of [840, 920, 1010]) line(0, y, W, y, 'rgba(244,239,230,.04)', 1);

    // floor reflections from the window
    ctx.globalCompositeOperation = 'lighter';
    for (let i = 0; i < 5; i++) {
      const x = 820 + i * 210;
      const g = ctx.createLinearGradient(0, 780, 0, 1080);
      g.addColorStop(0, 'rgba(255,140,70,.12)');
      g.addColorStop(1, 'rgba(255,90,40,0)');
      ctx.fillStyle = g;
      ctx.fillRect(x, 780, 40, 300);
    }
    ctx.globalCompositeOperation = 'source-over';

    // catwalk railing
    for (let i = 0; i < 12; i++) {
      const x = 640 + i * 100;
      line(x, 716, x, 790, 'rgba(10,10,12,.95)', 6);
      line(x + 3, 718, x + 3, 788, 'rgba(255,170,110,.12)', 1);
    }
    line(620, 716, 1840, 716, 'rgba(10,10,12,.98)', 8);
    line(620, 720, 1840, 720, 'rgba(255,170,110,.18)', 1);
    line(620, 786, 1840, 786, 'rgba(10,10,12,.98)', 8);
  }

  function drawDemon(time, freeze) {
    const cx = 1230, base = 726;
    const breathe = freeze ? 0 : Math.sin(time * 1.1) * 3;
    ctx.save();
    ctx.translate(cx, base + breathe);

    // torso and waist
    poly([[-95, 0], [-170, -90], [-195, -210], [-120, -270], [120, -270], [195, -210], [170, -90], [95, 0]], '#050506');
    // chest plates
    poly([[-95, -250], [-140, -150], [-70, -90], [0, -120], [70, -90], [140, -150], [95, -250], [0, -280]], '#0b0b0d');
    poly([[-60, -230], [-90, -160], [-30, -130], [0, -160], [30, -130], [90, -160], [60, -230], [0, -250]], '#111013');
    // ember core
    ctx.globalCompositeOperation = 'lighter';
    glow(0, -170, 64, 'rgba(255,70,20,.24)');
    ctx.globalCompositeOperation = 'source-over';
    poly([[-16, -170], [0, -190], [16, -170], [0, -150]], 'rgba(255,92,32,.55)');

    // shoulder pauldrons
    poly([[-120, -270], [-240, -230], [-260, -140], [-180, -120], [-130, -190]], '#050506');
    poly([[120, -270], [240, -230], [260, -140], [180, -120], [130, -190]], '#050506');
    for (const side of [-1, 1]) {
      poly([[side * 150, -260], [side * 205, -330], [side * 220, -270]], '#050506');
      poly([[side * 190, -225], [side * 252, -282], [side * 250, -220]], '#050506');
    }

    // long arms and claws
    poly([[-230, -190], [-295, -60], [-255, 34], [-190, -16], [-180, -120]], '#050506');
    poly([[230, -190], [295, -60], [255, 34], [190, -16], [180, -120]], '#050506');
    ctx.strokeStyle = 'rgba(255,120,60,.35)';
    ctx.lineWidth = 2;
    for (const side of [-1, 1]) {
      for (let i = 0; i < 3; i++) {
        ctx.beginPath();
        ctx.moveTo(side * (235 + i * 10), 10 + i * 4);
        ctx.lineTo(side * (285 + i * 18), 52 + i * 12);
        ctx.stroke();
      }
    }

    // neck and skull
    poly([[-46, -262], [-56, -322], [-22, -352], [22, -352], [56, -322], [46, -262]], '#050506');
    poly([[-58, -304], [-46, -246], [0, -228], [46, -246], [58, -304], [0, -320]], '#050506');
    // jaw teeth
    for (let i = -2; i <= 2; i++) {
      poly([[i * 13 - 5, -240], [i * 13, -254], [i * 13 + 5, -240]], 'rgba(244,239,230,.55)');
    }
    // short curved horns
    poly([[-40, -344], [-98, -392], [-118, -362], [-62, -320]], '#050506');
    poly([[40, -344], [98, -392], [118, -362], [62, -320]], '#050506');

    // eyes + mouth glow
    ctx.globalCompositeOperation = 'lighter';
    const pulse = freeze ? 1 : .74 + .26 * Math.sin(time * 2.7);
    glow(-23, -298, 30, `rgba(180,255,90,${.55 * pulse})`);
    glow(23, -298, 30, `rgba(180,255,90,${.55 * pulse})`);
    glow(0, -246, 42, 'rgba(255,50,16,.26)');
    ctx.fillStyle = `rgba(210,255,150,${.90 * pulse})`;
    ctx.beginPath(); ctx.arc(-23, -298, 4.2, 0, Math.PI * 2); ctx.fill();
    ctx.beginPath(); ctx.arc(23, -298, 4.2, 0, Math.PI * 2); ctx.fill();

    // right ember / left cyan rim
    ctx.strokeStyle = 'rgba(255,110,48,.55)';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(120, -270); ctx.lineTo(195, -210); ctx.lineTo(170, -90); ctx.lineTo(95, 0);
    ctx.moveTo(120, -270); ctx.lineTo(240, -230); ctx.lineTo(260, -140);
    ctx.stroke();
    ctx.strokeStyle = 'rgba(92,205,255,.28)';
    ctx.beginPath();
    ctx.moveTo(-120, -270); ctx.lineTo(-195, -210); ctx.lineTo(-170, -90); ctx.lineTo(-95, 0);
    ctx.moveTo(-120, -270); ctx.lineTo(-240, -230); ctx.lineTo(-260, -140);
    ctx.stroke();
    ctx.restore();
  }

  function drawShafts(time, freeze, sway) {
    ctx.globalCompositeOperation = 'lighter';
    const pulse = freeze ? .85 : .72 + .28 * Math.sin(time * 1.7);
    for (let i = 0; i < 5; i++) {
      const x0 = 760 + i * 210;
      const g = ctx.createLinearGradient(0, 160, 0, 900);
      g.addColorStop(0, `rgba(255,186,120,${.13 * pulse})`);
      g.addColorStop(.55, `rgba(255,130,70,${.055 * pulse})`);
      g.addColorStop(1, 'rgba(255,100,50,0)');
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.moveTo(x0, 160); ctx.lineTo(x0 + 80, 160);
      ctx.lineTo(x0 + 220, 900); ctx.lineTo(x0 - 80, 900);
      ctx.closePath(); ctx.fill();
    }
    // dust in the light
    for (const d of dust) {
      const y = ((d.y - time * d.sp) % (H + 60) + H + 60) % (H + 60) - 30;
      const x = d.x + Math.sin(time * .3 + d.ph) * 18;
      ctx.globalAlpha = .06 + .16 * (.5 + .5 * Math.sin(time * 1.9 + d.ph));
      ctx.fillStyle = '#ffd0a0';
      ctx.fillRect(x, y, d.s, d.s);
    }
    ctx.globalCompositeOperation = 'source-over';
    ctx.globalAlpha = 1;
  }

  function drawHud(time, freeze) {
    // crosshair and a restrained modern HUD
    const cx = 1230, cy = 480;
    ctx.strokeStyle = 'rgba(255,190,130,.55)';
    ctx.lineWidth = 1.2;
    ctx.beginPath();
    ctx.moveTo(cx - 18, cy); ctx.lineTo(cx - 6, cy);
    ctx.moveTo(cx + 6, cy); ctx.lineTo(cx + 18, cy);
    ctx.moveTo(cx, cy - 18); ctx.lineTo(cx, cy - 6);
    ctx.moveTo(cx, cy + 6); ctx.lineTo(cx, cy + 18);
    ctx.stroke();
    ctx.strokeStyle = 'rgba(255,190,130,.16)';
    ctx.beginPath(); ctx.arc(cx, cy, 30, 0, Math.PI * 2); ctx.stroke();

    // micro HUD in the lower left of the view (above the feature cards)
    ctx.font = '9px "Cascadia Mono", Consolas, monospace';
    ctx.fillStyle = 'rgba(244,239,230,.42)';
    ctx.fillText('SUIT 87', 80, 714);
    ctx.fillText('ARMOR 42', 80, 732);
    ctx.fillStyle = 'rgba(255,150,90,.72)';
    ctx.fillText('PBR / HDR / 4K / 120', 80, 752);
  }

  function draw(t, freeze) {
    const time = freeze ? 6.2 : t;
    const sway = {
      x: freeze ? 0 : Math.sin(time * .31) * 4,
      y: freeze ? 0 : Math.cos(time * .43) * 2
    };
    ctx.globalCompositeOperation = 'source-over';
    ctx.globalAlpha = 1;
    ctx.fillStyle = '#050506';
    ctx.fillRect(0, 0, W, H);

    drawWindow(time, freeze, sway);
    drawInterior(time, freeze, sway);
    drawDemon(time, freeze);
    drawShafts(time, freeze, sway);
    drawHud(time, freeze);

    // cinematic grade + vignette
    ctx.globalCompositeOperation = 'source-over';
    const grade = ctx.createLinearGradient(0, 0, 0, H);
    grade.addColorStop(0, 'rgba(20,12,28,.28)');
    grade.addColorStop(.55, 'rgba(255,96,36,.035)');
    grade.addColorStop(1, 'rgba(0,10,18,.35)');
    ctx.fillStyle = grade;
    ctx.fillRect(0, 0, W, H);

    const vig = ctx.createRadialGradient(W * .55, H * .5, 320, W * .55, H * .5, 1100);
    vig.addColorStop(0, 'rgba(0,0,0,0)');
    vig.addColorStop(1, 'rgba(0,0,0,.58)');
    ctx.fillStyle = vig;
    ctx.fillRect(0, 0, W, H);

    // bottom falloff for the feature cards / proof strip
    const bottom = ctx.createLinearGradient(0, H - 430, 0, H);
    bottom.addColorStop(0, 'rgba(5,5,6,0)');
    bottom.addColorStop(1, 'rgba(5,5,6,.94)');
    ctx.fillStyle = bottom;
    ctx.fillRect(0, H - 430, W, 430);
  }

  window.__SHOWCASE_DREAM__ = { draw };
})();
