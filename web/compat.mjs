export const EXPECTED_NES_SHA1 =
  "ea343f4e445a9050d4b4fbac2c77d0693b1d0922";

/* MAME's canonical suprmrio (SM4-4 E) chip identities.  These are
 * fingerprints only: the browser never ships or uploads their contents. */
export const VS_SOURCE_CHIPS = Object.freeze([
  Object.freeze({
    name: "mds-sm4-4__1dor6d_e.1d or 6d",
    size: 8192,
    crc32: 0xbe4d5436,
    sha1: "08162a7c987f1939d09bebdb676f596c86abf465",
    region: "prg",
    order: 0,
  }),
  Object.freeze({
    name: "mds-sm4-4__1cor6c_e.1c or 6c",
    size: 8192,
    crc32: 0x5e3fb550,
    sha1: "de4494e4dd52f7f7b04cf1d9019fd89fb90eaca9",
    region: "prg",
    order: 1,
  }),
  Object.freeze({
    name: "mds-sm4-4__1bor6b_e.1b or 6b",
    size: 8192,
    crc32: 0xb1b87893,
    sha1: "8563ceaca664cf4495ef1020c07179ca7e4af9f3",
    region: "prg",
    order: 2,
  }),
  Object.freeze({
    name: "mds-sm4-4__1aor6a_e.1a or 6a",
    size: 8192,
    crc32: 0x1abf053c,
    sha1: "f17db88ce0c9bf1ed88dc16b9650f11d10835cec",
    region: "prg",
    order: 3,
  }),
  Object.freeze({
    name: "mds-sm4-4__2bor8b_e.2b or 8b",
    size: 8192,
    crc32: 0x42418d40,
    sha1: "22ab61589742cfa4cc6856f7205d7b4b8310bc4d",
    region: "chr",
    order: 0,
  }),
  Object.freeze({
    name: "mds-sm4-4__2aor8a_e.2a or 8a",
    size: 8192,
    crc32: 0x15506b86,
    sha1: "69ecf7a3cc8bf719c1581ec7c0d68798817d416f",
    region: "chr",
    order: 1,
  }),
  Object.freeze({
    name: "rp2c04-0004.pal",
    size: 192,
    crc32: 0x0c2e8e4d,
    sha1: "0f9090225eb1f08ae5072d40af3e95547cbce05f",
    region: "palette",
    order: 0,
  }),
]);

const NES_CHR_SIZE = 8 * 1024;
const NES_CHR_TILES = 512;
const VS_PRG_SIZE = 32 * 1024;
const VS_CHR_SIZE = 16 * 1024;
const VS_CHR_TILES = 1024;
const VS_PALETTE_SIZE = 192;
const VS_PALETTE_WORD_BYTES = 128;
const TITLE_SCREEN_CHR_OFFSET = 0x1ec0;
const TITLE_SCREEN_CHR_SIZE = 0x013a;

const CROM_TILE_BYTES_PER_CHIP = 64;
const CROM_NES_TILE_BASE = 257;
const CROM_NES_TILE_BANK_SIZE = NES_CHR_TILES;
const CROM_NES_TILE_BANKS = 4;
const CROM_CHIP_SIZE = 2 * 1024 * 1024;
const SROM_TILE_BYTES = 32;
const SROM_NES_TILE_BASE = 1;
const SROM_SOLID_TILE = 514;
const SROM_SIZE = 128 * 1024;

export const VS_CARTRIDGE_ENTRIES = Object.freeze({
  p: "vssmbneo-p1.p1",
  m: "vssmbneo-m1.m1",
  v: "vssmbneo-v1.v1",
  s: "vssmbneo-s1.s1",
  c1: "vssmbneo-c1.c1",
  c2: "vssmbneo-c2.c2",
});

export const VS_WEB_PROM_ENTRY = "vssmbneo-web-p1.p1";

const CART_ENTRIES = Object.freeze({
  p: "smbneo-p1.p1",
  m: "smbneo-m1.m1",
  v: "smbneo-v1.v1",
  s: "smbneo-s1.s1",
  c1: "smbneo-c1.c1",
  c2: "smbneo-c2.c2",
});

const WEB_PROM_ENTRY = "smbneo-web-p1.p1";

const CART_SIZES = Object.freeze({
  p: 0x100000,
  m: 0x020000,
  v: 0x080000,
  s: 0x020000,
  c1: 0x200000,
  c2: 0x200000,
});

const NEOSD_HEADER_SIZE = 4096;
const NEOSD_ALIGN_64K = 64 * 1024;
const NEOSD_ALIGN_CROM = 256 * 1024;
const NEOSD_DEFAULTS = Object.freeze({
  name: "Super Mario Bros. Neo",
  manufacturer: "Community port",
  year: 2026,
  genre: 5,
  screenshot: 0,
  ngh: 0x2026,
});

const VS_NEOSD_DEFAULTS = Object.freeze({
  name: "VS. Super Mario Bros. Neo",
  manufacturer: "Community port",
  year: 2026,
  genre: 5,
  screenshot: 0,
  ngh: 0x2027,
});

const PUZZLEDP_LAYOUT = Object.freeze([
  ["202-p1.bin", 0x080000, 0x2b61415b, "p", 0xff],
  ["202-s1.bin", 0x020000, 0xcd19264f, "s", 0x00],
  ["202-m1.bin", 0x020000, 0x9c0291ea, "m", 0x00],
  ["202-v1.bin", 0x080000, 0xdebeb8fb, "v", 0x00],
  ["202-c1.bin", 0x100000, 0xcc0095ef, "c1", 0x00],
  ["202-c2.bin", 0x100000, 0x42371307, "c2", 0x00],
]);

let crcTable;

function asBytes(value) {
  if (value instanceof Uint8Array) {
    return value;
  }
  if (value instanceof ArrayBuffer) {
    return new Uint8Array(value);
  }
  if (ArrayBuffer.isView(value)) {
    return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  }
  throw new TypeError("expected byte data");
}

function baseName(path) {
  return path.split("/").pop();
}

function entriesByBaseName(entries) {
  const output = new Map();
  for (const [path, value] of Object.entries(entries)) {
    if (path.endsWith("/")) {
      continue;
    }
    const name = baseName(path);
    if (output.has(name)) {
      throw new Error(`archive contains more than one ${name}`);
    }
    output.set(name, asBytes(value));
  }
  return output;
}

function validateVsChipDefinitions(chips) {
  if (!Array.isArray(chips) || chips.length !== 7) {
    throw new Error("VS source profile must describe exactly seven chips");
  }
  const names = new Set();
  const regionOrders = new Set();
  for (const chip of chips) {
    if (
      chip === null ||
      typeof chip !== "object" ||
      typeof chip.name !== "string" ||
      !Number.isInteger(chip.size) ||
      chip.size <= 0 ||
      !Number.isInteger(chip.crc32) ||
      chip.crc32 < 0 ||
      chip.crc32 > 0xffffffff ||
      !/^[0-9a-f]{40}$/u.test(chip.sha1) ||
      !["prg", "chr", "palette"].includes(chip.region) ||
      !Number.isInteger(chip.order) ||
      chip.order < 0
    ) {
      throw new Error("invalid VS source chip fingerprint");
    }
    if (names.has(chip.name)) {
      throw new Error(`duplicate VS source chip definition ${chip.name}`);
    }
    const regionOrder = `${chip.region}:${chip.order}`;
    if (regionOrders.has(regionOrder)) {
      throw new Error(`duplicate VS source chip position ${regionOrder}`);
    }
    names.add(chip.name);
    regionOrders.add(regionOrder);
  }
  return names;
}

function concatenateVsRegion(entries, chips, region) {
  const selected = chips
    .filter((chip) => chip.region === region)
    .sort((left, right) => left.order - right.order);
  const size = selected.reduce((total, chip) => total + chip.size, 0);
  const output = new Uint8Array(size);
  let offset = 0;
  for (const chip of selected) {
    const bytes = entries.get(chip.name);
    output.set(bytes, offset);
    offset += bytes.length;
  }
  return output;
}

function extractVsRomFromEntries(entries, chips) {
  const expectedNames = validateVsChipDefinitions(chips);
  const actualNames = new Set(entries.keys());
  const missing = [...expectedNames].filter((name) => !actualNames.has(name));
  const unexpected = [...actualNames].filter((name) => !expectedNames.has(name));
  if (missing.length !== 0 || unexpected.length !== 0) {
    const details = [];
    if (missing.length !== 0) {
      details.push(`missing ${missing.sort().join(", ")}`);
    }
    if (unexpected.length !== 0) {
      details.push(`unexpected ${unexpected.sort().join(", ")}`);
    }
    throw new Error(`expected canonical suprmrio.zip contents: ${details.join("; ")}`);
  }

  for (const chip of chips) {
    const bytes = entries.get(chip.name);
    if (bytes.length !== chip.size) {
      throw new Error(
        `${chip.name} is ${bytes.length} bytes; expected ${chip.size}`
      );
    }
    const actualCrc = crc32(bytes);
    if (actualCrc !== (chip.crc32 >>> 0)) {
      throw new Error(
        `${chip.name} CRC is ${actualCrc.toString(16).padStart(8, "0")}; ` +
        `expected ${(chip.crc32 >>> 0).toString(16).padStart(8, "0")}`
      );
    }
    const actualSha1 = sha1HexSync(bytes);
    if (actualSha1 !== chip.sha1) {
      throw new Error(
        `${chip.name} SHA-1 is ${actualSha1}; expected ${chip.sha1}`
      );
    }
  }

  const rom = {
    prg: concatenateVsRegion(entries, chips, "prg"),
    chr: concatenateVsRegion(entries, chips, "chr"),
    palette: concatenateVsRegion(entries, chips, "palette"),
  };
  if (
    rom.prg.length !== VS_PRG_SIZE ||
    rom.chr.length !== VS_CHR_SIZE ||
    rom.palette.length !== VS_PALETTE_SIZE
  ) {
    throw new Error(
      `VS source profile assembled ${rom.prg.length} PRG, ` +
      `${rom.chr.length} CHR and ${rom.palette.length} palette bytes`
    );
  }
  return rom;
}

/** Extract and fingerprint a canonical, already-unzipped suprmrio set. */
export function extractVsRom(entries, chips = VS_SOURCE_CHIPS) {
  return extractVsRomFromEntries(entriesByBaseName(entries), chips);
}

function looksLikeZip(bytes) {
  return (
    bytes.length >= 4 &&
    bytes[0] === 0x50 &&
    bytes[1] === 0x4b &&
    (
      (bytes[2] === 0x03 && bytes[3] === 0x04) ||
      (bytes[2] === 0x05 && bytes[3] === 0x06) ||
      (bytes[2] === 0x07 && bytes[3] === 0x08)
    )
  );
}

export function classifyInput(input, unzipSync, options = {}) {
  const bytes = asBytes(input);
  if (!looksLikeZip(bytes)) {
    return { kind: "nes", rom: bytes };
  }

  const entries = entriesByBaseName(unzipSync(bytes));
  const hasCartridge = Object.values(CART_ENTRIES).every((name) =>
    entries.has(name)
  );
  if (hasCartridge) {
    const cartridge = {};
    for (const [part, name] of Object.entries(CART_ENTRIES)) {
      cartridge[part] = entries.get(name);
    }
    validateCartridgeParts(cartridge);
    return { kind: "cartridge", profile: "canonical", cartridge };
  }

  const hasPuzzledpCartridge = PUZZLEDP_LAYOUT.every(([name]) =>
    entries.has(name)
  );
  if (hasPuzzledpCartridge) {
    const cartridge = {};
    for (const [
      name,
      compatibilitySize,
      expectedCrc,
      part,
      paddingByte,
    ] of PUZZLEDP_LAYOUT) {
      const source = entries.get(name);
      if (source.length !== compatibilitySize) {
        throw new Error(
          `${name} is ${source.length} bytes; expected ${compatibilitySize}`
        );
      }
      const actualCrc = crc32(source);
      if (actualCrc !== expectedCrc) {
        throw new Error(
          `${name} CRC is ${actualCrc.toString(16).padStart(8, "0")}; ` +
          `expected ${expectedCrc.toString(16).padStart(8, "0")}`
        );
      }

      const expanded = new Uint8Array(CART_SIZES[part]);
      expanded.fill(paddingByte);
      expanded.set(source);
      /*
       * SMBNeo's compatibility builder owns these four bytes and always uses
       * them for CRC correction. Restore padding before another conversion.
       */
      expanded.fill(
        paddingByte,
        compatibilitySize - 4,
        compatibilitySize
      );
      cartridge[part] = expanded;
    }
    validateCartridgeParts(cartridge);
    return { kind: "cartridge", profile: "compatibility", cartridge };
  }

  const vsChips = options.vsChips ?? VS_SOURCE_CHIPS;
  const vsNames = validateVsChipDefinitions(vsChips);
  const hasVsChip = [...entries.keys()].some((name) => vsNames.has(name));
  if (hasVsChip) {
    return {
      kind: "vs",
      profile: "canonical",
      rom: extractVsRomFromEntries(entries, vsChips),
    };
  }

  const nesEntries = [...entries.entries()].filter(([name]) =>
    name.toLowerCase().endsWith(".nes")
  );
  if (nesEntries.length !== 1) {
    throw new Error(
      `expected one .nes file or an SMBNeo cartridge, found ${nesEntries.length} .nes files`
    );
  }
  return { kind: "nes", rom: nesEntries[0][1] };
}

export async function sha1Hex(input) {
  const bytes = asBytes(input);
  const digest = await globalThis.crypto.subtle.digest("SHA-1", bytes);
  return [...new Uint8Array(digest)]
    .map((value) => value.toString(16).padStart(2, "0"))
    .join("");
}

/* Synchronous SHA-1 keeps ZIP classification deterministic in both browsers
 * and Node without importing a platform-specific crypto package. */
export function sha1HexSync(input) {
  const bytes = asBytes(input);
  const paddedLength = Math.ceil((bytes.length + 9) / 64) * 64;
  const padded = new Uint8Array(paddedLength);
  padded.set(bytes);
  padded[bytes.length] = 0x80;
  const paddedView = new DataView(padded.buffer);
  const bitLength = bytes.length * 8;
  paddedView.setUint32(
    paddedLength - 8,
    Math.floor(bitLength / 0x100000000),
    false,
  );
  paddedView.setUint32(paddedLength - 4, bitLength >>> 0, false);

  let h0 = 0x67452301;
  let h1 = 0xefcdab89;
  let h2 = 0x98badcfe;
  let h3 = 0x10325476;
  let h4 = 0xc3d2e1f0;
  const words = new Uint32Array(80);
  const rotateLeft = (value, count) =>
    ((value << count) | (value >>> (32 - count))) >>> 0;

  for (let block = 0; block < paddedLength; block += 64) {
    for (let index = 0; index < 16; index += 1) {
      words[index] = paddedView.getUint32(block + index * 4, false);
    }
    for (let index = 16; index < 80; index += 1) {
      words[index] = rotateLeft(
        words[index - 3] ^ words[index - 8] ^
        words[index - 14] ^ words[index - 16],
        1,
      );
    }

    let a = h0;
    let b = h1;
    let c = h2;
    let d = h3;
    let e = h4;
    for (let index = 0; index < 80; index += 1) {
      let choose;
      let constant;
      if (index < 20) {
        choose = (b & c) | (~b & d);
        constant = 0x5a827999;
      } else if (index < 40) {
        choose = b ^ c ^ d;
        constant = 0x6ed9eba1;
      } else if (index < 60) {
        choose = (b & c) | (b & d) | (c & d);
        constant = 0x8f1bbcdc;
      } else {
        choose = b ^ c ^ d;
        constant = 0xca62c1d6;
      }
      const next = (
        rotateLeft(a, 5) + choose + e + constant + words[index]
      ) >>> 0;
      e = d;
      d = c;
      c = rotateLeft(b, 30);
      b = a;
      a = next;
    }
    h0 = (h0 + a) >>> 0;
    h1 = (h1 + b) >>> 0;
    h2 = (h2 + c) >>> 0;
    h3 = (h3 + d) >>> 0;
    h4 = (h4 + e) >>> 0;
  }

  return [h0, h1, h2, h3, h4]
    .map((value) => value.toString(16).padStart(8, "0"))
    .join("");
}

export function extractChr(input) {
  const rom = asBytes(input);
  if (
    rom.length < 16 ||
    rom[0] !== 0x4e ||
    rom[1] !== 0x45 ||
    rom[2] !== 0x53 ||
    rom[3] !== 0x1a
  ) {
    throw new Error("the selected file is not an iNES game image");
  }

  const prgBanks = rom[4];
  const chrBanks = rom[5];
  const flags6 = rom[6];
  const flags7 = rom[7];
  const mapper = (flags6 >>> 4) | (flags7 & 0xf0);

  if ((flags7 & 0x0c) === 0x08) {
    throw new Error("NES 2.0 images are not supported");
  }
  if (mapper !== 0 || prgBanks !== 2 || chrBanks !== 1) {
    throw new Error("expected the mapper-0, 32 KiB PRG and 8 KiB CHR revision");
  }
  if ((flags6 & 1) === 0) {
    throw new Error("expected the vertically mirrored cartridge revision");
  }

  const trainerSize = (flags6 & 0x04) !== 0 ? 512 : 0;
  const chrOffset = 16 + trainerSize + prgBanks * 16 * 1024;
  const chrEnd = chrOffset + NES_CHR_SIZE;
  if (rom.length < chrEnd) {
    throw new Error(`the selected image is truncated at ${rom.length} bytes`);
  }
  return rom.slice(chrOffset, chrEnd);
}

function decodeNesTile(chr, tileIndex) {
  const offset = tileIndex * 16;
  const pixels = new Uint8Array(64);
  for (let y = 0; y < 8; y += 1) {
    const plane0 = chr[offset + y];
    const plane1 = chr[offset + y + 8];
    for (let x = 0; x < 8; x += 1) {
      const bit = 7 - x;
      pixels[y * 8 + x] =
        (((plane1 >>> bit) & 1) << 1) | ((plane0 >>> bit) & 1);
    }
  }
  return pixels;
}

function expand2x(tile8) {
  const tile16 = new Uint8Array(256);
  for (let y = 0; y < 8; y += 1) {
    for (let x = 0; x < 8; x += 1) {
      const color = tile8[y * 8 + x];
      const output = y * 2 * 16 + x * 2;
      tile16[output] = color;
      tile16[output + 1] = color;
      tile16[output + 16] = color;
      tile16[output + 17] = color;
    }
  }
  return tile16;
}

function orientTile(tile8, orientation) {
  const output = new Uint8Array(64);
  const horizontal = (orientation & 1) !== 0;
  const vertical = (orientation & 2) !== 0;
  for (let y = 0; y < 8; y += 1) {
    const sourceY = vertical ? 7 - y : y;
    for (let x = 0; x < 8; x += 1) {
      const sourceX = horizontal ? 7 - x : x;
      output[y * 8 + x] = tile8[sourceY * 8 + sourceX];
    }
  }
  return output;
}

function encodeCromTile(tile, crom1, crom2, outputOffset) {
  let position = outputOffset;
  for (const quadrantOffset of [8, 136, 0, 128]) {
    let offset = quadrantOffset;
    for (let y = 0; y < 8; y += 1) {
      const planes = [0, 0, 0, 0];
      for (let x = 0; x < 8; x += 1) {
        const color = tile[offset];
        for (let plane = 0; plane < 4; plane += 1) {
          planes[plane] |= ((color >>> plane) & 1) << x;
        }
        offset += 1;
      }
      crom1[position] = planes[0];
      crom1[position + 1] = planes[1];
      crom2[position] = planes[2];
      crom2[position + 1] = planes[3];
      position += 2;
      offset += 8;
    }
  }
}

function encodeSromTile(tile, srom, outputOffset) {
  let position = outputOffset;
  for (const [pixelA, pixelB] of [[4, 5], [6, 7], [0, 1], [2, 3]]) {
    for (let y = 0; y < 8; y += 1) {
      const a = tile[y * 8 + pixelA] & 0x0f;
      const b = (tile[y * 8 + pixelB] & 0x0f) << 4;
      srom[position] = a | b;
      position += 1;
    }
  }
}

export function buildGraphics(chrInput) {
  const chr = asBytes(chrInput);
  if (chr.length !== NES_CHR_SIZE) {
    throw new Error(`expected ${NES_CHR_SIZE} CHR bytes, found ${chr.length}`);
  }

  const c1 = new Uint8Array(CROM_CHIP_SIZE);
  const c2 = new Uint8Array(CROM_CHIP_SIZE);
  const s = new Uint8Array(SROM_SIZE);
  const tiles = Array.from(
    { length: NES_CHR_TILES },
    (_, tileIndex) => decodeNesTile(chr, tileIndex),
  );

  for (
    let orientation = 0;
    orientation < CROM_NES_TILE_BANKS;
    orientation += 1
  ) {
    for (let tileIndex = 0; tileIndex < NES_CHR_TILES; tileIndex += 1) {
      const tile8 = tiles[tileIndex];
      const cromOffset = (
        CROM_NES_TILE_BASE +
        orientation * CROM_NES_TILE_BANK_SIZE +
        tileIndex
      ) * CROM_TILE_BYTES_PER_CHIP;
      encodeCromTile(
        expand2x(orientTile(tile8, orientation)),
        c1,
        c2,
        cromOffset,
      );
    }
  }

  for (let tileIndex = 0; tileIndex < NES_CHR_TILES; tileIndex += 1) {
    const tile8 = tiles[tileIndex];
    encodeSromTile(
      tile8,
      s,
      (SROM_NES_TILE_BASE + tileIndex) * SROM_TILE_BYTES,
    );
  }

  const solidTile = new Uint8Array(64);
  solidTile.fill(1);
  encodeSromTile(solidTile, s, SROM_SOLID_TILE * SROM_TILE_BYTES);

  return {
    c1,
    c2,
    s,
    title: chr.slice(
      TITLE_SCREEN_CHR_OFFSET,
      TITLE_SCREEN_CHR_OFFSET + TITLE_SCREEN_CHR_SIZE
    ),
  };
}

/** Build the full two-bank VS graphics layout used by gen_vs_assets.py. */
export function buildVsGraphics(chrInput) {
  const chr = asBytes(chrInput);
  if (chr.length !== VS_CHR_SIZE) {
    throw new Error(`expected ${VS_CHR_SIZE} VS CHR bytes, found ${chr.length}`);
  }

  const c1 = new Uint8Array(CROM_CHIP_SIZE);
  const c2 = new Uint8Array(CROM_CHIP_SIZE);
  const s = new Uint8Array(SROM_SIZE);
  const tiles = Array.from(
    { length: VS_CHR_TILES },
    (_, tileIndex) => decodeNesTile(chr, tileIndex),
  );

  for (let orientation = 0; orientation < 4; orientation += 1) {
    for (let tileIndex = 0; tileIndex < VS_CHR_TILES; tileIndex += 1) {
      const cromOffset = (
        CROM_NES_TILE_BASE + orientation * VS_CHR_TILES + tileIndex
      ) * CROM_TILE_BYTES_PER_CHIP;
      encodeCromTile(
        expand2x(orientTile(tiles[tileIndex], orientation)),
        c1,
        c2,
        cromOffset,
      );
    }
  }

  for (let tileIndex = 0; tileIndex < VS_CHR_TILES; tileIndex += 1) {
    encodeSromTile(
      tiles[tileIndex],
      s,
      (SROM_NES_TILE_BASE + tileIndex) * SROM_TILE_BYTES,
    );
  }

  const solidTile = new Uint8Array(64);
  solidTile.fill(1);
  encodeSromTile(solidTile, s, 1026 * SROM_TILE_BYTES);
  return { c1, c2, s };
}

function nearestNeoGeoComponent(value, sharedLow) {
  let best = 0;
  let bestError = Infinity;
  for (let component = 0; component < 32; component += 1) {
    const error = Math.abs(value - (component * 8 + sharedLow * 4));
    if (error < bestError) {
      best = component;
      bestError = error;
    }
  }
  return best;
}

function decodeNeoGeoColor(word) {
  const sharedLow = ((word >>> 15) & 1) ^ 1;
  const red5 = (((word >>> 8) & 0x0f) << 1) | ((word >>> 14) & 1);
  const green5 = (((word >>> 4) & 0x0f) << 1) | ((word >>> 13) & 1);
  const blue5 = ((word & 0x0f) << 1) | ((word >>> 12) & 1);
  return [red5, green5, blue5].map(
    (component) => ((component << 1) | sharedLow) << 2,
  );
}

function encodeNeoGeoColor(rgb) {
  let bestKey;
  let bestWord;
  for (const sharedLow of [0, 1]) {
    const [red5, green5, blue5] = rgb.map(
      (value) => nearestNeoGeoComponent(value, sharedLow),
    );
    const word = (
      ((sharedLow ^ 1) << 15) |
      ((red5 & 1) << 14) |
      ((green5 & 1) << 13) |
      ((blue5 & 1) << 12) |
      ((red5 >>> 1) << 8) |
      ((green5 >>> 1) << 4) |
      (blue5 >>> 1)
    );
    const decoded = decodeNeoGeoColor(word);
    const errors = rgb.map((value, index) =>
      Math.abs(value - decoded[index])
    );
    const key = [
      errors.reduce((total, error) => total + error * error, 0),
      Math.max(...errors),
      word,
    ];
    if (
      bestKey === undefined ||
      key[0] < bestKey[0] ||
      (key[0] === bestKey[0] && key[1] < bestKey[1]) ||
      (key[0] === bestKey[0] && key[1] === bestKey[1] && key[2] < bestKey[2])
    ) {
      bestKey = key;
      bestWord = word;
    }
  }
  return bestWord;
}

/** Convert rp2c04-0004.pal into 64 big-endian uint16_t logical bytes. */
export function convertVsPalette(paletteInput) {
  const palette = asBytes(paletteInput);
  if (palette.length !== VS_PALETTE_SIZE) {
    throw new Error(
      `expected ${VS_PALETTE_SIZE} VS palette bytes, found ${palette.length}`
    );
  }
  const output = new Uint8Array(VS_PALETTE_WORD_BYTES);
  for (let index = 0; index < 64; index += 1) {
    const source = palette.subarray(index * 3, index * 3 + 3);
    if (source.some((value) => value > 0x07)) {
      throw new Error(`VS palette color ${index} is outside the three-bit range`);
    }
    const rgb = [...source].map((value) =>
      (value << 5) | (value << 2) | (value >>> 1)
    );
    const word = encodeNeoGeoColor(rgb);
    output[index * 2] = word >>> 8;
    output[index * 2 + 1] = word & 0xff;
  }
  return output;
}

function validateVsRom(vsRom) {
  if (vsRom === null || typeof vsRom !== "object") {
    throw new TypeError("expected extracted VS ROM data");
  }
  for (const [name, size] of [
    ["prg", VS_PRG_SIZE],
    ["chr", VS_CHR_SIZE],
    ["palette", VS_PALETTE_SIZE],
  ]) {
    if (!(name in vsRom)) {
      throw new Error(`VS source is missing ${name}`);
    }
    const bytes = asBytes(vsRom[name]);
    if (bytes.length !== size) {
      throw new Error(
        `VS ${name} is ${bytes.length} bytes; expected ${size}`
      );
    }
  }
}

function validateWordSwappedRange(prom, payload, offset, label) {
  if (
    !Number.isInteger(offset) ||
    offset < 0 ||
    (offset & 1) !== 0 ||
    (payload.length & 1) !== 0 ||
    offset + payload.length > prom.length
  ) {
    throw new Error(`invalid ${label} word-swapped patch range`);
  }
}

/** Place logical m68k bytes into an already word-swapped physical P-ROM. */
export function patchWordSwappedPayload(
  promInput,
  payloadInput,
  offset,
  label = "payload",
) {
  const prom = asBytes(promInput);
  const payload = asBytes(payloadInput);
  if (prom.length !== CART_SIZES.p) {
    throw new Error(`template P-ROM has unexpected size ${prom.length}`);
  }
  validateWordSwappedRange(prom, payload, offset, label);
  const patched = prom.slice();
  for (let index = 0; index < payload.length; index += 1) {
    patched[offset + (index ^ 1)] = payload[index];
  }
  return patched;
}

/** Patch the three zero-filled VS data symbols in a ROM-free P template. */
export function patchVsTemplateProm(promInput, vsRom, offsets) {
  const prom = asBytes(promInput);
  validateVsRom(vsRom);
  if (prom.length !== CART_SIZES.p) {
    throw new Error(`template P-ROM has unexpected size ${prom.length}`);
  }
  if (offsets === null || typeof offsets !== "object") {
    throw new Error("VS template offsets are missing");
  }
  const payloads = [
    ["prg", asBytes(vsRom.prg), offsets.prg],
    ["chr", asBytes(vsRom.chr), offsets.chr],
    ["palette", convertVsPalette(vsRom.palette), offsets.palette],
  ];
  for (const [name, payload, offset] of payloads) {
    validateWordSwappedRange(prom, payload, offset, `VS ${name}`);
  }
  const ranges = payloads
    .map(([name, payload, offset]) => ({
      name,
      start: offset,
      end: offset + payload.length,
    }))
    .sort((left, right) => left.start - right.start);
  for (let index = 1; index < ranges.length; index += 1) {
    if (ranges[index].start < ranges[index - 1].end) {
      throw new Error(
        `VS ${ranges[index - 1].name} and ${ranges[index].name} patches overlap`
      );
    }
  }
  for (const range of ranges) {
    for (let index = range.start; index < range.end; index += 1) {
      if (prom[index] !== 0) {
        throw new Error(
          `VS ${range.name} placeholder is not zero-filled at ${index}`
        );
      }
    }
  }

  const patched = prom.slice();
  for (const [, payload, offset] of payloads) {
    for (let index = 0; index < payload.length; index += 1) {
      patched[offset + (index ^ 1)] = payload[index];
    }
  }
  return patched;
}

export function patchTemplateProm(promInput, titleInput, titleOffset) {
  const prom = asBytes(promInput);
  const title = asBytes(titleInput);
  if (prom.length !== CART_SIZES.p) {
    throw new Error(`template P-ROM has unexpected size ${prom.length}`);
  }
  if (title.length !== TITLE_SCREEN_CHR_SIZE) {
    throw new Error(`title payload has unexpected size ${title.length}`);
  }
  if (
    !Number.isInteger(titleOffset) ||
    titleOffset < 0 ||
    (titleOffset & 1) !== 0 ||
    titleOffset + title.length > prom.length
  ) {
    throw new Error(`invalid title patch offset ${titleOffset}`);
  }

  const patched = prom.slice();
  for (let index = 0; index < title.length; index += 1) {
    patched[titleOffset + (index ^ 1)] = title[index];
  }
  return patched;
}

export function adaptCartridgeForWeb(
  cartridge,
  templateEntries,
  titleOffset
) {
  validateCartridgeParts(cartridge);
  const entries = entriesByBaseName(templateEntries);
  if (!entries.has(WEB_PROM_ENTRY)) {
    throw new Error(`browser template is missing ${WEB_PROM_ENTRY}`);
  }

  const title = new Uint8Array(TITLE_SCREEN_CHR_SIZE);
  for (let index = 0; index < title.length; index += 1) {
    title[index] = cartridge.p[titleOffset.native + (index ^ 1)];
  }
  return {
    ...cartridge,
    p: patchTemplateProm(
      entries.get(WEB_PROM_ENTRY),
      title,
      titleOffset.web
    ),
  };
}

export function adaptCompatibilityCartridgeForNative(
  cartridge,
  templateEntries,
  titleOffset
) {
  validateCartridgeParts(cartridge);
  const entries = entriesByBaseName(templateEntries);
  if (!entries.has(CART_ENTRIES.p)) {
    throw new Error(`browser template is missing ${CART_ENTRIES.p}`);
  }

  const title = new Uint8Array(TITLE_SCREEN_CHR_SIZE);
  for (let index = 0; index < title.length; index += 1) {
    title[index] = cartridge.p[titleOffset.web + (index ^ 1)];
  }
  return {
    ...cartridge,
    p: patchTemplateProm(
      entries.get(CART_ENTRIES.p),
      title,
      titleOffset.native
    ),
  };
}

export function buildCartridgeFromNes(rom, templateEntries, titleOffset) {
  const entries = entriesByBaseName(templateEntries);
  for (const name of [
    CART_ENTRIES.p,
    CART_ENTRIES.m,
    CART_ENTRIES.v,
  ]) {
    if (!entries.has(name)) {
      throw new Error(`browser template is missing ${name}`);
    }
  }

  const graphics = buildGraphics(extractChr(rom));
  const cartridge = {
    p: patchTemplateProm(
      entries.get(CART_ENTRIES.p),
      graphics.title,
      titleOffset
    ),
    m: entries.get(CART_ENTRIES.m),
    v: entries.get(CART_ENTRIES.v),
    s: graphics.s,
    c1: graphics.c1,
    c2: graphics.c2,
  };
  validateCartridgeParts(cartridge);
  return cartridge;
}

/** Build the authoritative full-layout VS cartridge from local source chips. */
export function buildVsCartridgeFromSource(
  vsRom,
  templateEntries,
  patchOffsets,
) {
  validateVsRom(vsRom);
  const entries = entriesByBaseName(templateEntries);
  for (const name of [
    VS_CARTRIDGE_ENTRIES.p,
    VS_CARTRIDGE_ENTRIES.m,
    VS_CARTRIDGE_ENTRIES.v,
  ]) {
    if (!entries.has(name)) {
      throw new Error(`browser template is missing ${name}`);
    }
  }

  const graphics = buildVsGraphics(vsRom.chr);
  const cartridge = {
    p: patchVsTemplateProm(
      entries.get(VS_CARTRIDGE_ENTRIES.p),
      vsRom,
      patchOffsets,
    ),
    m: entries.get(VS_CARTRIDGE_ENTRIES.m),
    v: entries.get(VS_CARTRIDGE_ENTRIES.v),
    s: graphics.s,
    c1: graphics.c1,
    c2: graphics.c2,
  };
  validateVsCartridgeParts(cartridge);
  return cartridge;
}

/** Replace only the P-ROM with the independently linked FBNeo template. */
export function adaptVsCartridgeForWeb(
  vsRom,
  cartridge,
  templateEntries,
  patchOffsets,
) {
  validateVsRom(vsRom);
  validateVsCartridgeParts(cartridge);
  const entries = entriesByBaseName(templateEntries);
  if (!entries.has(VS_WEB_PROM_ENTRY)) {
    throw new Error(`browser template is missing ${VS_WEB_PROM_ENTRY}`);
  }
  return {
    ...cartridge,
    p: patchVsTemplateProm(
      entries.get(VS_WEB_PROM_ENTRY),
      vsRom,
      patchOffsets,
    ),
  };
}

export function validateCartridgeParts(cartridge) {
  for (const [part, expectedSize] of Object.entries(CART_SIZES)) {
    if (!(part in cartridge)) {
      throw new Error(`cartridge is missing ${CART_ENTRIES[part]}`);
    }
    const bytes = asBytes(cartridge[part]);
    if (bytes.length !== expectedSize) {
      throw new Error(
        `${CART_ENTRIES[part]} is ${bytes.length} bytes; expected ${expectedSize}`
      );
    }
  }
}

export function validateVsCartridgeParts(cartridge) {
  for (const [part, expectedSize] of Object.entries(CART_SIZES)) {
    if (!(part in cartridge)) {
      throw new Error(`VS cartridge is missing ${VS_CARTRIDGE_ENTRIES[part]}`);
    }
    const bytes = asBytes(cartridge[part]);
    if (bytes.length !== expectedSize) {
      throw new Error(
        `${VS_CARTRIDGE_ENTRIES[part]} is ${bytes.length} bytes; ` +
        `expected ${expectedSize}`
      );
    }
  }
}

export function buildCanonicalEntries(cartridge) {
  validateCartridgeParts(cartridge);
  const output = {};
  for (const [part, name] of Object.entries(CART_ENTRIES)) {
    output[name] = asBytes(cartridge[part]).slice();
  }
  return output;
}

export function buildVsCanonicalEntries(cartridge) {
  validateVsCartridgeParts(cartridge);
  const output = {};
  for (const [part, name] of Object.entries(VS_CARTRIDGE_ENTRIES)) {
    output[name] = asBytes(cartridge[part]).slice();
  }
  return output;
}

/** Build a VS-branded NeoSD image while retaining the generic packer API. */
export function buildVsNeoSdFile(cartridge, metadata = {}) {
  validateVsCartridgeParts(cartridge);
  return buildNeoSdFile(cartridge, { ...VS_NEOSD_DEFAULTS, ...metadata });
}

function padToMultiple(input, multiple) {
  const bytes = asBytes(input);
  const remainder = bytes.length % multiple;
  if (remainder === 0) {
    return bytes;
  }
  const output = new Uint8Array(bytes.length + multiple - remainder);
  output.fill(0xff);
  output.set(bytes);
  return output;
}

function requireUint32(value, label) {
  if (!Number.isInteger(value) || value < 0 || value > 0xffffffff) {
    throw new Error(`${label} must be an unsigned 32-bit integer`);
  }
  return value;
}

export function validatePackedBcdNgh(value) {
  const ngh = requireUint32(value, "NGH");
  if (
    ngh > 0xffff ||
    [0, 4, 8, 12].some((shift) => ((ngh >>> shift) & 0xf) > 9)
  ) {
    throw new Error(
      `NGH 0x${ngh.toString(16)} must be a four-digit packed-BCD value`,
    );
  }
  return ngh;
}

function writeAsciiField(output, offset, width, value, label) {
  if (typeof value !== "string") {
    throw new Error(`${label} must be text`);
  }
  const limit = width - 1;
  if (value.length > limit) {
    throw new Error(
      `${label} is ${value.length} bytes; the NeoSD limit is ${limit}`
    );
  }
  for (let index = 0; index < value.length; index += 1) {
    const code = value.charCodeAt(index);
    if (code === 0 || code > 0x7f) {
      throw new Error(`${label} must contain non-NUL ASCII characters only`);
    }
    output[offset + index] = code;
  }
}

function readAsciiField(bytes, offset, width, label) {
  let output = "";
  for (let index = 0; index < width; index += 1) {
    const value = bytes[offset + index];
    if (value === 0) {
      break;
    }
    if (value > 0x7f) {
      throw new Error(`NeoSD ${label} field is not ASCII`);
    }
    output += String.fromCharCode(value);
  }
  return output;
}

function interleaveCrom(c1Input, c2Input) {
  const c1 = asBytes(c1Input);
  const c2 = asBytes(c2Input);
  if (c1.length !== c2.length) {
    throw new Error(
      `C1 and C2 sizes differ: ${c1.length} and ${c2.length} bytes`
    );
  }
  const output = new Uint8Array(c1.length + c2.length);
  for (let index = 0; index < c1.length; index += 1) {
    output[index * 2] = c1[index];
    output[index * 2 + 1] = c2[index];
  }
  return output;
}

/**
 * Pack the canonical native cartridge into the version-1 NeoSD container.
 * The browser uses the native cartridge before applying its FBNeo P-ROM.
 */
export function buildNeoSdFile(cartridge, metadata = {}) {
  validateCartridgeParts(cartridge);
  const resolved = { ...NEOSD_DEFAULTS, ...metadata };

  const regions = {
    p: padToMultiple(cartridge.p, NEOSD_ALIGN_64K),
    s: padToMultiple(cartridge.s, NEOSD_ALIGN_64K),
    m: padToMultiple(cartridge.m, NEOSD_ALIGN_64K),
    v1: padToMultiple(cartridge.v, NEOSD_ALIGN_64K),
    v2: new Uint8Array(),
    c: padToMultiple(
      interleaveCrom(cartridge.c1, cartridge.c2),
      NEOSD_ALIGN_CROM,
    ),
  };
  const ordered = [
    regions.p,
    regions.s,
    regions.m,
    regions.v1,
    regions.v2,
    regions.c,
  ];
  const totalSize = ordered.reduce(
    (total, region) => total + region.length,
    NEOSD_HEADER_SIZE,
  );
  const output = new Uint8Array(totalSize);
  output.set([0x4e, 0x45, 0x4f, 0x01]);
  const view = new DataView(output.buffer);
  ordered.forEach((region, index) => {
    view.setUint32(0x04 + index * 4, region.length, true);
  });
  for (const [index, [label, value]] of [
    ["year", resolved.year],
    ["genre", resolved.genre],
    ["screenshot", resolved.screenshot],
    ["NGH", resolved.ngh],
  ].entries()) {
    const encoded = label === "NGH"
      ? validatePackedBcdNgh(value)
      : requireUint32(value, label);
    view.setUint32(0x1c + index * 4, encoded, true);
  }
  writeAsciiField(output, 0x2c, 33, resolved.name, "game name");
  writeAsciiField(
    output,
    0x4d,
    17,
    resolved.manufacturer,
    "manufacturer",
  );

  let offset = NEOSD_HEADER_SIZE;
  for (const region of ordered) {
    output.set(region, offset);
    offset += region.length;
  }
  return output;
}

export function parseNeoSdHeader(input) {
  const bytes = asBytes(input);
  if (bytes.length < NEOSD_HEADER_SIZE) {
    throw new Error(
      `NeoSD image is ${bytes.length} bytes; header requires ${NEOSD_HEADER_SIZE}`
    );
  }
  if (
    bytes[0] !== 0x4e ||
    bytes[1] !== 0x45 ||
    bytes[2] !== 0x4f ||
    bytes[3] !== 0x01
  ) {
    throw new Error("NeoSD image does not start with NEO version 1 magic");
  }

  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const sizes = {};
  const sectionNames = ["p", "s", "m", "v1", "v2", "c"];
  sectionNames.forEach((name, index) => {
    sizes[name] = view.getUint32(0x04 + index * 4, true);
  });
  const sections = {};
  let offset = NEOSD_HEADER_SIZE;
  for (const name of sectionNames) {
    sections[name] = Object.freeze({
      start: offset,
      end: offset + sizes[name],
    });
    offset += sizes[name];
  }
  if (offset !== bytes.length) {
    throw new Error(
      `NeoSD image has ${bytes.length} bytes; header describes ${offset}`
    );
  }
  for (let index = 0x5e; index < NEOSD_HEADER_SIZE; index += 1) {
    if (bytes[index] !== 0) {
      throw new Error("NeoSD reserved header bytes are not zero-filled");
    }
  }

  const ngh = validatePackedBcdNgh(view.getUint32(0x28, true));
  return Object.freeze({
    sizes: Object.freeze(sizes),
    sections: Object.freeze(sections),
    year: view.getUint32(0x1c, true),
    genre: view.getUint32(0x20, true),
    screenshot: view.getUint32(0x24, true),
    ngh,
    name: readAsciiField(bytes, 0x2c, 33, "game name"),
    manufacturer: readAsciiField(bytes, 0x4d, 17, "manufacturer"),
  });
}

function getCrcTable() {
  if (crcTable !== undefined) {
    return crcTable;
  }
  crcTable = new Uint32Array(256);
  for (let value = 0; value < 256; value += 1) {
    let crc = value;
    for (let bit = 0; bit < 8; bit += 1) {
      crc = (crc & 1) !== 0
        ? (0xedb88320 ^ (crc >>> 1)) >>> 0
        : crc >>> 1;
    }
    crcTable[value] = crc >>> 0;
  }
  return crcTable;
}

function crc32Update(state, input) {
  const bytes = asBytes(input);
  const table = getCrcTable();
  let crc = state >>> 0;
  for (let index = 0; index < bytes.length; index += 1) {
    crc = (table[(crc ^ bytes[index]) & 0xff] ^ (crc >>> 8)) >>> 0;
  }
  return crc >>> 0;
}

export function crc32(input) {
  return (crc32Update(0xffffffff, input) ^ 0xffffffff) >>> 0;
}

function tailIsPadding(bytes, patchOffset) {
  const value = bytes[patchOffset];
  if (value !== 0x00 && value !== 0xff) {
    return false;
  }
  const start = Math.max(0, patchOffset - 60);
  for (let index = start; index < bytes.length; index += 1) {
    if (bytes[index] !== value) {
      return false;
    }
  }
  return true;
}

export function forceTailCrc32(input, desiredCrc) {
  const bytes = asBytes(input).slice();
  if (bytes.length < 64) {
    throw new Error("ROM entry is too small for a safe CRC correction");
  }

  const patchOffset = bytes.length - 4;
  if (!tailIsPadding(bytes, patchOffset)) {
    throw new Error("ROM entry does not end in a safe zero/FF padding run");
  }

  bytes.fill(0, patchOffset);
  const prefixState = crc32Update(
    0xffffffff,
    bytes.subarray(0, patchOffset)
  );
  const patch = new Uint8Array(4);
  const baseCrc =
    (crc32Update(prefixState, patch) ^ 0xffffffff) >>> 0;
  const delta = (desiredCrc ^ baseCrc) >>> 0;
  const columns = new Uint32Array(32);

  for (let bit = 0; bit < 32; bit += 1) {
    patch.fill(0);
    patch[bit >>> 3] = 1 << (bit & 7);
    const value =
      (crc32Update(prefixState, patch) ^ 0xffffffff) >>> 0;
    columns[bit] = (value ^ baseCrc) >>> 0;
  }

  const rows = [];
  for (let outputBit = 0; outputBit < 32; outputBit += 1) {
    let mask = 0;
    for (let column = 0; column < 32; column += 1) {
      if (((columns[column] >>> outputBit) & 1) !== 0) {
        mask = (mask | (1 << column)) >>> 0;
      }
    }
    rows.push({
      mask: mask >>> 0,
      rhs: (delta >>> outputBit) & 1,
    });
  }

  let rank = 0;
  for (let column = 0; column < 32; column += 1) {
    let pivot = -1;
    for (let row = rank; row < 32; row += 1) {
      if (((rows[row].mask >>> column) & 1) !== 0) {
        pivot = row;
        break;
      }
    }
    if (pivot === -1) {
      continue;
    }

    [rows[rank], rows[pivot]] = [rows[pivot], rows[rank]];
    const pivotMask = rows[rank].mask;
    const pivotRhs = rows[rank].rhs;
    for (let row = 0; row < 32; row += 1) {
      if (
        row !== rank &&
        ((rows[row].mask >>> column) & 1) !== 0
      ) {
        rows[row].mask = (rows[row].mask ^ pivotMask) >>> 0;
        rows[row].rhs ^= pivotRhs;
      }
    }
    rank += 1;
  }

  if (rank !== 32) {
    throw new Error("CRC correction matrix is singular");
  }

  let patchValue = 0;
  for (const row of rows) {
    if (row.mask !== 0 && row.rhs !== 0) {
      const pivotBit = (row.mask & -row.mask) >>> 0;
      patchValue = (patchValue | pivotBit) >>> 0;
    }
  }
  new DataView(bytes.buffer, bytes.byteOffset + patchOffset, 4)
    .setUint32(0, patchValue, true);

  const actualCrc = crc32(bytes);
  if (actualCrc !== (desiredCrc >>> 0)) {
    throw new Error(
      `CRC correction failed: expected ${(desiredCrc >>> 0).toString(16)}, ` +
      `found ${actualCrc.toString(16)}`
    );
  }
  return bytes;
}

function compatibilityEntry(sourceInput, size, fillByte, label) {
  const source = asBytes(sourceInput);
  if (source.length < size) {
    throw new Error(
      `${label} source is ${source.length} bytes; target is ${size}`
    );
  }
  for (let offset = size; offset < source.length; offset += 1) {
    if (source[offset] !== fillByte) {
      throw new Error(
        `${label} cannot omit byte ${offset.toString(16)}: expected ` +
        `${fillByte.toString(16).padStart(2, "0")} padding`
      );
    }
  }
  return source.slice(0, size);
}

export function buildPuzzledpEntries(cartridge, onProgress = () => {}) {
  validateCartridgeParts(cartridge);
  const output = {};

  PUZZLEDP_LAYOUT.forEach(
    ([name, size, desiredCrc, sourcePart, fillByte], index) => {
      onProgress(index, PUZZLEDP_LAYOUT.length, name);
      const entry = compatibilityEntry(
        cartridge[sourcePart],
        size,
        fillByte,
        name
      );
      output[name] = forceTailCrc32(entry, desiredCrc);
    }
  );

  onProgress(PUZZLEDP_LAYOUT.length, PUZZLEDP_LAYOUT.length, "complete");
  return output;
}

export function formatBytes(bytes) {
  if (bytes < 1024) {
    return `${bytes} B`;
  }
  if (bytes < 1024 * 1024) {
    return `${(bytes / 1024).toFixed(1)} KiB`;
  }
  return `${(bytes / (1024 * 1024)).toFixed(1)} MiB`;
}
