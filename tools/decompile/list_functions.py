import angr, sys, pickle, logging
logging.getLogger('angr').setLevel(logging.ERROR)
logging.getLogger('cle').setLevel(logging.ERROR)
exe, out = sys.argv[1], sys.argv[2]
p = angr.Project(exe, auto_load_libs=False)
cfg = p.analyses.CFGFast(normalize=True, resolve_indirect_jumps=True, data_references=True, function_prologues=True)
with open(out,'w') as f:
    for fa in sorted(p.kb.functions):
        fn = p.kb.functions[fa]
        size = sum(b.size for b in fn.blocks) if not fn.is_simprocedure else 0
        f.write("%08x %6d %s %s\n" % (fa, size, fn.name, 'plt' if fn.is_plt or fn.is_simprocedure else ''))
print(len(p.kb.functions))
