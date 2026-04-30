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
      <div class="meta">Profile menus expose the standard `pll_rules.hh` presets. Non-preset frequency strings are resolved with the same strict calculator as `pllcalc`. If `ppwebgui` starts from a nonstandard current profile, that exact value is shown so the UI stays honest. If startup used an unmanaged/raw source selector, the current source stays read-only until you explicitly apply `int_clk` or `ext_clk` here. `ext_clk` follows the external-source path of the currently loaded bitstream.</div>
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
      <div class="panel-heading">
        <h2>Timeline Composer</h2>
        <span class="state-tag neutral-tag">browser only</span>
      </div>
      <div class="panel-note">Build a simple pulse timeline in raw cycles or absolute time units, then generate ordinary PulsePins sequence text for the stream form below. Final output is still restored by ppwebgui from the current tracked qout.</div>
      <div id="timeline-state" class="form-state">Edit channels and pulses, then generate sequence text.</div>

      <div class="subpanel">
        <div class="settings-grid">
          <label>Pulse time unit
            <select id="timeline-time-unit">
              <option value="cycles">cycles</option>
              <option value="ns">ns</option>
              <option value="us">us</option>
              <option value="ms">ms</option>
              <option value="s">s</option>
            </select>
          </label>
          <label>Preview click duration
            <input id="timeline-click-duration" value="10" inputmode="numeric">
          </label>
          <div class="setting"><div class="label">Clock used for time units</div><div id="timeline-clock-note" class="mono"></div></div>
        </div>
      </div>

      <div class="subpanel">
        <div class="panel-heading">
          <h3>Channels</h3>
          <button id="timeline-add-channel" type="button" class="secondary-button">Add channel</button>
        </div>
        <div class="table-wrap">
          <table class="timeline-table">
            <thead><tr><th>Name</th><th>Output bit</th><th>Color</th><th></th></tr></thead>
            <tbody id="timeline-channel-body"></tbody>
          </table>
        </div>
      </div>

      <div class="subpanel">
        <div class="panel-heading">
          <h3>Pulses</h3>
          <button id="timeline-add-pulse" type="button" class="secondary-button">Add pulse</button>
        </div>
        <div class="table-wrap">
          <table class="timeline-table">
            <thead><tr><th>Channel</th><th id="timeline-start-heading">Start cycle</th><th id="timeline-duration-heading">Duration cycles</th><th></th></tr></thead>
            <tbody id="timeline-pulse-body"></tbody>
          </table>
        </div>
      </div>

      <div class="timeline-actions">
        <button id="timeline-generate" type="button">Generate sequence text</button>
        <button id="timeline-load-example" type="button" class="secondary-button">Load example</button>
      </div>

      <div class="subpanel">
        <div class="panel-heading">
          <h3>Draft JSON</h3>
          <div class="timeline-actions">
            <button id="timeline-export-draft" type="button" class="secondary-button">Export draft</button>
            <button id="timeline-import-draft" type="button" class="secondary-button">Import draft</button>
          </div>
        </div>
        <label>Timeline draft
          <textarea id="timeline-draft-json" rows="6" placeholder="Export a timeline draft here, or paste a saved draft and import it."></textarea>
        </label>
      </div>

      <div class="subpanel">
        <div class="panel-heading">
          <h3>Pulse CSV</h3>
          <div class="timeline-actions">
            <button id="timeline-export-csv" type="button" class="secondary-button">Export CSV</button>
            <button id="timeline-import-csv" type="button" class="secondary-button">Import CSV</button>
          </div>
        </div>
        <label>Pulse table
          <textarea id="timeline-csv" rows="6" placeholder="channel,bit,start,duration,color"></textarea>
        </label>
        <div class="meta">CSV uses the currently selected pulse time unit. Import creates channels from the CSV rows.</div>
      </div>

      <div id="timeline-summary" class="meta mono"></div>
      <svg id="timeline-preview" class="timeline-preview" viewBox="0 0 900 180" role="img" aria-label="Timeline preview"></svg>
      <div class="meta">Click a preview lane to add a pulse at that time using the preview click duration above.</div>
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

.timeline-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 0.75rem;
  margin-bottom: 0.75rem;
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

.table-wrap {
  overflow-x: auto;
}

.timeline-table {
  width: 100%;
  border-collapse: collapse;
}

.timeline-table th, .timeline-table td {
  padding: 0.35rem;
  text-align: left;
  vertical-align: middle;
}

.timeline-table th {
  color: #cbd5e1;
  font-size: 0.85rem;
  font-weight: 600;
}

.timeline-table input, .timeline-table select {
  min-width: 7rem;
}

.timeline-table .narrow-input {
  min-width: 4.5rem;
}

.timeline-preview {
  width: 100%;
  min-height: 180px;
  background: #020617;
  border: 1px solid #334155;
  border-radius: 8px;
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
  const streamTextarea = streamForm.querySelector('[name="sequence_text"]');
  const timelineChannelBody = document.getElementById('timeline-channel-body');
  const timelinePulseBody = document.getElementById('timeline-pulse-body');
  const timelineAddChannelButton = document.getElementById('timeline-add-channel');
  const timelineAddPulseButton = document.getElementById('timeline-add-pulse');
  const timelineGenerateButton = document.getElementById('timeline-generate');
  const timelineLoadExampleButton = document.getElementById('timeline-load-example');
  const timelineState = document.getElementById('timeline-state');
  const timelineSummary = document.getElementById('timeline-summary');
  const timelinePreview = document.getElementById('timeline-preview');
  const timelineExportDraftButton = document.getElementById('timeline-export-draft');
  const timelineImportDraftButton = document.getElementById('timeline-import-draft');
  const timelineDraftTextarea = document.getElementById('timeline-draft-json');
  const timelineExportCsvButton = document.getElementById('timeline-export-csv');
  const timelineImportCsvButton = document.getElementById('timeline-import-csv');
  const timelineCsvTextarea = document.getElementById('timeline-csv');
  const timelineTimeUnitSelect = document.getElementById('timeline-time-unit');
  const timelineClickDurationInput = document.getElementById('timeline-click-duration');
  const timelineClockNote = document.getElementById('timeline-clock-note');
  const timelineStartHeading = document.getElementById('timeline-start-heading');
  const timelineDurationHeading = document.getElementById('timeline-duration-heading');
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
  let timelineNextChannelId = 1;
  let timelineNextPulseId = 1;
  let timelineChannels = [];
  let timelinePulses = [];
  let timelinePreviewGeometry = null;
  let timelineHoverElements = [];
  const timelinePalette = ['#38bdf8', '#f97316', '#a78bfa', '#22c55e', '#f43f5e', '#facc15', '#14b8a6', '#fb7185'];
  const timelineTimeUnits = {
    cycles: { label: 'cycles', scale: null },
    ns: { label: 'ns', scale: 1000000000n },
    us: { label: 'us', scale: 1000000n },
    ms: { label: 'ms', scale: 1000n },
    s: { label: 's', scale: 1n },
  };
  const pllProfileTriplets = new Map([
    ['100M', [5n, 20n, 2n]],
    ['80M', [3n, 24n, 5n]],
    ['75M', [5n, 30n, 4n]],
    ['60M', [5n, 30n, 5n]],
    ['50M', [5n, 30n, 6n]],
    ['40M', [5n, 20n, 5n]],
    ['25M', [10n, 30n, 6n]],
    ['30M', [10n, 30n, 5n]],
    ['20M', [10n, 20n, 5n]],
    ['10M', [10n, 20n, 10n]],
    ['5M', [20n, 20n, 10n]],
    ['1M', [50n, 20n, 20n]],
    ['100k', [100n, 20n, 100n]],
    ['10k', [500n, 20n, 200n]],
    ['lj', [1n, 20n, 10n]],
    ['ilj', [1n, 17n, 13n]],
    ['ih', [3n, 71n, 13n]],
    ['il', [5n, 79n, 17n]],
    ['i2h', [7n, 223n, 17n]],
    ['i2l', [9n, 271n, 23n]],
  ]);

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
    timelineTimeUnitSelect.disabled = busy;
    timelineClickDurationInput.disabled = busy;
    for (const button of [timelineAddChannelButton, timelineAddPulseButton, timelineGenerateButton, timelineLoadExampleButton, timelineExportDraftButton, timelineImportDraftButton, timelineExportCsvButton, timelineImportCsvButton]) {
      button.disabled = busy;
    }
  }

  function compareBigInt(a, b) {
    return a < b ? -1 : a > b ? 1 : 0;
  }

  function trackedQoutBigInt() {
    if (!lastStatus || !lastStatus.streamer) {
      return null;
    }
    const value = lastStatus.streamer.qout_streamer;
    try {
      if (typeof value === 'string') {
        return BigInt(value);
      }
      return BigInt(Number(value) >>> 0);
    } catch (error) {
      return null;
    }
  }

  function hexBigInt(value) {
    return `0x${value.toString(16)}`;
  }

  function decimalRational(text) {
    const match = String(text).trim().match(/^(?:(\d+)(?:\.(\d*))?|\.(\d+))$/);
    if (!match) {
      return null;
    }
    const whole = match[1] || '0';
    const fraction = match[2] !== undefined ? match[2] : (match[3] || '');
    const digits = `${whole}${fraction}`.replace(/^0+(?=\d)/, '') || '0';
    return { numerator: BigInt(digits), denominator: 10n ** BigInt(fraction.length) };
  }

  function roundDivide(numerator, denominator) {
    return (numerator + denominator / 2n) / denominator;
  }

  function parseFrequencyText(profile) {
    const match = String(profile || '').trim().match(/^(\d+(?:\.\d*)?|\.\d+)\s*([A-Za-z]+)$/);
    if (!match) {
      return null;
    }
    const value = decimalRational(match[1]);
    if (!value || value.numerator <= 0n) {
      return null;
    }
    const rawUnit = match[2];
    const unit = rawUnit.toLowerCase();
    let scaleNum = 0n;
    let scaleDen = 1n;
    if (rawUnit === 'M' || unit === 'mhz') {
      scaleNum = 1000000n;
    } else if (unit === 'g' || unit === 'ghz') {
      scaleNum = 1000000000n;
    } else if (unit === 'k' || unit === 'khz') {
      scaleNum = 1000n;
    } else if (unit === 'hz') {
      scaleNum = 1n;
    } else if (unit === 'm') {
      scaleNum = 1n;
      scaleDen = 1000n;
    } else if (unit === 'u') {
      scaleNum = 1n;
      scaleDen = 1000000n;
    }
    if (scaleNum === 0n) {
      return null;
    }
    return { num: value.numerator * scaleNum, den: value.denominator * scaleDen };
  }

  function pllProfileClock(profile) {
    const profileText = String(profile || '').trim();
    const triplet = pllProfileTriplets.get(profileText);
    const rawText = triplet ? triplet.join(',') : profileText;
    const raw = rawText.match(/^(\d+)\s*,\s*(\d+)\s*,\s*(\d+)$/);
    if (raw) {
      const n = BigInt(raw[1]);
      const m = BigInt(raw[2]);
      const c = BigInt(raw[3]);
      if (n > 0n && m > 0n && c > 0n) {
        return { num: 50000000n * m, den: n * c };
      }
    }
    return parseFrequencyText(profileText);
  }

  function formatClockHz(clock) {
    const hz = Number(clock.num) / Number(clock.den);
    const exact = clock.den === 1n ? `${clock.num.toString()} Hz` : `${clock.num.toString()}/${clock.den.toString()} Hz`;
    return `${formatFrequencyHz(hz)} (${exact})`;
  }

  function roundedLabClockHz(value) {
    const measured = Number(value);
    if (!Number.isFinite(measured) || measured <= 0) {
      return null;
    }
    const order = Math.floor(Math.log10(measured));
    for (let exponent = order; exponent >= 0; --exponent) {
      const step = 10 ** exponent;
      const rounded = Math.round(measured / step) * step;
      if (rounded > 0 && Math.abs(rounded - measured) / measured <= 0.001) {
        return Math.round(rounded);
      }
    }
    return Math.round(measured);
  }

  function resolveTimelineClock() {
    if (!lastStatus || !lastStatus.clocking) {
      return { ok: false, message: 'Waiting for ppwebgui status before resolving absolute time units' };
    }
    const tracked = lastStatus.clocking.tracked;
    const measured = lastStatus.clocking.measured;
    if (tracked.source === 'int_clk') {
      const clock = pllProfileClock(tracked.int_profile);
      if (!clock) {
        return { ok: false, message: `Cannot resolve nominal int_clk profile ${tracked.int_profile}` };
      }
      return { ok: true, clock, message: `int_clk ${tracked.int_profile} nominal ${formatClockHz(clock)}` };
    }
    if (tracked.source === 'ext_clk') {
      const rounded = roundedLabClockHz(measured.ext_clk_hz);
      if (rounded === null) {
        return { ok: false, message: 'ext_clk has not been measured yet' };
      }
      const clock = { num: BigInt(rounded), den: 1n };
      return { ok: true, clock, message: `ext_clk measured ${formatFrequencyHz(measured.ext_clk_hz)}, rounded to ${formatClockHz(clock)}` };
    }
    const rounded = roundedLabClockHz(measured.streamer_clk_hz);
    if (rounded === null) {
      return { ok: false, message: 'Streamer clock has not been measured yet' };
    }
    const clock = { num: BigInt(rounded), den: 1n };
    return { ok: true, clock, message: `unmanaged source ${tracked.source_display}, streamer clock rounded to ${formatClockHz(clock)}` };
  }

  function timelineTimeContext(errors = null) {
    const selected = timelineTimeUnits[timelineTimeUnitSelect.value] ? timelineTimeUnitSelect.value : 'cycles';
    const unit = timelineTimeUnits[selected];
    if (selected === 'cycles') {
      return { ok: true, unit: selected, unitLabel: unit.label, unitScale: null, clock: null, clockText: 'Raw cycle counts; no clock conversion.' };
    }
    const resolved = resolveTimelineClock();
    if (!resolved.ok) {
      if (errors) {
        errors.push(resolved.message);
      }
      return { ok: false, unit: selected, unitLabel: unit.label, unitScale: unit.scale, clock: null, clockText: resolved.message };
    }
    return { ok: true, unit: selected, unitLabel: unit.label, unitScale: unit.scale, clock: resolved.clock, clockText: resolved.message };
  }

  function updateTimelineUnitUi() {
    const context = timelineTimeContext();
    timelineStartHeading.textContent = context.unit === 'cycles' ? 'Start cycle' : `Start (${context.unitLabel})`;
    timelineDurationHeading.textContent = context.unit === 'cycles' ? 'Duration cycles' : `Duration (${context.unitLabel})`;
    timelineClickDurationInput.inputMode = context.unit === 'cycles' ? 'numeric' : 'decimal';
    timelineClockNote.textContent = context.clockText;
    timelineClockNote.classList.toggle('warning-text', !context.ok);
  }

  function decimalText(value) {
    if (!Number.isFinite(value)) {
      return '';
    }
    if (Math.abs(value) >= 1000000000) {
      return value.toFixed(0);
    }
    return value.toFixed(9).replace(/\.?0+$/, '') || '0';
  }

  function formatTimelineDuration(cycles, context) {
    if (!context || !context.clock || !context.unitScale) {
      return `${cycles.toString()} cycles`;
    }
    const value = Number(cycles) * Number(context.clock.den) * Number(context.unitScale) / Number(context.clock.num);
    const formatted = Number.isFinite(value) ? value.toLocaleString(undefined, { maximumFractionDigits: 6 }) : '(too large)';
    return `${cycles.toString()} cycles (${formatted} ${context.unitLabel})`;
  }

  function timelineInputFromCycles(cycles, context) {
    if (!context || !context.clock || !context.unitScale) {
      return cycles.toString();
    }
    const value = Number(cycles) * Number(context.clock.den) * Number(context.unitScale) / Number(context.clock.num);
    return decimalText(value);
  }

  function setTimelineState(message, isError = false) {
    timelineState.textContent = message;
    timelineState.classList.toggle('local-edit', isError);
  }

  function svgElement(tag, attrs = {}) {
    const element = document.createElementNS('http://www.w3.org/2000/svg', tag);
    for (const [name, value] of Object.entries(attrs)) {
      element.setAttribute(name, value);
    }
    return element;
  }

  function appendSvgText(parent, x, y, text, attrs = {}) {
    const element = svgElement('text', { x, y, fill: '#cbd5e1', 'font-size': '12', ...attrs });
    element.textContent = text;
    parent.appendChild(element);
  }

  function nextTimelineColor() {
    return timelinePalette[timelineChannels.length % timelinePalette.length];
  }

  function resetTimelineState() {
    timelineNextChannelId = 1;
    timelineNextPulseId = 1;
    timelineChannels = [];
    timelinePulses = [];
  }

  function addTimelineChannel(name = `CH${timelineNextChannelId}`, bit = timelineChannels.length, color = nextTimelineColor()) {
    timelineChannels.push({ id: `ch${timelineNextChannelId++}`, name, bit: String(bit), color });
  }

  function addTimelinePulse(channelId = '', start = 0, duration = 10) {
    const selectedChannel = channelId || (timelineChannels[0] ? timelineChannels[0].id : '');
    timelinePulses.push({ id: `pulse${timelineNextPulseId++}`, channelId: selectedChannel, start: String(start), duration: String(duration) });
  }

  function makeDeleteButton(label, onClick) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'secondary-button';
    button.textContent = label;
    button.addEventListener('click', onClick);
    return button;
  }

  function renderTimelineChannelRows() {
    timelineChannelBody.textContent = '';
    for (const channel of timelineChannels) {
      const row = document.createElement('tr');

      const nameCell = document.createElement('td');
      const nameInput = document.createElement('input');
      nameInput.value = channel.name;
      nameInput.addEventListener('input', () => {
        channel.name = nameInput.value;
        renderTimelinePulseRows();
        renderTimelinePreview();
      });
      nameCell.appendChild(nameInput);
      row.appendChild(nameCell);

      const bitCell = document.createElement('td');
      const bitInput = document.createElement('input');
      bitInput.className = 'narrow-input';
      bitInput.inputMode = 'numeric';
      bitInput.value = channel.bit;
      bitInput.addEventListener('input', () => {
        channel.bit = bitInput.value;
        renderTimelinePreview();
      });
      bitCell.appendChild(bitInput);
      row.appendChild(bitCell);

      const colorCell = document.createElement('td');
      const colorInput = document.createElement('input');
      colorInput.type = 'color';
      colorInput.className = 'narrow-input';
      colorInput.value = channel.color;
      colorInput.addEventListener('input', () => {
        channel.color = colorInput.value;
        renderTimelinePreview();
      });
      colorCell.appendChild(colorInput);
      row.appendChild(colorCell);

      const deleteCell = document.createElement('td');
      deleteCell.appendChild(makeDeleteButton('Delete', () => {
        timelineChannels = timelineChannels.filter((candidate) => candidate.id !== channel.id);
        timelinePulses = timelinePulses.filter((pulse) => pulse.channelId !== channel.id);
        renderTimelineTables();
      }));
      row.appendChild(deleteCell);

      timelineChannelBody.appendChild(row);
    }
  }

  function renderTimelinePulseRows() {
    timelinePulseBody.textContent = '';
    const decimalInput = timelineTimeUnitSelect.value !== 'cycles';
    for (const pulse of timelinePulses) {
      const row = document.createElement('tr');

      const channelCell = document.createElement('td');
      const channelSelect = document.createElement('select');
      for (const channel of timelineChannels) {
        const option = document.createElement('option');
        option.value = channel.id;
        option.textContent = `${channel.name || channel.id} [bit ${channel.bit}]`;
        channelSelect.appendChild(option);
      }
      channelSelect.value = pulse.channelId;
      channelSelect.addEventListener('change', () => {
        pulse.channelId = channelSelect.value;
        renderTimelinePreview();
      });
      channelCell.appendChild(channelSelect);
      row.appendChild(channelCell);

      const startCell = document.createElement('td');
      const startInput = document.createElement('input');
      startInput.inputMode = decimalInput ? 'decimal' : 'numeric';
      startInput.value = pulse.start;
      startInput.addEventListener('input', () => {
        pulse.start = startInput.value;
        renderTimelinePreview();
      });
      startCell.appendChild(startInput);
      row.appendChild(startCell);

      const durationCell = document.createElement('td');
      const durationInput = document.createElement('input');
      durationInput.inputMode = decimalInput ? 'decimal' : 'numeric';
      durationInput.value = pulse.duration;
      durationInput.addEventListener('input', () => {
        pulse.duration = durationInput.value;
        renderTimelinePreview();
      });
      durationCell.appendChild(durationInput);
      row.appendChild(durationCell);

      const deleteCell = document.createElement('td');
      deleteCell.appendChild(makeDeleteButton('Delete', () => {
        timelinePulses = timelinePulses.filter((candidate) => candidate.id !== pulse.id);
        renderTimelineTables();
      }));
      row.appendChild(deleteCell);

      timelinePulseBody.appendChild(row);
    }
  }

  function parseCycleCount(value, label, errors, positive) {
    const text = String(value).trim();
    if (!/^-?\d+$/.test(text)) {
      errors.push(`${label} must be an integer cycle count`);
      return null;
    }
    const parsed = BigInt(text);
    if (positive ? parsed <= 0n : parsed < 0n) {
      errors.push(`${label} must be ${positive ? 'greater than zero' : 'non-negative'}`);
      return null;
    }
    return parsed;
  }

  function parseTimelineCount(value, label, errors, positive, context) {
    if (!context || context.unit === 'cycles') {
      return parseCycleCount(value, label, errors, positive);
    }
    const text = String(value).trim();
    const parsed = decimalRational(text);
    if (!parsed) {
      errors.push(`${label} must be a decimal ${context.unitLabel} value`);
      return null;
    }
    if (positive ? parsed.numerator <= 0n : parsed.numerator < 0n) {
      errors.push(`${label} must be ${positive ? 'greater than zero' : 'non-negative'}`);
      return null;
    }
    if (!context.ok || !context.clock) {
      return null;
    }
    const numerator = parsed.numerator * context.clock.num;
    const denominator = parsed.denominator * context.unitScale * context.clock.den;
    const cycles = roundDivide(numerator, denominator);
    if (positive && cycles <= 0n) {
      errors.push(`${label} rounds to 0 cycles at ${context.clockText}`);
      return null;
    }
    return cycles;
  }

  function validateTimeline() {
    const errors = [];
    const channels = [];
    const channelById = new Map();
    const bits = new Map();
    let ownedMask = 0n;
    const timeContext = timelineTimeContext(errors);

    if (timelineChannels.length === 0) {
      errors.push('Add at least one channel');
    }
    for (const channel of timelineChannels) {
      const name = channel.name.trim();
      if (!name) {
        errors.push('Channel name must not be empty');
      }
      const bitText = String(channel.bit).trim();
      if (!/^\d+$/.test(bitText)) {
        errors.push(`Channel ${name || channel.id} bit must be an integer in 0..31`);
        continue;
      }
      const bit = Number(bitText);
      if (!Number.isInteger(bit) || bit < 0 || bit > 31) {
        errors.push(`Channel ${name || channel.id} bit must be in 0..31`);
        continue;
      }
      if (bits.has(bit)) {
        errors.push(`Output bit ${bit} is assigned to both ${bits.get(bit)} and ${name || channel.id}`);
        continue;
      }
      bits.set(bit, name || channel.id);
      const parsed = { id: channel.id, name: name || channel.id, bit, color: channel.color };
      channels.push(parsed);
      channelById.set(channel.id, parsed);
      ownedMask |= 1n << BigInt(bit);
    }

    if (timelinePulses.length === 0) {
      errors.push('Add at least one pulse');
    }
    const pulses = [];
    for (const pulse of timelinePulses) {
      const channel = channelById.get(pulse.channelId);
      if (!channel) {
        errors.push('Pulse references a missing channel');
        continue;
      }
      const start = parseTimelineCount(pulse.start, `${channel.name} start`, errors, false, timeContext);
      const duration = parseTimelineCount(pulse.duration, `${channel.name} duration`, errors, true, timeContext);
      if (start === null || duration === null) {
        continue;
      }
      pulses.push({ id: pulse.id, channel, start, duration, end: start + duration });
    }

    const pulsesByChannel = new Map();
    for (const pulse of pulses) {
      if (!pulsesByChannel.has(pulse.channel.id)) {
        pulsesByChannel.set(pulse.channel.id, []);
      }
      pulsesByChannel.get(pulse.channel.id).push(pulse);
    }
    for (const entries of pulsesByChannel.values()) {
      entries.sort((a, b) => compareBigInt(a.start, b.start));
      for (let i = 1; i < entries.length; ++i) {
        if (entries[i].start < entries[i - 1].end) {
          errors.push(`${entries[i].channel.name} pulses overlap at cycle ${entries[i].start.toString()}`);
        }
      }
    }

    return { errors, channels, pulses, ownedMask, timeContext };
  }

  function compileTimeline() {
    const parsed = validateTimeline();
    if (parsed.errors.length) {
      return { ok: false, errors: parsed.errors, parsed };
    }
    const trackedQout = trackedQoutBigInt();
    if (trackedQout === null) {
      return { ok: false, errors: ['Waiting for ppwebgui status before compiling timeline'], parsed };
    }

    const events = new Map();
    const cycleKeys = new Set(['0']);
    const ensureEvent = (cycle) => {
      const key = cycle.toString();
      if (!events.has(key)) {
        events.set(key, { setMask: 0n, clearMask: 0n });
      }
      cycleKeys.add(key);
      return events.get(key);
    };

    for (const pulse of parsed.pulses) {
      const bitMask = 1n << BigInt(pulse.channel.bit);
      ensureEvent(pulse.start).setMask |= bitMask;
      ensureEvent(pulse.end).clearMask |= bitMask;
    }

    const cycles = Array.from(cycleKeys, (key) => BigInt(key)).sort(compareBigInt);
    const baseline = trackedQout & ~parsed.ownedMask;
    let activeMask = 0n;
    const records = [];
    for (let i = 0; i + 1 < cycles.length; ++i) {
      const cycle = cycles[i];
      const event = events.get(cycle.toString());
      if (event) {
        activeMask &= ~event.clearMask;
        activeMask |= event.setMask;
      }
      const count = cycles[i + 1] - cycle;
      if (count <= 0n) {
        continue;
      }
      const value = baseline | activeMask;
      const previous = records[records.length - 1];
      if (previous && previous.value === value) {
        previous.count += count;
      } else {
        records.push({ count, value });
      }
    }

    return { ok: true, parsed, records, totalCycles: cycles[cycles.length - 1], baseline };
  }

  function timelineRatio(cycle, totalCycles) {
    if (totalCycles <= BigInt(Number.MAX_SAFE_INTEGER)) {
      return Number(cycle) / Math.max(1, Number(totalCycles));
    }
    const scale = 1000000n;
    return Number((cycle * scale) / totalCycles) / Number(scale);
  }

  function timelineCycleFromX(x, geometry) {
    const raw = Math.max(0, Math.min(1, (x - geometry.plotX) / geometry.plotW));
    const scale = 1000000n;
    return (geometry.totalCycles * BigInt(Math.round(raw * Number(scale)))) / scale;
  }

  function timelineSvgPoint(event) {
    const rect = timelinePreview.getBoundingClientRect();
    const viewBox = timelinePreview.viewBox.baseVal;
    if (rect.width <= 0 || rect.height <= 0) {
      return null;
    }
    return {
      x: viewBox.x + ((event.clientX - rect.left) / rect.width) * viewBox.width,
      y: viewBox.y + ((event.clientY - rect.top) / rect.height) * viewBox.height,
    };
  }

  function timelinePreviewTarget(event) {
    const geometry = timelinePreviewGeometry;
    if (!geometry || !geometry.channels.length || hardwareBusy) {
      return null;
    }
    const point = timelineSvgPoint(event);
    if (!point || point.x < geometry.plotX || point.x > geometry.plotX + geometry.plotW) {
      return null;
    }
    const channelIndex = Math.floor((point.y - (geometry.plotY - 16)) / geometry.rowH);
    if (channelIndex < 0 || channelIndex >= geometry.channels.length) {
      return null;
    }
    const startCycle = timelineCycleFromX(point.x, geometry);
    return {
      geometry,
      point,
      channelIndex,
      channel: geometry.channels[channelIndex],
      startCycle,
      start: timelineInputFromCycles(startCycle, geometry.context),
      duration: timelineClickDurationInput.value.trim() || '10',
    };
  }

  function clearTimelineHover() {
    for (const element of timelineHoverElements) {
      element.remove();
    }
    timelineHoverElements = [];
    timelinePreview.style.cursor = '';
  }

  function appendTimelineHover(element) {
    element.setAttribute('pointer-events', 'none');
    timelineHoverElements.push(element);
    timelinePreview.appendChild(element);
  }

  function previewDurationCycles(target) {
    const errors = [];
    return parseTimelineCount(target.duration, 'Preview click duration', errors, true, target.geometry.context);
  }

  function renderTimelineHover(event) {
    clearTimelineHover();
    const target = timelinePreviewTarget(event);
    if (!target) {
      return;
    }
    const { geometry, channelIndex, channel, startCycle } = target;
    const y = geometry.plotY + channelIndex * geometry.rowH;
    appendTimelineHover(svgElement('rect', { x: geometry.plotX, y: y - 14, width: geometry.plotW, height: 34, rx: 6, fill: channel.color, opacity: 0.14 }));
    appendTimelineHover(svgElement('line', { x1: target.point.x, y1: y - 14, x2: target.point.x, y2: y + 20, stroke: '#e2e8f0', 'stroke-width': 1, 'stroke-dasharray': '3 3' }));
    const durationCycles = previewDurationCycles(target);
    if (durationCycles === null) {
      timelinePreview.style.cursor = 'not-allowed';
      return;
    }
    const x = geometry.plotX + timelineRatio(startCycle, geometry.totalCycles) * geometry.plotW;
    const remaining = Math.max(1, geometry.plotX + geometry.plotW - x);
    const width = Math.min(remaining, Math.max(1, timelineRatio(durationCycles, geometry.totalCycles) * geometry.plotW));
    appendTimelineHover(svgElement('rect', { x, y: y - 4, width, height: 22, rx: 4, fill: channel.color, opacity: 0.35, stroke: '#e2e8f0', 'stroke-width': 1, 'stroke-dasharray': '4 3' }));
    timelinePreview.style.cursor = 'crosshair';
  }

  function addPulseFromTimelinePreview(event) {
    const target = timelinePreviewTarget(event);
    if (!target) {
      return;
    }
    if (previewDurationCycles(target) === null) {
      setTimelineState(`Preview click duration is invalid for ${target.geometry.context.unitLabel}.`, true);
      return;
    }
    addTimelinePulse(target.channel.id, target.start, target.duration);
    renderTimelineTables();
    if (compileTimeline().ok) {
      setTimelineState(`Added ${target.duration} ${target.geometry.context.unitLabel} pulse on ${target.channel.name} at ${target.start} ${target.geometry.context.unitLabel}.`, false);
    }
  }

  function renderTimelinePreview() {
    updateTimelineUnitUi();
    const compiled = compileTimeline();
    timelinePreview.textContent = '';
    timelineHoverElements = [];
    timelinePreview.style.cursor = '';
    if (!compiled.ok) {
      timelineSummary.textContent = '';
      timelinePreviewGeometry = null;
      setTimelineState(compiled.errors.join('; '), true);
      timelinePreview.setAttribute('viewBox', '0 0 900 180');
      appendSvgText(timelinePreview, 24, 42, 'Timeline has validation errors', { fill: '#fca5a5', 'font-size': '16' });
      return;
    }

    const height = Math.max(140, 54 + compiled.parsed.channels.length * 42);
    timelinePreview.setAttribute('viewBox', `0 0 900 ${height}`);
    timelinePreview.appendChild(svgElement('rect', { x: 0, y: 0, width: 900, height, fill: '#020617' }));
    const plotX = 150;
    const plotY = 34;
    const plotW = 720;
    const rowH = 42;
    const totalCycles = compiled.totalCycles;
    const previewLimited = totalCycles > BigInt(Number.MAX_SAFE_INTEGER);
    timelinePreviewGeometry = { plotX, plotY, plotW, rowH, totalCycles, context: compiled.parsed.timeContext, channels: compiled.parsed.channels };

    appendSvgText(timelinePreview, plotX, 22, '0 cycles', { fill: '#94a3b8' });
    appendSvgText(timelinePreview, plotX + plotW - 170, 22, formatTimelineDuration(totalCycles, compiled.parsed.timeContext), { fill: '#94a3b8' });
    for (let i = 0; i < compiled.parsed.channels.length; ++i) {
      const channel = compiled.parsed.channels[i];
      const y = plotY + i * rowH;
      appendSvgText(timelinePreview, 18, y + 16, `${channel.name} [${channel.bit}]`, { fill: channel.color });
      timelinePreview.appendChild(svgElement('line', { x1: plotX, y1: y + 10, x2: plotX + plotW, y2: y + 10, stroke: '#334155', 'stroke-width': 2 }));
      for (const pulse of compiled.parsed.pulses.filter((candidate) => candidate.channel.id === channel.id)) {
        const x = plotX + timelineRatio(pulse.start, totalCycles) * plotW;
        const width = Math.max(1, timelineRatio(pulse.duration, totalCycles) * plotW);
        timelinePreview.appendChild(svgElement('rect', { x, y: y - 4, width, height: 22, rx: 4, fill: channel.color, opacity: 0.85 }));
      }
    }
    const context = compiled.parsed.timeContext;
    const unitSummary = context.unit === 'cycles' ? 'unit=cycles' : `unit=${context.unitLabel} clock=${formatClockHz(context.clock)}`;
    timelineSummary.textContent = `records=${compiled.records.length} total=${formatTimelineDuration(compiled.totalCycles, context)} owned_mask=${hexBigInt(compiled.parsed.ownedMask)} baseline=${hexBigInt(compiled.baseline)} ${unitSummary}`;
    setTimelineState(previewLimited ? 'Timeline is valid; preview is approximate because the cycle range is very large.' : `Timeline is valid in ${context.unitLabel}. Generate sequence text when ready.`, false);
  }

  function renderTimelineTables() {
    renderTimelineChannelRows();
    renderTimelinePulseRows();
    renderTimelinePreview();
  }

  function loadTimelineExample() {
    resetTimelineState();
    addTimelineChannel('Laser', 0, '#38bdf8');
    addTimelineChannel('RF', 1, '#f97316');
    addTimelineChannel('Camera', 2, '#a78bfa');
    addTimelinePulse(timelineChannels[0].id, 100, 50);
    addTimelinePulse(timelineChannels[1].id, 200, 75);
    addTimelinePulse(timelineChannels[2].id, 90, 250);
    renderTimelineTables();
  }

  function timelineDraft() {
    const channelIndexById = new Map();
    timelineChannels.forEach((channel, index) => channelIndexById.set(channel.id, index));
    return {
      format: 'pulsepins.timeline',
      version: 1,
      time_unit: timelineTimeUnitSelect.value,
      channels: timelineChannels.map((channel) => ({
        name: channel.name,
        bit: channel.bit,
        color: channel.color,
      })),
      pulses: timelinePulses.map((pulse) => ({
        channel_index: channelIndexById.has(pulse.channelId) ? channelIndexById.get(pulse.channelId) : -1,
        start: pulse.start,
        duration: pulse.duration,
      })),
    };
  }

  function exportTimelineDraft() {
    timelineDraftTextarea.value = JSON.stringify(timelineDraft(), null, 2);
    setTimelineState(`Exported draft JSON with ${timelineChannels.length} channels and ${timelinePulses.length} pulses.`, false);
  }

  function draftIndex(value, label) {
    const text = String(value).trim();
    if (!/^\d+$/.test(text)) {
      throw new Error(`${label} must be a non-negative integer`);
    }
    const index = Number(text);
    if (!Number.isSafeInteger(index)) {
      throw new Error(`${label} is too large`);
    }
    return index;
  }

  function draftColor(value, index) {
    const text = String(value || '');
    return /^#[0-9a-fA-F]{6}$/.test(text) ? text : timelinePalette[index % timelinePalette.length];
  }

  function importTimelineDraft() {
    const previous = {
      unit: timelineTimeUnitSelect.value,
      nextChannelId: timelineNextChannelId,
      nextPulseId: timelineNextPulseId,
      channels: timelineChannels,
      pulses: timelinePulses,
    };
    try {
      const draft = JSON.parse(timelineDraftTextarea.value);
      if (!draft || typeof draft !== 'object' || Array.isArray(draft)) {
        throw new Error('Draft must be a JSON object');
      }
      const unit = String(draft.time_unit || draft.unit || 'cycles');
      if (!timelineTimeUnits[unit]) {
        throw new Error(`Unsupported draft time unit: ${unit}`);
      }
      if (!Array.isArray(draft.channels)) {
        throw new Error('Draft channels must be an array');
      }
      if (!Array.isArray(draft.pulses)) {
        throw new Error('Draft pulses must be an array');
      }

      resetTimelineState();
      timelineTimeUnitSelect.value = unit;
      draft.channels.forEach((channel, index) => {
        if (!channel || typeof channel !== 'object' || Array.isArray(channel)) {
          throw new Error(`Channel ${index} must be an object`);
        }
        const name = channel.name === undefined ? `CH${index + 1}` : String(channel.name);
        const bit = channel.bit === undefined ? String(index) : String(channel.bit);
        addTimelineChannel(name, bit, draftColor(channel.color, index));
      });
      draft.pulses.forEach((pulse, index) => {
        if (!pulse || typeof pulse !== 'object' || Array.isArray(pulse)) {
          throw new Error(`Pulse ${index} must be an object`);
        }
        const rawChannelIndex = pulse.channel_index === undefined ? pulse.channel : pulse.channel_index;
        const channelIndex = draftIndex(rawChannelIndex, `Pulse ${index} channel_index`);
        if (channelIndex >= timelineChannels.length) {
          throw new Error(`Pulse ${index} channel_index is outside the channel list`);
        }
        const start = pulse.start === undefined ? '0' : String(pulse.start);
        const duration = pulse.duration === undefined ? '10' : String(pulse.duration);
        addTimelinePulse(timelineChannels[channelIndex].id, start, duration);
      });
      renderTimelineTables();
      setTimelineState(`Imported draft JSON with ${timelineChannels.length} channels and ${timelinePulses.length} pulses.`, false);
    } catch (error) {
      timelineTimeUnitSelect.value = previous.unit;
      timelineNextChannelId = previous.nextChannelId;
      timelineNextPulseId = previous.nextPulseId;
      timelineChannels = previous.channels;
      timelinePulses = previous.pulses;
      renderTimelineTables();
      setTimelineState(`Could not import draft JSON: ${error.message}`, true);
    }
  }

  function csvEscape(value) {
    const text = String(value);
    if (/[",\r\n]/.test(text)) {
      return `"${text.replace(/"/g, '""')}"`;
    }
    return text;
  }

  function parseCsvRows(text) {
    const rows = [];
    let row = [];
    let field = '';
    let quoted = false;
    for (let i = 0; i < text.length; ++i) {
      const ch = text[i];
      if (quoted) {
        if (ch === '"') {
          if (text[i + 1] === '"') {
            field += '"';
            ++i;
          } else {
            quoted = false;
          }
        } else {
          field += ch;
        }
        continue;
      }
      if (ch === '"') {
        if (field.length !== 0) {
          throw new Error('Unexpected quote in CSV field');
        }
        quoted = true;
      } else if (ch === ',') {
        row.push(field);
        field = '';
      } else if (ch === '\n' || ch === '\r') {
        if (ch === '\r' && text[i + 1] === '\n') {
          ++i;
        }
        row.push(field);
        rows.push(row);
        row = [];
        field = '';
      } else {
        field += ch;
      }
    }
    if (quoted) {
      throw new Error('Unclosed quote in CSV field');
    }
    if (field.length || row.length) {
      row.push(field);
      rows.push(row);
    }
    return rows.filter((candidate) => candidate.some((cell) => String(cell).trim() !== ''));
  }

  function csvHeaderMap(row) {
    const names = row.map((cell) => String(cell).trim().toLowerCase());
    const channel = names.indexOf('channel');
    const start = names.indexOf('start');
    const duration = names.indexOf('duration');
    if (channel < 0 || start < 0 || duration < 0) {
      return null;
    }
    return { channel, bit: names.indexOf('bit'), start, duration, color: names.indexOf('color') };
  }

  function csvCell(row, index) {
    return index >= 0 && index < row.length ? String(row[index]).trim() : '';
  }

  function exportTimelineCsv() {
    const channelById = new Map(timelineChannels.map((channel) => [channel.id, channel]));
    const lines = [['channel', 'bit', 'start', 'duration', 'color'].map(csvEscape).join(',')];
    for (const pulse of timelinePulses) {
      const channel = channelById.get(pulse.channelId);
      if (!channel) {
        continue;
      }
      lines.push([channel.name, channel.bit, pulse.start, pulse.duration, channel.color].map(csvEscape).join(','));
    }
    timelineCsvTextarea.value = `${lines.join('\n')}\n`;
    setTimelineState(`Exported CSV with ${timelinePulses.length} pulse rows using ${timelineTimeUnitSelect.value}.`, false);
  }

  function importTimelineCsv() {
    const previous = {
      nextChannelId: timelineNextChannelId,
      nextPulseId: timelineNextPulseId,
      channels: timelineChannels,
      pulses: timelinePulses,
    };
    try {
      const rows = parseCsvRows(timelineCsvTextarea.value);
      if (rows.length === 0) {
        throw new Error('CSV has no pulse rows');
      }
      const header = csvHeaderMap(rows[0]);
      const firstDataRow = header ? 1 : 0;
      const columns = header || { channel: 0, bit: 1, start: 2, duration: 3, color: 4 };
      const importedChannels = new Map();

      resetTimelineState();
      for (let i = firstDataRow; i < rows.length; ++i) {
        const row = rows[i];
        const channelName = csvCell(row, columns.channel);
        const bit = csvCell(row, columns.bit);
        const start = csvCell(row, columns.start);
        const duration = csvCell(row, columns.duration);
        const color = csvCell(row, columns.color);
        if (!channelName) {
          throw new Error(`CSV row ${i + 1} has an empty channel`);
        }
        if (!bit) {
          throw new Error(`CSV row ${i + 1} has an empty bit`);
        }
        if (!start) {
          throw new Error(`CSV row ${i + 1} has an empty start`);
        }
        if (!duration) {
          throw new Error(`CSV row ${i + 1} has an empty duration`);
        }
        const channelKey = `${channelName}\u0000${bit}`;
        if (!importedChannels.has(channelKey)) {
          addTimelineChannel(channelName, bit, draftColor(color, importedChannels.size));
          importedChannels.set(channelKey, timelineChannels[timelineChannels.length - 1].id);
        }
        addTimelinePulse(importedChannels.get(channelKey), start, duration);
      }
      renderTimelineTables();
      setTimelineState(`Imported CSV with ${timelinePulses.length} pulse rows using ${timelineTimeUnitSelect.value}.`, false);
    } catch (error) {
      timelineNextChannelId = previous.nextChannelId;
      timelineNextPulseId = previous.nextPulseId;
      timelineChannels = previous.channels;
      timelinePulses = previous.pulses;
      renderTimelineTables();
      setTimelineState(`Could not import CSV: ${error.message}`, true);
    }
  }

  function generateTimelineSequence() {
    const compiled = compileTimeline();
    if (!compiled.ok) {
      setTimelineState(compiled.errors.join('; '), true);
      renderTimelinePreview();
      return;
    }
    streamTextarea.value = compiled.records.map((record) => `d ${record.count.toString()} ${hexBigInt(record.value)}`).join('\n') + '\n';
    streamTextarea.dispatchEvent(new Event('input', { bubbles: true }));
    renderTimelinePreview();
    setTimelineState(`Generated ${compiled.records.length} sequence records from ${compiled.parsed.pulses.length} pulses using ${compiled.parsed.timeContext.unitLabel}.`, false);
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
    renderTimelinePreview();
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

  timelineAddChannelButton.addEventListener('click', () => {
    addTimelineChannel();
    renderTimelineTables();
  });

  timelineAddPulseButton.addEventListener('click', () => {
    if (timelineChannels.length === 0) {
      addTimelineChannel();
    }
    addTimelinePulse();
    renderTimelineTables();
  });

  timelineLoadExampleButton.addEventListener('click', () => {
    loadTimelineExample();
  });

  timelineGenerateButton.addEventListener('click', () => {
    generateTimelineSequence();
  });

  timelineExportDraftButton.addEventListener('click', () => {
    exportTimelineDraft();
  });

  timelineImportDraftButton.addEventListener('click', () => {
    importTimelineDraft();
  });

  timelineExportCsvButton.addEventListener('click', () => {
    exportTimelineCsv();
  });

  timelineImportCsvButton.addEventListener('click', () => {
    importTimelineCsv();
  });

  timelinePreview.addEventListener('click', (event) => {
    addPulseFromTimelinePreview(event);
  });

  timelinePreview.addEventListener('mousemove', (event) => {
    renderTimelineHover(event);
  });

  timelinePreview.addEventListener('mouseleave', () => {
    clearTimelineHover();
  });

  timelineTimeUnitSelect.addEventListener('change', () => {
    renderTimelineTables();
  });

  loadTimelineExample();

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
    body.set('sequence_text', streamTextarea.value);
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
