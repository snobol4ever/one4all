#!/usr/bin/env node
// pl_run_wasm.js — Prolog WASM runner
// Usage: node pl_run_wasm.js <prog.wasm> [pl_runtime.wasm]
//
// Loads pl_runtime.wasm first, passes its exports as the "pl" import namespace
// for the Prolog program module.
// main() returns byte-count written to output buffer at offset 0.

const fs   = require('fs');
const path = require('path');

const progPath = process.argv[2];
const rtPath   = process.argv[3] ||
  path.join(__dirname, '../../src/runtime/wasm/pl_runtime.wasm');

if (!progPath) {
  process.stderr.write('usage: pl_run_wasm.js <prog.wasm> [pl_runtime.wasm]\n');
  process.exit(1);
}

const rtBytes   = fs.readFileSync(rtPath);
const progBytes = fs.readFileSync(progPath);

WebAssembly.instantiate(rtBytes).then(rtResult => {
  const rtExports = rtResult.instance.exports;
  // Prolog programs import from "pl" namespace
  const importObj = { pl: rtExports };
  return WebAssembly.instantiate(progBytes, importObj).then(progResult => {
    const { main } = progResult.instance.exports;
    const len = main();
    // Read output from runtime memory starting at offset 0
    process.stdout.write(Buffer.from(
      new Uint8Array(rtExports.memory.buffer, 0, len)));
  });
}).catch(e => { process.stderr.write(e + '\n'); process.exit(1); });
