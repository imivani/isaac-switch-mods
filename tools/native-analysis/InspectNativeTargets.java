// @category Isaac
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.SourceType;
import com.google.gson.GsonBuilder;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

public class InspectNativeTargets extends GhidraScript {
    Function makeFunction(long offset, int size, String name, boolean disassemble) throws Exception {
        Address address = currentProgram.getImageBase().add(offset);
        AddressSet body = new AddressSet(address, address.add(Math.max(size, 4) - 1));
        if (disassemble) new DisassembleCommand(address, body, true).applyTo(currentProgram, monitor);
        Function f = getFunctionAt(address);
        if (f == null) f = currentProgram.getFunctionManager().createFunction(name, address, body, SourceType.USER_DEFINED);
        else f.setBody(body);
        return f;
    }
    public void run() throws Exception {
        Path output = Path.of(getScriptArgs()[0]);
        Map<Long,String[]> stubs = new HashMap<>();
        for(String line:Files.readAllLines(output.resolve("plt.tsv"))) {
            String[] p=line.split("\t"); stubs.put(Long.parseUnsignedLong(p[0],16),p);
        }
        List<Function> targets = new ArrayList<>();
        List<Map<String,Object>> report = new ArrayList<>();
        for(String line:Files.readAllLines(output.resolve("targets.tsv"))) {
            String[] p=line.split("\t");
            long offset=Long.parseUnsignedLong(p[0],16); int size=Integer.parseInt(p[1]);
            Function f=makeFunction(offset,size,p[2],true); targets.add(f);
            Map<String,Object> result = new LinkedHashMap<>(); result.put("name",p[2]);result.put("offset",p[0]);result.put("size",size);report.add(result);
        }
        // Resolve each target's immediate PLT calls from relocation-table evidence.
        for(Function f:targets) {
            InstructionIterator iter=currentProgram.getListing().getInstructions(f.getBody(),true);
            while(iter.hasNext()) {
                Instruction ins=iter.next();
                for(Address flow:ins.getFlows()) {
                    long offset=flow.subtract(currentProgram.getImageBase()); String[] p=stubs.get(offset);
                    if(p==null) continue;
                    Function stub=makeFunction(offset,16,p[1]+"_plt",true);
                    if(p[4].equals("1")) {
                        Function target=makeFunction(Long.parseUnsignedLong(p[2],16),Integer.parseInt(p[3]),p[1],false);
                        if(!stub.equals(target)) stub.setThunkedFunction(target);
                    }
                }
            }
        }
        DecompInterface di=new DecompInterface();
        try {
            if(!di.openProgram(currentProgram)) throw new IllegalStateException("Open failed");
            for(int i=0;i<targets.size();i++) {
                Function f=targets.get(i); String id=report.get(i).get("offset").toString();
                StringBuilder asm=new StringBuilder();
                InstructionIterator iter=currentProgram.getListing().getInstructions(f.getBody(),true);
                while(iter.hasNext()) {Instruction ins=iter.next();asm.append(ins.getAddress()).append("  ").append(ins).append("\n");}
                Files.writeString(output.resolve(id+".asm.txt"),asm.toString());
                DecompileResults dr=di.decompileFunction(f,90,monitor); report.get(i).put("decompiled",dr.decompileCompleted());report.get(i).put("message",dr.getErrorMessage());
                if(dr.decompileCompleted() && dr.getDecompiledFunction()!=null) Files.writeString(output.resolve(id+".c"),dr.getDecompiledFunction().getC());
                println("Processed "+id+" "+f.getName());
            }
        } finally {di.dispose();}
        Files.writeString(output.resolve("report.json"),new GsonBuilder().setPrettyPrinting().create().toJson(report));
    }
}
