import ghidra.app.script.GhidraScript;
import ghidra.framework.options.Options;
import ghidra.program.model.listing.Program;

public class LightAnalysis extends GhidraScript
{
    private static final String[] DISABLE = {
        "Decompiler Parameter ID",
        "Stack",
        "Decompiler Switch Analysis",
        "Constant Propagation",
        "Demangler",
        "Create Address Tables",
        "Data Type Applier",
        "Function Start Search",
        "Non-Returning Discovered Functions",
        "Shared Routine Calls",
        "Aggressive Instruction Finder"
    };

    @Override
    public void run() throws Exception
    {
        Options options = currentProgram.getOptions(Program.ANALYSIS_PROPERTIES);
        for (String option : DISABLE)
        {
            try
            {
                options.setBoolean(option, false);
                println("disabled: " + option);
            }
            catch (Exception e)
            {
                println("skip (unknown option): " + option);
            }
        }
        try { options.setBoolean("ASCII Strings", true); println("enabled: ASCII Strings"); }
        catch (Exception e) { println("skip: ASCII Strings"); }
        try { options.setBoolean("Reference", true); println("enabled: Reference"); }
        catch (Exception e) { println("skip: Reference"); }
        println("LIGHT ANALYSIS PROFILE APPLIED");
    }
}
