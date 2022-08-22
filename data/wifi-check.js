ssidStatus = document.querySelector(".ssid-status");
spotifyActive = document.querySelector(".spotify-active");
albumArt = document.querySelector(".album-art");
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
        const large_img_url = get_large_art_url(e.data);
        albumArt.setAttribute("src", large_img_url);
    }, false);
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
            break;
    }
}

function updateSpotifyStatus() {
    switch (spotifyActive.textContent.toLowerCase()) {
        case "active":
            spotifyActive.style.backgroundColor = "rgb(98, 199, 85)";
            break;
        default:
            spotifyActive.style.backgroundColor = "gray";
            break;
    }
}

function get_large_art_url(url) {
    const url_obj = new URL(url);
    const url_parts = url_obj.pathname.split("/");
    const img_id = url_parts.at(-1);

    const large_tag = "b273"
    const img_tag_start = 12;

    const img_large_id = img_id.slice(0, img_tag_start) + large_tag + img_id.slice(img_tag_start + large_tag.length);
    const new_url = [url_obj.origin, ...url_parts.slice(1, -1), img_large_id].join("/");
    return new_url;
}