$(document).ready(function () {

  ['clock_color', 'temp_color', 'other_color'].forEach(id => {
    const $el = $('#' + id);
    if ($el.length) {
      $el.minicolors({
        inline: $el.attr('data-inline') === 'true',
        theme: 'bootstrap',
        change: () => refreshColorViews(id)
      });
    }
  });
  $('#autoBrightness').change(() => {
    toggleAutoBrightness();
  });
  getServerValues();
  refreshLight();
  setInterval(() => { if (!document.hidden) refreshLight(); }, 5000);
  $('#chimeDarkThreshold').on('input', showLight);
  let infoTimer = null;
  $('#deviceInfoBody')
    .on('show.bs.collapse', () => {
      getDeviceInfo();
      infoTimer = setInterval(() => { if (!document.hidden) getDeviceInfo(); }, 5000);
    })
    .on('hide.bs.collapse', () => {
      clearInterval(infoTimer);
      infoTimer = null;
    });
});

// minicolors' value setter takes a color string; it ignores {r,g,b} objects.
function rgbToHex(c) {
  return '#' + [c.r, c.g, c.b].map(v => v.toString(16).padStart(2, '0')).join('');
}

// Show a picker's value as text ("#rrggbb  rgb(r, g, b)") and redraw the
// Display Style preview, which uses the picker colors.
function refreshColorViews(id) {
  const ids = id ? [id] : ['clock_color', 'temp_color', 'other_color'];
  ids.forEach(cid => {
    const c = $('#' + cid).minicolors('rgbObject');
    $('#' + cid + '_val').text(`${rgbToHex(c)}  rgb(${c.r}, ${c.g}, ${c.b})`);
  });
  if (window.ClockPreview) ClockPreview.update();
}

function fmtBytes(n) {
  if (n >= 1048576) return `${(n / 1048576).toFixed(2)} MB`;
  if (n >= 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${n} B`;
}

function fmtUptime(s) {
  const d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600),
    m = Math.floor(s % 3600 / 60), sec = s % 60;
  return `${d ? d + "d " : ""}${h}h ${m}m ${sec}s`;
}

function fmtAgo(s) {
  return s < 0 ? "never" : `${fmtUptime(s)} ago`;
}

function fmtHa(i) {
  if (!i.haConfigured) return "not configured";
  if (i.haLastAttemptAgoSec < 0) return "not fetched yet";
  if (i.haOk) return `OK, ${fmtAgo(i.haLastAttemptAgoSec)}`;
  const why = i.haLastCode === -1 ? "connection error"
    : i.haLastCode === -2 ? "non-numeric state"
    : `HTTP ${i.haLastCode}`;
  const lastOk = i.haLastOkAgoSec < 0 ? "never succeeded" : `last OK ${fmtAgo(i.haLastOkAgoSec)}`;
  return `failed (${why}) ${fmtAgo(i.haLastAttemptAgoSec)}, ${lastOk}`;
}

function fmtRssi(r) {
  const q = r > -50 ? "excellent" : r > -60 ? "good" : r > -70 ? "fair" : "weak";
  return `${r} dBm (${q})`;
}

function getDeviceInfo() {
  $.get("/info", i => {
    const sections = [
      ["Firmware", [
        ["Version", i.fwVersion],
        ["Built", i.buildDate],
        ["Revision", i.gitRev],
        ["Arduino core", i.coreVersion],
        ["SDK", i.sdkVersion],
      ]],
      ["Network", [
        ["Mode", i.wifiMode + (i.wifiConnected ? "" : " (not connected)")],
        ["SSID", i.ssid],
        ["Signal", i.wifiConnected ? fmtRssi(i.rssi) : "n/a"],
        ["Channel", i.channel],
        ["BSSID", i.bssid],
        ["MAC", i.mac],
        ["IP", i.ip],
        ["Gateway", i.gateway],
        ["Subnet", i.subnet],
        ["DNS", i.dns],
        ["Hostname", i.hostname],
      ]],
      ["Time and sensors", [
        ["Clock time", i.localTime + (i.timeSynced ? "" : " (not synced)")],
        ["Last NTP sync", i.ntpSyncCount ? `${fmtAgo(i.ntpLastSyncAgoSec)} (${i.ntpSyncCount} so far)` : "never"],
        ["Timezone", `${i.tzName} (${i.tzValue})`],
        ["Temperature", i.temperature === -1000 ? "no reading" : `${i.temperature}`],
        ["Home Assistant", fmtHa(i)],
        ["Light sensor", `${i.ldr} (average ${i.lightLevel})`],
        ["Buzzer", !i.chimeEnabled ? "chime off" : i.buzzerMuted ? "muted (dark)" : "on"],
      ]],
      ["System", [
        ["Uptime", fmtUptime(i.uptimeSec)],
        ["Last reset", i.resetReason],
        ["Chip ID", i.chipId],
        ["CPU", `${i.cpuMHz} MHz`],
        ["Free heap", `${fmtBytes(i.freeHeap)} (largest block ${fmtBytes(i.maxFreeBlock)}, ${i.heapFragmentation}% fragmented)`],
        ["Lowest free heap", fmtBytes(i.minFreeHeap)],
        ["Flash", `${fmtBytes(i.flashSize)} @ ${i.flashSpeed / 1000000} MHz`],
        ["Sketch", `${fmtBytes(i.sketchSize)} used, ${fmtBytes(i.freeSketchSpace)} free`],
        ["Filesystem", i.fsTotal ? `${fmtBytes(i.fsUsed)} of ${fmtBytes(i.fsTotal)} used` : "n/a"],
      ]],
    ];
    const $root = $('#deviceInfo').empty();
    sections.forEach(([title, rows]) => {
      const $col = $('<div class="col-md-6 col-lg-3 mb-3">').append($('<h6>').text(title));
      const $dl = $('<dl class="mb-0">');
      rows.forEach(([k, v]) => {
        $dl.append($('<dt class="small text-muted font-weight-normal">').text(k));
        $dl.append($('<dd class="text-break">').text(v));
      });
      $root.append($col.append($dl));
    });
  }).fail(xhr => {
    $('#deviceInfo').text(`Could not load device info (${xhr.status})`);
  });
}

function getServerValues() {
  $.get("/config", data => {
    console.log("config: ", data);
    $(document).attr("title", `Matrix Clock [${data.wifiMacString.toLowerCase().replaceAll(":", "")}]`);
    $('#fwVersion').text(data.fwVersion ? `Firmware version: ${data.fwVersion}` : "Firmware version: unknown (pre-2.1.0)");
    $('#authStatus').text(data.authEnabled
      ? "Password protection is on."
      : "Password protection is off: anyone on your network can change settings.");
    $('#wifi_ssid').val(data.wifi_ssid);
    $('#wifi_password').val(data.wifi_password);
    $('#clock_color').minicolors('value', rgbToHex(data.clock_color));
    $('#temp_color').minicolors('value', rgbToHex(data.temp_color));
    $('#other_color').minicolors('value', rgbToHex(data.other_color));
    refreshColorViews();
    let rangeB = document.getElementById("rangeBrightness");
    rangeB.value = data.brightness;
    $('#hourFormat').val("" + data.hourFormat);
    $('#displayStyle').val("" + data.displayStyle);
    $('#chimeEnabled').prop("checked", data.chimeEnabled);
    $('#chimeMuteDark').prop("checked", data.chimeMuteDark);
    $('#chimeDarkThreshold').val(data.chimeDarkThreshold);
    showLight();
    if (window.ClockPreview) ClockPreview.update();
    $('#autoBrightness').prop("checked", data.autoBrightness);
    if (data.autoBrightness) {
      $('#rangeBrightness').prop("disabled", true);
    } else {
      $('#rangeBrightness').prop("disabled", false);
    }
    $('#hassBaseUrl').val(data.hassConfig.hassBaseUrl);
    $('#hassEntityId').val(data.hassConfig.hassEntityId);
    $('#hassAT').val(data.hassConfig.hassAT);
    // $('#timezones').val(data.tzValue).change();
    $("#timezones option").filter(function(index) { return $(this).text() === data.tzName; }).attr('selected', 'selected');
  });
}

// POST with console feedback: the config endpoints return 400 on invalid
// values, and without this a rejected setting fails silently.
function postSettings(url, data) {
  $.post(url, data)
    .done(resp => console.log(`${url}: ok`, resp))
    .fail((xhr) => console.warn(`${url}: rejected`, xhr.status, xhr.responseText));
}

function setAuthPassword() {
  const pw = $('#authPassword').val();
  const msg = pw ? "Set this password? The clock reboots to apply it." : "Turn password protection off? The clock reboots to apply it.";
  if (!confirm(msg)) return;
  $.post("/config/auth", { authPassword: pw })
    .done(() => {
      $('#authStatus').text("Saved. Rebooting...");
      setTimeout(() => location.reload(), 12000);
    })
    .fail(xhr => alert(`Rejected (${xhr.status}): ${xhr.responseText}`));
}

function setWifiConfig() {
  postSettings("/config/wifiConfig", {
    wifi_ssid: $('#wifi_ssid').val(),
    wifi_password: $('#wifi_password').val()
  });
}

function beep1() {
  console.log(`single beep`);
  $.post("/sound", {
    type: "single"
  });
}

function beep2() {
  console.log(`double beep`);
  $.post("/sound", {
    type: "double"
  });
}

function forceTemp() {
  postSettings("/forcetemp", {
    forceTemp: $('#forceTemp').val()
  });
}

function forceTime() {
  // today's date + the entered time, so forcing time keeps the date correct
  const d = new Date();
  const t = new Date(`${d.toDateString()} ${$('#forceTime').val()}`).getTime();
  postSettings("/forcetime", { forceTime: t });
}

function syncTime() {
  $.get("/syncTime");
}

function setClockColor() {
  postSettings("/config/clockcolor", $('#clock_color').minicolors('rgbObject'));
}

function setTempColor() {
  postSettings("/config/tempcolor", $('#temp_color').minicolors('rgbObject'));
}

function setOtherColor() {
  postSettings("/config/othercolor", $('#other_color').minicolors('rgbObject'));
}

function toggleAutoBrightness() {
  let abcb = document.querySelector('#autoBrightness');
  if (abcb.checked) {
    $('#rangeBrightness').prop("disabled", true);
    postSettings("/config/autobrightness", { autoBrightness: true });
    setTimeout(function () {
      getServerValues();
    }, 1000)
  } else {
    $('#rangeBrightness').prop("disabled", false);
    postSettings("/config/autobrightness", { autoBrightness: false });
  }
}

function setBrightness() {
  postSettings("/config/brightness", {
    brightness: $('#rangeBrightness').val()
  });
}

function updateHourFormat() {
  postSettings("/config/hourformat", {
    hourFormat: $('#hourFormat').val()
  });
}

// Live light reading next to the dark threshold, so the threshold can be
// tuned against what the sensor actually reads in the dark and in daylight.
let lastLight = null;

function refreshLight() {
  $.get("/info", i => {
    lastLight = i.lightLevel;
    showLight();
  });
}

function showLight() {
  if (lastLight === null || lastLight < 0) {
    $('#lightNow').text("Light sensor: waiting for a reading...");
    return;
  }
  const t = parseInt($('#chimeDarkThreshold').val(), 10);
  const verdict = isNaN(t) ? "" : lastLight < t ? " (dark)" : " (bright)";
  $('#lightNow').text(`Light sensor right now: ${lastLight}${verdict}. Lower = darker.`);
}

function updateChime() {
  const t = parseInt($('#chimeDarkThreshold').val(), 10);
  if (isNaN(t) || t < 0 || t > 1023) {
    alert("Dark threshold must be a number from 0 to 1023.");
    return;
  }
  postSettings("/config/chime", {
    chimeEnabled: $('#chimeEnabled').prop('checked'),
    chimeMuteDark: $('#chimeMuteDark').prop('checked'),
    chimeDarkThreshold: t
  });
}

function updateDisplayStyle() {
  postSettings("/config/displaystyle", {
    displayStyle: $('#displayStyle').val()
  });
}

function updateTzOffset() {
  postSettings("/config/timezones", {
    tzValue: $('#timezones').val(),
    tzName: $('#timezones option:selected').text()
  });
}

function sethassConfig() {
  postSettings("/config/hassConfig", {
    hassBaseUrl: $('#hassBaseUrl').val(),
    hassEntityId: $('#hassEntityId').val(),
    hassAT: $('#hassAT').val()
  });
}

function showMessage() {
  postSettings("/showmessage", {
    messageText: $('#messageText').val(),
    scrollDelay: $('#scrollDelay').val(),
    repeatCount: $('#repeatCount').val()
  });
}

