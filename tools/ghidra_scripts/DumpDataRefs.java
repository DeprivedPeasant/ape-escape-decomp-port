// Dump every reference into an address range with access width and function.
// Used to recover struct layouts of global objects.
// Args: <out.csv> <start hex> <end hex (exclusive)>
// @category ApeEscape
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import java.io.PrintWriter;

public class DumpDataRefs extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] a = getScriptArgs();
        Address start = toAddr(Long.parseLong(a[1], 16));
        Address end = toAddr(Long.parseLong(a[2], 16));
        var rm = currentProgram.getReferenceManager();
        var listing = currentProgram.getListing();
        int n = 0;
        try (PrintWriter out = new PrintWriter(a[0], "UTF-8")) {
            out.println("address,mnemonic,reftype,function,from");
            for (Address addr = start; addr.compareTo(end) < 0; addr = addr.add(1)) {
                for (Reference r : rm.getReferencesTo(addr)) {
                    Instruction ins = listing.getInstructionAt(r.getFromAddress());
                    Function f = getFunctionContaining(r.getFromAddress());
                    out.printf("%s,%s,%s,%s,%s%n", addr,
                            ins == null ? "data" : ins.getMnemonicString(),
                            r.getReferenceType(), f == null ? "-" : f.getName(), r.getFromAddress());
                    n++;
                }
            }
        }
        println("refs=" + n + " -> " + a[0]);
    }
}
