"""ctypes bindings for Chess-in-C's native multithreaded batched AlphaZero MCTS library."""
import ctypes
import os
import numpy as np

AZ_PLANES = 20
AZ_BOARD_FLOATS = AZ_PLANES * 64
AZ_NUM_ACTIONS = 64 * 73  # 4672
MAX_LEGAL_MOVES = 512
MAX_FEN_LENGTH = 100
REPLAY_MAX_POL = 96


class AzConfig(ctypes.Structure):
    _fields_ = [
        ("num_games", ctypes.c_int),
        ("fast_sims", ctypes.c_int),
        ("full_sims", ctypes.c_int),
        ("full_search_prob", ctypes.c_float),
        ("c_puct", ctypes.c_float),
        ("fpu_reduction", ctypes.c_float),
        ("dirichlet_alpha", ctypes.c_float),
        ("dirichlet_eps", ctypes.c_float),
        ("temp_plies", ctypes.c_int),
        ("max_plies", ctypes.c_int),
        ("batch_per_game", ctypes.c_int),
        ("forced_playouts", ctypes.c_int),
        ("num_threads", ctypes.c_int),
        ("seed", ctypes.c_uint64),
        ("resign_threshold", ctypes.c_float),
        ("tactical_mode", ctypes.c_int),
    ]


class AzSample(ctypes.Structure):
    _fields_ = [
        ("planes", ctypes.c_float * AZ_BOARD_FLOATS),
        ("actions", ctypes.c_int32 * MAX_LEGAL_MOVES),
        ("probs", ctypes.c_float * MAX_LEGAL_MOVES),
        ("num_legal", ctypes.c_int32),
        ("wdl", ctypes.c_float * 3),
        ("moves_left", ctypes.c_float),
        ("is_full_search", ctypes.c_int32),
    ]


class AzGameSnapshot(ctypes.Structure):
    _fields_ = [
        ("game_id", ctypes.c_int32),
        ("game_seq", ctypes.c_int32),
        ("ply", ctypes.c_int32),
        ("sims_done", ctypes.c_int32),
        ("sims_target", ctypes.c_int32),
        ("is_full_search", ctypes.c_int32),
        ("root_w", ctypes.c_float),
        ("root_d", ctypes.c_float),
        ("root_l", ctypes.c_float),
        ("root_ml", ctypes.c_float),
        ("fen", ctypes.c_char * MAX_FEN_LENGTH),
        ("board", ctypes.c_char * 65),
        ("last_san", ctypes.c_char * 16),
        ("last_uci", ctypes.c_char * 8),
        ("top_count", ctypes.c_int32),
        ("top_san", (ctypes.c_char * 16) * 3),
        ("top_uci", (ctypes.c_char * 8) * 3),
        ("top_visits", ctypes.c_int32 * 3),
        ("top_prior", ctypes.c_float * 3),
        ("top_q", ctypes.c_float * 3),
    ]


def load_az_lib(lib_path=None):
    if lib_path is None:
        root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
        lib_path = os.path.join(root, "build", "libaz_mcts.so")
    if not os.path.exists(lib_path):
        raise FileNotFoundError(
            f"Missing {lib_path}. Build it first with: make az-lib"
        )
    lib = ctypes.CDLL(lib_path)

    lib.az_global_init.argtypes = []
    lib.az_global_init.restype = None

    lib.az_arena_create.argtypes = [ctypes.POINTER(AzConfig)]
    lib.az_arena_create.restype = ctypes.c_void_p

    lib.az_arena_destroy.argtypes = [ctypes.c_void_p]
    lib.az_arena_destroy.restype = None

    lib.az_arena_num_threads.argtypes = [ctypes.c_void_p]
    lib.az_arena_num_threads.restype = ctypes.c_int

    lib.az_arena_reset_game.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p]
    lib.az_arena_reset_game.restype = None

    lib.az_arena_set_position_uci.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.c_char_p,
        ctypes.c_char_p,
    ]
    lib.az_arena_set_position_uci.restype = None

    lib.az_arena_collect_leaves.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
    ]
    lib.az_arena_collect_leaves.restype = ctypes.c_int

    lib.az_arena_apply_evals.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_float),
    ]
    lib.az_arena_apply_evals.restype = None

    lib.az_arena_step_moves.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
    ]
    lib.az_arena_step_moves.restype = ctypes.c_int

    lib.az_arena_pop_samples.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(AzSample),
        ctypes.c_int,
    ]
    lib.az_arena_pop_samples.restype = ctypes.c_int

    lib.az_arena_pending_samples.argtypes = [ctypes.c_void_p]
    lib.az_arena_pending_samples.restype = ctypes.c_int

    lib.az_arena_get_snapshot.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.POINTER(AzGameSnapshot),
    ]
    lib.az_arena_get_snapshot.restype = ctypes.c_bool

    lib.az_arena_best_move.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.c_float,
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_float),
    ]
    lib.az_arena_best_move.restype = ctypes.c_bool

    lib.az_global_init()
    return lib


class AzArena:
    """High-level zero-copy NumPy / pinned-memory wrapper around native C batched MCTS."""

    def __init__(self, cfg: AzConfig, lib=None, pinned_torch=None):
        self.lib = lib or load_az_lib()
        self.cfg = cfg
        self.ptr = self.lib.az_arena_create(ctypes.byref(cfg))
        if not self.ptr:
            raise RuntimeError("Failed to create C az_arena")

        self.num_threads = int(self.lib.az_arena_num_threads(self.ptr))
        self.max_batch = min(4096, max(16, cfg.num_games * cfg.batch_per_game))
        # Round pinned tensor capacity up to a Tensor-Core-friendly cuDNN bucket
        self.buf_cap = self.max_batch
        for _b in (64, 128, 256, 512, 1024, 2048, 4096):
            if self.max_batch <= _b:
                self.buf_cap = _b
                break
        self.pinned = False

        if pinned_torch is not None and pinned_torch.cuda.is_available():
            self.pinned = True
            self.planes_t = pinned_torch.zeros(
                (self.buf_cap, AZ_PLANES, 8, 8), dtype=pinned_torch.float32, pin_memory=True
            )
            self.legal_counts_t = pinned_torch.zeros(
                (self.buf_cap,), dtype=pinned_torch.int32, pin_memory=True
            )
            self.legal_actions_t = pinned_torch.zeros(
                (self.buf_cap, MAX_LEGAL_MOVES), dtype=pinned_torch.int32, pin_memory=True
            )
            self.priors_t = pinned_torch.zeros(
                (self.buf_cap, MAX_LEGAL_MOVES), dtype=pinned_torch.float32, pin_memory=True
            )
            self.wdl_t = pinned_torch.zeros(
                (self.buf_cap, 3), dtype=pinned_torch.float32, pin_memory=True
            )
            self.ml_t = pinned_torch.zeros(
                (self.buf_cap,), dtype=pinned_torch.float32, pin_memory=True
            )
            self.planes_buf = self.planes_t.numpy()
            self.legal_counts = self.legal_counts_t.numpy()
            self.legal_actions = self.legal_actions_t.numpy()
            self.priors_buf = self.priors_t.numpy()
            self.wdl_buf = self.wdl_t.numpy()
            self.ml_buf = self.ml_t.numpy()
        else:
            self.planes_buf = np.zeros((self.max_batch, AZ_PLANES, 8, 8), dtype=np.float32)
            self.legal_counts = np.zeros((self.max_batch,), dtype=np.int32)
            self.legal_actions = np.zeros((self.max_batch, MAX_LEGAL_MOVES), dtype=np.int32)
            self.priors_buf = np.zeros((self.max_batch, MAX_LEGAL_MOVES), dtype=np.float32)
            self.wdl_buf = np.zeros((self.max_batch, 3), dtype=np.float32)
            self.ml_buf = np.zeros((self.max_batch,), dtype=np.float32)
            self.planes_t = None
            self.legal_counts_t = None
            self.legal_actions_t = None
            self.priors_t = None
            self.wdl_t = None
            self.ml_t = None

        self._planes_ptr = self.planes_buf.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        self._lc_ptr = self.legal_counts.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
        self._la_ptr = self.legal_actions.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
        self._priors_ptr = self.priors_buf.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        self._wdl_ptr = self.wdl_buf.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        self._ml_ptr = self.ml_buf.ctypes.data_as(ctypes.POINTER(ctypes.c_float))

    def close(self):
        if getattr(self, "ptr", None):
            self.lib.az_arena_destroy(self.ptr)
            self.ptr = None

    def __del__(self):
        self.close()

    def set_position(self, game_idx: int, fen: str = None, moves: str = ""):
        fen_b = fen.encode("utf-8") if fen else None
        mv_b = moves.encode("utf-8") if moves else b""
        self.lib.az_arena_set_position_uci(self.ptr, game_idx, fen_b, mv_b)

    def collect_leaves(self) -> int:
        return int(
            self.lib.az_arena_collect_leaves(
                self.ptr,
                self.max_batch,
                self._planes_ptr,
                self._lc_ptr,
                self._la_ptr,
            )
        )

    def apply_evals(self, batch_size: int):
        self.lib.az_arena_apply_evals(
            self.ptr,
            batch_size,
            self._priors_ptr,
            self._wdl_ptr,
            self._ml_ptr,
        )

    def step_moves(self, auto_reset: bool = True):
        ww = ctypes.c_int32(0)
        dr = ctypes.c_int32(0)
        bw = ctypes.c_int32(0)
        pl = ctypes.c_int32(0)
        fin = int(
            self.lib.az_arena_step_moves(
                self.ptr,
                1 if auto_reset else 0,
                ctypes.byref(ww),
                ctypes.byref(dr),
                ctypes.byref(bw),
                ctypes.byref(pl),
            )
        )
        return fin, ww.value, dr.value, bw.value, pl.value

    def pop_samples_compact(self, max_samples: int = 4096):
        """Drain completed samples directly into compact vectorized NumPy arrays."""
        pending = int(self.lib.az_arena_pending_samples(self.ptr))
        if pending <= 0:
            return None
        take = min(pending, max_samples)
        arr = (AzSample * take)()
        got = int(self.lib.az_arena_pop_samples(self.ptr, arr, take))
        if got <= 0:
            return None

        planes_u8 = np.empty((got, AZ_PLANES, 8, 8), dtype=np.uint8)
        pol_acts = np.zeros((got, REPLAY_MAX_POL), dtype=np.int16)
        pol_probs = np.zeros((got, REPLAY_MAX_POL), dtype=np.float16)
        pol_n = np.zeros((got,), dtype=np.uint8)
        wdl = np.empty((got, 3), dtype=np.float32)
        ml = np.empty((got,), dtype=np.float32)
        is_full = np.empty((got,), dtype=np.uint8)

        for i in range(got):
            s = arr[i]
            p_arr = np.ctypeslib.as_array(s.planes).reshape(AZ_PLANES, 8, 8)
            planes_u8[i] = np.clip(np.rint(p_arr * 255.0), 0.0, 255.0).astype(np.uint8)

            nl = int(s.num_legal)
            acts = np.ctypeslib.as_array(s.actions)[:nl]
            probs = np.ctypeslib.as_array(s.probs)[:nl]
            if nl > REPLAY_MAX_POL:
                top_idx = np.argpartition(probs, -REPLAY_MAX_POL)[-REPLAY_MAX_POL:]
                acts = acts[top_idx]
                probs = probs[top_idx]
                ps = float(probs.sum())
                if ps > 1e-8:
                    probs = probs / ps
                nl = REPLAY_MAX_POL
            if nl > 0:
                pol_acts[i, :nl] = acts.astype(np.int16, copy=False)
                pol_probs[i, :nl] = probs.astype(np.float16, copy=False)
            pol_n[i] = nl
            wdl[i] = np.ctypeslib.as_array(s.wdl)
            ml[i] = float(s.moves_left)
            is_full[i] = 1 if s.is_full_search else 0

        return planes_u8, pol_acts, pol_probs, pol_n, wdl, ml, is_full

    def pop_samples(self, max_samples: int = 2048):
        pending = int(self.lib.az_arena_pending_samples(self.ptr))
        if pending <= 0:
            return []
        take = min(pending, max_samples)
        arr = (AzSample * take)()
        got = int(self.lib.az_arena_pop_samples(self.ptr, arr, take))
        out = []
        for i in range(got):
            s = arr[i]
            nl = int(s.num_legal)
            planes = np.ctypeslib.as_array(s.planes).reshape(AZ_PLANES, 8, 8).copy()
            acts = np.ctypeslib.as_array(s.actions)[:nl].copy()
            probs = np.ctypeslib.as_array(s.probs)[:nl].copy()
            wdl = np.ctypeslib.as_array(s.wdl).copy()
            out.append(
                (planes, acts, probs, wdl, float(s.moves_left), bool(s.is_full_search))
            )
        return out

    def get_snapshot(self, game_idx: int):
        snap = AzGameSnapshot()
        if not self.lib.az_arena_get_snapshot(self.ptr, game_idx, ctypes.byref(snap)):
            return None
        top = []
        for k in range(int(snap.top_count)):
            top.append(
                {
                    "san": snap.top_san[k].value.decode("utf-8", errors="ignore"),
                    "uci": snap.top_uci[k].value.decode("utf-8", errors="ignore"),
                    "visits": int(snap.top_visits[k]),
                    "prior": round(float(snap.top_prior[k]), 3),
                    "q": round(float(snap.top_q[k]), 3),
                }
            )
        return {
            "id": int(snap.game_id),
            "seq": int(snap.game_seq),
            "ply": int(snap.ply),
            "sims_done": int(snap.sims_done),
            "sims_target": int(snap.sims_target),
            "full_search": bool(snap.is_full_search),
            "w": round(float(snap.root_w), 3),
            "d": round(float(snap.root_d), 3),
            "l": round(float(snap.root_l), 3),
            "moves_left": round(float(snap.root_ml), 1),
            "fen": snap.fen.decode("utf-8", errors="ignore"),
            "board": snap.board.decode("utf-8", errors="ignore"),
            "last_san": snap.last_san.decode("utf-8", errors="ignore"),
            "last_uci": snap.last_uci.decode("utf-8", errors="ignore"),
            "top": top,
        }

    def best_move(self, game_idx: int = 0, temperature: float = 0.0):
        uci_buf = ctypes.create_string_buffer(8)
        wdl = (ctypes.c_float * 3)()
        ml = ctypes.c_float(60.0)
        ok = self.lib.az_arena_best_move(
            self.ptr, game_idx, ctypes.c_float(temperature), uci_buf, wdl, ctypes.byref(ml)
        )
        if not ok:
            return "0000", (0.33, 0.34, 0.33), 60.0
        return (
            uci_buf.value.decode("utf-8", errors="ignore"),
            (float(wdl[0]), float(wdl[1]), float(wdl[2])),
            float(ml.value),
        )
