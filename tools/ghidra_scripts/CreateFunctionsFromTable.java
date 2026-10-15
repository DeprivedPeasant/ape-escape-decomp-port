// Create functions at every code pointer in a pointer table (targets reachable only
// through function tables are missed by auto-analysis).
// Args: <table addr hex> <max entries> [<table addr hex> <max entries> ...]
// Stops a table at the first entry that is neither 0 nor a pointer into main code.
// @category ApeEscape
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class CreateFunctionsFromTable extends GhidraScript {
    private static final long CODE_START = 0x80010000L, CODE_END = 0x800B5000L;

    @Override
    protected void run() throws Exception {
        String[] a = getScriptArgs();
        for (int t = 0; t + 1 < a.length; t += 2) {
            Address table = toAddr(Long.parseLong(a[t], 16));
            int max = Integer.parseInt(a[t + 1]);
            int created = 0, existing = 0, n = 0;
            for (; n < max; n++) {
                long p = getInt(table.add(n * 4L)) & 0xFFFFFFFFL;
                if (p == 0) {
                    continue;
                }
                if (p < CODE_START || p >= CODE_END) {
                    break;
                }
                Address target = toAddr(p);
                if (getFunctionAt(target) != null) {
                    existing++;
                    continue;
                }
                disassemble(target);
                if (createFunction(target, null) != null) {
                    created++;
                } else {
                    println("  could not create function at " + target);
                }
            }
            println(String.format("table %s: %d entries, %d created, %d existing", table, n, created, existing));
        }
    }
}
