// Modified by TransitInk OS, 2026: Traditional Chinese dialog copy and
// same-origin imports. Based on ESP Web Tools 10.2.1 (Apache-2.0).
// Upstream: https://github.com/esphome/esp-web-tools

import {
  y as baseStyles,
  a as css,
  _ as decorate,
  t as customElement,
  i as LitElement,
  x as html,
} from "./vendor/styles-sT2V1cOw.js";

let NoPortPickedDialog = class extends LitElement {
  render() {
    return html`
      <ew-dialog open @closed=${this._handleClose}>
        <div slot="headline">未選擇連接埠</div>
        <div slot="content">
          <div>如果找不到裝置，請檢查以下事項：</div>
          <ol>
            <li>確認裝置已連接到正在開啟此網頁的電腦。</li>
            <li>確認裝置已開機；如有電源指示燈，應保持亮起。</li>
            <li>確認 USB 線支援數據傳輸，不是只供充電的線。</li>
          </ol>
        </div>
        <div slot="actions">
          ${this.doTryAgain
            ? html`
                <ew-text-button @click=${this.close}>取消</ew-text-button>
                <ew-text-button @click=${this.tryAgain}>再試一次</ew-text-button>
              `
            : html`<ew-text-button @click=${this.close}>關閉</ew-text-button>`}
        </div>
      </ew-dialog>
    `;
  }

  tryAgain() {
    this.close();
    this.doTryAgain?.();
  }

  close() {
    this.shadowRoot.querySelector("ew-dialog").close();
  }

  async _handleClose() {
    this.parentNode.removeChild(this);
  }
};

NoPortPickedDialog.styles = [
  baseStyles,
  css`
    li + li {
      margin-top: 8px;
    }

    ol {
      margin-bottom: 0;
      padding-left: 1.5em;
    }
  `,
];

NoPortPickedDialog = decorate(
  [customElement("ewt-no-port-picked-dialog")],
  NoPortPickedDialog,
);

const openNoPortPickedDialog = async (doTryAgain) => {
  const dialog = document.createElement("ewt-no-port-picked-dialog");
  dialog.doTryAgain = doTryAgain;
  document.body.append(dialog);
  return true;
};

export { openNoPortPickedDialog };
