import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class FindCryptoVerdict extends GhidraScript
{
    private static final String[] NEEDLES = {
        "bEnablePakSigning",
        "PakSigningRequired",
        "PakEncryptSettings",
        "PakEncryptionRequired",
        "bEnablePakIndexEncryption",
        "EncryptionKey",
        "bDataCryptoRequired"
    };

    @Override
    public void run() throws Exception
    {
        Memory memory = currentProgram.getMemory();
        for (String needle : NEEDLES)
        {
            Address found = null;
            for (MemoryBlock block : memory.getBlocks())
            {
                try
                {
                    Address hit = memory.findBytes(block.getStart(), block.getEnd(),
                        needle.getBytes("US-ASCII"), null, true, monitor);
                    if (hit != null)
                    {
                        found = hit;
                        break;
                    }
                }
                catch (Exception e)
                {
                }
            }
            if (found == null)
            {
                println("NOTFOUND " + needle);
                continue;
            }
            println("STRING " + needle + " @ " + found);
            ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(found);
            int count = 0;
            while (it.hasNext())
            {
                Reference ref = it.next();
                println("  XREF from " + ref.getFromAddress() + " type=" + ref.getReferenceType());
                count++;
                if (count > 40)
                {
                    println("  ... (truncated)");
                    break;
                }
            }
            if (count == 0)
            {
                println("  (no direct xrefs - reflection name, look up by FName index)");
            }
        }
        println("VERDICT_SCAN_DONE");
    }
}
