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
    ];
    for (const t of targets) {
      if (!t.el) continue;
      const prev = t.el.value || t.def;
      t.el.innerHTML = "";
      for (const p of presets) {
        let eloTag = "";
        if (p.engine === "c" && p.has_elo) {
          eloTag = p.is_relative ? ` (${p.est_elo >= 0 ? "+" : ""}${p.est_elo} rel)` : ` (~${p.est_elo} Elo)`;
        }
        const prefix = p.engine === "c" ? "Titan-C " : "";
        const label = p.available
          ? `${p.level} - ${prefix}${p.name}${eloTag}`
          : `${p.level} - ${prefix}${p.name} (unavailable)`;
        const opt = new Option(label, String(p.level));
        opt.title = p.desc;
        if (!p.available) opt.disabled = true;
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

    const eloMini = $("home-elo-mini");
    if (eloMini && presets) {
      const cPresets = presets.filter((p) => p.engine === "c");
      if (cPresets.some((p) => p.has_elo)) {
        eloMini.innerHTML = cPresets
          .map(
            (p) =>
              `<div class="elo-mini-pill"><span>L${p.level} ${p.name}</span><strong>${
                p.has_elo ? `~${p.est_elo}` : "—"
              }</strong></div>`
          )
          .join("");
      }
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
    $("top-dot").className = `player-dot ${topIsWhite ? "white" : "black"}`;
    $("bottom-dot").className = `player-dot ${topIsWhite ? "black" : "white"}`;
    $("top-name").textContent = topIsWhite ? state.white_name : state.black_name;
    $("bottom-name").textContent = topIsWhite ? state.black_name : state.white_name;

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
  // Post-Game Review Modal + Clickable Move Jumping into Analysis!
  // ==========================================================================
  function buildEvalChartSvg(timeline) {
    if (!timeline || timeline.length < 2) return "";
    const w = 700;
    const h = 110;
    const pts = timeline.map((wp, i) => {
      const x = (i / (timeline.length - 1)) * w;
      const y = (1 - wp) * (h - 10) + 5;
      return `${x.toFixed(1)},${y.toFixed(1)}`;
    });
    const areaWhite = `0,${h / 2} ${pts.join(" ")} ${w},${h / 2}`;
    return `
      <svg viewBox="0 0 ${w} ${h}" preserveAspectRatio="none">
        <rect x="0" y="0" width="${w}" height="${h / 2}" fill="rgba(255,255,255,0.03)" />
        <rect x="0" y="${h / 2}" width="${w}" height="${h / 2}" fill="rgba(0,0,0,0.22)" />
        <line x1="0" y1="${h / 2}" x2="${w}" y2="${h / 2}" stroke="#303846" stroke-dasharray="4 4" />
        <polygon points="${areaWhite}" fill="rgba(88, 166, 255, 0.22)" />
        <polyline points="${pts.join(" ")}" fill="none" stroke="#58a6ff" stroke-width="2.5" />
      </svg>`;
  }

  function badgeHtml(rating) {
    const map = {
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
    const revEngName = state.stockfish_available ? "Stockfish" : "Titan-C";
    $("review-sub").textContent = `Analyzing ${state.total_plies} plies with ${revEngName}...`;
    $("review-body").innerHTML = `<div class="empty-hint">Computing accuracy, turning points, and best alternatives...</div>`;

    const rev = await api("/api/review", "POST");
    if (!rev.ok) {
      $("review-sub").textContent = rev.error || "Review failed.";
      return;
    }

    const gradedBy = rev.engine === "stockfish" ? "Stockfish" : "Titan-C";
    $("review-sub").textContent = `${state.white_name} vs ${state.black_name} (${state.total_plies} plies • Graded by ${gradedBy}) — click any move to inspect in Analysis`;

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

    const rowsHtml = rev.moves
      .map((m) => {
        const mv = `${m.move_num}${m.side === "b" ? "..." : "."}`;
        const sugg =
          (m.rating === "inaccuracy" || m.rating === "mistake" || m.rating === "blunder") &&
          m.played !== m.best
            ? `Best: <strong class="badge-best">${m.best}</strong> <span class="term-dim">(-${m.cpl} cp)</span>`
            : "";
        return `<tr data-jump-ply="${m.ply}">
          <td>${mv}</td>
          <td><strong>${m.played}</strong></td>
          <td>${badgeHtml(m.rating)}</td>
          <td>${m.eval_str.trim()}</td>
          <td>${sugg}</td>
        </tr>`;
      })
      .join("");

    $("review-body").innerHTML = `
      <div class="review-summary">
        ${renderAccCard(state.white_name, rev.white)}
        ${renderAccCard(state.black_name, rev.black)}
      </div>
      <div class="eval-chart-box">${buildEvalChartSvg(rev.timeline)}</div>
      ${turningHtml}
      <table class="review-table">
        <thead>
          <tr><th>Move</th><th>Played</th><th>Rating</th><th>Eval</th><th>Engine Suggestion</th></tr>
        </thead>
        <tbody>${rowsHtml}</tbody>
      </table>`;

    $("review-body")
      .querySelectorAll("[data-jump-ply]")
      .forEach((el) => {
        el.addEventListener("click", async () => {
          const ply = parseInt(el.dataset.jumpPly, 10);
          $("review-modal").classList.add("hidden");
          await jumpToPlyInAnalysis(ply);
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
  // Live Multithreaded Elo Gauntlet Modal
  // ==========================================================================
  let gauntletPollTimer = null;

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

    // Populate thread selector once
    const thSel = $("gt-threads");
    if (thSel.options.length === 0) {
      const maxC = gs.cpu_cores || 4;
      for (let c = 1; c <= maxC; c++) {
        const opt = new Option(`${c} ${c === 1 ? "thread" : "threads"}${c === maxC ? " (all cores)" : ""}`, String(c));
        thSel.add(opt);
      }
      thSel.value = String(maxC);
    }

    const oppSel = $("gt-opponent");
    if (state && !state.stockfish_available) {
      oppSel.options[0].disabled = true;
      oppSel.options[0].textContent = "Stockfish (install stockfish first)";
      oppSel.value = "internal";
    }

    $("btn-gauntlet-start").classList.toggle("hidden", gs.running);
    $("btn-gauntlet-stop").classList.toggle("hidden", !gs.running);

    const pct =
      gs.scheduled_games > 0
        ? Math.min(100, (gs.completed_games / gs.scheduled_games) * 100)
        : 0;
    $("gt-progress-fill").style.width = `${pct.toFixed(1)}%`;

    if (gs.running) {
      $("gt-progress-text").textContent = `${gs.completed_games}/${gs.scheduled_games} games • ${gs.num_threads} threads • ${gs.elapsed_sec.toFixed(1)}s`;
      $("gauntlet-sub").textContent = `Running vs ${gs.opponent} (${gs.num_threads} parallel threads) • Auto-saving to ${gs.elo_file}`;
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
        return `<tr>
          <td><strong>Titan-C L${lp.level} ${lp.name}</strong></td>
          <td>${lp.opponent}</td>
          <td class="${stCls}">${stTxt}</td>
          <td>${wdl}</td>
          <td>${score}</td>
          <td><strong>${eloStr}</strong></td>
        </tr>`;
      })
      .join("");

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
    const gs = await api("/api/gauntlet/status", "GET");
    renderGauntletStatus(gs);
    if (gs.running) {
      startGauntletPolling();
    }
  }

  function closeGauntletModal() {
    $("gauntlet-modal").classList.add("hidden");
    stopGauntletPolling();
  }

  // ==========================================================================
  // Zero-C AlphaZero Self-Play Training Studio Modal
  // ==========================================================================
  let azPollTimer = null;

  function buildLineChartSvg(seriesList, yMin, yMax, unit = "") {
    const W = 440, H = 138, padL = 36, padR = 12, padT = 12, padB = 22;
    const pw = W - padL - padR, ph = H - padT - padB;
    const maxLen = Math.max(...seriesList.map((s) => s.vals.length));
    if (maxLen < 1) return `<div class="empty-hint">Waiting for Gen 1...</div>`;
    const span = Math.max(1e-5, yMax - yMin);
    const sx = (i) => padL + (maxLen === 1 ? pw / 2 : (i / (maxLen - 1)) * pw);
    const sy = (v) => padT + ph - ((Math.max(yMin, Math.min(yMax, v)) - yMin) / span) * ph;

    let grid = "";
    for (let g = 0; g <= 2; g++) {
      const val = yMin + (span * g) / 2;
      const y = sy(val).toFixed(1);
      grid += `<line x1="${padL}" y1="${y}" x2="${W - padR}" y2="${y}" stroke="rgba(139,148,158,0.18)" stroke-dasharray="3,3"/>`;
      grid += `<text x="${padL - 5}" y="${Number(y) + 3}" fill="#8b949e" font-size="9" text-anchor="end">${val.toFixed(unit === "%" ? 0 : 1)}${unit}</text>`;
    }

    let paths = "";
    for (const s of seriesList) {
      if (s.vals.length === 0) continue;
      const pts = s.vals.map((v, i) => `${sx(i).toFixed(1)},${sy(v).toFixed(1)}`).join(" ");
      if (s.vals.length === 1) {
        paths += `<circle cx="${sx(0).toFixed(1)}" cy="${sy(s.vals[0]).toFixed(1)}" r="4" fill="${s.color}"/>`;
      } else {
        paths += `<polyline fill="none" stroke="${s.color}" stroke-width="2" stroke-linejoin="round" points="${pts}"/>`;
        const last = s.vals[s.vals.length - 1];
        paths += `<circle cx="${sx(s.vals.length - 1).toFixed(1)}" cy="${sy(last).toFixed(1)}" r="3.2" fill="${s.color}"/>`;
      }
    }

    return `<svg viewBox="0 0 ${W} ${H}" class="az-svg">${grid}${paths}<text x="${W / 2}" y="${H - 4}" fill="#8b949e" font-size="9" text-anchor="middle">Generations (1..${maxLen})</text></svg>`;
  }

  function renderAzStatus(az) {
    if (!az || !az.ok) return;
    const isRunning = Boolean(az.running && az.proc_alive !== false);

    $("btn-az-start").classList.toggle("hidden", isRunning);
    $("btn-az-stop").classList.toggle("hidden", !isRunning);
    const badge = $("az-phase-badge");
    badge.textContent = isRunning ? (az.phase === "training" ? "FP16 TRAINING" : "SELF-PLAY") : "IDLE";
    badge.className = "az-phase-badge " + (isRunning ? (az.phase === "training" ? "az-ph-train" : "az-ph-play") : "az-ph-idle");

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
      ? `Saved at Gen ${az.gen} (${az.total_games} total games)`
      : "Ready — click Start Training";

    $("az-m-gen").textContent = `Gen ${az.gen || 0}`;
    $("az-m-arch").textContent = `${az.arch || "6x96 SE-ResNet"}${az.params ? ` • ${(az.params / 1e6).toFixed(2)}M` : ""}`;
    $("az-m-games").textContent = String(az.total_games || 0);
    $("az-m-gph").textContent = `${Math.round(az.games_per_hr || 0).toLocaleString()} games/hr`;
    $("az-m-pos").textContent = (az.total_positions || 0).toLocaleString();
    $("az-m-replay").textContent = `Replay: ${(az.replay_size || 0).toLocaleString()}`;
    $("az-m-eps").textContent = `${Math.round(az.evals_per_sec || 0).toLocaleString()} /s`;
    const vramStr = `${az.vram_mb || 0} MB`;
    const thrSub = [];
    if (az.avg_batch) thrSub.push(`Batch ~${Math.round(az.avg_batch)}`);
    thrSub.push(vramStr);
    if (az.cpu_pct) thrSub.push(`CPU ${Math.round(az.cpu_pct)}%`);
    $("az-m-vram").textContent = thrSub.join(" • ");

    const hist = az.history || [];
    const lastH = hist.length > 0 ? hist[hist.length - 1] : null;
    $("az-m-acc").textContent = lastH ? `${lastH.pol_acc}%` : "—";
    $("az-m-loss").textContent = lastH ? `Loss: ${lastH.loss}` : "Loss: —";

    const oc = az.outcomes || { white: 0, draw: 0, black: 0 };
    const ocTot = oc.white + oc.draw + oc.black;
    const decPct = ocTot > 0 ? `${Math.round(((oc.white + oc.black) / ocTot) * 100)}%` : "—";
    $("az-m-wdl").textContent = `${oc.white} / ${oc.draw} / ${oc.black}`;
    $("az-m-dec").textContent = `Decisive: ${decPct}`;

    // Charts
    if (hist.length > 0) {
      const totVals = hist.map((h) => h.loss);
      const polVals = hist.map((h) => h.pol_loss);
      const wdlVals = hist.map((h) => h.wdl_loss);
      const maxL = Math.max(2.0, ...totVals) * 1.08;
      $("az-loss-chart").innerHTML = buildLineChartSvg(
        [
          { vals: totVals, color: "#f778ba" },
          { vals: polVals, color: "#58a6ff" },
          { vals: wdlVals, color: "#3fb950" },
        ],
        0,
        maxL
      );
      $("az-acc-chart").innerHTML = buildLineChartSvg(
        [
          { vals: hist.map((h) => h.pol_acc), color: "#a371f7" },
          { vals: hist.map((h) => h.decisive_pct), color: "#e3b341" },
        ],
        0,
        100,
        "%"
      );
    } else {
      $("az-loss-chart").innerHTML = `<div class="empty-hint">Loss curves appear after Gen 1 completes.</div>`;
      $("az-acc-chart").innerHTML = `<div class="empty-hint">Accuracy &amp; decisive curves appear after Gen 1 completes.</div>`;
    }

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
          return `<div class="worker-card az-live-card" data-fen="${g.fen}">
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
      liveGrid.querySelectorAll(".worker-card[data-fen]").forEach((card) => {
        card.addEventListener("click", async () => {
          const fen = card.dataset.fen;
          closeAzModal();
          showBoardWorkspace();
          await loadFenString(fen);
          await enterAnalysisMode();
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
      tbody.innerHTML = `<tr><td colspan="7" class="empty-hint">No checkpoints saved yet.</td></tr>`;
    }

    // Update Home Card 7 live summary too!
    if ($("home-az-pill")) {
      $("home-az-pill").textContent = `${isRunning ? "Training" : "Idle"} • Gen ${az.gen || 0}`;
      $("home-az-pill").classList.toggle("active", isRunning);
      $("home-az-gen").textContent = String(az.gen || 0);
      $("home-az-games").textContent = String(az.total_games || 0);
      $("home-az-pos").textContent = (az.total_positions || 0).toLocaleString();
      $("home-az-dev").textContent = az.device || "Ready";
    }
  }

  async function pollAzStatus() {
    const az = await api("/api/az/status", "GET");
    renderAzStatus(az);
  }

  function startAzPolling() {
    stopAzPolling();
    azPollTimer = setInterval(pollAzStatus, 1200);
  }

  function stopAzPolling() {
    if (azPollTimer) {
      clearInterval(azPollTimer);
      azPollTimer = null;
    }
  }

  async function openAzModal() {
    $("az-modal").classList.remove("hidden");
    await pollAzStatus();
    startAzPolling();
  }

  function closeAzModal() {
    $("az-modal").classList.add("hidden");
    stopAzPolling();
  }

  // ==========================================================================
  // Event Wiring
  // ==========================================================================
  $("btn-gauntlet").addEventListener("click", () => openGauntletModal());
  $("btn-az-lab").addEventListener("click", () => openAzModal());
  $("btn-az-close").addEventListener("click", () => closeAzModal());
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
  $("btn-az-reset").addEventListener("click", async () => {
    const res = await api("/api/az/reset", "POST");
    renderAzStatus(res);
    logTerm("Zero-C checkpoints reset to Gen 0.", "term-warn");
  });
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
  $("btn-gauntlet-stop").addEventListener("click", async () => {
    const gs = await api("/api/gauntlet/stop", "POST");
    renderGauntletStatus(gs);
    logTerm("Stopping Elo gauntlet after active moves finish...", "term-warn");
  });

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
  $("btn-review-close").addEventListener("click", () => $("review-modal").classList.add("hidden"));
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

  function showBoardWorkspace() {
    currentScreen = "board";
    $("home-screen").classList.add("hidden");
    $("workspace").classList.remove("hidden");
    $("game-top-controls").classList.remove("hidden");
    $("btn-home").classList.remove("btn-toggled");
  }

  async function showHomeMenu() {
    currentScreen = "home";
    clearWatchTimer();
    $("workspace").classList.add("hidden");
    $("home-screen").classList.remove("hidden");
    $("game-top-controls").classList.add("hidden");
    $("btn-home").classList.add("btn-toggled");
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
    clearWatchTimer();
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
  $("btn-hist-close").addEventListener("click", () => $("history-modal").classList.add("hidden"));
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
    await showHomeMenu();
  }

  boot();
})();
