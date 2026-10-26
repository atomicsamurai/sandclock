$(document).ready(function () {

  if ($('#clock_color').length) {
    $('#clock_color').minicolors({
      inline: $('#clock_color').attr('data-inline') === 'true',
      theme: 'bootstrap'
    });
  }
  if ($('#temp_color').length) {
    $('#temp_color').minicolors({
      inline: $('#temp_color').attr('data-inline') === 'true',
      theme: 'bootstrap'
    });
  }
  if ($('#other_color').length) {
    $('#other_color').minicolors({
      inline: $('#other_color').attr('data-inline') === 'true',
      theme: 'bootstrap'
    });
  }
  $('#autoBrightness').change(() => {
    toggleAutoBrightness();
  });
  getServerValues();
});

function getServerValues() {
  $.get("/config", data => {
    console.log("config: ", data);
    $(document).attr("title", `Matrix Clock [${data.wifiMacString.toLowerCase().replaceAll(":", "")}]`);
    $('#wifi_ssid').val(data.wifi_ssid);
    $('#wifi_password').val(data.wifi_password);
    $('#clock_color').minicolors('value', data.clock_color);
    $('#temp_color').minicolors('value', data.temp_color);
    $('#other_color').minicolors('value', data.other_color);
    let rangeB = document.getElementById("rangeBrightness");
    rangeB.value = data.brightness;
    $('#hourFormat').val("" + data.hourFormat);
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

