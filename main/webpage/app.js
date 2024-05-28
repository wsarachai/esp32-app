/**
 * Add gobals here
 */
var seconds = null;
var otaTimerVar = null;
var wifiConnectInterval = null;

function isEmpty(val) {
  return val === undefined || val == null || val.length <= 0 ? true : false;
}

/**
 * Initialize functions here.
 */
$(document).ready(function () {
  $("#connect_info").hide();
  getSSID();
  getUpdateStatus();
  startESPServerStatusInterval();
  startLocalTimeInterval();
  getConnectInfo();
  $("#connect_wifi").on("click", function () {
    checkCredentials();
  });
  $("#save_water_config").on("click", function () {
    saveWaterConfigure();
  });
  $("#disconnect_wifi").on("click", function () {
    disconnectWifi();
  });
  $("#manual_water_btn").on("click", function () {
    toggle_water_on_off();
  });
});

/**
 * Gets file name and size for display on the web page.
 */
function getFileInfo() {
  var x = document.getElementById("selected_file");
  var file = x.files[0];

  document.getElementById("file_info").innerHTML =
    "<h4>File: " + file.name + "<br>" + "Size: " + file.size + " bytes</h4>";
}

/**
 * Handles the firmware update.
 */
function updateFirmware() {
  // Form Data
  var formData = new FormData();
  var fileSelect = document.getElementById("selected_file");

  if (fileSelect.files && fileSelect.files.length == 1) {
    var file = fileSelect.files[0];
    formData.set("file", file, file.name);
    document.getElementById("ota_update_status").innerHTML =
      "Uploading " + file.name + ", Firmware Update in Progress...";

    // Http Request
    var request = new XMLHttpRequest();

    request.upload.addEventListener("progress", updateProgress);
    request.open("POST", "/OTAupdate");
    request.responseType = "blob";
    request.send(formData);
  } else {
    window.alert("Select A File First");
  }
}

/**
 * Progress on transfers from the server to the client (downloads).
 */
function updateProgress(oEvent) {
  if (oEvent.lengthComputable) {
    getUpdateStatus();
  } else {
    window.alert("total size is unknown");
  }
}

/**
 * Posts the firmware udpate status.
 */
function getUpdateStatus() {
  var requestURL = "/OTAstatus";
  fetch(requestURL, {
    method: "POST",
    cache: "no-cache",
    body: "ota_update_status",
    headers: {
      "Content-Type": "application/x-www-form-urlencoded",
    },
  })
    .then((response) => response.json())
    .then((data) => {
      document.getElementById("latest_firmware").innerHTML =
        data.compile_date + " - " + data.compile_time;

      // If flashing was complete it will return a 1, else -1
      // A return of 0 is just for information on the Latest Firmware request
      if (data.ota_update_status == 1) {
        // Set the countdown timer time
        seconds = 10;
        // Start the countdown timer
        otaRebootTimer();
      } else if (data.ota_update_status == -1) {
        document.getElementById("ota_update_status").innerHTML =
          "!!! Upload Error !!!";
      }
    });
}

/**
 * Displays the reboot countdown.
 */
function otaRebootTimer() {
  document.getElementById("ota_update_status").innerHTML =
    "OTA Firmware Update Complete. This page will close shortly, Rebooting in: " +
    seconds;

  if (--seconds == 0) {
    clearTimeout(otaTimerVar);
    window.location.reload();
  } else {
    otaTimerVar = setTimeout(otaRebootTimer, 1000);
  }
}

/**
 * Gets DHT22 sensor temperature and humidity values for display on the web page.
 */
function getESPServerStatus() {
  $.getJSON("/ESPServerStatus.json", function (data) {
    $("#temperature_reading").text(data["temp"] + "°C");
    $("#humidity_reading").text(data["humidity"] + "%(RH)");
    $("#soil_moisture_reading").text(data["soil_moisture"] + "%");

    $("#min_moiture_level_data").text(data["min_moiture_level"] + "%");
    $("#required_moiture_level_data").text(
      data["required_moiture_level"] + "%"
    );
    $("#duration_data").text(data["duration"] + " นาที");

    setWaterButtonStatus(data["water_status"]);

    if (data["wifi_connect_status"]) {
      if (isEmpty($("#connected_ap").text())) {
        getConnectInfo();
      }
    }
  });
}

/**
 * Sets the interval for getting the updated DHT22 sensor values.
 */
function startESPServerStatusInterval() {
  setInterval(getESPServerStatus, 5000);
}

/**
 * Clears the connection status interval.
 */
function stopWifiConnectStatusInterval() {
  if (wifiConnectInterval != null) {
    clearInterval(wifiConnectInterval);
    wifiConnectInterval = null;
  }
}

/**
 * Gets the WiFi connection status.
 */
function getWifiConnectStatus() {
  var requestURL = "/wifiConnectStatus";

  fetch(requestURL, {
    method: "POST",
    cache: "no-cache",
    body: "wifi_connect_status",
    headers: {
      "Content-Type": "application/x-www-form-urlencoded",
    },
  })
    .then((response) => response.json())
    .then((data) => {
      if (data.wifi_connect_status == 2) {
        $("#wifi_connect_status").html(
          "การเชื่อมต่อล้มเหลว. โปรดตรวจสอบชื่อ AP และรหัสผ่านให้ถูกต้อง"
        );
        stopWifiConnectStatusInterval();
      } else if (data.wifi_connect_status == 3) {
        $("#wifi_connect_status").html("Connection Success!");
        $("#wifi_connect_status").removeClass("rd");
        $("#wifi_connect_status").addClass("gr");
        stopWifiConnectStatusInterval();
        getConnectInfo();
      }
    });
}

/**
 * Starts the interval for checking the connection status.
 */
function startWifiConnectStatusInterval() {
  wifiConnectInterval = setInterval(getWifiConnectStatus, 2800);
}

/**
 * Connect WiFi function called using the SSID and password entered into the text fields.
 */
function connectWifi() {
  // Get the SSID and password
  selectedSSID = $("#connect_ssid").val();
  pwd = $("#connect_pass").val();

  $("#wifi_connect_status").removeClass("hidden");
  $("#wifi_connect_status").addClass("rd");
  $("#wifi_connect_status").html("Connecting...");

  $.ajax({
    url: "/wifiConnect.json",
    dataType: "json",
    method: "POST",
    cache: false,
    headers: { "my-connect-ssid": selectedSSID, "my-connect-pwd": pwd },
    data: { timestamp: Date.now() },
  });

  startWifiConnectStatusInterval();
}

function saveWaterConfigure() {
  let min_moiture_level = $("#min_moiture_level").val();
  let required_moiture_level = $("#required_moiture_level").val();
  let duration = $("#duration").val();

  $.ajax({
    url: "/saveWaterConfigure.json",
    dataType: "json",
    method: "POST",
    cache: false,
    headers: {
      "min-moiture-level": min_moiture_level,
      "required-moiture-level": required_moiture_level,
      duration: duration,
    },
    data: { timestamp: Date.now() },
  });
}

/**
 * Checks credentials on connect_wifi button click.
 */
function checkCredentials() {
  errorList = "";
  credsOk = true;

  selectedSSID = $("#connect_ssid").val();
  pwd = $("#connect_pass").val();

  if (selectedSSID == "") {
    errorList += "<h4 class='rd'>SSID cannot be empty!</h4>";
    credsOk = false;
  }
  if (pwd == "") {
    errorList += "<h4 class='rd'>Password cannot be empty!</h4>";
    credsOk = false;
  }

  if (credsOk == false) {
    $("#wifi_connect_credentials_errors").html(errorList);
  } else {
    $("#wifi_connect_credentials_errors").html("");
    connectWifi();
  }
}

/**
 * Shows the WiFi password if the box is checked.
 */
function showPassword() {
  var x = document.getElementById("connect_pass");
  if (x.type === "password") {
    x.type = "text";
  } else {
    x.type = "password";
  }
}

/**
 * Gets the connection information for displaying on the web page.
 */
function getConnectInfo() {
  var requestURL = "/wifiConnectInfo.json";

  fetch(requestURL)
    .then((response) => response.json())
    .then((data) => {
      $("#connected_ap_label").html("Connected to: ");
      $("#connected_ap").text(data["ap"]);

      $("#ip_address_label").html("IP Address: ");
      $("#wifi_connect_ip").text(data["ip"]);

      $("#netmask_label").html("Netmask: ");
      $("#wifi_connect_netmask").text(data["netmask"]);

      $("#gateway_label").html("Gateway: ");
      $("#wifi_connect_gw").text(data["gw"]);

      if ($("#connect_info").is(":hidden")) {
        $("#wifi_connect_status").fadeOut("slow");
        $("#connect_info").fadeIn("slow");
      }
    });
}

/**
 * Disconnects Wifi once the disconnect button is pressed and reloads the web page.
 */
function disconnectWifi() {
  var requestURL = "/wifiDisconnect.json";

  fetch(requestURL, {
    method: "DELETE",
    cache: "no-cache",
    body: JSON.stringify({
      timestamp: Date.now(),
    }),
  }).then((data) => {
    console.log("disconnectWifi...");
    $("#connect_info").fadeOut("slow");
  });

  // Update the web page
  setTimeout("location.reload(true);", 2000);
}

/**
 * Sets the interval for displaying local time.
 */
function startLocalTimeInterval() {
  setInterval(getLocalTime, 10000);
}

/**
 * Gets the local time.
 * @note connect the ESP32 to the internet and the time will be updated.
 */
function getLocalTime() {
  var requestURL = "/localTime.json";

  fetch(requestURL, {
    method: "GET",
    cache: "no-cache",
  })
    .then((response) => response.json())
    .then((data) => {
      $("#local_time").text(data["time"]);
    });
}

/**
 * Toggle water on/off
 * @note manual toggle water on/off
 */
function toggle_water_on_off() {
  var requestURL = "/toggleWaterOnOff.json";

  fetch(requestURL, {
    method: "GET",
    cache: "no-cache",
  })
    .then((response) => response.json())
    .then((data) => {
      setWaterButtonStatus(data["water_status"]);
    });
}

function setWaterButtonStatus(status) {
  if (status && status === "ON") {
    $("#manual_water_btn").html("ปิดวาล์ว");
    $("#manual_water_btn").css("background", "#32cd32");
  } else if (status && status === "OFF") {
    $("#manual_water_btn").html("เปิดวาล์ว");
    $("#manual_water_btn").css("background", "#1D3557");
  }
}

/**
 * Gets the ESP32's access point SSID for displaying on the web page.
 */
function getSSID() {
  $.getJSON("/apSSID.json", function (data) {
    $("#ap_ssid").text(data["ssid"]);
  });
}
