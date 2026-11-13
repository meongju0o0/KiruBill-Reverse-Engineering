import angr, sys, logging, time, traceback
logging.disable(logging.CRITICAL)
exe, out, lo, hi = sys.argv[1], sys.argv[2], int(sys.argv[3],16), int(sys.argv[4],16)
p = angr.Project(exe, auto_load_libs=False)
cfg = p.analyses.CFGFast(normalize=True, resolve_indirect_jumps=True, data_references=True, function_prologues=True)
p.analyses.CompleteCallingConventions(recover_variables=True, prioritize_func_addrs=None)
with open(out,'w') as f:
    for fa in sorted(p.kb.functions):
        if not (lo <= fa < hi): continue
        fn = p.kb.functions[fa]
        if fn.is_simprocedure or fn.is_plt: continue
        t=time.time()
        try:
            dec = p.analyses.Decompiler(fn, cfg=cfg.model)
            code = dec.codegen.text if dec.codegen else '/* no codegen */'
        except Exception as e:
            code = '/* decompile failed: %r */' % e
        f.write('// ===== %08x (%s) =====\n%s\n\n' % (fa, fn.name, code))
        f.flush()
        print('%08x %.1fs' % (fa, time.time()-t), file=sys.stderr)
