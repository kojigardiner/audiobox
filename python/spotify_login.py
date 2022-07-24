"""
Script for testing Spotify authorization flow
"""

import base64
import random

import requests
from httpx import BasicAuth

SPOTIFY_REDIRECT_URI = "http://httpbin.org/anything"
SPOTIFY_AUTH_URL = "https://accounts.spotify.com/authorize"
SPOTIFY_TOKEN_URL = "https://accounts.spotify.com/api/token"
SPOTIFY_SCOPES = "user-read-playback-state" + " " + "user-read-playback-position"
SPOTIFY_PLAYER_URL = "https://api.spotify.com/v1/me/player"
SPOTIFY_FEATURES_URL = "https://api.spotify.com/v1/audio-features"


def gen_random_str(length: int) -> str:
    chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    s = ""
    for i in range(length):
        s += chars[random.randint(0, len(chars) - 1)]
    return s


# We need two spotify flows
# 1. Store the client ID/secret as part of initialization of a new ESP32 device. This should only be accessible to the developer and the keys should not be user accessible.
# 2. Authorize a new user. This should be user-accessible.


"""
Store client ID/secret
"""
client_id = input("Enter client ID: ")
client_secret = input("Enter client secret: ")

"""
Request user authorization
"""
payload = {
    "client_id": client_id,
    "response_type": "code",
    "redirect_uri": SPOTIFY_REDIRECT_URI,
    "state": gen_random_str(16),
    "scope": SPOTIFY_SCOPES,
}

r = requests.get(SPOTIFY_AUTH_URL, params=payload)
if r.status_code != requests.codes.ok:
    print("failed to access authorization endpoint")

print("Launch the following URL in your browser to authorize access: " + r.url)
print("After granting access, you will be redirected to a webpage.\n")

auth_code = input(
    'Copy and paste the characters after "code: " on the webpage. Do not include the quotation (") marks: '
)
print(r.request.headers)

"""
Request access token
"""
payload = {
    "grant_type": "authorization_code",
    "code": auth_code,
    "redirect_uri": SPOTIFY_REDIRECT_URI,
}

auth_b64 = base64.b64encode(f"{client_id}:{client_secret}".encode())
h = {
    "Authorization": f"Basic {auth_b64.decode()}",
    "Content-Type": "application/x-www-form-urlencoded",
}

r = requests.post(SPOTIFY_TOKEN_URL, headers=h, params=payload)
if r.status_code != requests.codes.ok:
    print("failed to obtain token")
print(r.json())
access_token = r.json()["access_token"]
refresh_token = r.json()["refresh_token"]

"""
Request refreshed access token
"""
payload = {"grant_type": "refresh_token", "refresh_token": refresh_token}

auth_b64 = base64.b64encode(f"{client_id}:{client_secret}".encode())
h = {
    "Authorization": f"Basic {auth_b64.decode()}",
    "Content-Type": "application/x-www-form-urlencoded",
}

r = requests.post(SPOTIFY_TOKEN_URL, headers=h, params=payload)
if r.status_code != requests.codes.ok:
    print("failed to obtain token")
print(r.json())
