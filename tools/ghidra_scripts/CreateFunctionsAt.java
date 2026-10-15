// Create functions at the given addresses (code reachable only via data tables).
// Args: <addr hex> [<addr hex> ...]
// @category ApeEscape
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class CreateFunctionsAt extends GhidraScript {
    @Override
    protected void run() throws Exception {
        int created = 0, existing = 0;
        for (String a : getScriptArgs()) {
            Address addr = currentProgram.getAddressFactory().getAddress(a);
            if (getFunctionAt(addr) != null) {
                existing++;
                continue;
            }
            disassemble(addr);
            if (createFunction(addr, null) != null) {
                created++;
            } else {
                println("could not create function at " + a);
            }
        }
        println("functions created=" + created + " existing=" + existing);
    }
}
