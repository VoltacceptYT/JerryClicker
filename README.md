# Jerry Clicker (Web)

The original Jerry Clicker: a clicker game starring Jerry, playable in any modern web browser. No build step, no dependencies, just open `index.html`.

> Looking for another version? See the [`main` branch](https://github.com/VoltacceptYT/JerryClicker) for the table of contents

## How to Play

Click Jerry to earn **Jerry Candies**, then spend them on upgrades that boost your candies per click or generate candies automatically every second.

| Stat | Meaning |
|---|---|
| Jerry Candies | Your current balance |
| Per Click | Candies earned each time you click Jerry |
| Per Second | Candies generated automatically by your upgrades |
| Prestige Multiplier | Permanent bonus from prestiging (shown once prestige is unlocked) |

### Upgrades

- **Auto-generators**: Inflatable Jerry, the Jerry Boxes (Green, Blue, Purple, Golden, Mega), Jerry Runes I-III and the Jerry Talismans (Green, Blue, Purple, Golden) add candies per second. Each one gets more expensive every time you buy it.
- **Jerry Staff**: Quadruples your auto-generation rate.
- **Jerry Stone**: Adds +50 candies per click.
- **Cosmetics**: Pet items that change how Jerry looks (Jerry 3D Glasses, Lord's Crown, Scuba Mask, Holy Halo, Iron Totem). Only one can be active at a time; swapping refunds half the old item's cost.

### Prestige

Buy **Aspect of the Jerry** to unlock the prestige system. Once you have enough candies (150M x your current multiplier), press **Prestige** to reset your progress in exchange for a permanent multiplier. **Aspect of the Jerry (Signature)** doubles the multiplier you gain from prestiging.

### Golden Jerry

Every 3 to 5 minutes a Golden Jerry appears at a random spot on the screen and disappears after 13 seconds. Click it for a random bonus:

| Bonus | Effect |
|---|---|
| Candy Windfall | A big lump sum of candies based on your auto-generation rate |
| Frenzy | 2x production for 30 seconds |
| Lucky | A bonus based on your current candies |

## Saving

- The game **autosaves to your browser's local storage every 30 seconds** and loads it when you come back.
- **Export Save** downloads your save as `JerryClicker.txt`, and **Import Save** loads one back in. Use these to back up your progress or move it between browsers/devices.
- Clearing your browser data erases the local save, so export first if you care about your progress.
- The game has basic anti-tamper protection. Editing the game state from the browser console will wipe your save and reload the page.

## Running Locally

Clone the repo, switch to this branch, and open `index.html` in your browser:

```bash
git clone -b web-port https://github.com/VoltacceptYT/JerryClicker.git
cd JerryClicker
# then open index.html in a browser
```

Or serve it with any static file server:

```bash
python3 -m http.server 8000
# visit http://localhost:8000
```

## Project Structure

```
index.html              Page layout
assets/
  css/styles.css        Styling
  js/script.js          All game logic
  font/                 Jerrified Pixels font
  img/                  UI, cosmetic and power-up sprites
api/
  jerrified/items.json  Item names and IDs
  resources/            Upgrade icons
```

## License

Released under the [MIT License](LICENSE).

## Credits

- **Game:** VoltacceptYT
- **Sprites:** Mojang / VoltacceptYT
- **Music:** VO!DWiNG

### Other Thanks

- **Hypixel** - Creating Jerry Himself