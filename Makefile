CC      = clang
CFLAGS  = -Wall -Wextra -Werror -Wpedantic -O3 -march=native -flto -Isrc
DEPFLAGS = -MMD -MP
LDFLAGS = -O3 -march=native -flto
LIBS    = -lm -lpthread

BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/obj

# ============================================================================
# Layered Module Sources
# ============================================================================

CORE_SRCS = \
	src/core/attacks.c \
	src/core/board.c \
	src/core/movegen.c \
	src/core/game.c \
	src/core/notation.c

ENGINE_SRCS = \
	src/engine/zobrist.c \
	src/engine/tt.c \
	src/engine/see.c \
	src/engine/endgame.c \
	src/engine/syzygy.c \
	src/engine/nnue.c \
	src/engine/eval.c \
	src/engine/search.c \
	src/engine/book.c

FEATURE_SRCS = \
	src/features/uci_client.c \
	src/features/gauntlet.c \
	src/features/review.c \
	src/features/puzzles.c \
	src/features/history.c

# Target source sets
DRIVER_SRCS     = $(CORE_SRCS) $(ENGINE_SRCS) src/features/review.c src/features/puzzles.c src/features/history.c src/az/az_mcts.c src/tests/driver.c
PROTOTYPER_SRCS = src/core/board.c src/tests/test_driver.c
DEMO_SRCS       = $(CORE_SRCS) src/ui/cli/demo.c
PLAY_SRCS       = $(CORE_SRCS) $(ENGINE_SRCS) src/features/uci_client.c src/features/review.c src/ui/cli/play.c
WEB_SRCS        = $(CORE_SRCS) $(ENGINE_SRCS) $(FEATURE_SRCS) src/ui/web/web_server.c
ELO_SRCS        = $(CORE_SRCS) $(ENGINE_SRCS) src/features/uci_client.c src/features/gauntlet.c src/ui/cli/elo_bench.c
UCI_SRCS        = $(CORE_SRCS) $(ENGINE_SRCS) src/engine/uci_server.c
TUNER_SRCS      = $(CORE_SRCS) $(ENGINE_SRCS) src/tools/texel_tuner.c
NNUE_TRN_SRCS   = src/core/board.c src/engine/nnue.c src/tools/nnue_trainer.c
DATAGEN_SRCS    = $(CORE_SRCS) $(ENGINE_SRCS) src/tools/datagen.c
AZ_LIB_SRCS     = $(CORE_SRCS) src/az/az_mcts.c

# Object file mappings
to_obj = $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(1))

DRIVER_OBJS     = $(call to_obj,$(DRIVER_SRCS))
PROTOTYPER_OBJS = $(call to_obj,$(PROTOTYPER_SRCS))
DEMO_OBJS       = $(call to_obj,$(DEMO_SRCS))
PLAY_OBJS       = $(call to_obj,$(PLAY_SRCS))
WEB_OBJS        = $(call to_obj,$(WEB_SRCS))
ELO_OBJS        = $(call to_obj,$(ELO_SRCS))
UCI_OBJS        = $(call to_obj,$(UCI_SRCS))
TUNER_OBJS      = $(call to_obj,$(TUNER_SRCS))
NNUE_TRN_OBJS   = $(call to_obj,$(NNUE_TRN_SRCS))
DATAGEN_OBJS    = $(call to_obj,$(DATAGEN_SRCS))

ALL_OBJS = $(sort $(DRIVER_OBJS) $(PROTOTYPER_OBJS) $(DEMO_OBJS) $(PLAY_OBJS) $(WEB_OBJS) $(ELO_OBJS) $(UCI_OBJS) $(TUNER_OBJS) $(NNUE_TRN_OBJS) $(DATAGEN_OBJS))
ALL_DEPS = $(ALL_OBJS:.o=.d)

# Binaries
BIN_DRIVER     = $(BUILD_DIR)/driver
BIN_PROTOTYPER = $(BUILD_DIR)/prototyper
BIN_DEMO       = $(BUILD_DIR)/demo
BIN_PLAY       = $(BUILD_DIR)/chess_play
BIN_WEB        = $(BUILD_DIR)/chess_web
BIN_ELO        = $(BUILD_DIR)/chess_elo
BIN_UCI        = $(BUILD_DIR)/chess_uci
BIN_TUNER      = $(BUILD_DIR)/texel_tuner
BIN_NNUE_TRN   = $(BUILD_DIR)/nnue_trainer
BIN_DATAGEN    = $(BUILD_DIR)/datagen
LIB_AZ_MCTS    = $(BUILD_DIR)/libaz_mcts.so

.PHONY: all all-bins driver prototyper demo chess_play chess_web chess_elo chess_uci tuner nnue_trainer datagen az-lib az-setup az-train \
        spsa train-nnue-v4 syzygy-5 \
        run run-deep walk run-demo play watch web elo uci \
        clean format valgrind scan-build drive prototype showcase

all: driver

all-bins: driver prototyper demo chess_play chess_web chess_elo chess_uci datagen az-lib

# Pattern rule: incremental per-file compilation with auto header dependencies
$(OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# Link targets
$(BIN_DRIVER): $(DRIVER_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BIN_PROTOTYPER): $(PROTOTYPER_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BIN_DEMO): $(DEMO_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BIN_PLAY): $(PLAY_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BIN_WEB): $(WEB_OBJS)
	$(CC) $(LDFLAGS) -o $@.tmp $^ $(LIBS) && mv -f $@.tmp $@

$(BIN_ELO): $(ELO_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BIN_UCI): $(UCI_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BIN_TUNER): $(TUNER_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BIN_NNUE_TRN): $(NNUE_TRN_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BIN_DATAGEN): $(DATAGEN_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(LIB_AZ_MCTS): $(AZ_LIB_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -Wall -Wextra -Werror -Wpedantic -O3 -march=native -fPIC -shared -Isrc -o $@.tmp $^ -lm -lpthread && mv -f $@.tmp $@

# Convenient build aliases
driver:     $(BIN_DRIVER)
prototyper: $(BIN_PROTOTYPER)
demo:       $(BIN_DEMO)
chess_play: $(BIN_PLAY)
chess_web:  $(BIN_WEB)
chess_elo:  $(BIN_ELO)
chess_uci:  $(BIN_UCI)
tuner:        $(BIN_TUNER)
nnue_trainer: $(BIN_NNUE_TRN)
datagen:      $(BIN_DATAGEN)
az-lib:       $(LIB_AZ_MCTS)

spsa: chess_uci
	python3 src/tools/spsa_tuner.py --threads 2 --iters 150 --pairs-per-iter 4 --nodes 12000

train-nnue-v4:
	@PY=python3; [ -x .venv-az/bin/python3 ] && PY=.venv-az/bin/python3; $$PY src/tools/train_big_nnue.py

syzygy-5:
	bash scripts/download_syzygy5.sh data/syzygy

az-setup:
	python3 -m venv .venv-az && .venv-az/bin/pip install --upgrade pip && .venv-az/bin/pip install numpy torch --index-url https://download.pytorch.org/whl/cu121

az-train: az-lib
	@PY=python3; [ -x .venv-az/bin/python3 ] && PY=.venv-az/bin/python3; $$PY src/az/trainer.py

# Run targets
run: driver
	./$(BIN_DRIVER)

run-deep: driver
	./$(BIN_DRIVER) --deep

walk: prototyper
	./$(BIN_PROTOTYPER)

run-demo: demo
	./$(BIN_DEMO)

play: chess_play
	./$(BIN_PLAY)

watch: chess_play
	./$(BIN_PLAY) --watch

web: chess_web
	./$(BIN_WEB)

elo: chess_elo
	./$(BIN_ELO)

uci: chess_uci
	./$(BIN_UCI)

# Utilities
clean:
	rm -rf $(BUILD_DIR)

format:
	find src -type f \( -name '*.c' -o -name '*.h' \) -exec $(CC)-format -i -style=file {} +

valgrind: driver
	valgrind --leak-check=full --error-exitcode=1 -s ./$(BIN_DRIVER) --quick

scan-build:
	scan-build make all-bins

drive: clean format driver run

prototype: clean format prototyper walk

showcase: clean format demo run-demo

-include $(ALL_DEPS)
