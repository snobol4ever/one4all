#!/usr/bin/env node
// run_wasm.js — SNOBOL4 WASM runner
// Usage: node run_wasm.js <prog.wasm> [runtime.wasm]
//
// Loads sno_runtime.wasm first, passes its exports as the import object
// for the program module. This means the runtime is compiled once by V8
// (cached) rather than inlined and recompiled in every program binary.
//
// Memory is owned by the runtime module and shared with the program via import.
// main() returns byte-count written to output buffer starting at offset 0.

const fs = require('fs');
const path = require('path');

const progPath = process.argv[2];
const rtPath = process.argv[3] ||
  path.join(__dirname, '../../src/runtime/wasm/sno_runtime.wasm');

if (!progPath) { process.stderr.write('usage: run_wasm.js <prog.wasm>\n'); process.exit(1); }

const rtBytes = fs.readFileSync(rtPath);
const progBytes = fs.readFileSync(progPath);

WebAssembly.instantiate(rtBytes).then(rtResult => {
  const rtExports = rtResult.instance.exports;
  // Program imports everything from the "sno" namespace
  const importObj = { sno: rtExports };
  return WebAssembly.instantiate(progBytes, importObj).then(progResult => {
    const { main } = progResult.instance.exports;
    const len = main();
    // Read output from runtime's memory
    process.stdout.write(Buffer.from(
      new Uint8Array(rtExports.memory.buffer, 0, len)));
  });
}).catch(e => { process.stderr.write(e + '\n'); process.exit(1); });
