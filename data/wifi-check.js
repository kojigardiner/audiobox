ssidStatus = document.querySelector(".ssid-status");
updateStatus();

// Handle server sent events
if (!!window.EventSource) {
    var source = new EventSource('/events');

    source.addEventListener('open', function (e) {
        console.log("Events Connected");
    }, false);

    source.addEventListener('error', function (e) {
        if (e.target.readyState != EventSource.OPEN) {
            console.log("Events Disconnected");
        }
    }, false);

    source.addEventListener('message', function (e) {
        console.log("message", e.data);
    }, false);

    source.addEventListener('wifi', function (e) {
        console.log("wifi", e.data);
        ssidStatus.textContent = e.data;
        updateStatus();
    }, false);
}

// Update status color
function updateStatus() {
    switch (ssidStatus.textContent.toLowerCase()) {
        case "connected":
            ssidStatus.style.backgroundColor = "rgb(98, 199, 85)";
            break;
        default:
            ssidStatus.style.backgroundColor = "rgb(234, 88, 76)";
            elements = document.querySelectorAll(".needs-wifi");
            elements.forEach(element => {
                element.style.backgroundColor = "black";
                element.style.color = "gray";
                element.setAttribute("href", "#");
            });
            break;
    }
}