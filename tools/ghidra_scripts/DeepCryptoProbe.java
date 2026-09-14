import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class DeepCryptoProbe extends GhidraScript
{
    private static final String[] NEEDLES = {
        "bEnablePakSigning",
        "PakSigningRequired",
        "PakEncryptSettings",
        "PakEncryptionRequired",
        "bEnablePakIndexEncryption",
        "EncryptionKey",
        "PakEncryptionKeys",
        "SecondaryEncryptionKeys"
    };

    private java.util.Set<Function> functionsToDecompile = new java.util.LinkedHashSet<>();

    @Override
    public void run() throws Exception
    {
        Memory memory = currentProgram.getMemory();

        for (String needle : NEEDLES)
        {
            Address found = findAscii(memory, needle);
            if (found == null)
            {
                println("NOTFOUND " + needle);
                continue;
            }
            println("STRING " + needle + " @ " + found);
            ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(found);
            int n = 0;
            while (it.hasNext() && n < 16)
            {
                Reference ref = it.next();
                Address from = ref.getFromAddress();
                Function fn = getFunctionContaining(from);
                println("  XREF " + from + " in "
                    + (fn != null ? fn.getName() + " @ " + fn.getEntryPoint() : "<no function>"));
                if (fn != null)
                {
                    functionsToDecompile.add(fn);
                    n++;
                }
            }
            if (n == 0)
            {
                println("  (no direct xrefs)");
            }
        }

        byte[] magic = { (byte) 0xE1, (byte) 0x12, (byte) 0x6F, (byte) 0x5A };
        int magicHits = 0;
        for (MemoryBlock block : memory.getBlocks())
        {
            if (!block.isInitialized() || magicHits >= 24)
            {
                continue;
            }
            Address cursor = block.getStart();
            while (magicHits < 24)
            {
                Address hit;
                try
                {
                    hit = memory.findBytes(cursor, block.getEnd(), magic, null, true, monitor);
                }
                catch (Exception e)
                {
                    break;
                }
                if (hit == null)
                {
                    break;
                }
                println("PAKMAGIC " + hit);
                Function fn = getFunctionContaining(hit);
                if (fn != null)
                {
                    println("  in " + fn.getName() + " @ " + fn.getEntryPoint());
                    functionsToDecompile.add(fn);
                }
                magicHits++;
                cursor = hit.add(1);
            }
        }

        println("functions to decompile: " + functionsToDecompile.size());

        DecompInterface ifc = new DecompInterface();
        ifc.setOptions(new DecompileOptions());
        ifc.openProgram(currentProgram);

        int decompiled = 0;
        for (Function fn : functionsToDecompile)
        {
            if (decompiled >= 12)
            {
                println("[cap reached, stopping decompilation]");
                break;
            }
            try
            {
                DecompileResults res = ifc.decompileFunction(fn, 120, monitor);
                if (res != null && res.getDecompiledFunction() != null)
                {
                    String code = res.getDecompiledFunction().getC();
                    println("===== DECOMPILE " + fn.getName() + " @ " + fn.getEntryPoint());
                    if (code.length() > 9000)
                    {
                        println(code.substring(0, 9000));
                        println("... [truncated]");
                    }
                    else
                    {
                        println(code);
                    }
                    decompiled++;
                }
            }
            catch (Exception e)
            {
                println("decompile failed for " + fn.getName() + ": " + e.getMessage());
            }
        }

        ifc.dispose();
        println("DEEP_PROBE_DONE decompiled=" + decompiled);
    }

    private Address findAscii(Memory memory, String s)
    {
        try
        {
            byte[] bytes = s.getBytes("US-ASCII");
            for (MemoryBlock block : memory.getBlocks())
            {
                if (!block.isInitialized())
                {
                    continue;
                }
                try
                {
                    Address hit = memory.findBytes(block.getStart(), block.getEnd(),
                        bytes, null, true, monitor);
                    if (hit != null)
                    {
                        return hit;
                    }
                }
                catch (Exception e)
                {
                }
            }
        }
        catch (Exception e)
        {
        }
        return null;
    }
}
