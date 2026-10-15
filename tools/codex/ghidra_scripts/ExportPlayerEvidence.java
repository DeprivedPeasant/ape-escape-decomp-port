// Selected player roots and direct callees, including named helpers.
// Args: output directory, newline-separated root addresses file.
// @category ApeEscape
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import java.nio.file.*;
import java.io.PrintWriter;
import java.util.*;

public class ExportPlayerEvidence extends GhidraScript {
 public void run() throws Exception {
  String[] args=getScriptArgs();
  Path dir=Paths.get(args[0]); Files.createDirectories(dir);
  Comparator<Function> order=(a,b)->a.getEntryPoint().compareTo(b.getEntryPoint());
  Set<Function> roots=new TreeSet<>(order), selected=new TreeSet<>(order);
  for(String line:Files.readAllLines(Paths.get(args[1]))) {
   if(line.isBlank()) continue;
   var address=currentProgram.getAddressFactory().getAddress(line.trim());
   Function f=getFunctionAt(address);
   if(f==null) throw new IllegalStateException("Missing root "+line);
   roots.add(f);
  }
  selected.addAll(roots);
  for(Function f:roots) selected.addAll(f.getCalledFunctions(monitor));
  // Ghidra's overlay/RAM artifacts are not executable evidence.
  selected.removeIf(f -> currentProgram.getMemory().getBlock(f.getEntryPoint()) == null ||
                        !currentProgram.getMemory().getBlock(f.getEntryPoint()).isInitialized());
  if(!selected.containsAll(roots)) throw new IllegalStateException("Uninitialized root");
  DecompInterface dec=new DecompInterface(); dec.openProgram(currentProgram);
  int failed=0;
  try(PrintWriter index=new PrintWriter(dir.resolve("functions.tsv").toString(),"UTF-8");
      PrintWriter deps=new PrintWriter(dir.resolve("dependencies.tsv").toString(),"UTF-8");
      PrintWriter refs=new PrintWriter(dir.resolve("data_references.tsv").toString(),"UTF-8")) {
   index.println("address\tname\trole\tbytes\tdecompile_ok\tfile");
   deps.println("caller_address\tcaller\tcallee_address\tcallee\tincluded");
   refs.println("function\tfrom\tto\tsymbol\ttype");
   for(Function f:selected) {
    monitor.checkCancelled();
    String stem=f.getEntryPoint().toString().replace(':','_')+"_"+f.getName().replaceAll("[^A-Za-z0-9_]","_");
    var result=dec.decompileFunction(f,120,monitor);
    boolean ok=result.decompileCompleted(); if(!ok) failed++;
    try(PrintWriter out=new PrintWriter(dir.resolve(stem+".c").toString(),"UTF-8")) {
     out.println("/* Ghidra reference pseudocode; not buildable port source. */");
     out.println("/* "+f.getName()+" @ "+f.getEntryPoint()+"; role="+(roots.contains(f)?"root":"direct callee")+" */");
     out.println(ok?result.getDecompiledFunction().getC():"/* DECOMPILE FAILED: "+result.getErrorMessage()+" */");
    }
    try(PrintWriter out=new PrintWriter(dir.resolve(stem+".asm.txt").toString(),"UTF-8")) {
     var ins=currentProgram.getListing().getInstructions(f.getBody(),true);
     while(ins.hasNext()) {
      Instruction i=ins.next(); out.println(i.getAddress()+"\t"+i.toString());
      for(var ref:i.getReferencesFrom()) {
       if(ref.getReferenceType().isFlow()||!ref.getToAddress().isMemoryAddress()) continue;
       var sym=getSymbolAt(ref.getToAddress());
       refs.println(f.getName()+"\t"+i.getAddress()+"\t"+ref.getToAddress()+"\t"+(sym==null?"":sym.getName())+"\t"+ref.getReferenceType());
      }
     }
    }
    index.println(f.getEntryPoint()+"\t"+f.getName()+"\t"+(roots.contains(f)?"root":"direct callee")+"\t"+f.getBody().getNumAddresses()+"\t"+ok+"\t"+stem+".c");
    Set<Function> callees=new TreeSet<>(order); callees.addAll(f.getCalledFunctions(monitor));
    for(Function c:callees)
     deps.println(f.getEntryPoint()+"\t"+f.getName()+"\t"+c.getEntryPoint()+"\t"+c.getName()+"\t"+selected.contains(c));
   }
  } finally { dec.dispose(); }
  println("PLAYER_EVIDENCE roots="+roots.size()+" functions="+selected.size()+" failed="+failed);
  if(failed>0) throw new IllegalStateException("Decompiler failures="+failed);
 }
}
