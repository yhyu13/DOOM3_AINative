/* Runtime: audio-first rotation, silent clock fallback, captions, controls, evidence. */
(() => {
  'use strict';
  const DATA = window.SHOWCASE_DATA;
  const CANVAS = window.__SHOWCASE_CANVAS__ || {
    stages: [],
    init() {}, drawActive() {}, drawAllStatic() {}, setStageActive() {}, setParallax() {}
  };
  if (!DATA) return;

  const $ = (sel, root = document) => root.querySelector(sel);
  const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));

  const stage = $('#stage');
  const posters = $$('.poster');
  const poster1 = $('.p1');
  const browserView = $('#browserView');
  const caption = $('#caption');
  const ccZh = $('#ccZh');
  const ccEn = $('#ccEn');
  const ccCount = $('#ccCount');
  const segs = $$('.seg');
  const posterNo = $('#posterNo');
  const posterName = $('#posterName');
  const timecode = $('#timecode');
  const playBtn = $('#playBtn');
  const muteBtn = $('#muteBtn');
  const ccBtn = $('#ccBtn');
  const soundGate = $('#soundGate');
  const toastEl = $('#toast');
  const totalSec = $('#totalSec');
  const mediaBadge = $('#mediaBadge');
  const heroVideo = $('#heroVideo');
  const heroImage = $('#heroImage');
  const p2 = $('.p2');
  const p2Old = $('#p2Old');
  const p2SplitLine = $('#p2SplitLine');
  const p2Handle = $('#p2Handle');
  const p2Reveal = $('#p2Reveal');
  const stageDetail = $('#stageDetail');
  const generateBtn = $('#generateBtn');
  const promptText = $('#promptText');
  const promptOut = $('#promptOut');
  const rmCells = $('#rmCells');
  const rmDetail = $('#rmDetail');

  const params = new URLSearchParams(location.search);
  const snap = params.get('snap') === '1';
  const reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  const initialPage = Math.min(3, Math.max(1, parseInt(params.get('page') || '1', 10) || 1));

  const META = DATA.pages.map(p => ({ name: p.name || String(p.id || '').toUpperCase() }));
  const STAGES = CANVAS.stages || [];
  const DEFAULT_STAGE = 'HOVER OR FOCUS A STAGE TO SEE THE EVIDENCE IT MUST PRODUCE.';
  const DEFAULT_RM = '<b>M0 · NOW</b> — the repository proves its own baseline before it claims a port.';

  const state = {
    page: initialPage,
    playing: !snap && params.get('autoplay') !== '0',
    mode: snap ? 'snap' : 'boot',
    muted: params.get('mute') === '1',
    captions: params.get('cc') !== '0',
    pausedAt: 0,
    pageT0: performance.now(),
    captionKey: -1,
    captionMarks: [],
    timer: 0,
    advanceScheduled: false,
    pinnedStage: 0,
    snap,
    reduceMotion,
    soundOn: false
  };

  let scale = 1;
  let toastTimer = 0;

  const audios = DATA.pages.map((p, i) => {
    const a = document.createElement('audio');
    a.preload = 'auto';
    a.src = p.audio;
    a.muted = state.muted;
    a.dataset.page = String(i + 1);
    a.setAttribute('playsinline', '');
    $('#audioBank').appendChild(a);
    a.addEventListener('ended', () => {
      if (state.page === i + 1 && state.playing) scheduleAdvance(950);
    });
    return a;
  });

  const audioFor = page => audios[page - 1];

  function fit() {
    scale = Math.min(window.innerWidth / 1920, window.innerHeight / 1080);
    stage.style.transform = `translate(-50%,-50%) scale(${scale})`;
  }

  function pad(n) { return String(n).padStart(2, '0'); }
  function fmt(sec) {
    const s = Math.max(0, Math.round(sec || 0));
    return `${pad(Math.floor(s / 60))}:${pad(s % 60)}`;
  }
  const esc = s => String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

  function toast(msg, ms = 1700) {
    toastEl.textContent = msg;
    toastEl.classList.add('is-on');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toastEl.classList.remove('is-on'), ms);
  }

  function pageTime() {
    if (snap) return DATA.pages[state.page - 1].duration * 0.42;
    if (!state.playing) return state.pausedAt;
    if (state.mode === 'audio') {
      const a = audioFor(state.page);
      if (a && Number.isFinite(a.currentTime)) return a.currentTime;
    }
    return Math.max(0, (performance.now() - state.pageT0) / 1000);
  }

  function updatePlayBtn() {
    playBtn.textContent = state.playing ? 'PAUSE' : 'PLAY';
    stage.dataset.state = state.playing ? 'playing' : 'paused';
  }

  function updateMuteBtn() {
    muteBtn.textContent = state.muted ? 'MUTED' : 'SOUND';
    muteBtn.classList.toggle('is-on', !state.muted);
  }

  function updateCapsBtn() {
    ccBtn.classList.toggle('is-on', state.captions);
  }

  function hideGate() { soundGate.hidden = true; }

  function cancelAdvance() {
    clearTimeout(state.timer);
    state.advanceScheduled = false;
  }

  function scheduleAdvance(ms) {
    cancelAdvance();
    state.advanceScheduled = true;
    state.timer = setTimeout(() => {
      state.advanceScheduled = false;
      setPage(state.page + 1, { force: true });
    }, ms);
  }

  function renderChunk(idx) {
    const p = DATA.pages[state.page - 1];
    const c = p.chunks[idx];
    if (!c) return;
    const chars = Array.from(c.zh || '');
    const marks = c.marks || [];
    const markAt = new Array(chars.length).fill(-1);
    for (let m = 0; m < marks.length; m++) {
      const s = Math.max(0, marks[m].s | 0);
      const e = Math.min(chars.length, marks[m].e | 0);
      for (let i = s; i < e; i++) markAt[i] = m;
    }
    let html = '';
    for (let i = 0; i < chars.length; i++) {
      const ch = esc(chars[i]);
      if (markAt[i] >= 0) html += `<span class="w" data-m="${markAt[i]}">${ch}</span>`;
      else html += ch;
    }
    ccZh.innerHTML = html;
    ccEn.textContent = c.en || '';
    ccCount.textContent = `${pad(idx + 1)} / ${pad(p.chunks.length)}`;
    state.captionKey = idx;
    state.captionMarks = marks;
  }

  function updateCaption(t) {
    const p = DATA.pages[state.page - 1];
    if (!p || !p.chunks || !p.chunks.length) return;
    let idx = 0;
    for (let i = 0; i < p.chunks.length; i++) {
      if (t >= p.chunks[i].start) idx = i;
    }
    if (idx !== state.captionKey) renderChunk(idx);
    const marks = state.captionMarks || [];
    const spans = ccZh.querySelectorAll('.w');
    for (const el of spans) {
      const m = marks[+el.dataset.m];
      if (!m) continue;
      const on = t >= m.t && t < m.t + m.d;
      el.classList.toggle('on', on);
      el.classList.toggle('done', t >= m.t + m.d);
    }
  }

  function updateProgress(t) {
    const dur = DATA.pages[state.page - 1].duration || 1;
    const pct = Math.max(0, Math.min(1, t / dur));
    segs.forEach((seg, i) => {
      const n = i + 1;
      const bar = seg.querySelector('i');
      const done = n < state.page;
      seg.classList.toggle('is-done', done);
      if (bar) bar.style.width = done ? '100%' : (n === state.page ? (pct * 100).toFixed(2) + '%' : '0%');
    });
    timecode.textContent = `${fmt(t)} / ${fmt(dur)}`;
  }

  function updatePageChrome() {
    const n = state.page;
    posterNo.textContent = pad(n);
    posterName.textContent = META[n - 1].name;
    stage.dataset.page = String(n);
    document.body.dataset.page = String(n);
    posters.forEach(p => p.classList.toggle('is-active', +p.dataset.poster === n));
    playBtn.textContent = state.playing ? 'PAUSE' : 'PLAY';
  }

  function setPage(n, opts = {}) {
    const next = ((Math.round(n) - 1) % 3 + 3) % 3 + 1;
    if (next === state.page && !opts.force) return;
    cancelAdvance();
    const old = audioFor(state.page);
    if (old) old.pause();
    state.page = next;
    if (next === 2) {
      lastInteract = performance.now();
      splitTarget = splitValue; splitV = 0;
      setAuto(false); autoNext = 0;
    } else if (autoActive) {
      setAuto(false); splitTarget = splitValue; splitV = 0;
    }
    state.pausedAt = 0;
    state.pageT0 = performance.now();
    state.captionKey = -1;
    state.captionMarks = [];
    const a = audioFor(next);
    if (a) { try { a.currentTime = 0; } catch (e) { /* metadata not ready */ } }
    updatePageChrome();
    updateProgress(0);
    if (snap) {
      CANVAS.drawActive(next, 0, true);
      updateCaption(DATA.pages[next - 1].duration * 0.42);
      updateProgress(DATA.pages[next - 1].duration * 0.42);
      const first = ccZh.querySelector('.w');
      if (first) first.classList.add('on');
      return;
    }
    CANVAS.drawActive(next, performance.now() / 1000, state.reduceMotion);
    updateCaption(0);
    if (state.playing) startAudioForCurrent();
  }

  function startAudioForCurrent() {
    const a = audioFor(state.page);
    if (!a) return;
    a.muted = state.muted;
    if (state.mode === 'silent') {
      state.pageT0 = performance.now() - state.pausedAt * 1000;
      return;
    }
    if (state.pausedAt > 0) {
      try { a.currentTime = Math.min(state.pausedAt, Math.max(0, (a.duration || DATA.pages[state.page - 1].duration) - .05)); } catch (e) { /* ignore */ }
    }
    let p;
    try { p = a.play(); } catch (e) { enterSilent(); return; }
    if (p && p.then) {
      p.then(() => {
        state.mode = 'audio';
        state.soundOn = true;
        stage.dataset.mode = 'audio';
        hideGate();
      }).catch(() => {
        if (state.playing) enterSilent();
      });
    }
  }

  function enterSilent() {
    if (state.mode === 'silent') return;
    state.mode = 'silent';
    state.soundOn = false;
    stage.dataset.mode = 'silent';
    state.pageT0 = performance.now() - state.pausedAt * 1000;
    if (!snap) soundGate.hidden = false;
    toast('AUTOPLAY BLOCKED · SILENT CLOCK RUNNING', 2400);
  }

  function enableSound() {
    if (state.mode === 'audio') return;
    if (snap) return;
    const a = audioFor(state.page);
    if (!a) return;
    const t = pageTime();
    state.pausedAt = t;
    state.playing = true;
    updatePlayBtn();
    a.muted = state.muted;
    try {
      a.currentTime = Math.min(t, Math.max(0, (a.duration || DATA.pages[state.page - 1].duration) - .05));
    } catch (e) { /* ignore */ }
    a.play().then(() => {
      state.mode = 'audio';
      state.soundOn = true;
      stage.dataset.mode = 'audio';
      hideGate();
      toast(state.muted ? 'PLAYING MUTED' : 'SOUND ON');
    }).catch(() => {
      enterSilent();
      toast('BROWSER STILL BLOCKING SOUND', 2200);
    });
  }

  function pause() {
    if (snap) return;
    state.pausedAt = pageTime();
    state.playing = false;
    const a = audioFor(state.page);
    if (a) a.pause();
    cancelAdvance();
    updatePlayBtn();
    toast('PAUSED');
  }

  function play() {
    if (snap) return;
    state.playing = true;
    updatePlayBtn();
    cancelAdvance();
    if (state.mode === 'silent') {
      state.pageT0 = performance.now() - state.pausedAt * 1000;
      toast('PLAYING · SILENT CLOCK');
      return;
    }
    const a = audioFor(state.page);
    if (!a) return;
    a.muted = state.muted;
    try { a.currentTime = Math.min(state.pausedAt, Math.max(0, (a.duration || DATA.pages[state.page - 1].duration) - .05)); } catch (e) { /* ignore */ }
    a.play().then(() => {
      state.mode = 'audio';
      stage.dataset.mode = 'audio';
      hideGate();
    }).catch(() => enterSilent());
  }

  function toggle() { state.playing ? pause() : play(); }

  function advance(dir) {
    const was = state.playing;
    state.pausedAt = 0;
    setPage(state.page + dir, { force: true });
    if (!was) { state.playing = false; updatePlayBtn(); }
  }

  function setMuted(next) {
    state.muted = typeof next === 'boolean' ? next : !state.muted;
    audios.forEach(a => { a.muted = state.muted; });
    updateMuteBtn();
    if (!state.muted && state.mode === 'silent') enableSound();
    toast(state.muted ? 'MUTED — CAPTIONS CONTINUE' : 'SOUND ON');
  }

  function setCaptions(next) {
    state.captions = typeof next === 'boolean' ? next : !state.captions;
    caption.classList.toggle('is-hidden', !state.captions);
    caption.setAttribute('aria-hidden', state.captions ? 'false' : 'true');
    updateCapsBtn();
    toast(state.captions ? 'CAPTIONS ON' : 'CAPTIONS OFF');
  }

  function fullscreen() {
    if (!document.fullscreenElement) {
      const p = document.documentElement.requestFullscreen && document.documentElement.requestFullscreen();
      if (p && p.catch) p.catch(() => {});
    } else if (document.exitFullscreen) {
      document.exitFullscreen().catch(() => {});
    }
  }

  function showStage(n) {
    if (!stageDetail) return;
    if (!n) {
      stageDetail.innerHTML = DEFAULT_STAGE;
      CANVAS.setStageActive(0);
      return;
    }
    const s = STAGES.find(x => x.n === n);
    if (!s) return;
    stageDetail.innerHTML = `<b>0${s.n} · ${s.label}</b> — ${esc(s.detail)}`;
    CANVAS.setStageActive(n);
  }

  function bindEvidence() {
    STAGES.forEach(s => {
      const el = $(`[data-stage="${s.n}"]`);
      if (!el) return;
      el.addEventListener('mouseenter', () => showStage(s.n));
      el.addEventListener('focus', () => showStage(s.n));
      el.addEventListener('mouseleave', () => showStage(state.pinnedStage));
      el.addEventListener('blur', () => showStage(state.pinnedStage));
      el.addEventListener('click', () => {
        state.pinnedStage = state.pinnedStage === s.n ? 0 : s.n;
        showStage(state.pinnedStage);
      });
    });
    if (rmCells) {
      const showRm = el => {
        if (!el || !rmDetail) return;
        rmDetail.innerHTML = `<b>${esc(el.dataset.m || '')}</b> — ${esc(el.dataset.detail || '')}`;
      };
      rmCells.addEventListener('mouseover', e => { const c = e.target.closest('.rm-cell'); if (c) showRm(c); });
      rmCells.addEventListener('mouseleave', () => { if (rmDetail) rmDetail.innerHTML = DEFAULT_RM; });
      rmCells.addEventListener('focusin', e => { const c = e.target.closest('.rm-cell'); if (c) showRm(c); });
      rmCells.addEventListener('focusout', () => { if (rmDetail) rmDetail.innerHTML = DEFAULT_RM; });
    }
  }

  function loop() {
    requestAnimationFrame(loop);
    if (snap) return;
    const now = performance.now();
    const dt = Math.min(.05, Math.max(0, (now - lastFrame) / 1000));
    lastFrame = now;
    const t = pageTime();
    updateCaption(t);
    updateProgress(t);
    if (!state.reduceMotion) CANVAS.drawActive(state.page, performance.now() / 1000, false);
    if (state.page === 2 && !state.reduceMotion) {
      if (state.playing && !dragging && now - lastInteract > 2600) {
        if (!autoActive) {
          setAuto(true);
          autoTarget = splitValue > 50 ? 40 : 72;
          splitTarget = autoTarget;
          autoNext = now + 4200;
          revealed = autoTarget < 50;
          if (p2Reveal) p2Reveal.textContent = revealed ? 'BACK TO 2004' : 'REVEAL 2026';
        } else if (now >= autoNext) {
          autoTarget = autoTarget > 50 ? 40 : 72;
          splitTarget = autoTarget;
          autoNext = now + 4200;
          revealed = autoTarget < 50;
          if (p2Reveal) p2Reveal.textContent = revealed ? 'BACK TO 2004' : 'REVEAL 2026';
        }
      } else if (autoActive) {
        setAuto(false);
        splitTarget = splitValue;
      }
      springStep(dt);
    } else if (autoActive) {
      setAuto(false);
    }
    if (state.playing && state.mode === 'silent' && !state.advanceScheduled) {
      const dur = DATA.pages[state.page - 1].duration;
      if (t >= dur) scheduleAdvance(900);
    }
  }

  function ready() {
    requestAnimationFrame(() => requestAnimationFrame(() => {
      const done = () => {
        window.__SHOWCASE_READY__ = true;
        document.documentElement.dataset.ready = '1';
      };
      if (document.fonts && document.fonts.ready) document.fonts.ready.then(done).catch(done);
      else done();
    }));
  }

  function boot() {
    CANVAS.init();
    fit();
    window.addEventListener('resize', fit);
    const total = DATA.pages.reduce((s, p) => s + (p.duration || 0), 0);
    totalSec.textContent = `${Math.round(total)} SEC`;
    updateMuteBtn();
    updateCapsBtn();
    caption.classList.toggle('is-hidden', !state.captions);
    caption.setAttribute('aria-hidden', state.captions ? 'false' : 'true');

    if (DATA.hero) {
      const h = DATA.hero;
      const credit = h.credit || 'LOCAL / NOT IN GIT';
      if (browserView) browserView.dataset.hero = '1';
      if (h.type === 'image' && heroImage) {
        heroImage.src = h.src;
        heroImage.hidden = false;
        heroImage.classList.add('is-on');
      } else if (heroVideo) {
        heroVideo.src = h.src;
        heroVideo.classList.add('is-on');
        heroVideo.load();
        heroVideo.play().catch(() => {});
      }
      if (mediaBadge) {
        const label = h.label || ('LICENSED ' + String(h.type || 'media').toUpperCase());
        mediaBadge.innerHTML = `<b>${esc(label)}</b>${esc(credit)}`;
      }
    }

    if (p2Old && DATA.archival && DATA.archival.src) p2Old.src = DATA.archival.src;
    else if (p2Old) p2Old.style.display = 'none';
    setSplit(46);

    if (snap) {
      stage.classList.add('snap');
      state.playing = false;
      state.mode = 'snap';
      stage.dataset.mode = 'snap';
      setPage(initialPage, { force: true });
      CANVAS.drawAllStatic();
      updatePlayBtn();
      bindEvidence();
      ready();
      return;
    }

    state.playing = false;
    setPage(initialPage, { force: true });
    state.playing = params.get('autoplay') !== '0';
    updatePlayBtn();
    if (state.playing) {
      const a = audioFor(state.page);
      a.muted = state.muted;
      a.play().then(() => {
        state.mode = 'audio';
        stage.dataset.mode = 'audio';
        hideGate();
      }).catch(() => enterSilent());
    } else {
      stage.dataset.state = 'paused';
    }

    bindEvidence();
    loop();
    ready();
  }

  /* ---------------- AI Rogue concept demo ---------------- */
  const RUNS = [
    { prompt: 'a flooded Mars base with one shotgun', seed: 'SEED 7F3A', map: 'MAP / FLOODED BASE', mod: 'MOD / ONE SHOTGUN' },
    { prompt: 'a midnight UAC cafeteria with no lights', seed: 'SEED 2C91', map: 'MAP / DARK CAFETERIA', mod: 'MOD / FLASHLIGHT ONLY' },
    { prompt: 'a hell gate with thirty imps and one door', seed: 'SEED B4E0', map: 'MAP / HELL GATE', mod: 'MOD / ONE DOOR' },
    { prompt: 'a silent medlab where the lights follow you', seed: 'SEED 08D2', map: 'MAP / MEDLAB', mod: 'MOD / FOLLOWING LIGHTS' }
  ];
  let runIndex = 0;
  function renderRun(i) {
    const run = RUNS[i % RUNS.length];
    if (promptText) promptText.textContent = run.prompt;
    if (promptOut) promptOut.innerHTML = `<span>${esc(run.seed)}</span><span>${esc(run.map)}</span><span>${esc(run.mod)}</span>`;
  }
  if (generateBtn) {
    generateBtn.addEventListener('click', () => {
      if (generateBtn.disabled) return;
      generateBtn.disabled = true;
      generateBtn.textContent = 'GENERATING…';
      toast('CONCEPT DEMO · BUILDING A RUN');
      setTimeout(() => {
        runIndex = (runIndex + 1) % RUNS.length;
        renderRun(runIndex);
        generateBtn.disabled = false;
        generateBtn.textContent = 'GENERATE RUN';
        toast('CONCEPT DEMO · RUN GENERATED');
      }, 680);
    });
  }

  /* ---------------- 2004 / 2026 comparison ---------------- */
  let splitValue = 46;
  let splitTarget = 46;
  let splitV = 0;
  let dragging = false;
  let revealed = false;
  let lastInteract = performance.now();
  let autoActive = false;
  let autoTarget = 72;
  let autoNext = 0;
  let lastFrame = performance.now();

  function setSplit(v) {
    const next = Math.max(18, Math.min(82, Number(v) || 46));
    splitValue = next;
    if (p2) p2.style.setProperty('--split', next);
    if (p2SplitLine) {
      const top = 1920 * next / 100;
      p2SplitLine.setAttribute('x1', String(top));
      p2SplitLine.setAttribute('x2', String(top - 260));
    }
    if (p2Handle) p2Handle.setAttribute('aria-valuenow', String(Math.round(next)));
    if (p2Reveal) {
      const nextRevealed = next < 50;
      if (nextRevealed !== revealed) {
        revealed = nextRevealed;
        p2Reveal.textContent = revealed ? 'BACK TO 2004' : 'REVEAL 2026';
      }
    }
  }
  function setAuto(on) {
    autoActive = on;
    if (p2) p2.classList.toggle('is-auto', on);
    const label = p2Handle && p2Handle.querySelector('span');
    if (label) label.textContent = on ? 'AUTO' : 'DRAG';
  }
  function markInteraction() {
    lastInteract = performance.now();
    splitTarget = splitValue;
    splitV = 0;
    if (autoActive) setAuto(false);
  }
  function springStep(dt) {
    if (dragging || state.snap) return;
    const diff = splitTarget - splitValue;
    splitV += diff * 7.5 * dt;
    splitV *= Math.max(0, 1 - 4.8 * dt);
    if (Math.abs(diff) < .10 && Math.abs(splitV) < .06) {
      splitV = 0;
      if (splitValue !== splitTarget) setSplit(splitTarget);
    } else {
      setSplit(splitValue + splitV * dt);
    }
  }
  if (p2Handle) {
    p2Handle.addEventListener('pointerdown', e => {
      dragging = true;
      markInteraction();
      setAuto(false);
      try { p2Handle.focus({ preventScroll: true }); } catch (_) {}
      if (p2) p2.classList.add('is-dragging');
      try { p2Handle.setPointerCapture(e.pointerId); } catch (_) {}
      e.stopPropagation();
      e.preventDefault();
    });
    p2Handle.addEventListener('pointermove', e => {
      if (!dragging) return;
      const r = stage.getBoundingClientRect();
      if (!r.width || !r.height) return;
      const scale = r.width / 1920;
      const lx = (e.clientX - r.left) / scale;
      const yFrac = (e.clientY - r.top) / r.height;
      setSplit((lx + 260 * yFrac) / 1920 * 100);
      splitTarget = splitValue;
      splitV = 0;
      lastInteract = performance.now();
      e.preventDefault();
    });
    p2Handle.addEventListener('pointerup', e => {
      dragging = false;
      splitTarget = splitValue;
      splitV = 0;
      lastInteract = performance.now();
      if (p2) p2.classList.remove('is-dragging');
      try { p2Handle.releasePointerCapture(e.pointerId); } catch (_) {}
    });
    p2Handle.addEventListener('pointercancel', () => {
      dragging = false;
      splitTarget = splitValue;
      splitV = 0;
      lastInteract = performance.now();
      if (p2) p2.classList.remove('is-dragging');
    });
    p2Handle.addEventListener('keydown', e => {
      if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') {
        markInteraction(); splitTarget = Math.max(18, splitTarget - 4);
        if (state.snap || state.reduceMotion) setSplit(splitTarget);
        e.preventDefault(); e.stopPropagation();
      } else if (e.key === 'ArrowRight' || e.key === 'ArrowUp') {
        markInteraction(); splitTarget = Math.min(82, splitTarget + 4);
        if (state.snap || state.reduceMotion) setSplit(splitTarget);
        e.preventDefault(); e.stopPropagation();
      }
    });
  }
  if (p2Reveal) {
    p2Reveal.addEventListener('click', () => {
      markInteraction();
      revealed = !revealed;
      splitTarget = revealed ? 32 : 80;
      if (state.snap || state.reduceMotion) setSplit(splitTarget);
      p2Reveal.textContent = revealed ? 'BACK TO 2004' : 'REVEAL 2026';
    });
  }

  /* ---------------- controls ---------------- */
  $$('[data-act]').forEach(btn => {
    btn.addEventListener('click', () => {
      const act = btn.dataset.act;
      if (act === 'prev') advance(-1);
      else if (act === 'next') advance(1);
      else if (act === 'toggle') toggle();
      else if (act === 'mute') setMuted();
      else if (act === 'cc') setCaptions();
      else if (act === 'full') fullscreen();
    });
  });

  soundGate.addEventListener('click', enableSound);

  let hiddenWasPlaying = false;
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
      hiddenWasPlaying = state.playing;
      if (state.playing) pause();
    } else if (hiddenWasPlaying) {
      hiddenWasPlaying = false;
      play();
    }
  });

  window.addEventListener('keydown', e => {
    if (e.target && /^(INPUT|TEXTAREA|SELECT)$/.test(e.target.tagName)) return;
    if (e.target === p2Handle) return;
    if ((e.code === 'Space' || e.key === ' ') && e.target && e.target.closest && e.target.closest('button,a,[role="button"]')) return;
    if (e.key === 'ArrowRight') { e.preventDefault(); advance(1); }
    else if (e.key === 'ArrowLeft') { e.preventDefault(); advance(-1); }
    else if (e.code === 'Space' || e.key === ' ') { e.preventDefault(); toggle(); }
    else if (e.key === 'm' || e.key === 'M') setMuted();
    else if (e.key === 'c' || e.key === 'C') setCaptions();
    else if (e.key === 'f' || e.key === 'F') fullscreen();
  });

  let sw = null;
  stage.addEventListener('pointerdown', e => {
    if (e.target.closest('button,.claim,.stage-node,.rm-cell,.caption,.bottombar,.p2-handle,.p2-reveal,.p2-compare-actions,.p2-divider')) return;
    if (state.page === 2) markInteraction();
    sw = { x: e.clientX, y: e.clientY, t: performance.now() };
    if (state.mode === 'silent') enableSound();
  });
  stage.addEventListener('pointerup', e => {
    if (!sw) return;
    const dx = e.clientX - sw.x, dy = e.clientY - sw.y;
    const dt = performance.now() - sw.t;
    sw = null;
    if (dt < 900 && Math.abs(dx) > 80 && Math.abs(dx) > Math.abs(dy) * 1.4) advance(dx < 0 ? 1 : -1);
  });
  stage.addEventListener('pointercancel', () => { sw = null; });

  stage.addEventListener('pointermove', e => {
    const r = stage.getBoundingClientRect();
    if (!r.width || !r.height) return;
    const x = ((e.clientX - r.left) / r.width) * 2 - 1;
    const y = ((e.clientY - r.top) / r.height) * 2 - 1;
    CANVAS.setParallax(x, y);
    if (state.page === 2) markInteraction();
  });

  window.__SHOWCASE__ = {
    data: DATA,
    go: n => setPage(n, { force: true }),
    next: () => advance(1),
    prev: () => advance(-1),
    play,
    pause,
    toggle,
    enableSound,
    state: () => ({
      page: state.page,
      playing: state.playing,
      mode: state.mode,
      muted: state.muted,
      captions: state.captions,
      snap: state.snap,
      split: splitValue,
      prompt: promptText ? promptText.textContent : null,
      time: pageTime(),
      duration: DATA.pages[state.page - 1].duration,
      ready: !!window.__SHOWCASE_READY__
    })
  };

  boot();
})();
