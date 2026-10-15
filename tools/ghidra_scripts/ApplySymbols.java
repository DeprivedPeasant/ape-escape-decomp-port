// Apply names from tools/symbols.csv: address,name,comment
// Address is plain hex for main RAM or "ovl_18C::8013d000" for overlay space.
// Lines starting with # are ignored. Creates a function at the address if none exists.
// Args: <symbols.csv>
// @category ApeEscape
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.Files;
import java.nio.file.Path;

public class ApplySymbols extends GhidraScript {
    @Override
    protected void run() throws Exception {
        int applied = 0, failed = 0;
        for (String line : Files.readAllLines(Path.of(getScriptArgs()[0]))) {
            line = line.strip();
            if (line.isEmpty() || line.startsWith("#")) {
                continue;
            }
            String[] f = line.split(",", 3);
            Address a = currentProgram.getAddressFactory().getAddress(f[0].strip());
            if (a == null) {
                println("bad address: " + line);
                failed++;
                continue;
            }
            String name = f[1].strip();
            if (name.startsWith("g_")) {
                // Global data label, not a function.
                for (var s : currentProgram.getSymbolTable().getSymbols(a)) {
                    if (s.getSource() == SourceType.DEFAULT || s.getSource() == SourceType.ANALYSIS) {
                        s.delete();
                    }
                }
                createLabel(a, name, true, SourceType.USER_DEFINED);
                if (f.length > 2 && !f[2].isBlank()) {
                    currentProgram.getListing().setComment(a, CodeUnit.EOL_COMMENT, f[2].strip());
                }
                applied++;
                continue;
            }
            Function fn = getFunctionAt(a);
            if (fn == null) {
                disassemble(a);
                fn = createFunction(a, name);
            }
            if (fn == null) {
                println("could not create function: " + line);
                failed++;
                continue;
            }
            fn.setName(name, SourceType.USER_DEFINED);
            if (f.length > 2 && !f[2].isBlank()) {
                currentProgram.getListing().setComment(a, CodeUnit.PLATE_COMMENT, f[2].strip());
            }
            applied++;
        }
        println("symbols applied=" + applied + " failed=" + failed);
    }
}
