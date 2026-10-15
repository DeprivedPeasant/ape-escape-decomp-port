// Export named direct callers too (the shared ExportDecomp intentionally skips named callees).
// @category ApeEscape
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.listing.Function;
import ghidra.app.script.GhidraScript;
import java.io.PrintWriter;
import java.util.*;

public class ExportCallers extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Set<Function> functions = new TreeSet<>((a,b) -> a.getEntryPoint().compareTo(b.getEntryPoint()));
        for (int i=1;i<args.length;i++) {
            Function f = getFunctionAt(currentProgram.getAddressFactory().getAddress(args[i]));
            if (f == null) { println("missing function " + args[i]); continue; }
            functions.add(f);
            functions.addAll(f.getCallingFunctions(monitor));
        }
        DecompInterface dec = new DecompInterface();
        dec.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(args[0],"UTF-8")) {
            for (Function f:functions) {
                monitor.checkCancelled();
                out.println("/* ==== " + f.getName() + " @ " + f.getEntryPoint() + " */");
                var r = dec.decompileFunction(f,60,monitor);
                out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// ERROR " + r.getErrorMessage());
            }
        }
        dec.dispose();
        println("exported direct callers="+functions.size());
    }
}
