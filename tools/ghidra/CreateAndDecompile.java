// Creates functions at the addresses listed in a file (one hex address per line),
// lets analysis settle, then decompiles every function that is not already in the
// output directory. Usage (headless): -postScript CreateAndDecompile.java <addresses.txt> <outdir>
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.io.File;
import java.io.FileWriter;
import java.nio.file.Files;

public class CreateAndDecompile extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File outDir = new File(args[1]);
        int created = 0;
        for (String line : Files.readAllLines(new File(args[0]).toPath())) {
            line = line.trim();
            if (line.isEmpty()) continue;
            Address a = toAddr(Long.decode(line));
            if (getFunctionAt(a) != null) continue;
            if (getInstructionAt(a) == null) disassemble(a);
            if (createFunction(a, null) != null) created++;
        }
        println("created " + created + " functions");
        analyzeChanges(currentProgram);
        DecompInterface dec = new DecompInterface();
        dec.openProgram(currentProgram);
        int written = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (f.isExternal() || f.isThunk()) continue;
            String addr = "0x" + Long.toHexString(f.getEntryPoint().getOffset());
            File out = new File(outDir, addr + ".c");
            if (out.exists()) continue;
            DecompileResults r = dec.decompileFunction(f, 60, monitor);
            String body = r.decompileCompleted() ? r.getDecompiledFunction().getC() : "/* decompile failed: " + r.getErrorMessage() + " */\n";
            try (FileWriter w = new FileWriter(out)) {
                w.write("// Mech3.exe 1.2 @ " + addr + " " + f.getName() + " (created from a data pointer)\n" + body);
            }
            written++;
        }
        println("wrote " + written + " new decompilations");
    }
}
