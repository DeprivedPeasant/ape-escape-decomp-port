// Summarize function coverage: total, auto-named (FUN_), and named (signatures/user).
// @category ApeEscape
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.util.Map;
import java.util.TreeMap;

public class FunctionStats extends GhidraScript {
    @Override
    protected void run() throws Exception {
        int total = 0, unnamed = 0;
        Map<String, Integer> bySource = new TreeMap<>();
        StringBuilder named = new StringBuilder();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            total++;
            SourceType src = f.getSymbol().getSource();
            bySource.merge(src.toString(), 1, Integer::sum);
            if (src == SourceType.DEFAULT) {
                unnamed++;
            } else {
                named.append(f.getEntryPoint()).append(' ').append(f.getName()).append('\n');
            }
        }
        println("functions=" + total + " unnamed=" + unnamed + " bySource=" + bySource);
        String out = getScriptArgs().length > 0 ? getScriptArgs()[0] : null;
        if (out != null) {
            java.nio.file.Files.writeString(java.nio.file.Path.of(out), named.toString());
            println("named list -> " + out);
        }
    }
}
