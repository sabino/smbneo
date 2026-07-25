export const EXPECTED_NES_SHA1 =
  "ea343f4e445a9050d4b4fbac2c77d0693b1d0922";

const NES_CHR_SIZE = 8 * 1024;
const NES_CHR_TILES = 512;
const TITLE_SCREEN_CHR_OFFSET = 0x1ec0;
const TITLE_SCREEN_CHR_SIZE = 0x013a;

const CROM_TILE_BYTES_PER_CHIP = 64;
const CROM_NES_TILE_BASE = 257;
const CROM_CHIP_SIZE = 2 * 1024 * 1024;
const SROM_TILE_BYTES = 32;
const SROM_NES_TILE_BASE = 1;
const SROM_SOLID_TILE = 514;
const SROM_SIZE = 128 * 1024;

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
    const name = baseName(path);
    if (output.has(name)) {
      throw new Error(`archive contains more than one ${name}`);
    }
    output.set(name, asBytes(value));
  }
  return output;
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

export function classifyInput(input, unzipSync) {
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

  for (let tileIndex = 0; tileIndex < NES_CHR_TILES; tileIndex += 1) {
    const tile8 = decodeNesTile(chr, tileIndex);
    const cromOffset =
      (CROM_NES_TILE_BASE + tileIndex) * CROM_TILE_BYTES_PER_CHIP;
    encodeCromTile(expand2x(tile8), c1, c2, cromOffset);
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

export function buildCanonicalEntries(cartridge) {
  validateCartridgeParts(cartridge);
  const output = {};
  for (const [part, name] of Object.entries(CART_ENTRIES)) {
    output[name] = asBytes(cartridge[part]).slice();
  }
  return output;
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
