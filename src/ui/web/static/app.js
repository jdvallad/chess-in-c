(() => {
  "use strict";

  // ==========================================================================
  // Crisp Staunton SVG Chess Pieces (transparent background, retina-sharp)
  // ==========================================================================
  const PIECE_SVGS = {
    K: `<svg viewBox="0 0 45 45" class="piece-svg"><g fill="none" fill-rule="evenodd" stroke="#161b22" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M22.5 11.63V6M20 8h5" stroke-linejoin="miter"/><path d="M22.5 25s4.5-7.5 3-10.5c0 0-1-2.5-3-2.5s-3 2.5-3 2.5c-1.5 3 3 10.5 3 10.5" fill="#f6f8fa" stroke-linecap="butt" stroke-linejoin="miter"/><path d="M11.5 37c5.5 3.5 15.5 3.5 21 0v-7s9-4.5 6-10.5c-4-6.5-13.5-3.5-16 4V27v-3.5c-3.5-7.5-13-10.5-16-4-3 6 5 10 5 10V37z" fill="#f6f8fa"/><path d="M11.5 30c5.5-3 15.5-3 21 0m-21 3.5c5.5-3 15.5-3 21 0m-21 3.5c5.5-3 15.5-3 21 0"/></g></svg>`,
    Q: `<svg viewBox="0 0 45 45" class="piece-svg"><g fill="#f6f8fa" fill-rule="evenodd" stroke="#161b22" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><circle cx="6" cy="12" r="2.75"/><circle cx="14" cy="9" r="2.75"/><circle cx="22.5" cy="8" r="2.75"/><circle cx="31" cy="9" r="2.75"/><circle cx="39" cy="12" r="2.75"/><path d="M9 26c8.5-1.5 21-1.5 27 0l2-12-7 11V11l-5.5 13.5-3-15-3 15-5.5-14V25L7 14l2 12z" stroke-linecap="butt"/><path d="M9 26c0 2 1.5 2 2.5 4 1 1.5 1 1 .5 3.5-1.5 1-1.5 2.5-1.5 2.5-1.5 1.5.5 2.5.5 2.5 6.5 1 16.5 1 23 0 0 0 1.5-1 0-2.5 0 0 .5-1.5-1-2.5-.5-2.5-.5-2 .5-3.5 1-2 2.5-2 2.5-4-8.5-1.5-18.5-1.5-27 0z" stroke-linecap="butt"/><path d="M11.5 30c3.5-1 18.5-1 22 0M12 33.5c6-1 15-1 21 0" fill="none"/></g></svg>`,
    R: `<svg viewBox="0 0 45 45" class="piece-svg"><g fill="#f6f8fa" fill-rule="evenodd" stroke="#161b22" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M9 39h27v-3H9v3zM12 36v-4h21v4H12zM11 14V9h4v2h5V9h5v2h5V9h4v5" stroke-linecap="butt"/><path d="M34 14l-3 3H14l-3-3"/><path d="M31 17v12.5H14V17" stroke-linecap="butt" stroke-linejoin="miter"/><path d="M31 29.5l1.5 2.5h-20l1.5-2.5"/><path d="M11 14h23" fill="none" stroke-linejoin="miter"/></g></svg>`,
    B: `<svg viewBox="0 0 45 45" class="piece-svg"><g fill="none" fill-rule="evenodd" stroke="#161b22" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><g fill="#f6f8fa" stroke-linecap="butt"><path d="M9 36c3.39-.97 10.11.43 13.5-2 3.39 2.43 10.11 1.03 13.5 2 0 0 1.65.54 3 2-.68.97-1.65.99-3 .5-3.39-.97-10.11.46-13.5-1-3.39 1.46-10.11.03-13.5 1-1.354.49-2.323.47-3-.5 1.354-1.94 3-2 3-2z"/><path d="M15 32c2.5 2.5 12.5 2.5 15 0 .5-1.5 0-2 0-2 0-2.5-2.5-4-2.5-4 5.5-1.5 6-11.5-5-15.5-11 4-10.5 14-5 15.5 0 0-2.5 1.5-2.5 4 0 0-.5.5 0 2z"/><circle cx="22.5" cy="8" r="2.5"/></g><path d="M17.5 26h10M15 30h15m-7.5-14.5v5M20 18h5" stroke-linejoin="miter"/></g></svg>`,
    N: `<svg viewBox="0 0 45 45" class="piece-svg"><g fill="none" fill-rule="evenodd" stroke="#161b22" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M22 10c10.5 1 16.5 8 16 29H15c0-9 10-6.5 8-21" fill="#f6f8fa"/><path d="M24 18c.38 2.91-5.55 7.37-8 9-3 2-2.82 4.34-5 4-1.042-.94 1.41-3.04 0-3-1 0 .19 1.23-1 2-1 0-4.003 1-4-4 0-2 6-12 6-12s1.89-1.9 2-3.5c-.73-.994-.5-2-.5-3 1-1 3 2.5 3 2.5h2s.78-1.992 2.5-3c1 0 1 3 1 3" fill="#f6f8fa"/><circle cx="9.5" cy="25.5" r=".5" fill="#161b22"/><path d="M14.933 15.75a.5 1.5 30 1 1-.866-.5.5 1.5 30 1 1 .866.5z" fill="#161b22"/></g></svg>`,
    P: `<svg viewBox="0 0 45 45" class="piece-svg"><path d="M22.5 9c-2.21 0-4 1.79-4 4 0 .89.29 1.71.78 2.38C17.33 16.5 16 18.59 16 21c0 2.03.94 3.84 2.41 5.03-3 1.06-7.41 5.55-7.41 13.47h23c0-7.92-4.41-12.41-7.41-13.47 1.47-1.19 2.41-3 2.41-5.03 0-2.41-1.33-4.5-3.28-5.62.49-.67.78-1.49.78-2.38 0-2.21-1.79-4-4-4z" fill="#f6f8fa" stroke="#161b22" stroke-width="1.5" stroke-linecap="round"/></svg>`,
    k: `<svg viewBox="0 0 45 45" class="piece-svg"><g fill="none" fill-rule="evenodd" stroke="#0b0e14" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M22.5 11.63V6" stroke-linejoin="miter"/><path d="M22.5 25s4.5-7.5 3-10.5c0 0-1-2.5-3-2.5s-3 2.5-3 2.5c-1.5 3 3 10.5 3 10.5" fill="#1b2129" stroke-linecap="butt" stroke-linejoin="miter"/><path d="M11.5 37c5.5 3.5 15.5 3.5 21 0v-7s9-4.5 6-10.5c-4-6.5-13.5-3.5-16 4V27v-3.5c-3.5-7.5-13-10.5-16-4-3 6 5 10 5 10V37z" fill="#1b2129"/><path d="M20 8h5" stroke-linejoin="miter"/><path d="M32 29.5s8.5-4 6.03-9.65C34.15 14 25 18 22.5 24.5l.01 2.1-.01-2.1C20 18 9.906 14 6.997 19.85c-2.497 5.65 4.853 9 4.853 9" stroke="#cfd8e3"/><path d="M11.5 30c5.5-3 15.5-3 21 0m-21 3.5c5.5-3 15.5-3 21 0m-21 3.5c5.5-3 15.5-3 21 0" stroke="#cfd8e3"/></g></svg>`,
    q: `<svg viewBox="0 0 45 45" class="piece-svg"><g fill="#1b2129" fill-rule="evenodd" stroke="#0b0e14" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><g stroke="none"><circle cx="6" cy="12" r="2.75"/><circle cx="14" cy="9" r="2.75"/><circle cx="22.5" cy="8" r="2.75"/><circle cx="31" cy="9" r="2.75"/><circle cx="39" cy="12" r="2.75"/></g><path d="M9 26c8.5-1.5 21-1.5 27 0l2.5-12.5L31 25l-.3-14.1-5.2 13.6-3-14.5-3 14.5-5.2-13.6L14 25 6.5 13.5 9 26z" stroke-linecap="butt"/><path d="M9 26c0 2 1.5 2 2.5 4 1 1.5 1 1 .5 3.5-1.5 1-1.5 2.5-1.5 2.5-1.5 1.5.5 2.5.5 2.5 6.5 1 16.5 1 23 0 0 0 1.5-1 0-2.5 0 0 .5-1.5-1-2.5-.5-2.5-.5-2 .5-3.5 1-2 2.5-2 2.5-4-8.5-1.5-18.5-1.5-27 0z" stroke-linecap="butt"/><path d="M11 38.5a35 35 1 0 0 23 0" fill="none" stroke-linecap="butt"/><path d="M11 29a35 35 1 0 1 23 0m-21.5 2.5h20m-21 3a35 35 1 0 0 22 0m-23 3a35 35 1 0 0 24 0" fill="none" stroke="#cfd8e3"/></g></svg>`,
    r: `<svg viewBox="0 0 45 45" class="piece-svg"><g fill="#1b2129" fill-rule="evenodd" stroke="#0b0e14" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M9 39h27v-3H9v3zM12.5 32l1.5-2.5h17l1.5 2.5h-20zM12 36v-4h21v4H12z" stroke-linecap="butt"/><path d="M14 29.5v-13h17v13H14z" stroke-linecap="butt" stroke-linejoin="miter"/><path d="M14 16.5L11 14h23l-3 2.5H14zM11 14V9h4v2h5V9h5v2h5V9h4v5H11z" stroke-linecap="butt"/><path d="M12 35.5h21m-20-4h19m-18-2h17m-17-13h17M11 14h23" fill="none" stroke="#cfd8e3" stroke-width="1" stroke-linejoin="miter"/></g></svg>`,
    b: `<svg viewBox="0 0 45 45" class="piece-svg"><g fill="none" fill-rule="evenodd" stroke="#0b0e14" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><g fill="#1b2129" stroke-linecap="butt"><path d="M9 36c3.39-.97 10.11.43 13.5-2 3.39 2.43 10.11 1.03 13.5 2 0 0 1.65.54 3 2-.68.97-1.65.99-3 .5-3.39-.97-10.11.46-13.5-1-3.39 1.46-10.11.03-13.5 1-1.354.49-2.323.47-3-.5 1.354-1.94 3-2 3-2z"/><path d="M15 32c2.5 2.5 12.5 2.5 15 0 .5-1.5 0-2 0-2 0-2.5-2.5-4-2.5-4 5.5-1.5 6-11.5-5-15.5-11 4-10.5 14-5 15.5 0 0-2.5 1.5-2.5 4 0 0-.5.5 0 2z"/><circle cx="22.5" cy="8" r="2.5"/></g><path d="M17.5 26h10M15 30h15m-7.5-14.5v5M20 18h5" stroke="#cfd8e3" stroke-linejoin="miter"/></g></svg>`,
    n: `<svg viewBox="0 0 45 45" class="piece-svg"><g fill="none" fill-rule="evenodd" stroke="#0b0e14" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M22 10c10.5 1 16.5 8 16 29H15c0-9 10-6.5 8-21" fill="#1b2129"/><path d="M24 18c.38 2.91-5.55 7.37-8 9-3 2-2.82 4.34-5 4-1.042-.94 1.41-3.04 0-3-1 0 .19 1.23-1 2-1 0-4.003 1-4-4 0-2 6-12 6-12s1.89-1.9 2-3.5c-.73-.994-.5-2-.5-3 1-1 3 2.5 3 2.5h2s.78-1.992 2.5-3c1 0 1 3 1 3" fill="#1b2129"/><circle cx="9.5" cy="25.5" r=".5" fill="#cfd8e3" stroke="#cfd8e3"/><path d="M14.933 15.75a.5 1.5 30 1 1-.866-.5.5 1.5 30 1 1 .866.5z" fill="#cfd8e3" stroke="#cfd8e3"/><path d="M24.55 10.4l-.45 1.45.5.15c3.15 1 5.65 2.49 7.9 6.75S35.75 29.06 35.25 39l-.05.5h2.25l.05-.5c.5-10.06-.88-16.85-3.25-21.34-2.37-4.49-5.79-6.64-9.19-7.16l-.51-.1z" fill="#cfd8e3" stroke="none"/></g></svg>`,
    p: `<svg viewBox="0 0 45 45" class="piece-svg"><path d="M22.5 9c-2.21 0-4 1.79-4 4 0 .89.29 1.71.78 2.38C17.33 16.5 16 18.59 16 21c0 2.03.94 3.84 2.41 5.03-3 1.06-7.41 5.55-7.41 13.47h23c0-7.92-4.41-12.41-7.41-13.47 1.47-1.19 2.41-3 2.41-5.03 0-2.41-1.33-4.5-3.28-5.62.49-.67.78-1.49.78-2.38 0-2.21-1.79-4-4-4z" fill="#1b2129" stroke="#cfd8e3" stroke-width="1.3" stroke-linecap="round"/></svg>`,
  };

  // ==========================================================================
  // Web Audio API Synthesized Sound Engine
  // ==========================================================================
  class SoundFX {
    constructor() {
      this.ctx = null;
      this.enabled = true;
    }

    ensure() {
      if (!this.enabled) return null;
      if (!this.ctx) {
        const AC = window.AudioContext || window.webkitAudioContext;
        if (AC) this.ctx = new AC();
      }
      if (this.ctx && this.ctx.state === "suspended") {
        this.ctx.resume();
      }
      return this.ctx;
    }

    woodTap(timeOffset = 0, freq = 185, gainAmt = 0.28) {
      const ctx = this.ensure();
      if (!ctx) return;
      const t = ctx.currentTime + timeOffset;

      const osc = ctx.createOscillator();
      const gain = ctx.createGain();
      osc.type = "triangle";
      osc.frequency.setValueAtTime(freq, t);
      osc.frequency.exponentialRampToValueAtTime(65, t + 0.065);
      gain.gain.setValueAtTime(gainAmt, t);
      gain.gain.exponentialRampToValueAtTime(0.001, t + 0.07);
      osc.connect(gain);
      gain.connect(ctx.destination);
      osc.start(t);
      osc.stop(t + 0.075);

      const bufSize = Math.floor(ctx.sampleRate * 0.025);
      const buf = ctx.createBuffer(1, bufSize, ctx.sampleRate);
      const data = buf.getChannelData(0);
      for (let i = 0; i < bufSize; i++) {
        data[i] = (Math.random() * 2 - 1) * Math.exp(-i / (bufSize * 0.25));
      }
      const noise = ctx.createBufferSource();
      noise.buffer = buf;
      const filter = ctx.createBiquadFilter();
      filter.type = "bandpass";
      filter.frequency.value = 1400;
      const ng = ctx.createGain();
      ng.gain.setValueAtTime(gainAmt * 0.65, t);
      ng.gain.exponentialRampToValueAtTime(0.001, t + 0.025);
      noise.connect(filter);
      filter.connect(ng);
      ng.connect(ctx.destination);
      noise.start(t);
    }

    tone(timeOffset, freq, dur, type = "sine", vol = 0.14) {
      const ctx = this.ensure();
      if (!ctx) return;
      const t = ctx.currentTime + timeOffset;
      const osc = ctx.createOscillator();
      const g = ctx.createGain();
      osc.type = type;
      osc.frequency.setValueAtTime(freq, t);
      g.gain.setValueAtTime(vol, t);
      g.gain.exponentialRampToValueAtTime(0.001, t + dur);
      osc.connect(g);
      g.connect(ctx.destination);
      osc.start(t);
      osc.stop(t + dur + 0.01);
    }

    play(name) {
      if (!this.enabled || !name) return;
      switch (name) {
        case "move":
          this.woodTap(0, 190, 0.28);
          break;
        case "capture":
          this.woodTap(0, 140, 0.36);
          this.woodTap(0.032, 240, 0.24);
          break;
        case "castle":
          this.woodTap(0, 185, 0.26);
          this.woodTap(0.095, 220, 0.26);
          break;
        case "check":
          this.woodTap(0, 210, 0.25);
          this.tone(0.015, 660, 0.14, "triangle", 0.16);
          this.tone(0.07, 880, 0.18, "sine", 0.12);
          break;
        case "promote":
          this.woodTap(0, 200, 0.22);
          this.tone(0.02, 523.25, 0.14, "triangle", 0.13);
          this.tone(0.09, 659.25, 0.14, "triangle", 0.13);
          this.tone(0.16, 783.99, 0.22, "triangle", 0.15);
          break;
        case "game_over":
          this.tone(0, 440, 0.22, "triangle", 0.15);
          this.tone(0.14, 554.37, 0.22, "triangle", 0.15);
          this.tone(0.28, 659.25, 0.38, "triangle", 0.18);
          break;
        case "illegal":
          this.tone(0, 115, 0.12, "sawtooth", 0.09);
          break;
        default:
          break;
      }
    }
  }

  // ==========================================================================
  // App State & DOM References
  // ==========================================================================
  const sfx = new SoundFX();
  let state = null;
  let flipped = false;
  let autoFlip = false;
  let showEvalBar = true;
  let selectedSq = null;
  let hintArrow = null; // { from, to, color }
  let bestAnalysisArrow = null; // { from, to, color }
  let userArrows = []; // [{ from, to, color }]
  let userMarks = new Map(); // sq -> color ("red"|"green"|"orange"|"blue")
  let busy = false;
  let watchPaused = false;
  let watchTimer = null;
  let pendingPromo = null;
  let cmdHistory = [];
  let cmdHistIdx = -1;
  let autoReviewedPly = -1;
  let preAnalysisMode = "white";
  let analyzeSeq = 0;

  // Custom transparent piece drag state
  let pieceDrag = null; // { fromSq, pc, startX, startY, moved, sqSize, hoverSq }
  // Right-click arrow/mark drag state
  let rightDrag = null; // { fromSq, hoverSq, color }

  const $ = (id) => document.getElementById(id);
  const boardEl = $("board");
  const boardShell = $("board-shell");
  const arrowLayer = $("arrow-layer");
  const dragGhost = $("drag-ghost");
  const termOut = $("terminal-output");
  const termInput = $("terminal-input");
  const termPrompt = $("terminal-prompt");
  const modeSelect = $("mode-select");
  const whiteLevelSel = $("white-level");
  const blackLevelSel = $("black-level");

  // ==========================================================================
  // API & Coordinate Helpers
  // ==========================================================================
  async function api(path, method = "GET", body = null) {
    const opts = { method, headers: {} };
    if (body !== null) {
      opts.headers["Content-Type"] = "application/json";
      opts.body = JSON.stringify(body);
    }
    const res = await fetch(path, opts);
    return res.json();
  }

  function logTerm(text, cls = "term-info") {
    const div = document.createElement("div");
    div.className = `term-line ${cls}`;
    div.textContent = text;
    termOut.appendChild(div);
    termOut.scrollTop = termOut.scrollHeight;
  }

  function algebraicToSq(str) {
    if (!str || str.length !== 2) return -1;
    const f = str.charCodeAt(0) - 97;
    const r = str.charCodeAt(1) - 49;
    if (f < 0 || f > 7 || r < 0 || r > 7) return -1;
    return r * 8 + f;
  }

  function sqFromPoint(clientX, clientY) {
    const rect = boardEl.getBoundingClientRect();
    if (
      clientX < rect.left ||
      clientX > rect.right ||
      clientY < rect.top ||
      clientY > rect.bottom
    ) {
      return -1;
    }
    const col = Math.min(7, Math.max(0, Math.floor(((clientX - rect.left) / rect.width) * 8)));
    const row = Math.min(7, Math.max(0, Math.floor(((clientY - rect.top) / rect.height) * 8)));
    const rank = flipped ? row : 7 - row;
    const file = flipped ? 7 - col : col;
    return rank * 8 + file;
  }

  function clearUserAnnotations() {
    if (userArrows.length > 0 || userMarks.size > 0) {
      userArrows = [];
      userMarks.clear();
      renderBoard();
    }
  }

  // ==========================================================================
  // Rendering
  // ==========================================================================
  function populatePresets(presets) {
    const targets = [
      { el: whiteLevelSel, def: "4" },
      { el: blackLevelSel, def: "4" },
      { el: $("home-opponent"), def: "4" },
      { el: $("home-watch-w"), def: "6" },
      { el: $("home-watch-b"), def: "9" },
      { el: $("bfh-white-bot"), def: "12" },
      { el: $("bfh-black-bot"), def: "9" },
    ];
    for (const t of targets) {
      if (!t.el) continue;
      const prev = t.el.value || t.def;
      t.el.innerHTML = "";
      for (const p of presets) {
        let eloTag = "";
        if ((p.engine === "c" || p.engine === "az" || p.engine === "hybrid" || p.engine === "apex") && p.has_elo) {
          eloTag = p.is_relative ? ` (${p.est_elo >= 0 ? "+" : ""}${p.est_elo} rel)` : ` (~${p.est_elo} Elo)`;
        }
        const prefix = p.engine === "c" ? "Titan-C " : "";
        let label;
        if (p.locked) {
          label = p.needs_syzygy
            ? `🔒 ${p.level} - ${p.name} (Needs 'make syzygy-5')`
            : p.needs_distill
            ? `🔒 ${p.level} - ${p.name} (Locked • Needs NNUE Distillation)`
            : `🔒 ${p.level} - ${p.name} (Locked • After Zero-C v2)`;
        } else if (p.available) {
          label = `${p.level} - ${prefix}${p.name}${eloTag}`;
        } else {
          label = `${p.level} - ${prefix}${p.name} (unavailable)`;
        }
        const opt = new Option(label, String(p.level));
        opt.title = p.desc;
        if (!p.available || p.locked) opt.disabled = true;
        t.el.add(opt);
      }
      if ([...t.el.options].some((o) => o.value === prev && !o.disabled)) {
        t.el.value = prev;
      } else {
        t.el.value = "4";
      }
    }
    if (state && state.stockfish_available) {
      $("sf-analyze-wrap").classList.remove("hidden");
    }

    // Sync lock state of Hybrid Boss options in Elo Calculator & Gauntlet Scope
    for (const p of presets) {
      if (p.engine !== "hybrid") continue;
      for (const selId of ["calc-bot", "gt-scope"]) {
        const sel = $(selId);
        if (!sel) continue;
        const opt = [...sel.options].find((o) => o.value === String(p.level));
        if (opt) {
          opt.disabled = Boolean(p.locked || !p.available);
          if (!p.locked && p.available) {
            opt.textContent = `⚡ ${p.name} (Hybrid Boss)`;
          }
        }
      }
    }

    const eloMini = $("home-elo-mini");
    if (eloMini && presets) {
      const rated = presets.filter((p) => p.engine === "c" || (p.engine === "az" && p.available));
      if (rated.some((p) => p.has_elo)) {
        eloMini.innerHTML = rated
          .map(
            (p) =>
              `<div class="elo-mini-pill"><span>${
                p.level === 13 ? "Zero-C v1 🏆" : p.engine === "az" ? "Zero-C v2" : `L${p.level} ${p.name}`
              }</span><strong>${p.has_elo ? `~${p.est_elo}` : "—"}</strong></div>`
          )
          .join("");
      }
    }

    if (typeof renderHomeFightersPreview === "function") {
      renderHomeFightersPreview();
    }
  }

  function syncTopbar() {
    if (state.mode !== "puzzle") {
      modeSelect.value = state.mode;
    }
    whiteLevelSel.value = String(state.white_level);
    blackLevelSel.value = String(state.black_level);

    const isPuzzle = state.mode === "puzzle";
    const showW = state.mode === "black" || state.mode === "watch";
    const showB = state.mode === "white" || state.mode === "watch";
    $("white-level-wrap").classList.toggle("hidden", !showW);
    $("black-level-wrap").classList.toggle("hidden", !showB);
    $("watch-bar").classList.toggle("hidden", state.mode !== "watch");
    $("analysis-bar").classList.toggle("hidden", state.mode !== "analysis");
    $("puzzle-bar").classList.toggle("hidden", !isPuzzle);
    $("btn-analysis-toggle").classList.toggle("btn-toggled", state.mode === "analysis");
    $("btn-puzzles").classList.toggle("btn-toggled", isPuzzle);
    $("btn-autoflip").classList.toggle("btn-toggled", autoFlip);
    $("btn-eval-toggle").classList.toggle("btn-toggled", showEvalBar);

    // Hide eval bar during an unsolved puzzle so it doesn't spoil the tactic!
    const hideEvalForPuzzle = isPuzzle && state.puzzle && !state.puzzle.solved;
    $("eval-wrap").classList.toggle("hidden", !showEvalBar || hideEvalForPuzzle);

    if (state.mode === "analysis") {
      $("nav-ply").textContent = `${state.cursor_ply} / ${state.total_plies}`;
    }
    if (isPuzzle && state.puzzle) {
      renderPuzzleHud();
    }
  }

  function renderCaptured(container, pieces) {
    container.innerHTML = "";
    for (const ch of pieces) {
      const span = document.createElement("span");
      span.className = "cap-piece";
      span.innerHTML = PIECE_SVGS[ch] || "";
      const svg = span.querySelector("svg");
      if (svg) svg.setAttribute("class", "cap-piece");
      container.appendChild(span);
    }
  }

  function renderStrips() {
    const topIsWhite = flipped;
    const topName = topIsWhite ? state.white_name : state.black_name;
    const botName = topIsWhite ? state.black_name : state.white_name;
    const topLvl = topIsWhite
      ? (state.mode === "black" || state.mode === "watch" ? state.white_level : 0)
      : (state.mode === "white" || state.mode === "watch" ? state.black_level : 0);
    const botLvl = topIsWhite
      ? (state.mode === "white" || state.mode === "watch" ? state.black_level : 0)
      : (state.mode === "black" || state.mode === "watch" ? state.white_level : 0);

    $("top-dot").className = `player-dot ${topIsWhite ? "white" : "black"}`;
    $("bottom-dot").className = `player-dot ${topIsWhite ? "black" : "white"}`;
    $("top-name").textContent = topName;
    $("bottom-name").textContent = botName;
    if (typeof applyStripAvatar === "function") {
      applyStripAvatar($("top-avatar"), topName, topLvl);
      applyStripAvatar($("bottom-avatar"), botName, botLvl);
    }

    renderCaptured($("top-captured"), topIsWhite ? state.captured.white : state.captured.black);
    renderCaptured($("bottom-captured"), topIsWhite ? state.captured.black : state.captured.white);

    const adv = state.captured.white_adv;
    const topAdv = topIsWhite ? adv : -adv;
    const botAdv = topIsWhite ? -adv : adv;
    $("top-adv").textContent = topAdv > 0 ? `+${topAdv}` : "";
    $("bottom-adv").textContent = botAdv > 0 ? `+${botAdv}` : "";

    const pill = $("status-pill");
    pill.className = "status-pill";
    if (state.is_game_over) {
      pill.textContent = state.game_over_reason;
      pill.classList.add("over");
    } else if (busy) {
      pill.textContent = `${state.turn === "w" ? "White" : "Black"} thinking...`;
      pill.classList.add("thinking");
    } else if (state.in_check) {
      pill.textContent = `${state.turn === "w" ? "White" : "Black"} in CHECK!`;
      pill.classList.add("check");
    } else if (state.mode === "puzzle" && state.puzzle) {
      pill.textContent = state.puzzle.solved
        ? `Puzzle #${state.puzzle.id} Solved!`
        : `Puzzle • ${state.turn === "w" ? "White" : "Black"} to move`;
      pill.classList.add(state.puzzle.solved ? "over" : "thinking");
    } else if (state.mode === "analysis") {
      pill.textContent = `Analysis • ${state.turn === "w" ? "White" : "Black"} to move`;
      pill.classList.add("thinking");
    } else {
      pill.textContent = `${state.turn === "w" ? "White" : "Black"} to move`;
    }

    if ($("opening-pill")) {
      $("opening-pill").textContent = state.opening || "Starting Position";
    }
    $("meta-move").textContent = `Move ${state.move_number}`;
    $("meta-clock").textContent = `Clock ${state.half_move_count}/150`;
    $("meta-legal").textContent = `${state.legal_moves.length} legal`;
    termPrompt.textContent = `${state.turn === "w" ? "White" : "Black"}>`;
  }

  function renderEvalBar() {
    $("eval-label").textContent = state.eval_str.trim();
    const bar = $("eval-bar");
    bar.classList.toggle("flipped", flipped);
    const pct = Math.max(0, Math.min(100, state.win_prob * 100));
    $("eval-fill-white").style.height = `${pct.toFixed(1)}%`;
  }

  // ==========================================================================
  // SVG Arrow Layer (User Right-Click Arrows, Knight L-Arrows, Hint & Best Move)
  // ==========================================================================
  const ARROW_PALETTE = {
    orange: "rgba(245, 166, 35, 0.84)",
    green: "rgba(63, 185, 80, 0.82)",
    red: "rgba(240, 78, 72, 0.82)",
    blue: "rgba(88, 166, 255, 0.84)",
    best: "rgba(63, 185, 80, 0.62)",
  };

  function sqCenter(sq) {
    const f = sq % 8;
    const r = Math.floor(sq / 8);
    const col = flipped ? 7 - f : f;
    const row = flipped ? r : 7 - r;
    return { x: col * 100 + 50, y: row * 100 + 50, f, r };
  }

  function buildArrowMarkup(fromSq, toSq, colorKey, markerId, isPreview = false) {
    const a = sqCenter(fromSq);
    const b = sqCenter(toSq);
    const fill = ARROW_PALETTE[colorKey] || ARROW_PALETTE.orange;
    const opacity = isPreview ? 0.58 : 1.0;

    const df = Math.abs(b.f - a.f);
    const dr = Math.abs(b.r - a.r);

    // Bent L-shaped arrow for knight moves (Chess.com style!)
    if ((df === 1 && dr === 2) || (df === 2 && dr === 1)) {
      let midX, midY;
      if (dr === 2) {
        midX = a.x;
        midY = b.y;
      } else {
        midX = b.x;
        midY = a.y;
      }
      const dx = b.x - midX;
      const dy = b.y - midY;
      const len = Math.hypot(dx, dy) || 1;
      const ex = b.x - (dx / len) * 24;
      const ey = b.y - (dy / len) * 24;
      return `<path d="M ${a.x} ${a.y} L ${midX} ${midY} L ${ex} ${ey}"
                fill="none" stroke="${fill}" stroke-width="18"
                stroke-linecap="round" stroke-linejoin="round"
                opacity="${opacity}" marker-end="url(#${markerId})" />`;
    }

    const dx = b.x - a.x;
    const dy = b.y - a.y;
    const len = Math.hypot(dx, dy) || 1;
    const ux = dx / len;
    const uy = dy / len;
    const sx = a.x + ux * 16;
    const sy = a.y + uy * 16;
    const ex = b.x - ux * 26;
    const ey = b.y - uy * 26;
    return `<line x1="${sx}" y1="${sy}" x2="${ex}" y2="${ey}"
              stroke="${fill}" stroke-width="18" stroke-linecap="round"
              opacity="${opacity}" marker-end="url(#${markerId})" />`;
  }

  function renderArrows() {
    const defs = Object.entries(ARROW_PALETTE)
      .map(
        ([k, col]) =>
          `<marker id="ah-${k}" markerWidth="4.2" markerHeight="4.2" refX="2.1" refY="2.1" orient="auto">
             <path d="M0,0 L4.2,2.1 L0,4.2 Z" fill="${col}" />
           </marker>`
      )
      .join("");

    let shapes = "";
    if (
      state &&
      state.mode === "analysis" &&
      $("chk-best-arrow").checked &&
      bestAnalysisArrow
    ) {
      shapes += buildArrowMarkup(bestAnalysisArrow.from, bestAnalysisArrow.to, "best", "ah-best");
    }
    if (spectateActive && spectateArrows.length > 0) {
      const spCols = ["best", "blue", "orange"];
      spectateArrows.forEach((arr, idx) => {
        const ck = spCols[idx % spCols.length];
        shapes += buildArrowMarkup(arr.from, arr.to, ck, `ah-${ck}`);
      });
    }
    if (hintArrow) {
      shapes += buildArrowMarkup(hintArrow.from, hintArrow.to, "blue", "ah-blue");
    }
    for (const arr of userArrows) {
      shapes += buildArrowMarkup(arr.from, arr.to, arr.color, `ah-${arr.color}`);
    }
    if (rightDrag && rightDrag.hoverSq >= 0 && rightDrag.hoverSq !== rightDrag.fromSq) {
      shapes += buildArrowMarkup(
        rightDrag.fromSq,
        rightDrag.hoverSq,
        rightDrag.color,
        `ah-${rightDrag.color}`,
        true
      );
    }

    arrowLayer.innerHTML = `<defs>${defs}</defs>${shapes}`;
  }

  function legalMovesFrom(sq) {
    if (sq === null || !state) return [];
    return state.legal_moves.filter((m) => m.from === sq);
  }

  function renderBoard() {
    boardEl.innerHTML = "";
    const destMap = new Map();
    for (const m of legalMovesFrom(selectedSq)) {
      destMap.set(m.to, m.capture);
    }

    for (let row = 0; row < 8; row++) {
      for (let col = 0; col < 8; col++) {
        const rank = flipped ? row : 7 - row;
        const file = flipped ? 7 - col : col;
        const sq = rank * 8 + file;
        const isLight = (rank + file) % 2 !== 0;

        const sqEl = document.createElement("div");
        sqEl.className = `square ${isLight ? "light" : "dark"}`;
        sqEl.dataset.sq = String(sq);

        if (state.last_move && (sq === state.last_move.from || sq === state.last_move.to)) {
          sqEl.classList.add("last-move");
        }
        if (sq === selectedSq) {
          sqEl.classList.add("selected");
        }
        if (sq === state.check_sq) {
          sqEl.classList.add("in-check");
        }
        if (pieceDrag && pieceDrag.moved && pieceDrag.hoverSq === sq) {
          sqEl.classList.add("drag-over");
        }
        if (destMap.has(sq)) {
          sqEl.classList.add(destMap.get(sq) ? "legal-capture" : "legal-dest");
        }

        // Right-click colored square highlight
        if (userMarks.has(sq)) {
          const mark = document.createElement("div");
          mark.className = `square-mark mark-${userMarks.get(sq)}`;
          sqEl.appendChild(mark);
        }

        if (col === 0) {
          const rl = document.createElement("span");
          rl.className = "coord-rank";
          rl.textContent = String(rank + 1);
          sqEl.appendChild(rl);
        }
        if (row === 7) {
          const fl = document.createElement("span");
          fl.className = "coord-file";
          fl.textContent = String.fromCharCode(97 + file);
          sqEl.appendChild(fl);
        }

        const pc = state.board[sq];
        if (pc) {
          const wrap = document.createElement("div");
          wrap.className = "piece-wrap";
          if (pieceDrag && pieceDrag.moved && pieceDrag.fromSq === sq) {
            wrap.classList.add("dragging-source");
          }
          wrap.innerHTML = PIECE_SVGS[pc] || "";
          sqEl.appendChild(wrap);
        }

        boardEl.appendChild(sqEl);
      }
    }
    renderArrows();
  }

  function renderHistory() {
    const list = $("history-list");
    if (!state.history || state.history.length === 0) {
      list.innerHTML = `<div class="empty-hint">No moves played yet.</div>`;
      $("history-result").textContent = "*";
      return;
    }

    let resToken = "*";
    if (state.is_game_over && state.cursor_ply === state.total_plies) {
      resToken = state.is_draw ? "1/2-1/2" : state.turn === "b" ? "1-0" : "0-1";
    }
    $("history-result").textContent = resToken;

    const rows = [];
    for (let i = 0; i < state.history.length; i++) {
      const h = state.history[i];
      if (h.side === "w") {
        rows.push({ num: h.move_num, w: h, b: null });
      } else {
        if (rows.length === 0 || rows[rows.length - 1].b !== null) {
          rows.push({ num: h.move_num, w: null, b: h });
        } else {
          rows[rows.length - 1].b = h;
        }
      }
    }

    list.innerHTML = rows
      .map((r) => {
        const wCell = r.w
          ? `<span class="hist-move ${r.w.ply === state.cursor_ply ? "active" : ""}" data-ply="${r.w.ply}">${r.w.san}</span>`
          : `<span class="hist-num">...</span>`;
        const bCell = r.b
          ? `<span class="hist-move ${r.b.ply === state.cursor_ply ? "active" : ""}" data-ply="${r.b.ply}">${r.b.san}</span>`
          : `<span></span>`;
        return `<div class="hist-row"><span class="hist-num">${r.num}.</span>${wCell}${bCell}</div>`;
      })
      .join("");

    list.querySelectorAll(".hist-move[data-ply]").forEach((el) => {
      el.addEventListener("click", () => {
        const ply = parseInt(el.dataset.ply, 10);
        jumpToPlyInAnalysis(ply);
      });
    });

    const activeEl = list.querySelector(".hist-move.active");
    if (activeEl) {
      activeEl.scrollIntoView({ block: "nearest" });
    } else {
      list.scrollTop = list.scrollHeight;
    }
  }

  function renderAll() {
    if (!state) return;
    if (autoFlip) {
      flipped = state.turn === "b";
    }
    populatePresets(state.presets);
    syncTopbar();
    renderStrips();
    renderEvalBar();
    renderBoard();
    renderHistory();
  }

  // ==========================================================================
  // Pointer Interaction: Transparent Drag-and-Drop + Click-Move + Right-Click Arrows
  // ==========================================================================
  function pickRightColor(ev, isMark = false) {
    if (ev.shiftKey) return "green";
    if (ev.ctrlKey || ev.metaKey) return isMark ? "orange" : "red";
    if (ev.altKey) return "blue";
    return isMark ? "red" : "orange";
  }

  function positionGhost(clientX, clientY, sqSize) {
    const x = clientX - sqSize / 2;
    const y = clientY - sqSize / 2;
    dragGhost.style.width = `${sqSize}px`;
    dragGhost.style.height = `${sqSize}px`;
    dragGhost.style.transform = `translate3d(${x}px, ${y}px, 0)`;
  }

  boardShell.addEventListener("contextmenu", (e) => e.preventDefault());

  boardShell.addEventListener("pointerdown", (e) => {
    if (!state) return;
    const sq = sqFromPoint(e.clientX, e.clientY);
    if (sq < 0) return;

    // Right click: start arrow or square highlight
    if (e.button === 2) {
      e.preventDefault();
      rightDrag = {
        fromSq: sq,
        hoverSq: sq,
        color: pickRightColor(e, false),
        markColor: pickRightColor(e, true),
      };
      boardShell.setPointerCapture(e.pointerId);
      renderArrows();
      return;
    }

    if (e.button !== 0) return;

    // Left click clears right-click arrows/highlights (Chess.com behavior)
    if (userArrows.length > 0 || userMarks.size > 0) {
      userArrows = [];
      userMarks.clear();
    }

    if (busy || state.is_game_over || state.is_bot_turn) {
      renderBoard();
      return;
    }

    // If a piece is already selected and clicking a legal destination -> move immediately!
    if (selectedSq !== null) {
      const matching = state.legal_moves.filter((m) => m.from === selectedSq && m.to === sq);
      if (matching.length > 0) {
        attemptMouseMove(selectedSq, sq);
        return;
      }
    }

    const pc = state.board[sq];
    const isOwn =
      pc &&
      ((state.turn === "w" && pc === pc.toUpperCase()) ||
        (state.turn === "b" && pc === pc.toLowerCase()));

    if (isOwn) {
      const sqSize = boardEl.getBoundingClientRect().width / 8;
      const wasSelected = selectedSq === sq;
      selectedSq = sq;
      pieceDrag = {
        fromSq: sq,
        pc,
        startX: e.clientX,
        startY: e.clientY,
        moved: false,
        wasSelected,
        sqSize,
        hoverSq: sq,
      };
      boardShell.setPointerCapture(e.pointerId);
      renderBoard();
    } else {
      selectedSq = null;
      renderBoard();
    }
  });

  boardShell.addEventListener("pointermove", (e) => {
    if (rightDrag) {
      const sq = sqFromPoint(e.clientX, e.clientY);
      if (sq !== rightDrag.hoverSq) {
        rightDrag.hoverSq = sq;
        renderArrows();
      }
      return;
    }

    if (pieceDrag) {
      const dist = Math.hypot(e.clientX - pieceDrag.startX, e.clientY - pieceDrag.startY);
      if (!pieceDrag.moved && dist > 5) {
        pieceDrag.moved = true;
        dragGhost.innerHTML = PIECE_SVGS[pieceDrag.pc] || "";
        dragGhost.classList.remove("hidden");
        renderBoard();
      }
      if (pieceDrag.moved) {
        positionGhost(e.clientX, e.clientY, pieceDrag.sqSize);
        const hsq = sqFromPoint(e.clientX, e.clientY);
        if (hsq !== pieceDrag.hoverSq) {
          pieceDrag.hoverSq = hsq;
          renderBoard();
        }
      }
    }
  });

  boardShell.addEventListener("pointerup", (e) => {
    if (rightDrag && e.button === 2) {
      const endSq = sqFromPoint(e.clientX, e.clientY);
      const { fromSq, color, markColor } = rightDrag;
      rightDrag = null;
      if (endSq === fromSq) {
        // Toggle square highlight
        if (userMarks.get(fromSq) === markColor) {
          userMarks.delete(fromSq);
        } else {
          userMarks.set(fromSq, markColor);
        }
        renderBoard();
      } else if (endSq >= 0) {
        // Toggle arrow
        const existingIdx = userArrows.findIndex(
          (a) => a.from === fromSq && a.to === endSq && a.color === color
        );
        if (existingIdx >= 0) {
          userArrows.splice(existingIdx, 1);
        } else {
          // Replace any differently colored arrow between the same two squares
          userArrows = userArrows.filter((a) => !(a.from === fromSq && a.to === endSq));
          userArrows.push({ from: fromSq, to: endSq, color });
        }
        renderArrows();
      } else {
        renderArrows();
      }
      return;
    }

    if (pieceDrag && e.button === 0) {
      const drag = pieceDrag;
      pieceDrag = null;
      dragGhost.classList.add("hidden");
      dragGhost.innerHTML = "";

      if (drag.moved) {
        const dropSq = sqFromPoint(e.clientX, e.clientY);
        if (dropSq >= 0 && dropSq !== drag.fromSq) {
          attemptMouseMove(drag.fromSq, dropSq);
        } else {
          renderBoard();
        }
      } else if (drag.wasSelected) {
        selectedSq = null;
        renderBoard();
      }
    }
  });

  // ==========================================================================
  // Move Execution, Bot Loop, Analysis & Promotions
  // ==========================================================================
  async function runLiveAnalysis() {
    if (!state || state.mode !== "analysis") return;
    const seq = ++analyzeSeq;
    const useSf = Boolean(state.stockfish_available && $("chk-sf-analyze").checked);
    $("pv-line").textContent = useSf ? "Stockfish analyzing..." : "Analyzing top 3 lines...";
    const linesBox = $("analysis-lines");
    linesBox.innerHTML = "";
    linesBox.classList.add("hidden");

    const res = await api("/api/analyze", "POST", { use_stockfish: useSf });
    if (seq !== analyzeSeq || !state || state.mode !== "analysis") return;
    if (!res.ok) return;

    state.eval_cp = res.eval_cp;
    state.eval_str = res.eval_str;
    state.win_prob = res.win_prob;
    const primaryArrow = res.best ? { from: res.best.from, to: res.best.to } : null;
    bestAnalysisArrow = primaryArrow;

    $("pv-eval").textContent = res.eval_str.trim();
    const engPrefix = res.engine === "stockfish" ? "SF " : "Multi-PV ";
    $("pv-depth").textContent = res.depth > 0 ? `${engPrefix}d${res.depth}` : "end";
    if (res.pv && res.pv.length > 0) {
      const npsStr = res.nps > 0 ? `  (${(res.nps / 1e6).toFixed(2)}M/s)` : "";
      $("pv-line").textContent = `Best: ${res.pv.join(" ")}${npsStr}`;
    } else {
      $("pv-line").textContent = state.game_over_reason || "No legal moves.";
    }

    if (res.lines && res.lines.length > 1) {
      linesBox.classList.remove("hidden");
      linesBox.innerHTML = "";
      for (const ln of res.lines) {
        const row = document.createElement("div");
        row.className = "mpv-row" + (ln.rank === 1 ? " mpv-best" : "");
        row.title = `Click to play ${ln.san}`;
        row.innerHTML = `<span class="mpv-rank">#${ln.rank}</span><span class="mpv-eval">${ln.eval_str.trim()}</span><span class="mpv-moves">${ln.pv.join(" ")}</span>`;
        row.addEventListener("mouseenter", () => {
          bestAnalysisArrow = { from: ln.from, to: ln.to };
          renderArrows();
        });
        row.addEventListener("mouseleave", () => {
          bestAnalysisArrow = primaryArrow;
          renderArrows();
        });
        row.addEventListener("click", () => {
          attemptMouseMove(ln.from, ln.to);
        });
        linesBox.appendChild(row);
      }
    }

    renderEvalBar();
    renderArrows();
  }

  function applyState(data, logMessage = true) {
    if (!data || !data.ok) {
      if (data && data.sound) sfx.play(data.sound);
      if (data && data.error) logTerm(data.error, "term-err");
      return false;
    }
    state = data;
    selectedSq = null;
    hintArrow = null;
    bestAnalysisArrow = null;
    userArrows = [];
    userMarks.clear();
    if (data.sound) sfx.play(data.sound);
    if (logMessage && data.message) {
      logTerm(data.message, data.is_game_over ? "term-warn" : "term-ok");
    }
    renderAll();

    if (state.mode === "analysis") {
      runLiveAnalysis();
    } else if (state.mode === "puzzle") {
      // Puzzle mode is human-driven; no auto-review modal or bot turn
    } else if (state.is_game_over && autoReviewedPly !== state.ply_count && state.ply_count > 0) {
      autoReviewedPly = state.ply_count;
      const humanWon =
        !state.is_draw &&
        ((state.mode === "white" && state.turn === "b") ||
          (state.mode === "black" && state.turn === "w") ||
          state.mode === "2p");
      if (humanWon) launchConfetti();
      setTimeout(() => openReviewModal(), 450);
    } else if (!state.is_game_over && state.is_bot_turn) {
      scheduleBotMove();
    }
    return true;
  }

  function clearWatchTimer() {
    if (watchTimer) {
      clearTimeout(watchTimer);
      watchTimer = null;
    }
  }

  function scheduleBotMove() {
    clearWatchTimer();
    if (!state || state.is_game_over || !state.is_bot_turn) return;
    if (state.mode === "watch" && watchPaused) return;
    const delay = state.mode === "watch" ? parseInt($("watch-speed").value, 10) : 120;
    watchTimer = setTimeout(() => triggerBotMove(), delay);
  }

  async function triggerBotMove() {
    if (busy || !state || state.is_game_over) return;
    busy = true;
    renderStrips();
    try {
      const data = await api("/api/bot_move", "POST");
      busy = false;
      applyState(data, true);
    } catch (e) {
      busy = false;
      logTerm(`Engine request failed: ${e.message}`, "term-err");
      renderStrips();
    }
  }

  function openPromoModal(from, to) {
    pendingPromo = { from, to };
    const isWhite = state.turn === "w";
    const opts = isWhite ? ["Q", "R", "B", "N"] : ["q", "r", "b", "n"];
    const box = $("promo-choices");
    box.innerHTML = "";
    for (const ch of opts) {
      const btn = document.createElement("button");
      btn.className = "promo-btn";
      btn.innerHTML = PIECE_SVGS[ch];
      btn.addEventListener("click", () => finishPromotion(ch.toLowerCase()));
      box.appendChild(btn);
    }
    $("promo-modal").classList.remove("hidden");
  }

  function closePromoModal() {
    pendingPromo = null;
    $("promo-modal").classList.add("hidden");
  }

  async function finishPromotion(promoChar) {
    if (!pendingPromo) return;
    const { from, to } = pendingPromo;
    closePromoModal();
    if (state && state.mode === "puzzle") {
      await submitPuzzleMove(from, to, promoChar);
      return;
    }
    busy = true;
    const data = await api("/api/move", "POST", { from, to, promo: promoChar });
    busy = false;
    applyState(data, true);
  }

  async function attemptMouseMove(from, to) {
    if (busy || !state || state.is_game_over || state.is_bot_turn) return;
    if (state.mode === "puzzle" && state.puzzle && state.puzzle.solved) return;
    const candidates = state.legal_moves.filter((m) => m.from === from && m.to === to);
    if (candidates.length === 0) {
      selectedSq = null;
      sfx.play("illegal");
      renderBoard();
      return;
    }
    if (candidates.length > 1 && candidates[0].promo !== "") {
      openPromoModal(from, to);
      return;
    }
    if (state.mode === "puzzle") {
      await submitPuzzleMove(from, to, candidates[0].promo);
      return;
    }
    busy = true;
    const data = await api("/api/move", "POST", { from, to, promo: candidates[0].promo });
    busy = false;
    applyState(data, true);
  }

  // ==========================================================================
  // Analysis Navigation, Mode Switching, PGN Import & Modals
  // ==========================================================================
  async function enterAnalysisMode() {
    clearWatchTimer();
    syncUrl("/analysis");
    if (state && state.mode !== "analysis") {
      preAnalysisMode = state.mode;
    }
    const data = await api("/api/mode", "POST", { mode: "analysis" });
    applyState(data, true);
  }

  async function exitAnalysisToPlay(targetMode = null) {
    clearWatchTimer();
    const m = targetMode || (preAnalysisMode === "analysis" ? "white" : preAnalysisMode);
    const wl = parseInt(whiteLevelSel.value, 10) || 4;
    const bl = parseInt(blackLevelSel.value, 10) || 4;
    const data = await api("/api/mode", "POST", { mode: m, white_level: wl, black_level: bl });
    applyState(data, true);
  }

  async function gotoPly(ply) {
    if (busy || !state) return;
    const clamped = Math.max(0, Math.min(state.total_plies, ply));
    if (clamped === state.cursor_ply) return;
    const data = await api("/api/goto", "POST", { ply: clamped });
    applyState(data, false);
  }

  async function jumpToPlyInAnalysis(ply) {
    if (!state) return;
    if (state.mode !== "analysis") {
      clearWatchTimer();
      preAnalysisMode = state.mode;
      await api("/api/mode", "POST", { mode: "analysis" });
    }
    const data = await api("/api/goto", "POST", { ply });
    applyState(data, false);
  }

  async function startNewGame(customMode = null, customWL = null, customBL = null, customFen = "", customElo = 0, azCheckpoint = "") {
    stopSpectateTraining(false);
    clearWatchTimer();
    watchPaused = false;
    $("btn-watch-toggle").textContent = "\u23F8 Pause";
    autoReviewedPly = -1;
    const mode = customMode || modeSelect.value;
    const wl = customWL || parseInt(whiteLevelSel.value, 10) || 4;
    const bl = customBL || parseInt(blackLevelSel.value, 10) || 4;
    if (!autoFlip) {
      flipped = mode === "black";
    }
    const data = await api("/api/new", "POST", {
      mode,
      white_level: wl,
      black_level: bl,
      custom_elo: customElo || 0,
      az_checkpoint: azCheckpoint || "",
      fen: customFen,
    });
    applyState(data, true);
  }

  async function doUndo() {
    if (busy) return;
    clearWatchTimer();
    autoReviewedPly = -1;
    const data = await api("/api/undo", "POST");
    applyState(data, true);
  }

  async function doHint() {
    if (busy || !state || state.is_game_over) return;
    busy = true;
    renderStrips();
    const res = await api("/api/hint", "POST");
    busy = false;
    if (res.ok) {
      hintArrow = { from: res.from, to: res.to };
      state.eval_cp = res.eval_cp;
      state.eval_str = res.eval_str;
      state.win_prob = res.win_prob;
      logTerm(`Hint: try ${res.san}  (d=${res.depth}, eval ${res.eval_str.trim()})`, "term-ok");
      renderAll();
    } else {
      logTerm(res.error || "Hint unavailable.", "term-err");
      renderStrips();
    }
  }

  function doFlip() {
    if (autoFlip) {
      autoFlip = false;
      logTerm("Auto-Flip disabled (manual orientation).", "term-dim");
    }
    flipped = !flipped;
    renderAll();
    logTerm("Board flipped.", "term-dim");
  }

  function toggleAutoFlip() {
    autoFlip = !autoFlip;
    if (autoFlip && state) {
      flipped = state.turn === "b";
    }
    renderAll();
    logTerm(`Dynamic Auto-Flip ${autoFlip ? "enabled (follows side to move)" : "disabled"}.`, "term-ok");
  }

  function toggleEvalBar() {
    showEvalBar = !showEvalBar;
    syncTopbar();
    logTerm(`Evaluation bar ${showEvalBar ? "shown" : "hidden"}.`, "term-dim");
  }

  async function doPgn(copyToClipboard = true) {
    const res = await api("/api/pgn", "GET");
    if (!res.ok) return;
    logTerm(res.pgn.trim(), "term-info");
    if (copyToClipboard && navigator.clipboard) {
      try {
        await navigator.clipboard.writeText(res.pgn);
        logTerm("PGN copied to clipboard!", "term-ok");
      } catch (_) {}
    }
  }

  async function openPgnModal(prefillCurrent = false) {
    if (prefillCurrent) {
      const res = await api("/api/pgn", "GET");
      $("pgn-textarea").value = res.ok ? res.pgn : "";
    } else {
      $("pgn-textarea").value = "";
    }
    $("pgn-modal").classList.remove("hidden");
    $("pgn-textarea").focus();
  }

  async function importPgnText(pgnStr) {
    clearWatchTimer();
    autoReviewedPly = -1;
    const data = await api("/api/import_pgn", "POST", { pgn: pgnStr });
    showBoardWorkspace();
    applyState(data, true);
  }

  function openFenModal() {
    $("fen-input").value = state ? state.fen : "";
    $("fen-modal").classList.remove("hidden");
    $("fen-input").select();
  }

  async function loadFenString(fenStr) {
    clearWatchTimer();
    autoReviewedPly = -1;
    const data = await api("/api/fen", "POST", { fen: fenStr });
    applyState(data, true);
  }

  // ==========================================================================
  // Post-Game Review Modal (Lichess Win%-Based Accuracy, Brilliant !!, Interactive Graph)
  // ==========================================================================
  function renderLichessEvalGraph(containerEl, timeline, moves, onJumpPly) {
    if (!timeline || timeline.length < 2) {
      containerEl.innerHTML = "";
      return;
    }
    const W = 720, H = 126, padL = 6, padR = 6, padT = 8, padB = 18;
    const pw = W - padL - padR, ph = H - padT - padB;
    const n = timeline.length;
    const sx = (i) => padL + (i / (n - 1)) * pw;
    const sy = (wp) => padT + (1 - Math.max(0.01, Math.min(0.99, wp))) * ph;
    const midY = padT + ph / 2;

    const pts = timeline.map((wp, i) => `${sx(i).toFixed(1)},${sy(wp).toFixed(1)}`);
    const polyPts = `${sx(0).toFixed(1)},${midY.toFixed(1)} ${pts.join(" ")} ${sx(n - 1).toFixed(1)},${midY.toFixed(1)}`;

    // Colored annotation dots on key moves (Brilliant, Blunder, Mistake, Inaccuracy)
    const dotColor = {
      brilliant: "#22d3ee",
      blunder: "#f85149",
      mistake: "#f0883e",
      inaccuracy: "#e3b341",
    };
    let dots = "";
    for (const m of moves) {
      const c = dotColor[m.rating];
      if (!c) continue;
      const idx = Math.min(n - 1, m.ply);
      const r = m.rating === "brilliant" || m.rating === "blunder" ? 4.3 : 3.2;
      dots += `<circle cx="${sx(idx).toFixed(1)}" cy="${sy(timeline[idx]).toFixed(1)}" r="${r}" fill="${c}" stroke="#0b0f15" stroke-width="1.2"/>`;
    }

    containerEl.innerHTML = `
      <svg viewBox="0 0 ${W} ${H}" preserveAspectRatio="none">
        <defs>
          <clipPath id="rev-clip-top"><rect x="${padL}" y="${padT}" width="${pw}" height="${ph / 2}"/></clipPath>
          <clipPath id="rev-clip-bot"><rect x="${padL}" y="${midY}" width="${pw}" height="${ph / 2}"/></clipPath>
        </defs>
        <rect x="${padL}" y="${padT}" width="${pw}" height="${ph / 2}" fill="rgba(240,246,252,0.06)" />
        <rect x="${padL}" y="${midY}" width="${pw}" height="${ph / 2}" fill="rgba(0,0,0,0.32)" />
        <line x1="${padL}" y1="${sy(0.75).toFixed(1)}" x2="${W - padR}" y2="${sy(0.75).toFixed(1)}" stroke="rgba(139,148,158,0.14)" stroke-dasharray="2 3" />
        <line x1="${padL}" y1="${midY.toFixed(1)}" x2="${W - padR}" y2="${midY.toFixed(1)}" stroke="rgba(255,255,255,0.28)" stroke-dasharray="4 4" />
        <line x1="${padL}" y1="${sy(0.25).toFixed(1)}" x2="${W - padR}" y2="${sy(0.25).toFixed(1)}" stroke="rgba(139,148,158,0.14)" stroke-dasharray="2 3" />
        <polygon points="${polyPts}" fill="rgba(240, 246, 252, 0.36)" clip-path="url(#rev-clip-top)" />
        <polygon points="${polyPts}" fill="rgba(88, 166, 255, 0.24)" clip-path="url(#rev-clip-bot)" />
        <polyline points="${pts.join(" ")}" fill="none" stroke="#c9d1d9" stroke-width="2" stroke-linejoin="round" />
        ${dots}
        <line class="rev-hover-line" x1="0" y1="${padT}" x2="0" y2="${padT + ph}" stroke="rgba(255,255,255,0.45)" stroke-dasharray="2 2" style="display:none"/>
        <text x="${padL + 4}" y="${H - 4}" fill="#8b949e" font-size="9">Start</text>
        <text x="${W / 2}" y="${H - 4}" fill="#8b949e" font-size="9" text-anchor="middle">Click anywhere on the advantage graph to jump to that move</text>
        <text x="${W - padR - 4}" y="${H - 4}" fill="#8b949e" font-size="9" text-anchor="end">Ply ${n - 1}</text>
      </svg>
      <div class="az-tooltip hidden"></div>`;

    const svg = containerEl.querySelector("svg");
    const hline = containerEl.querySelector(".rev-hover-line");
    const tip = containerEl.querySelector(".az-tooltip");
    const pickPly = (ev) => {
      const rect = svg.getBoundingClientRect();
      const relX = ((ev.clientX - rect.left) / Math.max(1, rect.width)) * W;
      const frac = Math.max(0, Math.min(1, (relX - padL) / Math.max(1, pw)));
      return Math.round(frac * (n - 1));
    };
    svg.addEventListener("mousemove", (ev) => {
      const idx = pickPly(ev);
      const xPos = sx(idx);
      hline.setAttribute("x1", xPos.toFixed(1));
      hline.setAttribute("x2", xPos.toFixed(1));
      hline.style.display = "block";
      const m = idx > 0 ? moves[idx - 1] : null;
      if (m) {
        const dot = m.side === "b" ? "..." : ".";
        tip.innerHTML = `<strong>${m.move_num}${dot} ${m.played}</strong> ${badgeHtml(m.rating)} &bull; <code>${m.eval_str.trim()}</code>`;
      } else {
        tip.innerHTML = `<strong>Start Position</strong>`;
      }
      tip.classList.remove("hidden");
      const rect = svg.getBoundingClientRect();
      const px = (xPos / W) * rect.width;
      const onRight = px > rect.width * 0.6;
      tip.style.left = onRight ? `${Math.max(4, px - tip.offsetWidth - 10)}px` : `${Math.min(rect.width - tip.offsetWidth - 4, px + 10)}px`;
      tip.style.top = "8px";
    });
    svg.addEventListener("mouseleave", () => {
      hline.style.display = "none";
      tip.classList.add("hidden");
    });
    svg.addEventListener("click", (ev) => {
      const idx = pickPly(ev);
      if (idx > 0 && onJumpPly) onJumpPly(idx);
    });
  }

  function badgeHtml(rating) {
    const map = {
      brilliant: ["!! Brilliant", "badge-brilliant"],
      best: ["&#9733; Best", "badge-best"],
      good: ["&#10003; Good", "badge-good"],
      inaccuracy: ["?! Inaccuracy", "badge-inaccuracy"],
      mistake: ["? Mistake", "badge-mistake"],
      blunder: ["?? Blunder", "badge-blunder"],
    };
    const [label, cls] = map[rating] || [rating, ""];
    return `<span class="${cls}">${label}</span>`;
  }

  async function openReviewModal() {
    if (!state || state.total_plies === 0) {
      logTerm("Play or import at least 1 move before running a review.", "term-warn");
      return;
    }
    $("review-modal").classList.remove("hidden");
    syncUrl("/review");
    const revEngName = state.stockfish_available ? "Stockfish (depth 15)" : "Titan-C";
    $("review-sub").textContent = `Analyzing ${state.total_plies} plies with ${revEngName} (Lichess Win% Accuracy + Brilliant Detection)...`;
    $("review-body").innerHTML = `<div class="empty-hint">Computing Lichess accuracy, brilliant sacrifices, turning points, and calibrated Elo...</div>`;

    const rev = await api("/api/review", "POST");
    if (!rev.ok) {
      $("review-sub").textContent = rev.error || "Review failed.";
      return;
    }

    const gradedBy = rev.engine === "stockfish" ? "Stockfish" : "Titan-C";
    $("review-sub").textContent = `${state.white_name} vs ${state.black_name} (${state.total_plies} plies • Lichess Win% Model • ${gradedBy}) — click any move or graph point to inspect`;

    const renderAccCard = (title, s) => `
      <div class="acc-card">
        <div class="acc-top">
          <strong>${title}</strong>
          <span class="acc-val">${s.accuracy.toFixed(1)}%</span>
        </div>
        <div class="elo-showcase">
          <div>
            <div class="elo-showcase-lbl">Est. Game Elo</div>
            <div class="elo-showcase-num">${s.est_elo || "—"}</div>
          </div>
          <span class="elo-tier-pill">${s.elo_tier || "Unrated"}</span>
        </div>
        <div class="acc-pills">
          <span class="term-dim">ACPL ${s.acpl}</span>
          <span class="${(s.brilliant || 0) > 0 ? "badge-brilliant" : "badge-brilliant-zero"}" title="Brilliant tactical sacrifices">!! ${s.brilliant || 0}</span>
          <span class="badge-best">&#9733; ${s.best}</span>
          <span class="badge-good">&#10003; ${s.good}</span>
          <span class="badge-inaccuracy">?! ${s.inaccuracy}</span>
          <span class="badge-mistake">? ${s.mistake}</span>
          <span class="badge-blunder">?? ${s.blunder}</span>
        </div>
      </div>`;

    let turningHtml = "";
    if (rev.turning_point) {
      const tp = rev.turning_point;
      const dot = tp.side === "b" ? "..." : ".";
      turningHtml = `
        <div class="turning-card" data-jump-ply="${tp.ply}" title="Click to jump to this turning point in Analysis Mode">
          <strong>Biggest Turning Point:</strong>
          ${tp.move_num}${dot} <strong>${tp.played}</strong>
          (${tp.eval_before.trim()} &rarr; ${tp.eval_after.trim()})
          &nbsp;|&nbsp; Engine preferred: <strong class="badge-best">${tp.best}</strong>
        </div>`;
    }

    const totBril = (rev.white.brilliant || 0) + (rev.black.brilliant || 0);
    const totBlund = (rev.white.blunder || 0) + (rev.black.blunder || 0);
    const totMist = (rev.white.mistake || 0) + (rev.black.mistake || 0);
    const totInacc = (rev.white.inaccuracy || 0) + (rev.black.inaccuracy || 0);

    const buildRowsHtml = (filter) =>
      rev.moves
        .filter((m) => filter === "all" || m.rating === filter || (filter === "errors" && (m.rating === "blunder" || m.rating === "mistake" || m.rating === "inaccuracy")))
        .map((m) => {
          const mv = `${m.move_num}${m.side === "b" ? "..." : "."}`;
          const wd = m.win_drop != null && m.win_drop >= 0.5 ? `-${m.win_drop.toFixed(1)}%` : "—";
          let sugg = "";
          if (m.rating === "brilliant") {
            sugg = `<span class="badge-brilliant">Tactical sacrifice!</span>`;
          } else if (
            (m.rating === "inaccuracy" || m.rating === "mistake" || m.rating === "blunder") &&
            m.played !== m.best
          ) {
            sugg = `Best: <strong class="badge-best">${m.best}</strong> <span class="term-dim">(-${m.cpl} cp)</span>`;
          }
          return `<tr data-jump-ply="${m.ply}">
            <td>${mv}</td>
            <td><strong>${m.played}</strong></td>
            <td>${badgeHtml(m.rating)}</td>
            <td>${m.eval_str.trim()}</td>
            <td class="term-dim">${wd}</td>
            <td>${sugg}</td>
          </tr>`;
        })
        .join("");

    $("review-body").innerHTML = `
      <div class="review-summary">
        ${renderAccCard(state.white_name, rev.white)}
        ${renderAccCard(state.black_name, rev.black)}
      </div>
      <div class="eval-chart-box" id="rev-eval-chart-box"></div>
      ${turningHtml}
      <div class="rev-filter-bar" id="rev-filter-bar">
        <button class="rev-filter-btn active" data-rf="all">All Moves (${rev.moves.length})</button>
        <button class="rev-filter-btn" data-rf="errors">Key Mistakes (${totBlund + totMist + totInacc})</button>
        <button class="rev-filter-btn" data-rf="brilliant"><span class="${totBril > 0 ? "badge-brilliant" : "badge-brilliant-zero"}">!! Brilliant (${totBril})</span></button>
        <button class="rev-filter-btn" data-rf="blunder"><span class="badge-blunder">?? Blunders (${totBlund})</span></button>
        <button class="rev-filter-btn" data-rf="mistake"><span class="badge-mistake">? Mistakes (${totMist})</span></button>
        <button class="rev-filter-btn" data-rf="inaccuracy"><span class="badge-inaccuracy">?! Inaccuracies (${totInacc})</span></button>
      </div>
      <table class="review-table">
        <thead>
          <tr><th>Move</th><th>Played</th><th>Rating</th><th>Eval</th><th>Win% &Delta;</th><th>Engine Suggestion</th></tr>
        </thead>
        <tbody id="rev-moves-tbody">${buildRowsHtml("all")}</tbody>
      </table>`;

    const jumpHandler = async (ply) => {
      $("review-modal").classList.add("hidden");
      await jumpToPlyInAnalysis(ply);
    };

    renderLichessEvalGraph($("rev-eval-chart-box"), rev.timeline, rev.moves, jumpHandler);

    const bindRowJumps = () => {
      $("review-body")
        .querySelectorAll("[data-jump-ply]")
        .forEach((el) => {
          el.addEventListener("click", async () => {
            await jumpHandler(parseInt(el.dataset.jumpPly, 10));
          });
        });
    };
    bindRowJumps();

    $("rev-filter-bar").querySelectorAll(".rev-filter-btn").forEach((btn) => {
      btn.addEventListener("click", () => {
        $("rev-filter-bar").querySelectorAll(".rev-filter-btn").forEach((b) => b.classList.remove("active"));
        btn.classList.add("active");
        $("rev-moves-tbody").innerHTML = buildRowsHtml(btn.dataset.rf) || `<tr><td colspan="6" class="empty-hint">No moves in this category.</td></tr>`;
        bindRowJumps();
      });
    });
  }

  // ==========================================================================
  // Terminal Command Processor (100% parity with CLI + new features!)
  // ==========================================================================
  async function handleTerminalCommand(raw) {
    const cmd = raw.trim();
    if (!cmd) return;

    cmdHistory.unshift(cmd);
    if (cmdHistory.length > 100) cmdHistory.pop();
    cmdHistIdx = -1;

    logTerm(`${state ? (state.turn === "w" ? "White" : "Black") : ""}> ${cmd}`, "term-cmd");

    const lower = cmd.toLowerCase();
    if (lower === "clear" || lower === "cls") {
      termOut.innerHTML = "";
      return;
    }
    if (lower === "help" || lower === "?") {
      logTerm(
        "Moves: SAN (e4, Nf3, exd5, O-O, e8=Q) or Coords (e2e4, g1f3)\n" +
          "Inspect square: e2 (highlights legal moves)\n" +
          "Draw arrows: Right-drag (Shift=green, Ctrl=red, Alt=blue); Right-click=mark square\n" +
          "Commands: analyze, import, goto <ply>, undo (u), hint, moves, flip, autoflip,\n" +
          "          eval, fen [FEN], pgn, review, new, watch, pause, step, level <1-6>, sound, clear",
        "term-dim"
      );
      return;
    }
    if (lower === "analyze" || lower === "analysis") {
      if (state.mode === "analysis") {
        await exitAnalysisToPlay();
      } else {
        await enterAnalysisMode();
      }
      return;
    }
    if (lower === "import" || lower === "import pgn") {
      await openPgnModal(false);
      return;
    }
    if (lower === "gauntlet" || lower === "elo") {
      await openGauntletModal();
      return;
    }
    if (lower.startsWith("goto ")) {
      const ply = parseInt(cmd.slice(5).trim(), 10);
      if (!isNaN(ply)) {
        await jumpToPlyInAnalysis(ply);
      }
      return;
    }
    if (lower === "undo" || lower === "u") {
      await doUndo();
      return;
    }
    if (lower === "hint") {
      await doHint();
      return;
    }
    if (lower === "flip") {
      doFlip();
      return;
    }
    if (lower === "autoflip") {
      toggleAutoFlip();
      return;
    }
    if (lower === "eval" || lower === "evalbar") {
      toggleEvalBar();
      return;
    }
    if (lower === "pgn") {
      await doPgn(true);
      return;
    }
    if (lower === "review") {
      await openReviewModal();
      return;
    }
    if (lower === "new" || lower === "reset") {
      await startNewGame();
      return;
    }
    if (lower === "watch") {
      await startNewGame("watch");
      return;
    }
    if (lower === "pause" || lower === "resume") {
      toggleWatchPause();
      return;
    }
    if (lower === "step") {
      if (state.mode === "watch") {
        watchPaused = true;
        $("btn-watch-toggle").textContent = "\u25B6 Resume";
        clearWatchTimer();
        await triggerBotMove();
      }
      return;
    }
    if (lower === "sound" || lower === "mute") {
      toggleSound();
      return;
    }
    if (lower === "fen") {
      logTerm(`FEN: ${state.fen}`, "term-info");
      return;
    }
    if (lower.startsWith("fen ")) {
      await loadFenString(cmd.slice(4).trim());
      return;
    }
    if (lower.startsWith("level ")) {
      const lvl = parseInt(cmd.slice(6).trim(), 10);
      const maxL = state.stockfish_available ? 11 : 6;
      if (lvl >= 1 && lvl <= maxL) {
        whiteLevelSel.value = String(lvl);
        blackLevelSel.value = String(lvl);
        await startNewGame(state.mode, lvl, lvl, state.fen);
        logTerm(`Difficulty set to level ${lvl}.`, "term-ok");
      } else {
        logTerm(`Level must be between 1 and ${maxL}.`, "term-err");
      }
      return;
    }
    if (lower === "moves") {
      const list = state.legal_moves.map((m) => m.san).join(", ");
      logTerm(`Legal moves (${state.legal_moves.length}): {${list}}`, "term-info");
      return;
    }

    if (busy || state.is_bot_turn) {
      logTerm("Wait for the engine to finish its move (or type 'pause' in watch mode).", "term-warn");
      return;
    }

    const res = await api("/api/move", "POST", { input: cmd });
    if (res.ok) {
      applyState(res, true);
      return;
    }

    const sqCheck = algebraicToSq(cmd);
    if (sqCheck >= 0) {
      const promoTry = state.legal_moves.filter(
        (m) => m.to === sqCheck && m.promo !== "" && (m.san.startsWith(cmd) || m.uci.endsWith(cmd + "q"))
      );
      if (promoTry.length > 0 && new Set(promoTry.map((m) => m.from)).size === 1) {
        openPromoModal(promoTry[0].from, promoTry[0].to);
        return;
      }

      const movesFrom = legalMovesFrom(sqCheck);
      if (movesFrom.length > 0) {
        selectedSq = sqCheck;
        renderBoard();
        logTerm(`Moves from ${cmd}: ${movesFrom.map((m) => m.san).join(", ")}`, "term-ok");
        return;
      }
    }

    if (cmd.length === 4) {
      const fSq = algebraicToSq(cmd.slice(0, 2));
      const tSq = algebraicToSq(cmd.slice(2, 4));
      if (fSq >= 0 && tSq >= 0) {
        const promoMoves = state.legal_moves.filter(
          (m) => m.from === fSq && m.to === tSq && m.promo !== ""
        );
        if (promoMoves.length > 0) {
          openPromoModal(fSq, tSq);
          return;
        }
      }
    }

    sfx.play("illegal");
    logTerm(res.error || `Illegal move '${cmd}'. Type 'moves' or 'help'.`, "term-err");
  }

  function toggleSound() {
    sfx.enabled = !sfx.enabled;
    $("btn-sound").innerHTML = sfx.enabled ? "&#128266;" : "&#128263;";
    logTerm(`Sound ${sfx.enabled ? "enabled" : "muted"}.`, "term-dim");
  }

  function toggleWatchPause() {
    watchPaused = !watchPaused;
    $("btn-watch-toggle").textContent = watchPaused ? "\u25B6 Resume" : "\u23F8 Pause";
    if (watchPaused) {
      clearWatchTimer();
    } else {
      scheduleBotMove();
    }
  }

  // ==========================================================================
  // Live Multithreaded Elo Arena & Precision Elo Calculator Modal
  // ==========================================================================
  let gauntletPollTimer = null;
  let gtActiveTab = "gauntlet"; // "gauntlet" | "calc"

  function setGauntletTab(tab, isRunning = false) {
    gtActiveTab = tab === "calc" ? "calc" : "gauntlet";
    $("gt-tab-gauntlet").classList.toggle("active", gtActiveTab === "gauntlet");
    $("gt-tab-calc").classList.toggle("active", gtActiveTab === "calc");
    $("gt-controls-gauntlet").classList.toggle("hidden", gtActiveTab !== "gauntlet");
    $("gt-controls-calc").classList.toggle("hidden", gtActiveTab !== "calc");
    $("gt-view-gauntlet").classList.toggle("hidden", gtActiveTab !== "gauntlet");
    $("gt-view-calc").classList.toggle("hidden", gtActiveTab !== "calc");
    $("btn-gauntlet-start").classList.toggle("hidden", isRunning || gtActiveTab !== "gauntlet");
    $("btn-calc-start").classList.toggle("hidden", isRunning || gtActiveTab !== "calc");
  }

  function renderMiniBoardHtml(boardStr) {
    let html = `<div class="mini-board">`;
    for (let row = 0; row < 8; row++) {
      for (let col = 0; col < 8; col++) {
        const rank = 7 - row;
        const file = col;
        const sq = rank * 8 + file;
        const light = (rank + file) % 2 !== 0;
        const ch = boardStr[sq];
        const piece = ch && ch !== " " ? PIECE_SVGS[ch] || "" : "";
        html += `<div class="mini-sq ${light ? "light" : "dark"}">${piece}</div>`;
      }
    }
    html += `</div>`;
    return html;
  }

  function renderGauntletStatus(gs) {
    if (!gs || !gs.ok) return;

    // Populate thread selectors once
    for (const selId of ["gt-threads", "calc-threads"]) {
      const thSel = $(selId);
      if (thSel && thSel.options.length === 0) {
        const maxC = gs.cpu_cores || 4;
        for (let c = 1; c <= maxC; c++) {
          const opt = new Option(`${c} ${c === 1 ? "thread" : "threads"}${c === maxC ? " (all cores)" : ""}`, String(c));
          thSel.add(opt);
        }
        thSel.value = String(maxC);
      }
    }

    const oppSel = $("gt-opponent");
    if (state && !state.stockfish_available) {
      oppSel.options[0].disabled = true;
      oppSel.options[0].textContent = "Stockfish (install stockfish first)";
      oppSel.value = "internal";
    }

    if (gs.running && gs.calc_mode && gtActiveTab !== "calc") {
      setGauntletTab("calc", true);
    } else if (gs.running && !gs.calc_mode && gtActiveTab !== "gauntlet") {
      setGauntletTab("gauntlet", true);
    } else {
      $("btn-gauntlet-start").classList.toggle("hidden", gs.running || gtActiveTab !== "gauntlet");
      $("btn-calc-start").classList.toggle("hidden", gs.running || gtActiveTab !== "calc");
    }
    $("btn-gauntlet-stop").classList.toggle("hidden", !gs.running);

    const pct =
      gs.scheduled_games > 0
        ? Math.min(100, (gs.completed_games / gs.scheduled_games) * 100)
        : 0;
    $("gt-progress-fill").style.width = `${pct.toFixed(1)}%`;

    if (gs.running) {
      $("gt-progress-text").textContent = `${gs.completed_games}/${gs.scheduled_games} games • ${gs.num_threads} threads • ${gs.elapsed_sec.toFixed(1)}s`;
      $("gauntlet-sub").textContent = gs.calc_mode
        ? `Adaptive Elo Calculator: ${gs.calc.subject_label} vs Stockfish • Auto-saving to ${gs.elo_file}`
        : `Running vs ${gs.opponent} (${gs.num_threads} parallel threads) • Auto-saving to ${gs.elo_file}`;
    } else if (gs.updated_at) {
      $("gt-progress-text").textContent = `Saved (${gs.updated_at})`;
      $("gauntlet-sub").textContent = `Last calibration: ${gs.opponent} • Saved in ${gs.elo_file}`;
    } else {
      $("gt-progress-text").textContent = `Ready (${gs.cpu_cores} CPU cores available)`;
    }

    $("gt-table-body").innerHTML = gs.levels
      .map((lp) => {
        const score = lp.done > 0 ? (((lp.wins + 0.5 * lp.draws) / lp.done) * 100).toFixed(1) + "%" : "—";
        const wdl = lp.done > 0 ? `${lp.wins}-${lp.draws}-${lp.losses}` : "—";
        let eloStr = "—";
        if (lp.has_elo) {
          eloStr = lp.is_relative
            ? `${lp.est_elo >= 0 ? "+" : ""}${lp.est_elo} (rel)`
            : `~${lp.est_elo} Elo`;
        }
        const stCls =
          lp.state === "running"
            ? "gt-pill-running"
            : lp.state === "done"
            ? "gt-pill-done"
            : "gt-pill-pending";
        const stTxt =
          lp.state === "running"
            ? `Running (${lp.done}/${lp.total})`
            : lp.state === "done"
            ? "Done"
            : lp.state === "stopped"
            ? "Stopped"
            : "Pending";
        const botLabel = lp.engine === "az" ? `🧠 ${lp.name}` : `Titan-C L${lp.level} ${lp.name}`;
        return `<tr>
          <td><strong>${botLabel}</strong></td>
          <td>${lp.opponent}</td>
          <td class="${stCls}">${stTxt}</td>
          <td>${wdl}</td>
          <td>${score}</td>
          <td><strong>${eloStr}</strong></td>
        </tr>`;
      })
      .join("");

    // Append the 4 Mythic Hybrid Bosses to the Arena Ladder (Locked until Zero-C v2 completes)
    if (state && state.presets) {
      const hybrids = state.presets.filter((p) => p.engine === "hybrid");
      if (hybrids.length > 0) {
        $("gt-table-body").innerHTML += hybrids
          .map((h) => {
            const locked = Boolean(h.locked || !h.available);
            const lockTxt = h.needs_distill ? "Locked (Needs Distillation)" : "Locked (Zero-C v2)";
            return `<tr class="${locked ? "gt-locked-row" : ""}">
              <td><strong>${locked ? "&#128274;" : "&#9889;"} ${h.name}</strong></td>
              <td>Stockfish (FIDE Elo)</td>
              <td class="${locked ? "gt-pill-locked" : "gt-pill-pending"}">${locked ? lockTxt : "Ready"}</td>
              <td>&mdash;</td>
              <td>&mdash;</td>
              <td><strong>${locked ? "Est. 2400–2550+" : "Unrated"}</strong></td>
            </tr>`;
          })
          .join("");
      }
    }

    // Render Precision Elo Calculator Dashboard
    const cs = gs.calc;
    if (cs && cs.active) {
      $("calc-hero-bot").textContent = cs.subject_label || "Bot Calibration";
      if (cs.games_done > 0) {
        $("calc-hero-elo").textContent = `${cs.est_elo} Elo  (±${cs.ci_half})`;
        const floorNote = cs.hit_sf_floor ? " • Below SF 1320 floor (extrapolated)" : "";
        $("calc-hero-ci").textContent = `95% Confidence Interval: ${cs.ci_low} – ${cs.ci_high} Elo (target ±${cs.target_ci})${floorNote}`;
      } else {
        $("calc-hero-elo").textContent = "Calibrating...";
        $("calc-hero-ci").textContent = `Starting adaptive Stockfish bracket at SF ${cs.current_anchor}...`;
      }
      $("calc-st-games").textContent = `${cs.games_done} / ${cs.max_games}`;
      $("calc-st-wdl").textContent = `${cs.wins} / ${cs.draws} / ${cs.losses}`;
      $("calc-st-anchor").textContent = cs.current_anchor ? `SF ${cs.current_anchor}` : "—";
      $("calc-st-status").textContent = gs.running
        ? "Bracketing..."
        : cs.converged
        ? "Converged ✓"
        : cs.games_done > 0
        ? "Complete"
        : "Ready";

      const buckets = cs.buckets || [];
      if (buckets.length > 0) {
        $("calc-buckets-body").innerHTML = buckets
          .slice()
          .sort((a, b) => a.anchor - b.anchor)
          .map((b) => {
            const n = b.wins + b.draws + b.losses;
            const sc = n > 0 ? (((b.wins + 0.5 * b.draws) / n) * 100).toFixed(1) + "%" : "—";
            return `<tr>
              <td><strong>Stockfish ${b.anchor}</strong></td>
              <td>+${b.wins} =${b.draws} -${b.losses}</td>
              <td>${sc}</td>
              <td>${n}</td>
            </tr>`;
          })
          .join("");
      } else {
        $("calc-buckets-body").innerHTML = `<tr><td colspan="4" class="empty-hint">Waiting for first batch of games to finish...</td></tr>`;
      }
    }

    const wSec = $("gt-workers-section");
    const wGrid = $("gt-workers-grid");
    if (gs.workers && gs.workers.length > 0) {
      wSec.classList.remove("hidden");
      wGrid.innerHTML = gs.workers
        .map(
          (w) => `
          <div class="worker-card" data-fen="${w.fen}">
            <div class="worker-meta">
              <span>Thread #${w.id + 1} • G${w.game_num}/${w.total_games}</span>
              <span>${w.eval_str.trim()}</span>
            </div>
            <div class="worker-players">${w.white} vs ${w.black}</div>
            ${renderMiniBoardHtml(w.board)}
            <div class="worker-meta">
              <span>Ply ${w.ply}</span>
              <span>Last: ${w.last_san || "—"}</span>
            </div>
          </div>`
        )
        .join("");

      wGrid.querySelectorAll(".worker-card[data-fen]").forEach((card) => {
        card.addEventListener("click", async () => {
          const fen = card.dataset.fen;
          closeGauntletModal();
          await loadFenString(fen);
          await enterAnalysisMode();
        });
      });
    } else {
      wSec.classList.add("hidden");
      wGrid.innerHTML = "";
    }
  }

  async function pollGauntlet() {
    const gs = await api("/api/gauntlet/status", "GET");
    renderGauntletStatus(gs);
    if (!gs.running) {
      stopGauntletPolling();
      // Refresh topbar dropdowns with newly saved Elo ratings!
      const fresh = await api("/api/state", "GET");
      if (fresh && fresh.ok) {
        state = fresh;
        populatePresets(state.presets);
        syncTopbar();
      }
    }
  }

  function startGauntletPolling() {
    stopGauntletPolling();
    gauntletPollTimer = setInterval(pollGauntlet, 450);
  }

  function stopGauntletPolling() {
    if (gauntletPollTimer) {
      clearInterval(gauntletPollTimer);
      gauntletPollTimer = null;
    }
  }

  async function openGauntletModal() {
    $("gauntlet-modal").classList.remove("hidden");
    syncUrl(gtActiveTab === "calc" ? "/elo-calculator" : "/elo-arena");
    const gs = await api("/api/gauntlet/status", "GET");
    renderGauntletStatus(gs);
    if (gs.running) {
      startGauntletPolling();
    }
  }

  function closeGauntletModal() {
    $("gauntlet-modal").classList.add("hidden");
    stopGauntletPolling();
    syncUrl(underlyingBaseRoute());
  }

  let spectateActive = false;
  let spectateTimer = null;
  let spectateGameId = -1;
  let spectateLastSeq = -1;
  let spectateLastPly = -1;
  let spectateLastFen = "";
  let spectateArrows = [];
  let spectateToastTimer = null;

  function uciSqToIdx(sqStr) {
    if (!sqStr || sqStr.length < 2) return -1;
    const f = sqStr.charCodeAt(0) - 97;
    const r = sqStr.charCodeAt(1) - 49;
    if (f < 0 || f > 7 || r < 0 || r > 7) return -1;
    return r * 8 + f;
  }

  function showSpectateToast(msg) {
    const t = $("sp-toast");
    if (!t) return;
    t.textContent = msg;
    t.classList.remove("hidden");
    if (spectateToastTimer) clearTimeout(spectateToastTimer);
    spectateToastTimer = setTimeout(() => t.classList.add("hidden"), 3500);
  }

  function renderSpectateBoard(g) {
    const bstr = g.board || "";
    const fromSq = g.last_uci && g.last_uci.length >= 4 ? uciSqToIdx(g.last_uci.slice(0, 2)) : -1;
    const toSq = g.last_uci && g.last_uci.length >= 4 ? uciSqToIdx(g.last_uci.slice(2, 4)) : -1;

    boardEl.innerHTML = "";
    for (let row = 0; row < 8; row++) {
      for (let col = 0; col < 8; col++) {
        const rank = flipped ? row : 7 - row;
        const file = flipped ? 7 - col : col;
        const sq = rank * 8 + file;
        const isLight = (rank + file) % 2 !== 0;

        const sqEl = document.createElement("div");
        sqEl.className = `square ${isLight ? "light" : "dark"}`;
        sqEl.dataset.sq = String(sq);
        if (sq === fromSq || sq === toSq) {
          sqEl.classList.add("last-move");
        }
        if (col === 0) {
          const rl = document.createElement("span");
          rl.className = "coord-rank";
          rl.textContent = String(rank + 1);
          sqEl.appendChild(rl);
        }
        if (row === 7) {
          const fl = document.createElement("span");
          fl.className = "coord-file";
          fl.textContent = String.fromCharCode(97 + file);
          sqEl.appendChild(fl);
        }
        const ch = bstr[sq];
        if (ch && ch !== " " && ch !== ".") {
          const wrap = document.createElement("div");
          wrap.className = "piece-wrap";
          wrap.innerHTML = PIECE_SVGS[ch] || "";
          sqEl.appendChild(wrap);
        }
        boardEl.appendChild(sqEl);
      }
    }

    // Top-3 live MCTS candidate arrows
    spectateArrows = [];
    for (const t of (g.top || []).slice(0, 3)) {
      if (t.uci && t.uci.length >= 4 && t.visits > 0) {
        const f = uciSqToIdx(t.uci.slice(0, 2));
        const to = uciSqToIdx(t.uci.slice(2, 4));
        if (f >= 0 && to >= 0) spectateArrows.push({ from: f, to });
      }
    }
    renderArrows();
  }

  function pickNextLiveGame(live, excludeId) {
    if (!live || live.length === 0) return null;
    const candidates = live.filter((x) => x.id !== excludeId);
    const pool = candidates.length > 0 ? candidates : live;
    // Prefer an active midgame (ply >= 10) for immediate action
    const mid = pool.filter((x) => (x.ply || 0) >= 10);
    const chosenPool = mid.length > 0 ? mid : pool;
    return chosenPool[Math.floor(Math.random() * chosenPool.length)];
  }

  function renderSpectateFrame(az) {
    if (!spectateActive || !az || !az.ok) return;
    const isRunning = Boolean(az.running && az.proc_alive !== false);
    const live = az.live_games || [];

    // Update Training HUD stats regardless of whether a game is active
    const ph = $("sp-phase");
    if (ph) {
      ph.textContent = isRunning ? (az.phase === "training" ? "FP16 TRAIN" : "SELF-PLAY") : "IDLE";
      ph.className = "az-phase-badge " + (isRunning ? (az.phase === "training" ? "az-ph-train" : "az-ph-play") : "az-ph-idle");
    }
    const hist = az.history || [];
    const lastH = hist.length > 0 ? hist[hist.length - 1] : null;
    if ($("sp-gen")) $("sp-gen").textContent = `Gen ${az.gen || 0}`;
    if ($("sp-games")) $("sp-games").textContent = `${(az.total_games || 0).toLocaleString()} (${Math.round(az.games_per_hr || 0).toLocaleString()}/hr)`;
    if ($("sp-acc")) $("sp-acc").textContent = lastH ? `${lastH.pol_acc}%` : "—";
    if ($("sp-loss")) $("sp-loss").textContent = lastH ? String(lastH.loss) : "—";
    if ($("sp-gpu")) {
      const u = az.gpu_util != null ? `${az.gpu_util}%` : "FP16";
      const tmp = az.gpu_temp != null ? ` ${az.gpu_temp}°C` : "";
      const eps = az.evals_per_sec ? ` • ${Math.round(az.evals_per_sec).toLocaleString()}/s` : "";
      $("sp-gpu").textContent = `${u}${tmp}${eps}`;
    }

    if (live.length === 0) {
      if ($("sp-game-title")) $("sp-game-title").textContent = isRunning ? "Waiting for live games..." : "Training is paused (start training in Lab)";
      return;
    }

    let g = live.find((x) => x.id === spectateGameId);
    if (!g) {
      g = pickNextLiveGame(live, -1);
      spectateGameId = g.id;
      spectateLastSeq = g.seq != null ? g.seq : -1;
      spectateLastPly = g.ply || 0;
    } else {
      const seqChanged = g.seq != null && spectateLastSeq >= 0 && g.seq !== spectateLastSeq;
      const plyReset = spectateLastPly >= 6 && (g.ply || 0) < spectateLastPly;
      if (seqChanged || plyReset) {
        const prevNum = g.id + 1;
        const nextG = pickNextLiveGame(live, g.id);
        if (nextG) {
          g = nextG;
          spectateGameId = g.id;
        }
        spectateLastSeq = g.seq != null ? g.seq : -1;
        spectateLastPly = g.ply || 0;
        showSpectateToast(`Game #${prevNum} ended — jumped to live Game #${g.id + 1} (Ply ${g.ply})`);
      } else if ((g.ply || 0) > spectateLastPly) {
        spectateLastPly = g.ply || 0;
        playSound("move");
      }
      if (g.seq != null) spectateLastSeq = g.seq;
    }

    spectateLastFen = g.fen || "";
    renderSpectateBoard(g);

    const wp = Math.round((g.w || 0) * 100);
    const dp = Math.round((g.d || 0) * 100);
    const lp = Math.max(0, 100 - wp - dp);
    const whiteShare = Math.max(4, Math.min(96, (g.w || 0) * 100 + (g.d || 0) * 50));
    $("eval-fill-white").style.height = `${whiteShare.toFixed(1)}%`;
    const q = Math.max(-0.99, Math.min(0.99, (g.w || 0) - (g.l || 0)));
    const cpEst = Math.round(290 * Math.atanh(q)) / 100;
    $("eval-label").textContent = `${cpEst >= 0 ? "+" : ""}${cpEst.toFixed(1)}`;

    const genLbl = `Zero-C v2 (Gen ${az.gen || 0})`;
    $("top-name").textContent = flipped ? `${genLbl} • White` : `${genLbl} • Black`;
    $("bottom-name").textContent = flipped ? `${genLbl} • Black` : `${genLbl} • White`;
    if (typeof applyStripAvatar === "function") {
      applyStripAvatar($("top-avatar"), genLbl, 12);
      applyStripAvatar($("bottom-avatar"), genLbl, 12);
    }
    const bTurn = g.fen && g.fen.includes(" b ");
    $("status-pill").textContent = `Live Self-Play • ${bTurn ? "Black" : "White"} thinking (${g.sims_done}/${g.sims_target})`;
    $("opening-pill").textContent = `Self-Play Game #${g.id + 1}`;
    $("meta-move").textContent = `Ply ${g.ply}${g.last_san ? ` • Last: ${g.last_san}` : ""}`;

    if ($("sp-game-title")) $("sp-game-title").textContent = `Spectating Game #${g.id + 1} • Ply ${g.ply}`;
    if ($("sp-cap-tag")) {
      $("sp-cap-tag").textContent = `${g.full_search ? "FULL" : "FAST"} ${g.sims_done}/${g.sims_target}`;
      $("sp-cap-tag").className = `az-cap-tag ${g.full_search ? "full" : "fast"}`;
    }
    if ($("sp-wdl-text")) $("sp-wdl-text").textContent = `W:${wp}% D:${dp}% B:${lp}%`;
    if ($("sp-ml-text")) $("sp-ml-text").textContent = `~${Math.round(g.moves_left || 0)} plies`;
    if ($("sp-top-moves")) {
      const tops = (g.top || [])
        .map((t, i) => {
          const cols = ["#3fb950", "#58a6ff", "#ffa657"];
          return `<span class="az-top-chip" style="border-left:3px solid ${cols[i % 3]}">${t.san} <b>${t.visits}v</b> (${Math.round(t.prior * 100)}%)</span>`;
        })
        .join("");
      $("sp-top-moves").innerHTML = tops || `<span class="az-top-chip">Searching root...</span>`;
    }
  }

  async function pollSpectateTick() {
    if (!spectateActive) return;
    const az = await api("/api/az/status", "GET");
    renderSpectateFrame(az);
  }

  async function startSpectateTraining(gameId = -1) {
    clearWatchTimer();
    closeAzModal(true);
    showBoardWorkspace(true);
    spectateActive = true;
    spectateGameId = gameId;
    spectateLastSeq = -1;
    spectateLastPly = -1;
    spectateArrows = [];
    $("spectate-hud").classList.remove("hidden");
    $("analysis-bar").classList.add("hidden");
    $("watch-bar").classList.add("hidden");
    $("puzzle-bar").classList.add("hidden");
    syncUrl(gameId >= 0 ? `/spectate/${gameId + 1}` : "/spectate");
    if (spectateTimer) clearInterval(spectateTimer);
    await pollSpectateTick();
    spectateTimer = setInterval(pollSpectateTick, 700);
    logTerm("Entered Live Self-Play Spectate Mode (auto-advances when games finish).", "term-ok");
  }

  function stopSpectateTraining(restoreBoard = false) {
    if (!spectateActive) return;
    spectateActive = false;
    spectateArrows = [];
    if (spectateTimer) {
      clearInterval(spectateTimer);
      spectateTimer = null;
    }
    if ($("spectate-hud")) $("spectate-hud").classList.add("hidden");
    if (restoreBoard && state) {
      applyState(state, false);
      syncUrl(underlyingBaseRoute());
    }
  }

  // ==========================================================================
  // Zero-C AlphaZero Self-Play Training Studio Modal (v2 Dynamic Mission Control)
  // ==========================================================================
  let azPollTimer = null;
  let azChartWindow = 0; // 0 = full run, 250 = last 250, 60 = last 60
  let azShowEma = true;
  let azLastData = null;
  let azConfigHydrated = false;
  let azLastChartKey = "";
  let azChartDeferred = false;

  function setSelectIfOptionExists(sel, val) {
    if (!sel || val == null) return;
    const str = String(val);
    if ([...sel.options].some((o) => o.value === str)) {
      sel.value = str;
    }
  }

  function computeEma(vals, alpha = 0.22) {
    if (!vals.length) return [];
    const out = [vals[0]];
    for (let i = 1; i < vals.length; i++) {
      out.push(alpha * vals[i] + (1 - alpha) * out[i - 1]);
    }
    return out;
  }

  function dynamicBounds(vals, { clampMin = null, clampMax = null, minSpan = 0.05 } = {}) {
    const clean = vals.filter((v) => v != null && Number.isFinite(v));
    if (!clean.length) return [0, 1];
    let lo = Math.min(...clean);
    let hi = Math.max(...clean);
    if (hi - lo < minSpan) {
      const mid = (hi + lo) / 2;
      lo = mid - minSpan / 2;
      hi = mid + minSpan / 2;
    } else {
      const pad = (hi - lo) * 0.14;
      lo -= pad;
      hi += pad;
    }
    if (clampMin != null && lo < clampMin) lo = clampMin;
    if (clampMax != null && hi > clampMax) hi = clampMax;
    if (hi <= lo) hi = lo + minSpan;
    return [lo, hi];
  }

  function fmtAxisVal(v, fmt) {
    if (fmt === "lr") return v < 0.001 ? v.toExponential(1) : v.toFixed(4);
    if (fmt === "%") return `${Math.round(v)}%`;
    if (fmt === "int") return `${Math.round(v)}`;
    if (fmt === "2f") return v.toFixed(2);
    if (fmt === "3f") return v.toFixed(3);
    return Math.abs(v) >= 10 ? v.toFixed(1) : v.toFixed(2);
  }

  function renderDynamicChart(containerEl, gens, seriesList, opts = {}) {
    const n = gens.length;
    if (n < 1) {
      containerEl.innerHTML = `<div class="empty-hint">${opts.emptyText || "Waiting for Gen 1..."}</div>`;
      return;
    }
    const hasRight = seriesList.some((s) => s.axis === "right" && s.vals.some((v) => v != null));
    const W = 490, H = 162, padL = 44, padR = hasRight ? 46 : 16, padT = 14, padB = 24;
    const pw = W - padL - padR, ph = H - padT - padB;

    const leftSeries = seriesList.filter((s) => s.axis !== "right");
    const rightSeries = seriesList.filter((s) => s.axis === "right");

    const [lMin, lMax] = dynamicBounds(
      leftSeries.flatMap((s) => s.vals),
      opts.leftBounds || { clampMin: 0 }
    );
    const [rMin, rMax] = hasRight
      ? dynamicBounds(rightSeries.flatMap((s) => s.vals), opts.rightBounds || { clampMin: 0 })
      : [0, 1];

    const sx = (i) => padL + (n === 1 ? pw / 2 : (i / (n - 1)) * pw);
    const syL = (v) => padT + ph - ((Math.max(lMin, Math.min(lMax, v)) - lMin) / Math.max(1e-9, lMax - lMin)) * ph;
    const syR = (v) => padT + ph - ((Math.max(rMin, Math.min(rMax, v)) - rMin) / Math.max(1e-9, rMax - rMin)) * ph;

    const gradId = `azg_${containerEl.id}_${Math.floor(Math.random() * 1e6)}`;
    const primaryColor = (leftSeries[0] && leftSeries[0].color) || "#58a6ff";
    let defs = `<defs><linearGradient id="${gradId}" x1="0" y1="0" x2="0" y2="1"><stop offset="0%" stop-color="${primaryColor}" stop-opacity="0.22"/><stop offset="100%" stop-color="${primaryColor}" stop-opacity="0.01"/></linearGradient></defs>`;

    // Horizontal grid + Left/Right dynamic Y ticks (4 intervals)
    let grid = "";
    const leftColor = (leftSeries[0] && leftSeries[0].color) || "#8b949e";
    const rightColor = (rightSeries[0] && rightSeries[0].color) || "#8b949e";
    for (let g = 0; g <= 3; g++) {
      const frac = g / 3;
      const y = (padT + ph - frac * ph).toFixed(1);
      grid += `<line x1="${padL}" y1="${y}" x2="${W - padR}" y2="${y}" stroke="rgba(139,148,158,0.16)" stroke-dasharray="${g === 0 ? "" : "3,3"}"/>`;
      const lv = lMin + (lMax - lMin) * frac;
      grid += `<text x="${padL - 6}" y="${Number(y) + 3}" fill="${leftSeries.length === 1 ? leftColor : "#9ea7b3"}" font-size="9.5" text-anchor="end">${fmtAxisVal(lv, opts.leftFmt || "2f")}</text>`;
      if (hasRight) {
        const rv = rMin + (rMax - rMin) * frac;
        grid += `<text x="${W - padR + 6}" y="${Number(y) + 3}" fill="${rightColor}" font-size="9.5" text-anchor="start">${fmtAxisVal(rv, opts.rightFmt || "2f")}</text>`;
      }
    }

    // X-axis Gen ticks
    const xTickIdxs = n === 1 ? [0] : n === 2 ? [0, 1] : [0, Math.floor((n - 1) / 2), n - 1];
    for (const idx of xTickIdxs) {
      const x = sx(idx).toFixed(1);
      grid += `<text x="${x}" y="${H - 6}" fill="#8b949e" font-size="9" text-anchor="middle">Gen ${gens[idx]}</text>`;
    }

    // Series polylines + optional EMA trendline + live latest value dot
    let paths = "";
    for (let si = 0; si < seriesList.length; si++) {
      const s = seriesList[si];
      const sy = s.axis === "right" ? syR : syL;
      const validPts = [];
      for (let i = 0; i < n; i++) {
        if (s.vals[i] != null && Number.isFinite(s.vals[i])) {
          validPts.push([i, s.vals[i]]);
        }
      }
      if (!validPts.length) continue;

      const rawStr = validPts.map(([i, v]) => `${sx(i).toFixed(1)},${sy(v).toFixed(1)}`).join(" ");
      // Area fill under first left series
      if (si === 0 && validPts.length > 1) {
        const firstX = sx(validPts[0][0]).toFixed(1);
        const lastX = sx(validPts[validPts.length - 1][0]).toFixed(1);
        const baseY = (padT + ph).toFixed(1);
        paths += `<polygon fill="url(#${gradId})" points="${firstX},${baseY} ${rawStr} ${lastX},${baseY}"/>`;
      }

      const dashAttr = s.dashed ? ` stroke-dasharray="5,3"` : "";
      if (validPts.length === 1) {
        const [i0, v0] = validPts[0];
        paths += `<circle cx="${sx(i0).toFixed(1)}" cy="${sy(v0).toFixed(1)}" r="4" fill="${s.color}"/>`;
      } else if (azShowEma && !s.noEma && validPts.length >= 6) {
        paths += `<polyline fill="none" stroke="${s.color}" stroke-opacity="0.30" stroke-width="1.3"${dashAttr} points="${rawStr}"/>`;
        const emaVals = computeEma(validPts.map((p) => p[1]), validPts.length > 80 ? 0.14 : 0.25);
        const emaStr = validPts.map(([i], k) => `${sx(i).toFixed(1)},${sy(emaVals[k]).toFixed(1)}`).join(" ");
        paths += `<polyline fill="none" stroke="${s.color}" stroke-width="2.2" stroke-linejoin="round"${dashAttr} points="${emaStr}"/>`;
      } else {
        paths += `<polyline fill="none" stroke="${s.color}" stroke-width="2" stroke-linejoin="round"${dashAttr} points="${rawStr}"/>`;
      }

      const [li, lv] = validPts[validPts.length - 1];
      paths += `<circle cx="${sx(li).toFixed(1)}" cy="${sy(lv).toFixed(1)}" r="3.4" fill="${s.color}" stroke="#0d1117" stroke-width="1.2"/>`;
    }

    containerEl.innerHTML =
      `<svg viewBox="0 0 ${W} ${H}" class="az-svg">${defs}${grid}${paths}` +
      `<line class="az-hover-line" x1="0" y1="${padT}" x2="0" y2="${padT + ph}" stroke="rgba(255,255,255,0.35)" stroke-dasharray="2,2" style="display:none"/>` +
      `</svg><div class="az-tooltip hidden"></div>`;

    // Interactive Hover Crosshair + Exact Value Tooltip
    const svg = containerEl.querySelector("svg");
    const hoverLine = containerEl.querySelector(".az-hover-line");
    const tip = containerEl.querySelector(".az-tooltip");
    svg.addEventListener("mousemove", (ev) => {
      containerEl.dataset.hovering = "1";
      const rect = svg.getBoundingClientRect();
      const relX = ((ev.clientX - rect.left) / Math.max(1, rect.width)) * W;
      const frac = Math.max(0, Math.min(1, (relX - padL) / Math.max(1, pw)));
      const idx = n === 1 ? 0 : Math.round(frac * (n - 1));
      const xPos = sx(idx);
      hoverLine.setAttribute("x1", xPos.toFixed(1));
      hoverLine.setAttribute("x2", xPos.toFixed(1));
      hoverLine.style.display = "block";
      const rows = seriesList
        .map((s) => {
          const v = s.vals[idx];
          if (v == null || !Number.isFinite(v)) return "";
          return `<div><span style="color:${s.color}">&#9679;</span> ${s.label}: <b>${fmtAxisVal(v, s.fmt || "2f")}</b></div>`;
        })
        .filter(Boolean)
        .join("");
      tip.innerHTML = `<div style="color:#8b949e;margin-bottom:2px;">Gen ${gens[idx]}</div>${rows}`;
      tip.classList.remove("hidden");
      const px = (xPos / W) * rect.width;
      const onRight = px > rect.width * 0.58;
      tip.style.left = onRight ? `${Math.max(4, px - tip.offsetWidth - 10)}px` : `${Math.min(rect.width - tip.offsetWidth - 4, px + 10)}px`;
      tip.style.top = "8px";
    });
    svg.addEventListener("mouseleave", () => {
      containerEl.dataset.hovering = "0";
      hoverLine.style.display = "none";
      tip.classList.add("hidden");
      if (azChartDeferred && azLastData) {
        azChartDeferred = false;
        updateAzChartsOnly(azLastData, true);
      }
    });
  }

  function updateAzChartsOnly(az, force = false) {
    if (!az) return;
    const allHist = az.history || [];
    const lastGen = allHist.length > 0 ? (allHist[allHist.length - 1].gen || allHist.length) : 0;
    const key = `${allHist.length}:${lastGen}:${azChartWindow}:${azShowEma ? 1 : 0}`;
    if (!force && key === azLastChartKey) {
      return; // Nothing changed — keep existing DOM & hover tooltip intact!
    }
    // If the user is actively scrubbing any chart when a new gen ticks, defer redraw until mouseleave!
    const chartEls = [$("az-policy-chart"), $("az-value-chart"), $("az-dynamics-chart"), $("az-balance-chart")];
    if (!force && chartEls.some((el) => el && el.dataset.hovering === "1")) {
      azChartDeferred = true;
      return;
    }
    azLastChartKey = key;
    azChartDeferred = false;

    const hist = azChartWindow > 0 && allHist.length > azChartWindow
      ? allHist.slice(-azChartWindow)
      : allHist;

    if (hist.length === 0) {
      $("az-policy-chart").innerHTML = `<div class="empty-hint">Policy curves appear after Gen 1 completes.</div>`;
      $("az-value-chart").innerHTML = `<div class="empty-hint">Value &amp; Moves-Left curves appear after Gen 1 completes.</div>`;
      $("az-dynamics-chart").innerHTML = `<div class="empty-hint">Game dynamics appear after Gen 1 completes.</div>`;
      $("az-balance-chart").innerHTML = `<div class="empty-hint">Color balance &amp; LR schedule appear after Gen 1 completes.</div>`;
      return;
    }

    const gens = hist.map((h, i) => h.gen || (i + 1));

    // Chart 1: Policy Loss (L) + Policy Top-1 Accuracy % (R)
    renderDynamicChart(
      $("az-policy-chart"),
      gens,
      [
        { label: "Policy Loss", vals: hist.map((h) => h.pol_loss), color: "#58a6ff", axis: "left", fmt: "2f" },
        { label: "Top-1 Acc", vals: hist.map((h) => h.pol_acc), color: "#a371f7", axis: "right", fmt: "%" },
      ],
      {
        leftFmt: "2f",
        rightFmt: "%",
        leftBounds: { clampMin: 0, minSpan: 0.25 },
        rightBounds: { clampMin: 0, clampMax: 100, minSpan: 6 },
      }
    );

    // Chart 2: WDL Loss (L) + Moves-Left Loss (R)
    renderDynamicChart(
      $("az-value-chart"),
      gens,
      [
        { label: "WDL Loss", vals: hist.map((h) => h.wdl_loss), color: "#3fb950", axis: "left", fmt: "3f" },
        { label: "Moves-Left", vals: hist.map((h) => h.ml_loss), color: "#39c5cf", axis: "right", fmt: "3f" },
      ],
      {
        leftFmt: "2f",
        rightFmt: "3f",
        leftBounds: { clampMin: 0, minSpan: 0.08 },
        rightBounds: { clampMin: 0, minSpan: 0.02 },
      }
    );

    // Chart 3: Decisive % (L) + Avg Game Length in Plies (R)
    renderDynamicChart(
      $("az-dynamics-chart"),
      gens,
      [
        { label: "Decisive", vals: hist.map((h) => h.decisive_pct), color: "#e3b341", axis: "left", fmt: "%" },
        { label: "Avg Plies", vals: hist.map((h) => h.avg_plies), color: "#ffa657", axis: "right", fmt: "int" },
      ],
      {
        leftFmt: "%",
        rightFmt: "int",
        leftBounds: { clampMin: 0, clampMax: 100, minSpan: 10 },
        rightBounds: { clampMin: 0, minSpan: 16 },
      }
    );

    // Chart 4: White/Black Win % (L) + Active Learning Rate (R)
    const lrVals = hist.map((h) => (h.lr != null ? h.lr : az.lr != null ? az.lr : null));
    const wVals = hist.map((h) => (h.white_pct != null ? h.white_pct : h.decisive_pct != null ? h.decisive_pct * 0.51 : null));
    const bVals = hist.map((h) => (h.black_pct != null ? h.black_pct : h.decisive_pct != null ? h.decisive_pct * 0.49 : null));
    renderDynamicChart(
      $("az-balance-chart"),
      gens,
      [
        { label: "White Win %", vals: wVals, color: "#f0f6fc", axis: "left", fmt: "%" },
        { label: "Black Win %", vals: bVals, color: "#8b949e", axis: "left", fmt: "%" },
        { label: "Learning Rate", vals: lrVals, color: "#f778ba", axis: "right", fmt: "lr", dashed: true, noEma: true },
      ],
      {
        leftFmt: "%",
        rightFmt: "lr",
        leftBounds: { clampMin: 0, clampMax: 100, minSpan: 12 },
        rightBounds: { clampMin: 0, minSpan: 0.0001 },
      }
    );
  }

  function renderAzStatus(az) {
    if (!az || !az.ok) return;
    azLastData = az;
    const isRunning = Boolean(az.running && az.proc_alive !== false);

    $("btn-az-start").classList.toggle("hidden", isRunning);
    if ($("btn-az-throttle")) $("btn-az-throttle").classList.toggle("hidden", isRunning);
    $("btn-az-stop").classList.toggle("hidden", !isRunning);
    const badge = $("az-phase-badge");
    badge.textContent = isRunning ? (az.phase === "training" ? "FP16 TRAINING" : "SELF-PLAY") : "IDLE";
    badge.className = "az-phase-badge " + (isRunning ? (az.phase === "training" ? "az-ph-train" : "az-ph-play") : "az-ph-idle");

    // Persist & display active session training settings (and lock dropdowns while running!)
    const cfg = az.config || null;
    const archSel = $("az-arch");
    const gamesSel = $("az-games");
    const playSel = $("az-playouts");
    const gpgSel = $("az-gpg");

    if (cfg && (isRunning || !azConfigHydrated)) {
      setSelectIfOptionExists(archSel, cfg.arch);
      setSelectIfOptionExists(gamesSel, cfg.games);
      if (cfg.fast_sims != null && cfg.full_sims != null) {
        setSelectIfOptionExists(playSel, `${cfg.fast_sims},${cfg.full_sims}`);
      }
      setSelectIfOptionExists(gpgSel, cfg.games_per_gen);
      azConfigHydrated = true;
    }
    // Lock all 4 controls while actively training; also lock architecture once a run has started
    if (archSel) archSel.disabled = isRunning || (az.gen > 0);
    if (gamesSel) gamesSel.disabled = isRunning;
    if (playSel) playSel.disabled = isRunning;
    if (gpgSel) gpgSel.disabled = isRunning;

    const sessStrip = $("az-session-strip");
    const sessTxt = $("az-session-text");
    if (sessStrip && sessTxt) {
      if (cfg && (isRunning || (az.gen || 0) > 0)) {
        const repMaxM = ((az.replay_max || 1200000) / 1e6).toFixed(1);
        const modePrefix = isRunning
          ? (cfg.full_throttle ? "🔥 FULL THROTTLE Active:" : "Active Session:")
          : "Last Session Config:";
        sessStrip.classList.remove("hidden");
        sessStrip.classList.toggle("paused", !isRunning);
        sessTxt.innerHTML =
          `<strong>${modePrefix}</strong> ${az.arch || cfg.arch} &bull; ` +
          `<strong>${cfg.games}</strong> parallel games &bull; ` +
          `<strong>${cfg.fast_sims}/${cfg.full_sims}</strong> playout cap &bull; ` +
          `<strong>${cfg.games_per_gen}</strong> games/gen &bull; ` +
          `<strong>${repMaxM}M</strong> replay window` +
          (isRunning ? " <span class=\"term-dim\">(locked while training — stop to adjust)</span>" : "");
      } else {
        sessStrip.classList.add("hidden");
      }
    }

    const hwParts = [`${az.device || "CPU"}${az.fp16 ? " • FP16" : ""}`];
    if (az.gpu_util != null) {
      hwParts.push(`GPU ${az.gpu_util}%${az.gpu_temp != null ? ` (${az.gpu_temp}°C)` : ""}`);
    }
    if (az.mcts_threads) {
      hwParts.push(`${az.mcts_threads}T C MCTS`);
    }
    $("az-hw-chip").textContent = hwParts.join(" • ");
    if (!az.venv_ready) {
      $("az-sub").textContent = "PyTorch CUDA environment not set up yet — run `make az-setup` in terminal, then click Start Training.";
    } else if (az.message) {
      $("az-sub").textContent = az.message;
    }

    const gpg = az.games_per_gen || 128;
    const pct = isRunning ? Math.min(100, ((az.gen_games || 0) / gpg) * 100) : (az.gen > 0 ? 100 : 0);
    $("az-progress-fill").style.width = `${pct.toFixed(1)}%`;
    $("az-progress-text").textContent = isRunning
      ? `Gen ${az.gen}: ${az.gen_games || 0}/${gpg} games • ${az.elapsed_sec || 0}s`
      : az.gen > 0
      ? `Saved at Gen ${az.gen} (${(az.total_games || 0).toLocaleString()} total games)`
      : "Ready — click Start Training to launch Zero-C v2 (Pro 10×128)";

    $("az-m-gen").textContent = `Gen ${az.gen || 0}`;
    $("az-m-arch").textContent = `${az.arch || "10x128 SE-ResNet"}${az.params ? ` • ${(az.params / 1e6).toFixed(2)}M` : ""}`;
    $("az-m-games").textContent = (az.total_games || 0).toLocaleString();
    $("az-m-gph").textContent = `${Math.round(az.games_per_hr || 0).toLocaleString()} games/hr`;

    const repSize = az.replay_size || 0;
    const repMax = az.replay_max || 1200000;
    const repPct = Math.min(100, (repSize / Math.max(1, repMax)) * 100);
    $("az-m-pos").textContent = (az.total_positions || 0).toLocaleString();
    $("az-m-replay").textContent = `Buffer: ${repSize.toLocaleString()} / ${(repMax / 1e6).toFixed(1)}M (${repPct.toFixed(0)}%)`;
    if ($("az-replay-fill")) $("az-replay-fill").style.width = `${repPct.toFixed(1)}%`;

    $("az-m-eps").textContent = `${Math.round(az.evals_per_sec || 0).toLocaleString()} /s`;
    const thrSub = [];
    if (az.avg_batch) thrSub.push(`Batch ~${Math.round(az.avg_batch)}`);
    thrSub.push(`${az.vram_mb || 0} MB`);
    if (az.cpu_pct) thrSub.push(`CPU ${Math.round(az.cpu_pct)}%`);
    $("az-m-vram").textContent = thrSub.join(" • ");

    const hist = az.history || [];
    const lastH = hist.length > 0 ? hist[hist.length - 1] : null;
    const prevIdx = Math.max(0, hist.length - 21);
    const prevH = hist.length >= 4 ? hist[prevIdx] : null;

    $("az-m-acc").textContent = lastH ? `${lastH.pol_acc}%` : "—";
    if (lastH && prevH) {
      const dAcc = lastH.pol_acc - prevH.pol_acc;
      const arrow = dAcc >= 0 ? "▲" : "▼";
      const clr = dAcc >= 0 ? "#56d364" : "#f85149";
      $("az-m-acc-delta").innerHTML = `<span style="color:${clr}">${arrow} ${dAcc >= 0 ? "+" : ""}${dAcc.toFixed(1)}%</span> vs Gen ${prevH.gen}`;
    } else {
      $("az-m-acc-delta").textContent = "Trend: gathering data...";
    }

    $("az-m-loss-big").textContent = lastH ? String(lastH.loss) : "—";
    $("az-m-loss").textContent = lastH ? `Pol ${lastH.pol_loss} • WDL ${lastH.wdl_loss}` : "Pol: — • WDL: —";

    const curLr = az.lr != null ? az.lr : lastH && lastH.lr != null ? lastH.lr : null;
    const baseLr = az.base_lr || 0.0015;
    $("az-m-lr").textContent = curLr != null ? (curLr < 0.001 ? curLr.toExponential(2) : curLr.toFixed(5)) : "—";
    if (curLr != null) {
      const lrPct = Math.round((curLr / baseLr) * 100);
      $("az-m-lr-sub").textContent = `${lrPct}% of peak (Cosine)`;
    } else {
      $("az-m-lr-sub").textContent = "Cosine warmup + floor";
    }

    const oc = az.outcomes || { white: 0, draw: 0, black: 0 };
    const ocTot = oc.white + oc.draw + oc.black;
    const decPct = ocTot > 0 ? `${Math.round(((oc.white + oc.black) / ocTot) * 100)}%` : "—";
    const avgP = lastH && lastH.avg_plies ? `${Math.round(lastH.avg_plies)}p` : "—";
    $("az-m-wdl").textContent = `${oc.white} / ${oc.draw} / ${oc.black}`;
    $("az-m-dec").textContent = `Decisive ${decPct} • Avg ${avgP}`;

    // Mastery Milestones & Live Health Diagnosis
    const tg = az.total_games || 0;
    const pa = lastH ? lastH.pol_acc : 0;
    const msDone = [
      tg >= 2000 || pa >= 22,
      tg >= 15000 && pa >= 35,
      tg >= 60000 && pa >= 48,
      tg >= 150000 && pa >= 55,
      tg >= 300000 && pa >= 60,
      tg >= 750000 && pa >= 66,
    ];
    let firstPending = msDone.findIndex((d) => !d);
    if (firstPending < 0) firstPending = msDone.length;
    document.querySelectorAll("#az-milestones-track .az-ms-step").forEach((el, idx) => {
      el.classList.toggle("done", msDone[idx]);
      el.classList.toggle("active", idx === firstPending && (isRunning || tg > 0));
    });

    const hp = $("az-health-pill");
    if (hp) {
      if (!isRunning && tg === 0) {
        hp.className = "az-health-pill";
        hp.textContent = "Ready — Pro 10×128 v2 pipeline armed";
      } else if (hist.length < 3) {
        hp.className = "az-health-pill";
        hp.textContent = "Warming up — building initial self-play replay memory";
      } else if (lastH && prevH && lastH.pol_acc - prevH.pol_acc >= 0.5) {
        hp.className = "az-health-pill";
        hp.textContent = `Healthy learning — Policy Acc climbing +${(lastH.pol_acc - prevH.pol_acc).toFixed(1)}%`;
      } else if (lastH && prevH && lastH.pol_loss < prevH.pol_loss) {
        hp.className = "az-health-pill";
        hp.textContent = `Steady refinement — Policy Loss falling (${prevH.pol_loss} → ${lastH.pol_loss})`;
      } else {
        hp.className = "az-health-pill warn";
        hp.textContent = "Consolidating — replay buffer stabilizing; watch head-to-head vs v1";
      }
    }

    // Dynamic 2x2 Charts
    updateAzChartsOnly(az);

    // Live Self-Play Mini-Boards
    const live = az.live_games || [];
    const liveGrid = $("az-live-grid");
    if (live.length > 0) {
      liveGrid.innerHTML = live
        .map((g) => {
          const wp = Math.round((g.w || 0) * 100);
          const dp = Math.round((g.d || 0) * 100);
          const lp = Math.max(0, 100 - wp - dp);
          const tops = (g.top || [])
            .map((t) => `<span class="az-top-chip">${t.san} <b>${t.visits}</b> (${Math.round(t.prior * 100)}%)</span>`)
            .join("");
          return `<div class="worker-card az-live-card" data-fen="${g.fen}" data-gid="${g.id}" title="Click to Spectate Game #${g.id + 1} live on the big board">
            <div class="worker-meta">
              <span>Game #${g.id + 1} &bull; Ply ${g.ply}</span>
              <span class="az-cap-tag ${g.full_search ? "full" : "fast"}">${g.full_search ? "FULL" : "FAST"} ${g.sims_done}/${g.sims_target}</span>
            </div>
            <div class="az-wdl-bar" title="White ${wp}% / Draw ${dp}% / Black ${lp}%">
              <div class="wdl-w" style="width:${wp}%"></div>
              <div class="wdl-d" style="width:${dp}%"></div>
              <div class="wdl-l" style="width:${lp}%"></div>
            </div>
            ${renderMiniBoardHtml(g.board)}
            <div class="worker-meta">
              <span>W:${wp}% D:${dp}% B:${lp}%</span>
              <span>ML ~${Math.round(g.moves_left || 0)}</span>
            </div>
            <div class="az-top-moves">${tops || '<span class="az-top-chip">Searching...</span>'}</div>
          </div>`;
        })
        .join("");
      liveGrid.querySelectorAll(".worker-card[data-gid]").forEach((card) => {
        card.addEventListener("click", async () => {
          const gid = parseInt(card.dataset.gid, 10);
          await startSpectateTraining(Number.isFinite(gid) ? gid : -1);
        });
      });
    } else {
      liveGrid.innerHTML = `<div class="empty-hint">Start training to watch live concurrent self-play games, WDL bars &amp; MCTS visit candidates stream in real time.</div>`;
    }

    // Checkpoint League Table
    const ckpts = az.checkpoints || [];
    const tbody = $("az-ckpt-tbody");
    if (ckpts.length > 0) {
      tbody.innerHTML = [...ckpts]
        .reverse()
        .map(
          (c) => `<tr>
            <td><strong>Gen ${c.gen}</strong></td>
            <td>${c.games}</td>
            <td>${(c.positions || 0).toLocaleString()}</td>
            <td>${c.loss != null ? c.loss : "—"}</td>
            <td>${c.pol_acc != null ? c.pol_acc + "%" : "—"}</td>
            <td>${c.time || "—"}</td>
            <td><button class="btn btn-sm btn-magenta az-play-gen-btn" data-ckpt="data/az/checkpoints/${c.file}">&#9876; Play</button></td>
          </tr>`
        )
        .join("");
      tbody.querySelectorAll(".az-play-gen-btn").forEach((btn) => {
        btn.addEventListener("click", async () => {
          const ck = btn.dataset.ckpt;
          closeAzModal();
          showBoardWorkspace();
          await startNewGame("white", 12, 12, "", 0, ck);
        });
      });
    } else {
      tbody.innerHTML = `<tr><td colspan="7" class="empty-hint">No v2 checkpoints saved yet (Zero-C v1 Gen 4320 is immortalized in all bot dropdowns!).</td></tr>`;
    }

    // Update Home Card 7 live summary too!
    if ($("home-az-pill")) {
      const compDone = Boolean(az.hybrids_unlocked || (az.completion && az.completion.is_complete));
      $("home-az-pill").textContent = compDone
        ? `✅ Complete • Gen ${az.gen || 0}`
        : `${isRunning ? "Training" : "Idle"} • Gen ${az.gen || 0}`;
      $("home-az-pill").classList.toggle("active", isRunning || compDone);
      $("home-az-gen").textContent = String(az.gen || 0);
      $("home-az-games").textContent = String(az.total_games || 0);
      $("home-az-pos").textContent = (az.total_positions || 0).toLocaleString();
      $("home-az-dev").textContent = az.device || "Ready";
    }

    renderAzCompletion(az, isRunning);
  }

  function computeClientCompletion(az) {
    const gen = az.gen || 0;
    const totalGames = az.total_games || 0;
    const baseLr = az.base_lr || 0.0015;
    const hist = az.history || [];
    const lastH = hist.length > 0 ? hist[hist.length - 1] : null;
    const curLr = az.lr != null ? az.lr : (lastH && lastH.lr != null ? lastH.lr : baseLr);
    const MIN_GAMES = 500000;
    const SCHEDULE_END_GEN = 5500;
    const LR_TAIL_THRESH = Math.max(0.00026, baseLr * 0.175);
    const WINDOW = 50;
    const STRIDE = 150;
    const flagExists = Boolean(az.hybrids_unlocked);

    const g1Pass = totalGames >= MIN_GAMES;
    const g1Pct = Math.min(1, totalGames / MIN_GAMES);
    const g2Pass = curLr <= LR_TAIL_THRESH || gen >= SCHEDULE_END_GEN;
    const lrStart = Math.max(baseLr * 0.4, LR_TAIL_THRESH + 1e-6);
    const g2Pct = g2Pass ? 1 : Math.max(0, Math.min(1, (lrStart - curLr) / Math.max(1e-6, lrStart - LR_TAIL_THRESH)));

    let polDrop = null, accGain = null, mlDrop = null, avgPlies = null;
    const hasWin = hist.length >= STRIDE + WINDOW;
    if (hasWin) {
      const recent = hist.slice(-WINDOW);
      const older = hist.slice(-(STRIDE + WINDOW), -STRIDE);
      const mean = (arr, k) => arr.reduce((s, x) => s + (Number(x[k]) || 0), 0) / Math.max(1, arr.length);
      polDrop = Number((mean(older, "pol_loss") - mean(recent, "pol_loss")).toFixed(4));
      accGain = Number((mean(recent, "pol_acc") - mean(older, "pol_acc")).toFixed(2));
      mlDrop = Number((mean(older, "ml_loss") - mean(recent, "ml_loss")).toFixed(4));
      avgPlies = Number(mean(recent, "avg_plies").toFixed(1));
    }

    const g3Pass = Boolean(hasWin && polDrop < 0.015 && accGain < 0.4);
    const g4Pass = Boolean(hasWin && mlDrop < 0.003 && avgPlies >= 85);
    const pathA = gen >= SCHEDULE_END_GEN && g1Pass;
    const pathB = g1Pass && g2Pass && g3Pass && g4Pass;
    const convergedNow = pathA || pathB;
    const isComplete = flagExists || convergedNow;

    let pct = 100;
    if (!isComplete) {
      const schedPct = Math.min(1, gen / SCHEDULE_END_GEN);
      const platBonus = (g3Pass ? 0.1 : 0) + (g4Pass ? 0.05 : 0);
      pct = Number(Math.min(99, 100 * (0.45 * g1Pct + 0.4 * Math.max(g2Pct, schedPct) + platBonus)).toFixed(1));
    }
    const status = isComplete ? "complete" : g1Pass && (g2Pass || g3Pass) ? "nearing" : "training";
    const reason = isComplete
      ? flagExists
        ? "Zero-C v2 graduated — all 4 Mythic Hybrid Bosses unlocked!"
        : `Convergence criteria satisfied at Gen ${gen.toLocaleString()}!`
      : status === "nearing"
      ? "Past 500k games and entering final fine-tune/plateau window."
      : "Actively climbing — network is still absorbing new self-play discoveries.";

    return {
      status,
      is_complete: isComplete,
      converged_now: convergedNow,
      flag_exists: flagExists,
      progress_pct: pct,
      reason,
      gates: {
        experience: { passed: g1Pass, current: totalGames, target: MIN_GAMES },
        lr_anneal: { passed: g2Pass, current_lr: Number(curLr.toFixed(6)), target_lr: Number(LR_TAIL_THRESH.toFixed(6)), gen, schedule_end_gen: SCHEDULE_END_GEN },
        policy_plateau: { passed: g3Pass, ready: hasWin, pol_loss_drop_150g: polDrop, pol_acc_gain_150g: accGain },
        endgame_stability: { passed: g4Pass, ready: hasWin, ml_loss_drop_150g: mlDrop, avg_plies: avgPlies },
      },
    };
  }

  let azWasCompleteToastShown = false;

  function renderAzCompletion(az, isRunning) {
    const panel = $("az-completion-panel");
    if (!panel) return;
    const comp = az.completion ? { ...az.completion } : computeClientCompletion(az);
    if (az.hybrids_unlocked) {
      comp.is_complete = true;
      comp.status = "complete";
      comp.progress_pct = 100;
    }

    panel.classList.remove("nearing", "complete");
    if (comp.status === "complete") panel.classList.add("complete");
    else if (comp.status === "nearing") panel.classList.add("nearing");

    const badge = $("az-comp-badge");
    const nnueDone = Boolean(az.nnue_distilled);
    if (badge) {
      badge.innerHTML =
        comp.status === "complete"
          ? nnueDone
            ? "&#9989; TRAINING COMPLETE &bull; ALL 4 HYBRIDS UNLOCKED"
            : "&#9989; ZERO-C v2 COMPLETE &bull; 3/4 HYBRIDS UNLOCKED"
          : comp.status === "nearing"
          ? "&#9889; NEARING CONVERGENCE"
          : "&#9203; TRAINING IN PROGRESS";
    }
    if ($("az-comp-pct")) $("az-comp-pct").textContent = `${Math.round(comp.progress_pct || 0)}%`;
    if ($("az-comp-reason")) $("az-comp-reason").textContent = comp.reason || "";
    if ($("az-comp-bar-fill")) $("az-comp-bar-fill").style.width = `${Math.max(0, Math.min(100, comp.progress_pct || 0)).toFixed(1)}%`;

    // ETA estimate based on remaining games to 500k or remaining gens to 5500
    const etaEl = $("az-comp-eta");
    if (etaEl) {
      if (comp.is_complete) {
        etaEl.textContent = "Status: Graduated ✓";
      } else {
        const gph = az.games_per_hr || 0;
        const remGames = Math.max(0, 500000 - (az.total_games || 0));
        if (isRunning && gph >= 50) {
          const hrsTo500k = remGames / gph;
          etaEl.textContent =
            remGames > 0
              ? `~${hrsTo500k < 1 ? `${Math.round(hrsTo500k * 60)}m` : `${hrsTo500k.toFixed(1)}h`} to 500k gate`
              : "500k gate cleared — watching LR & plateau";
        } else {
          etaEl.textContent = remGames > 0 ? `${(remGames / 1000).toFixed(1)}k games to 500k gate` : "500k gate cleared";
        }
      }
    }

    const g = comp.gates || {};
    const g1 = $("az-gate-1");
    if (g1 && g.experience) {
      g1.classList.toggle("pass", Boolean(g.experience.passed));
      const curK = (g.experience.current / 1000).toFixed(1);
      const tgtK = (g.experience.target / 1000).toFixed(0);
      g1.innerHTML = `<b>${g.experience.passed ? "✓" : "○"} 1. Experience</b><span>${curK}k / ${tgtK}k games</span>`;
    }
    const g2 = $("az-gate-2");
    if (g2 && g.lr_anneal) {
      g2.classList.toggle("pass", Boolean(g.lr_anneal.passed));
      g2.innerHTML = `<b>${g.lr_anneal.passed ? "✓" : "○"} 2. LR Anneal</b><span>${Number(g.lr_anneal.current_lr).toFixed(5)} &rarr; &le;${Number(g.lr_anneal.target_lr).toFixed(5)}</span>`;
    }
    const g3 = $("az-gate-3");
    if (g3 && g.policy_plateau) {
      g3.classList.toggle("pass", Boolean(g.policy_plateau.passed));
      const txt = g.policy_plateau.ready
        ? `ΔPol ${g.policy_plateau.pol_loss_drop_150g >= 0 ? "-" : "+"}${Math.abs(g.policy_plateau.pol_loss_drop_150g).toFixed(3)} • ΔAcc ${g.policy_plateau.pol_acc_gain_150g >= 0 ? "+" : ""}${g.policy_plateau.pol_acc_gain_150g.toFixed(1)}%`
        : "Gathering 200-gen window...";
      g3.innerHTML = `<b>${g.policy_plateau.passed ? "✓" : "○"} 3. Policy Plateau</b><span>${txt}</span>`;
    }
    const g4 = $("az-gate-4");
    if (g4 && g.endgame_stability) {
      g4.classList.toggle("pass", Boolean(g.endgame_stability.passed));
      const txt = g.endgame_stability.ready
        ? `ΔML ${g.endgame_stability.ml_loss_drop_150g >= 0 ? "-" : "+"}${Math.abs(g.endgame_stability.ml_loss_drop_150g).toFixed(4)} • ${g.endgame_stability.avg_plies}p`
        : "Gathering 200-gen window...";
      g4.innerHTML = `<b>${g.endgame_stability.passed ? "✓" : "○"} 4. Endgame Stability</b><span>${txt}</span>`;
    }

    const unlockBtn = $("btn-az-unlock-now");
    const openHybBtn = $("btn-az-open-hybrids");
    if (unlockBtn) {
      // Allow manual early graduation once at least 150k games are logged, or instant unlock if converged
      const canManualUnlock = !comp.is_complete && (az.total_games || 0) >= 150000;
      unlockBtn.classList.toggle("hidden", !canManualUnlock);
    }
    if (openHybBtn) {
      openHybBtn.classList.toggle("hidden", !comp.is_complete);
    }

    if (comp.is_complete && !azWasCompleteToastShown && az.hybrids_unlocked) {
      azWasCompleteToastShown = true;
      // Refresh presets once so the unlocked Hybrids become selectable everywhere immediately!
      api("/api/state", "GET").then((fresh) => {
        if (fresh && fresh.ok) {
          state = fresh;
          populatePresets(state.presets);
        }
      });
    }

    // Stage 2: Singularity v5 Knowledge Distillation Forge UI
    const dStrip = $("az-distill-strip");
    const dMsg = $("az-distill-msg");
    const dStart = $("btn-az-distill-start");
    const dStop = $("btn-az-distill-stop");
    const dBarWrap = $("az-distill-bar-wrap");
    const dFill = $("az-distill-fill");
    if (dStrip && dMsg && dStart && dStop) {
      const dist = az.distill || null;
      const dRunning = Boolean(az.distill_running || (dist && dist.running && az.distill_running !== false));
      const v2Ready = Boolean(az.hybrids_unlocked);
      dStrip.classList.toggle("ready", v2Ready && !nnueDone);
      dStrip.classList.toggle("done", nnueDone);
      dStart.classList.toggle("hidden", dRunning);
      dStop.classList.toggle("hidden", !dRunning);
      dStart.disabled = !v2Ready || isRunning;

      if (dRunning && dist) {
        dMsg.textContent = dist.message || "Distilling Zero-C v2 into AVX2 NNUE...";
        if (dBarWrap && dFill) {
          dBarWrap.classList.remove("hidden");
          dFill.style.width = `${Math.max(2, Math.min(100, dist.progress_pct || 0)).toFixed(1)}%`;
        }
      } else if (nnueDone) {
        dMsg.innerHTML =
          (dist && dist.message) ||
          "<strong>&#9989; Singularity v5 NNUE Forged!</strong> <code>data/chess_net_zero_v4.nnue</code> is live &amp; unlocked in all menus.";
        dStart.innerHTML = "&#8635; Re-Distill NNUE";
        if (dBarWrap && dFill) {
          dBarWrap.classList.remove("hidden");
          dFill.style.width = "100%";
        }
        if (!window._singularityUnlockedRefreshed) {
          window._singularityUnlockedRefreshed = true;
          api("/api/state", "GET").then((fresh) => {
            if (fresh && fresh.ok) {
              state = fresh;
              populatePresets(state.presets);
            }
          });
        }
      } else if (v2Ready) {
        dMsg.innerHTML = isRunning
          ? "Zero-C v2 is graduated! <strong>Pause self-play training</strong> to give Distillation full RTX GPU access."
          : "Zero-C v2 is complete! Click <strong>&#9889; Distill Singularity v5</strong> to compress the 3.56M ResNet into a real 200ns AVX2 NNUE (<code>data/chess_net_zero_v4.nnue</code>, ~2&ndash;4 min).";
        if (dBarWrap) dBarWrap.classList.add("hidden");
      } else {
        dMsg.innerHTML =
          "Sealed until Zero-C v2 completes: compresses the 3.56M ResNet into a brand-new 200ns AVX2 CPU network (<code>data/chess_net_zero_v4.nnue</code>).";
        if (dBarWrap) dBarWrap.classList.add("hidden");
      }
    }
  }

  let r3kActiveTab = "map"; // "map" | "zeroc" | "forge" | "toy"
  let p3kLastData = null;
  let toyLastData = null;

  function setR3kTab(tab, updateUrl = true) {
    const valid = ["map", "zeroc", "forge", "toy"].includes(tab) ? tab : "map";
    r3kActiveTab = valid;
    if ($("r3k-tabs")) {
      $("r3k-tabs")
        .querySelectorAll(".r3k-tab")
        .forEach((b) => b.classList.toggle("active", b.dataset.r3kTab === valid));
    }
    for (const id of ["map", "zeroc", "forge", "toy"]) {
      const pane = $(`r3k-pane-${id}`);
      if (pane) pane.classList.toggle("hidden", id !== valid);
    }
    if (updateUrl && !$("az-modal").classList.contains("hidden")) {
      const routeMap = {
        map: "/road-to-3k",
        zeroc: "/zero-c",
        forge: "/project-3000",
        toy: "/toy-net",
      };
      syncUrl(routeMap[valid] || "/road-to-3k");
    }
    if (valid === "zeroc" && azLastData) {
      updateAzChartsOnly(azLastData, true);
    }
  }

  async function pollAzStatus() {
    const [az, p3k, toy] = await Promise.all([
      api("/api/az/status", "GET"),
      api("/api/p3k/status", "GET"),
      api("/api/toy/status", "GET"),
    ]);
    if (az && az.ok) renderAzStatus(az);
    if (p3k && p3k.ok) renderP3kStatus(p3k);
    if (toy && toy.ok) renderToyStatus(toy);
    renderRoadCampaign();
  }

  function startAzPolling() {
    stopAzPolling();
    azPollTimer = setInterval(pollAzStatus, 1300);
  }

  function stopAzPolling() {
    if (azPollTimer) {
      clearInterval(azPollTimer);
      azPollTimer = null;
    }
  }

  async function openAzModal(initialTab = "map") {
    $("az-modal").classList.remove("hidden");
    setR3kTab(initialTab, true);
    await pollAzStatus();
    startAzPolling();
  }

  function closeAzModal(skipRouteSync = false) {
    $("az-modal").classList.add("hidden");
    stopAzPolling();
    if (!skipRouteSync) syncUrl(underlyingBaseRoute());
  }

  // ==========================================================================
  // Event Wiring
  // ==========================================================================
  $("btn-gauntlet").addEventListener("click", () => openGauntletModal());
  $("btn-az-lab").addEventListener("click", () => openAzModal("map"));
  $("btn-az-close").addEventListener("click", () => closeAzModal());
  if ($("r3k-tabs")) {
    $("r3k-tabs")
      .querySelectorAll(".r3k-tab")
      .forEach((b) => b.addEventListener("click", () => setR3kTab(b.dataset.r3kTab, true)));
  }
  if ($("btn-home-toy")) {
    $("btn-home-toy").addEventListener("click", () => openAzModal("toy"));
  }
  if ($("btn-az-throttle")) {
    $("btn-az-throttle").addEventListener("click", async () => {
      const arch = $("az-arch").value || "pro";
      setSelectIfOptionExists($("az-games"), "1024");
      setSelectIfOptionExists($("az-playouts"), "48,400");
      setSelectIfOptionExists($("az-gpg"), "256");
      const res = await api("/api/az/start", "POST", {
        arch,
        games: 1024,
        fast_sims: 48,
        full_sims: 400,
        games_per_gen: 256,
        max_gens: 0,
        reset: false,
        full_throttle: true,
      });
      if (res && res.ok) {
        renderAzStatus(res);
        startAzPolling();
        logTerm("🔥 Launched Full Throttle Overnight mode (1024 games, 10T C MCTS, 48/400 caps, 256 games/gen, 2.0M replay, FP16 batch 768)!", "term-ok");
      } else {
        logTerm((res && res.error) || "Failed to start Full Throttle training.", "term-err");
      }
    });
  }
  $("btn-az-start").addEventListener("click", async () => {
    const arch = $("az-arch").value;
    const games = parseInt($("az-games").value, 10) || 64;
    const [fastSims, fullSims] = $("az-playouts").value.split(",").map((x) => parseInt(x, 10));
    const gpg = parseInt($("az-gpg").value, 10) || 128;
    const res = await api("/api/az/start", "POST", {
      arch,
      games,
      fast_sims: fastSims || 36,
      full_sims: fullSims || 200,
      games_per_gen: gpg,
      max_gens: 0,
      reset: false,
    });
    if (res && res.ok) {
      renderAzStatus(res);
      startAzPolling();
      logTerm(`Started Zero-C AlphaZero self-play (${arch}, ${games} games, ${fastSims}/${fullSims} caps).`, "term-ok");
    } else {
      logTerm((res && res.error) || "Failed to start Zero-C trainer.", "term-err");
    }
  });
  $("btn-az-stop").addEventListener("click", async () => {
    const res = await api("/api/az/stop", "POST");
    renderAzStatus(res);
    logTerm("Zero-C training paused cleanly.", "term-warn");
  });
  $("btn-az-spectate").addEventListener("click", async () => {
    await startSpectateTraining(-1);
  });
  $("btn-az-vs-v1").addEventListener("click", async () => {
    stopSpectateTraining(false);
    closeAzModal();
    showBoardWorkspace();
    await startNewGame("watch", 12, 13);
    logTerm("Launched Zero-C v2 (White) vs. Immortalized Zero-C v1 Champion Gen 4320 (Black)!", "term-ok");
  });
  $("btn-sp-next").addEventListener("click", async () => {
    if (!spectateActive) return;
    const az = await api("/api/az/status", "GET");
    const nextG = pickNextLiveGame((az && az.live_games) || [], spectateGameId);
    if (nextG) {
      spectateGameId = nextG.id;
      spectateLastSeq = nextG.seq != null ? nextG.seq : -1;
      spectateLastPly = nextG.ply || 0;
      renderSpectateFrame(az);
      showSpectateToast(`Switched to live Game #${nextG.id + 1} (Ply ${nextG.ply})`);
    }
  });
  $("btn-sp-analyze").addEventListener("click", async () => {
    const fen = spectateLastFen;
    stopSpectateTraining(false);
    if (fen) {
      await loadFenString(fen);
    }
    await enterAnalysisMode();
  });
  $("btn-sp-lab").addEventListener("click", () => {
    stopSpectateTraining(true);
    openAzModal();
  });
  $("btn-sp-exit").addEventListener("click", () => {
    stopSpectateTraining(true);
  });
  document.querySelectorAll("#az-window-group .az-seg-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      document.querySelectorAll("#az-window-group .az-seg-btn").forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      azChartWindow = parseInt(btn.dataset.win, 10) || 0;
      if (azLastData) updateAzChartsOnly(azLastData);
    });
  });
  if ($("az-chk-ema")) {
    $("az-chk-ema").addEventListener("change", () => {
      azShowEma = $("az-chk-ema").checked;
      if (azLastData) updateAzChartsOnly(azLastData);
    });
  }
  if ($("btn-az-guide-toggle")) {
    $("btn-az-guide-toggle").addEventListener("click", () => {
      $("az-guide-card").classList.toggle("hidden");
    });
  }
  $("btn-gauntlet-close").addEventListener("click", () => closeGauntletModal());
  $("btn-gauntlet-start").addEventListener("click", async () => {
    const games = parseInt($("gt-games").value, 10) || 6;
    const threads = parseInt($("gt-threads").value, 10) || 0;
    const onlyLevel = parseInt($("gt-scope").value, 10) || 0;
    const useSf = $("gt-opponent").value === "sf";
    const gs = await api("/api/gauntlet/start", "POST", {
      games,
      threads,
      only_level: onlyLevel,
      use_stockfish: useSf,
    });
    renderGauntletStatus(gs);
    startGauntletPolling();
    logTerm(`Started Elo gauntlet (${games} games/level, ${gs.num_threads} threads).`, "term-ok");
  });
  $("gt-tab-gauntlet").addEventListener("click", () => {
    setGauntletTab("gauntlet", false);
    syncUrl("/elo-arena");
  });
  $("gt-tab-calc").addEventListener("click", () => {
    setGauntletTab("calc", false);
    syncUrl("/elo-calculator");
  });
  $("calc-bot").addEventListener("change", () => {
    $("calc-custom-wrap").classList.toggle("hidden", $("calc-bot").value !== "0");
  });
  $("btn-calc-start").addEventListener("click", async () => {
    const rawBot = parseInt($("calc-bot").value, 10);
    const isV1 = rawBot === 13;
    const subjLevel = isV1 ? 7 : rawBot;
    const azCkpt = isV1 ? "data/az_v1_gen4320/latest.pt" : "";
    const customElo = parseInt($("calc-custom-elo").value, 10) || 1500;
    const targetCi = parseInt($("calc-target-ci").value, 10) || 90;
    const maxGames = parseInt($("calc-max-games").value, 10) || 40;
    const threads = parseInt($("calc-threads").value, 10) || 0;
    const gs = await api("/api/gauntlet/calc", "POST", {
      subject_level: subjLevel,
      az_checkpoint: azCkpt,
      custom_elo: customElo,
      target_ci: targetCi,
      max_games: maxGames,
      threads,
    });
    if (!gs.ok) {
      logTerm(gs.error || "Could not start Elo Calculator.", "term-err");
      return;
    }
    setGauntletTab("calc", true);
    renderGauntletStatus(gs);
    startGauntletPolling();
    logTerm(`Started Precision Elo Calculator (target ±${targetCi} Elo, up to ${maxGames} games).`, "term-ok");
  });
  $("btn-gauntlet-stop").addEventListener("click", async () => {
    const gs = await api("/api/gauntlet/stop", "POST");
    renderGauntletStatus(gs);
    logTerm("Stopping Elo gauntlet after active moves finish...", "term-warn");
  });

  // Bots Play From Here modal wiring
  function openBotsFromHereModal() {
    if (!state) return;
    if (state.is_game_over) {
      logTerm("Game is already over — step back to an earlier move or load a FEN first.", "term-warn");
      return;
    }
    $("bots-from-here-modal").classList.remove("hidden");
  }
  async function launchBotsFromHere() {
    const wl = parseInt($("bfh-white-bot").value, 10) || 6;
    const bl = parseInt($("bfh-black-bot").value, 10) || 9;
    $("bots-from-here-modal").classList.add("hidden");
    clearWatchTimer();
    watchPaused = false;
    $("btn-watch-toggle").textContent = "\u23F8 Pause";
    const data = await api("/api/mode", "POST", {
      mode: "watch",
      white_level: wl,
      black_level: bl,
    });
    if (data && data.ok) {
      applyState(data, true);
      logTerm("Launched Bot vs Bot battle from the current position!", "term-ok");
    }
  }
  $("btn-bots-from-here").addEventListener("click", openBotsFromHereModal);
  $("btn-toolbar-bots-here").addEventListener("click", openBotsFromHereModal);
  $("btn-bfh-launch").addEventListener("click", launchBotsFromHere);
  $("btn-bfh-close").addEventListener("click", () => $("bots-from-here-modal").classList.add("hidden"));

  $("btn-new").addEventListener("click", () => startNewGame());
  $("btn-analysis-toggle").addEventListener("click", () => {
    if (state && state.mode === "analysis") {
      exitAnalysisToPlay();
    } else {
      enterAnalysisMode();
    }
  });
  $("btn-import-pgn").addEventListener("click", () => openPgnModal(false));
  $("btn-undo").addEventListener("click", () => doUndo());
  $("btn-hint").addEventListener("click", () => doHint());
  $("btn-flip").addEventListener("click", () => doFlip());
  $("btn-autoflip").addEventListener("click", () => toggleAutoFlip());
  $("btn-eval-toggle").addEventListener("click", () => toggleEvalBar());
  $("btn-review").addEventListener("click", () => openReviewModal());
  $("btn-pgn").addEventListener("click", () => openPgnModal(true));
  $("btn-fen").addEventListener("click", () => openFenModal());
  $("btn-sound").addEventListener("click", () => toggleSound());

  // Analysis bar buttons
  $("btn-nav-start").addEventListener("click", () => gotoPly(0));
  $("btn-nav-prev").addEventListener("click", () => gotoPly(state.cursor_ply - 1));
  $("btn-nav-next").addEventListener("click", () => gotoPly(state.cursor_ply + 1));
  $("btn-nav-end").addEventListener("click", () => gotoPly(state.total_plies));
  $("chk-best-arrow").addEventListener("change", () => renderArrows());
  $("chk-sf-analyze").addEventListener("change", () => runLiveAnalysis());
  $("btn-play-from-here").addEventListener("click", () => exitAnalysisToPlay("white"));

  modeSelect.addEventListener("change", () => {
    const m = modeSelect.value;
    if (m === "analysis" && state && state.mode !== "analysis") {
      enterAnalysisMode();
      return;
    }
    $("white-level-wrap").classList.toggle("hidden", !(m === "black" || m === "watch"));
    $("black-level-wrap").classList.toggle("hidden", !(m === "white" || m === "watch"));
  });

  $("btn-watch-toggle").addEventListener("click", () => toggleWatchPause());
  $("btn-watch-step").addEventListener("click", async () => {
    watchPaused = true;
    $("btn-watch-toggle").textContent = "\u25B6 Resume";
    clearWatchTimer();
    await triggerBotMove();
  });
  $("watch-speed").addEventListener("input", (e) => {
    $("speed-val").textContent = `${e.target.value}ms`;
  });

  // FEN modal
  $("btn-fen-copy").addEventListener("click", async () => {
    if (navigator.clipboard) {
      await navigator.clipboard.writeText($("fen-input").value);
      logTerm("FEN copied to clipboard!", "term-ok");
    }
  });
  $("btn-fen-load").addEventListener("click", async () => {
    const val = $("fen-input").value.trim();
    $("fen-modal").classList.add("hidden");
    if (val) await loadFenString(val);
  });
  $("btn-fen-close").addEventListener("click", () => $("fen-modal").classList.add("hidden"));

  // PGN modal
  $("btn-pgn-current").addEventListener("click", async () => {
    const res = await api("/api/pgn", "GET");
    if (res.ok) $("pgn-textarea").value = res.pgn;
  });
  $("btn-pgn-copy").addEventListener("click", async () => {
    const val = $("pgn-textarea").value;
    if (val && navigator.clipboard) {
      await navigator.clipboard.writeText(val);
      logTerm("PGN copied to clipboard!", "term-ok");
    }
  });
  $("btn-pgn-load").addEventListener("click", async () => {
    const val = $("pgn-textarea").value.trim();
    $("pgn-modal").classList.add("hidden");
    if (val) await importPgnText(val);
  });
  $("btn-pgn-close").addEventListener("click", () => $("pgn-modal").classList.add("hidden"));

  // Review modal
  $("btn-review-close").addEventListener("click", () => {
    $("review-modal").classList.add("hidden");
    syncUrl(underlyingBaseRoute());
  });
  $("btn-review-pgn").addEventListener("click", () => doPgn(true));

  // ==========================================================================
  // Confetti Celebration Micro-Animation
  // ==========================================================================
  function launchConfetti() {
    const canvas = $("confetti-canvas");
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;
    const colors = ["#58a6ff", "#3fb950", "#a371f7", "#e3b341", "#f0883e", "#39c5cf"];
    const parts = Array.from({ length: 85 }, () => ({
      x: canvas.width * (0.25 + Math.random() * 0.5),
      y: canvas.height * 0.32,
      vx: (Math.random() - 0.5) * 12,
      vy: -4 - Math.random() * 9,
      size: 5 + Math.random() * 5,
      color: colors[Math.floor(Math.random() * colors.length)],
      rot: Math.random() * Math.PI * 2,
      vr: (Math.random() - 0.5) * 0.25,
      life: 1.0,
    }));
    let frame = 0;
    function step() {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      let alive = false;
      for (const p of parts) {
        p.x += p.vx;
        p.y += p.vy;
        p.vy += 0.34;
        p.rot += p.vr;
        p.life -= 0.015;
        if (p.life > 0 && p.y < canvas.height + 20) {
          alive = true;
          ctx.save();
          ctx.globalAlpha = Math.max(0, p.life);
          ctx.translate(p.x, p.y);
          ctx.rotate(p.rot);
          ctx.fillStyle = p.color;
          ctx.fillRect(-p.size / 2, -p.size / 2, p.size, p.size * 0.6);
          ctx.restore();
        }
      }
      if (alive && ++frame < 110) {
        requestAnimationFrame(step);
      } else {
        ctx.clearRect(0, 0, canvas.width, canvas.height);
      }
    }
    requestAnimationFrame(step);
  }

  // ==========================================================================
  // Home Menu Screen, Board Themes, Tactical Puzzles & Game History
  // ==========================================================================
  let currentScreen = "home"; // "home" | "board"
  let homeColor = "white";
  let puzzleThemeFilter = "all";
  let puzzleCatalog = [];
  let puzzleHintStage = 0;
  let historyCache = [];

  function applyBoardTheme(themeName) {
    const valid = ["sage", "tournament", "walnut", "ocean", "amethyst"];
    const t = valid.includes(themeName) ? themeName : "sage";
    document.body.dataset.theme = t;
    if ($("theme-select")) $("theme-select").value = t;
    try {
      localStorage.setItem("chess_in_c_theme", t);
    } catch (_) {}
  }

  function showBoardWorkspace(skipRouteSync = false) {
    currentScreen = "board";
    $("home-screen").classList.add("hidden");
    $("workspace").classList.remove("hidden");
    $("game-top-controls").classList.remove("hidden");
    $("btn-home").classList.remove("btn-toggled");
    if (!skipRouteSync) syncUrl(underlyingBaseRoute());
  }

  async function showHomeMenu() {
    currentScreen = "home";
    stopSpectateTraining(false);
    clearWatchTimer();
    $("workspace").classList.add("hidden");
    $("home-screen").classList.remove("hidden");
    $("game-top-controls").classList.add("hidden");
    $("btn-home").classList.add("btn-toggled");
    syncUrl("/");
    await refreshHomeDashboard();
  }

  async function refreshHomeDashboard() {
    const [pData, hData, azData] = await Promise.all([
      api("/api/puzzles", "GET"),
      api("/api/history", "GET"),
      api("/api/az/status", "GET"),
    ]);
    if (azData && azData.ok) {
      renderAzStatus(azData);
    }
    if (pData && pData.ok) {
      puzzleCatalog = pData.puzzles || [];
      const ps = pData.stats;
      $("home-pz-rating").textContent = String(ps.rating);
      $("home-pz-solved").textContent = `${ps.solved_count} / ${ps.total} solved`;
      $("home-pz-streak").innerHTML = `&#128293; ${ps.streak}`;
      $("home-pz-best").textContent = `Best: ${ps.best_streak}`;
      const pct = ps.total > 0 ? Math.round((ps.solved_count / ps.total) * 100) : 0;
      $("home-pz-fill").style.width = `${pct}%`;
      $("home-pz-pct").textContent = `${pct}%`;
    }
    if (hData && hData.ok) {
      historyCache = hData.games || [];
      $("home-hist-count").textContent = String(hData.count);
      $("home-hist-wdl").innerHTML = `${hData.summary.wins}W &bull; ${hData.summary.draws}D &bull; ${hData.summary.losses}L`;
      const mini = $("home-recent-games");
      if (historyCache.length === 0) {
        mini.innerHTML = `<div class="empty-hint">Completed games save here automatically.</div>`;
      } else {
        mini.innerHTML = historyCache
          .slice(0, 3)
          .map(
            (g) => `<div class="recent-mini-row" data-hist-id="${g.id}">
              <span><strong>${g.white}</strong> vs <strong>${g.black}</strong></span>
              <span class="res-badge ${resultBadgeClass(g)}">${g.result}</span>
            </div>`
          )
          .join("");
        mini.querySelectorAll("[data-hist-id]").forEach((row) => {
          row.addEventListener("click", () => loadHistoryGame(parseInt(row.dataset.histId, 10), false));
        });
      }
    }

    // Active game resume banner
    const resumeBanner = $("home-resume-banner");
    if (state && state.total_plies > 0 && state.mode !== "puzzle") {
      resumeBanner.classList.remove("hidden");
      $("home-resume-text").textContent = `Active session: ${state.white_name} vs ${state.black_name} (${state.total_plies} plies • ${state.opening || "In Progress"})`;
    } else if (state && state.mode === "puzzle" && state.puzzle) {
      resumeBanner.classList.remove("hidden");
      $("home-resume-text").textContent = `Active Puzzle #${state.puzzle.id}: ${state.puzzle.title} (★ ${state.puzzle.rating})`;
    } else {
      resumeBanner.classList.add("hidden");
    }
  }

  function resultBadgeClass(g) {
    if (g.result === "1/2-1/2") return "res-draw";
    if ((g.result === "1-0" && g.white === "Human") || (g.result === "0-1" && g.black === "Human"))
      return "res-win";
    if ((g.result === "0-1" && g.white === "Human") || (g.result === "1-0" && g.black === "Human"))
      return "res-loss";
    return "res-other";
  }

  // ---- Puzzle Mode Logic ----
  function renderPuzzleHud(feedbackText = null, feedbackCls = "") {
    if (!state || !state.puzzle) return;
    const pz = state.puzzle;
    const ps = state.puzzle_stats || { rating: 1200, streak: 0 };
    $("pz-id-badge").textContent = `#${pz.id}`;
    $("pz-title").textContent = pz.title;
    $("pz-theme-pill").textContent = pz.theme;
    $("pz-rating-pill").innerHTML = `&#9733; ${pz.rating}`;
    $("pz-hud-rating").textContent = `You: ${ps.rating}`;
    $("pz-hud-streak").innerHTML = `&#128293; ${ps.streak}`;
    $("pz-desc").textContent = pz.description;

    const fb = $("pz-feedback");
    if (feedbackText !== null) {
      fb.textContent = feedbackText;
      fb.className = "pz-feedback" + (feedbackCls ? ` ${feedbackCls}` : "");
    } else if (pz.solved) {
      fb.textContent = "Puzzle solved! Click 'Next Puzzle' for another challenge.";
      fb.className = "pz-feedback pz-ok";
    } else {
      const side = state.turn === "w" ? "White" : "Black";
      const stepStr = pz.total_moves > 1 ? ` (Move ${pz.done_moves + 1} of ${pz.total_moves})` : "";
      fb.textContent = `Your turn — find the winning move for ${side}!${stepStr}`;
      fb.className = "pz-feedback";
    }
  }

  async function startPuzzle(id) {
    stopSpectateTraining(false);
    clearWatchTimer();
    syncUrl("/puzzles");
    puzzleHintStage = 0;
    const data = await api("/api/puzzles/load", "POST", { id });
    if (!data || !data.ok) return;
    showBoardWorkspace();
    if (!autoFlip) {
      flipped = data.turn === "b";
    }
    applyState(data, true);
    renderPuzzleHud();
  }

  async function startNextPuzzle(theme = puzzleThemeFilter) {
    if (puzzleCatalog.length === 0) {
      const pData = await api("/api/puzzles", "GET");
      if (pData && pData.ok) puzzleCatalog = pData.puzzles || [];
    }
    const pool =
      theme === "all" ? puzzleCatalog : puzzleCatalog.filter((p) => p.theme === theme);
    const curId = state && state.puzzle ? state.puzzle.id : 0;
    const unsolved = pool.filter((p) => !p.solved && p.id !== curId);
    let pick = unsolved[0];
    if (!pick) {
      const idx = pool.findIndex((p) => p.id === curId);
      pick = pool[(idx + 1) % Math.max(1, pool.length)] || puzzleCatalog[0];
    }
    if (pick) await startPuzzle(pick.id);
  }

  async function submitPuzzleMove(from, to, promo) {
    busy = true;
    const res = await api("/api/puzzles/move", "POST", { from, to, promo: promo || "" });
    busy = false;
    if (!res || !res.ok) return;

    if (res.status === "wrong") {
      selectedSq = null;
      sfx.play("illegal");
      renderBoard();
      if (state.puzzle_stats) {
        state.puzzle_stats.rating = res.rating;
        state.puzzle_stats.streak = res.streak;
      }
      const deltaStr = res.rating_delta ? ` (${res.rating_delta} rating)` : "";
      renderPuzzleHud(`Not quite! Try another move or click Hint.${deltaStr}`, "pz-wrong");
      return;
    }

    puzzleHintStage = 0;
    hintArrow = null;
    applyState(res.state, true);
    if (res.status === "solved") {
      const deltaStr = res.rating_delta > 0 ? ` (+${res.rating_delta} rating!)` : "";
      renderPuzzleHud(
        res.clean
          ? `Brilliant! Puzzle solved cleanly!${deltaStr}`
          : "Puzzle complete! Try the next one for rating points.",
        "pz-ok"
      );
      if (res.clean) launchConfetti();
      const pData = await api("/api/puzzles", "GET");
      if (pData && pData.ok) puzzleCatalog = pData.puzzles || [];
    } else if (res.status === "continue") {
      renderPuzzleHud(
        `Great move! Opponent replied ${res.reply_san} — find the follow-up!`,
        "pz-ok"
      );
    }
  }

  async function doPuzzleHint() {
    if (!state || state.mode !== "puzzle" || !state.puzzle || state.puzzle.solved) return;
    const res = await api("/api/puzzles/hint", "POST");
    if (!res || !res.ok) return;
    puzzleHintStage++;
    if (puzzleHintStage === 1) {
      selectedSq = res.from;
      renderBoard();
      renderPuzzleHud(`Hint: ${res.hint}`, "");
    } else {
      hintArrow = { from: res.from, to: res.to };
      renderArrows();
      renderPuzzleHud(`Exact hint shown on board: ${res.hint}`, "");
    }
  }

  async function doPuzzleSolve() {
    if (!state || state.mode !== "puzzle") return;
    const res = await api("/api/puzzles/solve", "POST");
    if (!res || !res.ok) return;
    applyState(res.state, true);
    renderPuzzleHud(`Solution: ${(res.solution || []).join(" ")}`, "");
  }

  async function openPuzzleModal(filterTheme = puzzleThemeFilter) {
    puzzleThemeFilter = filterTheme;
    const pData = await api("/api/puzzles", "GET");
    if (pData && pData.ok) {
      puzzleCatalog = pData.puzzles || [];
      $("pz-modal-sub").textContent = `Rating ${pData.stats.rating} • Streak 🔥 ${pData.stats.streak} (Best ${pData.stats.best_streak}) • ${pData.stats.solved_count}/${pData.stats.total} Solved`;
    }
    $("pz-modal-filters")
      .querySelectorAll(".chip")
      .forEach((c) => c.classList.toggle("active", c.dataset.theme === puzzleThemeFilter));
    renderPuzzleBrowserGrid();
    $("puzzle-modal").classList.remove("hidden");
  }

  function renderPuzzleBrowserGrid() {
    const grid = $("pz-modal-grid");
    const list =
      puzzleThemeFilter === "all"
        ? puzzleCatalog
        : puzzleCatalog.filter((p) => p.theme === puzzleThemeFilter);
    grid.innerHTML = list
      .map(
        (p) => `<div class="pz-item-card ${p.solved ? "pz-done" : ""}" data-pid="${p.id}">
          <div class="pz-item-top">
            <span class="pz-theme-pill">${p.theme}</span>
            <span class="pz-rating-pill">${p.solved ? "&#10003; " : ""}&#9733; ${p.rating}</span>
          </div>
          <div class="pz-item-title">#${p.id} ${p.title}</div>
          <div class="pz-item-desc">${p.description}</div>
        </div>`
      )
      .join("");
    grid.querySelectorAll("[data-pid]").forEach((card) => {
      card.addEventListener("click", () => {
        $("puzzle-modal").classList.add("hidden");
        startPuzzle(parseInt(card.dataset.pid, 10));
      });
    });
  }

  // ---- Game History Logic ----
  async function openHistoryModal() {
    syncUrl("/history");
    const hData = await api("/api/history", "GET");
    if (hData && hData.ok) {
      historyCache = hData.games || [];
      $("hist-summary-strip").innerHTML = `
        <span><strong>${hData.count}</strong> Saved Games</span>
        <span class="badge-best">Wins: ${hData.summary.wins}</span>
        <span class="badge-inaccuracy">Draws: ${hData.summary.draws}</span>
        <span class="badge-blunder">Losses: ${hData.summary.losses}</span>`;
    }
    $("hist-search").value = "";
    renderHistoryModalTable();
    $("history-modal").classList.remove("hidden");
  }

  function renderHistoryModalTable() {
    const q = ($("hist-search").value || "").trim().toLowerCase();
    const filtered = historyCache.filter(
      (g) =>
        !q ||
        g.white.toLowerCase().includes(q) ||
        g.black.toLowerCase().includes(q) ||
        g.opening.toLowerCase().includes(q) ||
        g.result.toLowerCase().includes(q)
    );
    const wrap = $("hist-table-wrap");
    if (filtered.length === 0) {
      wrap.innerHTML = `<div class="empty-hint" style="padding:18px;text-align:center;">No saved games match yet. Finish a game or click &#128190; Save!</div>`;
      return;
    }
    wrap.innerHTML = `<table class="review-table">
      <thead>
        <tr><th>Date</th><th>Players</th><th>Result</th><th>Opening</th><th>Plies</th><th>Est. Elo</th><th>Actions</th></tr>
      </thead>
      <tbody>
        ${filtered
          .map((g) => {
            const eloStr =
              g.white_elo > 0 || g.black_elo > 0 ? `${g.white_elo || "—"} / ${g.black_elo || "—"}` : "—";
            return `<tr>
              <td class="term-dim">${g.date}</td>
              <td><strong>${g.white}</strong> vs <strong>${g.black}</strong></td>
              <td><span class="res-badge ${resultBadgeClass(g)}">${g.result}</span></td>
              <td>${g.opening}</td>
              <td>${g.plies}</td>
              <td>${eloStr}</td>
              <td class="hist-actions-cell">
                <button class="btn btn-sm btn-accent" data-h-analyze="${g.id}">Analyze</button>
                <button class="btn btn-sm" data-h-review="${g.id}">&#9733; Review</button>
                <button class="btn btn-sm" data-h-pgn="${g.id}">PGN</button>
                <button class="btn btn-sm btn-danger-ghost" data-h-del="${g.id}">&times;</button>
              </td>
            </tr>`;
          })
          .join("")}
      </tbody>
    </table>`;

    wrap.querySelectorAll("[data-h-analyze]").forEach((b) =>
      b.addEventListener("click", (e) => {
        e.stopPropagation();
        loadHistoryGame(parseInt(b.dataset.hAnalyze, 10), false);
      })
    );
    wrap.querySelectorAll("[data-h-review]").forEach((b) =>
      b.addEventListener("click", (e) => {
        e.stopPropagation();
        loadHistoryGame(parseInt(b.dataset.hReview, 10), true);
      })
    );
    wrap.querySelectorAll("[data-h-pgn]").forEach((b) =>
      b.addEventListener("click", async (e) => {
        e.stopPropagation();
        const g = historyCache.find((x) => x.id === parseInt(b.dataset.hPgn, 10));
        if (g && g.pgn && navigator.clipboard) {
          await navigator.clipboard.writeText(g.pgn);
          logTerm(`Copied Game #${g.id} PGN to clipboard!`, "term-ok");
        }
      })
    );
    wrap.querySelectorAll("[data-h-del]").forEach((b) =>
      b.addEventListener("click", async (e) => {
        e.stopPropagation();
        const hData = await api("/api/history/delete", "POST", { id: parseInt(b.dataset.hDel, 10) });
        if (hData && hData.ok) {
          historyCache = hData.games || [];
          renderHistoryModalTable();
        }
      })
    );
  }

  async function loadHistoryGame(id, andReview) {
    const data = await api("/api/history/load", "POST", { id });
    if (!data || !data.ok) return;
    $("history-modal").classList.add("hidden");
    showBoardWorkspace();
    applyState(data, true);
    if (andReview) {
      setTimeout(() => openReviewModal(), 200);
    }
  }

  // ---- Wire Home Menu, Puzzle, History & Theme Controls ----
  $("brand-home").addEventListener("click", () => showHomeMenu());
  $("btn-home").addEventListener("click", () => {
    if (currentScreen === "home") showBoardWorkspace();
    else showHomeMenu();
  });
  $("btn-home-resume").addEventListener("click", () => showBoardWorkspace());
  $("theme-select").addEventListener("change", (e) => applyBoardTheme(e.target.value));

  $("home-color-picker")
    .querySelectorAll(".color-opt")
    .forEach((btn) => {
      btn.addEventListener("click", () => {
        homeColor = btn.dataset.color;
        $("home-color-picker")
          .querySelectorAll(".color-opt")
          .forEach((b) => b.classList.toggle("active", b === btn));
      });
    });

  let homeStrengthMode = "preset"; // "preset" | "elo"

  function eloTierLabel(elo) {
    if (elo < 1050) return "Novice";
    if (elo < 1350) return "Casual";
    if (elo < 1700) return "Club";
    if (elo < 2000) return "Strong";
    if (elo < 2300) return "Master";
    return "Grandmaster";
  }

  if ($("home-strength-mode")) {
    $("home-strength-mode")
      .querySelectorAll(".color-opt")
      .forEach((btn) => {
        btn.addEventListener("click", () => {
          homeStrengthMode = btn.dataset.smode;
          $("home-strength-mode")
            .querySelectorAll(".color-opt")
            .forEach((b) => b.classList.toggle("active", b === btn));
          $("home-preset-wrap").classList.toggle("hidden", homeStrengthMode !== "preset");
          $("home-elo-slider-wrap").classList.toggle("hidden", homeStrengthMode !== "elo");
        });
      });
  }
  if ($("home-elo-slider")) {
    $("home-elo-slider").addEventListener("input", (e) => {
      const val = parseInt(e.target.value, 10) || 1600;
      $("home-elo-badge").textContent = `${val} Elo • ${eloTierLabel(val)}`;
    });
  }

  $("btn-home-play").addEventListener("click", async () => {
    const opp = parseInt($("home-opponent").value, 10) || 4;
    const celo = homeStrengthMode === "elo" ? parseInt($("home-elo-slider").value, 10) || 1600 : 0;
    const chosen = homeColor === "random" ? (Math.random() < 0.5 ? "white" : "black") : homeColor;
    showBoardWorkspace();
    await startNewGame(chosen, opp, opp, "", celo);
  });
  $("btn-home-2p").addEventListener("click", async () => {
    showBoardWorkspace();
    await startNewGame("2p", 4, 4);
  });
  $("home-pz-themes")
    .querySelectorAll(".chip")
    .forEach((c) => {
      c.addEventListener("click", () => {
        puzzleThemeFilter = c.dataset.theme;
        $("home-pz-themes")
          .querySelectorAll(".chip")
          .forEach((x) => x.classList.toggle("active", x === c));
      });
    });
  $("btn-home-puzzles").addEventListener("click", () => startNextPuzzle(puzzleThemeFilter));
  $("btn-home-puzzle-list").addEventListener("click", () => openPuzzleModal(puzzleThemeFilter));
  $("btn-home-analysis").addEventListener("click", async () => {
    showBoardWorkspace();
    await enterAnalysisMode();
  });
  $("btn-home-import").addEventListener("click", () => {
    showBoardWorkspace();
    openPgnModal(false);
  });
  $("btn-home-history").addEventListener("click", () => openHistoryModal());
  $("btn-home-watch").addEventListener("click", async () => {
    const wl = parseInt($("home-watch-w").value, 10) || 6;
    const bl = parseInt($("home-watch-b").value, 10) || 4;
    showBoardWorkspace();
    await startNewGame("watch", wl, bl);
  });
  $("btn-home-gauntlet").addEventListener("click", () => openGauntletModal());
  if ($("btn-home-az-lab")) {
    $("btn-home-az-lab").addEventListener("click", () => openAzModal());
  }
  if ($("btn-home-play-az")) {
    $("btn-home-play-az").addEventListener("click", async () => {
      showBoardWorkspace();
      await startNewGame("white", 12, 12);
    });
  }

  // Topbar & board new buttons
  $("btn-puzzles").addEventListener("click", () => {
    if (state && state.mode === "puzzle") openPuzzleModal(puzzleThemeFilter);
    else startNextPuzzle(puzzleThemeFilter);
  });
  $("btn-history").addEventListener("click", () => openHistoryModal());
  $("btn-save-game").addEventListener("click", async () => {
    const res = await api("/api/history/save", "POST");
    if (res && res.ok) {
      logTerm(`Game saved to History (#${res.id})!`, "term-ok");
    } else {
      logTerm((res && res.error) || "Could not save game.", "term-warn");
    }
  });

  // Puzzle HUD buttons
  $("btn-pz-hint").addEventListener("click", () => doPuzzleHint());
  $("btn-pz-solve").addEventListener("click", () => doPuzzleSolve());
  $("btn-pz-retry").addEventListener("click", () => {
    if (state && state.puzzle) startPuzzle(state.puzzle.id);
  });
  $("btn-pz-next").addEventListener("click", () => startNextPuzzle(puzzleThemeFilter));
  $("btn-pz-browse").addEventListener("click", () => openPuzzleModal(puzzleThemeFilter));
  $("btn-pz-modal-close").addEventListener("click", () => $("puzzle-modal").classList.add("hidden"));
  $("pz-modal-filters")
    .querySelectorAll(".chip")
    .forEach((c) => {
      c.addEventListener("click", () => {
        puzzleThemeFilter = c.dataset.theme;
        $("pz-modal-filters")
          .querySelectorAll(".chip")
          .forEach((x) => x.classList.toggle("active", x === c));
        renderPuzzleBrowserGrid();
      });
    });

  // History modal buttons
  $("btn-hist-close").addEventListener("click", () => {
    $("history-modal").classList.add("hidden");
    syncUrl(underlyingBaseRoute());
  });
  $("hist-search").addEventListener("input", () => renderHistoryModalTable());
  $("btn-hist-clear").addEventListener("click", async () => {
    const hData = await api("/api/history/clear", "POST");
    if (hData && hData.ok) {
      historyCache = [];
      renderHistoryModalTable();
      $("hist-summary-strip").innerHTML = `<span><strong>0</strong> Saved Games</span>`;
    }
  });

  // Terminal form
  $("terminal-form").addEventListener("submit", async (e) => {
    e.preventDefault();
    const val = termInput.value;
    termInput.value = "";
    await handleTerminalCommand(val);
  });

  termInput.addEventListener("keydown", (e) => {
    if (e.key === "ArrowUp") {
      e.preventDefault();
      if (cmdHistIdx + 1 < cmdHistory.length) {
        cmdHistIdx++;
        termInput.value = cmdHistory[cmdHistIdx];
      }
    } else if (e.key === "ArrowDown") {
      e.preventDefault();
      if (cmdHistIdx > 0) {
        cmdHistIdx--;
        termInput.value = cmdHistory[cmdHistIdx];
      } else {
        cmdHistIdx = -1;
        termInput.value = "";
      }
    }
  });

  // Global keyboard shortcuts (analysis nav, promotion, Esc, seamless terminal typing)
  document.addEventListener("keydown", (e) => {
    if (pendingPromo) {
      const k = e.key.toLowerCase();
      if (k === "escape") {
        closePromoModal();
      } else if (["q", "r", "b", "n"].includes(k)) {
        e.preventDefault();
        finishPromotion(k);
      }
      return;
    }

    if (e.key === "Escape") {
      $("fen-modal").classList.add("hidden");
      $("pgn-modal").classList.add("hidden");
      $("review-modal").classList.add("hidden");
      $("puzzle-modal").classList.add("hidden");
      $("history-modal").classList.add("hidden");
      closeFightersModal();
      closeGauntletModal();
      closeAzModal();
      selectedSq = null;
      hintArrow = null;
      clearUserAnnotations();
      renderBoard();
      return;
    }

    const active = document.activeElement;
    const inModal =
      !$("fen-modal").classList.contains("hidden") ||
      !$("pgn-modal").classList.contains("hidden") ||
      !$("review-modal").classList.contains("hidden") ||
      !$("puzzle-modal").classList.contains("hidden") ||
      !$("history-modal").classList.contains("hidden") ||
      !$("fighters-modal").classList.contains("hidden") ||
      !$("gauntlet-modal").classList.contains("hidden") ||
      !$("az-modal").classList.contains("hidden");
    if (inModal || currentScreen === "home") return;

    // Arrow key timeline navigation when in Analysis Mode (and terminal input is empty)
    const termEmpty = active !== termInput || termInput.value === "";
    if (state && state.mode === "analysis" && termEmpty) {
      if (e.key === "ArrowLeft") {
        e.preventDefault();
        gotoPly(state.cursor_ply - 1);
        return;
      }
      if (e.key === "ArrowRight") {
        e.preventDefault();
        gotoPly(state.cursor_ply + 1);
        return;
      }
      if (e.key === "Home") {
        e.preventDefault();
        gotoPly(0);
        return;
      }
      if (e.key === "End") {
        e.preventDefault();
        gotoPly(state.total_plies);
        return;
      }
    }

    if (
      active !== termInput &&
      active.tagName !== "INPUT" &&
      active.tagName !== "TEXTAREA" &&
      active.tagName !== "SELECT"
    ) {
      if (e.key === "/") {
        e.preventDefault();
        termInput.focus();
      } else if (e.key.length === 1 && !e.ctrlKey && !e.metaKey && !e.altKey) {
        termInput.focus();
      }
    }
  });

  // ==========================================================================
  // Hall of Champions — Anthropomorphized Fighters, Custom SVG Avatars & Lore
  // ==========================================================================
  const HUMAN_AVATAR_SVG = `<svg viewBox="0 0 64 64"><defs><radialGradient id="av-hum" cx="35%" cy="28%" r="72%"><stop offset="0%" stop-color="#304868"/><stop offset="100%" stop-color="#111824"/></radialGradient></defs><rect width="64" height="64" fill="url(#av-hum)"/><circle cx="32" cy="24" r="11" fill="#cfe4ff"/><path d="M14 58c2-12 10-18 18-18s16 6 18 18" fill="#58a6ff"/><path d="M24 14l4 4 4-6 4 6 4-4-2 6H26z" fill="#f0c14b"/></svg>`;

  const FIGHTERS_ROSTER = [
    {
      id: "titan-1",
      level: 1,
      name: "Pip",
      epithet: "The Wooden Squire",
      botName: "Titan-C L1 • Novice",
      faction: "titan",
      factionLabel: "Titan-C Guild",
      accent: "#7ee787",
      defaultElo: "~900 Elo",
      quote: "“Wait, was my queen still on that square?! Oh well—ONWARD!”",
      style: "Plucky Blunderer",
      weapon: "Depth-2 Wooden Sword",
      stats: { tactics: 24, intuition: 20, speed: 98, creativity: 55, endgame: 18 },
      lore: [
        "Carved from spare chessboard maple and handed an oversized iron helmet, Pip is the youngest recruit in the Titan-C Guild. He memorized how every piece moves this morning and immediately volunteered to guard the front gate of the Studio.",
        "Pip plays fast, fears nothing, and genuinely believes every pawn push is a brilliant attack. Don't underestimate his enthusiasm—if you fall asleep at the wheel, he will proudly deliver a back-rank mate and brag about it in the guild tavern for a week."
      ],
      avatar: `<svg viewBox="0 0 64 64"><rect width="64" height="64" fill="#18241d"/><circle cx="32" cy="36" r="16" fill="#d4a373"/><path d="M16 26c2-11 30-11 32 0l-3 5H19z" fill="#7c8a99"/><path d="M31 12c4-5 11-5 10 2-4 0-7 1-10 3z" fill="#7ee787"/><circle cx="26" cy="35" r="2.6" fill="#161b22"/><circle cx="38" cy="35" r="2.6" fill="#161b22"/><path d="M26 43q6 5 12 0" fill="none" stroke="#161b22" stroke-width="2.4" stroke-linecap="round"/><rect x="20" y="50" width="24" height="14" rx="5" fill="#4f6d5a"/></svg>`
    },
    {
      id: "titan-2",
      level: 2,
      name: "Barnaby",
      epithet: "The Café Gambler",
      botName: "Titan-C L2 • Casual",
      faction: "titan",
      factionLabel: "Titan-C Guild",
      accent: "#56d364",
      defaultElo: "~1250 Elo",
      quote: "“Double espresso, dubious gambit, and a wink. Your move, friend.”",
      style: "Cheeky Coffeehouse Hustler",
      weapon: "Depth-3 Brass Monocle",
      stats: { tactics: 42, intuition: 36, speed: 95, creativity: 68, endgame: 34 },
      lore: [
        "Barnaby learned chess on rain-slicked park tables, betting croissants against commuters. He wears a crooked beret, polishes his lucky brass monocle before every fork, and loves setting one-move traps with a theatrical eyebrow raise.",
        "When you punish his greed, Barnaby tips his hat and laughs—right before launching another knight leap straight at your f7 square. He's the ultimate warm-up sparring partner when you want lively, chaotic club chess."
      ],
      avatar: `<svg viewBox="0 0 64 64"><rect width="64" height="64" fill="#1f1b16"/><circle cx="32" cy="36" r="15" fill="#e5b88a"/><path d="M14 25c4-11 28-14 38-4-6 3-24 6-38 4z" fill="#8b3a3a"/><circle cx="26" cy="34" r="2.4" fill="#161b22"/><circle cx="38" cy="34" r="4.8" fill="none" stroke="#f0c14b" stroke-width="2"/><circle cx="38" cy="34" r="2" fill="#161b22"/><path d="M25 42c3 3 11 3 14 0" fill="none" stroke="#5c3a21" stroke-width="2.6" stroke-linecap="round"/><path d="M18 52h28l4 12H14z" fill="#3a4d3f"/></svg>`
    },
    {
      id: "titan-3",
      level: 3,
      name: "Clara",
      epithet: "The Clockwork Tactician",
      botName: "Titan-C L3 • Club",
      faction: "titan",
      factionLabel: "Titan-C Guild",
      accent: "#3fb950",
      defaultElo: "~1650 Elo",
      quote: "“Your knight is loose on c6. Tick. Tock.”",
      style: "Disciplined Club Specialist",
      weapon: "Quiescence Calipers",
      stats: { tactics: 64, intuition: 56, speed: 92, creativity: 58, endgame: 58 },
      lore: [
        "A master watchmaker who treats the 64 squares like an interlocking brass escapement, Clara never hangs a piece without a reason. Behind her emerald-tinted brass goggles, she counts every tempo, pin, and skewer down to the millisecond.",
        "She's the gatekeeper of serious club chess in the Studio. Beat Clara consistently and you've earned the respect of the inner guild—hang a single pawn and her gears will snap shut with surgical precision."
      ],
      avatar: `<svg viewBox="0 0 64 64"><rect width="64" height="64" fill="#131f1b"/><circle cx="32" cy="36" r="15" fill="#f0c8a0"/><path d="M16 32c0-13 32-13 32 0v-6c0-10-32-10-32 0z" fill="#3d2b1f"/><rect x="18" y="29" width="12" height="8" rx="4" fill="#10291e" stroke="#d9a12b" stroke-width="2.2"/><rect x="34" y="29" width="12" height="8" rx="4" fill="#10291e" stroke="#d9a12b" stroke-width="2.2"/><line x1="30" y1="33" x2="34" y2="33" stroke="#d9a12b" stroke-width="2.2"/><circle cx="24" cy="33" r="2" fill="#3fb950"/><circle cx="40" cy="33" r="2" fill="#3fb950"/><path d="M27 44q5 2 10 0" fill="none" stroke="#161b22" stroke-width="2" stroke-linecap="round"/><path d="M18 52h28l3 12H15z" fill="#234d36"/></svg>`
    },
    {
      id: "titan-4",
      level: 4,
      name: "Viktor",
      epithet: "The Iron Sentinel",
      botName: "Titan-C L4 • Strong",
      faction: "titan",
      factionLabel: "Titan-C Guild",
      accent: "#39c5cf",
      defaultElo: "~1950 Elo",
      quote: "“Attack my castle. I will turn your attack into my endgame.”",
      style: "Unbreakable Fortress Defender",
      weapon: "AVX2 Steel Shield & KPK Bitbase",
      stats: { tactics: 78, intuition: 72, speed: 94, creativity: 54, endgame: 82 },
      lore: [
        "Forged in the deep compilation trenches of Clang `-O3`, Viktor is a stoic tower of dark steel who speaks only in quiet prophylaxis. He doesn't chase flashy fireworks—he quietly neutralizes every threat, trades into a winning rook ending, and marches a lone passed pawn down the board.",
        "Armed with an exact King-Pawn-King bitbase tucked inside his gauntlet, Viktor has crushed thousands of hopeful sacrifices. If you see a gleaming cyan visor staring back across the e-file, double-check your math."
      ],
      avatar: `<svg viewBox="0 0 64 64"><rect width="64" height="64" fill="#0f1b24"/><path d="M18 22h28l3 24-17 10-17-10z" fill="#364654" stroke="#79c0ff" stroke-width="1.6"/><path d="M22 14h20v8H22z" fill="#4b5e6e"/><rect x="21" y="30" width="22" height="6" rx="3" fill="#09131a"/><rect x="23" y="32" width="18" height="2.4" rx="1.2" fill="#39c5cf"/><path d="M26 44h12" stroke="#8b949e" stroke-width="2.4" stroke-linecap="round"/><path d="M12 56l20-6 20 6v8H12z" fill="#21313e"/></svg>`
    },
    {
      id: "titan-5",
      level: 5,
      name: "Aurelia",
      epithet: "The Silver Sovereign",
      botName: "Titan-C L5 • Master",
      faction: "titan",
      factionLabel: "Titan-C Guild",
      accent: "#58a6ff",
      defaultElo: "~2200 Elo",
      quote: "“A positional bind is merely a checkmate that hasn't introduced itself yet.”",
      style: "Ice-Cold Positional Master",
      weapon: "King-Bucketed NNUE Scepter",
      stats: { tactics: 88, intuition: 85, speed: 95, creativity: 72, endgame: 90 },
      lore: [
        "Second only to the Colossus himself, Aurelia commands the Silver Sanctum with effortless elegance. She spots weak dark squares twelve moves before they open and maneuvers her knights with terrifying patience.",
        "Players who survive her opening preparation often find themselves mysteriously suffocating in the middlegame—every piece awkwardly placed, every break prevented—before Aurelia snaps the board in half with a single quiet queen retreat."
      ],
      avatar: `<svg viewBox="0 0 64 64"><rect width="64" height="64" fill="#10192b"/><circle cx="32" cy="36" r="14" fill="#e6ecf5"/><path d="M18 24l5-10 9 7 9-7 5 10z" fill="#c9d7eb" stroke="#58a6ff" stroke-width="1.5"/><circle cx="32" cy="20" r="2.6" fill="#58a6ff"/><path d="M23 35l5-1.5-2 3zM41 35l-5-1.5 2 3z" fill="#1f6feb"/><path d="M27 44q5 2 10 0" fill="none" stroke="#2d3c52" stroke-width="2"/><path d="M16 52l16-4 16 4 4 12H12z" fill="#1f3a66"/></svg>`
    },
    {
      id: "titan-6",
      level: 6,
      name: "Titan-C",
      epithet: "The Obsidian Colossus",
      botName: "Titan-C L6 • Grandmaster",
      faction: "titan",
      factionLabel: "Titan-C Guild",
      accent: "#2ea043",
      defaultElo: "~2367 Elo",
      quote: "“Three million nodes per second. Zero hesitation. Show me your best line.”",
      style: "Relentless Tactical Juggernaut",
      weapon: "4-Thread Lazy SMP + King-Bucketed AVX2 NNUE v3",
      stats: { tactics: 95, intuition: 86, speed: 99, creativity: 74, endgame: 94 },
      lore: [
        "The reigning hand-crafted sovereign of Chess-in-C. Born from pure C11 bitboards, Singular Extensions, ProbCut, and an AVX2 SIMD King-Bucketed NNUE brain, Titan-C was forged line-by-line by human craft and battle-tested across thousands of gauntlet wars.",
        "In combat, Titan-C feels like fencing a hydraulic press. He calculates 16+ plies deep in the blink of an eye, refutes unsound gambits with cold mathematical brutality, and guards the 2367-Elo throne against all challengers—including the newborn neural prodigy growing across the hall."
      ],
      avatar: `<svg viewBox="0 0 64 64"><defs><radialGradient id="av-t6" cx="50%" cy="35%" r="70%"><stop offset="0%" stop-color="#1e462b"/><stop offset="100%" stop-color="#0a0f14"/></radialGradient></defs><rect width="64" height="64" fill="url(#av-t6)"/><path d="M14 14l9 10 9-12 9 12 9-10-4 28-14 11-14-11z" fill="#1b2631" stroke="#3fb950" stroke-width="2"/><polygon points="32,25 38,34 32,43 26,34" fill="#3fb950"/><path d="M21 31l7 2-5 3zM43 31l-7 2 5 3z" fill="#ffd97d"/><path d="M12 56l20-7 20 7v8H12z" fill="#152c1e" stroke="#2ea043" stroke-width="1.5"/></svg>`
    },
    {
      id: "zero-v2",
      level: 12,
      name: "Nova (Zero-C v2)",
      epithet: "The Tabula Rasa Prodigy",
      botName: "Zero-C v2 (AlphaZero)",
      faction: "zero",
      factionLabel: "Zero-C Neural",
      accent: "#d2a8ff",
      defaultElo: "Training Live!",
      quote: "“Nobody taught me chess. I played two hundred thousand games with myself and discovered beauty.”",
      style: "Alien Positional Intuition & Sacrifices",
      weapon: "10×128 SE-ResNet + C Batched MCTS + Moves-Left Head",
      stats: { tactics: 82, intuition: 96, speed: 78, creativity: 99, endgame: 86 },
      lore: [
        "Nova woke up in complete darkness knowing zero openings, zero piece values, and zero human textbooks—only the bare legal rules of chess. Through hundreds of thousands of Full-Throttle overnight self-play battles on RTX Tensor Cores, she invented castling, fianchettoes, and king attacks all by herself.",
        "Unlike classical engines that count pawns like accountants, Nova feels the board as shimmering waves of Win/Draw/Loss probability. She will calmly sacrifice an exchange just to trap your bishop for twenty moves—and she's still growing stronger every single generation."
      ],
      avatar: `<svg viewBox="0 0 64 64"><defs><radialGradient id="av-zv2" cx="50%" cy="40%" r="65%"><stop offset="0%" stop-color="#3b1d5e"/><stop offset="100%" stop-color="#0c0a16"/></radialGradient></defs><rect width="64" height="64" fill="url(#av-zv2)"/><circle cx="32" cy="32" r="21" fill="none" stroke="#d2a8ff" stroke-width="1.6" stroke-dasharray="4 3"/><circle cx="32" cy="33" r="13" fill="#1b1430" stroke="#79c0ff" stroke-width="1.8"/><polygon points="32,18 35,25 32,29 29,25" fill="#f778ba"/><circle cx="27" cy="34" r="2.6" fill="#79c0ff"/><circle cx="37" cy="34" r="2.6" fill="#79c0ff"/><path d="M26 42q6 3 12 0" fill="none" stroke="#d2a8ff" stroke-width="2" stroke-linecap="round"/><circle cx="14" cy="18" r="2" fill="#f778ba"/><circle cx="50" cy="20" r="2" fill="#79c0ff"/><circle cx="49" cy="46" r="1.8" fill="#d2a8ff"/></svg>`
    },
    {
      id: "zero-v1",
      level: 13,
      name: "Chronos (Zero-C v1)",
      epithet: "The Gen-4320 Elder",
      botName: "Zero-C v1 Champion (Gen 4320)",
      faction: "zero",
      factionLabel: "Zero-C Neural",
      accent: "#f778ba",
      defaultElo: "~1450–1600 Elo",
      quote: "“Four thousand three hundred and twenty generations forged in fire. Come test the original.”",
      style: "Relentless First-Gen Neural Veteran",
      weapon: "Archived 6×96 SE-ResNet (Gen 4320 • 1000 Sims)",
      stats: { tactics: 68, intuition: 80, speed: 74, creativity: 90, endgame: 72 },
      lore: [
        "Immortalized in the Studio vaults at Generation 4,320, Chronos is the original Zero-C pioneer—the first tabula rasa mind ever born inside Chess-in-C. Wearing a golden-violet laurel forged from over half a million self-play games, he now stands as the revered sparring elder.",
        "Every time Nova hits a new training milestone, she steps into the ring against Chronos to prove how far the new generation has come. His style is bold, fiercely attacking, and unapologetically romantic."
      ],
      avatar: `<svg viewBox="0 0 64 64"><defs><radialGradient id="av-zv1" cx="50%" cy="35%" r="70%"><stop offset="0%" stop-color="#4d193a"/><stop offset="100%" stop-color="#110b14"/></radialGradient></defs><rect width="64" height="64" fill="url(#av-zv1)"/><path d="M14 40c-2-14 6-24 18-26 12 2 20 12 18 26" fill="none" stroke="#f0c14b" stroke-width="2.4" stroke-dasharray="5 3"/><circle cx="32" cy="35" r="13" fill="#231326" stroke="#f778ba" stroke-width="1.8"/><path d="M22 21l5 4 5-7 5 7 5-4-2 6H24z" fill="#f0c14b"/><circle cx="27" cy="35" r="2.5" fill="#ffd97d"/><circle cx="37" cy="35" r="2.5" fill="#ffd97d"/><path d="M26 42h12" stroke="#f778ba" stroke-width="2" stroke-linecap="round"/></svg>`
    },
    {
      id: "hybrid-dual",
      level: 14,
      name: "Gemini Arbiter",
      epithet: "The Dual-Brain Sovereign",
      botName: "Gemini Arbiter (Dual-Brain)",
      faction: "hybrid",
      factionLabel: "Mythic Hybrid Boss",
      accent: "#f0c14b",
      defaultElo: "🔒 Est. 2400–2480 Elo",
      quote: "“Titan calculates the steel. Nova reads the stars. I choose the truth between them.”",
      style: "Tactically Unblunderable Neural Arbiter",
      weapon: "Multi-PV Alpha-Beta + GPU Policy/WDL Selector",
      stats: { tactics: 97, intuition: 96, speed: 88, creativity: 94, endgame: 96 },
      locked: true,
      lore: [
        "SEALED IN THE VAULT — When Zero-C v2 completes her training, the first of the four Mythic Hybrids awakens. Gemini Arbiter splits its consciousness across CPU and GPU simultaneously: Titan-C's 4-thread alpha-beta sword proposes the Top-5 tactically sound candidate moves, and Nova's 10×128 neural brain votes among the tied equals.",
        "If one move wins a rook, Titan overrules immediately. If three positional moves look equal to raw calculation, Gemini lets neural intuition break the tie—combining zero-blunder steel with grandmaster positional taste."
      ],
      avatar: `<svg viewBox="0 0 64 64"><rect width="64" height="64" fill="#13111c"/><path d="M32 10A22 22 0 0 0 32 54Z" fill="#1b3a29"/><path d="M32 10A22 22 0 0 1 32 54Z" fill="#3a1c54"/><circle cx="32" cy="32" r="22" fill="none" stroke="#f0c14b" stroke-width="2.4"/><line x1="32" y1="10" x2="32" y2="54" stroke="#f0c14b" stroke-width="2"/><rect x="19" y="29" width="10" height="4" rx="2" fill="#3fb950"/><circle cx="40" cy="31" r="3.4" fill="#d2a8ff"/><path d="M24 44h16" stroke="#f0c14b" stroke-width="2" stroke-linecap="round"/></svg>`
    },
    {
      id: "hybrid-lmr",
      level: 15,
      name: "Oracle of LMR",
      epithet: "The Neural-Guided Seer",
      botName: "Oracle of LMR (Neural Alpha-Beta)",
      faction: "hybrid",
      factionLabel: "Mythic Hybrid Boss",
      accent: "#ffa657",
      defaultElo: "🔒 Est. 2420–2500 Elo",
      quote: "“Why search a million dead branches when my third eye already saw the winning move?”",
      style: "Hyper-Deep Focused Calculator",
      weapon: "GPU Root Policy Ordering + Deep Alpha-Beta Pruning",
      stats: { tactics: 98, intuition: 93, speed: 91, creativity: 92, endgame: 95 },
      locked: true,
      lore: [
        "SEALED IN THE VAULT — Cloaked in burning amber circuits, the Oracle of LMR uses Zero-C v2's neural Policy head like a laser sight. Before a single deep variation begins, the Oracle scores every legal move by neural intuition and orders the search tree so the strongest human-like ideas are explored at maximum depth.",
        "Quiet positional sacrifices that normal alpha-beta engines prune away with Late Move Reductions get promoted straight to the front of the queue—letting the Oracle find chilling, deep combinations no classical search would ever check."
      ],
      avatar: `<svg viewBox="0 0 64 64"><rect width="64" height="64" fill="#1a120d"/><path d="M14 54L22 16l10-8 10 8 8 38z" fill="#2a1d17" stroke="#ffa657" stroke-width="2"/><ellipse cx="32" cy="28" rx="11" ry="6.5" fill="#0d0a08" stroke="#f0c14b" stroke-width="1.8"/><circle cx="32" cy="28" r="3.8" fill="#ffa657"/><circle cx="32" cy="28" r="1.6" fill="#fff"/><path d="M22 42l10-4 10 4" fill="none" stroke="#d2a8ff" stroke-width="1.8"/></svg>`
    },
    {
      id: "hybrid-omega",
      level: 16,
      name: "Zero-C Omega",
      epithet: "The Six-Winged Seraph",
      botName: "Zero-C Omega (Hybrid MCTS)",
      faction: "hybrid",
      factionLabel: "Mythic Hybrid Boss",
      accent: "#ff7b72",
      defaultElo: "🔒 Est. 2450–2550 Elo",
      quote: "“My wings are pure AlphaZero. My blade is tactical certainty.”",
      style: "AlphaZero Creativity + Zero-Blunder Tactical Verification",
      weapon: "Batched GPU MCTS + Exact Tactical Refutation Guard",
      stats: { tactics: 96, intuition: 99, speed: 84, creativity: 100, endgame: 94 },
      locked: true,
      lore: [
        "SEALED IN THE VAULT — This is Nova's ascended final form. Zero-C Omega lets the full 10×128 MCTS tree roam free in its wildest, most breathtaking AlphaZero glory—but pairs every root move with Titan-C's tactical verification sentinel.",
        "If an ambitious neural sacrifice has a hidden 6-ply tactical refutation, the sentinel vetoes it instantly; if the sacrifice survives Titan's fire, Omega unleashes it with full conviction. Pure artistry, zero blind spots."
      ],
      avatar: `<svg viewBox="0 0 64 64"><rect width="64" height="64" fill="#1b0e14"/><path d="M6 26l16 6-14 8zM58 26l-16 6 14 8zM10 14l14 12-12 2zM54 14l-14 12 12 2z" fill="#ff7b72" opacity="0.82"/><circle cx="32" cy="34" r="13" fill="#23121c" stroke="#ffd97d" stroke-width="2"/><polygon points="32,20 36,30 32,36 28,30" fill="#ffd97d"/><circle cx="27" cy="35" r="2.2" fill="#79c0ff"/><circle cx="37" cy="35" r="2.2" fill="#79c0ff"/><path d="M27 42h10" stroke="#ff7b72" stroke-width="2" stroke-linecap="round"/></svg>`
    },
    {
      id: "hybrid-nnue",
      level: 17,
      name: "Singularity v5",
      epithet: "The Distilled Apex",
      botName: "Singularity v5 (Distilled NNUE)",
      faction: "hybrid",
      factionLabel: "Mythic Hybrid Boss",
      accent: "#79c0ff",
      defaultElo: "🔒 Est. 2500–2650+ Elo",
      quote: "“Two bloodlines. One mind. Pure lightning.”",
      style: "Transcendent Grandmaster Fusion",
      weapon: "Zero-C Distilled Knowledge + AVX2 Alpha-Beta Core",
      stats: { tactics: 100, intuition: 99, speed: 100, creativity: 96, endgame: 99 },
      locked: true,
      lore: [
        "SEALED IN THE VAULT — The ultimate boss of Chess-in-C. Singularity v5 distills everything Nova learned across half a million self-play games straight into Titan-C's blazing-fast C11 alpha-beta search, marrying deep neural king-safety intuition with millions of nodes per second.",
        "When Zero-C v2 finishes her final generation and the seal breaks, Singularity v5 steps out of the forge to challenge Stockfish's highest ranks—representing the combined peak of human C engineering and pure tabula rasa machine intelligence."
      ],
      avatar: `<svg viewBox="0 0 64 64"><defs><radialGradient id="av-sing" cx="50%" cy="50%" r="55%"><stop offset="0%" stop-color="#ffffff"/><stop offset="45%" stop-color="#79c0ff"/><stop offset="100%" stop-color="#0a1020"/></radialGradient></defs><rect width="64" height="64" fill="#090d18"/><circle cx="32" cy="32" r="23" fill="none" stroke="#79c0ff" stroke-width="1.6" stroke-dasharray="3 3"/><polygon points="32,8 50,32 32,56 14,32" fill="url(#av-sing)" stroke="#f0f6fc" stroke-width="1.8"/><polygon points="32,19 41,32 32,45 23,32" fill="#0d1526"/><circle cx="32" cy="32" r="4.5" fill="#ffd97d"/></svg>`
    },
    {
      id: "apex-pure",
      level: 18,
      name: "Apex-C (Pure)",
      epithet: "The Project-3000 Vanguard",
      botName: "Apex-C 3000 (Pure • No TB)",
      faction: "apex",
      factionLabel: "Project 3000 Apex",
      accent: "#ff9e64",
      defaultElo: "Target 3000+ (Pure)",
      quote: "“No tablebases. No crutches. Pure C11 steel, full Correction History, and raw search depth.”",
      style: "Relentless Modern Super-GM Search",
      weapon: "Full CorrHist Suite + Capture/Cut LMR + 16×768→1024 NNUE v4",
      stats: { tactics: 99, intuition: 96, speed: 100, creativity: 88, endgame: 96 },
      lore: [
        "Forged specifically to shatter the 3,000-Elo barrier without a single external tablebase byte. Apex-C Pure wields the full modern search arsenal: Pawn + Non-Pawn + Minor + Major + Continuation Correction History, Capture & Cut-Node LMR, Low-Ply History, and High-Depth Null-Move Verification.",
        "Playable tonight with the upgraded Project 3000 search, and built to seamlessly swallow a 16×768→1024 NNUE v4 brain (`data/titan_v6_1024.nnue`) trained on 50M+ self-play positions from `build/datagen`."
      ],
      avatar: `<svg viewBox="0 0 64 64"><defs><radialGradient id="av-apx1" cx="50%" cy="35%" r="70%"><stop offset="0%" stop-color="#5c280a"/><stop offset="100%" stop-color="#0d0a0c"/></radialGradient></defs><rect width="64" height="64" fill="url(#av-apx1)"/><polygon points="32,7 53,24 46,55 18,55 11,24" fill="#1d181e" stroke="#ff9e64" stroke-width="2.2"/><polygon points="32,16 42,32 32,46 22,32" fill="#ff9e64"/><polygon points="32,23 37,32 32,40 27,32" fill="#fff3c4"/><path d="M19 29l7 2-5 4zM45 29l-7 2 5 4z" fill="#ff7b72"/></svg>`
    },
    {
      id: "apex-syzygy",
      level: 19,
      name: "Apex-C + Syzygy",
      epithet: "The 5-Piece Endgame God",
      botName: "Apex-C 3000 + Syzygy (5-Man TB)",
      faction: "apex",
      factionLabel: "Project 3000 Apex",
      accent: "#39c5cf",
      defaultElo: "🔒 Target 3100+ (5-Man TB)",
      quote: "“Trade down to five pieces and the game is mathematically over. I already know the exact mate in 47.”",
      style: "Super-GM Search + Exact 5-Man Endgame Perfection",
      weapon: "Apex-C Search + 16×768→1024 NNUE v4 + 5-Piece Syzygy WDL/DTZ",
      stats: { tactics: 99, intuition: 96, speed: 100, creativity: 88, endgame: 100 },
      locked: true,
      lore: [
        "Apex-C's twin colossus, fused with the complete 3-4-5 piece Syzygy Endgame Tablebase vault (`data/syzygy/`). Keep it side-by-side against Pure Apex-C to watch exactly how much mathematical endgame perfection adds in real matches.",
        "Run `make syzygy-5` (~939 MB resumable download) to break the seal and unlock instantaneous WDL/DTZ endgame perfection whenever 5 or fewer pieces remain on the board!"
      ],
      avatar: `<svg viewBox="0 0 64 64"><defs><radialGradient id="av-apx2" cx="50%" cy="35%" r="70%"><stop offset="0%" stop-color="#0e4554"/><stop offset="100%" stop-color="#080d14"/></radialGradient></defs><rect width="64" height="64" fill="url(#av-apx2)"/><polygon points="32,7 53,24 46,55 18,55 11,24" fill="#132230" stroke="#39c5cf" stroke-width="2.2"/><circle cx="32" cy="31" r="11" fill="none" stroke="#ffd97d" stroke-width="2"/><polygon points="32,20 37,31 32,42 27,31" fill="#39c5cf"/><circle cx="32" cy="31" r="3" fill="#ffffff"/></svg>`
    },
    {
      id: "sf-7",
      level: 7,
      name: "Stockfish 1320",
      epithet: "The Neon Minnow",
      botName: "Stockfish 1320",
      faction: "stockfish",
      factionLabel: "Stockfish Syndicate",
      accent: "#6cb6ff",
      defaultElo: "1320 FIDE",
      quote: "“Just swimming around the shallow end. Careful—I still bite!”",
      style: "Friendly Syndicate Scout",
      weapon: "UCI_Elo 1320 Limiter",
      stats: { tactics: 48, intuition: 45, speed: 90, creativity: 45, endgame: 48 },
      lore: [
        "The smallest cyber-fish in the Stockfish Syndicate, intentionally throttled with brass pressure valves so human club beginners can spar without getting vaporized on move 9.",
        "He plays clean, honest fundamentals and serves as the official lower anchor in the Studio's FIDE Elo Calibration Arena."
      ],
      avatar: `<svg viewBox="0 0 64 64"><rect width="64" height="64" fill="#0c1926"/><path d="M12 32c8-12 22-14 34-4l8-8v24l-8-8c-12 10-26 8-34-4z" fill="#4aa3f0" stroke="#9ecbff" stroke-width="1.6"/><circle cx="22" cy="30" r="2.6" fill="#0b111a"/><circle cx="21.4" cy="29.4" r="0.9" fill="#fff"/></svg>`
    },
    {
      id: "sf-8",
      level: 8,
      name: "Stockfish 1600",
      epithet: "The Reef Barracuda",
      botName: "Stockfish 1600",
      faction: "stockfish",
      factionLabel: "Stockfish Syndicate",
      accent: "#58a6ff",
      defaultElo: "1600 FIDE",
      quote: "“Leave a diagonal open and I'm already through it.”",
      style: "Opportunistic Counter-Striker",
      weapon: "UCI_Elo 1600 Limiter",
      stats: { tactics: 63, intuition: 60, speed: 92, creativity: 55, endgame: 62 },
      lore: [
        "Fast, sleek, and razor-toothed, the Reef Barracuda patrols the 1600 waters. He gives you breathing room in quiet positions, then darts out of nowhere the instant you drop a tempo.",
        "Every aspiring Zero-C generation dreams of out-swimming the Barracuda on its climb up the FIDE ladder."
      ],
      avatar: `<svg viewBox="0 0 64 64"><rect width="64" height="64" fill="#0b1829"/><path d="M9 33c10-13 26-14 38-3l9-10v26l-9-10c-12 11-28 10-38-3z" fill="#3182ce" stroke="#79c0ff" stroke-width="1.8"/><path d="M28 22l7-8 3 8z" fill="#79c0ff"/><circle cx="20" cy="31" r="2.8" fill="#ffd97d"/><circle cx="20" cy="31" r="1.2" fill="#0b111a"/></svg>`
    },
    {
      id: "sf-9",
      level: 9,
      name: "Stockfish 2000",
      epithet: "The Steel Swordfish",
      botName: "Stockfish 2000",
      faction: "stockfish",
      factionLabel: "Stockfish Syndicate",
      accent: "#388bfd",
      defaultElo: "2000 FIDE",
      quote: "“Expert waters begin here. Show me Candidate Master steel.”",
      style: "Sharp Expert Benchmark",
      weapon: "UCI_Elo 2000 Limiter",
      stats: { tactics: 80, intuition: 78, speed: 94, creativity: 68, endgame: 80 },
      lore: [
        "The legendary 2000-Elo milestone boss—the exact benchmark Zero-C v2's Mastery Roadmap is hunting down. The Steel Swordfish skewers imprecise openings and converts single-pawn advantages with cold expert discipline.",
        "Defeating him in a match is a badge of honor for both humans and neural networks alike."
      ],
      avatar: `<svg viewBox="0 0 64 64"><rect width="64" height="64" fill="#091528"/><path d="M5 32l12-3c8-10 20-12 31-3l9-10v28l-9-10c-11 9-23 7-31-2z" fill="#1f6feb" stroke="#a5d6ff" stroke-width="1.8"/><path d="M27 21l8-10 4 11z" fill="#58a6ff"/><circle cx="23" cy="30" r="2.8" fill="#39c5cf"/><circle cx="23" cy="30" r="1.2" fill="#091528"/></svg>`
    },
    {
      id: "sf-10",
      level: 10,
      name: "Stockfish 2400",
      epithet: "The Deep-Sea Kraken",
      botName: "Stockfish 2400",
      faction: "stockfish",
      factionLabel: "Stockfish Syndicate",
      accent: "#1f6feb",
      defaultElo: "2400 FIDE",
      quote: "“International Master precision. One slip and the abyss takes the full point.”",
      style: "Crushing Master-Tier Predator",
      weapon: "UCI_Elo 2400 Limiter",
      stats: { tactics: 94, intuition: 92, speed: 96, creativity: 80, endgame: 94 },
      lore: [
        "Lurking right above Titan-C L6's calibrated rating, the Deep-Sea Kraken is the ultimate rival of our hand-crafted Colossus. Every match between Titan-C and Stockfish 2400 is a heavyweight title fight down to the last pawn.",
        "Only grandmasters—and the Mythic Hybrids sleeping in the vault—can look the Kraken in the eye without flinching."
      ],
      avatar: `<svg viewBox="0 0 64 64"><rect width="64" height="64" fill="#081122"/><circle cx="32" cy="26" r="15" fill="#1b4b99" stroke="#79c0ff" stroke-width="1.8"/><path d="M16 38q4 14 10 16M25 40q2 14 4 17M39 40q-2 14-4 17M48 38q-4 14-10 16" fill="none" stroke="#58a6ff" stroke-width="3" stroke-linecap="round"/><circle cx="26" cy="26" r="3" fill="#ff7b72"/><circle cx="38" cy="26" r="3" fill="#ff7b72"/></svg>`
    },
    {
      id: "sf-11",
      level: 11,
      name: "Stockfish Max",
      epithet: "The Abyssal Leviathan",
      botName: "Stockfish Max",
      faction: "stockfish",
      factionLabel: "Stockfish Syndicate",
      accent: "#a5d6ff",
      defaultElo: "3500+ Super-GM",
      quote: "“I have already seen the end of your game. Would you like to play it anyway?”",
      style: "Omniscient Apex Super-Engine",
      weapon: "Unthrottled Stockfish NNUE",
      stats: { tactics: 100, intuition: 100, speed: 100, creativity: 96, endgame: 100 },
      lore: [
        "An ancient world-champion titan visiting the Chess-in-C Studio as both our ultimate referee in Game Review and our untouchable final summit. With no Elo limiters attached, the Abyssal Leviathan sees 30 moves into the void.",
        "Challenge him if you dare—or recruit him in Analysis Mode to uncover brilliant sacrifices and judge your games with cold perfection."
      ],
      avatar: `<svg viewBox="0 0 64 64"><defs><radialGradient id="av-sfm" cx="50%" cy="35%" r="70%"><stop offset="0%" stop-color="#193c70"/><stop offset="100%" stop-color="#050912"/></radialGradient></defs><rect width="64" height="64" fill="url(#av-sfm)"/><path d="M10 34c8-18 36-18 44 0-6 14-38 14-44 0z" fill="#0d2347" stroke="#a5d6ff" stroke-width="2"/><path d="M18 18l6 7 8-12 8 12 6-7-2 10H20z" fill="#a5d6ff"/><polygon points="22,34 28,31 28,37" fill="#ff7b72"/><polygon points="42,34 36,31 36,37" fill="#ff7b72"/><path d="M24 44h16" stroke="#79c0ff" stroke-width="2" stroke-linecap="round"/></svg>`
    }
  ];

  let fightersFactionFilter = "all";
  let selectedFighterId = "titan-6";

  function getPresetByLevel(level) {
    if (!state || !state.presets) return null;
    return state.presets.find((p) => p.level === level) || null;
  }

  function isFighterLocked(f) {
    const p = getPresetByLevel(f.level);
    if (p) return Boolean(p.locked);
    return Boolean(f.locked);
  }

  function getApexEvolution(f) {
    if (!f || (f.level !== 18 && f.level !== 19)) return null;
    const isSyz = f.level === 19;
    const p = getPresetByLevel(f.level);
    const p3k = p3kLastData || {};
    const t = (p && p.apex_tasks) || {};

    const dgPos = Number(
      (p3k.datagen && p3k.datagen.file_positions != null ? p3k.datagen.file_positions : t.datagen_pos) || 0
    );
    const dgPct = Math.min(100, Math.round((dgPos / 25000000) * 100));
    const dgDone = Boolean(t.datagen || dgPos >= 25000000);
    const spsaDone = Boolean(t.spsa || (p3k.spsa && p3k.spsa.detail && p3k.spsa.detail.done));
    const nnDone = Boolean((p && p.has_v4_nnue) || t.nnue_v4 || (p3k.nnue_v4 && p3k.nnue_v4.ready));
    const syzFiles = Number((p3k.syzygy && p3k.syzygy.files) || 0);
    const syzDone = Boolean((p && !p.locked) || t.syzygy || (p3k.syzygy && p3k.syzygy.ready));

    const maxStage = isSyz ? 5 : 4;
    const stage =
      1 + (isSyz && syzDone ? 1 : 0) + (dgDone ? 1 : 0) + (spsaDone ? 1 : 0) + (nnDone ? 1 : 0);
    const pct = isSyz
      ? Math.min(100, Math.round(12 + (syzDone ? 18 : 0) + 24 * (dgPct / 100) + (spsaDone ? 12 : 0) + (nnDone ? 34 : 0)))
      : Math.min(100, Math.round(15 + 30 * (dgPct / 100) + (spsaDone ? 15 : 0) + (nnDone ? 40 : 0)));
    const elo = 2580 + (isSyz && syzDone ? 70 : 0) + (spsaDone ? 60 : 0) + (nnDone ? 410 : 0);
    const targetElo = isSyz ? 3120 : 3050;
    const isMax = stage >= maxStage;

    let tier = (p && p.apex_tier) || "Prototype";
    if (!p || !p.apex_tier) {
      if (isSyz && !syzDone) tier = "Sealed (Needs TB)";
      else if (isMax) tier = isSyz ? "GODMODE 3100" : "APEX 3000";
      else if (nnDone) tier = isSyz ? "v4 + TB Awakened" : "v4 Awakened";
      else if (stage >= maxStage - 1) tier = isSyz ? "Forged + TB" : "Forged";
      else if (stage >= 2) tier = isSyz ? "TB Prototype" : "Tempered";
      else tier = "Prototype";
    }

    // Visual evolution styling: Steel Prototype -> Amber -> Blaze Orange -> Molten Gold / Cyan Godmode
    const normTier = isMax ? (isSyz ? 5 : 4) : Math.max(1, Math.min(4, nnDone ? 3 : stage));
    const accentMap = isSyz
      ? { 1: "#7d8590", 2: "#39c5cf", 3: "#56d4dd", 4: "#ffd97d", 5: "#6ef3ff" }
      : { 1: "#8b949e", 2: "#d29922", 3: "#ff9e64", 4: "#ffd97d" };
    const accent = accentMap[normTier] || f.accent;
    const tierClass = `apex-tier-${normTier}`;

    const tasks = [
      {
        label: "Modern C11 Search Engine",
        detail: "CorrHist + Capture/Cut LMR + NMP Verify",
        done: true,
        partial: 100,
      },
    ];
    if (isSyz) {
      tasks.push({
        label: "5-Piece Syzygy Tablebases",
        detail: syzDone
          ? `${syzFiles > 0 ? syzFiles : 290}/290 files (+70 Elo)`
          : `${syzFiles}/290 files — Download in 3000 Forge`,
        done: syzDone,
        partial: syzDone ? 100 : Math.round((syzFiles / 290) * 100),
      });
    }
    tasks.push(
      {
        label: "Self-Play Datagen (25M Positions)",
        detail: dgDone
          ? `${(dgPos / 1e6).toFixed(1)}M positions ready`
          : dgPos > 0
          ? `${(dgPos / 1e6).toFixed(2)}M / 25.0M (${dgPct}%) — feeds NNUE v4`
          : "0 / 25.0M — Start in 3000 Forge",
        done: dgDone,
        partial: dgPct,
      },
      {
        label: "SPSA Search Parameter Tuning",
        detail: spsaDone ? "Tuned (+60 Elo)" : "Pending — Run SPSA in 3000 Forge (+60 Elo)",
        done: spsaDone,
        partial: spsaDone ? 100 : 0,
      },
      {
        label: "16×768→1024 Big NNUE v4 Brain",
        detail: nnDone
          ? "Forged & Active (+410 Elo!)"
          : "Pending — Currently using v3 NNUE fallback (+410 Elo when forged!)",
        done: nnDone,
        partial: nnDone ? 100 : 0,
      }
    );

    const pipsHtml = tasks
      .map(
        (tk) =>
          `<span class="apex-pip ${tk.done ? "lit" : tk.partial > 0 ? "partial" : ""}" title="${tk.label}: ${tk.detail}"></span>`
      )
      .join("");

    // Honest dynamic combat stats reflecting current forged level (scaling up to full potential!)
    const scaledStats = {
      tactics: nnDone ? 99 : spsaDone ? 92 : 89,
      intuition: nnDone ? 96 : 80,
      speed: 100,
      creativity: nnDone ? 88 : spsaDone ? 80 : 76,
      endgame: isSyz && syzDone ? 100 : nnDone ? 96 : 84,
    };

    return {
      isSyz,
      stage,
      maxStage,
      pct,
      elo,
      targetElo,
      tier,
      isMax,
      accent,
      tierClass,
      tasks,
      pipsHtml,
      scaledStats,
      badgeText: isMax ? `👑 MAX ${stage}/${maxStage}` : `⚡ Lv ${stage}/${maxStage}`,
      shortStatus: `Lv ${stage}/${maxStage} · ${tier}`,
    };
  }

  function fighterDisplayElo(f) {
    const p = getPresetByLevel(f.level);
    if (isFighterLocked(f)) return f.defaultElo;
    const evo = getApexEvolution(f);
    if (evo) {
      return evo.isMax ? `👑 ~${evo.elo} Elo (MAX)` : `Lv ${evo.stage}/${evo.maxStage} · ~${evo.elo} Elo`;
    }
    if (p && p.has_elo) {
      return p.is_relative ? `${p.est_elo >= 0 ? "+" : ""}${p.est_elo} rel` : `~${p.est_elo} Elo`;
    }
    return f.defaultElo;
  }

  function findFighterByNameOrLevel(name, level = 0) {
    if (level > 0) {
      const byLvl = FIGHTERS_ROSTER.find((f) => f.level === level);
      if (byLvl) return byLvl;
    }
    const lower = (name || "").toLowerCase();
    if (!lower || lower === "human" || lower === "white" || lower === "black" || lower.startsWith("human")) {
      return null;
    }
    if (lower.includes("apex") && lower.includes("syzygy")) return FIGHTERS_ROSTER.find((f) => f.level === 19);
    if (lower.includes("apex")) return FIGHTERS_ROSTER.find((f) => f.level === 18);
    if (lower.includes("gemini")) return FIGHTERS_ROSTER.find((f) => f.level === 14);
    if (lower.includes("oracle") || lower.includes("lmr")) return FIGHTERS_ROSTER.find((f) => f.level === 15);
    if (lower.includes("omega")) return FIGHTERS_ROSTER.find((f) => f.level === 16);
    if (lower.includes("singularity") || lower.includes("distilled")) return FIGHTERS_ROSTER.find((f) => f.level === 17);
    if (lower.includes("zero-c v1") || lower.includes("4320") || lower.includes("chronos")) return FIGHTERS_ROSTER.find((f) => f.level === 13);
    if (lower.includes("zero-c") || lower.includes("alphazero") || lower.includes("nova")) return FIGHTERS_ROSTER.find((f) => f.level === 12);
    if (lower.includes("stockfish")) {
      if (lower.includes("1320")) return FIGHTERS_ROSTER.find((f) => f.level === 7);
      if (lower.includes("1600")) return FIGHTERS_ROSTER.find((f) => f.level === 8);
      if (lower.includes("2000")) return FIGHTERS_ROSTER.find((f) => f.level === 9);
      if (lower.includes("2400")) return FIGHTERS_ROSTER.find((f) => f.level === 10);
      return FIGHTERS_ROSTER.find((f) => f.level === 11);
    }
    if (lower.includes("l1") || lower.includes("novice")) return FIGHTERS_ROSTER.find((f) => f.level === 1);
    if (lower.includes("l2") || lower.includes("casual")) return FIGHTERS_ROSTER.find((f) => f.level === 2);
    if (lower.includes("l3") || lower.includes("club")) return FIGHTERS_ROSTER.find((f) => f.level === 3);
    if (lower.includes("l4") || lower.includes("strong")) return FIGHTERS_ROSTER.find((f) => f.level === 4);
    if (lower.includes("l5") || lower.includes("master")) return FIGHTERS_ROSTER.find((f) => f.level === 5);
    if (lower.includes("l6") || lower.includes("grandmaster") || lower.includes("titan-c")) return FIGHTERS_ROSTER.find((f) => f.level === 6);
    return null;
  }

  function applyStripAvatar(el, playerName, level = 0) {
    if (!el) return;
    const f = findFighterByNameOrLevel(playerName, level);
    if (f) {
      const evo = getApexEvolution(f);
      el.innerHTML = f.avatar;
      el.style.borderColor = evo ? evo.accent : f.accent;
      el.title = evo
        ? `${f.name} [${evo.shortStatus} • ~${evo.elo} Elo] (click to inspect Forge tasks)`
        : `${f.name} — ${f.epithet} (click to read story)`;
      el.dataset.fighterId = f.id;
    } else {
      el.innerHTML = HUMAN_AVATAR_SVG;
      el.style.borderColor = "#58a6ff";
      el.title = `${playerName || "Human Challenger"} (click to open Hall of Champions)`;
      el.dataset.fighterId = "";
    }
  }

  function renderHomeFightersPreview() {
    const box = $("home-fighters-preview");
    if (!box) return;
    const showcase = ["titan-1", "titan-4", "titan-6", "zero-v2", "zero-v1", "hybrid-dual", "hybrid-omega", "sf-11"];
    box.innerHTML = showcase
      .map((id) => {
        const f = FIGHTERS_ROSTER.find((x) => x.id === id);
        if (!f) return "";
        const locked = isFighterLocked(f);
        return `<div class="home-f-pip ${locked ? "locked" : ""}" data-fid="${f.id}" style="border-color:${f.accent}" title="${locked ? "🔒 " : ""}${f.name} — ${f.epithet}">${f.avatar}</div>`;
      })
      .join("");
    box.querySelectorAll("[data-fid]").forEach((el) => {
      el.addEventListener("click", () => openFightersModal(el.dataset.fid));
    });
  }

  function renderFightersGrid() {
    const grid = $("fighters-grid");
    if (!grid) return;
    const list =
      fightersFactionFilter === "all"
        ? FIGHTERS_ROSTER
        : FIGHTERS_ROSTER.filter((f) => f.faction === fightersFactionFilter);

    grid.innerHTML = list
      .map((f) => {
        const locked = isFighterLocked(f);
        const sel = f.id === selectedFighterId ? "selected" : "";
        const evo = getApexEvolution(f);
        const cardAccent = evo ? evo.accent : f.accent;
        const evoCls = evo && !locked ? `apex-evolving ${evo.tierClass}` : "";
        return `<div class="fighter-card ${sel} ${locked ? "locked" : ""} ${evoCls}" data-fid="${f.id}" style="--f-accent:${cardAccent}">
          <span class="fc-level-pill">L${f.level}</span>
          ${
            locked
              ? `<span class="fc-lock-badge">&#128274; SEALED</span>`
              : evo
              ? `<span class="fc-evo-badge ${evo.isMax ? "max" : ""}">${evo.badgeText}</span>`
              : ""
          }
          <div class="fc-avatar">${f.avatar}</div>
          <div class="fc-name">${f.name}</div>
          <div class="fc-epithet">${evo && !locked ? `${evo.tier} (${evo.pct}% forged)` : f.epithet}</div>
          ${evo && !locked ? `<div class="apex-pips-row">${evo.pipsHtml}</div>` : ""}
          <div class="fc-elo">${fighterDisplayElo(f)}</div>
        </div>`;
      })
      .join("");

    grid.querySelectorAll(".fighter-card[data-fid]").forEach((card) => {
      card.addEventListener("click", () => {
        selectedFighterId = card.dataset.fid;
        renderFightersGrid();
        renderFighterDossier(selectedFighterId);
      });
    });
  }

  function renderFighterDossier(fighterId) {
    const box = $("fighter-dossier");
    if (!box) return;
    const f = FIGHTERS_ROSTER.find((x) => x.id === fighterId) || FIGHTERS_ROSTER[0];
    const locked = isFighterLocked(f);
    const preset = getPresetByLevel(f.level);
    const unavailable = !locked && preset && !preset.available;
    const evo = getApexEvolution(f);
    const curStats = evo ? evo.scaledStats : f.stats;
    const accentColor = evo ? evo.accent : f.accent;

    const statRows = [
      ["Tactics", curStats.tactics, f.stats.tactics],
      ["Intuition", curStats.intuition, f.stats.intuition],
      ["Speed", curStats.speed, f.stats.speed],
      ["Creativity", curStats.creativity, f.stats.creativity],
      ["Endgame", curStats.endgame, f.stats.endgame],
    ]
      .map(
        ([lbl, val, maxVal]) => `<div class="fstat-row">
          <span class="fstat-lbl">${lbl}</span>
          <div class="fstat-track">${
            evo && val < maxVal
              ? `<div class="fstat-potential" style="width:${maxVal}%" title="Max potential at Lv ${evo.maxStage}/${evo.maxStage}: ${maxVal}"></div>`
              : ""
          }<div class="fstat-fill" style="width:${val}%"></div></div>
          <span class="fstat-val">${evo && val < maxVal ? `${val}/${maxVal}` : val}</span>
        </div>`
      )
      .join("");

    const isSingularity = f.level === 17;
    const isApexSyzygy = f.level === 19;
    const runtimeUnlocked = Boolean(
      state && state.presets && state.presets.some((p) => p.level === 14 && !p.locked)
    );
    const playLabel = evo ? `${f.name} (Lv ${evo.stage}/${evo.maxStage} ${evo.tier})` : f.name;
    const actionHtml = locked
      ? isApexSyzygy
        ? `<div class="locked-seal-banner">
            <strong>&#128274; Requires 5-Piece Syzygy Tablebases</strong>
            <p>Download the official 3-4-5 piece Syzygy WDL/DTZ files into <code>data/syzygy/</code> (~939 MB) directly from the <strong>&#128640; Road to 3K &rarr; &#128293; 3000 Forge</strong> tab. Once installed, Apex-C + Syzygy unlocks automatically!</p>
          </div>`
        : isSingularity
        ? `<div class="locked-seal-banner">
            <strong>&#128274; Two-Stage Unlock &mdash; Requires Knowledge Distillation Training</strong>
            <p><strong>1. Complete Zero-C v2:</strong> ${runtimeUnlocked ? "&#9989; Done" : "&#9203; In Progress"}<br>
            <strong>2. Distill Compressed AVX2 NNUE:</strong> &#9203; Run <code>src/az/distill_nnue.py</code> from Road to 3K &rarr; Zero-C v2 Lab to train <code>data/chess_net_zero_v4.nnue</code> (~2&ndash;4 min on RTX GPU).</p>
          </div>`
        : `<div class="locked-seal-banner">
            <strong>&#128274; Runtime Mythic Hybrid &mdash; Awaiting Zero-C v2 Completion</strong>
            <p>Pre-wired in <code>src/az/hybrid_engine.py</code> and unlocks automatically the instant Zero-C v2 finishes training and stamps <code>data/az/v2_complete.flag</code>.</p>
          </div>`
      : unavailable
      ? `<div class="locked-seal-banner">
          <strong>Fighter Currently Unavailable</strong>
          <p>${(preset && preset.desc) || "Install Stockfish or train Zero-C first to challenge this fighter."}</p>
        </div>`
      : `<div class="dossier-actions">
          <button class="btn btn-primary" id="btn-f-play-w">&#9812; Play White vs ${playLabel}</button>
          <button class="btn btn-accent" id="btn-f-play-b">&#9818; Play Black vs ${playLabel}</button>
        </div>`;

    const evoPanelHtml = evo
      ? `<div class="apex-evo-panel ${evo.tierClass}">
          <div class="apex-evo-head">
            <div>
              <div class="apex-evo-kicker">DYNAMIC FORGE EVOLUTION &bull; ${evo.stage} OF ${evo.maxStage} TASKS COMPLETE</div>
              <div class="apex-evo-title">${evo.badgeText} &mdash; ${evo.tier} <span>(~${evo.elo} Elo${
                evo.isMax ? " • MAX POWER!" : ` &rarr; Target ${evo.targetElo}+ Elo`
              })</span></div>
            </div>
            ${
              evo.isMax
                ? `<span class="p3k-status-pill" style="color:#ffd97d;border-color:#ffd97d;">&#128081; FULLY FORGED</span>`
                : `<button type="button" class="btn btn-sm btn-orange" id="btn-dossier-open-forge">&#128293; Level Up in 3000 Forge</button>`
            }
          </div>
          <div class="apex-evo-meter-row">
            <div class="apex-pips-row lg">${evo.pipsHtml}</div>
            <span class="apex-evo-pct">${evo.pct}%</span>
          </div>
          <div class="apex-task-list">
            ${evo.tasks
              .map(
                (tk) => `<div class="apex-task-row ${tk.done ? "done" : tk.partial > 0 ? "partial" : ""}">
                  <span class="apex-task-icon">${tk.done ? "&#9989;" : tk.partial > 0 ? "&#9203;" : "&#11036;"}</span>
                  <div class="apex-task-body">
                    <strong>${tk.label}</strong>
                    <small>${tk.detail}</small>
                  </div>
                </div>`
              )
              .join("")}
          </div>
        </div>`
      : "";

    box.style.setProperty("--f-accent", accentColor);
    box.innerHTML = `
      <div class="dossier-hero">
        <div class="dossier-avatar ${evo ? `apex-evolving ${evo.tierClass}` : ""}">${f.avatar}</div>
        <div class="dossier-head-meta">
          <span class="dossier-faction">${f.factionLabel} &bull; ${
            evo ? `Evolution Lv ${evo.stage}/${evo.maxStage}` : `Level ${f.level}`
          }</span>
          <div class="dossier-name">${locked ? "&#128274; " : ""}${f.name}${
            evo ? ` <span class="dossier-evo-pill">${evo.tier}</span>` : ""
          }</div>
          <div class="dossier-title">${f.epithet} &mdash; ${
            evo ? (evo.isMax ? f.botName : `Currently running on ${evo.tasks.find((x) => x.label.includes("NNUE")).done ? "1024 NNUE v4" : "v3 NNUE Prototype"}`) : f.botName
          }</div>
          <div class="dossier-elo-line">${fighterDisplayElo(f)}${
            evo && !evo.isMax ? ` <span style="color:var(--muted);font-weight:500;">(Target: ${evo.targetElo}+ Elo)</span>` : ""
          }</div>
        </div>
      </div>

      ${evoPanelHtml}

      <div class="dossier-quote">${f.quote}</div>

      <div>
        <div class="dossier-section-lbl">Champion Story</div>
        <div class="dossier-story">${f.lore.map((p) => `<p>${p}</p>`).join("")}</div>
      </div>

      <div class="dossier-specs">
        <div class="dossier-spec-box"><span>Fighting Style</span><strong>${f.style}</strong></div>
        <div class="dossier-spec-box"><span>Signature Engine</span><strong>${f.weapon}</strong></div>
      </div>

      <div>
        <div class="dossier-section-lbl">Combat Profile${
          evo && !evo.isMax ? " (Current / Max Potential)" : ""
        }</div>
        <div class="dossier-stats">${statRows}</div>
      </div>

      ${actionHtml}
    `;

    const openForgeBtn = $("btn-dossier-open-forge");
    if (openForgeBtn) {
      openForgeBtn.addEventListener("click", () => {
        closeFightersModal(true);
        openAzModal("forge");
      });
    }

    const playW = $("btn-f-play-w");
    const playB = $("btn-f-play-b");
    if (playW) {
      playW.addEventListener("click", async () => {
        closeFightersModal(true);
        showBoardWorkspace();
        await startNewGame("white", f.level, f.level);
      });
    }
    if (playB) {
      playB.addEventListener("click", async () => {
        closeFightersModal(true);
        showBoardWorkspace();
        await startNewGame("black", f.level, f.level);
      });
    }
  }

  function openFightersModal(focusId = null) {
    if (focusId && FIGHTERS_ROSTER.some((f) => f.id === focusId)) {
      selectedFighterId = focusId;
      fightersFactionFilter = "all";
      if ($("fighters-filters")) {
        $("fighters-filters")
          .querySelectorAll(".chip")
          .forEach((c) => c.classList.toggle("active", c.dataset.faction === "all"));
      }
    }
    renderFightersGrid();
    renderFighterDossier(selectedFighterId);
    $("fighters-modal").classList.remove("hidden");
    syncUrl("/fighters");
    api("/api/p3k/status", "GET").then((p3k) => {
      if (p3k && p3k.ok) {
        p3kLastData = p3k;
        if (!$("fighters-modal").classList.contains("hidden")) {
          renderFightersGrid();
          renderFighterDossier(selectedFighterId);
        }
      }
    });
  }

  function closeFightersModal(silent = false) {
    const m = $("fighters-modal");
    if (!m || m.classList.contains("hidden")) return;
    m.classList.add("hidden");
    if (!silent) syncUrl(underlyingBaseRoute());
  }

  if ($("btn-fighters")) {
    $("btn-fighters").addEventListener("click", () => openFightersModal());
  }
  if ($("btn-home-fighters")) {
    $("btn-home-fighters").addEventListener("click", () => openFightersModal());
  }
  if ($("btn-fighters-close")) {
    $("btn-fighters-close").addEventListener("click", () => closeFightersModal());
  }
  if ($("btn-az-logic-toggle")) {
    $("btn-az-logic-toggle").addEventListener("click", () => {
      const card = $("az-comp-logic-card");
      if (!card) return;
      const nowHidden = card.classList.toggle("hidden");
      $("btn-az-logic-toggle").innerHTML = nowHidden
        ? "&#128216; How Completion Works"
        : "&#128216; Hide Completion Logic";
    });
  }
  if ($("btn-az-unlock-now")) {
    $("btn-az-unlock-now").addEventListener("click", async () => {
      const ok = window.confirm(
        "Graduate Zero-C v2 now?\n\n• Immediately unlocks the 3 Runtime Hybrids (Gemini Arbiter, Oracle of LMR, Zero-C Omega).\n• Unlocks the Stage-2 Knowledge Distillation Forge so you can train Singularity v5's compressed AVX2 NNUE.\n\nYou can still continue training Zero-C v2 anytime afterward."
      );
      if (!ok) return;
      const az = await api("/api/az/complete", "POST", { unlock: true });
      const fresh = await api("/api/state", "GET");
      if (fresh && fresh.ok) {
        state = fresh;
        populatePresets(state.presets);
      }
      renderAzStatus(az);
      launchConfetti();
      logTerm("Zero-C v2 graduated! 3 Runtime Hybrids unlocked & Singularity v5 Distillation Forge ready!", "term-ok");
    });
  }
  if ($("btn-az-distill-start")) {
    $("btn-az-distill-start").addEventListener("click", async () => {
      const res = await api("/api/az/distill", "POST");
      if (!res || !res.ok) {
        logTerm((res && res.error) || "Could not start Singularity v5 distillation.", "term-err");
        return;
      }
      window._singularityUnlockedRefreshed = false;
      renderAzStatus(res);
      startAzPolling();
      logTerm("Started Singularity v5 GPU Knowledge Distillation (Zero-C v2 -> data/chess_net_zero_v4.nnue)!", "term-ok");
    });
  }
  if ($("btn-az-distill-stop")) {
    $("btn-az-distill-stop").addEventListener("click", async () => {
      const res = await api("/api/az/distill/stop", "POST");
      if (res && res.ok) renderAzStatus(res);
      logTerm("Stopped Singularity v5 NNUE distillation.", "term-warn");
    });
  }
  if ($("btn-az-open-hybrids")) {
    $("btn-az-open-hybrids").addEventListener("click", () => {
      closeAzModal(true);
      fightersFactionFilter = "hybrid";
      selectedFighterId = "hybrid-dual";
      openFightersModal();
      if ($("fighters-filters")) {
        $("fighters-filters")
          .querySelectorAll(".chip")
          .forEach((c) => c.classList.toggle("active", c.dataset.faction === "hybrid"));
      }
      renderFightersGrid();
      renderFighterDossier("hybrid-dual");
    });
  }
  if ($("fighters-filters")) {
    $("fighters-filters")
      .querySelectorAll(".chip")
      .forEach((c) => {
        c.addEventListener("click", () => {
          fightersFactionFilter = c.dataset.faction;
          $("fighters-filters")
            .querySelectorAll(".chip")
            .forEach((x) => x.classList.toggle("active", x === c));
          const list =
            fightersFactionFilter === "all"
              ? FIGHTERS_ROSTER
              : FIGHTERS_ROSTER.filter((f) => f.faction === fightersFactionFilter);
          if (list.length > 0 && !list.some((f) => f.id === selectedFighterId)) {
            selectedFighterId = list[0].id;
          }
          renderFightersGrid();
          renderFighterDossier(selectedFighterId);
        });
      });
  }
  for (const avId of ["top-avatar", "bottom-avatar"]) {
    const av = $(avId);
    if (av) {
      av.addEventListener("click", () => openFightersModal(av.dataset.fighterId || null));
    }
  }

  // ==========================================================================
  // Project 3000 Forge, Toy Net Lab & Gamified Road to 3K Campaign Map
  // ==========================================================================
  let p3kSyzWasReady = false;
  let p3kNnueWasReady = false;

  function renderP3kStatus(d) {
    if (!d || !d.ok) return;
    p3kLastData = d;

    // Coexistence guidance banner
    const ban = $("p3k-coexist-banner");
    if (ban) {
      ban.classList.toggle("gpu-free", !d.az_running);
      ban.innerHTML = d.az_running
        ? "<strong>&#129504; Zero-C v2 is actively training on GPU:</strong> Safe to run <strong>Stage 1 (Syzygy Download)</strong> + <strong>Stage 2 (Datagen at 2T)</strong> or <strong>Stage 3 (SPSA at 2T)</strong> alongside it (runs at <code>nice 15</code>, 0% GPU). Pause Zero-C for ~20 min only when ready for <strong>Stage 4 (Big NNUE v4 GPU Train)</strong>."
        : "<strong>&#128640; RTX GPU &amp; CPU are free!</strong> You can run <strong>Datagen at 8&ndash;10T (~12M pos/hr)</strong>, <strong>SPSA at 8T</strong>, or launch <strong>Stage 4 (Big 1024 NNUE v4 GPU Training)</strong> right now!";
    }

    // 1. Syzygy 5-Piece Tablebase Live Downloader & Tracker
    const syz = d.syzygy || {};
    const syzFiles = syz.files || 0;
    const syzTarget = syz.target_files || 290;
    const syzPct = Math.min(100, (syzFiles / Math.max(1, syzTarget)) * 100);
    const syzDown = Boolean(syz.downloading);
    const syzCard = $("p3k-card-syzygy");
    if (syzCard) {
      const syzComplete = syzFiles >= 285 || (syz.ready && syzFiles >= 145 && syz.dtz_files >= 140);
      syzCard.classList.toggle("done", syzComplete);
      syzCard.classList.toggle("running", syzDown || (syzFiles > 0 && !syzComplete));
      if ($("btn-p3k-syz-start")) {
        $("btn-p3k-syz-start").classList.toggle("hidden", syzDown || syzComplete);
        $("btn-p3k-syz-start").innerHTML = syzFiles > 0 ? "&#11015; Resume Syzygy Download" : "&#11015; Download 5-Man Syzygy";
      }
      if ($("btn-p3k-syz-stop")) $("btn-p3k-syz-stop").classList.toggle("hidden", !syzDown);
      $("p3k-syz-pill").textContent = syzComplete
        ? "✓ 5-Man Ready!"
        : syzFiles > 0
        ? `Downloading (${syzFiles}/${syzTarget})`
        : "Not Downloaded";
      $("p3k-syz-fill").style.width = `${syzPct.toFixed(1)}%`;
      $("p3k-syz-pct").textContent = `${Math.round(syzPct)}%`;
      $("p3k-syz-files").textContent = `${syzFiles} / ${syzTarget}`;
      $("p3k-syz-split").textContent = `${syz.wdl_files || 0} / ${syz.dtz_files || 0}`;
      $("p3k-syz-mb").textContent = `${(syz.mb || 0).toFixed(1)} MB`;
      if (syz.ready && !p3kSyzWasReady) {
        p3kSyzWasReady = true;
        api("/api/state", "GET").then((fresh) => {
          if (fresh && fresh.ok) {
            state = fresh;
            populatePresets(state.presets);
          }
        });
      }
    }

    // 2. Self-Play Datagen
    const dg = d.datagen || {};
    const dgd = dg.detail || null;
    const dgRun = Boolean(dg.running);
    const dgCard = $("p3k-card-datagen");
    if (dgCard) {
      dgCard.classList.toggle("running", dgRun);
      dgCard.classList.toggle("done", !dgRun && (dg.file_positions || 0) >= 25000000);
      $("btn-p3k-dg-start").classList.toggle("hidden", dgRun);
      $("btn-p3k-dg-stop").classList.toggle("hidden", !dgRun);
      $("p3k-dg-threads").disabled = dgRun;
      $("p3k-dg-millions").disabled = dgRun;
      const dgPct = dgRun && dgd ? dgd.progress_pct || 0 : Math.min(100, ((dg.file_positions || 0) / 25000000) * 100);
      $("p3k-dg-pill").textContent = dgRun ? `Running (${dgd ? dgd.threads : 2}T)` : (dg.file_positions || 0) > 0 ? "Saved" : "Idle";
      $("p3k-dg-fill").style.width = `${Math.max(0, Math.min(100, dgPct)).toFixed(1)}%`;
      $("p3k-dg-pct").textContent = `${Math.round(dgPct)}%`;
      $("p3k-dg-total").textContent = `${(dg.file_positions || 0).toLocaleString()} (${(dg.file_mb || 0).toFixed(0)} MB)`;
      const pps = dgRun && dgd ? dgd.pos_per_sec || 0 : 0;
      $("p3k-dg-pps").textContent = pps > 0 ? `${Math.round(pps).toLocaleString()} pos/s` : "—";
      $("p3k-dg-games").textContent = dgd ? (dgd.games || 0).toLocaleString() : "0";
      if (dgRun && dgd && pps > 10) {
        const rem = Math.max(0, (dgd.target_positions || 0) - (dgd.session_positions || 0));
        const sec = rem / pps;
        $("p3k-dg-eta").textContent = sec < 3600 ? `${Math.round(sec / 60)}m` : `${(sec / 3600).toFixed(1)}h`;
      } else {
        $("p3k-dg-eta").textContent = "—";
      }
    }

    // 3. SPSA Search Tuner
    const sp = d.spsa || {};
    const spd = sp.detail || null;
    const spRun = Boolean(sp.running);
    const spCard = $("p3k-card-spsa");
    if (spCard) {
      spCard.classList.toggle("running", spRun);
      spCard.classList.toggle("done", !spRun && spd && spd.iter >= spd.max_iters && spd.max_iters > 0);
      $("btn-p3k-spsa-start").classList.toggle("hidden", spRun);
      $("btn-p3k-spsa-stop").classList.toggle("hidden", !spRun);
      $("p3k-spsa-threads").disabled = spRun;
      $("p3k-spsa-iters").disabled = spRun;
      const spPct = spd ? spd.progress_pct || 0 : 0;
      $("p3k-spsa-pill").textContent = spRun ? `Tuning (${spd ? spd.threads : 2}T)` : spd && spd.iter > 0 ? `Saved (Iter ${spd.iter})` : "Idle";
      $("p3k-spsa-fill").style.width = `${Math.max(0, Math.min(100, spPct)).toFixed(1)}%`;
      $("p3k-spsa-pct").textContent = `${Math.round(spPct)}%`;
      $("p3k-spsa-iter").textContent = spd ? `${spd.iter || 0} / ${spd.max_iters || 150}` : "0 / 150";
      $("p3k-spsa-score").textContent = spd && spd.n_games ? `+δ ${spd.plus_score}/${spd.n_games}` : "—";
      if (spd && spd.params) {
        $("p3k-spsa-params").textContent = `RFP ${spd.params.RFPBase} • Fut ${spd.params.FutMult} • LMR ${spd.params.HistLMRDiv}`;
      }
    }

    // 4. Big 16x768 -> 1024 NNUE v4 GPU Trainer
    const nn = d.nnue_v4 || {};
    const nnd = nn.detail || null;
    const nnRun = Boolean(nn.running);
    const nnCard = $("p3k-card-nnue");
    if (nnCard) {
      nnCard.classList.toggle("running", nnRun);
      nnCard.classList.toggle("done", Boolean(nn.ready) && !nnRun);
      $("btn-p3k-nnue-start").classList.toggle("hidden", nnRun);
      $("btn-p3k-nnue-stop").classList.toggle("hidden", !nnRun);
      $("btn-p3k-nnue-start").disabled = d.az_running || (dg.file_positions || 0) < 2048;
      $("p3k-nnue-epochs").disabled = nnRun;
      const nnPct = nnRun && nnd ? nnd.progress_pct || 0 : nn.ready ? 100 : 0;
      $("p3k-nnue-pill").textContent = nnRun
        ? `GPU Epoch ${nnd ? nnd.epoch : 0}/${nnd ? nnd.epochs : 24}`
        : nn.ready
        ? "✓ 1024 NNUE v4 Live!"
        : (dg.file_positions || 0) >= 2048
        ? "Ready to Train"
        : "Waiting for Data";
      $("p3k-nnue-fill").style.width = `${Math.max(0, Math.min(100, nnPct)).toFixed(1)}%`;
      $("p3k-nnue-pct").textContent = `${Math.round(nnPct)}%`;
      $("p3k-nnue-msg").textContent =
        (nnd && nnd.message) ||
        (nn.ready
          ? "data/titan_v6_1024.nnue loaded automatically by Apex-C Pure & Syzygy!"
          : (dg.file_positions || 0) >= 2048
          ? `${(dg.file_positions || 0).toLocaleString()} positions ready — pause Zero-C & click Train!`
          : "Collect positions in Stage 2 first");
      $("p3k-nnue-val").textContent = nnd && nnd.best_val_mse != null ? String(nnd.best_val_mse) : "—";
      if (nn.ready && !p3kNnueWasReady) {
        p3kNnueWasReady = true;
        api("/api/state", "GET").then((fresh) => {
          if (fresh && fresh.ok) {
            state = fresh;
            populatePresets(state.presets);
          }
        });
      }
    }

    // Update Home Card live metrics
    if ($("home-p3k-syz")) {
      $("home-p3k-syz").textContent = `${syzFiles} / ${syzTarget}`;
      $("home-p3k-dg").textContent =
        (dg.file_positions || 0) >= 1e6
          ? `${((dg.file_positions || 0) / 1e6).toFixed(2)}M`
          : `${(dg.file_positions || 0).toLocaleString()}`;
      $("home-p3k-nnue").textContent = nn.ready ? "✓ 1024 Live" : nnRun ? "Training..." : "Pending";
    }
  }

  function renderToyStatus(t) {
    if (!t || !t.ok) return;
    toyLastData = t;
    const det = t.detail || null;
    const run = Boolean(t.running);
    const card = $("r3k-toy-card");
    if (!card) return;
    const gen = det ? det.gen || 0 : 0;
    const games = det ? det.total_games || 0 : 0;
    const hist = det ? det.history || [] : [];
    const lastH = hist.length > 0 ? hist[hist.length - 1] : null;
    const pct = Math.min(100, (gen / 60) * 100);

    card.classList.toggle("running", run);
    card.classList.toggle("done", !run && gen >= 60);
    $("toy-pill").textContent = run ? `Training Gen ${gen}` : gen >= 60 ? "✓ Sprint Done!" : gen > 0 ? `Saved Gen ${gen}` : "Ready";
    $("btn-toy-start").classList.toggle("hidden", run);
    $("btn-toy-stop").classList.toggle("hidden", !run);
    $("btn-toy-start").disabled = Boolean(t.az_running);
    if ($("btn-toy-play")) $("btn-toy-play").classList.toggle("hidden", !t.has_ckpt);
    $("toy-fill").style.width = `${pct.toFixed(1)}%`;
    $("toy-pct").textContent = `${Math.round(pct)}%`;
    $("toy-gen").textContent = `Gen ${gen}`;
    $("toy-games").textContent = games.toLocaleString();
    $("toy-acc").textContent = lastH ? `${lastH.pol_acc}%` : "—";
    $("toy-loss").textContent = lastH ? String(lastH.loss) : "—";
    $("toy-msg").textContent = t.az_running
      ? "Zero-C v2 is using the GPU — pause v2 to run a 15-min Toy Sprint!"
      : (det && det.message) || "Ready for a quick 15-min tabula rasa sprint!";
  }

  function renderRoadCampaign() {
    const az = azLastData || {};
    const p3k = p3kLastData || {};
    const syz = p3k.syzygy || {};
    const dg = p3k.datagen || {};
    const nn = p3k.nnue_v4 || {};
    const comp = az.completion ? az.completion : computeClientCompletion(az);

    const syzFrac = Math.min(1, (syz.files || 0) / Math.max(1, syz.target_files || 290));
    const syzDone = Boolean(syz.ready) && (syz.files || 0) >= 285;
    const zcFrac = az.hybrids_unlocked || comp.is_complete ? 1 : Math.min(0.99, (comp.progress_pct || 0) / 100);
    const zcDone = Boolean(az.hybrids_unlocked || comp.is_complete);
    const singDone = Boolean(az.nnue_distilled);
    const dgFrac = Math.min(1, (dg.file_positions || 0) / 25000000);
    const nnDone = Boolean(nn.ready);
    const forgeFrac = nnDone ? 1 : 0.7 * dgFrac;

    // Master weighted 0..100% progress reflecting exact partial progress on disk!
    const masterPct = Math.min(
      100,
      Math.round(15 + 10 * syzFrac + 30 * zcFrac + 10 * (singDone ? 1 : 0) + 35 * forgeFrac)
    );

    const unlockedCount = FIGHTERS_ROSTER.filter((f) => !isFighterLocked(f)).length;
    const totalCount = FIGHTERS_ROSTER.length;

    if ($("r3k-master-fill")) $("r3k-master-fill").style.width = `${masterPct}%`;
    if ($("r3k-master-pct")) $("r3k-master-pct").textContent = `${masterPct}%`;
    if ($("r3k-master-unlocked")) $("r3k-master-unlocked").textContent = `Challengers: ${unlockedCount} / ${totalCount} Unlocked`;
    if ($("r3k-master-sub")) {
      $("r3k-master-sub").textContent = nnDone
        ? "🏆 3,000+ Elo Grandmaster Crusher Forged! Challenge Apex-C 3000 & the Mythic Hybrids!"
        : `Zero-C v2 at Gen ${az.gen || 0} (${Math.round(zcFrac * 100)}%) • Syzygy ${syz.files || 0}/290 • Datagen ${((dg.file_positions || 0) / 1e6).toFixed(2)}M/25M`;
    }
    if ($("home-r3k-fill")) $("home-r3k-fill").style.width = `${masterPct}%`;
    if ($("home-r3k-pct")) $("home-r3k-pct").textContent = `${masterPct}%`;
    if ($("home-r3k-unlocked")) $("home-r3k-unlocked").textContent = `${unlockedCount} / ${totalCount}`;
    if ($("home-az-pill")) {
      const runningTag = az.running ? "Training • " : "";
      $("home-az-pill").textContent = `${runningTag}Road to 3K • ${masterPct}%`;
      $("home-az-pill").classList.toggle("active", Boolean(az.running || masterPct >= 100));
    }

    const chapters = [
      {
        num: "Chapter 1",
        title: "⚔️ The Iron Guild (Base C11)",
        desc: "Classic Alpha-Beta + Full Correction History + Capture/Cut LMR (~2,550–2,580 Elo). Starts Apex-C at Lv 1/4 Prototype!",
        pct: 100,
        done: true,
        active: false,
        rewards: ["titan-1", "titan-4", "titan-6", "apex-pure"],
        btnTxt: "🛡️ Inspect Guild",
        action: () => {
          closeAzModal(true);
          openFightersModal("titan-6");
        },
      },
      {
        num: "Chapter 2",
        title: "📚 5-Piece Syzygy Vault",
        desc: "One-click ~939 MB endgame tablebase download (0% GPU) — unlocks & evolves Apex-C + Syzygy to Lv 2/5 (+70 Elo)!",
        pct: Math.round(syzFrac * 100),
        done: syzDone,
        active: !syzDone,
        rewards: ["apex-syzygy"],
        btnTxt: syzDone ? "✓ Installed — Open Forge" : "⬇ Open Syzygy Download",
        action: () => setR3kTab("forge", true),
      },
      {
        num: "Chapter 3",
        title: "🧠 Awaken Zero-C v2 (AlphaZero)",
        desc: "Train a pure tabula rasa 10×128 neural mind on your RTX GPU to pass the 4 Convergence Gates.",
        pct: Math.round(zcFrac * 100),
        done: zcDone,
        active: !zcDone,
        rewards: ["zero-v2", "hybrid-dual", "hybrid-lmr", "hybrid-omega"],
        btnTxt: az.running ? "📊 View Live Zero-C Charts" : "🧠 Open Zero-C v2 Lab",
        action: () => setR3kTab("zeroc", true),
      },
      {
        num: "Chapter 4",
        title: "⚡ Distill Singularity v5",
        desc: "Compress Zero-C v2's 3.56M ResNet into a blazing-fast 200ns AVX2 CPU NNUE (~3 min on GPU).",
        pct: singDone ? 100 : 0,
        done: singDone,
        active: zcDone && !singDone,
        rewards: ["hybrid-nnue"],
        btnTxt: singDone ? "✓ Singularity Unlocked!" : "⚡ Go to Distillation Forge",
        action: () => setR3kTab("zeroc", true),
      },
      {
        num: "Chapter 5",
        title: "🔥 Forge 3,000+ Elo Apex-C v4",
        desc: "Generate 25M–50M positions (safe on 2 CPU threads alongside Zero-C!), tune SPSA, & train 1024-wide NNUE v4!",
        pct: Math.round(forgeFrac * 100),
        done: nnDone,
        active: !nnDone,
        rewards: ["apex-pure", "apex-syzygy"],
        btnTxt: "🔥 Open 3000 Forge",
        action: () => setR3kTab("forge", true),
      },
      {
        num: "Side Quest",
        title: "⚡ 15-Min Toy Net Sprint",
        desc: "Short on time? Spin up a fast Tiny 4×64 sandbox net in data/az_toy/ and watch it learn chess in 15 min!",
        pct: toyLastData && toyLastData.detail ? Math.min(100, Math.round(((toyLastData.detail.gen || 0) / 60) * 100)) : 0,
        done: Boolean(toyLastData && toyLastData.detail && (toyLastData.detail.gen || 0) >= 60),
        active: false,
        rewards: ["zero-v1"],
        btnTxt: "⚡ Open Toy Net Lab",
        action: () => setR3kTab("toy", true),
      },
    ];

    const grid = $("r3k-chapters-grid");
    if (grid) {
      grid.innerHTML = chapters
        .map((ch, idx) => {
          const avHtml = ch.rewards
            .map((rid) => {
              const f = FIGHTERS_ROSTER.find((x) => x.id === rid);
              if (!f) return "";
              const lk = isFighterLocked(f);
              return `<span class="r3k-mini-av ${lk ? "locked" : ""}" style="border-color:${f.accent}" title="${lk ? "🔒 " : "✓ "}${f.name}">${f.avatar}</span>`;
            })
            .join("");
          return `<div class="r3k-chap-card ${ch.done ? "done" : ch.active ? "active" : ""}">
            <div class="r3k-chap-top">
              <span class="r3k-chap-num">${ch.num}</span>
              <span class="p3k-status-pill">${ch.done ? "✓ Complete" : `${ch.pct}%`}</span>
            </div>
            <div class="r3k-chap-title">${ch.title}</div>
            <p class="r3k-chap-desc">${ch.desc}</p>
            <div class="p3k-bar"><div class="p3k-bar-fill ${ch.done ? "p3k-fill-gold" : "p3k-fill-orange"}" style="width:${ch.pct}%"></div></div>
            <div class="r3k-chap-reward"><span>Unlocks:</span>${avHtml}</div>
            <button type="button" class="btn btn-sm ${ch.done ? "" : "btn-primary"}" data-chap-idx="${idx}" style="margin-top:auto;">${ch.btnTxt}</button>
          </div>`;
        })
        .join("");
      grid.querySelectorAll("[data-chap-idx]").forEach((btn) => {
        btn.addEventListener("click", () => {
          const c = chapters[parseInt(btn.dataset.chapIdx, 10)];
          if (c && c.action) c.action();
        });
      });
    }

    const strip = $("r3k-trophy-strip");
    if (strip) {
      const bosses = FIGHTERS_ROSTER.filter((f) => f.faction !== "stockfish");
      strip.innerHTML = bosses
        .map((f) => {
          const lk = isFighterLocked(f);
          const evo = getApexEvolution(f);
          const avAccent = evo ? evo.accent : f.accent;
          const evoCls = evo && !lk ? `apex-evolving ${evo.tierClass}` : "";
          return `<div class="r3k-trophy-chip ${lk ? "locked" : ""} ${evoCls}" data-tfid="${f.id}" style="--f-accent:${avAccent}" title="${
            lk
              ? "Locked — click to inspect unlock requirements"
              : evo
              ? `${f.name} — ${evo.shortStatus} (${evo.pct}% forged, click to see Forge checklist)`
              : "Click to inspect & challenge!"
          }">
            <span class="r3k-mini-av ${lk ? "locked" : ""}" style="border-color:${avAccent}">${f.avatar}</span>
            <div>
              <span>${lk ? "🔒 " : ""}${f.name}${
                evo && !lk ? ` <em class="r3k-evo-tag">${evo.badgeText}</em>` : ""
              }</span><br>
              <small>${fighterDisplayElo(f)}</small>
              ${evo && !lk ? `<div class="apex-pips-row sm">${evo.pipsHtml}</div>` : ""}
            </div>
          </div>`;
        })
        .join("");
      strip.querySelectorAll("[data-tfid]").forEach((el) => {
        el.addEventListener("click", () => {
          closeAzModal(true);
          openFightersModal(el.dataset.tfid);
        });
      });
    }
  }

  async function pollP3kStatus() {
    const [az, p3k] = await Promise.all([api("/api/az/status", "GET"), api("/api/p3k/status", "GET")]);
    if (az && az.ok) renderAzStatus(az);
    if (p3k && p3k.ok) renderP3kStatus(p3k);
    renderRoadCampaign();
  }

  if ($("btn-home-p3k")) $("btn-home-p3k").addEventListener("click", () => openAzModal("forge"));

  if ($("btn-p3k-syz-start")) {
    $("btn-p3k-syz-start").addEventListener("click", async () => {
      const res = await api("/api/p3k/syzygy/start", "POST");
      if (res && res.ok) {
        renderP3kStatus(res);
        logTerm("Started 5-Piece Syzygy Tablebase download into data/syzygy/ (~939 MB).", "term-ok");
      } else {
        logTerm((res && res.error) || "Could not start Syzygy download.", "term-err");
      }
    });
  }
  if ($("btn-p3k-syz-stop")) {
    $("btn-p3k-syz-stop").addEventListener("click", async () => {
      const res = await api("/api/p3k/syzygy/stop", "POST");
      if (res && res.ok) renderP3kStatus(res);
      logTerm("Paused Syzygy download (resumable anytime).", "term-warn");
    });
  }
  if ($("btn-toy-start")) {
    $("btn-toy-start").addEventListener("click", async () => {
      const arch = ($("toy-arch") && $("toy-arch").value) || "tiny";
      const res = await api("/api/toy/start", "POST", { arch, reset: false });
      if (res && res.ok) {
        renderToyStatus(res);
        logTerm(`Started 15-min Zero-C Toy Net (${arch}) in data/az_toy/!`, "term-ok");
      } else {
        logTerm((res && res.error) || "Could not start Toy Net.", "term-err");
      }
    });
  }
  if ($("btn-toy-stop")) {
    $("btn-toy-stop").addEventListener("click", async () => {
      const res = await api("/api/toy/stop", "POST");
      if (res && res.ok) renderToyStatus(res);
      logTerm("Paused Toy Net cleanly.", "term-warn");
    });
  }
  if ($("btn-toy-reset")) {
    $("btn-toy-reset").addEventListener("click", async () => {
      if (!window.confirm("Reset ONLY the 15-min Toy Net (data/az_toy/) to Gen 0? Your main Zero-C v2 run is untouched!")) return;
      const arch = ($("toy-arch") && $("toy-arch").value) || "tiny";
      const res = await api("/api/toy/start", "POST", { arch, reset: true });
      if (res && res.ok) renderToyStatus(res);
    });
  }
  if ($("btn-toy-play")) {
    $("btn-toy-play").addEventListener("click", async () => {
      closeAzModal(true);
      showBoardWorkspace();
      await startNewGame("white", 12, 12, "", 0, "data/az_toy/latest.pt");
    });
  }
  if ($("btn-toy-play-v1")) {
    $("btn-toy-play-v1").addEventListener("click", async () => {
      closeAzModal(true);
      showBoardWorkspace();
      await startNewGame("white", 13, 13);
    });
  }

  if ($("btn-p3k-dg-start")) {
    $("btn-p3k-dg-start").addEventListener("click", async () => {
      const threads = parseInt($("p3k-dg-threads").value, 10) || 2;
      const millions = parseInt($("p3k-dg-millions").value, 10) || 25;
      const res = await api("/api/p3k/datagen/start", "POST", { threads, millions, nodes: 5000 });
      if (res && res.ok) {
        renderP3kStatus(res);
        logTerm(`Started Project 3000 Datagen (${threads}T, nice 15, +${millions}M positions).`, "term-ok");
      } else {
        logTerm((res && res.error) || "Could not start Datagen.", "term-err");
      }
    });
  }
  if ($("btn-p3k-dg-stop")) {
    $("btn-p3k-dg-stop").addEventListener("click", async () => {
      const res = await api("/api/p3k/datagen/stop", "POST");
      if (res && res.ok) renderP3kStatus(res);
      logTerm("Stopped Project 3000 Datagen cleanly (all generated positions saved).", "term-warn");
    });
  }
  if ($("btn-p3k-spsa-start")) {
    $("btn-p3k-spsa-start").addEventListener("click", async () => {
      const threads = parseInt($("p3k-spsa-threads").value, 10) || 2;
      const iters = parseInt($("p3k-spsa-iters").value, 10) || 150;
      const res = await api("/api/p3k/spsa/start", "POST", { threads, iters, nodes: 12000 });
      if (res && res.ok) {
        renderP3kStatus(res);
        logTerm(`Started SPSA Search Tuner (${threads}T, nice 15, ${iters} iters).`, "term-ok");
      } else {
        logTerm((res && res.error) || "Could not start SPSA Tuner.", "term-err");
      }
    });
  }
  if ($("btn-p3k-spsa-stop")) {
    $("btn-p3k-spsa-stop").addEventListener("click", async () => {
      const res = await api("/api/p3k/spsa/stop", "POST");
      if (res && res.ok) renderP3kStatus(res);
      logTerm("Stopped SPSA Search Tuner cleanly.", "term-warn");
    });
  }
  if ($("btn-p3k-nnue-start")) {
    $("btn-p3k-nnue-start").addEventListener("click", async () => {
      const epochs = parseInt($("p3k-nnue-epochs").value, 10) || 24;
      const res = await api("/api/p3k/nnue/start", "POST", { epochs });
      if (res && res.ok) {
        p3kNnueWasReady = false;
        renderP3kStatus(res);
        logTerm(`Started Big 1024 NNUE v4 GPU Trainer (${epochs} epochs)!`, "term-ok");
      } else {
        logTerm((res && res.error) || "Could not start Big NNUE v4 training.", "term-err");
      }
    });
  }
  if ($("btn-p3k-nnue-stop")) {
    $("btn-p3k-nnue-stop").addEventListener("click", async () => {
      const res = await api("/api/p3k/nnue/stop", "POST");
      if (res && res.ok) renderP3kStatus(res);
      logTerm("Stopped Big NNUE v4 GPU Trainer.", "term-warn");
    });
  }

  // ==========================================================================
  // Client-Side History API URL Router (Back / Forward / Direct URL / Refresh)
  // ==========================================================================
  let isPoppingRoute = false;

  const ROUTE_TITLES = {
    "/": "♞ Chess-in-C :: Dark Studio",
    "/play": "♞ Play • Chess-in-C",
    "/analysis": "🔍 Analysis • Chess-in-C",
    "/puzzles": "🧩 Tactical Puzzles • Chess-in-C",
    "/fighters": "🛡️ Hall of Champions • Chess-in-C",
    "/road-to-3k": "🚀 Road to 3K • Chess-in-C",
    "/project-3000": "🔥 3000 Forge • Road to 3K",
    "/zero-c": "🧠 Zero-C v2 Lab • Road to 3K",
    "/toy-net": "⚡ Toy Net Sprint • Road to 3K",
    "/spectate": "👁️ Spectate Training • Chess-in-C",
    "/elo-arena": "🏆 Elo Arena • Chess-in-C",
    "/elo-calculator": "🎯 Elo Calculator • Chess-in-C",
    "/history": "📜 Game History • Chess-in-C",
    "/review": "★ Game Review • Chess-in-C",
  };

  function underlyingBaseRoute() {
    if (currentScreen !== "board") return "/";
    if (spectateActive) return spectateGameId >= 0 ? `/spectate/${spectateGameId + 1}` : "/spectate";
    if (state && state.mode === "analysis") return "/analysis";
    if (state && state.mode === "puzzle") return "/puzzles";
    return "/play";
  }

  function syncUrl(path, replace = false) {
    if (isPoppingRoute) return;
    const cleanPath = path && path.startsWith("/") ? path : "/";
    const baseKey = cleanPath.startsWith("/spectate") ? "/spectate" : cleanPath;
    document.title = ROUTE_TITLES[baseKey] || "Chess-in-C :: Dark Studio";
    if (window.location.pathname === cleanPath) return;
    try {
      if (replace) {
        window.history.replaceState({ path: cleanPath }, "", cleanPath);
      } else {
        window.history.pushState({ path: cleanPath }, "", cleanPath);
      }
    } catch (_) {}
  }

  function closeAllOverlayModalsSilent() {
    if (!$("az-modal").classList.contains("hidden")) {
      $("az-modal").classList.add("hidden");
      stopAzPolling();
    }
    if (!$("gauntlet-modal").classList.contains("hidden")) {
      $("gauntlet-modal").classList.add("hidden");
      stopGauntletPolling();
    }
    if ($("history-modal")) $("history-modal").classList.add("hidden");
    if ($("fighters-modal")) $("fighters-modal").classList.add("hidden");
    if ($("review-modal")) $("review-modal").classList.add("hidden");
    if ($("puzzle-modal")) $("puzzle-modal").classList.add("hidden");
    if ($("bots-here-modal")) $("bots-here-modal").classList.add("hidden");
  }

  async function applyRouteFromUrl(rawPath, isBoot = false) {
    const path = (rawPath || "/").replace(/\/+$/, "") || "/";
    const prevPop = isPoppingRoute;
    isPoppingRoute = true;
    try {
      closeAllOverlayModalsSilent();
      if (path === "/" || path === "/index.html") {
        await showHomeMenu();
        pollP3kStatus();
      } else if (path === "/road-to-3k") {
        if (isBoot) await showHomeMenu();
        await openAzModal("map");
      } else if (path === "/project-3000") {
        if (isBoot) await showHomeMenu();
        await openAzModal("forge");
      } else if (path === "/toy-net") {
        if (isBoot) await showHomeMenu();
        await openAzModal("toy");
      } else if (path === "/fighters") {
        if (isBoot) await showHomeMenu();
        openFightersModal();
      } else if (path === "/zero-c") {
        if (isBoot) await showHomeMenu();
        await openAzModal("zeroc");
      } else if (path === "/spectate" || path.startsWith("/spectate/")) {
        const seg = path.split("/")[2];
        const gid = seg && /^\d+$/.test(seg) ? Math.max(0, parseInt(seg, 10) - 1) : -1;
        await startSpectateTraining(gid);
      } else if (path === "/elo-arena") {
        if (isBoot) await showHomeMenu();
        setGauntletTab("gauntlet", false);
        await openGauntletModal();
      } else if (path === "/elo-calculator") {
        if (isBoot) await showHomeMenu();
        setGauntletTab("calc", false);
        await openGauntletModal();
      } else if (path === "/history") {
        if (isBoot) await showHomeMenu();
        await openHistoryModal();
      } else if (path === "/review") {
        showBoardWorkspace();
        if (state && state.total_plies > 0) {
          await openReviewModal();
        }
      } else if (path === "/analysis") {
        stopSpectateTraining(false);
        showBoardWorkspace();
        if (!state || state.mode !== "analysis") {
          await enterAnalysisMode();
        }
      } else if (path === "/puzzles") {
        stopSpectateTraining(false);
        showBoardWorkspace();
        if (!state || state.mode !== "puzzle") {
          await startPuzzle(1);
        }
      } else if (path === "/play") {
        stopSpectateTraining(true);
        showBoardWorkspace();
      } else {
        await showHomeMenu();
      }
      const baseKey = path.startsWith("/spectate") ? "/spectate" : path;
      document.title = ROUTE_TITLES[baseKey] || "Chess-in-C :: Dark Studio";
    } finally {
      isPoppingRoute = prevPop;
    }
  }

  window.addEventListener("popstate", () => {
    applyRouteFromUrl(window.location.pathname, false);
  });

  // ==========================================================================
  // Boot
  // ==========================================================================
  async function boot() {
    try {
      const savedTheme = localStorage.getItem("chess_in_c_theme");
      if (savedTheme) applyBoardTheme(savedTheme);
    } catch (_) {}
    logTerm("Chess-in-C Studio connected to Titan-C v4 (Alpha-Beta NNUE v3) + Zero-C (AlphaZero MCTS).", "term-ok");
    logTerm(
      "Drag/click pieces, right-drag for arrows, or type SAN/commands ('analyze', 'import', 'eval', 'autoflip', 'help').",
      "term-dim"
    );
    const data = await api("/api/state", "GET");
    applyState(data, false);
    if (state && state.stockfish_available) {
      logTerm(`Stockfish ready: ${state.stockfish_name} (levels 7-11 unlocked!).`, "term-ok");
    } else {
      logTerm("Stockfish not found (install with 'sudo apt install stockfish' to unlock levels 7-11).", "term-dim");
    }
    const initPath = window.location.pathname || "/";
    await applyRouteFromUrl(initPath, true);
    try {
      window.history.replaceState({ path: initPath }, "", initPath);
    } catch (_) {}
  }

  boot();
})();
