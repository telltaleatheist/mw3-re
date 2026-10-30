// Headless post-script for another exe (Pirate's Moon) the way MW3's analysis
// was made: after Ghidra's analysis, functions are created at the code
// pointers in the data sections (vtables, callback tables; vtscan.py's test),
// then each function's facts go to <outdir>/functions.jsonl for
// tools/analysis/pmdiff.py, and with "decompile" its pseudocode to
// <outdir>/<addr>.c. Usage: -postScript DumpFunctions.java <outdir> <label> [decompile]
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import java.io.File;
import java.io.FileWriter;
import java.io.Writer;
import java.util.TreeSet;

public class DumpFunctions extends GhidraScript {
    static String esc(String s) {
        StringBuilder b = new StringBuilder();
        for (char c : s.toCharArray()) {
            if (c == '"' || c == '\\') b.append('\\').append(c);
            else if (c < 0x20 || c > 0x7e) b.append(String.format("\\u%04x", (int) c));
            else b.append(c);
        }
        return b.toString();
    }

    static String hex(long v) { return "0x" + Long.toHexString(v); }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File outDir = new File(args[0]);
        String label = args[1];
        boolean decompile = args.length > 2 && args[2].equals("decompile");
        outDir.mkdirs();
        Memory mem = currentProgram.getMemory();

        // The code pointers in the data sections whose targets are no function
        // yet, with a plausible first byte (vtscan.py's list).
        MemoryBlock text = null;
        for (MemoryBlock b : mem.getBlocks()) if (b.isExecute() && (text == null || b.getSize() > text.getSize())) text = b;
        String starts = "55 56 53 57 83 81 8b 6a 64 a1 33 b8 c3 d9 51 68 e9 c2 8a 0f";
        int created = 0;
        for (int pass = 0; pass < 2; pass++) {
            for (MemoryBlock b : mem.getBlocks()) {
                if (b.isExecute() || !b.isInitialized()) continue;
                for (long off = 0; off + 4 <= b.getSize(); off += 4) {
                    Address at = b.getStart().add(off);
                    long v = mem.getInt(at) & 0xffffffffL;
                    if (v < text.getStart().getOffset() || v >= text.getEnd().getOffset()) continue;
                    Address t = toAddr(v);
                    if (getFunctionContaining(t) != null) continue;
                    String first = String.format("%02x", mem.getByte(t) & 0xff);
                    if (!starts.contains(first)) continue;
                    if (getInstructionAt(t) == null) disassemble(t);
                    if (createFunction(t, null) != null) created++;
                }
            }
            analyzeChanges(currentProgram);
        }
        println(label + ": created " + created + " functions from data pointers");

        DecompInterface dec = null;
        if (decompile) {
            dec = new DecompInterface();
            dec.openProgram(currentProgram);
        }
        int n = 0;
        try (Writer w = new FileWriter(new File(outDir, "functions.jsonl"))) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (f.isExternal()) continue;
                StringBuilder j = new StringBuilder();
                j.append("{\"addr\":\"").append(hex(f.getEntryPoint().getOffset())).append("\"");
                j.append(",\"name\":\"").append(esc(f.getName())).append("\"");
                j.append(",\"thunk\":").append(f.isThunk());
                j.append(",\"ranges\":[");
                boolean first = true;
                for (AddressRange r : f.getBody().getAddressRanges()) {
                    j.append(first ? "" : ",").append("[\"").append(hex(r.getMinAddress().getOffset())).append("\",\"")
                     .append(hex(r.getMaxAddress().getOffset() + 1)).append("\"]");
                    first = false;
                }
                j.append("],\"callees\":[");
                first = true;
                for (Function c : f.getCalledFunctions(monitor)) {
                    j.append(first ? "" : ",").append("\"").append(hex(c.getEntryPoint().getOffset())).append("\"");
                    first = false;
                }
                j.append("],\"callers\":[");
                first = true;
                for (Function c : f.getCallingFunctions(monitor)) {
                    j.append(first ? "" : ",").append("\"").append(hex(c.getEntryPoint().getOffset())).append("\"");
                    first = false;
                }
                // The strings it refers to, the external calls by name, the
                // scalar constants that are not addresses, its mnemonics.
                TreeSet<String> strings = new TreeSet<>(), imports = new TreeSet<>();
                TreeSet<Long> consts = new TreeSet<>();
                StringBuilder mn = new StringBuilder();
                for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                    mn.append(mn.length() == 0 ? "" : " ").append(ins.getMnemonicString());
                    for (Reference r : ins.getReferencesFrom()) {
                        Address to = r.getToAddress();
                        if (to.isExternalAddress()) {
                            ghidra.program.model.symbol.Symbol s = currentProgram.getSymbolTable().getPrimarySymbol(to);
                            if (s != null) imports.add(s.getName());
                            continue;
                        }
                        Function ext = getFunctionAt(to);
                        if (ext != null && ext.isThunk() && ext.getThunkedFunction(true) != null && ext.getThunkedFunction(true).isExternal())
                            imports.add(ext.getThunkedFunction(true).getName());
                        Data d = getDataAt(to);
                        if (d != null && d.hasStringValue()) strings.add(String.valueOf(d.getValue()));
                        else if (d != null && d.isPointer() && d.getValue() instanceof Address) {
                            Data s2 = getDataAt((Address) d.getValue());
                            if (s2 != null && s2.hasStringValue()) strings.add(String.valueOf(s2.getValue()));
                        }
                    }
                    for (int i = 0; i < ins.getNumOperands(); i++)
                        for (Object o : ins.getOpObjects(i))
                            if (o instanceof Scalar) {
                                long v = ((Scalar) o).getUnsignedValue();
                                if (mem.contains(toAddr(v)) || v < 0x10) continue;
                                consts.add(v);
                            }
                }
                j.append("],\"strings\":[");
                first = true;
                for (String s : strings) { j.append(first ? "" : ",").append("\"").append(esc(s)).append("\""); first = false; }
                j.append("],\"imports\":[");
                first = true;
                for (String s : imports) { j.append(first ? "" : ",").append("\"").append(esc(s)).append("\""); first = false; }
                j.append("],\"consts\":[");
                first = true;
                for (Long v : consts) { j.append(first ? "" : ",").append(v); first = false; }
                j.append("],\"mnemonics\":\"").append(mn).append("\"}\n");
                w.write(j.toString());
                n++;
                if (dec != null && !f.isThunk()) {
                    String addr = hex(f.getEntryPoint().getOffset());
                    DecompileResults r = dec.decompileFunction(f, 60, monitor);
                    String body = r.decompileCompleted() ? r.getDecompiledFunction().getC() : "/* decompile failed: " + r.getErrorMessage() + " */\n";
                    try (FileWriter cw = new FileWriter(new File(outDir, addr + ".c"))) {
                        cw.write("// " + label + " @ " + addr + " " + f.getName() + "\n" + body);
                    }
                }
                if (n % 1000 == 0) println(label + ": " + n + " functions");
            }
        }
        println(label + ": " + n + " functions written");
    }
}
