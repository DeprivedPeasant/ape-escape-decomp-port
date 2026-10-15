// Function boundary inspection for Codex's overlay dispatch inventory.
// Args: input addresses TSV, output TSV, optional "define" (private scratch only).
// @category ApeEscape
import ghidra.app.script.GhidraScript;
import java.nio.file.Files;
import java.nio.file.Path;
import java.io.PrintWriter;

public class InspectDispatchEntries extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        boolean define = args.length > 2 && args[2].equals("define");
        int count = 0;
        try (PrintWriter out = new PrintWriter(args[1], "UTF-8")) {
            out.println("address\texact_function\tcontaining_entry\tcontaining_name\tsource\tinstruction\tword");
            for (String line : Files.readAllLines(Path.of(args[0]))) {
                if (line.isBlank() || line.startsWith("#")) continue;
                String address = line.split("\\t")[0];
                var a = currentProgram.getAddressFactory().getAddress(address);
                if (a == null || !currentProgram.getMemory().contains(a)) {
                    out.println(address + "\t\t\t\t\tMISSING_MEMORY\t");
                    continue;
                }
                var exact = getFunctionAt(a);
                var containing = getFunctionContaining(a);
                if (define && exact == null && containing == null) {
                    int word = currentProgram.getMemory().getInt(a);
                    // The only non-stack entry was independently inspected: an Obj field
                    // load at the proven dispatch address, followed by a leaf state handler.
                    boolean inspectedLeaf = address.equals("ovl_191::8013e090") &&
                        (word >>> 26) == 0x23 && ((word >>> 21) & 31) == 4 && (word & 0xffff) == 0xe0;
                    // Proven camera handler table index3: four-instruction leaf
                    // sets CameraSelectMode's force-follow flag at 0x800E6912.
                    boolean forceFollowLeaf = address.equals("8004b580") && word == 0x3c03800e &&
                        currentProgram.getMemory().getInt(a.add(4)) == 0x24020001 &&
                        currentProgram.getMemory().getInt(a.add(8)) == 0x03e00008 &&
                        currentProgram.getMemory().getInt(a.add(12)) == 0xa0626912;
                    inspectedLeaf = inspectedLeaf || forceFollowLeaf;
                    if (!inspectedLeaf && ((word & 0xffff0000) != 0x27bd0000 || (short)word >= 0)) {
                        throw new IllegalStateException("Not a negative stack prologue: " + address);
                    }
                    disassemble(a);
                    exact = createFunction(a, null);
                    if (exact == null) throw new IllegalStateException("Could not define: " + address);
                    containing = exact;
                }
                var instruction = getInstructionAt(a);
                out.println(address + "\t" + (exact == null ? "" : exact.getName()) + "\t" +
                    (containing == null ? "" : containing.getEntryPoint()) + "\t" +
                    (containing == null ? "" : containing.getName()) + "\t" +
                    (exact == null ? "" : exact.getSymbol().getSource()) + "\t" +
                    (instruction == null ? "UNDECODED" : instruction.toString()) + "\t" +
                    String.format("%08X", currentProgram.getMemory().getInt(a)));
                count++;
            }
        }
        println("dispatch entries inspected=" + count);
    }
}
