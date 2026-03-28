/**
 * SnoRuntime — thin shim between generated SNOBOL4 JVM bytecode and the JVM.
 *
 * In standalone mode (harness=false, the default): sno_exit calls System.exit.
 * In harness mode (harness=true, set by SnoHarness): sno_exit throws
 * SnoExitException which SnoHarness catches, allowing the JVM to continue
 * running the next test without restarting.
 *
 * Generated .j files call:
 *   invokestatic SnoRuntime/sno_exit(I)V
 * instead of:
 *   invokestatic java/lang/System/exit(I)V
 *
 * M-G-INV-JVM: single-JVM harness support.
 */
public class SnoRuntime {
    public static volatile boolean harness = false;

    public static class SnoExitException extends RuntimeException {
        public final int code;
        public SnoExitException(int code) { super("exit:" + code); this.code = code; }
    }

    public static void sno_exit(int code) {
        if (harness) throw new SnoExitException(code);
        System.exit(code);
    }
}
