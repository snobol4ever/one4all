# Makefile — one4all unified build
#
# Primary targets:
#   make scrip        — build the unified scrip x86 executable
#   make all          — alias for scrip
#   make setup        — install system packages + CSNOBOL4 + SPITBOL oracle
#   make test         — run corpus (--sm-run, PASS=178 gate)
#   make test-ir      — run corpus (--ir-run mode)
#   make test-all     — both passes back-to-back
#   make monitor-ipc  — build test/monitor/monitor_ipc.so
#   make clean        — remove build artefacts
#   make distclean    — clean + remove /tmp caches
#
# Runner wrappers (run a single .sno file):
#   make run SNO=file.sno              — default (--sm-run)
#   make run-ir SNO=file.sno           — --ir-run (IR tree-walk)
#   make run-jvm SNO=file.sno          — legacy JVM (until M-JITEM-JVM)
#   make run-net SNO=file.sno          — legacy .NET (until M-JITEM-NET)
#
# Note: run-asm retired — replaced by: scrip --jit-emit --x64 (M-JITEM-X64)
#
# Prerequisites:
#   apt-get install -y libgc-dev flex nasm build-essential libgmp-dev m4
#
# Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6

ROOT    := $(shell pwd)
SRC     := $(ROOT)/src
RT      := $(SRC)/runtime
BOXES   := $(RT)/x86
CORPUS  ?= $(ROOT)/../corpus
OBJ     := /tmp/si_objs
CC      := gcc
WARN    := -w
CBASE   := -O0 -g $(WARN) -I$(SRC) -I$(RT)/x86 -I$(RT) -I$(RT)/x86
CRT     := $(CBASE) -I$(RT)/x86 -DDYN_ENGINE_LINKED
LIBS    := -lgc -lm

# Runner defaults
SNO          ?= $(error SNO is required — e.g. make run SNO=prog.sno)
INC          ?= $(CORPUS)/programs/inc
JVM_CACHE    := /tmp/scrip_jvm_cache
NET_CACHE    := /tmp/scrip_net_cache
JASMIN       := $(SRC)/backend/jasmin.jar
SCRIP_CC_BIN := $(ROOT)/scrip

.PHONY: all scrip scrip-interp scrip setup \
        test test-ir test-all \
        monitor-ipc \
        libscrip_rt \
        run run-ir run-jvm run-net \
        clean distclean

# ── Primary target ────────────────────────────────────────────────────────────

all: scrip

# ── libscrip_rt.so — runtime support library for --jit-emit --x64 ────────────
# EM-6: full SNOBOL4 runtime compiled -fPIC and linked into the .so.
# Emitted x86-64 binaries link against this .so for all language-level
# semantics (pattern matcher, NV table, exec_stmt, builtins, GC).
libscrip_rt: out/libscrip_rt.so

# EM-6 runtime objects (all compiled -fPIC so they can go into the .so)
RT_PIC_SRCS := \
    $(RT)/rt/rt.c \
    $(RT)/x86/snobol4.c \
    $(RT)/x86/snobol4_pattern.c \
    $(RT)/x86/snobol4_invoke.c \
    $(RT)/x86/snobol4_argval.c \
    $(RT)/x86/snobol4_nmd.c \
    $(RT)/x86/name_t.c \
    $(RT)/x86/stmt_exec.c \
    $(RT)/x86/eval_code.c \
    $(RT)/x86/eval_pat.c \
    $(RT)/x86/bb_pool.c \
    $(RT)/x86/bb_emit.c \
    $(RT)/x86/emitter_text.c \
    $(RT)/x86/emitter_binary.c \
    $(RT)/x86/emitter_macro_def.c \
    $(RT)/x86/bb_build.c \
    $(RT)/x86/bb_flat.c \
    $(RT)/x86/templates/bb_xchr.c \
    $(RT)/x86/templates/bb_xspnc.c \
    $(RT)/x86/templates/bb_xlnth.c \
    $(RT)/x86/templates/bb_xtb.c \
    $(RT)/x86/templates/bb_xrtb.c \
    $(RT)/x86/templates/bb_xstar.c \
    $(RT)/x86/templates/bb_xabrt.c \
    $(RT)/x86/templates/bb_xsucf.c \
    $(RT)/x86/templates/bb_xbal.c \
    $(RT)/x86/templates/bb_xvar.c \
    $(RT)/x86/templates/bb_xfnce.c \
    $(RT)/x86/templates/bb_xatp.c \
    $(RT)/x86/templates/bb_xdsar.c \
    $(RT)/x86/templates/bb_xnme.c \
    $(RT)/x86/templates/bb_xfnme.c \
    $(RT)/x86/templates/bb_xcallcap.c \
    $(RT)/x86/templates/bb_xarbn.c \
    $(RT)/x86/templates/bb_icon_to.c \
    $(RT)/x86/templates/bb_icon_iterate.c \
    $(RT)/x86/templates/bb_icon_alt.c \
    $(RT)/x86/templates/bb_icon_every.c \
    $(RT)/x86/templates/bb_icon_limit.c \
    $(RT)/x86/templates/bb_icon_bang.c \
    $(RT)/x86/templates/bb_icon_lconcat.c \
    $(RT)/x86/templates/bb_icon_seq.c \
    $(RT)/x86/templates/bb_xcat.c \
    $(RT)/x86/templates/bb_xor.c \
    $(RT)/x86/templates/bb_xbrkx.c \
    $(RT)/x86/templates/bb_xposi.c \
    $(RT)/x86/templates/bb_xrpsi.c \
    $(RT)/x86/templates/bb_xfarb.c \
    $(RT)/x86/templates/bb_xeps.c \
    $(RT)/x86/templates/bb_xfail.c \
    $(RT)/x86/templates/sm_halt.c \
    $(RT)/x86/templates/sm_push_lit_i.c \
    $(RT)/x86/templates/sm_void_pop.c \
    $(RT)/x86/templates/sm_call_fn.c \
    $(RT)/x86/templates/sm_push_lit_s.c \
    $(RT)/x86/templates/sm_helpers.c \
    $(RT)/x86/templates/sm_add.c \
    $(RT)/x86/templates/sm_sub.c \
    $(RT)/x86/templates/sm_mul.c \
    $(RT)/x86/templates/sm_div.c \
    $(RT)/x86/templates/sm_mod.c \
    $(RT)/x86/templates/sm_exp.c \
    $(RT)/x86/templates/sm_neg.c \
    $(RT)/x86/templates/sm_concat.c \
    $(RT)/x86/templates/sm_push_null.c \
    $(RT)/x86/templates/sm_push_null_noflip.c \
    $(RT)/x86/templates/sm_coerce_num.c \
    $(RT)/x86/templates/sm_var.c \
    $(RT)/x86/templates/sm_jump.c \
    $(RT)/x86/templates/sm_jump_s.c \
    $(RT)/x86/templates/sm_jump_f.c \
    $(RT)/x86/templates/sm_label_stno.c \
    $(RT)/x86/templates/sm_return_variant.c \
    $(RT)/x86/templates/sm_freturn.c \
    $(RT)/x86/templates/sm_nreturn.c \
    $(RT)/x86/templates/sm_return_s.c \
    $(RT)/x86/templates/sm_return_f.c \
    $(RT)/x86/templates/sm_freturn_s.c \
    $(RT)/x86/templates/sm_freturn_f.c \
    $(RT)/x86/templates/sm_nreturn_s.c \
    $(RT)/x86/templates/sm_nreturn_f.c \
    $(RT)/x86/templates/sm_push_expression.c \
    $(RT)/x86/templates/sm_call_expression.c \
    $(RT)/x86/templates/sm_push_lit_f.c \
    $(RT)/x86/templates/sm_push_expr.c \
    $(RT)/x86/templates/sm_incr.c \
    $(RT)/x86/templates/sm_decr.c \
    $(RT)/x86/templates/sm_acomp.c \
    $(RT)/x86/templates/sm_lcomp.c \
    $(RT)/x86/templates/sm_define_entry.c \
    $(RT)/x86/templates/sm_define.c \
    $(RT)/x86/templates/sm_suspend.c \
    $(RT)/x86/templates/sm_resume.c \
    $(RT)/x86/templates/sm_suspend_value.c \
    $(RT)/x86/templates/sm_gen_tick.c \
    $(RT)/x86/templates/sm_load_glocal.c \
    $(RT)/x86/templates/sm_store_glocal.c \
    $(RT)/x86/templates/sm_icmp_gt.c \
    $(RT)/x86/templates/sm_icmp_lt.c \
    $(RT)/x86/templates/sm_load_frame.c \
    $(RT)/x86/templates/sm_store_frame.c \
    $(RT)/x86/templates/sm_bb_pump.c \
    $(RT)/x86/templates/sm_bb_once.c \
    $(RT)/x86/templates/sm_bb_once_proc.c \
    $(RT)/x86/templates/sm_bb_pump_proc.c \
    $(RT)/x86/templates/sm_bb_pump_case.c \
    $(RT)/x86/templates/sm_bb_pump_sm.c \
    $(RT)/x86/templates/sm_bb_pump_every.c \
    $(RT)/x86/templates/sm_bb_pump_ast.c \
    $(RT)/x86/templates/sm_pat_eps.c \
    $(RT)/x86/templates/sm_pat_arb.c \
    $(RT)/x86/templates/sm_pat_rem.c \
    $(RT)/x86/templates/sm_pat_fail.c \
    $(RT)/x86/templates/sm_pat_succeed.c \
    $(RT)/x86/templates/sm_pat_abort.c \
    $(RT)/x86/templates/sm_pat_bal.c \
    $(RT)/x86/templates/sm_pat_fence0.c \
    $(RT)/x86/templates/sm_pat_fence1.c \
    $(RT)/x86/templates/sm_pat_span.c \
    $(RT)/x86/templates/sm_pat_break.c \
    $(RT)/x86/templates/sm_pat_any.c \
    $(RT)/x86/templates/sm_pat_notany.c \
    $(RT)/x86/templates/sm_pat_len.c \
    $(RT)/x86/templates/sm_pat_pos.c \
    $(RT)/x86/templates/sm_pat_rpos.c \
    $(RT)/x86/templates/sm_pat_tab.c \
    $(RT)/x86/templates/sm_pat_rtab.c \
    $(RT)/x86/templates/sm_pat_arbno.c \
    $(RT)/x86/templates/sm_pat_cat.c \
    $(RT)/x86/templates/sm_pat_alt.c \
    $(RT)/x86/templates/sm_pat_deref.c \
    $(RT)/x86/templates/sm_pat_lbl.c \
    $(RT)/x86/templates/sm_pat_capture.c \
    $(RT)/x86/templates/sm_pat_capture_fn.c \
    \
    $(RT)/x86/bb_boxes.c \
    $(RT)/x86/bb_broker.c \
    $(RT)/x86/sm_prog.c \
    $(RT)/x86/sm_interp.c \
    $(RT)/x86/lower.c \
    $(RT)/x86/lower_ctx.c \
    $(RT)/x86/sm_image.c \
    $(RT)/x86/sm_codegen.c \
    $(RT)/x86/sm_emit_template.c \
    $(RT)/x86/sm_codegen_x64_emit.c \
    $(SRC)/runtime/interp/coro_runtime.c \
    $(SRC)/runtime/interp/coro_value.c \
    $(SRC)/runtime/interp/coro_stmt.c \
    $(SRC)/runtime/interp/scan_builtins.c \
    $(SRC)/runtime/interp/raku_builtins.c \
    $(SRC)/runtime/interp/pl_runtime.c \
    $(SRC)/runtime/interp/icon_box_rt.c \
    $(SRC)/runtime/common/coerce.c \
    $(SRC)/runtime/common/ast_clone.c \
    $(SRC)/driver/interp_globals.c \
    $(SRC)/driver/interp_label.c \
    $(SRC)/driver/interp_call.c \
    $(SRC)/driver/interp_eval.c \
    $(SRC)/driver/interp_ref.c \
    $(SRC)/driver/interp_hooks.c \
    $(SRC)/driver/interp_data.c \
    $(SRC)/driver/interp_exec.c \
    $(SRC)/driver/scrip_sm.c \
    $(SRC)/driver/sync_monitor.c \
    $(SRC)/driver/polyglot.c \
    $(SRC)/driver/stmt_ast.c \
    $(SRC)/ast/ast_print.c \
    $(SRC)/frontend/snobol4/snobol4.tab.c \
    $(SRC)/frontend/snobol4/snobol4.lex.c \
    $(SRC)/frontend/icon/icon_gen.c \
    $(SRC)/frontend/icon/icon_runtime.c \
    $(SRC)/frontend/icon/icon_parse.c \
    $(SRC)/frontend/icon/icon_lex.c \
    $(SRC)/frontend/icon/icon_driver.c \
    $(SRC)/frontend/prolog/prolog_lex.c \
    $(SRC)/frontend/prolog/prolog_parse.c \
    $(SRC)/frontend/prolog/prolog_lower.c \
    $(SRC)/frontend/prolog/prolog_atom.c \
    $(SRC)/frontend/prolog/prolog_builtin.c \
    $(SRC)/frontend/prolog/prolog_unify.c \
    $(SRC)/frontend/prolog/prolog_driver.c \
    $(SRC)/frontend/prolog/pl_broker.c \
    $(SRC)/frontend/snocone/snocone_lex.c \
    $(SRC)/frontend/snocone/snocone_parse.tab.c \
    $(SRC)/frontend/snocone/snocone_driver.c \
    $(SRC)/frontend/raku/raku.tab.c \
    $(SRC)/frontend/raku/raku.lex.c \
    $(SRC)/frontend/raku/raku_driver.c \
    $(SRC)/frontend/raku/raku_re.c \
    $(SRC)/frontend/rebus/rebus.tab.c \
    $(SRC)/frontend/rebus/lex.rebus.c \
    $(SRC)/frontend/rebus/rebus_lower.c \
    $(SRC)/frontend/rebus/rebus_emit.c \
    $(SRC)/frontend/rebus/rebus_print.c

out/libscrip_rt.so: $(RT_PIC_SRCS) $(RT)/rt/rt.h
	@mkdir -p out
	$(CC) -O0 -g $(WARN) -fPIC -shared \
	    -I$(SRC) -I$(RT)/x86 -I$(RT) -I$(RT)/rt \
	    -I$(SRC)/frontend/snobol4 -I$(SRC)/frontend/raku \
	    -DDYN_ENGINE_LINKED -DIR_DEFINE_NAMES \
	    $(RT_PIC_SRCS) \
	    -lgc -lm -Wl,--allow-multiple-definition \
	    -o out/libscrip_rt.so
	@echo "Built: out/libscrip_rt.so"

# ── EM-2 synthetic-program harness ───────────────────────────────────────────
# Standalone helper: builds a 3-op SM_Program in memory and emits asm
# via sm_codegen_x64_emit().  The shell gate then assembles/links/runs.
out/sm_codegen_x64_emit_test: $(RT)/x86/sm_codegen_x64_emit_test.c \
                               $(RT)/x86/sm_codegen_x64_emit.c \
                               $(RT)/x86/sm_codegen_x64_emit.h \
                               $(RT)/x86/sm_emit_template.c \
                               $(RT)/x86/sm_emit_template.h \
                               $(RT)/x86/sm_prog.c \
                               $(RT)/x86/sm_prog.h \
                               out/libscrip_rt.so
	@mkdir -p out
	$(CC) -O0 -g $(WARN) \
	    -I$(SRC) -I$(RT)/x86 -I$(RT) -I$(RT)/rt \
	    -DDYN_ENGINE_LINKED \
	    $(RT)/x86/sm_codegen_x64_emit_test.c \
	    $(RT)/x86/sm_codegen_x64_emit.c \
	    $(RT)/x86/sm_emit_template.c \
	    $(RT)/x86/sm_prog.c \
	    -Lout -lscrip_rt -lgc -lm \
	    -Wl,-rpath,$(shell pwd)/out \
	    -o out/sm_codegen_x64_emit_test
	@echo "Built: out/sm_codegen_x64_emit_test"


# ── EM-7a Phase-2 simulator unit test ────────────────────────────────────────
# Links against libscrip_rt.so for pat_* constructors + GC.
out/sm_phase2_sim_test: $(RT)/x86/sm_phase2_sim_test.c \
                        $(RT)/x86/sm_codegen_x64_emit.c \
                        $(RT)/x86/sm_codegen_x64_emit.h \
                        $(RT)/x86/sm_emit_template.c \
                        $(RT)/x86/sm_emit_template.h \
                        $(RT)/x86/sm_prog.c \
                        $(RT)/x86/sm_prog.h \
                        out/libscrip_rt.so
	@mkdir -p out
	$(CC) -O0 -g $(WARN) \
	    -I$(SRC) -I$(RT)/x86 -I$(RT) -I$(RT)/rt \
	    -DDYN_ENGINE_LINKED \
	    $(RT)/x86/sm_phase2_sim_test.c \
	    $(RT)/x86/sm_codegen_x64_emit.c \
	    $(RT)/x86/sm_emit_template.c \
	    $(RT)/x86/sm_prog.c \
	    -Lout -lscrip_rt -lgc -lm \
	    -Wl,-rpath,$(shell pwd)/out \
	    -o out/sm_phase2_sim_test

# ── EM-7b bb_build_flat_text unit test ───────────────────────────────────────
# Verifies dual-mode bb_flat.c: TEXT-mode emission produces a .s with
# the four externally-visible α/β/γ/ω labels and assembles cleanly.
# Links against libscrip_rt.so (which already includes bb_flat.c +
# bb_emit.c + pat_* constructors).
out/bb_flat_text_test: $(RT)/x86/bb_flat_text_test.c \
                       $(RT)/x86/sm_codegen_x64_emit.c \
                       $(RT)/x86/sm_codegen_x64_emit.h \
                       $(RT)/x86/sm_emit_template.c \
                       $(RT)/x86/sm_emit_template.h \
                       $(RT)/x86/sm_prog.c \
                       $(RT)/x86/sm_prog.h \
                       out/libscrip_rt.so \
                       $(RT)/x86/bb_flat.h \
                       $(RT)/x86/bb_emit.h \
                       $(RT)/x86/bb_pool.h
	@mkdir -p out
	$(CC) -O0 -g $(WARN) \
	    -I$(SRC) -I$(RT)/x86 -I$(RT) -I$(RT)/rt \
	    -DDYN_ENGINE_LINKED \
	    $(RT)/x86/bb_flat_text_test.c \
	    $(RT)/x86/sm_codegen_x64_emit.c \
	    $(RT)/x86/sm_emit_template.c \
	    $(RT)/x86/sm_prog.c \
	    -Lout -lscrip_rt -lgc -lm \
	    -Wl,-rpath,$(shell pwd)/out \
	    -o out/bb_flat_text_test
	@echo "Built: out/bb_flat_text_test"
	@echo "Built: out/sm_phase2_sim_test"

# ── scrip — unified driver (all modes, all frontends) ────────────────────────
# WASM removed from scrip build (2026-04-08): --jit-emit --wasm / emit_wasm.c
# dropped. Use scrip legacy driver if WASM emission is ever needed.

scrip:
	@mkdir -p $(OBJ)
	@rm -f $(OBJ)/*.o
	$(CC) $(CBASE) -c $(SRC)/frontend/snobol4/snobol4.lex.c -o $(OBJ)/snobol4.lex.o
	$(CC) $(CBASE) -c $(SRC)/frontend/snobol4/snobol4.tab.c -o $(OBJ)/snobol4.tab.o
	$(CC) $(CRT)   -c $(RT)/x86/snobol4.c               -o $(OBJ)/snobol4.o
	$(CC) $(CRT)   -c $(RT)/x86/snobol4_pattern.c        -o $(OBJ)/snobol4_pattern.o
	$(CC) $(CRT)   -c $(RT)/x86/snobol4_invoke.c                 -o $(OBJ)/snobol4_invoke.o
	$(CC) $(CRT)   -c $(RT)/x86/snobol4_argval.c                 -o $(OBJ)/snobol4_argval.o
	$(CC) $(CRT)   -c $(RT)/x86/snobol4_nmd.c                    -o $(OBJ)/snobol4_nmd.o
	$(CC) $(CRT)   -c $(RT)/x86/name_t.c                         -o $(OBJ)/name_t.o
	$(CC) $(CRT)   -c $(RT)/x86/stmt_exec.c                  -o $(OBJ)/stmt_exec.o
	$(CC) $(CRT)   -c $(RT)/x86/eval_code.c                  -o $(OBJ)/eval_code.o
	$(CC) $(CRT)   -c $(RT)/x86/bb_pool.c                    -o $(OBJ)/bb_pool.o
	$(CC) $(CRT)   -c $(RT)/x86/bb_emit.c                    -o $(OBJ)/bb_emit.o
	$(CC) $(CRT)   -c $(RT)/x86/emitter_text.c               -o $(OBJ)/emitter_text.o
	$(CC) $(CRT)   -c $(RT)/x86/emitter_binary.c             -o $(OBJ)/emitter_binary.o
	$(CC) $(CRT)   -c $(RT)/x86/emitter_macro_def.c          -o $(OBJ)/emitter_macro_def.o
	$(CC) $(CRT)   -c $(RT)/x86/bb_build.c               -o $(OBJ)/bb_build.o
	$(CC) $(CRT)   -c $(RT)/x86/bb_flat.c                    -o $(OBJ)/bb_flat.o
	$(CC) $(CRT) -c $(RT)/x86/bb_boxes.c -o $(OBJ)/bb_boxes.o
	$(CC) $(CRT) -c $(RT)/x86/bb_broker.c -o $(OBJ)/bb_broker.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -DIR_DEFINE_NAMES \
	    -c $(SRC)/ast/ast_print.c -o $(OBJ)/ast_print.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/snocone/snocone_lex.c        -o $(OBJ)/snocone_lex.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/snocone/snocone_parse.tab.c  -o $(OBJ)/snocone_parse.tab.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/snocone/snocone_driver.c     -o $(OBJ)/snocone_driver.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_lex.c      -o $(OBJ)/prolog_lex.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_parse.c    -o $(OBJ)/prolog_parse.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_lower.c    -o $(OBJ)/prolog_lower.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_atom.c     -o $(OBJ)/prolog_atom.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_builtin.c  -o $(OBJ)/prolog_builtin.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_unify.c    -o $(OBJ)/prolog_unify.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_driver.c   -o $(OBJ)/prolog_driver.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/pl_broker.c       -o $(OBJ)/pl_broker.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/icon/icon_lex.c         -o $(OBJ)/icon_lex.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/icon/icon_parse.c       -o $(OBJ)/icon_parse.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/icon/icon_runtime.c     -o $(OBJ)/icon_runtime.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/icon/icon_gen.c         -o $(OBJ)/icon_gen.o

	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/icon/icon_driver.c      -o $(OBJ)/icon_driver.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -I$(SRC)/frontend/raku -c $(SRC)/frontend/raku/raku.tab.c    -o $(OBJ)/raku.tab.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -I$(SRC)/frontend/raku -c $(SRC)/frontend/raku/raku.lex.c    -o $(OBJ)/raku.lex.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -I$(SRC)/frontend/raku -c $(SRC)/frontend/raku/raku_driver.c -o $(OBJ)/raku_driver.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -I$(SRC)/frontend/raku -c $(SRC)/frontend/raku/raku_re.c      -o $(OBJ)/raku_re.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/rebus/rebus.tab.c    -o $(OBJ)/rebus.tab.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/rebus/lex.rebus.c    -o $(OBJ)/lex.rebus.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/rebus/rebus_lower.c  -o $(OBJ)/rebus_lower.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/rebus/rebus_emit.c   -o $(OBJ)/rebus_emit.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/rebus/rebus_print.c  -o $(OBJ)/rebus_print.o
	$(CC) $(CRT)   -c $(SRC)/runtime/interp/coro_runtime.c -o $(OBJ)/coro_runtime.o
	$(CC) $(CRT)   -c $(SRC)/runtime/interp/coro_value.c   -o $(OBJ)/coro_value.o
	$(CC) $(CRT)   -c $(SRC)/runtime/interp/coro_stmt.c    -o $(OBJ)/coro_stmt.o
	$(CC) $(CRT)   -c $(SRC)/runtime/interp/raku_builtins.c -o $(OBJ)/raku_builtins.o
	$(CC) $(CRT)   -c $(SRC)/runtime/interp/scan_builtins.c -o $(OBJ)/scan_builtins.o
	$(CC) $(CRT)   -c $(SRC)/runtime/interp/pl_runtime.c  -o $(OBJ)/pl_runtime.o
	$(CC) $(CRT)   -c $(SRC)/runtime/interp/icon_box_rt.c  -o $(OBJ)/icon_box_rt.o
	$(CC) $(CRT)   -c $(SRC)/runtime/common/coerce.c      -o $(OBJ)/coerce.o
	$(CC) $(CRT)   -c $(SRC)/runtime/common/ast_clone.c    -o $(OBJ)/ast_clone.o
	$(CC) $(CRT)   -c $(RT)/x86/sm_prog.c    -o $(OBJ)/sm_prog.o
	$(CC) $(CRT)   -c $(RT)/x86/sm_interp.c  -o $(OBJ)/sm_interp.o
	$(CC) $(CRT)   -c $(RT)/x86/lower.c      -o $(OBJ)/lower.o
	$(CC) $(CRT)   -c $(RT)/x86/lower_ctx.c  -o $(OBJ)/lower_ctx.o
	$(CC) $(CRT)   -c $(RT)/x86/sm_image.c   -o $(OBJ)/sm_image.o
	$(CC) $(CRT)   -c $(RT)/x86/sm_codegen.c -o $(OBJ)/sm_codegen.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_halt.c -o $(OBJ)/template_sm_halt.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_push_lit_i.c -o $(OBJ)/template_sm_push_lit_i.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_void_pop.c -o $(OBJ)/template_sm_void_pop.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_call_fn.c -o $(OBJ)/template_sm_call_fn.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_push_lit_s.c -o $(OBJ)/template_sm_push_lit_s.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_helpers.c -o $(OBJ)/template_sm_helpers.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_acomp.c -o $(OBJ)/template_sm_acomp.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_add.c -o $(OBJ)/template_sm_add.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_bb_once.c -o $(OBJ)/template_sm_bb_once.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_bb_once_proc.c -o $(OBJ)/template_sm_bb_once_proc.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_bb_pump.c -o $(OBJ)/template_sm_bb_pump.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_bb_pump_ast.c -o $(OBJ)/template_sm_bb_pump_ast.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_bb_pump_case.c -o $(OBJ)/template_sm_bb_pump_case.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_bb_pump_every.c -o $(OBJ)/template_sm_bb_pump_every.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_bb_pump_proc.c -o $(OBJ)/template_sm_bb_pump_proc.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_bb_pump_sm.c -o $(OBJ)/template_sm_bb_pump_sm.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_call_expression.c -o $(OBJ)/template_sm_call_expression.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_coerce_num.c -o $(OBJ)/template_sm_coerce_num.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_concat.c -o $(OBJ)/template_sm_concat.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_decr.c -o $(OBJ)/template_sm_decr.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_define.c -o $(OBJ)/template_sm_define.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_define_entry.c -o $(OBJ)/template_sm_define_entry.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_div.c -o $(OBJ)/template_sm_div.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_exp.c -o $(OBJ)/template_sm_exp.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_freturn.c -o $(OBJ)/template_sm_freturn.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_freturn_f.c -o $(OBJ)/template_sm_freturn_f.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_freturn_s.c -o $(OBJ)/template_sm_freturn_s.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_gen_tick.c -o $(OBJ)/template_sm_gen_tick.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_icmp_gt.c -o $(OBJ)/template_sm_icmp_gt.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_icmp_lt.c -o $(OBJ)/template_sm_icmp_lt.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_incr.c -o $(OBJ)/template_sm_incr.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_jump_f.c -o $(OBJ)/template_sm_jump_f.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_jump.c -o $(OBJ)/template_sm_jump.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_jump_s.c -o $(OBJ)/template_sm_jump_s.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_label_stno.c -o $(OBJ)/template_sm_label_stno.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_lcomp.c -o $(OBJ)/template_sm_lcomp.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_load_frame.c -o $(OBJ)/template_sm_load_frame.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_load_glocal.c -o $(OBJ)/template_sm_load_glocal.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_mod.c -o $(OBJ)/template_sm_mod.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_mul.c -o $(OBJ)/template_sm_mul.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_neg.c -o $(OBJ)/template_sm_neg.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_nreturn.c -o $(OBJ)/template_sm_nreturn.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_nreturn_f.c -o $(OBJ)/template_sm_nreturn_f.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_nreturn_s.c -o $(OBJ)/template_sm_nreturn_s.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_abort.c -o $(OBJ)/template_sm_pat_abort.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_alt.c -o $(OBJ)/template_sm_pat_alt.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_any.c -o $(OBJ)/template_sm_pat_any.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_arb.c -o $(OBJ)/template_sm_pat_arb.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_arbno.c -o $(OBJ)/template_sm_pat_arbno.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_bal.c -o $(OBJ)/template_sm_pat_bal.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_break.c -o $(OBJ)/template_sm_pat_break.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_cat.c -o $(OBJ)/template_sm_pat_cat.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_deref.c -o $(OBJ)/template_sm_pat_deref.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_lbl.c -o $(OBJ)/template_sm_pat_lbl.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_capture.c -o $(OBJ)/template_sm_pat_capture.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_capture_fn.c -o $(OBJ)/template_sm_pat_capture_fn.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_eps.c -o $(OBJ)/template_sm_pat_eps.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_fail.c -o $(OBJ)/template_sm_pat_fail.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_fence0.c -o $(OBJ)/template_sm_pat_fence0.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_fence1.c -o $(OBJ)/template_sm_pat_fence1.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_len.c -o $(OBJ)/template_sm_pat_len.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_notany.c -o $(OBJ)/template_sm_pat_notany.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_pos.c -o $(OBJ)/template_sm_pat_pos.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_rem.c -o $(OBJ)/template_sm_pat_rem.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_rpos.c -o $(OBJ)/template_sm_pat_rpos.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_rtab.c -o $(OBJ)/template_sm_pat_rtab.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_span.c -o $(OBJ)/template_sm_pat_span.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_succeed.c -o $(OBJ)/template_sm_pat_succeed.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_pat_tab.c -o $(OBJ)/template_sm_pat_tab.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_push_expr.c -o $(OBJ)/template_sm_push_expr.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_push_expression.c -o $(OBJ)/template_sm_push_expression.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_push_lit_f.c -o $(OBJ)/template_sm_push_lit_f.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_push_null.c -o $(OBJ)/template_sm_push_null.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_push_null_noflip.c -o $(OBJ)/template_sm_push_null_noflip.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_var.c -o $(OBJ)/template_sm_var.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_resume.c -o $(OBJ)/template_sm_resume.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_return_f.c -o $(OBJ)/template_sm_return_f.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_return_s.c -o $(OBJ)/template_sm_return_s.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_return_variant.c -o $(OBJ)/template_sm_return_variant.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_store_frame.c -o $(OBJ)/template_sm_store_frame.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_store_glocal.c -o $(OBJ)/template_sm_store_glocal.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_sub.c -o $(OBJ)/template_sm_sub.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_suspend.c -o $(OBJ)/template_sm_suspend.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/sm_suspend_value.c -o $(OBJ)/template_sm_suspend_value.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xbrkx.c -o $(OBJ)/template_bb_xbrkx.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xposi.c -o $(OBJ)/template_bb_xposi.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xrpsi.c -o $(OBJ)/template_bb_xrpsi.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xfarb.c -o $(OBJ)/template_bb_xfarb.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xeps.c -o $(OBJ)/template_bb_xeps.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xfail.c -o $(OBJ)/template_bb_xfail.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xchr.c -o $(OBJ)/template_bb_xchr.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xspnc.c -o $(OBJ)/template_bb_xspnc.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xlnth.c -o $(OBJ)/template_bb_xlnth.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xtb.c -o $(OBJ)/template_bb_xtb.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xrtb.c -o $(OBJ)/template_bb_xrtb.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xstar.c -o $(OBJ)/template_bb_xstar.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xabrt.c -o $(OBJ)/template_bb_xabrt.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xsucf.c -o $(OBJ)/template_bb_xsucf.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xbal.c -o $(OBJ)/template_bb_xbal.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xvar.c -o $(OBJ)/template_bb_xvar.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xfnce.c -o $(OBJ)/template_bb_xfnce.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xatp.c -o $(OBJ)/template_bb_xatp.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xdsar.c -o $(OBJ)/template_bb_xdsar.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xnme.c -o $(OBJ)/template_bb_xnme.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xfnme.c -o $(OBJ)/template_bb_xfnme.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xcallcap.c -o $(OBJ)/template_bb_xcallcap.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xarbn.c -o $(OBJ)/template_bb_xarbn.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_icon_to.c -o $(OBJ)/template_bb_icon_to.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_icon_iterate.c -o $(OBJ)/template_bb_icon_iterate.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_icon_alt.c -o $(OBJ)/template_bb_icon_alt.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_icon_every.c -o $(OBJ)/template_bb_icon_every.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_icon_limit.c -o $(OBJ)/template_bb_icon_limit.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_icon_bang.c -o $(OBJ)/template_bb_icon_bang.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_icon_lconcat.c -o $(OBJ)/template_bb_icon_lconcat.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_icon_seq.c -o $(OBJ)/template_bb_icon_seq.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xcat.c -o $(OBJ)/template_bb_xcat.o
	$(CC) $(CRT)   -c $(RT)/x86/templates/bb_xor.c -o $(OBJ)/template_bb_xor.o
	$(CC) $(CRT)   -c $(RT)/x86/sm_emit_template.c -o $(OBJ)/sm_emit_template.o
	$(CC) $(CRT)   -c $(RT)/x86/sm_codegen_x64_emit.c -o $(OBJ)/sm_codegen_x64_emit.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_globals.c -o $(OBJ)/interp_globals.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_label.c   -o $(OBJ)/interp_label.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_call.c    -o $(OBJ)/interp_call.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_eval.c    -o $(OBJ)/interp_eval.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_ref.c     -o $(OBJ)/interp_ref.o
	$(CC) $(CRT)   -c $(SRC)/runtime/x86/eval_pat.c  -o $(OBJ)/eval_pat.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_exec.c    -o $(OBJ)/interp_exec.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_hooks.c   -o $(OBJ)/interp_hooks.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_data.c    -o $(OBJ)/interp_data.o
	$(CC) $(CRT)   -c $(SRC)/driver/scrip_sm.c       -o $(OBJ)/scrip_sm.o
	$(CC) $(CRT)   -c $(SRC)/driver/sync_monitor.c -o $(OBJ)/sync_monitor.o
	$(CC) $(CRT)   -c $(SRC)/driver/polyglot.c -o $(OBJ)/polyglot.o
	$(CC) $(CRT)   -c $(SRC)/driver/stmt_ast.c -o $(OBJ)/stmt_ast.o
	$(CC) $(CRT)   -c $(SRC)/driver/scrip.c  -o $(OBJ)/scrip_driver.o
	$(CC) -m64 -no-pie $(OBJ)/*.o $(LIBS) -o scrip
	@echo "Built: scrip"

# backward-compat symlink
scrip-interp: scrip
	@ln -sf scrip scrip-interp

# ── scrip-monitor: scrip with CSNOBOL4 4th executor linked in (IM-15b) ───────
# Build: make scrip-monitor CSN_A=/home/claude/csnobol4/libcsnobol4.a
# Requires: bash scripts/build_csnobol4_archive.sh first
CSN_A   ?= /home/claude/csnobol4/libcsnobol4.a
CSN_INC ?= /home/claude/csnobol4

scrip-monitor:
	@# Build all scrip objects, then relink with CSNOBOL4 4th executor
	$(MAKE) -f Makefile scrip
	$(CC) $(CRT) -DWITH_CSNOBOL4=1 -I$(CSN_INC) \
	      -c $(SRC)/driver/csnobol4_shim.c -o $(OBJ)/csnobol4_shim_csn.o
	$(CC) $(CRT) -DWITH_CSNOBOL4=1 \
	      -c $(SRC)/driver/sync_monitor.c -o $(OBJ)/sync_monitor_csn.o
	$(CC) -m64 -no-pie -Wl,--allow-multiple-definition \
	      $(OBJ)/csnobol4_shim_csn.o $(OBJ)/sync_monitor_csn.o \
	      $(filter-out $(OBJ)/sync_monitor.o $(OBJ)/sync_monitor_csn.o $(OBJ)/csnobol4_shim.o $(OBJ)/csnobol4_shim_csn.o $(OBJ)/scrip_driver.o, $(wildcard $(OBJ)/*.o)) \
	      $(OBJ)/scrip_driver.o \
	      $(CSN_A) $(LIBS) -lutil -ldl -lz -lbz2 -o scrip-monitor
	@echo "Built: scrip-monitor (with CSNOBOL4 4th executor)"

# ── monitor_ipc.so ────────────────────────────────────────────────────────────

monitor-ipc:
	gcc -shared -fPIC \
	    -o test/monitor/monitor_ipc.so \
	    test/monitor/monitor_ipc.c
	@echo "Built: test/monitor/monitor_ipc.so"

# ── Environment setup (idempotent) ────────────────────────────────────────────

setup:
	bash $(ROOT)/setup.sh

# ── Test targets ──────────────────────────────────────────────────────────────

test: scrip
	CORPUS=$(CORPUS) bash test/run_interp_broad.sh

test-ir: scrip
	INTERP="./scrip --ir-run" CORPUS=$(CORPUS) bash test/run_interp_broad.sh

test-all: test test-ir

# ── Runner wrappers ───────────────────────────────────────────────────────────

run: scrip
	./scrip $(SNO)

run-ir: scrip
	./scrip --ir-run $(SNO)

# Legacy JVM runner — uses old scrip text emitter until M-JITEM-JVM lands
run-jvm: scrip
	@mkdir -p $(JVM_CACHE); \
	base=$$(basename $(SNO) .sno); \
	hash=$$(echo $(SNO) | md5sum | cut -c1-8); \
	key=$${base}_$${hash}; \
	jfile=$(JVM_CACHE)/$${key}.j; \
	stamp=$(JVM_CACHE)/$${key}.stamp; \
	$(SCRIP_CC_BIN) -jvm $(SNO) > $$jfile; \
	classname=$$(grep '\.class' $$jfile | head -1 | awk '{print $$NF}'); \
	j_md5=$$(md5sum $$jfile | cut -d' ' -f1); \
	cached_md5=$$(cat $$stamp 2>/dev/null || echo ''); \
	if [ "$$j_md5" != "$$cached_md5" ] || [ ! -f $(JVM_CACHE)/$$classname.class ]; then \
	    java -jar $(JASMIN) $$jfile -d $(JVM_CACHE) >/dev/null; \
	    echo "$$j_md5" > $$stamp; \
	fi; \
	java -cp $(JVM_CACHE) $$classname

# Legacy .NET runner — uses old scrip text emitter until M-JITEM-NET lands
run-net: scrip
	@mkdir -p $(NET_CACHE); \
	base=$$(basename $(SNO) .sno); \
	hash=$$(echo $(SNO) | md5sum | cut -c1-8); \
	key=$${base}_$${hash}; \
	il=$(NET_CACHE)/$${key}.il; \
	exe=$(NET_CACHE)/$${key}.exe; \
	stamp=$(NET_CACHE)/$${key}.stamp; \
	$(SCRIP_CC_BIN) -net $(SNO) > $$il; \
	il_md5=$$(md5sum $$il | cut -d' ' -f1); \
	cached_md5=$$(cat $$stamp 2>/dev/null || echo ''); \
	if [ "$$il_md5" != "$$cached_md5" ] || [ ! -f $$exe ]; then \
	    ilasm $$il /output:$$exe >/dev/null; \
	    echo "$$il_md5" > $$stamp; \
	fi; \
	mono $$exe

# ── Clean ─────────────────────────────────────────────────────────────────────

clean:
	rm -rf $(OBJ) scrip scrip-interp

distclean: clean
	rm -rf $(JVM_CACHE) $(NET_CACHE) /tmp/snobol4_asm_* /tmp/scrip_cc_*
