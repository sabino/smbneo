import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import {
  adaptCartridgeForWeb,
  buildCartridgeFromNes,
  buildFbneoEntries,
  buildGraphics,
  classifyInput,
  crc32,
  extractChr,
  forceTailCrc32,
  patchTemplateProm,
} from "../web/compat.mjs";

const CART_SIZES = {
  p: 0x100000,
  m: 0x020000,
  v: 0x080000,
  s: 0x020000,
  c1: 0x200000,
  c2: 0x200000,
};

function encodeNesTile(pixels) {
  const output = new Uint8Array(16);
  for (let y = 0; y < 8; y += 1) {
    for (let x = 0; x < 8; x += 1) {
      const color = pixels[y * 8 + x];
      const bit = 7 - x;
      output[y] |= (color & 1) << bit;
      output[y + 8] |= ((color >>> 1) & 1) << bit;
    }
  }
  return output;
}

function syntheticRom() {
  const rom = new Uint8Array(16 + 32 * 1024 + 8 * 1024);
  rom.set([0x4e, 0x45, 0x53, 0x1a, 2, 1, 1, 0], 0);
  const chrOffset = 16 + 32 * 1024;
  const tile = new Uint8Array(64);
  for (let y = 0; y < 8; y += 1) {
    for (let x = 0; x < 8; x += 1) {
      tile[y * 8 + x] = (x + y * 3) & 3;
    }
  }
  rom.set(encodeNesTile(tile), chrOffset);
  for (let index = 0; index < 0x013a; index += 1) {
    rom[chrOffset + 0x1ec0 + index] = (index * 29 + 7) & 0xff;
  }
  return rom;
}

test("raw iNES input is recognized and its CHR bank is converted", () => {
  const rom = syntheticRom();
  const source = classifyInput(rom, () => {
    throw new Error("raw input must not be unzipped");
  });
  assert.equal(source.kind, "nes");

  const chr = extractChr(source.rom);
  const graphics = buildGraphics(chr);
  assert.equal(chr.length, 8192);
  assert.equal(graphics.c1.length, CART_SIZES.c1);
  assert.equal(graphics.c2.length, CART_SIZES.c2);
  assert.equal(graphics.s.length, CART_SIZES.s);
  assert.equal(graphics.title.length, 0x013a);
  assert.deepEqual(
    [...graphics.title.slice(0, 4)],
    [7, 36, 65, 94],
  );
  assert.ok(graphics.c1.some((value) => value !== 0));
  assert.ok(graphics.c2.every((value) => value === 0));
  assert.ok(graphics.s.some((value) => value !== 0));
});

test("title payload is placed in the word-swapped P-ROM", () => {
  const prom = new Uint8Array(CART_SIZES.p);
  const title = Uint8Array.from({ length: 0x013a }, (_, index) => index & 0xff);
  const offset = 0x046a;
  const patched = patchTemplateProm(prom, title, offset);

  assert.equal(patched[offset], title[1]);
  assert.equal(patched[offset + 1], title[0]);
  assert.equal(patched[offset + 0x0138], title[0x0139]);
  assert.equal(patched[offset + 0x0139], title[0x0138]);
});

test("a native cartridge receives the FBNeo-specific P-ROM", () => {
  const titleOffset = 0x046a;
  const title = Uint8Array.from(
    { length: 0x013a },
    (_, index) => (index * 7) & 0xff,
  );
  const nativeProm = patchTemplateProm(
    new Uint8Array(CART_SIZES.p),
    title,
    titleOffset,
  );
  const webProm = new Uint8Array(CART_SIZES.p);
  webProm.fill(0xff);
  webProm.fill(0, titleOffset, titleOffset + title.length);
  const cartridge = {
    p: nativeProm,
    m: new Uint8Array(CART_SIZES.m),
    v: new Uint8Array(CART_SIZES.v),
    s: new Uint8Array(CART_SIZES.s),
    c1: new Uint8Array(CART_SIZES.c1),
    c2: new Uint8Array(CART_SIZES.c2),
  };

  const adapted = adaptCartridgeForWeb(
    cartridge,
    { "smbneogeo-p1.p1": webProm },
    titleOffset,
  );
  assert.notDeepEqual(adapted.p, nativeProm);
  for (let index = 0; index < title.length; index += 1) {
    assert.equal(adapted.p[titleOffset + (index ^ 1)], title[index]);
  }
  assert.equal(adapted.c1, cartridge.c1);
  assert.equal(adapted.c2, cartridge.c2);
});

test("tail CRC correction preserves content and reaches the target", () => {
  const input = new Uint8Array(4096);
  for (let index = 0; index < 128; index += 1) {
    input[index] = index;
  }
  const target = 0x59374c47;
  const patched = forceTailCrc32(input, target);
  assert.equal(crc32(patched), target);
  assert.deepEqual(patched.slice(0, 128), input.slice(0, 128));
});

test("NES input becomes a complete FBNeo compatibility set", () => {
  const template = {
    "smbneogeo-p1.p1": new Uint8Array(CART_SIZES.p),
    "smbneogeo-m1.m1": new Uint8Array(CART_SIZES.m),
    "smbneogeo-v1.v1": new Uint8Array(CART_SIZES.v),
  };
  template["smbneogeo-p1.p1"].fill(0xff);
  template["smbneogeo-p1.p1"].fill(0, 0x046a, 0x046a + 0x013a);

  const cartridge = buildCartridgeFromNes(
    syntheticRom(),
    template,
    0x046a,
  );
  for (const [part, size] of Object.entries(CART_SIZES)) {
    assert.equal(cartridge[part].length, size, part);
  }

  const entries = buildFbneoEntries(cartridge);
  const expected = {
    "19yy-p1.p1": [0x200000, 0x59374c47],
    "19yy-s1.s1": [0x020000, 0x219b6f40],
    "19yy-c1.c1": [0x400000, 0x622719d5],
    "19yy-c2.c2": [0x400000, 0x41b07be5],
    "19yy-m1.m1": [0x020000, 0x8e05762a],
    "19yy-v1.v1": [0x800000, 0x944146c2],
    "19yy-v2.v2": [0x800000, 0xa4bafe45],
  };
  for (const [name, [size, expectedCrc]] of Object.entries(expected)) {
    assert.equal(entries[name].length, size, name);
    assert.equal(crc32(entries[name]), expectedCrc, name);
  }
});

test("the browser mapping uses arrow keys for movement", async () => {
  const playerSource = await readFile(
    new URL("../web/player.mjs", import.meta.url),
    "utf8",
  );
  assert.match(playerSource, /value: "up arrow", value2: "DPAD_UP"/);
  assert.match(playerSource, /value: "down arrow", value2: "DPAD_DOWN"/);
  assert.match(playerSource, /value: "left arrow", value2: "DPAD_LEFT"/);
  assert.match(playerSource, /value: "right arrow", value2: "DPAD_RIGHT"/);
  assert.match(playerSource, /value: "a", value2: "BUTTON_2"/);
  assert.match(playerSource, /value: "s", value2: "BUTTON_1"/);
});
