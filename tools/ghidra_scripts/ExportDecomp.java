// Decompile functions reachable from roots (breadth-first over calls) to one text file.
// Args: <out file> <max depth> <max functions> <root addr> [<root addr>...]
// Roots may be plain addresses (main RAM) or "ovl_18C::8013d000".
// Skips functions that already have non-default names unless they are roots,
// so named SDK functions don't flood the output (they are still listed as callees).
// @category ApeEscape
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.io.PrintWriter;
import java.util.*;

public class ExportDecomp extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        String outPath = args[0];
        int maxDepth = Integer.parseInt(args[1]);
        int maxFuncs = Integer.parseInt(args[2]);

        Deque<Object[]> queue = new ArrayDeque<>();
        Set<Function> roots = new HashSet<>();
        for (int i = 3; i < args.length; i++) {
            Address a = currentProgram.getAddressFactory().getAddress(args[i]);
            Function f = a == null ? null : getFunctionContaining(a);
            if (f == null) {
                println("no function at " + args[i]);
                continue;
            }
            roots.add(f);
            queue.add(new Object[] {f, 0});
        }

        DecompInterface dec = new DecompInterface();
        dec.openProgram(currentProgram);
        Set<Function> seen = new HashSet<>();
        int written = 0;
        try (PrintWriter out = new PrintWriter(outPath, "UTF-8")) {
            while (!queue.isEmpty() && written < maxFuncs) {
                Object[] item = queue.poll();
                Function f = (Function) item[0];
                int depth = (Integer) item[1];
                if (!seen.add(f)) {
                    continue;
                }
                boolean named = f.getSymbol().getSource() != SourceType.DEFAULT;
                Set<Function> callees = f.getCalledFunctions(monitor);
                if (named && !roots.contains(f)) {
                    continue;
                }
                List<String> calleeNames = new ArrayList<>();
                for (Function c : callees) {
                    calleeNames.add(c.getName() + "@" + c.getEntryPoint());
                    if (depth < maxDepth) {
                        queue.add(new Object[] {c, depth + 1});
                    }
                }
                Collections.sort(calleeNames);
                List<String> callerNames = new ArrayList<>();
                for (Function c : f.getCallingFunctions(monitor)) {
                    callerNames.add(c.getName());
                }
                Collections.sort(callerNames);

                DecompileResults r = dec.decompileFunction(f, 60, monitor);
                out.println("/* ==== " + f.getName() + " @ " + f.getEntryPoint() + "  depth=" + depth
                        + "  size=" + f.getBody().getNumAddresses());
                out.println("   callers(" + callerNames.size() + "): " + String.join(", ", callerNames.subList(0, Math.min(12, callerNames.size()))));
                out.println("   callees: " + String.join(", ", calleeNames) + " */");
                out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompile failed: " + r.getErrorMessage());
                written++;
            }
        }
        dec.dispose();
        println("exported " + written + " functions -> " + outPath);
    }
}
