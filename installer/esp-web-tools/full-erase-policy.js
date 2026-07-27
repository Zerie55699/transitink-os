// TransitInk's browser installer writes a merged factory image at offset zero.
// Always use ESP Web Tools' erase path so the UI cannot imply that this is a
// settings-preserving firmware update.

import "./vendor/install-dialog-C5LjR_e6.js";

const InstallDialog = customElements.get("ewt-install-dialog");
const startInstall = InstallDialog?.prototype?._startInstall;

if (typeof startInstall !== "function") {
  throw new Error("Unsupported ESP Web Tools install dialog");
}

InstallDialog.prototype._startInstall = function forceFullErase() {
  return startInstall.call(this, true);
};
