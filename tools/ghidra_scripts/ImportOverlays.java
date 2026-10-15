// Map KKIIDDZZ BNS code overlays into the main program as Ghidra overlay blocks,
// and add an uninitialized RAM block for BSS/heap above the EXE's text.
// Run as a -preScript so auto-analysis picks up the new code.
// Args: <overlay_map.json> <members dir>
// @category ApeEscape
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.Files;
import java.nio.file.Path;

public class ImportOverlays extends GhidraScript {
    private static final long RAM_END = 0x80200000L;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        Path mapPath = Path.of(args[0]);
        Path membersDir = Path.of(args[1]);
        Memory mem = currentProgram.getMemory();

        println("Memory map before:");
        for (MemoryBlock b : mem.getBlocks()) {
            println(String.format("  %-16s %s-%s init=%s ovl=%s", b.getName(), b.getStart(), b.getEnd(),
                    b.isInitialized(), b.isOverlay()));
        }

        addBss(mem);

        JsonObject root = JsonParser.parseString(Files.readString(mapPath)).getAsJsonObject();
        int added = 0;
        for (JsonElement e : root.getAsJsonArray("overlays")) {
            JsonObject o = e.getAsJsonObject();
            int index = Integer.decode(o.get("index").getAsString());
            String name = String.format("ovl_%03X", index);
            if (mem.getBlock(name) != null) {
                println("skip existing " + name);
                continue;
            }
            byte[] data = Files.readAllBytes(membersDir.resolve(String.format("%03X.bin", index)));
            Address base = toAddr(o.get("load_addr").getAsString());
            MemoryBlock blk = mem.createInitializedBlock(name, base,
                    new java.io.ByteArrayInputStream(data), data.length, monitor, true);
            blk.setRead(true);
            blk.setWrite(true);
            blk.setExecute(true);
            blk.setComment("BNS member " + o.get("index").getAsString() + " - " + o.get("status").getAsString());

            Address ovlBase = blk.getStart();
            for (JsonElement ent : o.getAsJsonArray("entries")) {
                long off = Long.decode(ent.getAsString()) - base.getOffset();
                Address a = ovlBase.add(off);
                disassemble(a);
                if (getFunctionAt(a) == null) {
                    createFunction(a, null);
                }
                createLabel(a, String.format("%s_entry_%08X", name, base.getOffset() + off), false,
                        SourceType.ANALYSIS);
            }
            added++;
        }
        println("overlay blocks added: " + added);
    }

    /** Uninitialized RAM from the end of loaded text up to 2 MB, minus existing blocks. */
    private void addBss(Memory mem) throws Exception {
        if (mem.getBlock("bss_heap") != null) {
            return;
        }
        Address start = null;
        for (MemoryBlock b : mem.getBlocks()) {
            if (!b.isOverlay() && b.isInitialized() && b.getStart().getOffset() >= 0x80010000L
                    && b.getStart().getOffset() < RAM_END) {
                Address next = b.getEnd().add(1);
                if (start == null || next.compareTo(start) > 0) {
                    start = next;
                }
            }
        }
        // Stop at the first existing block above the text (the loader may already map overlay areas).
        long end = RAM_END;
        for (MemoryBlock b : mem.getBlocks()) {
            long s = b.getStart().getOffset();
            if (!b.isOverlay() && s >= start.getOffset() && s < end) {
                end = s;
            }
        }
        if (end > start.getOffset()) {
            mem.createUninitializedBlock("bss_heap", start, end - start.getOffset(), false);
            println(String.format("bss_heap added %s-%08X", start, end - 1));
        } else {
            println("no gap for bss_heap after " + start);
        }
    }
}
