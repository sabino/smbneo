import {
  EXPECTED_NES_SHA1,
  adaptCartridgeForWeb,
  adaptCompatibilityCartridgeForNative,
  buildCanonicalEntries,
  buildCartridgeFromNes,
  buildPuzzledpEntries,
  classifyInput,
  formatBytes,
  sha1Hex,
} from "./compat.mjs";

const EMULATORJS_DATA =
  "https://cdn.emulatorjs.org/4.2.3/data/";
const EMULATORJS_LOADER = `${EMULATORJS_DATA}loader.js`;
const ZIP_TIMESTAMP = new Date(2026, 0, 1, 0, 0, 0);

const fileInput = document.querySelector("#game-file");
const fileButton = document.querySelector(".file-button");
const status = document.querySelector("#load-status");
const gameWrap = document.querySelector("#game-wrap");
const launcher = document.querySelector("#launcher");
const game = document.querySelector("#game");
const downloads = document.querySelector("#downloads");
const canonicalDownload = document.querySelector("#download-canonical");
const compatibilityDownload = document.querySelector("#download-compatible");

let downloadArchives = {};

const playerState = {
  phase: "waiting",
  inputKind: null,
  controls: {
    up: "up arrow",
    down: "down arrow",
    left: "left arrow",
    right: "right arrow",
    jump: "a",
    run: "s",
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
  fileButton.setAttribute("aria-disabled", busy ? "true" : "false");
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

async function loadConfig() {
  const response = await fetch(
    new URL("build-manifest.json", document.baseURI),
    { cache: "no-store" }
  );
  if (!response.ok) {
    throw new Error(`player configuration could not be loaded (${response.status})`);
  }
  const config = await response.json();
  if (
    config.project !== "SMBNeo" ||
    config.product?.shortname !== "smbneo" ||
    config.product?.title !== "Super Mario Bros. Neo" ||
    config.fbneo_driver !== "puzzledp" ||
    config.downloads?.canonical?.filename !== "smbneo.zip" ||
    config.downloads?.compatibility?.filename !== "puzzledp.zip" ||
    !Number.isInteger(config.title_patch_offsets?.native) ||
    !Number.isInteger(config.title_patch_offsets?.web)
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
    new Blob([archive.bytes], { type: "application/zip" })
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

canonicalDownload.addEventListener(
  "click",
  () => downloadArchive("canonical")
);
compatibilityDownload.addEventListener(
  "click",
  () => downloadArchive("compatibility")
);

function enableDownloads(canonicalArchive, compatibilityArchive, config) {
  downloadArchives = {
    canonical: {
      filename: config.downloads.canonical.filename,
      bytes: canonicalArchive,
    },
    compatibility: {
      filename: config.downloads.compatibility.filename,
      bytes: compatibilityArchive,
    },
  };
  canonicalDownload.disabled = false;
  compatibilityDownload.disabled = false;
  downloads.hidden = false;
  playerState.downloads = {
    canonical: {
      filename: config.downloads.canonical.filename,
      bytes: canonicalArchive.length,
    },
    compatibility: {
      filename: config.downloads.compatibility.filename,
      bytes: compatibilityArchive.length,
    },
  };
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
      }
    );
  });
}

function installEmulator(gameArchive, config) {
  const gameFile = new File(
    [gameArchive],
    "puzzledp.zip",
    { type: "application/zip" }
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
  window.EJS_startButtonName = "Play SMBNeo";
  window.EJS_alignStartButton = "center";
  window.EJS_backgroundImage =
    new URL("title.png", document.baseURI).href;
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

async function prepareSelectedFile(file) {
  if (window.fflate === undefined) {
    throw new Error("the archive library did not load");
  }

  const config = await loadConfig();
  const selectedBytes = new Uint8Array(await file.arrayBuffer());
  const source = classifyInput(selectedBytes, window.fflate.unzipSync);
  playerState.inputKind = source.kind;

  setStatus("Loading the ROM-free Neo Geo template…", "busy");
  const templateBytes = await fetchVerifiedBytes(
    config.template.path,
    config.template.sha256,
    "Neo Geo template"
  );
  const templateEntries = window.fflate.unzipSync(templateBytes);

  let cartridge;
  if (source.kind === "cartridge") {
    cartridge = source.profile === "compatibility"
      ? adaptCompatibilityCartridgeForNative(
          source.cartridge,
          templateEntries,
          config.title_patch_offsets
        )
      : source.cartridge;
  } else {
    setStatus("Checking the game revision…", "busy");
    const sourceSha1 = await sha1Hex(source.rom);
    if (sourceSha1 !== EXPECTED_NES_SHA1) {
      throw new Error(
        "this is not the supported Super Mario Bros. (World) revision"
      );
    }

    setStatus("Converting graphics for the Neo Geo…", "busy");
    await new Promise((resolve) => requestAnimationFrame(resolve));
    cartridge = buildCartridgeFromNes(
      source.rom,
      templateEntries,
      config.title_patch_offsets.native
    );
  }

  setStatus("Preparing the canonical SMBNeo cartridge…", "busy");
  await new Promise((resolve) => requestAnimationFrame(resolve));
  const canonicalEntries = buildCanonicalEntries(cartridge);
  const canonicalArchive = await zipEntries(canonicalEntries);

  setStatus("Adapting the cartridge for the browser player…", "busy");
  const webCartridge = adaptCartridgeForWeb(
    cartridge,
    templateEntries,
    config.title_patch_offsets
  );

  await fetchVerifiedBytes(
    config.bios.path,
    config.bios.sha256,
    "open Neo Geo BIOS"
  );

  setStatus("Preparing the FBNeo compatibility package…", "busy");
  await new Promise((resolve) => requestAnimationFrame(resolve));
  const fbneoEntries = buildPuzzledpEntries(
    webCartridge,
    (completed, total) => {
      if (completed < total) {
        setStatus(
          `Preparing the FBNeo compatibility package (${completed + 1}/${total})…`,
          "busy"
        );
      }
    }
  );

  setStatus("Compressing the browser cartridge…", "busy");
  const launchArchive = await zipEntries(fbneoEntries);
  enableDownloads(canonicalArchive, launchArchive, config);
  playerState.archiveBytes = launchArchive.length;
  playerState.phase = "loading-emulator";
  setStatus(
    `Cartridge ready (${formatBytes(launchArchive.length)}). Loading the player…`,
    "ready"
  );
  installEmulator(launchArchive, config);
}

fileInput.addEventListener("change", async () => {
  const [file] = fileInput.files;
  if (file === undefined) {
    return;
  }

  setBusy(true);
  setStatus(`Reading ${file.name}…`, "busy");
  try {
    await prepareSelectedFile(file);
  } catch (error) {
    console.error(error);
    setBusy(false);
    fileInput.value = "";
    setStatus(error instanceof Error ? error.message : String(error), "error");
  }
});
