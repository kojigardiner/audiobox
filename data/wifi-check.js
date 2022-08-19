// Update status color
ssidStatus = document.querySelector(".ssid-status");
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