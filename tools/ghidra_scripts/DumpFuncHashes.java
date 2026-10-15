// Dump a position-independent hash of every function (opcode / register shape with addresses masked), so
// identical code duplicated across overlays can be matched and named once (tools/propagate_names.py).
// Args: <out csv>
// Writes: entry,name,block,size,hash,shape (shape = opcode sequence only, a looser match)
// @category ApeEscape
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import java.io.PrintWriter;
import java.nio.file.Path;

public class DumpFuncHashes extends GhidraScript {
    @Override
    protected void run() throws Exception {
        Path out = Path.of(getScriptArgs()[0]);
        var fm = currentProgram.getFunctionManager();
        var listing = currentProgram.getListing();
        var mem = currentProgram.getMemory();
        try (PrintWriter w = new PrintWriter(out.toFile(), "UTF-8")) {
            w.println("entry,name,block,size,hash,shape");
            for (Function f : fm.getFunctions(true)) {
                monitor.checkCancelled();
                MemoryBlock blk = mem.getBlock(f.getEntryPoint());
                String bn = blk == null ? "?" : blk.getName();
                if (!bn.startsWith("ovl_") && !bn.equals("CODE")) {
                    continue;
                }
                long h = 1125899906842597L;
                long sh = 7L;
                int n = 0;
                for (Instruction ins : listing.getInstructions(f.getBody(), true)) {
                    long word = 0;
                    for (int i = 3; i >= 0; i--) {
                        word = (word << 8) | (ins.getByte(i) & 0xFF);
                    }
                    int op = (int) (word >>> 26) & 0x3F;
                    long m = word;
                    if (op == 2 || op == 3) {
                        m = word & ~0x3FFFFFFL; // j / jal target
                    } else if (op >= 4 && op <= 7 || op == 1) {
                        m = word & ~0xFFFFL; // branches
                    } else if (op == 0xF) {
                        long imm = word & 0xFFFF;
                        if (imm >= 0x8000 && imm < 0x8020) {
                            m = word & ~0xFFFFL; // lui of a RAM address
                        }
                    } else if (ins.getReferencesFrom().length > 0 && op >= 0x20) {
                        m = word & ~0xFFFFL; // load / store through an address
                    } else if (ins.getReferencesFrom().length > 0 && op >= 8 && op <= 0xE) {
                        m = word & ~0xFFFFL; // addiu / ori forming an address
                    }
                    h = h * 1000003L + m;
                    sh = sh * 1000003L + (op == 0 ? (word & 0x3F) | 0x100 : op);
                    n++;
                }
                w.printf("%s,%s,%s,%d,%016x,%016x%n", f.getEntryPoint(), f.getName(), bn, n, h, sh);
            }
        }
    }
}
