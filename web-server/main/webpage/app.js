"use strict";

////////////////////////////////////////
// Helper Functions
////////////////////////////////////////
function isEmpty(val) {
    return val === undefined || val == null || val.length <= 0 ? true : false;
}

function createElement(element, attribute, inner) {
    if (typeof element === "undefined") {
        return false;
    }
    if (typeof inner === "undefined") {
        inner = "";
    }
    var el = document.createElement(element);
    if (typeof attribute === "object") {
        for (var key in attribute) {
            el.setAttribute(key, attribute[key]);
        }
    }
    if (!Array.isArray(inner)) {
        inner = [inner];
    }
    for (var k = 0; k < inner.length; k++) {
        if (inner[k].tagName) {
            el.appendChild(inner[k]);
        } else {
            el.appendChild(document.createTextNode(inner[k]));
        }
    }
    return el;
}

HTMLElement.prototype._appendChildren = HTMLElement.prototype.appendChild;
HTMLElement.prototype.appendChild = function (child) {
    if (child instanceof ProgressBar) {
        return HTMLElement.prototype._appendChildren.call(this, child.barContainer);
    } else if (child instanceof Section) {
        return HTMLElement.prototype._appendChildren.call(
            this,
            child.sectionContainer
        );
    } else {
        return HTMLElement.prototype._appendChildren.call(this, child);
    }
};

////////////////////////////////////////
// Image
////////////////////////////////////////
const Image = function (id) {
    this.id = id;
    this.imageContainer = createElement("div", { id: this.id });
}

////////////////////////////////////////
// Progress Bar
////////////////////////////////////////
const ProgressBar = function (id) {
    this.id = id;
    this.barContainer = createElement("div", { id: this.id });
    this.bar = createElement("div", { id: this.id + "-fill" });
    this.barContainer.appendChild(this.bar);
};

ProgressBar.prototype.changeBackgroundByValue = function (width) {
    if (width <= 30) {
        $(this.bar).css("background", "red");
    } else if (width <= 50) {
        $(this.bar).css("background", "orange");
    } else {
        $(this.bar).css("background", "green");
    }
};

ProgressBar.prototype.setProgress = function (width) {
    $(this.bar).css("width", width + "%");
    this.changeBackgroundByValue(width);
};

////////////////////////////////////////
// Soil Moisture Progress Bar
////////////////////////////////////////
const SoilMoistureProgressBar = function (id) {
    ProgressBar.call(this, id);
};
SoilMoistureProgressBar.prototype = Object.create(ProgressBar.prototype);

SoilMoistureProgressBar.prototype.changeBackgroundByValue = function (
    width
) {
    if (width <= 40) {
        $(this.bar).css("background", "red");
    } else if (width <= 60) {
        $(this.bar).css("background", "orange");
    } else {
        $(this.bar).css("background", "green");
    }
};

////////////////////////////////////////
// Section Div
////////////////////////////////////////
const Section = function (sectionId, title) {
    this.sectionContainer = createElement("section", {
        id: sectionId,
    });
    this.title = title;

    this.createHeader();
};

Section.prototype.getHeaderContainerId = function () {
    return "header-container";
};

Section.prototype.createHeader = function () {
    const headerContainer = createElement("div", {
        id: this.getHeaderContainerId(),
    });
    headerContainer.appendChild(createElement("h2", {}, this.title));
    this.appendChild(headerContainer);
};

Section.prototype.appendChild = function (element) {
    this.sectionContainer.appendChild(element);
};

////////////////////////////////////////
// General Information
////////////////////////////////////////
const GeneralInfo = function () {
    Section.call(this, "general-info", "ข้อมูลทั่วไป");
    this.lastSensorDataTime = "";

    this.createSensorInfo();
    setInterval(this.getLocalTime.bind(this), 10000);
};
GeneralInfo.prototype = Object.create(Section.prototype);

GeneralInfo.prototype.createHeader = function () {
    const headerContainer = createElement("div", {
        id: this.getHeaderContainerId(),
    });

    const header = createElement("div", { class: "flex-container" });
    this.currentTime = createElement("div", { class: "time-data" });

    header.appendChild(createElement("h2", {}, this.title));
    header.appendChild(this.currentTime);

    headerContainer.appendChild(header);

    this.appendChild(headerContainer);
};

GeneralInfo.prototype.createSensorInfo = function () {
    const sensorInfo = createElement("div", { class: "sensor-container" });

    this.sensorAvailabilityNotice = createElement("div", {
        id: "sensor-availability-notice",
        class: "data",
    });
    this.sensorAvailabilityNotice.innerHTML = "";

    // Temperature
    const temperatureInfo = createElement("div", {
        class: "sensor-box temperature",
    });
    temperatureInfo.appendChild(createElement("label", {}, "อุณหภูมิ"));
    this.temperatureReading = createElement("div", {
        id: "temperature_reading",
        class: "data",
    });
    temperatureInfo.appendChild(this.temperatureReading);

    // Humidity
    const humidityInfo = createElement("div", { class: "sensor-box humidity" });
    humidityInfo.appendChild(createElement("label", {}, "ความชื้นอากาศ"));
    this.humidityReading = createElement("div", {
        id: "humidity_reading",
        class: "data",
    });
    this.humidityBar = new ProgressBar("humidity-bar");

    humidityInfo.appendChild(this.humidityReading);
    humidityInfo.appendChild(this.humidityBar);

    sensorInfo.appendChild(temperatureInfo);
    sensorInfo.appendChild(humidityInfo);

    // Soil Moisture
    const soilMoistureInfo = createElement("div", {
        class: "sensor-box soil-moisture",
    });
    soilMoistureInfo.appendChild(createElement("label", {}, "ความชื้นในดิน"));
    this.soilMoistureReading = createElement("div", {
        id: "soil_moisture_reading",
        class: "data",
    });
    this.soilMoistureBar = new SoilMoistureProgressBar("soil-moisture-bar");

    soilMoistureInfo.appendChild(this.soilMoistureReading);
    soilMoistureInfo.appendChild(this.soilMoistureBar);

    sensorInfo.appendChild(temperatureInfo);
    sensorInfo.appendChild(humidityInfo);
    sensorInfo.appendChild(soilMoistureInfo);
    sensorInfo.appendChild(this.sensorAvailabilityNotice);

    this.appendChild(sensorInfo);
};

GeneralInfo.prototype.getLocalTime = function () {
    var requestURL = "/localTime.json";

    function payload(data) {
        if (data["time"]) {
            this.setCurrentTime(data["time"]);
        }
    }

    fetch(requestURL, {
        method: "GET",
        cache: "no-cache",
    })
        .then((response) => response.json())
        .then(payload.bind(this));
};

GeneralInfo.prototype.setCurrentTime = function (time) {
    if (time) {
        this.currentTime.innerHTML = time;
    }
};
GeneralInfo.prototype.setTemperatureReading = function (temperature) {
    this.temperatureReading.innerHTML =
        parseFloat(temperature).toFixed(2) + " °C";
};

GeneralInfo.prototype.setHumidityReading = function (humidity) {
    this.humidityReading.innerHTML = parseFloat(humidity).toFixed(2) + " %";
    this.humidityBar.setProgress(humidity);
};

GeneralInfo.prototype.setSoilMoistureReading = function (soilMoisture) {
    this.soilMoistureReading.innerHTML =
        parseFloat(soilMoisture).toFixed(2) + " %";
    this.soilMoistureBar.setProgress(soilMoisture);
};

GeneralInfo.prototype.setNoDataState = function () {
    if (this.lastSensorDataTime) {
        this.setCurrentTime(this.lastSensorDataTime);
        this.sensorAvailabilityNotice.innerHTML =
            "No data from server. Showing last values at " +
            this.lastSensorDataTime;
    } else {
        this.sensorAvailabilityNotice.innerHTML = "No data from server yet.";
    }
};

GeneralInfo.prototype.setDataAvailableState = function (time) {
    if (time) {
        this.lastSensorDataTime = time;
    }
    this.sensorAvailabilityNotice.innerHTML = "";
};

GeneralInfo.prototype.updateStatus = function (data) {
    this.setCurrentTime(data["time"]);
    this.setTemperatureReading(data["temp"]);
    this.setHumidityReading(data["humidity"]);
    this.setSoilMoistureReading(data["soil_moisture"]);
};

////////////////////////////////////////
// System configuration
////////////////////////////////////////
const SystemConfig = function () {
    Section.call(this, "water-config", "กำหนดค่าของระบบ");

    this.createBodyInfo();
};
SystemConfig.prototype = Object.create(Section.prototype);

SystemConfig.prototype.createBodyInfo = function () {
    // Min Moisture Level
    const minMoistureLevelID = "min-moisture-level";
    const minLabel = createElement(
        "span",
        { for: minMoistureLevelID, class: "lbl" },
        "ความชื้นเปิดวาล์ว (ค่าต่ำสุด):"
    );
    this.minMoistureLevel = createElement("span", {
        id: minMoistureLevelID + "-data",
        class: "data",
    });
    this.minMoistureLevelInput = createElement("input", {
        type: "number",
        min: 0,
        max: 100,
        id: minMoistureLevelID,
        placeholder: "0-100%",
    });

    // max moisture level
    const maxMoistureLevelID = "max-moisture-level";
    const maxLabel = createElement(
        "span",
        { for: maxMoistureLevelID, class: "lbl" },
        "ความชื้นปิดวาล์ว (ค่าสูงสุด):"
    );
    this.maxMoistureLevel = createElement("span", {
        id: maxMoistureLevelID + "-data",
        class: "data",
    });
    this.maxMoistureLevelInput = createElement("input", {
        type: "number",
        min: 0,
        max: 100,
        id: maxMoistureLevelID,
        placeholder: "0-100%",
    });

    // max moisture level
    const maxTimeSpendID = "max-time-spend-level";
    const maxTimeSpendLabel = createElement(
        "span",
        { for: maxTimeSpendID, class: "lbl" },
        "เวลาสูงสุด (หากเกินเวลานี้วาล์วน้ำจะปิดอัตโนมัติ):"
    );
    this.maxTimeSpend = createElement("span", {
        id: maxTimeSpendID + "-data",
        class: "data",
    });
    this.maxTimeSpendInput = createElement("input", {
        type: "number",
        min: 0,
        max: 100,
        id: maxTimeSpendID,
        placeholder: "จำนวนนาที",
    });

    // Submit button
    const submitButton = createElement(
        "button",
        { id: "save-water-config" },
        "บันทึก"
    );
    submitButton.addEventListener("click", this.saveWaterConfigure.bind(this));

    this.appendChild(minLabel);
    this.appendChild(this.minMoistureLevel);
    this.appendChild(this.minMoistureLevelInput);
    this.appendChild(maxLabel);
    this.appendChild(this.maxMoistureLevel);
    this.appendChild(this.maxMoistureLevelInput);
    this.appendChild(maxTimeSpendLabel);
    this.appendChild(this.maxTimeSpend);
    this.appendChild(this.maxTimeSpendInput);
    this.appendChild(submitButton);
};

SystemConfig.prototype.setMinMoistureLevel = function (minMoistureLevel) {
    this.minMoistureLevel.innerHTML = " " + minMoistureLevel + " %";
    $(this.minMoistureLevelInput).val(minMoistureLevel);
};

SystemConfig.prototype.setMaxMoistureLevel = function (maxMoistureLevel) {
    this.maxMoistureLevel.innerHTML = " " + maxMoistureLevel + " %";
    $(this.maxMoistureLevelInput).val(maxMoistureLevel);
};

SystemConfig.prototype.setMaxTimeSpend = function (timeSpend) {
    this.maxTimeSpend.innerHTML = " " + timeSpend + " นาที";
    $(this.maxTimeSpendInput).val(timeSpend);
};

SystemConfig.prototype.saveWaterConfigure = function () {
    let min_moiture_level = $(this.minMoistureLevelInput).val();
    let max_moiture_level = $(this.maxMoistureLevelInput).val();
    let duration = $(this.maxTimeSpendInput).val();

    $.ajax({
        url: "/saveWaterConfigure.json",
        dataType: "json",
        method: "POST",
        cache: false,
        headers: {
            "min-moisture-level": min_moiture_level,
            "max-moisture-level": max_moiture_level,
            duration: duration,
        },
        data: { timestamp: Date.now() },
        success: (data) => {
            this.setMinMoistureLevel(data["min-moiture-level"]);
            this.setMaxMoistureLevel(data["max-moiture-level"]);
            this.setMaxTimeSpend(data["duration"]);
        },
        error: (xhr, status, error) => {
            console.error("Failed to save water configuration:", error);
        },
    });
};

////////////////////////////////////////
// Relay Control
////////////////////////////////////////
const RELAY_ON = "เปิด";
const RELAY_OFF = "ปิด";

const RelayControl = function () {
    Section.call(this, "relay-control", "ควบคุมรีเลย์");

    this.createBodyInfo();
};
RelayControl.prototype = Object.create(Section.prototype);

RelayControl.prototype.createBodyInfo = function () {
    const divBody = createElement("div");
    this.relayCheckbox = createElement("input", {
        type: "checkbox",
        id: "relay-chk",
    });
    const lblStatus = createElement(
        "span",
        { id: "relay-chk-label" },
        "กำหนดเอง"
    );
    divBody.appendChild(this.relayCheckbox);
    divBody.appendChild(lblStatus);

    this.relayIndicator = createElement("div", {
        id: "water-switch-indicator",
        class: "water-switch-indicator is-off",
    });
    this.relayIndicatorIcon = createElement("div", {
        class: "water-switch-icon",
    });
    const indicatorText = createElement("div", { class: "water-switch-text" });
    this.relayIndicatorTitle = createElement("div", {
        class: "water-switch-title",
    }, "วาล์วน้ำ");
    this.relayIndicatorState = createElement("div", {
        class: "water-switch-state",
    }, "ปิดอยู่");
    this.relayIndicatorMode = createElement("div", {
        class: "water-switch-mode",
    }, "โหมดอัตโนมัติ");
    indicatorText.appendChild(this.relayIndicatorTitle);
    indicatorText.appendChild(this.relayIndicatorState);
    indicatorText.appendChild(this.relayIndicatorMode);
    this.relayIndicator.appendChild(this.relayIndicatorIcon);
    this.relayIndicator.appendChild(indicatorText);

    this.relayButton = createElement("button", { id: "relay-button" }, RELAY_ON);

    this.relayCheckbox.addEventListener("click", this.relayCheckbox_Click.bind(this));
    this.relayButton.addEventListener("click", this.toggleRelayOnOff.bind(this));

    this.appendChild(divBody);
    this.appendChild(this.relayIndicator);
    this.appendChild(this.relayButton);
};

RelayControl.prototype.setControlMode = function (isManual) {
    this.relayIndicatorMode.innerHTML = isManual ? "โหมดกำหนดเอง" : "โหมดอัตโนมัติ";
};

RelayControl.prototype.setRelayIndicatorStatus = function (status) {
    if (status && status === "ON") {
        this.relayIndicator.className = "water-switch-indicator is-on";
        this.relayIndicatorState.innerHTML = "กำลังจ่ายน้ำ";
    } else {
        this.relayIndicator.className = "water-switch-indicator is-off";
        this.relayIndicatorState.innerHTML = "ปิดอยู่";
    }
};

RelayControl.prototype.relayCheckbox_Click = function () {
    const relayButton = $(this.relayButton);
    if (this.relayCheckbox.checked) {
        this.setControlMode(true);
        relayButton.fadeIn("fast");
    } else {
        this.setControlMode(false);
        relayButton.fadeOut("fast");
        // Return relay to automatic (irrigation_ctrl) mode.
        $.ajax({
            url: "/relayControl.json",
            dataType: "json",
            method: "POST",
            cache: false,
            headers: { "relay-control": "auto" },
            data: { timestamp: Date.now() },
        });
    }
};

RelayControl.prototype.toggleRelayOnOff = function () {
    var relayStatus = $(this.relayButton).html() === RELAY_ON ? "OFF" : "ON";

    $.ajax({
        url: "/relayControl.json",
        dataType: "json",
        method: "POST",
        cache: false,
        headers: {
            "relay-control": relayStatus === "ON" ? "1" : "0",
        },
        data: { timestamp: Date.now() },
        success: (data) => {
            this.setRelayButtonStatus(data["relay_status"]);
        },
        error: (xhr, status, error) => {
            console.error("Failed to control relay:", error);
        },
    });
};

RelayControl.prototype.setRelayButtonStatus = function (status) {
    const relayButton = $(this.relayButton);
    this.setRelayIndicatorStatus(status);
    if (status && status === "ON") {
        relayButton.html(RELAY_ON);
        relayButton.css("background", "#ff6b6b");
    } else if (status && status === "OFF") {
        relayButton.html(RELAY_OFF);
        relayButton.css("background", "#1D3557");
    }
};

RelayControl.prototype.getRelayStatus = function () {
    var requestURL = "/relayStatus.json";

    fetch(requestURL, {
        method: "GET",
        cache: "no-cache",
    })
        .then((response) => response.json())
        .then((data) => {
            if (data["relay_status"]) {
                this.setRelayButtonStatus(data["relay_status"]);
            }
            // Restore checkbox state when manual override is already active
            // (e.g. the page was reloaded while the user had manual control).
            if (data["manual_override"]) {
                this.relayCheckbox.checked = true;
                this.setControlMode(true);
                $(this.relayButton).show();
            } else {
                this.relayCheckbox.checked = false;
                this.setControlMode(false);
                $(this.relayButton).hide();
            }
        })
        .catch((error) => console.error("Failed to get relay status:", error));
};

////////////////////////////////////////
// SSID Information
////////////////////////////////////////
const ConnectivityInfo = function () {
    Section.call(this, "ssid-info", "ข้อมูลเครือข่าย (SSID)");

    this.createBodyInfo();
};
ConnectivityInfo.prototype = Object.create(Section.prototype);

ConnectivityInfo.prototype.createBodyInfo = function () {
    this.apSSID = createElement("div", { id: "ap-ssid", class: "data" });
    this.appendChild(this.apSSID);

    // Client Node Info
    const clientNodeInfo = createElement("div", { class: "client-info" });
    clientNodeInfo.appendChild(createElement("label", {}, "โหนด Client:"));
    this.clientNodeStatus = createElement("span", {
        id: "client-node-status",
        class: "data",
    });
    clientNodeInfo.appendChild(this.clientNodeStatus);

    // Web Connection Info
    const webConnectionInfo = createElement("div", { class: "client-info" });
    webConnectionInfo.appendChild(createElement("label", {}, "การเชื่อมต่อ Web:"));
    this.webConnectionStatus = createElement("span", {
        id: "web-connection-status",
        class: "data",
    });
    webConnectionInfo.appendChild(this.webConnectionStatus);

    this.appendChild(clientNodeInfo);
    this.appendChild(webConnectionInfo);
};

ConnectivityInfo.prototype.setSSID = function (ssid) {
    this.apSSID.innerHTML = ssid;
};

ConnectivityInfo.prototype.getSSID = function () {
    function payload(data) {
        this.setSSID(data["ssid"]);
    }

    $.getJSON("/apSSID.json", payload.bind(this));
};

ConnectivityInfo.prototype.getClientInfo = function () {
    function payload(data) {
        this.setClientNodeStatus(data["online-nodes"], data["registered-nodes"]);
        this.setWebConnectionStatus(
            data["web-connected"],
            data["web-total"] || data["web-connected"]
        );
    }

    fetch("/clientInfo.json", {
        method: "GET",
        cache: "no-cache",
    })
        .then((response) => response.json())
        .then(payload.bind(this))
        .catch((error) => console.error("Failed to get client info:", error));
};

ConnectivityInfo.prototype.setClientNodeStatus = function (onlineCount, registeredCount) {
    this.clientNodeStatus.innerHTML = onlineCount + " / " + registeredCount;
};

ConnectivityInfo.prototype.setWebConnectionStatus = function (connectedCount, totalCount) {
    this.webConnectionStatus.innerHTML = connectedCount + " / " + totalCount;
};

////////////////////////////////////////
// WIFI Connection
////////////////////////////////////////
const WiFiConnection = function (connectionInfo) {
    Section.call(this, "wifi-connection", "เชื่อมต่อ WIFI");

    this.connectionInfo = connectionInfo;
    this.wifiConnectInterval = null;
    this.connectAttemptStartedAt = 0;
    this.connectFailureGraceMs = 8000;

    this.createBodyInfo();
};
WiFiConnection.prototype = Object.create(Section.prototype);

WiFiConnection.prototype.createBodyInfo = function () {
    this.ssidInput = createElement("input", {
        type: "text",
        maxLength: 32,
        id: "ssid-input",
        placeholder: "SSID",
    });
    this.passwordInput = createElement("input", {
        type: "password",
        maxLength: 64,
        id: "password-input",
        placeholder: "รหัสผ่าน",
    });
    const showPasswordLabel = createElement("label", {});
    const showPasswordCheckbox = createElement("input", {
        type: "checkbox",
        id: "show-password",
    });
    showPasswordLabel.appendChild(showPasswordCheckbox);
    showPasswordLabel.appendChild(createElement("span", {}, "แสดงรหัสผ่าน"));

    const showPassword_onClick = function () {
        var x = document.getElementById("password-input");
        if (x.type === "password") {
            x.type = "text";
        } else {
            x.type = "password";
        }
    };
    showPasswordCheckbox.addEventListener("click", showPassword_onClick);

    this.connectButton = createElement(
        "button",
        { id: "connect-button" },
        "เชื่อมต่อ"
    );
    this.connectButton.addEventListener(
        "click",
        this.checkCredentials.bind(this)
    );

    this.credentialErrors = createElement("div", {
        id: "wifi-connect-credentials-errors",
    });
    this.connectionStatus = createElement("div", { id: "wifi-connect-status" });

    this.appendChild(this.ssidInput);
    this.appendChild(this.passwordInput);
    this.appendChild(showPasswordLabel);
    this.appendChild(this.connectButton);
    this.appendChild(this.credentialErrors);
    this.appendChild(this.connectionStatus);

    $(this.connectionStatus).hide();
};

WiFiConnection.prototype.checkCredentials = function () {
    let errorList = "";
    let credsOk = true;

    const selectedSSID = $(this.ssidInput).val();
    const pwd = $(this.passwordInput).val();

    if (selectedSSID == "") {
        errorList += "<h4 class='rd'>SSID cannot be empty!</h4>";
        credsOk = false;
    }
    if (pwd == "") {
        errorList += "<h4 class='rd'>Password cannot be empty!</h4>";
        credsOk = false;
    }

    if (credsOk == false) {
        this.credentialErrors.innerHTML = errorList;
    } else {
        this.credentialErrors.innerHTML = "";
        this.connectWifi();
    }
};

WiFiConnection.prototype.connectWifi = function () {
    // Get the SSID and password
    const selectedSSID = $(this.ssidInput).val();
    const pwd = $(this.passwordInput).val();

    this.stopWifiConnectStatusInterval();
    this.connectAttemptStartedAt = Date.now();

    $.ajax({
        url: "/wifiConnect.json",
        dataType: "json",
        method: "POST",
        cache: false,
        headers: { "my-connect-ssid": selectedSSID, "my-connect-pwd": pwd },
        data: { timestamp: Date.now() },
    });

    this.startWifiConnectStatusInterval();
};

WiFiConnection.prototype.startWifiConnectStatusInterval = function () {
    this.wifiConnectInterval = setInterval(
        this.getWifiConnectStatus.bind(this),
        2800
    );
    this.getWifiConnectStatus();
};

WiFiConnection.prototype.stopWifiConnectStatusInterval = function () {
    if (this.wifiConnectInterval != null) {
        clearInterval(this.wifiConnectInterval);
        this.wifiConnectInterval = null;
    }
};

WiFiConnection.prototype.getWifiConnectStatus = function () {
    var requestURL = "/wifiConnectStatus";

    function payload(data) {
        $(this.connectionStatus).show();
        this.connectionStatus.innerHTML = "<h4 class='rd'>Connecting...</h4>";

        if (data.wifi_connect_status == 2) {
            const elapsedMs = Date.now() - this.connectAttemptStartedAt;
            if (elapsedMs < this.connectFailureGraceMs) {
                return;
            }
            this.connectionStatus.innerHTML =
                "<h4 class='rd'>Failed to Connect. Please check your AP credentials and compatibility</h4>";
            this.stopWifiConnectStatusInterval();
        } else if (data.wifi_connect_status == 3) {
            this.connectionStatus.innerHTML =
                "<h4 class='gr'>Connection Success!</h4>";
            $(this.ssidInput).val("");
            $(this.passwordInput).val("");
            this.stopWifiConnectStatusInterval();
            this.getConnectInfo();

            setTimeout(() => {
                $(this.connectionStatus).fadeOut("slow");
            }, 5000);
        }
    }

    fetch(requestURL, {
        method: "POST",
        cache: "no-cache",
        body: "wifi_connect_status",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded",
        },
    })
        .then((response) => response.json())
        .then(payload.bind(this));
};

WiFiConnection.prototype.getConnectInfo = function () {
    this.connectionInfo.getConnectInfo();
};

////////////////////////////////////////
// WIFI Connection Information
////////////////////////////////////////
const WiFiConnectionInfo = function () {
    Section.call(this, "wifi-connection-info", "ข้อมูลการเชื่อมต่อ WIFI");

    this.createBodyInfo();
};
WiFiConnectionInfo.prototype = Object.create(Section.prototype);

WiFiConnectionInfo.prototype.createBodyInfo = function () {
    this.connectedAPLabel = createElement(
        "label",
        { id: "connected-ap-label" },
        "เชื่อมต่อไปที่:"
    );
    this.connectedAPContent = createElement("div", {
        id: "connected-ap-content",
        class: "data",
    });
    this.ipAddressLabel = createElement(
        "label",
        { id: "ip-address-label" },
        "IP Address:"
    );
    this.ipAddressContent = createElement("div", {
        id: "ip-address-content",
        class: "data",
    });
    this.netmaskLabel = createElement(
        "label",
        { id: "netmask-label" },
        "Netmask:"
    );
    this.netmaskContent = createElement("div", {
        id: "netmask-content",
        class: "data",
    });
    this.gateWayLabel = createElement(
        "label",
        { id: "gateway-label" },
        "Gateway:"
    );
    this.gatewayContent = createElement("div", {
        id: "gateway-content",
        class: "data",
    });
    this.disconnectButton = createElement(
        "button",
        { id: "disconnect-button" },
        "ตัดการเชื่อมต่อ"
    );

    this.disconnectButton.addEventListener(
        "click",
        this.disconnectWifi.bind(this)
    );

    this.appendChild(this.connectedAPLabel);
    this.appendChild(this.connectedAPContent);
    this.appendChild(this.ipAddressLabel);
    this.appendChild(this.ipAddressContent);
    this.appendChild(this.netmaskLabel);
    this.appendChild(this.netmaskContent);
    this.appendChild(this.gateWayLabel);
    this.appendChild(this.gatewayContent);
    this.appendChild(this.disconnectButton);

    this.hideDisconnectButton();
};

WiFiConnectionInfo.prototype.disconnectWifi = function () {
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
};

WiFiConnectionInfo.prototype.hideDisconnectButton = function () {
    $(this.disconnectButton).hide();
};

WiFiConnectionInfo.prototype.setConnectedAPName = function (data) {
    this.connectedAPContent.innerHTML = data;
};

WiFiConnectionInfo.prototype.getConnectedAPName = function () {
    return this.connectedAPContent.innerHTML;
};

WiFiConnectionInfo.prototype.setIPAddress = function (ipAddress) {
    this.ipAddressContent.innerHTML = ipAddress;
};

WiFiConnectionInfo.prototype.setNetmask = function (netmask) {
    this.netmaskContent.innerHTML = netmask;
};

WiFiConnectionInfo.prototype.setGateway = function (gateway) {
    this.gatewayContent.innerHTML = gateway;
};

WiFiConnectionInfo.prototype.getConnectInfo = function () {
    var requestURL = "/wifiConnectInfo.json";

    function payload(data) {
        if (parseInt(data["wifi_connect_status"]) === 3) {
            this.setConnectedAPName(data["ap"]);
            this.setIPAddress(data["ip"]);
            this.setNetmask(data["netmask"]);
            this.setGateway(data["gw"]);
            $(this.disconnectButton).show();
        } else {
            this.setConnectedAPName("Not Connected");
            this.setIPAddress("0.0.0.0");
            this.setNetmask("0.0.0.0");
            this.setGateway("0.0.0.0");
            this.hideDisconnectButton();
        }
    }

    fetch(requestURL)
        .then((response) => response.json())
        .then(payload.bind(this));
};

////////////////////////////////////////
// Firmware Update
////////////////////////////////////////
const FirmwareUpdate = function () {
    Section.call(this, "firmware-update-ota", "อัพเดตเฟิร์มแวร์");

    this.seconds = null;
    this.otaTimerVar = null;

    this.createBodyInfo();
};
FirmwareUpdate.prototype = Object.create(Section.prototype);

FirmwareUpdate.prototype.createBodyInfo = function () {
    const label = createElement(
        "label",
        { id: "latest-firmware-label" },
        "เฟิร์มแวร์ล่าสุด:"
    );
    this.latestFirmwareDate = createElement("div", {
        id: "latest-firmware-date",
        class: "data",
    });
    this.selectFile = createElement("input", {
        type: "file",
        id: "select-file",
        accept: ".bin",
        style: "display: none;",
    });
    const divButtons = createElement("div", { class: "buttons" });
    const selectFileButton = createElement("button", {}, "เลือกไฟล์");
    const updateButton = createElement("button", {}, "อัพเดตเฟิร์มแวร์");
    divButtons.appendChild(selectFileButton);
    divButtons.appendChild(updateButton);

    this.selectFile.addEventListener("change", () => {
        let file = this.selectFile.files[0];

        this.fileInfo.innerHTML =
            "File: " + file.name + "<br>" + "Size: " + file.size + " bytes";
    });

    selectFileButton.addEventListener("click", () => {
        this.selectFile.click();
    });

    updateButton.addEventListener("click", this.updateFirmware.bind(this));

    this.fileInfo = createElement("h4", { id: "file-info", class: "data" });
    this.otaUpdateStatus = createElement("h4", {
        id: "ota-update-status",
        class: "data",
    });

    this.appendChild(label);
    this.appendChild(this.latestFirmwareDate);
    this.appendChild(this.selectFile);
    this.appendChild(divButtons);
    this.appendChild(this.fileInfo);
    this.appendChild(this.otaUpdateStatus);
};

FirmwareUpdate.prototype.updateFirmware = function () {
    let fileSelect = this.selectFile;

    if (fileSelect.files && fileSelect.files.length == 1) {
        var file = fileSelect.files[0];
        this.otaUpdateStatus.innerHTML =
            "เฟิร์มแวร์ไฟล์ " +
            file.name +
            ", อยู่ในระหว่างการกำลังอัพเดตเฟิร์มแวร์...";

        // Http Request
        let request = new XMLHttpRequest();

        request.upload.addEventListener("progress", this.updateProgress.bind(this));
        request.addEventListener("load", this.getUpdateStatus.bind(this));
        request.addEventListener("error", () => {
            this.setOTAUpdateStatus("!!! Upload Error !!!");
        });
        request.open("POST", "/OTAupdate");
        request.setRequestHeader("Content-Type", "application/octet-stream");
        request.send(file);
    } else {
        window.alert("Select A File First");
    }
};

FirmwareUpdate.prototype.updateProgress = function (oEvent) {
    if (oEvent.lengthComputable) {
        this.getUpdateStatus();
    } else {
        window.alert("total size is unknown");
    }
};

FirmwareUpdate.prototype.setOTAUpdateStatus = function (status) {
    this.otaUpdateStatus.innerHTML = status;
};

FirmwareUpdate.prototype.setLatestFirmwareDate = function (latestDate) {
    this.latestFirmwareDate.innerHTML = "&nbsp;" + latestDate;
};

FirmwareUpdate.prototype.otaRebootTimer = function () {
    this.setOTAUpdateStatus(
        "OTA: อัพเดตเฟิร์มแวร์เรียบร้อยแล้ว โปรแกรมกำลังจะปิดในอีกซักครู่เพื่อเริ่มต้นระบบใหม่ในอีก: " +
        this.seconds
    );

    if (--this.seconds == 0) {
        clearTimeout(this.otaTimerVar);
        window.location.reload();
    } else {
        this.otaTimerVar = setTimeout(this.otaRebootTimer.bind(this), 1000);
    }
};

FirmwareUpdate.prototype.getUpdateStatus = function () {
    var requestURL = "/OTAstatus";

    function payload(data) {
        this.setLatestFirmwareDate(data.compile_date + " - " + data.compile_time);

        // If flashing was complete it will return a 1, else -1
        // A return of 0 is just for information on the Latest Firmware request
        if (data.ota_update_status == 1) {
            // Set the countdown timer time
            this.seconds = 10;
            // Start the countdown timer
            this.otaRebootTimer();
        } else if (data.ota_update_status == -1) {
            this.setOTAUpdateStatus("!!! Upload Error !!!");
        }
    }

    fetch(requestURL, {
        method: "POST",
        cache: "no-cache",
        body: "ota_update_status",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded",
        },
    })
        .then((response) => response.json())
        .then(payload.bind(this));
};
////////////////////////////////////////
// Main Program
////////////////////////////////////////
let mainContainer = null;
const SERVER_UNAVAILABLE_TIMEOUT_MS = 15000;
const WEB_SESSION_HEARTBEAT_INTERVAL_MS = 10000;
let serverLastSeenAtMs = Date.now();

function createWebSessionId() {
    if (window.crypto && window.crypto.randomUUID) {
        return window.crypto.randomUUID();
    }
    return "ws-" + Date.now() + "-" + Math.random().toString(16).slice(2);
}

function sendWebSessionHeartbeat(sessionId) {
    return fetch("/webSession.json", {
        method: "POST",
        cache: "no-cache",
        headers: {
            "X-Web-Session-Id": sessionId,
        },
    }).catch((error) => {
        console.warn("Failed to send web session heartbeat", error);
    });
}

function setWebDisabled(disabled) {
    if (disabled) {
        document.body.classList.add("server-unavailable");
    } else {
        document.body.classList.remove("server-unavailable");
    }
}

function markServerAvailable() {
    serverLastSeenAtMs = Date.now();
    setWebDisabled(false);
}

function checkServerAvailabilityTimeout() {
    const elapsedMs = Date.now() - serverLastSeenAtMs;
    if (elapsedMs >= SERVER_UNAVAILABLE_TIMEOUT_MS) {
        setWebDisabled(true);
    }
}

function getESPServerStatus(
    generalInfo,
    systemConfig,
    relayControl,
    wifiConnectionInfo
) {
    $.getJSON("/ESPServerStatus.json")
        .done(function (data) {
            markServerAvailable();
            const sensorDataAvailable = !!data["sensor-data-available"];
            if (sensorDataAvailable) {
                generalInfo.setDataAvailableState(data["time"]);
                generalInfo.setCurrentTime(data["time"]);
                generalInfo.setTemperatureReading(data["temp"]);
                generalInfo.setHumidityReading(data["humidity"]);
                generalInfo.setSoilMoistureReading(data["soil-moisture"]);
            } else {
                generalInfo.setNoDataState();
            }

            systemConfig.setMinMoistureLevel(data["min-moiture-level"]);
            systemConfig.setMaxMoistureLevel(data["max-moiture-level"]);
            systemConfig.setMaxTimeSpend(data["duration"]);

            if (data["relay-status"]) {
                relayControl.setRelayButtonStatus(data["relay-status"]);
            }

            if (parseInt(data["wifi-connect-status"]) == 3) {
                wifiConnectionInfo.getConnectInfo();
            } else {
                wifiConnectionInfo.setConnectedAPName("Not Connected");
                wifiConnectionInfo.setIPAddress("0.0.0.0");
                wifiConnectionInfo.setNetmask("0.0.0.0");
                wifiConnectionInfo.setGateway("0.0.0.0");
                wifiConnectionInfo.hideDisconnectButton();
            }
        })
        .fail(function () {
            // Watchdog timer handles disable state after timeout window.
            console.warn("ESPServerStatus request failed");
        });
}

$(document).ready(function () {
    const offlineOverlay = createElement(
        "div",
        { id: "server-unavailable-overlay" },
        "Server unavailable - waiting for reconnection"
    );
    document.body.appendChild(offlineOverlay);

    mainContainer = document.getElementById("main-container");

    const generalInfo = new GeneralInfo();
    const systemConfig = new SystemConfig();
    const relayControl = new RelayControl();
    const connectivityInfo = new ConnectivityInfo();
    const wifiConnectionInfo = new WiFiConnectionInfo();
    const wifiConnection = new WiFiConnection(wifiConnectionInfo);
    const firmwareUpdate = new FirmwareUpdate();
    const webSessionId = createWebSessionId();

    $(relayControl.relayButton).hide();
    relayControl.getRelayStatus();
    sendWebSessionHeartbeat(webSessionId);
    setInterval(function () {
        sendWebSessionHeartbeat(webSessionId);
    }, WEB_SESSION_HEARTBEAT_INTERVAL_MS);
    connectivityInfo.getSSID();
    connectivityInfo.getClientInfo();
    setInterval(connectivityInfo.getClientInfo.bind(connectivityInfo), 10000);
    firmwareUpdate.getUpdateStatus();

    mainContainer.appendChild(generalInfo);
    mainContainer.appendChild(relayControl);
    mainContainer.appendChild(connectivityInfo);
    mainContainer.appendChild(wifiConnection);
    mainContainer.appendChild(wifiConnectionInfo);
    mainContainer.appendChild(systemConfig);
    mainContainer.appendChild(firmwareUpdate);

    const getESPServerStatusBind = getESPServerStatus.bind(
        null,
        generalInfo,
        systemConfig,
        relayControl,
        wifiConnectionInfo
    );
    getESPServerStatusBind();
    setInterval(getESPServerStatusBind, 5000);
    setInterval(checkServerAvailabilityTimeout, 1000);
});
