import java.io.*;
import java.net.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;

public class SnoHarness {
    static final int TIMEOUT_MS = 3000;

    public static void main(String[] args) throws Exception {
        if (args.length < 2) { System.err.println("Usage: SnoHarness <classdir> <refdir> [inputdir]"); System.exit(2); }
        File classDir = new File(args[0]).getCanonicalFile();
        File refDir   = new File(args[1]).getCanonicalFile();
        File inputDir = args.length > 2 ? new File(args[2]).getCanonicalFile() : refDir;
        URL[] urls = { classDir.toURI().toURL() };
        SnoRuntime.harness = true;

        List<String> classes = new ArrayList<>();
        for (File f : classDir.listFiles())
            if (f.getName().endsWith(".class") && !f.getName().contains("$")
                    && !f.getName().equals("SnoHarness.class") && !f.getName().equals("SnoRuntime.class"))
                classes.add(f.getName().replace(".class",""));
        Collections.sort(classes);

        int pass=0, fail=0;
        PrintStream origOut=System.out, origErr=System.err;
        InputStream origIn=System.in;

        for (String cls : classes) {
            File ref = new File(refDir, cls+".ref");
            if (!ref.exists()) continue;

            File inf = new File(inputDir, cls+".input");
            final byte[] inputBytes = inf.exists() ? Files.readAllBytes(inf.toPath()) : new byte[0];
            final ByteArrayOutputStream buf = new ByteArrayOutputStream();

            // Run in thread with timeout to handle blocking INPUT reads
            ExecutorService ex = Executors.newSingleThreadExecutor(r -> {
                Thread t = new Thread(r, "sno-"+cls); t.setDaemon(true); return t;
            });

            final String[] result = {"PASS"};
            Future<?> future = ex.submit(() -> {
                System.setOut(new PrintStream(buf, true));
                System.setErr(new PrintStream(new ByteArrayOutputStream()));
                System.setIn(new ByteArrayInputStream(inputBytes));
                try (URLClassLoader loader = new URLClassLoader(urls, SnoHarness.class.getClassLoader())) {
                    loader.loadClass(cls).getMethod("main",String[].class).invoke(null,(Object)new String[0]);
                } catch (InvocationTargetException e) {
                    if (!(e.getCause() instanceof SnoRuntime.SnoExitException)) result[0]="FAIL";
                } catch (Exception e) { result[0]="FAIL"; }
            });

            try { future.get(TIMEOUT_MS, TimeUnit.MILLISECONDS); }
            catch (TimeoutException e) { result[0]="TIMEOUT"; future.cancel(true); }
            catch (ExecutionException e) { result[0]="FAIL"; }
            finally { ex.shutdownNow(); System.setOut(origOut); System.setErr(origErr); System.setIn(origIn); }

            if (result[0].equals("PASS")) {
                String got = buf.toString().trim();
                String exp = new String(Files.readAllBytes(ref.toPath())).trim();
                if (!got.equals(exp)) result[0]="FAIL";
            }
            origOut.println(result[0]+" "+cls);
            if (result[0].equals("PASS")) pass++; else fail++;
        }
        origOut.printf("%nResults: %d passed, %d failed (of %d)%n", pass, fail, pass+fail);
        System.exit(fail>0?1:0);
    }
}
