import assert from "node:assert/strict";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import test from "node:test";

import {
  adaptCartridgeForWeb,
  adaptCompatibilityCartridgeForNative,
  buildCanonicalEntries,
  buildCartridgeFromNes,
  buildNeoSdFile,
  buildPuzzledpEntries,
  buildGraphics,
  classifyInput,
  crc32,
  extractChr,
  forceTailCrc32,
  parseNeoSdHeader,
  patchTemplateProm,
  validatePackedBcdNgh,
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

function decodeCromTile(crom1, crom2, tileIndex) {
  const tile = new Uint8Array(256);
  let position = tileIndex * 64;
  for (const quadrantOffset of [8, 136, 0, 128]) {
    let offset = quadrantOffset;
    for (let y = 0; y < 8; y += 1) {
      const planes = [
        crom1[position],
        crom1[position + 1],
        crom2[position],
        crom2[position + 1],
      ];
      position += 2;
      for (let x = 0; x < 8; x += 1) {
        tile[offset] = planes.reduce(
          (color, plane, index) =>
            color | (((plane >>> x) & 1) << index),
          0,
        );
        offset += 1;
      }
      offset += 8;
    }
  }
  return tile;
}

function expandAndOrient(tile8, orientation) {
  const output = new Uint8Array(256);
  for (let y = 0; y < 8; y += 1) {
    const sourceY = (orientation & 2) !== 0 ? 7 - y : y;
    for (let x = 0; x < 8; x += 1) {
      const sourceX = (orientation & 1) !== 0 ? 7 - x : x;
      const color = tile8[sourceY * 8 + sourceX];
      const outputOffset = y * 2 * 16 + x * 2;
      output[outputOffset] = color;
      output[outputOffset + 1] = color;
      output[outputOffset + 16] = color;
      output[outputOffset + 17] = color;
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
  assert.ok(graphics.s.slice(0, 32).every((value) => value === 0));
  assert.ok(graphics.s.slice(32, 64).some((value) => value !== 0));
  assert.ok(
    graphics.s
      .slice(513 * 32, 514 * 32)
      .every((value) => value === 0),
  );
  assert.ok(
    graphics.s
      .slice(514 * 32, 515 * 32)
      .some((value) => value !== 0),
  );

  const tile8 = new Uint8Array(64);
  for (let y = 0; y < 8; y += 1) {
    for (let x = 0; x < 8; x += 1) {
      tile8[y * 8 + x] = (x + y * 3) & 3;
    }
  }
  const orientationBases = [257, 769, 1281, 1793];
  for (const [orientation, tileBase] of orientationBases.entries()) {
    assert.deepEqual(
      decodeCromTile(graphics.c1, graphics.c2, tileBase),
      expandAndOrient(tile8, orientation),
      `orientation ${orientation}`,
    );
  }
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

test("a native cartridge receives the web-specific P-ROM", () => {
  const titleOffset = { native: 0x046a, web: 0x086a };
  const title = Uint8Array.from(
    { length: 0x013a },
    (_, index) => (index * 7) & 0xff,
  );
  const nativeProm = patchTemplateProm(
    new Uint8Array(CART_SIZES.p),
    title,
    titleOffset.native,
  );
  const webProm = new Uint8Array(CART_SIZES.p);
  webProm.fill(0xff);
  webProm.fill(0, titleOffset.web, titleOffset.web + title.length);
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
    { "smbneo-web-p1.p1": webProm },
    titleOffset,
  );
  assert.notDeepEqual(adapted.p, nativeProm);
  for (let index = 0; index < title.length; index += 1) {
    assert.equal(adapted.p[titleOffset.web + (index ^ 1)], title[index]);
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

test("NES input becomes an exact canonical full-layout cartridge", () => {
  const template = {
    "smbneo-p1.p1": new Uint8Array(CART_SIZES.p),
    "smbneo-m1.m1": new Uint8Array(CART_SIZES.m),
    "smbneo-v1.v1": new Uint8Array(CART_SIZES.v),
  };
  template["smbneo-p1.p1"].fill(0xff);
  template["smbneo-p1.p1"].fill(0, 0x046a, 0x046a + 0x013a);

  const cartridge = buildCartridgeFromNes(
    syntheticRom(),
    template,
    0x046a,
  );
  for (const [part, size] of Object.entries(CART_SIZES)) {
    assert.equal(cartridge[part].length, size, part);
  }

  const canonical = buildCanonicalEntries(cartridge);
  const expectedCanonical = {
    "smbneo-p1.p1": CART_SIZES.p,
    "smbneo-m1.m1": CART_SIZES.m,
    "smbneo-v1.v1": CART_SIZES.v,
    "smbneo-s1.s1": CART_SIZES.s,
    "smbneo-c1.c1": CART_SIZES.c1,
    "smbneo-c2.c2": CART_SIZES.c2,
  };
  assert.deepEqual(
    Object.keys(canonical).sort(),
    Object.keys(expectedCanonical).sort(),
  );
  for (const [name, size] of Object.entries(expectedCanonical)) {
    assert.equal(canonical[name].length, size, name);
  }
  assert.deepEqual(canonical["smbneo-p1.p1"], cartridge.p);
  assert.notEqual(canonical["smbneo-p1.p1"], cartridge.p);

  const entries = buildPuzzledpEntries(cartridge);
  const expected = {
    "202-p1.bin": [0x080000, 0x2b61415b],
    "202-s1.bin": [0x020000, 0xcd19264f],
    "202-m1.bin": [0x020000, 0x9c0291ea],
    "202-v1.bin": [0x080000, 0xdebeb8fb],
    "202-c1.bin": [0x100000, 0xcc0095ef],
    "202-c2.bin": [0x100000, 0x42371307],
  };
  assert.deepEqual(Object.keys(entries).sort(), Object.keys(expected).sort());
  for (const [name, [size, expectedCrc]] of Object.entries(expected)) {
    assert.equal(entries[name].length, size, name);
    assert.equal(crc32(entries[name]), expectedCrc, name);
  }
});

test("canonical cartridge becomes a valid NeoSD v1 image", () => {
  const cartridge = {
    p: new Uint8Array(CART_SIZES.p),
    m: new Uint8Array(CART_SIZES.m),
    v: new Uint8Array(CART_SIZES.v),
    s: new Uint8Array(CART_SIZES.s),
    c1: new Uint8Array(CART_SIZES.c1),
    c2: new Uint8Array(CART_SIZES.c2),
  };
  cartridge.p.fill(0xff);
  cartridge.p.set([0x4e, 0xf9, 0x00, 0xc0, 0x00, 0x80]);
  cartridge.c1.set([0x01, 0x02, 0x03, 0x04]);
  cartridge.c2.set([0x81, 0x82, 0x83, 0x84]);

  const neo = buildNeoSdFile(cartridge);
  const header = parseNeoSdHeader(neo);
  assert.equal(neo.length, 0x5c1000);
  assert.deepEqual(header.sizes, {
    p: 0x100000,
    s: 0x020000,
    m: 0x020000,
    v1: 0x080000,
    v2: 0,
    c: 0x400000,
  });
  assert.equal(header.name, "Super Mario Bros. Neo");
  assert.equal(header.manufacturer, "Community port");
  assert.equal(header.year, 2026);
  assert.equal(header.genre, 5);
  assert.equal(header.screenshot, 0);
  assert.equal(header.ngh, 0x2026);
  assert.deepEqual(
    [...neo.slice(header.sections.p.start, header.sections.p.start + 6)],
    [0x4e, 0xf9, 0x00, 0xc0, 0x00, 0x80],
  );
  assert.deepEqual(
    [...neo.slice(header.sections.c.start, header.sections.c.start + 8)],
    [0x01, 0x81, 0x02, 0x82, 0x03, 0x83, 0x04, 0x84],
  );
  assert.ok(neo.slice(0x5e, 0x1000).every((value) => value === 0));
});

test("NeoSD NGH metadata must be four-digit packed BCD", () => {
  assert.equal(validatePackedBcdNgh(0x2026), 0x2026);
  assert.throws(() => validatePackedBcdNgh(0x534d), /packed-BCD/);

  const cartridge = {
    p: new Uint8Array(CART_SIZES.p),
    m: new Uint8Array(CART_SIZES.m),
    v: new Uint8Array(CART_SIZES.v),
    s: new Uint8Array(CART_SIZES.s),
    c1: new Uint8Array(CART_SIZES.c1),
    c2: new Uint8Array(CART_SIZES.c2),
  };
  assert.throws(
    () => buildNeoSdFile(cartridge, { ngh: 0x534d }),
    /packed-BCD/,
  );

  const neo = buildNeoSdFile(cartridge);
  new DataView(neo.buffer, neo.byteOffset, neo.byteLength).setUint32(
    0x28,
    0x534d,
    true,
  );
  assert.throws(() => parseNeoSdHeader(neo), /packed-BCD/);
});

test("browser NeoSD output is byte-identical to the native reference packer", async () => {
  const cartridge = {
    p: Uint8Array.from(
      { length: CART_SIZES.p },
      (_, index) => (index * 13 + 1) & 0xff,
    ),
    m: Uint8Array.from(
      { length: CART_SIZES.m },
      (_, index) => (index * 17 + 2) & 0xff,
    ),
    v: Uint8Array.from(
      { length: CART_SIZES.v },
      (_, index) => (index * 19 + 3) & 0xff,
    ),
    s: Uint8Array.from(
      { length: CART_SIZES.s },
      (_, index) => (index * 23 + 4) & 0xff,
    ),
    c1: Uint8Array.from(
      { length: CART_SIZES.c1 },
      (_, index) => (index * 29 + 5) & 0xff,
    ),
    c2: Uint8Array.from(
      { length: CART_SIZES.c2 },
      (_, index) => (index * 31 + 6) & 0xff,
    ),
  };
  const directory = await mkdtemp(join(tmpdir(), "smbneo-neosd-"));
  try {
    const names = {
      p: "smbneo-p1.p1",
      s: "smbneo-s1.s1",
      m: "smbneo-m1.m1",
      v: "smbneo-v1.v1",
      c1: "smbneo-c1.c1",
      c2: "smbneo-c2.c2",
    };
    for (const [part, name] of Object.entries(names)) {
      await writeFile(join(directory, name), cartridge[part]);
    }
    const nativeOutput = join(directory, "native.neo");
    const builder = fileURLToPath(
      new URL("../tools/build_neosd.py", import.meta.url),
    );
    const completed = spawnSync(
      "python3",
      [builder, "--rom-dir", directory, "--output", nativeOutput],
      { encoding: "utf8" },
    );
    assert.equal(completed.status, 0, completed.stderr);
    const native = new Uint8Array(await readFile(nativeOutput));
    assert.deepEqual(buildNeoSdFile(cartridge), native);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("NeoSD metadata and structural damage are rejected", () => {
  const cartridge = {
    p: new Uint8Array(CART_SIZES.p),
    m: new Uint8Array(CART_SIZES.m),
    v: new Uint8Array(CART_SIZES.v),
    s: new Uint8Array(CART_SIZES.s),
    c1: new Uint8Array(CART_SIZES.c1),
    c2: new Uint8Array(CART_SIZES.c2),
  };
  assert.throws(
    () => buildNeoSdFile(cartridge, { name: "x".repeat(33) }),
    /limit is 32/,
  );
  assert.throws(
    () => buildNeoSdFile(cartridge, { manufacturer: "Mário" }),
    /ASCII/,
  );
  assert.throws(
    () => buildNeoSdFile(cartridge, { manufacturer: "x".repeat(17) }),
    /limit is 16/,
  );

  const neo = buildNeoSdFile(cartridge);
  assert.throws(() => parseNeoSdHeader(neo.slice(0, -1)), /header describes/);
  const reserved = neo.slice();
  reserved[0x100] = 1;
  assert.throws(() => parseNeoSdHeader(reserved), /reserved header/);

  const unaligned = new Uint8Array(4096 + 35);
  unaligned.set(neo.slice(0, 4096));
  const unalignedView = new DataView(unaligned.buffer);
  [3, 5, 7, 9, 0, 11].forEach((size, index) => {
    unalignedView.setUint32(0x04 + index * 4, size, true);
  });
  assert.deepEqual(parseNeoSdHeader(unaligned).sizes, {
    p: 3,
    s: 5,
    m: 7,
    v1: 9,
    v2: 0,
    c: 11,
  });
});

test("compatibility upload can be restored onto the native P-ROM template", () => {
  const offsets = { native: 0x046a, web: 0x086a };
  const title = Uint8Array.from(
    { length: 0x013a },
    (_, index) => (index * 11 + 5) & 0xff,
  );
  const compatibility = {
    p: patchTemplateProm(new Uint8Array(CART_SIZES.p), title, offsets.web),
    m: new Uint8Array(CART_SIZES.m),
    v: new Uint8Array(CART_SIZES.v),
    s: new Uint8Array(CART_SIZES.s),
    c1: new Uint8Array(CART_SIZES.c1),
    c2: new Uint8Array(CART_SIZES.c2),
  };
  compatibility.p.fill(0xff, 0x10000);
  compatibility.p.set(
    patchTemplateProm(
      new Uint8Array(CART_SIZES.p),
      title,
      offsets.web,
    ).slice(0, 0x10000),
  );
  const nativeTemplate = new Uint8Array(CART_SIZES.p);
  nativeTemplate.fill(0xff);
  nativeTemplate.fill(0, offsets.native, offsets.native + title.length);

  const native = adaptCompatibilityCartridgeForNative(
    compatibility,
    { "smbneo-p1.p1": nativeTemplate },
    offsets,
  );
  for (let index = 0; index < title.length; index += 1) {
    assert.equal(native.p[offsets.native + (index ^ 1)], title[index]);
  }
});

test("NES input also converts to the exact optional puzzledp set", () => {
  const template = {
    "smbneo-p1.p1": new Uint8Array(CART_SIZES.p),
    "smbneo-m1.m1": new Uint8Array(CART_SIZES.m),
    "smbneo-v1.v1": new Uint8Array(CART_SIZES.v),
  };
  template["smbneo-p1.p1"].fill(0xff);
  template["smbneo-p1.p1"].fill(0, 0x046a, 0x046a + 0x013a);
  const cartridge = buildCartridgeFromNes(
    syntheticRom(),
    template,
    0x046a,
  );
  const entries = buildPuzzledpEntries(cartridge);
  const expected = {
    "202-p1.bin": [0x080000, 0x2b61415b],
    "202-s1.bin": [0x020000, 0xcd19264f],
    "202-m1.bin": [0x020000, 0x9c0291ea],
    "202-v1.bin": [0x080000, 0xdebeb8fb],
    "202-c1.bin": [0x100000, 0xcc0095ef],
    "202-c2.bin": [0x100000, 0x42371307],
  };
  assert.deepEqual(Object.keys(entries).sort(), Object.keys(expected).sort());
  for (const [name, [size, expectedCrc]] of Object.entries(expected)) {
    assert.equal(entries[name].length, size, name);
    assert.equal(crc32(entries[name]), expectedCrc, name);
  }
});

test("puzzledp P-ROM keeps address-zero word-swap loading semantics", () => {
  const cartridge = {
    p: new Uint8Array(CART_SIZES.p),
    m: new Uint8Array(CART_SIZES.m),
    v: new Uint8Array(CART_SIZES.v),
    s: new Uint8Array(CART_SIZES.s),
    c1: new Uint8Array(CART_SIZES.c1),
    c2: new Uint8Array(CART_SIZES.c2),
  };
  cartridge.p.fill(0xff);
  cartridge.p.set([0x12, 0x34, 0x56, 0x78], 0);

  const pRom = buildPuzzledpEntries(cartridge)["202-p1.bin"];
  assert.deepEqual([...pRom.slice(0, 4)], [0x12, 0x34, 0x56, 0x78]);
  assert.deepEqual(
    [pRom[1], pRom[0], pRom[3], pRom[2]],
    [0x34, 0x12, 0x78, 0x56],
  );
});

test("puzzledp conversion rejects live bytes in omitted or CRC padding", () => {
  const cartridge = {
    p: new Uint8Array(CART_SIZES.p),
    m: new Uint8Array(CART_SIZES.m),
    v: new Uint8Array(CART_SIZES.v),
    s: new Uint8Array(CART_SIZES.s),
    c1: new Uint8Array(CART_SIZES.c1),
    c2: new Uint8Array(CART_SIZES.c2),
  };
  cartridge.p.fill(0xff);

  const omittedLive = { ...cartridge, c1: cartridge.c1.slice() };
  omittedLive.c1[0x100000] = 0x41;
  assert.throws(
    () => buildPuzzledpEntries(omittedLive),
    /cannot omit byte 100000/,
  );

  const tailLive = { ...cartridge, v: cartridge.v.slice() };
  tailLive.v[tailLive.v.length - 16] = 0x41;
  assert.throws(
    () => buildPuzzledpEntries(tailLive),
    /safe zero\/FF padding run/,
  );
});

test("a generated puzzledp set can be uploaded and rebuilt byte-for-byte", () => {
  const cartridge = {
    p: new Uint8Array(CART_SIZES.p),
    m: new Uint8Array(CART_SIZES.m),
    v: new Uint8Array(CART_SIZES.v),
    s: new Uint8Array(CART_SIZES.s),
    c1: new Uint8Array(CART_SIZES.c1),
    c2: new Uint8Array(CART_SIZES.c2),
  };
  cartridge.p.fill(0xff);
  for (let index = 0; index < 4096; index += 1) {
    for (const [partIndex, part] of ["p", "m", "v", "s", "c1", "c2"].entries()) {
      cartridge[part][index] = (index * 13 + partIndex * 37) & 0xff;
    }
  }
  const entries = buildPuzzledpEntries(cartridge);
  const zipSignature = Uint8Array.from([0x50, 0x4b, 0x03, 0x04]);
  const source = classifyInput(zipSignature, () => entries);
  assert.equal(source.kind, "cartridge");
  assert.equal(source.profile, "compatibility");

  const rebuilt = buildPuzzledpEntries(source.cartridge);
  assert.deepEqual(Object.keys(rebuilt).sort(), Object.keys(entries).sort());
  for (const name of Object.keys(entries)) {
    assert.deepEqual(rebuilt[name], entries[name], name);
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
  assert.match(playerSource, /EJS_gameName = "puzzledp"/);
  assert.match(
    playerSource,
    /mtime: ZIP_TIMESTAMP/,
  );
  assert.match(playerSource, /EJS_biosUrl = config\.bios\.path/);
  assert.match(playerSource, /EJS_dontExtractBIOS = true/);
  assert.match(playerSource, /zipEntries\(fbneoEntries\)/);
  assert.match(playerSource, /buildCanonicalEntries\(cartridge\)/);
  assert.match(playerSource, /buildNeoSdFile\(cartridge\)/);
  assert.match(playerSource, /config\.downloads\.canonical\.filename/);
  assert.match(playerSource, /config\.downloads\.neosd\.filename/);
  assert.match(playerSource, /config\.downloads\.compatibility\.filename/);
  assert.match(playerSource, /downloadArchive\("canonical"\)/);
  assert.match(playerSource, /downloadArchive\("neosd"\)/);
  assert.match(playerSource, /downloadArchive\("compatibility"\)/);
  assert.doesNotMatch(playerSource, /\.\.\.biosEntries/);

  const pageSource = await readFile(
    new URL("../web/index.html", import.meta.url),
    "utf8",
  );
  assert.match(pageSource, /id="download-neosd"/);
  assert.match(pageSource, /Download smbneo\.neo/);
});
