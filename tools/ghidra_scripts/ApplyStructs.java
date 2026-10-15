// Create/replace structs from tools/structs.txt and apply types at addresses.
// Args: <structs.txt>
// @category ApeEscape
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public class ApplyStructs extends GhidraScript {
    private static final CategoryPath CAT = new CategoryPath("/ApeEscape");
    private DataTypeManager dtm;

    @Override
    protected void run() throws Exception {
        dtm = currentProgram.getDataTypeManager();
        List<String> lines = Files.readAllLines(Path.of(getScriptArgs()[0]));
        StructureDataType cur = null;
        int structs = 0, applied = 0;
        for (String raw : lines) {
            String line = raw.split(";", 2)[0].strip();
            String comment = raw.contains(";") ? raw.split(";", 2)[1].strip() : null;
            if (line.isEmpty() || line.startsWith("#")) {
                continue;
            }
            String[] t = line.split("\\s+");
            if (t[0].equals("struct")) {
                cur = new StructureDataType(CAT, t[1], Integer.decode(t[2]), dtm);
            } else if (t[0].equals("end")) {
                dtm.addDataType(cur, DataTypeConflictHandler.REPLACE_HANDLER);
                println("struct " + cur.getName() + " size=0x" + Integer.toHexString(cur.getLength()));
                cur = null;
                structs++;
            } else if (t[0].equals("apply")) {
                Address a = toAddr(Long.parseLong(t[1], 16));
                DataType dt = resolve(t[2]);
                Address last = a.add(dt.getLength() - 1);
                // Drop our old per-field labels inside the range; the struct names them now.
                for (var s : currentProgram.getSymbolTable().getSymbols(
                        new ghidra.program.model.address.AddressSet(a.add(1), last), ghidra.program.model.symbol.SymbolType.LABEL, true)) {
                    if (s.getSource() == ghidra.program.model.symbol.SourceType.USER_DEFINED) {
                        println("  removing label " + s.getName() + " @ " + s.getAddress());
                        s.delete();
                    }
                }
                clearListing(a, last);
                createData(a, dt);
                println("applied " + dt.getName() + " @ " + a);
                applied++;
            } else if (cur != null) {
                int off = Integer.decode(t[0]);
                DataType dt = resolve(t[1]);
                cur.replaceAtOffset(off, dt, dt.getLength(), t[2], comment);
            }
        }
        println("structs=" + structs + " applied=" + applied);
    }

    private DataType resolve(String spec) throws Exception {
        if (spec.endsWith("]")) {
            int i = spec.lastIndexOf('[');
            DataType base = resolve(spec.substring(0, i));
            int n = Integer.decode(spec.substring(i + 1, spec.length() - 1));
            return new ArrayDataType(base, n, base.getLength(), dtm);
        }
        if (spec.endsWith("*")) {
            return new PointerDataType(resolve(spec.substring(0, spec.length() - 1)), 4, dtm);
        }
        switch (spec) {
            case "u8": return ByteDataType.dataType;
            case "s8": return SignedByteDataType.dataType;
            case "u16": return UnsignedShortDataType.dataType;
            case "s16": return ShortDataType.dataType;
            case "u32": return UnsignedIntegerDataType.dataType;
            case "s32": return IntegerDataType.dataType;
            default:
                DataType dt = dtm.getDataType(CAT, spec);
                if (dt != null) {
                    return dt;
                }
                // Fall back to any category (e.g. PsyQ types like MATRIX from the loader's archive).
                var it = dtm.getAllDataTypes();
                while (it.hasNext()) {
                    DataType d = it.next();
                    if (d.getName().equals(spec)) {
                        return d;
                    }
                }
                throw new IllegalArgumentException("unknown type " + spec);
        }
    }
}
