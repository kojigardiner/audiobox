let paletteSize = 4;
let paletteMargin = 1;
const ssidStatus = document.querySelector(".ssid-status");
const spotifyActive = document.querySelector(".spotify-active");
const albumArt = document.querySelector(".album-art");
const album = document.querySelector(".album");
const buttons = document.querySelector(".buttons");
const message = document.querySelector(".message");
createPalette(paletteSize);
updateWifiStatus();
updateSpotifyStatus();

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
        updateWifiStatus();
    }, false);

    source.addEventListener('spotify_active', function (e) {
        console.log("spotify_active", e.data);
        spotifyActive.textContent = e.data;
        updateSpotifyStatus();
    }, false);

    source.addEventListener('spotify_art_url', function (e) {
        console.log("spotify_art_url", e.data);
        if (e.data !== "") {
            const large_img_url = getLargeArtUrl(e.data);
            albumArt.setAttribute("src", large_img_url);
            album.classList.remove("hidden");
        }
    }, false);

    source.addEventListener('palette', function (e) {
        console.log("palette", e.data);
        parsePalette(e.data);
        album.classList.remove("hidden");
    }, false);
}

function createPalette(paletteSize) {
    const container = document.querySelector(".palette");

    if (container !== null) {
        container.replaceChildren();    // clear the existing elements

        // create the grid elements
        for (let i = 0; i < paletteSize; i++) {
            const divRow = document.createElement("div");
            divRow.classList.add("paletteRow");
            container.appendChild(divRow);
            for (let j = 0; j < paletteSize; j++) {
                const divElement = document.createElement("div");
                divElement.classList.add(`entry${j}`);
                divElement.classList.add("paletteEntry");
                divElement.style.backgroundColor = "black";
                divElement.style.width = `${albumArt.width / paletteSize}px`;
                divElement.style.height = `${albumArt.width / paletteSize}px`;
                divElement.style.margin = `${paletteMargin}px`;
                divRow.appendChild(divElement);
            }
        }
    }
}

// Update status color
function updateWifiStatus() {
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
            changeControlVisibility(false);
            break;
    }
}

// Update status bar
function updateSpotifyStatus() {
    switch (spotifyActive.textContent.toLowerCase()) {
        case "active":
            spotifyActive.style.backgroundColor = "rgb(98, 199, 85)";
            changeControlVisibility(true);
            break;
        default:
            spotifyActive.style.backgroundColor = "gray";
            changeControlVisibility(false);
            break;
    }
}

// Convert url to get the 640x640 image
function getLargeArtUrl(url) {
    const url_obj = new URL(url);
    const url_parts = url_obj.pathname.split("/");
    const img_id = url_parts.at(-1);

    // by inspection, can determine that replacing the characters here results in a
    // link to the 640x640 image
    const large_tag = "b273"
    const img_tag_start = 12;

    const img_large_id = img_id.slice(0, img_tag_start) + large_tag + img_id.slice(img_tag_start + large_tag.length);
    const new_url = [url_obj.origin, ...url_parts.slice(1, -1), img_large_id].join("/");
    return new_url;
}

// parse the palette string sent by the box
function parsePalette(paletteStr) {
    colorStrArr = paletteStr.split("\n").map(element => `rgb(${element})`);

    entries = Array.from(document.querySelectorAll(".paletteEntry"));

    for (let i = 0; i < entries.length; i++) {
        entries.at(i).style.backgroundColor = colorStrArr.at(i);
    }
}

function changeControlVisibility(turnOn) {
    if (buttons !== null && album !== null && message !== null) {
        if (turnOn) {
            buttons.classList.remove("hidden");
            album.classList.remove("hidden");
            message.classList.add("hidden");
        } else {
            buttons.classList.add("hidden");
            album.classList.add("hidden");
            message.classList.remove("hidden");
        }
    }
}