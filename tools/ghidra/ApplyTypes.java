// Applies docs/types.json and docs/symbols.csv to the headless project, then
// decompiles the listed functions with the types in place.
// Usage (headless): -postScript ApplyTypes.java <types.json> <symbols.csv> <outdir>
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.SymbolTable;
import java.io.File;
import java.io.FileWriter;
import java.nio.file.Files;
import java.util.HashMap;
import java.util.Map;

public class ApplyTypes extends GhidraScript {
    private DataTypeManager dtm;
    private final Map<String, Structure> structs = new HashMap<>();
    private static final CategoryPath CAT = CategoryPath.ROOT;

    private DataType type(String spec) throws Exception {
        spec = spec.trim();
        if (spec.endsWith("*")) {
            return new PointerDataType(type(spec.substring(0, spec.length() - 1)), 4, dtm);
        }
        int br = spec.indexOf('[');
        if (br > 0) {
            int n = Integer.decode(spec.substring(br + 1, spec.indexOf(']')));
            DataType el = type(spec.substring(0, br));
            return new ArrayDataType(el, n, el.getLength(), dtm);
        }
        switch (spec) {
            case "float": return FloatDataType.dataType;
            case "int": return IntegerDataType.dataType;
            case "uint": return UnsignedIntegerDataType.dataType;
            case "short": return ShortDataType.dataType;
            case "byte": return ByteDataType.dataType;
            case "bool": return BooleanDataType.dataType;
            case "char": return CharDataType.dataType;
            case "void": return VoidDataType.dataType;
            case "undefined4": return Undefined4DataType.dataType;
            case "undefined1": return Undefined1DataType.dataType;
        }
        Structure s = structs.get(spec);
        if (s == null) throw new Exception("unknown type " + spec);
        return s;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        JsonObject root = JsonParser.parseString(Files.readString(new File(args[0]).toPath())).getAsJsonObject();
        dtm = currentProgram.getDataTypeManager();
        JsonObject defs = root.getAsJsonObject("structs");

        for (Map.Entry<String, JsonElement> e : defs.entrySet()) {
            int size = Integer.decode(e.getValue().getAsJsonObject().get("size").getAsString());
            DataType old = dtm.getDataType(CAT, e.getKey());
            if (old != null) dtm.remove(old, monitor);
            StructureDataType s = new StructureDataType(CAT, e.getKey(), size, dtm);
            structs.put(e.getKey(), (Structure) dtm.addDataType(s, DataTypeConflictHandler.REPLACE_HANDLER));
        }
        for (Map.Entry<String, JsonElement> e : defs.entrySet()) {
            Structure s = structs.get(e.getKey());
            JsonObject d = e.getValue().getAsJsonObject();
            if (d.has("base")) {
                Structure b = structs.get(d.get("base").getAsString());
                s.replaceAtOffset(0, b, b.getLength(), "base", null);
            }
            for (JsonElement fe : d.getAsJsonArray("fields")) {
                JsonArray f = fe.getAsJsonArray();
                int off = Integer.decode(f.get(0).getAsString());
                DataType t = type(f.get(1).getAsString());
                String name = f.get(2).getAsString();
                String comment = f.size() > 3 ? f.get(3).getAsString() : null;
                try {
                    s.replaceAtOffset(off, t, t.getLength(), name, comment);
                } catch (Exception ex) {
                    println("skip " + e.getKey() + "." + name + ": " + ex.getMessage());
                }
            }
        }

        int renamed = 0;
        for (String line : Files.readAllLines(new File(args[1]).toPath())) {
            if (!line.startsWith("0x")) continue;
            String[] cols = line.split(",", 3);
            Function f = getFunctionAt(toAddr(Long.decode(cols[0])));
            if (f == null || cols[1].isEmpty()) continue;
            String name = cols[1];
            Namespace ns = currentProgram.getGlobalNamespace();
            int sep = name.lastIndexOf("::");
            if (sep > 0) {
                ns = classNamespace(name.substring(0, sep));
                name = name.substring(sep + 2);
            }
            f.setParentNamespace(ns);
            f.setName(name, SourceType.USER_DEFINED);
            renamed++;
        }
        println("named " + renamed + " functions");

        int methods = 0;
        JsonObject classes = root.getAsJsonObject("methods");
        for (Map.Entry<String, JsonElement> e : classes.entrySet()) {
            Namespace cls = classNamespace(e.getKey());
            for (JsonElement a : e.getValue().getAsJsonArray()) {
                Function f = getFunctionAt(toAddr(Long.decode(a.getAsString())));
                if (f == null) continue;
                if (f.getParentNamespace() != cls) f.setParentNamespace(cls);
                f.setCallingConvention("__thiscall");
                methods++;
            }
        }
        println("bound " + methods + " methods");

        File outDir = new File(args[2]);
        outDir.mkdirs();
        DecompInterface dec = new DecompInterface();
        dec.openProgram(currentProgram);
        int written = 0;
        for (JsonElement a : root.getAsJsonArray("decompile")) {
            Address addr = toAddr(Long.decode(a.getAsString()));
            Function f = getFunctionAt(addr);
            if (f == null) continue;
            DecompileResults r = dec.decompileFunction(f, 120, monitor);
            String body = r.decompileCompleted() ? r.getDecompiledFunction().getC() : "/* failed: " + r.getErrorMessage() + " */\n";
            try (FileWriter w = new FileWriter(new File(outDir, a.getAsString() + ".c"))) {
                w.write("// Mech3.exe 1.2 @ " + a.getAsString() + " " + f.getName(true) + " (with docs/types.json)\n" + body);
            }
            written++;
        }
        println("wrote " + written + " typed decompilations");
    }

    private Namespace classNamespace(String name) throws Exception {
        SymbolTable st = currentProgram.getSymbolTable();
        Namespace ns = st.getNamespace(name, currentProgram.getGlobalNamespace());
        if (ns == null) return st.createClass(currentProgram.getGlobalNamespace(), name, SourceType.USER_DEFINED);
        if (!(ns instanceof ghidra.program.model.listing.GhidraClass)) {
            return ghidra.app.util.NamespaceUtils.convertNamespaceToClass(ns);
        }
        return ns;
    }
}
