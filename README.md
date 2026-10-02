# Ransom

Ransom from DOORS: The Archives, in Geometry Dash.

Each attempt has a 1/N chance (setting) for Ransom to flash on screen for 0.5s,
1-15s into the attempt. Jump while it's visible and Ransom attacks: your pause
menu gets encrypted, every button is locked, and you have to collect the coins
before the timer runs out. Pay in time and you keep playing; run out of time and
you die.

## Resources

- `resources/ransom.png`: Ransom's face (spawn and fail jumpscare)
- `resources/ransom-button.png`: icon on locked pause buttons
- `resources/ransom-<node-id>.png`: optional per-button icon (e.g. `ransom-play-button.png`)
- `resources/encryption.png`: the ransom note shown during the minigame
- `resources/attack/attack_NN.png`: attack animation frames (10 fps)
- `resources/sfx/`: `spawn.ogg`, `attack.ogg`, `coin.mp3`, `paid.ogg`, `fail.mp3`
