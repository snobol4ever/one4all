// sno_host.mjs — generic WASM host for scrip --sm-emit --target=wasm output
// Usage: node sno_host.mjs <prog.wasm> [<bb_boxes.wasm>] [<sno_runtime.wasm>]
// Default runtime paths relative to this script's directory.
import { readFileSync, createReadStream } from 'fs';
import { createInterface } from 'readline';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const runtimePath   = process.env.SNO_RUNTIME_WASM || join(HERE, 'sno_runtime.wasm');
const bbBoxesPath   = process.env.SNO_BB_BOXES_WASM || join(HERE, 'bb_boxes.wasm');
const progPath      = process.argv[2];

if (!progPath) {
  process.stderr.write('Usage: node sno_host.mjs <prog.wasm>\n');
  process.exit(1);
}

// Read stdin lines for INPUT
const rl = createInterface({ input: process.stdin, terminal: false });
const inputLines = [];
let inputIdx = 0;
rl.on('line', l => inputLines.push(l));

await new Promise(resolve => rl.once('close', resolve));

// Compile runtime and bb_boxes modules
const snoRtBytes  = readFileSync(runtimePath);
const progBytes   = readFileSync(progPath);

// Host imports for sno_runtime
const hostImports = {
  host: {
    write_line: (ptr, len) => {
      const mem = new Uint8Array(snoRtInst.exports.memory.buffer);
      const s = new TextDecoder().decode(mem.slice(ptr, ptr + len));
      process.stdout.write(s + '\n');
    },
    read_line: (buf_ptr) => {
      if (inputIdx >= inputLines.length) return -1;
      const line = inputLines[inputIdx++];
      const mem = new Uint8Array(snoRtInst.exports.memory.buffer);
      const bytes = new TextEncoder().encode(line);
      for (let i = 0; i < bytes.length; i++) mem[buf_ptr + i] = bytes[i];
      return bytes.length;
    },
  }
};

const snoRtMod  = await WebAssembly.instantiate(snoRtBytes, hostImports);
const snoRtInst = snoRtMod.instance;

// Expose sno_runtime exports as "sno" namespace for the program
// Also expose a minimal "bb" namespace (arena_alloc from bb_boxes or a stub)
let arenaAllocFn = () => 0x50000;
try {
  const bbBytes = readFileSync(bbBoxesPath);
  const bbImports = { env: { memory: snoRtInst.exports.memory } };
  const bbMod = await WebAssembly.instantiate(bbBytes, bbImports);
  arenaAllocFn = bbMod.instance.exports.arena_alloc || arenaAllocFn;
} catch (_) {}

const progImports = {
  sno: snoRtInst.exports,
  bb: { arena_alloc: arenaAllocFn },
};

const progMod  = await WebAssembly.instantiate(progBytes, progImports);
const progInst = progMod.instance;
progInst.exports.main();
