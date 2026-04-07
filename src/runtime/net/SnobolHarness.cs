// SnobolHarness.cs — run multiple scrip-cc-compiled .exe files in one mono process
//
// Usage: mono SnobolHarness.exe <out_dir> <exe1> <exe2> ...
// For each exe: captures stdout to <out_dir>/<basename>.out
// 5-second timeout per program via Thread.
//
// Compile: mcs SnobolHarness.cs -out:SnobolHarness.exe

using System;
using System.IO;
using System.Reflection;
using System.Threading;

class SnobolHarness {
    static int Main(string[] args) {
        if (args.Length < 2) {
            Console.Error.WriteLine("Usage: SnobolHarness <out_dir> <exe1> [exe2 ...]");
            return 1;
        }
        string outDir = args[0];
        Directory.CreateDirectory(outDir);

        for (int i = 1; i < args.Length; i++) {
            string exe = Path.GetFullPath(args[i]);
            string outFile = Path.Combine(outDir,
                Path.GetFileNameWithoutExtension(exe) + ".out");

            var oldOut = Console.Out;
            var oldIn  = Console.In;
            bool timedOut = false;

            using (var sw = new StreamWriter(outFile, false)) {
                Console.SetOut(sw);
                Console.SetIn(new StringReader(""));

                var t = new Thread(() => {
                    try {
                        var asm = Assembly.LoadFrom(exe);
                        var entry = asm.EntryPoint;
                        object[] eargs = (entry.GetParameters().Length > 0)
                            ? new object[]{ new string[]{} }
                            : new object[]{};
                        entry.Invoke(null, eargs);
                    } catch (TargetInvocationException ex) {
                        Console.Error.WriteLine("ERR {0}: {1}",
                            Path.GetFileName(exe),
                            ex.InnerException?.Message ?? ex.Message);
                    } catch (Exception ex) {
                        Console.Error.WriteLine("LOAD {0}: {1}",
                            Path.GetFileName(exe), ex.Message);
                    }
                });
                t.IsBackground = true;
                t.Start();
                if (!t.Join(5000)) {
                    timedOut = true;
                    t.Interrupt();
                }
            }

            Console.SetOut(oldOut);
            Console.SetIn(oldIn);

            if (timedOut)
                Console.Error.WriteLine("TIMEOUT {0}", Path.GetFileName(exe));
        }
        return 0;
    }
}
