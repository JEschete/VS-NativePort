import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import java.io.ByteArrayInputStream;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.MessageDigest;

public class BattleUnitEvidence extends GhidraScript {
    private static final long BASE_ADDRESS = 0x80068800L;
    private static final long[] UNIT_ENTRIES = {
        0x800A141CL, 0x800A152CL, 0x800A1648L, 0x800A1720L,
        0x800A17BCL, 0x800A190CL, 0x800A1AF8L, 0x800A1B28L,
        0x800A1B9CL, 0x800A1C10L, 0x800A1DE8L, 0x800A208CL,
        0x800A249CL, 0x800A2574L, 0x800A25ECL, 0x800A2790L,
        0x800A291CL
    };
    private static final long[] DEPENDENCY_ENTRIES = {
        0x80041C68L, 0x80041598L, 0x80041578L, 0x800417ECL, 0x800B07DCL
    };

    @Override
    public void run() throws Exception {
        if (!currentProgram.getLanguageID().toString().equals("PSX:LE:32:default")) {
            throw new IllegalStateException("The PSX processor extension is required.");
        }
        MemoryBlock retailBlock = currentProgram.getMemory().getBlock(toAddr(BASE_ADDRESS));
        if (retailBlock == null || !retailBlock.getStart().equals(toAddr(BASE_ADDRESS))
            || !retailBlock.isInitialized()) {
            throw new IllegalStateException("Unexpected BATTLE.PRG load address.");
        }
        String[] arguments = getScriptArgs();
        if (arguments.length < 1 || arguments.length > 2) {
            throw new IllegalArgumentException("Expected prepare [main executable] or export.");
        }
        if (arguments[0].equals("prepare")) {
            if (arguments.length == 2) {
                byte[] executable = Files.readAllBytes(Paths.get(arguments[1]));
                StringBuilder hash = new StringBuilder();
                for (byte value : MessageDigest.getInstance("SHA-1").digest(executable)) {
                    hash.append(String.format("%02x", value & 0xFF));
                }
                if (!hash.toString().equals("fababcfd4325d42f350d95b3472874affeb0e48c")) {
                    throw new IllegalStateException("Unexpected main executable SHA-1.");
                }
                MemoryBlock mainBlock = currentProgram.getMemory().createInitializedBlock(
                    "SLUS_010.40", toAddr(0x80010000L),
                    new ByteArrayInputStream(executable, 0x800, executable.length - 0x800),
                    executable.length - 0x800, monitor, false);
                mainBlock.setExecute(true);
                mainBlock.setWrite(true);
                println("DEPENDENCY_BINARY " + arguments[1] + " SHA1=" + hash
                    + " FILE_BASE=0x800 CPU_BASE=0x80010000");
            }
            for (long entry : UNIT_ENTRIES) {
                monitor.checkCancelled();
                Address address = toAddr(entry);
                disassemble(address);
                if (getFunctionAt(address) == null) {
                    createFunction(address, String.format("func_%08X", entry));
                }
                if (getFunctionAt(address) == null) {
                    throw new IllegalStateException("Cannot define function at " + address);
                }
            }
            for (long entry : DEPENDENCY_ENTRIES) {
                monitor.checkCancelled();
                Address address = toAddr(entry);
                disassemble(address);
                if (getFunctionAt(address) == null) {
                    createFunction(address, String.format("func_%08X", entry));
                }
            }
            return;
        }
        if (!arguments[0].equals("export")) {
            throw new IllegalArgumentException("Expected prepare or export.");
        }

        println("BINARY " + currentProgram.getExecutablePath());
        println("SHA256 " + currentProgram.getExecutableSHA256());
        println("LANGUAGE " + currentProgram.getLanguageID());
        println("COMPILER " + currentProgram.getCompilerSpec().getCompilerSpecID());
        println("CALLING_CONVENTION "
            + currentProgram.getCompilerSpec().getDefaultCallingConvention().getName());
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            println("MEMORY " + block.getName() + " " + block.getStart() + ".."
                + block.getEnd() + " initialized=" + block.isInitialized()
                + " read=" + block.isRead() + " write=" + block.isWrite()
                + " execute=" + block.isExecute());
        }

        DecompInterface decompiler = new DecompInterface();
        try {
            if (!decompiler.openProgram(currentProgram)) {
                throw new IllegalStateException(decompiler.getLastMessage());
            }
            long[] evidenceEntries = new long[UNIT_ENTRIES.length + DEPENDENCY_ENTRIES.length];
            System.arraycopy(UNIT_ENTRIES, 0, evidenceEntries, 0, UNIT_ENTRIES.length);
            System.arraycopy(DEPENDENCY_ENTRIES, 0, evidenceEntries, UNIT_ENTRIES.length,
                DEPENDENCY_ENTRIES.length);
            for (long entry : evidenceEntries) {
                monitor.checkCancelled();
                Address address = toAddr(entry);
                Function function = getFunctionAt(address);
                if (function == null) {
                    throw new IllegalStateException("Missing function at " + address);
                }
                boolean mainExecutable = entry < BASE_ADDRESS;
                long fileOffset = mainExecutable ? entry - 0x80010000L + 0x800 : entry - BASE_ADDRESS;
                println(String.format("FUNCTION %s CPU=0x%08X FILE=0x%X BINARY=%s",
                    function.getName(), entry, fileOffset,
                    mainExecutable ? "SLUS_010.40" : "BATTLE.PRG"));
                println("SIGNATURE " + function.getSignature());
                ReferenceIterator references = currentProgram.getReferenceManager()
                    .getReferencesTo(address);
                while (references.hasNext()) {
                    Reference reference = references.next();
                    println("REFERENCE " + reference.getFromAddress() + " "
                        + reference.getReferenceType());
                }
                InstructionIterator instructions = currentProgram.getListing()
                    .getInstructions(function.getBody(), true);
                while (instructions.hasNext()) {
                    Instruction instruction = instructions.next();
                    StringBuilder bytes = new StringBuilder();
                    for (byte value : instruction.getBytes()) {
                        bytes.append(String.format("%02X", value & 0xFF));
                    }
                    println("INSTRUCTION " + instruction.getAddress() + " "
                        + bytes + " " + instruction);
                }
                DecompileResults result = decompiler.decompileFunction(function, 10, monitor);
                if (!result.decompileCompleted() || result.getDecompiledFunction() == null) {
                    throw new IllegalStateException("Decompilation failed: "
                        + result.getErrorMessage());
                }
                for (String line : result.getDecompiledFunction().getC().split("\n")) {
                    println("DECOMPILED " + line);
                }
            }
            println("BATTLE_UNIT_EVIDENCE_COMPLETE");
        }
        finally {
            decompiler.dispose();
        }
    }
}