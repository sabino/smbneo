import {
  EXPECTED_NES_SHA1,
  adaptCartridgeForWeb,
  adaptCompatibilityCartridgeForNative,
  adaptVsCartridgeForWeb,
  buildCanonicalEntries,
  buildCartridgeFromNes,
  buildNeoSdFile,
  buildPuzzledpEntries,
  buildVsCanonicalEntries,
  buildVsCartridgeFromSource,
  buildVsNeoSdFile,
  classifyInput,
  formatBytes,
  sha1Hex,
} from "./compat.mjs";

const EMULATORJS_DATA = "https://cdn.emulatorjs.org/4.2.3/data/";
const EMULATORJS_LOADER = `${EMULATORJS_DATA}loader.js`;
const ZIP_TIMESTAMP = new Date(2026, 0, 1, 0, 0, 0);

const EDITION_UI = Object.freeze({
  home: Object.freeze({
    fileAccept: ".nes,.zip,application/zip,application/x-nes-rom",
    launcherTitle: "Home edition",
    launcherCopy:
      "Choose your supported Super Mario Bros. (World) game image.",
    waitingStatus: "Waiting for a local .nes file or ZIP.",
    startButton: "Play SMBNeo",
  }),
  vs: Object.freeze({
    fileAccept: ".zip,application/zip",
    launcherTitle: "VS Arcade Edition",
    launcherCopy:
      "Choose your canonical MAME suprmrio.zip set (seven verified chips).",
    waitingStatus: "Waiting for your local suprmrio.zip.",
    startButton: "Play VS Arcade Edition",
  }),
});

const fileInput = document.querySelector("#game-file");
const fileButton = document.querySelector(".file-button");
const editionInputs = [...document.querySelectorAll('input[name="edition"]')];
const launcherTitle = document.querySelector("#launcher-title");
const launcherCopy = document.querySelector("#launcher-copy");
const status = document.querySelector("#load-status");
const gameWrap = document.querySelector("#game-wrap");
const launcher = document.querySelector("#launcher");
const game = document.querySelector("#game");
const downloads = document.querySelector("#downloads");
const downloadsTitle = document.querySelector("#downloads-title");
const canonicalDownload = document.querySelector("#download-canonical");
const neoSdDownload = document.querySelector("#download-neosd");
const compatibilityDownload = document.querySelector("#download-compatible");

let downloadArchives = {};

const playerState = {
  phase: "waiting",
  edition: "home",
  inputKind: null,
  controls: {
    up: "up arrow",
    down: "down arrow",
    left: "left arrow",
    right: "right arrow",
    jump: ["a", "s"],
    run: ["q", "w"],
    start: "1",
    select: "2",
  },
  product: {
    shortname: "smbneo",
    title: "Super Mario Bros. Neo",
  },
  launchIdentity: "puzzledp",
};
window.__smbneoPlayerState = playerState;

function selectedEdition() {
  return editionInputs.find((input) => input.checked)?.value ?? "home";
}

function setStatus(message, state = "") {
  status.textContent = message;
  if (state) {
    status.dataset.state = state;
  } else {
    delete status.dataset.state;
  }
  playerState.phase = state || "waiting";
}

function setBusy(busy) {
  fileInput.disabled = busy;
  for (const input of editionInputs) {
    input.disabled = busy;
  }
  fileButton.setAttribute("aria-disabled", busy ? "true" : "false");
}

function clearDownloads() {
  downloadArchives = {};
  downloads.hidden = true;
  canonicalDownload.disabled = true;
  neoSdDownload.disabled = true;
  compatibilityDownload.disabled = true;
  delete playerState.downloads;
}

function applyEditionUi() {
  const edition = selectedEdition();
  const ui = EDITION_UI[edition];
  playerState.edition = edition;
  fileInput.accept = ui.fileAccept;
  fileInput.value = "";
  launcherTitle.textContent = ui.launcherTitle;
  launcherCopy.textContent = ui.launcherCopy;
  clearDownloads();
  setStatus(ui.waitingStatus);
}

for (const input of editionInputs) {
  input.addEventListener("change", applyEditionUi);
}

async function digestHex(algorithm, bytes) {
  const digest = await crypto.subtle.digest(algorithm, bytes);
  return [...new Uint8Array(digest)]
    .map((value) => value.toString(16).padStart(2, "0"))
    .join("");
}

async function fetchVerifiedBytes(path, expectedSha256, label) {
  const url = new URL(path, document.baseURI);
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`${label} could not be loaded (${response.status})`);
  }
  const bytes = new Uint8Array(await response.arrayBuffer());
  const actualSha256 = await digestHex("SHA-256", bytes);
  if (actualSha256 !== expectedSha256) {
    throw new Error(`${label} failed its integrity check`);
  }
  return bytes;
}

function validVsOffsets(offsets) {
  return ["native", "web"].every((profile) =>
    ["prg", "chr", "palette"].every((field) =>
      Number.isInteger(offsets?.[profile]?.[field]),
    ),
  );
}

async function loadConfig() {
  const response = await fetch(
    new URL("build-manifest.json", document.baseURI),
    { cache: "no-store" },
  );
  if (!response.ok) {
    throw new Error(`player configuration could not be loaded (${response.status})`);
  }
  const config = await response.json();
  const home = config.editions?.home;
  const vs = config.editions?.vs;
  if (
    config.project !== "SMBNeo" ||
    config.product?.shortname !== "smbneo" ||
    config.product?.title !== "Super Mario Bros. Neo" ||
    config.fbneo_driver !== "puzzledp" ||
    home?.downloads?.canonical?.filename !== "smbneo.zip" ||
    home?.downloads?.neosd?.filename !== "smbneo.neo" ||
    home?.downloads?.compatibility?.filename !== "puzzledp.zip" ||
    !Number.isInteger(home?.patch_offsets?.native) ||
    !Number.isInteger(home?.patch_offsets?.web) ||
    vs?.downloads?.canonical?.filename !== "vssmbneo.zip" ||
    vs?.downloads?.neosd?.filename !== "vssmbneo.neo" ||
    vs?.downloads?.compatibility?.filename !== "puzzledp.zip" ||
    vs?.source_chips !== 7 ||
    !validVsOffsets(vs?.patch_offsets)
  ) {
    throw new Error("player configuration is not compatible with this build");
  }
  return config;
}

function downloadArchive(kind) {
  const archive = downloadArchives[kind];
  if (archive === undefined) {
    return;
  }
  const url = URL.createObjectURL(
    new Blob([archive.bytes], { type: archive.mimeType }),
  );
  const link = document.createElement("a");
  link.href = url;
  link.download = archive.filename;
  link.hidden = true;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 0);
}

canonicalDownload.addEventListener("click", () => downloadArchive("canonical"));
neoSdDownload.addEventListener("click", () => downloadArchive("neosd"));
compatibilityDownload.addEventListener(
  "click",
  () => downloadArchive("compatibility"),
);

function enableDownloads(
  canonicalArchive,
  neoSdImage,
  compatibilityArchive,
  editionConfig,
) {
  const editionDownloads = editionConfig.downloads;
  downloadArchives = {
    canonical: {
      filename: editionDownloads.canonical.filename,
      bytes: canonicalArchive,
      mimeType: "application/zip",
    },
    neosd: {
      filename: editionDownloads.neosd.filename,
      bytes: neoSdImage,
      mimeType: "application/octet-stream",
    },
    compatibility: {
      filename: editionDownloads.compatibility.filename,
      bytes: compatibilityArchive,
      mimeType: "application/zip",
    },
  };
  canonicalDownload.textContent =
    `Download ${editionDownloads.canonical.filename}`;
  neoSdDownload.textContent = `Download ${editionDownloads.neosd.filename}`;
  compatibilityDownload.textContent =
    `Download ${editionDownloads.compatibility.filename}`;
  canonicalDownload.disabled = false;
  neoSdDownload.disabled = false;
  compatibilityDownload.disabled = false;
  downloadsTitle.textContent = `${editionConfig.title} is ready`;
  downloads.hidden = false;
  playerState.downloads = Object.fromEntries(
    Object.entries(downloadArchives).map(([kind, archive]) => [
      kind,
      { filename: archive.filename, bytes: archive.bytes.length },
    ]),
  );
}

function zipEntries(entries) {
  return new Promise((resolve, reject) => {
    window.fflate.zip(
      entries,
      { level: 9, mtime: ZIP_TIMESTAMP },
      (error, archive) => {
        if (error) {
          reject(error);
        } else {
          resolve(archive);
        }
      },
    );
  });
}

function installEmulator(gameArchive, config, edition) {
  const gameFile = new File(
    [gameArchive],
    "puzzledp.zip",
    { type: "application/zip" },
  );

  window.EJS_player = "#game";
  window.EJS_core = "fbneo";
  window.EJS_gameName = "puzzledp";
  window.EJS_gameID = 202;
  window.EJS_gameUrl = gameFile;
  /*
   * FBNeo resolves its parent set as /neogeo.zip. EmulatorJS 4.2.3 must
   * receive the root-relative filename plus dontExtractBIOS; an absolute URL
   * or extracted loose members cannot satisfy the arcade parent-set lookup.
   */
  window.EJS_biosUrl = config.bios.path;
  window.EJS_dontExtractBIOS = true;
  window.EJS_pathtodata = EMULATORJS_DATA;
  window.EJS_startOnLoaded = false;
  window.EJS_startButtonName = EDITION_UI[edition].startButton;
  window.EJS_alignStartButton = "center";
  window.EJS_backgroundImage = new URL("title.png", document.baseURI).href;
  window.EJS_backgroundBlur = false;
  window.EJS_backgroundColor = "#050505";
  window.EJS_color = "#e26b31";
  window.EJS_volume = 0.7;
  window.EJS_controlScheme = "arcade";
  window.EJS_defaultControls = {
    0: {
      0: { value: "a", value2: "BUTTON_2" },
      1: { value: "q", value2: "BUTTON_4" },
      2: { value: "2", value2: "SELECT" },
      3: { value: "1", value2: "START" },
      4: { value: "up arrow", value2: "DPAD_UP" },
      5: { value: "down arrow", value2: "DPAD_DOWN" },
      6: { value: "left arrow", value2: "DPAD_LEFT" },
      7: { value: "right arrow", value2: "DPAD_RIGHT" },
      8: { value: "s", value2: "BUTTON_1" },
      9: { value: "w", value2: "BUTTON_3" },
    },
    1: {
      3: { value: "enter", value2: "START" },
    },
    2: {},
    3: {},
  };
  window.EJS_ready = () => {
    playerState.phase = "ready";
    playerState.emulatorReady = true;
  };
  window.EJS_onGameStart = () => {
    playerState.phase = "running";
    playerState.gameStarted = true;
  };

  launcher.hidden = true;
  game.hidden = false;
  gameWrap.classList.add("is-running");

  const loader = document.createElement("script");
  loader.src = EMULATORJS_LOADER;
  loader.addEventListener("error", () => {
    playerState.phase = "error";
    game.hidden = true;
    launcher.hidden = false;
    gameWrap.classList.remove("is-running");
    setBusy(false);
    setStatus("The emulator runtime could not be loaded. Please try again.", "error");
  });
  document.body.append(loader);
}

async function buildHomeCartridge(source, templateEntries, editionConfig) {
  if (source.kind === "vs") {
    throw new Error("choose VS Arcade Edition to use suprmrio.zip");
  }
  if (source.kind === "cartridge") {
    return source.profile === "compatibility"
      ? adaptCompatibilityCartridgeForNative(
          source.cartridge,
          templateEntries,
          editionConfig.patch_offsets,
        )
      : source.cartridge;
  }

  setStatus("Checking the game revision…", "busy");
  const sourceSha1 = await sha1Hex(source.rom);
  if (sourceSha1 !== EXPECTED_NES_SHA1) {
    throw new Error("this is not the supported Super Mario Bros. (World) revision");
  }
  setStatus("Converting Home edition graphics for the Neo Geo…", "busy");
  await new Promise((resolve) => requestAnimationFrame(resolve));
  return buildCartridgeFromNes(
    source.rom,
    templateEntries,
    editionConfig.patch_offsets.native,
  );
}

async function prepareSelectedFile(file, edition) {
  if (window.fflate === undefined) {
    throw new Error("the archive library did not load");
  }

  const config = await loadConfig();
  const editionConfig = config.editions[edition];
  const selectedBytes = new Uint8Array(await file.arrayBuffer());
  const source = classifyInput(selectedBytes, window.fflate.unzipSync);
  playerState.inputKind = source.kind;
  playerState.edition = edition;

  if (edition === "vs" && source.kind !== "vs") {
    throw new Error(
      "VS Arcade Edition requires the canonical seven-chip suprmrio.zip set",
    );
  }

  setStatus(`Loading the ROM-free ${editionConfig.title} template…`, "busy");
  const templateBytes = await fetchVerifiedBytes(
    editionConfig.template.path,
    editionConfig.template.sha256,
    `${editionConfig.title} template`,
  );
  const templateEntries = window.fflate.unzipSync(templateBytes);

  let cartridge;
  let canonicalEntries;
  let neoSdImage;
  let webCartridge;
  if (edition === "vs") {
    setStatus("Converting both VS graphics banks and arcade palette…", "busy");
    await new Promise((resolve) => requestAnimationFrame(resolve));
    cartridge = buildVsCartridgeFromSource(
      source.rom,
      templateEntries,
      editionConfig.patch_offsets.native,
    );
    canonicalEntries = buildVsCanonicalEntries(cartridge);
    neoSdImage = buildVsNeoSdFile(cartridge);
    webCartridge = adaptVsCartridgeForWeb(
      source.rom,
      cartridge,
      templateEntries,
      editionConfig.patch_offsets.web,
    );
  } else {
    cartridge = await buildHomeCartridge(source, templateEntries, editionConfig);
    canonicalEntries = buildCanonicalEntries(cartridge);
    neoSdImage = buildNeoSdFile(cartridge);
    webCartridge = adaptCartridgeForWeb(
      cartridge,
      templateEntries,
      editionConfig.patch_offsets,
    );
  }

  setStatus(`Compressing ${editionConfig.downloads.canonical.filename}…`, "busy");
  await new Promise((resolve) => requestAnimationFrame(resolve));
  const canonicalArchive = await zipEntries(canonicalEntries);

  await fetchVerifiedBytes(
    config.bios.path,
    config.bios.sha256,
    "open Neo Geo BIOS",
  );

  setStatus("Preparing the internal FBNeo compatibility package…", "busy");
  await new Promise((resolve) => requestAnimationFrame(resolve));
  const fbneoEntries = buildPuzzledpEntries(
    webCartridge,
    (completed, total) => {
      if (completed < total) {
        setStatus(
          `Preparing the browser cartridge (${completed + 1}/${total})…`,
          "busy",
        );
      }
    },
  );

  setStatus("Compressing the browser cartridge…", "busy");
  const launchArchive = await zipEntries(fbneoEntries);
  enableDownloads(
    canonicalArchive,
    neoSdImage,
    launchArchive,
    editionConfig,
  );
  playerState.archiveBytes = launchArchive.length;
  playerState.phase = "loading-emulator";
  setStatus(
    `${editionConfig.title} ready (${formatBytes(launchArchive.length)}). ` +
      "Loading the player…",
    "ready",
  );
  installEmulator(launchArchive, config, edition);
}

fileInput.addEventListener("change", async () => {
  const [file] = fileInput.files;
  if (file === undefined) {
    return;
  }

  const edition = selectedEdition();
  setBusy(true);
  setStatus(`Reading ${file.name}…`, "busy");
  try {
    await prepareSelectedFile(file, edition);
  } catch (error) {
    console.error(error);
    setBusy(false);
    fileInput.value = "";
    setStatus(error instanceof Error ? error.message : String(error), "error");
  }
});

applyEditionUi();
