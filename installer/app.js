const deviceSelect = document.querySelector("#device-select");
const installButton = document.querySelector("#install-button");
const installAction = document.querySelector(".install-action");
const releaseStatus = document.querySelector("#release-status");
const firmwareVersion = document.querySelector("#firmware-version");

const fields = {
  heroName: document.querySelector("#hero-device-name"),
  heroStatus: document.querySelector("#hero-device-status"),
  chipFamily: document.querySelector("#hero-chip-family"),
  display: document.querySelector("#hero-display"),
  connection: document.querySelector("#hero-connection"),
  selectedName: document.querySelector("#selected-device-name"),
  selectedStatus: document.querySelector("#selected-device-status"),
  installNote: document.querySelector("#device-install-note"),
};

let devices = [];
let activeManifest = "";

function setText(element, value) {
  if (element) element.textContent = value;
}

function showReleaseState(state, message) {
  releaseStatus.dataset.state = state;
  setText(firmwareVersion, message);
}

async function loadVersion(manifestUrl) {
  activeManifest = manifestUrl;
  installAction.disabled = true;
  installAction.textContent = "正在準備安裝資料";
  showReleaseState("loading", "正在讀取");
  try {
    const response = await fetch(manifestUrl, { cache: "no-store" });
    if (!response.ok) throw new Error("manifest unavailable");
    const manifest = await response.json();
    if (typeof manifest.version !== "string" || !manifest.version.trim()) {
      throw new Error("manifest version unavailable");
    }
    if (manifestUrl === activeManifest) {
      installAction.disabled = false;
      installAction.textContent = "連接裝置並安裝";
      showReleaseState("ready", manifest.version);
    }
  } catch {
    if (manifestUrl === activeManifest) {
      installAction.disabled = true;
      installAction.textContent = "安裝資料未能讀取";
      showReleaseState("error", "未能讀取");
    }
  }
}

function applyDevice(device) {
  setText(fields.heroName, device.name);
  setText(fields.heroStatus, device.status_label);
  setText(fields.chipFamily, device.chip_family);
  setText(fields.display, device.display);
  setText(fields.connection, device.connection);
  setText(fields.selectedName, device.name);
  setText(fields.selectedStatus, device.status_label);
  setText(fields.installNote, device.install_note);

  installButton.setAttribute("manifest", device.manifest);
  if (device.installable === false) {
    activeManifest = "";
    installAction.disabled = true;
    installAction.textContent = "此裝置尚未提供安裝檔";
    showReleaseState("error", "尚未發布");
  } else {
    loadVersion(device.manifest);
  }
}

function renderDevices(catalog) {
  const available = catalog.devices.filter(
    (device) => device && typeof device.id === "string" && typeof device.name === "string"
  );
  if (!available.length) throw new Error("device catalog is empty");

  devices = available;
  deviceSelect.replaceChildren();
  for (const device of devices) {
    const option = document.createElement("option");
    option.value = device.id;
    option.textContent = device.name;
    deviceSelect.append(option);
  }
  applyDevice(devices[0]);
}

deviceSelect.addEventListener("change", () => {
  const selected = devices.find((device) => device.id === deviceSelect.value);
  if (selected) applyDevice(selected);
});

try {
  const response = await fetch("./devices.json", { cache: "no-store" });
  if (!response.ok) throw new Error("device catalog unavailable");
  const catalog = await response.json();
  if (catalog.schema_version !== 1 || !Array.isArray(catalog.devices)) {
    throw new Error("device catalog format is invalid");
  }
  renderDevices(catalog);
} catch {
  devices = [
    {
      id: "zectrix_note4",
      name: "Zectrix Note 4",
      status_label: "正式支援",
      chip_family: "ESP32-S3",
      display: "400 × 300 電子紙",
      connection: "USB-C 數據線",
      manifest: "./manifest.json",
      install_note: "如未能找到連接埠，按住 BOOT 鍵再重新接上 USB。",
      installable: true,
    },
  ];
  applyDevice(devices[0]);
}
