#!/usr/bin/env node
// run_wasm_pl.js — Prolog WASM runner
// Usage: node run_wasm_pl.js <prog.wasm> [pl_runtime.wasm]
const fs = require('fs');
const path = require('path');
const progPath = process.argv[2];
const rtPath = process.argv[3] ||
  path.join(__dirname, '../../src/runtime/wasm/pl_runtime.wasm');
if (!progPath) { process.stderr.write('usage: run_wasm_pl.js <prog.wasm>\n'); process.exit(1); }
const rtBytes = fs.readFileSync(rtPath);
const progBytes = fs.readFileSync(progPath);
WebAssembly.instantiate(rtBytes).then(rtResult => {
  const rtExports = rtResult.instance.exports;
  const importObj = { pl: rtExports };
  return WebAssembly.instantiate(progBytes, importObj).then(progResult => {
    const { main } = progResult.instance.exports;
    const len = main();
    process.stdout.write(Buffer.from(new Uint8Array(rtExports.memory.buffer, 0, len)));
  });
}).catch(e => { process.stderr.write(e + '\n'); process.exit(1); });
