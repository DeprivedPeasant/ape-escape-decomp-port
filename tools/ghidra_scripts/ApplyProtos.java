// Apply C function prototypes from tools/protos.txt (one per line: "<addr hex> <C prototype>;").
// Types may be Ghidra built-ins (int, short, ushort, uint, byte, char, void, undefined4, ...) or any struct
// from tools/structs.txt (category /ApeEscape). Overlay functions: use "ovl_XXX::addr". A name in the prototype
// renames the function; use the existing name from symbols.csv to keep it.
// Args: <protos.txt>
// @category ApeEscape
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.app.util.parser.FunctionSignatureParser;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public class ApplyProtos extends GhidraScript {
    @Override
    protected void run() throws Exception {
        List<String> lines = Files.readAllLines(Path.of(getScriptArgs()[0]));
        FunctionSignatureParser parser =
            new FunctionSignatureParser(currentProgram.getDataTypeManager(), null);
        int ok = 0, failed = 0;
        for (String raw : lines) {
            String line = raw.strip();
            if (line.isEmpty() || line.startsWith("#")) {
                continue;
            }
            int sp = line.indexOf(' ');
            String addrText = line.substring(0, sp);
            String proto = line.substring(sp + 1).strip().replaceAll(";$", "");
            try {
                Address a = addrText.contains("::")
                    ? currentProgram.getAddressFactory().getAddress(addrText)
                    : toAddr(Long.parseLong(addrText, 16));
                Function f = getFunctionAt(a);
                if (f == null) {
                    println("no function at " + addrText);
                    failed++;
                    continue;
                }
                FunctionDefinitionDataType def = parser.parse(f.getSignature(), proto);
                ApplyFunctionSignatureCmd cmd =
                    new ApplyFunctionSignatureCmd(a, def, SourceType.USER_DEFINED, true, true);
                if (cmd.applyTo(currentProgram)) {
                    ok++;
                } else {
                    println("apply failed " + addrText + ": " + cmd.getStatusMsg());
                    failed++;
                }
            } catch (Exception e) {
                println("bad prototype " + addrText + " '" + proto + "': " + e.getMessage());
                failed++;
            }
        }
        println("protos applied=" + ok + " failed=" + failed);
    }
}
