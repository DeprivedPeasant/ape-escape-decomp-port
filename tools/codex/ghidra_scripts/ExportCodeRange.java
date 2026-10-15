// Original instruction range, including SDK functions split by Ghidra.
// Args: output file, inclusive start, exclusive end.
// @category ApeEscape
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import java.nio.file.*;
import java.io.PrintWriter;
public class ExportCodeRange extends GhidraScript {
 public void run() throws Exception {
  String[] a=getScriptArgs();
  var start=toAddr(a[1]);var end=toAddr(a[2]).subtract(1);
  Path path=Paths.get(a[0]);Files.createDirectories(path.getParent());
  int count=0;
  try(PrintWriter out=new PrintWriter(path.toString(),"UTF-8")) {
   var instructions=currentProgram.getListing().getInstructions(new AddressSet(start,end),true);
   while(instructions.hasNext()) {
    monitor.checkCancelled();var i=instructions.next();
    var block=currentProgram.getMemory().getBlock(i.getAddress());
    if(block==null||!block.isInitialized())throw new IllegalStateException("Uninitialized instruction");
    out.println(i.getAddress()+"\t"+i.toString());count++;
   }
  }
  if(count==0)throw new IllegalStateException("No instructions in requested range");
  println("CODE_RANGE count="+count);
 }
}
