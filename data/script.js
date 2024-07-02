if (!!window.EventSource) {
    var source = new EventSource('/events');

    source.addEventListener('open', function (e) {
        console.log("Events Connected");
        document.getElementById("connection_prog").hidden = true;
    }, false);
    source.addEventListener('error', function (e) {
        if (e.target.readyState != EventSource.OPEN) {
            console.log("Events Disconnected");
            document.getElementById("connection_prog").hidden = false;
        }
    }, false);

    source.addEventListener('temperature', function (e) {
        document.getElementById("temp").innerHTML = e.data;
    }, false);

    source.addEventListener('humidity', function (e) {
        document.getElementById("hum").innerHTML = e.data;
    }, false);

    source.addEventListener('heater', function (e) {
        document.getElementById("heat").checked = e.data === "on";
    }, false);

    source.addEventListener('fan', function (e) {
        document.getElementById("vent").checked = e.data === "on";
    }, false);

    source.addEventListener('auto', function (e) {
        document.getElementById("auto").checked = e.data === "on";
        disableControls(e.data === "on");
    }, false);

    source.addEventListener('tempMin', function (e) {
        document.getElementById("targetTempMin").value = e.data;
    }, false);

    source.addEventListener('tempMax', function (e) {
        document.getElementById("targetTempMax").value = e.data;
    }, false);

    source.addEventListener('humMin', function (e) {
        document.getElementById("targetHumMin").value = e.data;
    }, false);

    source.addEventListener('humMax', function (e) {
        document.getElementById("targetHumMax").value = e.data;
    }, false);
}

// disable the manual controls when auto is enabled
function disableControls(state) {
    document.getElementById("heat").disabled = state;
    document.getElementById("vent").disabled = state;
    if (state) {
        document.getElementById("heater_tooltip").innerHTML = "Automatic heater control";
        document.getElementById("vent_tooltip").innerHTML = "Automatic ventilation control";
    }
    else {
        document.getElementById("heater_tooltip").innerHTML = "Manual heater control";
        document.getElementById("vent_tooltip").innerHTML = "Manual ventilation control";
    }
}

// control the heater using the api
function toggleHeater(event) {
    const state = event.target.checked;
    fetch("/api/heater?enabled=" + state);
}

// control the fan using the api
function toggleFan(event) {
    const state = event.target.checked;
    fetch("/api/fan?enabled=" + state);
}
``
// set the target temperature using the api
// setHumRange?min=30&max=70
function setTempRange() {
    const min = document.getElementById("targetTempMin").value;
    const max = document.getElementById("targetTempMax").value;
    fetch("/api/setTempRange?min=" + min + "&max=" + max);
}

// set the target humidity using the api
function setHumRange() {
    const min = document.getElementById("targetHumMin").value;
    const max = document.getElementById("targetHumMax").value;
    fetch("/api/setHumRange?min=" + min + "&max=" + max);
}

function toggleAuto(event) {
    //disable the other 2 swiches
    const state = event.target.checked;
    fetch("/api/auto?enabled=" + state);
    disableControls(state);
}

// reboot the device
function reboot() {
    fetch("/reboot");
    console.log("Rebooting...");
}

//handle ui state
function handleTheme() {
    //save the state in the local storage
    //set default to dark 
    ui('mode', ui('mode') == 'dark' ? 'light' : 'dark');
    //save the theme in the local storage
    const theme = ui('mode');
    localStorage.setItem("theme", theme);
}

window.addEventListener("load", function () {
    //load the theme from the local storage
    console.log("Loading previous theme...");
    const theme = localStorage.getItem("theme");
    ui('mode', theme);
}); 