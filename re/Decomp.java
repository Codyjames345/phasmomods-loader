// Ghidra headless script: decompile functions at the given GameAssembly RVAs.
// args: <outFile> <rva> [<rva> ...]   (RVAs in hex, relative to the image base)
// Functions are created on demand because the program is imported without auto-analysis.
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;

import java.io.FileWriter;
import java.io.PrintWriter;

public class Decomp extends GhidraScript {
  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
      DecompInterface ifc = new DecompInterface();
      ifc.openProgram(currentProgram);
      Address base = currentProgram.getImageBase();
      for (int i = 1; i < args.length; i++) {
        long rva = Long.parseUnsignedLong(args[i].replace("0x", ""), 16);
        Address a = base.add(rva);
        Function f = getFunctionAt(a);
        if (f == null) {
          new DisassembleCommand(a, new AddressSet(a), true).applyTo(currentProgram, monitor);
          f = createFunction(a, null);
        }
        out.println("// ===== RVA 0x" + Long.toHexString(rva).toUpperCase());
        if (f == null) {
          out.println("// could not create function");
          continue;
        }
        DecompileResults r = ifc.decompileFunction(f, 120, monitor);
        out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompile failed: " + r.getErrorMessage());
      }
      ifc.dispose();
    }
  }
}
