// Regenerate golden packets by executing the captured official JS composer.
// Usage: node dump_uploader_vectors.cjs /path/to/meoser.js > uploader_golden.json
const fs = require('fs');
const vm = require('vm');
const crypto = require('crypto');
const source = fs.readFileSync(process.argv[2], 'utf8');
const expected = '7fb25c12b525e4aefea123c46d671badbb42c9a7083ef54e99964034c297cfeb';
if (crypto.createHash('sha256').update(source).digest('hex') !== expected) {
    throw new Error('meoser.js revision differs; review module IDs and protocol first');
}
const sandbox = {console: {log() {}, warn() {}, error() {}}, setTimeout, clearTimeout};
sandbox.window = sandbox;
vm.createContext(sandbox);
vm.runInContext(source.replace('n.m=e,n.c=t,', 'globalThis.meoserRequire=n,n.m=e,n.c=t,'), sandbox);
const composer = sandbox.meoserRequire(67).default;
const vectors = [];
for (const data of [[], [1], [1, 2], [1, 2, 3], Array.from(Buffer.from('print("hello")\n')),
                   Array.from({length: 163}, (_, i) => i % 256)]) {
    const path = '/flash/_xx_ADR.py';
    vectors.push({path, data: Buffer.from(data).toString('hex'),
        frames: composer.composer(path, data.slice(), 80).map(x => Buffer.from(x).toString('hex'))});
}
process.stdout.write(JSON.stringify(vectors, null, 2) + '\n');
