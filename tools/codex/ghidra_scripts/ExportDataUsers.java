// Export direct function users of named global addresses; private analysis only.
// @category ApeEscape
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;
import java.util.*;
public class ExportDataUsers extends GhidraScript {
 public void run() throws Exception {
  String[] args=getScriptArgs();
  Set<Function> functions=new TreeSet<>((a,b)->a.getEntryPoint().compareTo(b.getEntryPoint()));
  for(int i=1;i<args.length;i++) {
   var address=currentProgram.getAddressFactory().getAddress(args[i]);
   for(var ref: getReferencesTo(address)) {
    var function=getFunctionContaining(ref.getFromAddress());
    if(function!=null) functions.add(function);
   }
  }
  var decompiler=new DecompInterface();decompiler.openProgram(currentProgram);
  try(var out=new PrintWriter(args[0],"UTF-8")) {
   for(var function:functions) {
    monitor.checkCancelled();
    out.println("/* ==== "+function.getName()+" @ "+function.getEntryPoint()+" */");
    var result=decompiler.decompileFunction(function,60,monitor);
    out.println(result.decompileCompleted()?result.getDecompiledFunction().getC():"// ERROR "+result.getErrorMessage());
   }
  }
  decompiler.dispose();println("exported global users="+functions.size());
 }
}
