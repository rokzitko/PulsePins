// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#include "ppwebgui_assets.hh"

const char *index_html = R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>PulsePins Web GUI</title>
  <link rel="stylesheet" href="/app.css">
</head>
<body>
  <main class="app-shell">
    <header class="app-header">
      <div>
        <h1>PulsePins Web GUI</h1>
        <div class="meta">Single-stream control, trigger monitoring, combiner setup, and sequence playback.</div>
      </div>
      <div class="header-actions">
        <div class="header-action-group">
          <button id="reset-button" type="button" class="secondary-button">Reset hardware</button>
          <div class="meta action-note">Resets FPGA-side state, then restores current ppwebgui-managed settings.</div>
        </div>
        <div id="global-status" class="notice">Connecting...</div>
      </div>
    </header>

    <section class="panel">
      <h2>Status Provenance</h2>
      <div class="legend-grid">
        <div class="legend-item">
          <span class="state-tag live-tag">live hardware</span>
          <div class="meta">Polled from stable hardware register paths.</div>
        </div>
        <div class="legend-item">
          <span class="state-tag tracked-tag">tracked by ppwebgui</span>
          <div class="meta">Controller-managed state restored after reset. Not reread live.</div>
        </div>
        <div class="legend-item">
          <span class="state-tag local-tag">local edit</span>
          <div class="meta">Browser-only form changes until you click Apply or Revert.</div>
        </div>
      </div>
      <div class="meta warning-text">If another tool changes trigger, combiner, or qout state after ppwebgui starts, tracked fields here can drift from live hardware.</div>
    </section>

    <section class="panel">
      <div class="panel-heading">
        <h2>Live Hardware</h2>
        <span class="state-tag live-tag">live</span>
      </div>
      <div class="panel-note">Only stable hardware readbacks are polled live.</div>
      <div class="status-grid">
        <div class="status-card">
          <div class="label">AUX</div>
          <div id="aux-bits" class="bits"></div>
          <div id="aux-raw" class="meta mono"></div>
        </div>
        <div class="status-card">
          <div class="label">TRIG</div>
          <div id="trig-bits" class="bits"></div>
          <div id="trig-raw" class="meta mono"></div>
          <div id="trig-flags" class="meta mono"></div>
        </div>
        <div class="status-card">
          <div class="label">Streamer runtime</div>
          <div id="stream-runtime-flags" class="meta mono"></div>
          <div id="stream-runtime-raw" class="meta mono"></div>
        </div>
      </div>
    </section>

    <section class="panel">
      <div class="panel-heading">
        <h2>Tracked by ppwebgui</h2>
        <span class="state-tag tracked-tag">tracked</span>
      </div>
      <div class="panel-note">These values come from ppwebgui's controller snapshot and are restored after reset.</div>
      <div class="status-grid">
        <div class="status-card">
          <div class="label">Displayed qout</div>
          <div id="streamer-qout" class="meta mono"></div>
        </div>
        <div class="status-card">
          <div class="label">Tracked idle streamer qout</div>
          <div id="streamer-qout-raw" class="meta mono"></div>
        </div>
        <div class="status-card">
          <div class="label">Output override</div>
          <div id="streamer-override" class="meta mono"></div>
        </div>
      </div>
      <div class="meta-row">
        <span><strong>Combiner:</strong> <span id="combiner-mode"></span></span>
        <span><strong>Trigger mode:</strong> <span id="trigger-mode-summary"></span></span>
        <span><strong>Last action:</strong> <span id="last-action"></span></span>
        <span><strong>Last error:</strong> <span id="last-error"></span></span>
      </div>
    </section>

    <section class="panel">
      <div class="panel-heading">
        <h2>Trigger Settings</h2>
        <div class="heading-tags">
          <span class="state-tag tracked-tag">tracked</span>
          <span id="trigger-local-tag" class="state-tag local-tag hidden">local edit</span>
        </div>
      </div>
      <div id="trigger-form-state" class="form-state">Tracked trigger settings are shown below. Local edits stay in the browser until you click Apply or Revert.</div>
      <div class="panel-note">Restored by ppwebgui on reset. These settings are tracked, not polled live. AUX fields remain visible but read-only.</div>
      <form id="trigger-form">
        <div class="settings-grid">
          <label>Mode
            <select name="mode" id="trigger-mode-select">
              <option>STANDARD</option>
              <option>INT</option>
              <option>EXT</option>
              <option>MISC</option>
              <option>ANY</option>
              <option>ALL</option>
            </select>
          </label>
          <label>Result invert<input name="invert_result" value="0x0"></label>
          <label>INT invert<input name="invert_int" value="0x0"></label>
          <label>EXT invert<input id="trigger-ext-invert" name="invert_ext" value="0xffffffff"></label>
          <label>MISC invert<input name="invert_misc" value="0x0"></label>
          <label>INT mask<input name="mask_int" value="0x0"></label>
          <label>EXT mask<input name="mask_ext" value="0x0"></label>
          <label>MISC mask<input name="mask_misc" value="0x0"></label>
        </div>
        <div class="form-grid">
          <button type="submit">Apply trigger settings</button>
          <button id="trigger-revert-button" type="button" class="secondary-button">Revert local edits</button>
        </div>
      </form>
      <div class="meta">STANDARD follows CLI semantics and forces EXT invert to `0xffffffff`.</div>
      <div class="settings-grid">
        <div class="setting"><div class="label">AUX invert</div><div id="trigger-invert-aux" class="mono"></div></div>
        <div class="setting"><div class="label">AUX mask</div><div id="trigger-mask-aux" class="mono"></div></div>
      </div>
    </section>

    <section class="panel">
      <div class="panel-heading">
        <h2>Clocking</h2>
        <div class="heading-tags">
          <span class="state-tag tracked-tag">tracked</span>
          <span id="clocking-local-tag" class="state-tag local-tag hidden">local edit</span>
        </div>
      </div>
      <div id="clocking-form-state" class="form-state">Tracked clock settings are shown below. Local edits stay in the browser until you click Apply or Revert.</div>
      <div class="panel-note">Applying clock settings reruns reset and bring-up, then remeasures clocks. Measured values below are the last captured snapshot, not live-polled frequencies. The current tracked source is shown read-only below.</div>
      <div class="settings-grid">
        <div class="setting"><div class="label">Current tracked source</div><div id="clocking-source-current" class="mono"></div></div>
      </div>
      <form id="clocking-form">
        <div class="settings-grid">
          <label>Managed streamer clock source
            <select name="source" id="clocking-source-select">
              <option value="int_clk">int_clk</option>
              <option value="ext_clk">ext_clk</option>
            </select>
          </label>
          <label>core_clk profile
            <select name="core_profile" id="clocking-core-profile-select">
              <option>100M</option>
              <option>80M</option>
              <option>75M</option>
              <option>60M</option>
              <option>50M</option>
              <option>40M</option>
              <option>30M</option>
              <option>25M</option>
              <option>20M</option>
              <option>10M</option>
              <option>5M</option>
              <option>1M</option>
              <option>100k</option>
              <option>10k</option>
              <option>lj</option>
              <option>ilj</option>
              <option>ih</option>
              <option>il</option>
              <option>i2h</option>
              <option>i2l</option>
            </select>
          </label>
          <label>int_clk profile
            <select name="int_profile" id="clocking-int-profile-select">
              <option>100M</option>
              <option>80M</option>
              <option>75M</option>
              <option>60M</option>
              <option>50M</option>
              <option>40M</option>
              <option>30M</option>
              <option>25M</option>
              <option>20M</option>
              <option>10M</option>
              <option>5M</option>
              <option>1M</option>
              <option>100k</option>
              <option>10k</option>
              <option>lj</option>
              <option>ilj</option>
              <option>ih</option>
              <option>il</option>
              <option>i2h</option>
              <option>i2l</option>
            </select>
          </label>
        </div>
        <div class="form-grid">
          <button type="submit">Apply clock settings</button>
          <button id="clocking-revert-button" type="button" class="secondary-button">Revert local edits</button>
          <button id="clocking-measure-button" type="button" class="secondary-button">Remeasure clocks</button>
        </div>
      </form>
      <div class="meta">Profile menus expose the standard `pll_rules.hh` presets. If `ppwebgui` starts from a nonstandard current profile, that exact value is shown so the UI stays honest. If startup used an unmanaged/raw source selector, the current source stays read-only until you explicitly apply `int_clk` or `ext_clk` here. `ext_clk` follows the external-source path of the currently loaded bitstream.</div>
      <div class="settings-grid">
        <div class="setting"><div class="label">ext_clk</div><div id="clocking-ext-hz" class="mono"></div></div>
        <div class="setting"><div class="label">int_clk</div><div id="clocking-int-hz" class="mono"></div></div>
        <div class="setting"><div class="label">streamer_clk</div><div id="clocking-streamer-hz" class="mono"></div></div>
        <div class="setting"><div class="label">core_clk</div><div id="clocking-core-hz" class="mono"></div></div>
      </div>
    </section>

    <section class="panel">
      <div class="panel-heading">
        <h2>Output Override</h2>
        <div class="heading-tags">
          <span class="state-tag tracked-tag">tracked</span>
          <span id="qout-local-tag" class="state-tag local-tag hidden">local edit</span>
        </div>
      </div>
      <div id="qout-form-state" class="form-state">Tracked output-override values are shown below. Local edits stay in the browser until you click Apply or Revert.</div>
      <form id="qout-form" class="form-grid">
        <label>Enabled<select name="override_enabled"><option value="0">false</option><option value="1">true</option></select></label>
        <label>Override value<input name="override_value" value="0x0" placeholder="0x0"></label>
        <button type="submit">Apply override</button>
        <button id="qout-revert-button" type="button" class="secondary-button">Revert local edits</button>
      </form>
      <div class="meta">Manual final-output override, implemented through the combiner output-force path.</div>
      <div class="meta">Accepted integer formats: decimal (`42`), hex (`0xff`), binary (`0b1010`), octal (`077`), and Verilog-style literals like `8'hFF` or `'b1010`.</div>
    </section>

    <section class="panel">
      <div class="panel-heading">
        <h2>Output Combiner</h2>
        <div class="heading-tags">
          <span class="state-tag tracked-tag">tracked</span>
          <span id="combiner-local-tag" class="state-tag local-tag hidden">local edit</span>
        </div>
      </div>
      <div id="combiner-form-state" class="form-state">Tracked combiner values are shown below. Local edits stay in the browser until you click Apply or Revert.</div>
      <form id="combiner-form" class="combiner-form">
        <label>Mode
          <select name="mode" id="combiner-mode-select">
            <option>SEL1</option>
            <option>SEL2</option>
            <option>SEL3</option>
            <option>SEL4</option>
            <option>AND</option>
            <option>OR</option>
            <option>XOR</option>
            <option>XNOR</option>
            <option>MAJ</option>
            <option>BLOCK8</option>
            <option>BLOCK16</option>
            <option>SUM12</option>
            <option>SUM1234</option>
            <option>DIFF12</option>
          </select>
        </label>

        <div class="subpanel">
          <h3>Output</h3>
          <div class="port-grid">
            <label>Invert<input name="output_invert" value="0x0"></label>
            <label>Mask<input name="output_mask" value="0xffffffff"></label>
            <label>Force enabled<select name="output_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
            <label>Force value<input name="output_force_value" value="0x0"></label>
          </div>
        </div>

        <div class="ports-grid">
          <div class="subpanel">
            <h3>Input 1</h3>
            <div class="port-grid">
              <label>Invert<input name="in1_invert" value="0x0"></label>
              <label>Mask<input name="in1_mask" value="0xffffffff"></label>
              <label>Force enabled<select name="in1_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
              <label>Force value<input name="in1_force_value" value="0x0"></label>
            </div>
          </div>
          <div class="subpanel">
            <h3>Input 2</h3>
            <div class="port-grid">
              <label>Invert<input name="in2_invert" value="0x0"></label>
              <label>Mask<input name="in2_mask" value="0xffffffff"></label>
              <label>Force enabled<select name="in2_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
              <label>Force value<input name="in2_force_value" value="0x0"></label>
            </div>
          </div>
          <div class="subpanel">
            <h3>Input 3</h3>
            <div class="port-grid">
              <label>Invert<input name="in3_invert" value="0x0"></label>
              <label>Mask<input name="in3_mask" value="0xffffffff"></label>
              <label>Force enabled<select name="in3_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
              <label>Force value<input name="in3_force_value" value="0x0"></label>
            </div>
          </div>
          <div class="subpanel">
            <h3>Input 4</h3>
            <div class="port-grid">
              <label>Invert<input name="in4_invert" value="0x0"></label>
              <label>Mask<input name="in4_mask" value="0xffffffff"></label>
              <label>Force enabled<select name="in4_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
              <label>Force value<input name="in4_force_value" value="0x0"></label>
            </div>
          </div>
        </div>

        <button type="submit">Apply combiner</button>
        <button id="combiner-revert-button" type="button" class="secondary-button">Revert local edits</button>
      </form>
    </section>

    <section class="panel">
      <h2>Sequence</h2>
      <div class="panel-note">Start streaming resets hardware first and appends the tracked idle qout as the final output.</div>
      <form id="stream-form" class="sequence-form">
        <label>Sequence text
          <textarea name="sequence_text" rows="8">d 10000000 0xff
d 10000000 0x00
d 10000000 0xff
d 10000000 0x00
d 10000000 0xff
</textarea>
        </label>
        <label class="checkbox"><input type="checkbox" name="force_trigger"> Force trigger</label>
        <label class="checkbox"><input type="checkbox" name="check_readback"> Check readback</label>
        <button type="submit">Start streaming</button>
      </form>
      <div id="stream-state" class="meta"></div>
      <pre id="stream-result" class="result-box"></pre>
    </section>
  </main>
  <script src="/app.js"></script>
</body>
</html>
)HTML";

const char *app_css = R"CSS(:root {
  color-scheme: light dark;
  font-family: Inter, ui-sans-serif, system-ui, sans-serif;
}

body {
  margin: 0;
  background: #0f172a;
  color: #e2e8f0;
}

.app-shell {
  max-width: 1200px;
  margin: 0 auto;
  padding: 1rem;
}

.app-shell h1 {
  margin: 0 0 0.25rem 0;
}

.panel > h2 {
  margin-top: 0;
}

.app-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 1rem;
  margin-bottom: 1rem;
}

.header-actions {
  display: flex;
  align-items: flex-start;
  justify-content: flex-end;
  flex-wrap: wrap;
  gap: 0.75rem;
}

.header-action-group {
  display: grid;
  gap: 0.4rem;
  justify-items: start;
}

.action-note {
  max-width: 24rem;
}

.panel, .subpanel {
  background: #111827;
  border: 1px solid #334155;
  border-radius: 10px;
  padding: 1rem;
  margin-bottom: 1rem;
}

.status-grid, .ports-grid, .form-grid, .port-grid, .settings-grid, .legend-grid {
  display: grid;
  gap: 0.75rem;
}

.status-grid {
  grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
}

.form-grid {
  grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
  align-items: end;
}

.ports-grid {
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
}

.settings-grid {
  grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
}

.legend-grid {
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  margin-bottom: 0.75rem;
}

.port-grid {
  grid-template-columns: 1fr 1fr;
}

.panel-heading {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 0.75rem;
  flex-wrap: wrap;
  margin-bottom: 0.35rem;
}

.panel-heading h2 {
  margin: 0;
}

.panel-note {
  color: #cbd5e1;
  margin-bottom: 0.75rem;
}

.heading-tags {
  display: flex;
  gap: 0.5rem;
  flex-wrap: wrap;
}

label {
  display: grid;
  gap: 0.35rem;
  font-size: 0.95rem;
}

input, select, textarea, button {
  box-sizing: border-box;
  width: 100%;
  border-radius: 8px;
  border: 1px solid #475569;
  background: #0f172a;
  color: inherit;
  padding: 0.6rem 0.75rem;
}

button {
  cursor: pointer;
  background: #1d4ed8;
  font-weight: 600;
}

.secondary-button {
  width: auto;
  background: #475569;
}

button:disabled {
  opacity: 0.6;
  cursor: wait;
}

.checkbox {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.checkbox input {
  width: auto;
}

.bits {
  font-family: ui-monospace, SFMono-Regular, monospace;
  font-size: 1.25rem;
  letter-spacing: 0.15em;
  margin: 0.4rem 0;
}

.mono {
  font-family: ui-monospace, SFMono-Regular, monospace;
}

.meta, .meta-row, .notice, .result-box {
  color: #cbd5e1;
}

.legend-item, .status-card, .setting, .form-state {
  background: #0f172a;
  border: 1px solid #334155;
  border-radius: 8px;
  padding: 0.75rem;
}

.label {
  font-weight: 600;
  margin-bottom: 0.35rem;
}

.meta-row {
  display: flex;
  gap: 1rem;
  flex-wrap: wrap;
  margin-top: 0.75rem;
}

.state-tag {
  display: inline-flex;
  align-items: center;
  border-radius: 999px;
  padding: 0.18rem 0.55rem;
  font-size: 0.78rem;
  font-weight: 600;
  letter-spacing: 0.02em;
  text-transform: lowercase;
}

.live-tag {
  background: #0f766e;
  color: #ccfbf1;
}

.tracked-tag {
  background: #334155;
  color: #e2e8f0;
}

.local-tag {
  background: #92400e;
  color: #fef3c7;
}

.neutral-tag {
  background: #3730a3;
  color: #e0e7ff;
}

.warning-text {
  color: #fbbf24;
}

.notice {
  padding: 0.6rem 0.8rem;
  border-radius: 8px;
  background: #1e293b;
}

.notice.error {
  background: #7f1d1d;
}

.result-box {
  min-height: 4rem;
  white-space: pre-wrap;
  word-break: break-word;
}

.form-state {
  margin-bottom: 0.75rem;
}

.form-state.local-edit {
  border-color: #f59e0b;
  color: #fde68a;
}

.form-dirty {
  outline: 1px solid #f59e0b;
  outline-offset: 0.35rem;
  border-radius: 10px;
}

.hidden {
  display: none;
}

@media (max-width: 640px) {
  .app-header {
    align-items: flex-start;
    flex-direction: column;
  }

  .header-actions {
    width: 100%;
  }

  .port-grid {
    grid-template-columns: 1fr;
  }
}
)CSS";

const char *app_js = R"JS((() => {
  const globalStatus = document.getElementById('global-status');
  const streamResult = document.getElementById('stream-result');
  const resetButton = document.getElementById('reset-button');
  const clockingForm = document.getElementById('clocking-form');
  const triggerForm = document.getElementById('trigger-form');
  const qoutForm = document.getElementById('qout-form');
  const combinerForm = document.getElementById('combiner-form');
  const streamForm = document.getElementById('stream-form');
  const clockingSourceSelect = document.getElementById('clocking-source-select');
  const clockingCoreProfileSelect = document.getElementById('clocking-core-profile-select');
  const clockingIntProfileSelect = document.getElementById('clocking-int-profile-select');
  const clockingRevertButton = document.getElementById('clocking-revert-button');
  const clockingMeasureButton = document.getElementById('clocking-measure-button');
  const triggerModeSelect = document.getElementById('trigger-mode-select');
  const triggerExtInvertInput = document.getElementById('trigger-ext-invert');
  const triggerRevertButton = document.getElementById('trigger-revert-button');
  const qoutRevertButton = document.getElementById('qout-revert-button');
  const combinerRevertButton = document.getElementById('combiner-revert-button');
  const clockingFormState = document.getElementById('clocking-form-state');
  const triggerFormState = document.getElementById('trigger-form-state');
  const qoutFormState = document.getElementById('qout-form-state');
  const combinerFormState = document.getElementById('combiner-form-state');
  const clockingLocalTag = document.getElementById('clocking-local-tag');
  const triggerLocalTag = document.getElementById('trigger-local-tag');
  const qoutLocalTag = document.getElementById('qout-local-tag');
  const combinerLocalTag = document.getElementById('combiner-local-tag');
  let clockingCleanText = 'Tracked clock settings are shown below. Local edits stay in the browser until you click Apply or Revert.';
  const triggerCleanText = 'Tracked trigger settings are shown below. Local edits stay in the browser until you click Apply or Revert.';
  const qoutCleanText = 'Tracked output-override values are shown below. Local edits stay in the browser until you click Apply or Revert.';
  const combinerCleanText = 'Tracked combiner values are shown below. Local edits stay in the browser until you click Apply or Revert.';
  let pollMs = 100;
  let hardwareBusy = false;
  let clockingDirty = false;
  let triggerDirty = false;
  let qoutDirty = false;
  let combinerDirty = false;
  let lastStatus = null;

  function formatHex(value, width = 8) {
    const normalized = Number(value) >>> 0;
    return `0x${normalized.toString(16).padStart(width, '0')}`;
  }

  function formatFrequencyHz(value) {
    const hz = Number(value);
    if (!Number.isFinite(hz) || hz <= 0) {
      return '(unmeasured)';
    }
    const units = [
      { scale: 1e9, suffix: 'GHz' },
      { scale: 1e6, suffix: 'MHz' },
      { scale: 1e3, suffix: 'kHz' },
      { scale: 1, suffix: 'Hz' },
    ];
    const unit = units.find((candidate) => hz >= candidate.scale) || units[units.length - 1];
    const valueInUnit = hz / unit.scale;
    const decimals = valueInUnit >= 100 ? 0 : valueInUnit >= 10 ? 2 : 3;
    return `${valueInUnit.toLocaleString(undefined, { maximumFractionDigits: decimals })} ${unit.suffix}`;
  }

  function setText(id, value) {
    document.getElementById(id).textContent = value;
  }

  function setGlobal(message, isError = false) {
    globalStatus.textContent = message;
    globalStatus.classList.toggle('error', isError);
  }

  function setBusy(form, busy) {
    for (const element of form.elements) {
      element.disabled = busy;
    }
  }

  function setHardwareBusy(busy) {
    hardwareBusy = busy;
    resetButton.disabled = busy;
    setBusy(clockingForm, busy);
    setBusy(triggerForm, busy);
    setBusy(qoutForm, busy);
    setBusy(combinerForm, busy);
    setBusy(streamForm, busy);
  }

  function setFormDirty(form, dirty, stateElement, tagElement, cleanText, dirtyText) {
    form.classList.toggle('form-dirty', dirty);
    stateElement.classList.toggle('local-edit', dirty);
    stateElement.textContent = dirty ? dirtyText : cleanText;
    tagElement.classList.toggle('hidden', !dirty);
  }

  function ensureSelectValue(select, value, fallback = '100M') {
    const resolved = value || fallback;
    const hasOption = Array.from(select.options).some((option) => option.value === resolved);
    if (!hasOption) {
      const dynamicOption = document.createElement('option');
      dynamicOption.value = resolved;
      dynamicOption.textContent = resolved;
      dynamicOption.dataset.dynamic = 'true';
      select.appendChild(dynamicOption);
    }
    select.value = resolved;
  }

  function syncTriggerModeUi() {
    const standard = triggerModeSelect.value === 'STANDARD';
    if (standard) {
      triggerExtInvertInput.value = '0xffffffff';
    }
    triggerExtInvertInput.readOnly = standard;
  }

  function setClockingDirty(dirty) {
    clockingDirty = dirty;
    setFormDirty(
      clockingForm,
      dirty,
      clockingFormState,
      clockingLocalTag,
      clockingCleanText,
      'Local edit only. This clocking form differs from the tracked ppwebgui state until you click Apply or Revert.');
  }

  function clockingCleanTextForStatus(clocking) {
    if (clocking.tracked.source_managed) {
      return 'Tracked clock settings are shown below. Local edits stay in the browser until you click Apply or Revert.';
    }
    return `Clock settings are tracked, but the current source (${clocking.tracked.source_display}) is read-only until you explicitly apply int_clk or ext_clk.`;
  }

  function setTriggerDirty(dirty) {
    triggerDirty = dirty;
    setFormDirty(
      triggerForm,
      dirty,
      triggerFormState,
      triggerLocalTag,
      triggerCleanText,
      'Local edit only. This trigger form differs from the tracked ppwebgui state until you click Apply or Revert.');
  }

  function setQoutDirty(dirty) {
    qoutDirty = dirty;
    setFormDirty(
      qoutForm,
      dirty,
      qoutFormState,
      qoutLocalTag,
      qoutCleanText,
      'Local edit only. This output-override form differs from the tracked ppwebgui state until you click Apply.');
  }

  function setCombinerDirty(dirty) {
    combinerDirty = dirty;
    setFormDirty(
      combinerForm,
      dirty,
      combinerFormState,
      combinerLocalTag,
      combinerCleanText,
      'Local edit only. This combiner form differs from the tracked ppwebgui state until you click Apply.');
  }

  function populateClocking(status, force = false) {
    const clocking = status.clocking;
    setText('clocking-ext-hz', formatFrequencyHz(clocking.measured.ext_clk_hz));
    setText('clocking-int-hz', formatFrequencyHz(clocking.measured.int_clk_hz));
    setText('clocking-streamer-hz', formatFrequencyHz(clocking.measured.streamer_clk_hz));
    setText('clocking-core-hz', formatFrequencyHz(clocking.measured.core_clk_hz));
    setText('clocking-source-current', clocking.tracked.source_display);
    if (!force && clockingDirty) return;
    clockingCleanText = clockingCleanTextForStatus(clocking);
    if (clocking.tracked.source_managed) {
      clockingSourceSelect.value = clocking.tracked.source;
    } else {
      clockingSourceSelect.value = 'int_clk';
    }
    ensureSelectValue(clockingCoreProfileSelect, clocking.tracked.core_profile);
    ensureSelectValue(clockingIntProfileSelect, clocking.tracked.int_profile);
    setClockingDirty(false);
  }

  function populateTrigger(status, force = false) {
    if (!force && triggerDirty) return;
    const settings = status.trigger_settings;
    triggerModeSelect.value = settings.mode;
    triggerForm.querySelector('[name="invert_result"]').value = formatHex(settings.invert_result);
    triggerForm.querySelector('[name="invert_int"]').value = formatHex(settings.invert_int);
    triggerForm.querySelector('[name="invert_ext"]').value = formatHex(settings.invert_ext);
    triggerForm.querySelector('[name="invert_misc"]').value = formatHex(settings.invert_misc);
    triggerForm.querySelector('[name="mask_int"]').value = formatHex(settings.mask_int);
    triggerForm.querySelector('[name="mask_ext"]').value = formatHex(settings.mask_ext);
    triggerForm.querySelector('[name="mask_misc"]').value = formatHex(settings.mask_misc);
    setText('trigger-invert-aux', formatHex(settings.invert_aux));
    setText('trigger-mask-aux', formatHex(settings.mask_aux));
    syncTriggerModeUi();
    setTriggerDirty(false);
  }

  function populateQout(status, force = false) {
    if (!force && qoutDirty) return;
    qoutForm.querySelector('[name="override_enabled"]').value = status.streamer.override.enabled ? '1' : '0';
    qoutForm.querySelector('[name="override_value"]').value = formatHex(status.streamer.override.value);
    setQoutDirty(false);
  }

  function populateCombiner(status, force = false) {
    if (!force && combinerDirty) return;
    combinerForm.querySelector('[name="mode"]').value = status.combiner.mode;
    const output = status.combiner.output;
    combinerForm.querySelector('[name="output_invert"]').value = formatHex(output.invert);
    combinerForm.querySelector('[name="output_mask"]').value = formatHex(output.mask);
    combinerForm.querySelector('[name="output_force_enabled"]').value = output.force_enabled ? '1' : '0';
    combinerForm.querySelector('[name="output_force_value"]').value = formatHex(output.force_value);
    status.combiner.inputs.forEach((input) => {
      const base = `in${input.index}`;
      combinerForm.querySelector(`[name="${base}_invert"]`).value = formatHex(input.invert);
      combinerForm.querySelector(`[name="${base}_mask"]`).value = formatHex(input.mask);
      combinerForm.querySelector(`[name="${base}_force_enabled"]`).value = input.force_enabled ? '1' : '0';
      combinerForm.querySelector(`[name="${base}_force_value"]`).value = formatHex(input.force_value);
    });
    setCombinerDirty(false);
  }

  function renderStatus(status, options = {}) {
    lastStatus = status;
    const runtimeFlags = [];
    if (status.stream.runtime.buffer_error) runtimeFlags.push('buffer_error');
    if (status.stream.runtime.done) runtimeFlags.push('done');
    if (status.stream.runtime.triggered) runtimeFlags.push('triggered');
    if (status.stream.runtime.armed) runtimeFlags.push('armed');
    const runtimeSummary = runtimeFlags.length ? runtimeFlags.join(' ') : 'idle';

    setText('aux-bits', status.aux.bits);
    setText('aux-raw', `raw=${formatHex(status.aux.raw, 2)}`);
    setText('trig-bits', status.trig.bits);
    setText('trig-raw', `raw=${formatHex(status.trig.raw)}`);
    setText('trig-flags', `enable=${status.trig.enable} force=${status.trig.force} reset=${status.trig.reset}`);
    setText('stream-runtime-flags', `flags=${runtimeSummary}`);
    setText('stream-runtime-raw', `raw=${formatHex(status.stream.runtime.raw)}`);
    setText('streamer-qout', formatHex(status.streamer.qout));
    setText('streamer-qout-raw', formatHex(status.streamer.qout_streamer));
    setText('streamer-override', `enabled=${status.streamer.override.enabled} value=${formatHex(status.streamer.override.value)}`);
    setText('combiner-mode', status.combiner.mode);
    setText('trigger-mode-summary', status.trigger_settings.mode);
    setText('last-action', status.last_action);
    setText('last-error', status.last_error || '(none)');
    setText('stream-state', `last result rc=${status.stream.last_rc} message=${status.stream.message} live runtime=${runtimeSummary} raw=${formatHex(status.stream.runtime.raw)}`);
    streamResult.textContent = status.stream.message;
    populateClocking(status, options.forceClocking === true);
    populateTrigger(status, options.forceTrigger === true);
    populateQout(status, options.forceQout === true);
    populateCombiner(status, options.forceCombiner === true);
    pollMs = status.poll_ms || 100;
  }

  function attachDirtyHandlers(form, markDirty) {
    const handler = (event) => {
      if (!event.isTrusted) return;
      markDirty(true);
    };
    form.addEventListener('input', handler);
    form.addEventListener('change', handler);
  }

  setClockingDirty(false);
  setTriggerDirty(false);
  setQoutDirty(false);
  setCombinerDirty(false);
  syncTriggerModeUi();
  attachDirtyHandlers(clockingForm, setClockingDirty);
  attachDirtyHandlers(triggerForm, setTriggerDirty);
  attachDirtyHandlers(qoutForm, setQoutDirty);
  attachDirtyHandlers(combinerForm, setCombinerDirty);

  clockingRevertButton.addEventListener('click', () => {
    if (!lastStatus) return;
    populateClocking(lastStatus, true);
  });

  triggerModeSelect.addEventListener('change', () => {
    syncTriggerModeUi();
  });

  triggerRevertButton.addEventListener('click', () => {
    if (!lastStatus) return;
    populateTrigger(lastStatus, true);
  });

  qoutRevertButton.addEventListener('click', () => {
    if (!lastStatus) return;
    populateQout(lastStatus, true);
  });

  combinerRevertButton.addEventListener('click', () => {
    if (!lastStatus) return;
    populateCombiner(lastStatus, true);
  });

  async function fetchJson(url, options = {}) {
    const response = await fetch(url, { cache: 'no-store', ...options });
    const data = await response.json();
    if (!response.ok || data.ok === false) {
      throw new Error(data.error || data.message || `HTTP ${response.status}`);
    }
    return data;
  }

  async function pollStatus() {
    if (hardwareBusy) {
      window.setTimeout(pollStatus, pollMs);
      return;
    }
    try {
      const status = await fetchJson(`/api/status?ts=${Date.now()}`);
      renderStatus(status);
      setGlobal('Connected');
    } catch (error) {
      setGlobal(error.message, true);
    } finally {
      window.setTimeout(pollStatus, pollMs);
    }
  }

  qoutForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    const body = new URLSearchParams(new FormData(qoutForm));
    setHardwareBusy(true);
    try {
      const result = await fetchJson('/api/qout', { method: 'POST', body });
      if (result.status) renderStatus(result.status, { forceQout: true });
      setGlobal(result.message || 'override updated');
    } catch (error) {
      setGlobal(error.message, true);
    } finally {
      setHardwareBusy(false);
    }
  });

  clockingForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    const body = new URLSearchParams(new FormData(clockingForm));
    setHardwareBusy(true);
    try {
      const result = await fetchJson('/api/clocking', { method: 'POST', body });
      if (result.status) renderStatus(result.status, { forceClocking: true });
      setGlobal(result.message || 'clock settings updated');
    } catch (error) {
      setGlobal(error.message, true);
    } finally {
      setHardwareBusy(false);
    }
  });

  clockingMeasureButton.addEventListener('click', async () => {
    const body = new URLSearchParams();
    setHardwareBusy(true);
    try {
      const result = await fetchJson('/api/clocking/measure', { method: 'POST', body });
      if (result.status) renderStatus(result.status);
      setGlobal(result.message || 'clock measurement updated');
    } catch (error) {
      setGlobal(error.message, true);
    } finally {
      setHardwareBusy(false);
    }
  });

  triggerForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    const body = new URLSearchParams(new FormData(triggerForm));
    setHardwareBusy(true);
    try {
      const result = await fetchJson('/api/trigger', { method: 'POST', body });
      if (result.status) renderStatus(result.status, { forceTrigger: true });
      setGlobal(result.message || 'trigger updated');
    } catch (error) {
      setGlobal(error.message, true);
    } finally {
      setHardwareBusy(false);
    }
  });

  combinerForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    const body = new URLSearchParams(new FormData(combinerForm));
    setHardwareBusy(true);
    try {
      const result = await fetchJson('/api/combiner', { method: 'POST', body });
      if (result.status) renderStatus(result.status, { forceCombiner: true });
      setGlobal(result.message || 'combiner updated');
    } catch (error) {
      setGlobal(error.message, true);
    } finally {
      setHardwareBusy(false);
    }
  });

  streamForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    const body = new URLSearchParams();
    body.set('sequence_text', streamForm.querySelector('[name="sequence_text"]').value);
    if (streamForm.querySelector('[name="force_trigger"]').checked) {
      body.set('force_trigger', '1');
    }
    if (streamForm.querySelector('[name="check_readback"]').checked) {
      body.set('check_readback', '1');
    }
    setHardwareBusy(true);
    streamResult.textContent = 'Streaming...';
    try {
      const result = await fetchJson('/api/stream', { method: 'POST', body });
      if (result.status) renderStatus(result.status);
      streamResult.textContent = result.message || 'Sequence completed';
      setGlobal(result.message || 'stream completed');
    } catch (error) {
      streamResult.textContent = error.message;
      setGlobal(error.message, true);
    } finally {
      setHardwareBusy(false);
    }
  });

  resetButton.addEventListener('click', async () => {
    const body = new URLSearchParams();
    setHardwareBusy(true);
    try {
      const result = await fetchJson('/api/reset', { method: 'POST', body });
      if (result.status) renderStatus(result.status);
      streamResult.textContent = result.message || 'Hardware reset completed';
      setGlobal(result.message || 'hardware reset completed');
    } catch (error) {
      streamResult.textContent = error.message;
      setGlobal(error.message, true);
    } finally {
      setHardwareBusy(false);
    }
  });

  pollStatus();
})();
)JS";

WebGuiAssets get_ppwebgui_assets() {
  return {index_html, app_css, app_js};
}
