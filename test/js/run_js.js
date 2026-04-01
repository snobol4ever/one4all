#!/usr/bin/env node
'use strict';
/*
 * run_js.js — Node.js runner shim for scrip-cc -js output
 *
 * Usage:
 *   node test/js/run_js.js <compiled.js>
 *   node test/js/run_js.js <compiled.js> [args...]
 *
 * The compiled .js file is produced by:
 *   scrip-cc -js input.sno -o output.js
 *
 * Sprint: SJ-2  Milestone: M-SJ-A01
 */

const path = require('path');
const fs   = require('fs');

const [,, src, ...rest] = process.argv;

if (!src) {
    process.stderr.write('usage: run_js.js <compiled.js>\n');
    process.exit(1);
}

const resolved = path.resolve(src);
if (!fs.existsSync(resolved)) {
    process.stderr.write('run_js.js: file not found: ' + resolved + '\n');
    process.exit(1);
}

/* Run the compiled program */
try {
    require(resolved);
} catch (e) {
    process.stderr.write('run_js.js: runtime error: ' + e.message + '\n');
    if (process.env.SNO_TRACE) process.stderr.write(e.stack + '\n');
    process.exit(1);
}
