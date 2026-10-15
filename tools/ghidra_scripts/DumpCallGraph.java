// Dump functions, call edges, COP2 (GTE) instruction use and hardware-register
// references for offline analysis (tools/sdk_inventory.py).
// Args: <out dir>
// Writes: functions.csv, calls.csv, gte.csv, hwrefs.csv
// @category ApeEscape
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import java.io.PrintWriter;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.Map;

public class DumpCallGraph extends GhidraScript {
    @Override
    protected void run() throws Exception {
        Path out = Path.of(getScriptArgs()[0]);
        var fm = currentProgram.getFunctionManager();
        var listing = currentProgram.getListing();
        var mem = currentProgram.getMemory();

        try (PrintWriter fns = new PrintWriter(out.resolve("functions.csv").toFile(), "UTF-8");
             PrintWriter calls = new PrintWriter(out.resolve("calls.csv").toFile(), "UTF-8");
             PrintWriter gte = new PrintWriter(out.resolve("gte.csv").toFile(), "UTF-8");
             PrintWriter hw = new PrintWriter(out.resolve("hwrefs.csv").toFile(), "UTF-8")) {
            fns.println("entry,name,source,block,size");
            calls.println("caller,callee,count");
            gte.println("function,mnemonic,count");
            hw.println("function,block,address,count");

            for (Function f : fm.getFunctions(true)) {
                monitor.checkCancelled();
                MemoryBlock blk = mem.getBlock(f.getEntryPoint());
                fns.printf("%s,%s,%s,%s,%d%n", f.getEntryPoint(), f.getName(),
                        f.getSymbol().getSource(), blk == null ? "?" : blk.getName(),
                        f.getBody().getNumAddresses());

                Map<String, Integer> callCount = new HashMap<>();
                Map<String, Integer> gteCount = new HashMap<>();
                Map<String, Integer> hwCount = new HashMap<>();
                for (Instruction ins : listing.getInstructions(f.getBody(), true)) {
                    int op = (ins.getByte(3) & 0xFF) >>> 2; // big opcode field (LE word, top 6 bits)
                    if (op == 0x12 || op == 0x32 || op == 0x3A) {
                        gteCount.merge(ins.getMnemonicString(), 1, Integer::sum);
                    }
                    for (Reference r : ins.getReferencesFrom()) {
                        Address to = r.getToAddress();
                        if (r.getReferenceType().isCall()) {
                            Function callee = fm.getFunctionAt(to);
                            if (callee != null) {
                                callCount.merge(callee.getEntryPoint().toString(), 1, Integer::sum);
                            }
                        } else if (to.isMemoryAddress()) {
                            long o = to.getOffset();
                            if (o >= 0x1F800000L && o < 0x1F803000L) {
                                MemoryBlock hb = mem.getBlock(to);
                                String key = (hb == null ? "?" : hb.getName()) + "," + to;
                                hwCount.merge(key, 1, Integer::sum);
                            }
                        }
                    }
                }
                String self = f.getEntryPoint().toString();
                callCount.forEach((k, v) -> calls.printf("%s,%s,%d%n", self, k, v));
                gteCount.forEach((k, v) -> gte.printf("%s,%s,%d%n", self, k, v));
                hwCount.forEach((k, v) -> hw.printf("%s,%s,%d%n", self, k, v));
            }
        }
        println("call graph dumped -> " + out);
    }
}
