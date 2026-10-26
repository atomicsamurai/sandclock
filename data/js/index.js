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

function setWifiConfig() {
  const wifi_ssid = $('#wifi_ssid').val();
  const wifi_password = $('#wifi_password').val();

  $.post("/config/wifiConfig", {
    wifi_ssid: wifi_ssid,
    wifi_password: wifi_password
  });
}

function reboot() {
  $.get("/reboot");
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
  const ft = $('#forceTemp').val();
  console.log(ft);
  $.post("/forcetemp", {
    forceTemp: ft
  });
}

function forceTime() {
  const ft = $('#forceTime').val();
  const t = new Date(`01 Jan 2023 ${ft}`).getTime();
  console.log(t);
  $.post("/forcetime",  {
    forceTime: t
  });
}

function syncTime() {
  $.get("/syncTime");
}

function setClockColor() {
  const color = $('#clock_color').minicolors('rgbObject');
  console.log(color);
  $.post("/config/clockcolor", color);
}

function setTempColor() {
  const color = $('#temp_color').minicolors('rgbObject');
  console.log(color);
  $.post("/config/tempcolor", color);
}

function setOtherColor() {
  const color = $('#other_color').minicolors('rgbObject');
  console.log(color);
  $.post("/config/othercolor", color);
}

function toggleAutoBrightness() {
  let abcb = document.querySelector('#autoBrightness');
  if (abcb.checked) {
    $('#rangeBrightness').prop("disabled", true);
    $.post("/config/autobrightness", {
      autoBrightness: true
    });
    setTimeout(function () {
      getServerValues();
    }, 1000)
  } else {
    $('#rangeBrightness').prop("disabled", false);
    $.post("/config/autobrightness", {
      autoBrightness: false
    });
  }
}

function setBrightness() {
  const brightness = $('#rangeBrightness').val();
  console.log(brightness);
  $.post("/config/brightness", {
    brightness: brightness
  });
}

function setDateTime() {
  const d = moment().format('MMMM DD YYYY');
  const t = moment().format('HH:mm:ss');
  const datetime = {
    date: d,
    time: t
  };
  console.log(datetime);
  $.post("/setdate", datetime);
}

function showClock() {
  $.post("/clock");
}

function updateHourFormat() {
  const hourFormat = $('#hourFormat').val();
  $.post("/config/hourformat", {
    hourFormat: hourFormat
  });
}

function updateTzOffset() {
  const tzValue = $('#timezones').val();
  const tzName = $('#timezones option:selected').text();
  $.post("/config/timezones", {
    tzValue: tzValue,
    tzName: tzName
  });
}

function sethassConfig() {
  const hassBaseUrl = $('#hassBaseUrl').val();
  const hassEntityId = $('#hassEntityId').val();
  const hassAT = $('#hassAT').val();

  $.post("/config/hassConfig", {
    hassBaseUrl: hassBaseUrl,
    hassEntityId: hassEntityId,
    hassAT: hassAT
  });
}

function showMessage() {
  const messageText = $('#messageText').val();
  const scrollDelay = $('#scrollDelay').val();
  const repeatCount = $('#repeatCount').val();
  console.log(`showmessage: ${messageText}:${scrollDelay}:${repeatCount}`);

  $.post("/showmessage", {
    messageText: messageText,
    scrollDelay: scrollDelay,
    repeatCount: repeatCount
  });
}

