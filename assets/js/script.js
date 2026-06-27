// Game state
let gameState = {
  candies: 0,
  perClick: 1,
  autoRate: 0,
  upgrades: {},
  prestigeLevel: 0,
  prestigeMultiplier: 1,
};

// Upgrade definitions
var upgrades = [
  {
    id: "inflatable_jerry",
    name: "Inflatable Jerry",
    cost: 75,
    effect: "+0.1 candy/sec auto-gen",
    type: "auto",
    value: 0.1,
    upgrade: false,
  },
  {
    id: "jerry_box_green",
    name: "Jerry Box (Green)",
    cost: 600,
    effect: "+0.5 candies/sec auto-gen",
    type: "auto",
    value: 0.5,
    upgrade: false,
  },
  {
    id: "jerry_box_blue",
    name: "Jerry Box (Blue)",
    cost: 5000,
    effect: "+3 candies/sec auto-gen",
    type: "auto",
    value: 3,
    upgrade: false,
  },
  {
    id: "jerry_box_purple",
    name: "Jerry Box (Purple)",
    cost: 40000,
    effect: "+15 candies/sec auto-gen",
    type: "auto",
    value: 15,
    upgrade: false,
  },
  {
    id: "jerry_box_golden",
    name: "Jerry Box (Golden)",
    cost: 500000,
    effect: "+80 candies/sec auto-gen",
    type: "auto",
    value: 80,
    upgrade: false,
  },
  {
    id: "jerry_box_mega",
    name: "Jerry Box (Mega)",
    cost: 8000000,
    effect: "+500 candies/sec auto-gen",
    type: "auto",
    value: 500,
    upgrade: false,
  },
  {
    id: "jerry_staff",
    name: "Jerry Staff",
    cost: 40000000,
    effect: "Quadruples auto-gen rate",
    type: "autoMult",
    value: 4,
  },
  {
    id: "jerry_rune",
    name: "Jerry Rune I",
    cost: 80000000,
    effect: "+2.5k candies/sec auto-gen",
    type: "auto",
    value: 2500,
    upgrade: false,
  },
  {
    id: "jerry_rune_2",
    name: "Jerry Rune II",
    cost: 1500000000,
    effect: "+15k candies/sec auto-gen",
    type: "auto",
    value: 15000,
    upgrade: false,
  },
  {
    id: "jerry_rune_3",
    name: "Jerry Rune III",
    cost: 25000000000,
    effect: "+90k candies/sec auto-gen",
    type: "auto",
    value: 90000,
    upgrade: false,
  },
  {
    id: "jerry_talisman_green",
    name: "Jerry Talisman (Green)",
    cost: 400000000000,
    effect: "+550k candies/sec auto-gen",
    type: "auto",
    value: 550000,
    upgrade: false,
  },
  {
    id: "jerry_talisman_blue",
    name: "Jerry Talisman (Blue)",
    cost: 6000000000000,
    effect: "+3.5M candies/sec auto-gen",
    type: "auto",
    value: 3500000,
    upgrade: false,
  },
  {
    id: "jerry_talisman_purple",
    name: "Jerry Talisman (Purple)",
    cost: 80000000000000,
    effect: "+22M candies/sec auto-gen",
    type: "auto",
    value: 22000000,
    upgrade: false,
  },
  {
    id: "jerry_talisman_golden",
    name: "Jerry Talisman (Golden)",
    cost: 1000000000000000,
    effect: "+150M candies/sec auto-gen",
    type: "auto",
    value: 150000000,
    upgrade: false,
  },
  {
    id: "jerry_stone",
    name: "Jerry Stone",
    cost: 200000,
    effect: "+50 candies per click",
    type: "click",
    value: 50,
    upgrade: true,
  },
  {
    id: "aspect_of_the_jerry",
    name: "Aspect of the Jerry",
    cost: 750000,
    effect: "Unlock prestige system",
    type: "prestige",
    value: 1,
    upgrade: true,
  },
  {
    id: "aspect_of_the_jerry_signature",
    name: "Aspect of the Jerry (Signature)",
    cost: 2000000,
    effect: "Prestige multiplier x2",
    type: "prestigeMult",
    value: 2,
    upgrade: false,
  },
  {
    id: "pet_item_toy_jerry",
    name: "Pet Item: Jerry 3D Glasses",
    cost: 25000000,
    effect: "Adds trippy 3D anaglyph effect to Jerry",
    type: "cosmetic",
    value: 1,
    upgrade: true,
  },
  {
    id: "pet_item_lord_jerry",
    name: "Pet Item: Lord's Crown",
    cost: 25000000,
    effect: "Converts Jerry into Lord Jerry",
    type: "cosmetic",
    value: 1,
    upgrade: true,
  },
  {
    id: "pet_item_scuba_jerry",
    name: "Pet Item: Scuba Mask",
    cost: 25000000,
    effect: "Converts Jerry into Scuba Jerry",
    type: "cosmetic",
    value: 1,
    upgrade: true,
  },
  {
    id: "pet_item_angel_jerry",
    name: "Pet Item: Holy Halo",
    cost: 25000000,
    effect: "Converts Jerry into Angel Jerry",
    type: "cosmetic",
    value: 1,
    upgrade: true,
  },
  {
    id: "pet_item_iron_jerry",
    name: "Pet Item: Iron Totem",
    cost: 25000000,
    effect: "Converts Jerry into Iron Jerry",
    type: "cosmetic",
    value: 1,
    upgrade: true,
  },
];

/* -------------------------
  Robust Anti-Tamper Proxy
-------------------------*/

// Keep a deep copy of original upgrades to reset to defaults
const originalUpgrades = JSON.parse(JSON.stringify(upgrades));

// Proxy cache so the same proxy is reused for the same target
const proxyCache = new WeakMap();

// Suppression counter: when >0, proxy does NOT treat sets as cheating
let __internalSuppression = 0;
function runInternal(fn) {
  __internalSuppression++;
  try {
    return fn();
  } finally {
    __internalSuppression--;
  }
}

function isObject(x) {
  return x && (typeof x === "object" || typeof x === "function");
}

function deepProxy(target, options = {}, path = "") {
  if (!isObject(target)) return target;

  // If we already proxied this object, return cached proxy
  if (proxyCache.has(target)) return proxyCache.get(target);

  const handler = {
    get(t, prop, receiver) {
      if (prop === "__isProxy") return true;
      if (prop === "__raw") return t;

      if (prop === "prototype") {
        return Reflect.get(t, prop, receiver);
      }

      const val = Reflect.get(t, prop, receiver);
      return isObject(val)
        ? deepProxy(val, options, `${path}.${String(prop)}`)
        : val;
    },

    set(t, prop, value, receiver) {
      const oldVal = t[prop];

      // If identical, ignore
      if (oldVal === value) return true;

      // If currently running internal code, allow without treating as cheat
      if (__internalSuppression > 0) {
        return Reflect.set(t, prop, value, receiver);
      }

      const fullPath = path ? `${path}.${String(prop)}` : String(prop);

      // Call onModify handler (if provided) — external change detected
      if (typeof options.onModify === "function") {
        try {
          options.onModify(fullPath, oldVal, value, t, prop);
        } catch (err) {
          console.error("onModify handler error:", err);
        }
      }

      // Apply the change after handler (some handlers may block/apply their own logic)
      return Reflect.set(t, prop, value, receiver);
    },

    deleteProperty(t, prop) {
      // Block deletes from external sources
      if (__internalSuppression > 0) {
        return Reflect.deleteProperty(t, prop);
      }

      const fullPath = path ? `${path}.${String(prop)}` : String(prop);
      if (typeof options.onModify === "function") {
        try {
          options.onModify(fullPath, "deleted", null, t, prop);
        } catch (err) {
          console.error("onModify handler error:", err);
        }
      }
      // Block external deletion
      return false;
    },
  };

  const p = new Proxy(target, handler);
  proxyCache.set(target, p);
  return p;
}

/* -------------------------
  Apply proxies safely
-------------------------*/

// gameState: external mutation => delete save + reload
gameState = deepProxy(gameState, {
  onModify(fullPath, oldVal, newVal) {
    console.warn("CHEAT DETECTED (gameState):", fullPath, oldVal, "→", newVal);

    // Remove save and reload (do it in a timeout so console logs show)
    try {
      localStorage.removeItem("jerryClickerSave");
    } catch (e) {
      console.error("Failed to remove save:", e);
    }

    // Give a tiny delay so the console shows the warning, then reload
    setTimeout(() => location.reload(), 50);
  },
});

// upgrades: external mutation => reset that single field back to original safely
upgrades = deepProxy(upgrades, {
  onModify(fullPath, oldVal, newVal, target, prop) {
    console.warn("CHEAT DETECTED (upgrades):", fullPath, oldVal, "→", newVal);

    // Try to determine the upgrade id for the modified object
    // target could be the upgrade object itself (since we proxied child objects)
    // or the upgrades array if an array index property changed.
    let upgradeObj = null;

    if (Array.isArray(target)) {
      // If modification on array-level (e.g., reassigning an index),
      // attempt to map the index to an original upgrade
      const idx = Number(prop);
      if (!Number.isNaN(idx)) upgradeObj = target[idx];
    } else {
      // target is likely the upgrade object itself
      upgradeObj = target;
    }

    // If we have an id on the object, find the original upgrade
    const id = upgradeObj && upgradeObj.id;
    const original = originalUpgrades.find((u) => u.id === id);

    if (original) {
      // Safely reset only the modified property to default using runInternal
      runInternal(() => {
        try {
          // If prop is numeric string for array case, handle separately
          if (Array.isArray(target) && !Number.isNaN(Number(prop))) {
            // reset entire object at that index to original
            const idx = Number(prop);
            target[idx] = JSON.parse(JSON.stringify(original));
          } else {
            // Reset property on the upgrade object
            target[prop] = original[prop];
          }
          console.info(`Reset upgrade '${id}' property '${prop}' to default.`);
        } catch (err) {
          console.error("Failed to reset upgrade property:", err);
        }
      });
    } else {
      // Fallback: if we couldn't find original, do nothing but warn
      console.warn(
        "Could not find original upgrade to reset for path:",
        fullPath,
      );
    }

    // Re-render/refresh UI safely (internal)
    runInternal(() => {
      try {
        renderUpgrades();
        updateUI();
      } catch (e) {
        // ignore UI errors here
      }
    });
  },
});

/* -------------------------
  Wrap internal functions so their mutations don't trigger cheat
  (supports late-binding: if function isn't declared yet we queue it)
-------------------------*/

const pendingWraps = []; // [{ obj, name }]

const wrapAsInternal = (obj, name) => {
  if (!obj) return;
  const fn = obj[name];
  if (typeof fn === "function") {
    const orig = fn;
    obj[name] = function (...args) {
      return runInternal(() => orig.apply(this, args));
    };
    return true;
  } else {
    // queue for late-binding
    pendingWraps.push({ obj, name });
    return false;
  }
};

function applyPendingWraps() {
  // Try to apply queued wraps; keep ones that still don't exist for later
  for (let i = pendingWraps.length - 1; i >= 0; i--) {
    const { obj, name } = pendingWraps[i];
    if (!obj) {
      pendingWraps.splice(i, 1);
      continue;
    }
    const fn = obj[name];
    if (typeof fn === "function") {
      wrapAsInternal(obj, name); // this will replace and remove from pending
      pendingWraps.splice(i, 1);
    }
  }
}

// Register the global functions we expect to exist (some declared later)
[window].forEach((ctx) => {
  wrapAsInternal(ctx, "buyUpgrade");
  wrapAsInternal(ctx, "applyUpgrade");
  wrapAsInternal(ctx, "loadFromLocalStorage");
  wrapAsInternal(ctx, "saveToLocalStorage");
  wrapAsInternal(ctx, "loadGame");
  wrapAsInternal(ctx, "saveGame");
  wrapAsInternal(ctx, "exportSave");
  wrapAsInternal(ctx, "prestige");
  wrapAsInternal(ctx, "initUI");
  wrapAsInternal(ctx, "updateUI");
  wrapAsInternal(ctx, "renderUpgrades");
});

/* -------------------------
  Safe event wrapper for Jerry click (ensure internal suppression)
  We'll rebind the click to a wrapper that runs internal code while preserving original logic.
-------------------------*/
(function safeBindLordJerryClick() {
  const el = document.getElementById("jerry");
  if (!el) return;

  // Capture any existing handlers attached via addEventListener is tricky;
  // we keep the existing listener (you already added one later), but we'll
  // add a wrapper that runs internal suppression while allowing the other handler to run.
  el.addEventListener(
    "click",
    (ev) => {
      // This wrapper doesn't call original; it only ensures internal suppression
      // during the click event so any mutations done by click handlers won't trigger cheat detection.
      // Actual click logic is still executed by your existing handler; we just suppress detection here.
      // If your click handler runs after this, __internalSuppression will already be >0 for its duration.
      runInternal(() => {
        // no-op: suppression window
      });
    },
    { capture: false },
  );
})();

/* -------------------------
  Initialization (safe)
-------------------------*/

// Initialize upgrades inside internal suppression so their initial sets are not treated as cheats
runInternal(() => {
  upgrades.forEach((upgrade) => {
    gameState.upgrades[upgrade.id] = { count: 0, currentCost: upgrade.cost };
  });
});

// ensure saving interval is scheduled within internal suppression context
runInternal(() => {
  setInterval(() => {
    // call the function by name; it's wrapped later once declared
    try {
      if (typeof saveToLocalStorage === "function") saveToLocalStorage();
    } catch (e) {
      console.error("save interval error:", e);
    }
  }, 30000);
});

/* -------------------------
  UI / Save / Load / Game logic
  (these functions exist as in your original file)
-------------------------*/

/* -------------------------
  Save Migration
  Runs after any load to fix upgrade costs if the source definitions changed.
  Recalculates currentCost based on how many times the player has bought the upgrade,
  using the new base cost — so existing stacks are preserved but old pricing is gone.
-------------------------*/
function migrateSave(state) {
  upgrades.forEach((def) => {
    const entry = state.upgrades[def.id];
    if (!entry) return;

    // Reconstruct what currentCost should be given the new base cost and owned count.
    // buyUpgrade applies *1.15 per purchase, so: currentCost = baseCost * 1.15^count
    const expectedCost = def.cost * Math.pow(1.15, entry.count);

    // If the stored cost doesn't match (old pricing), update it.
    // Use a small epsilon to avoid floating-point false positives.
    if (Math.abs(entry.currentCost - expectedCost) / expectedCost > 0.001) {
      entry.currentCost = expectedCost;
    }
  });

  return state;
}

function showPopup(message, options = {}) {
  const popup = document.getElementById("popup");
  const msg = document.getElementById("popup-message");
  const okBtn = document.getElementById("popup-ok");
  const cancelBtn = document.getElementById("popup-cancel");

  msg.textContent = message;
  popup.classList.remove("hidden");

  // Reset buttons
  cancelBtn.classList.add("hidden");

  return new Promise((resolve) => {
    okBtn.onclick = () => {
      popup.classList.add("hidden");
      resolve(true);
    };

    if (options.confirm) {
      cancelBtn.classList.remove("hidden");
      cancelBtn.onclick = () => {
        popup.classList.add("hidden");
        resolve(false);
      };
    }
  });
}

// Shared helper: apply a loaded/parsed gameState object and refresh all UI + cosmetics.
// Must be called from within runInternal() or will be suppressed by caller.
function applyLoadedState(loaded) {
  runInternal(() => {
    gameState = loaded;
  });

  const jerryEl = document.getElementById("jerry");

  // Clear ALL cosmetic classes first so no bleed-through from a previous session
  jerryEl.classList.remove(
    "glasses-3d", "fast-animate", "lord", "scuba", "iron", "angel"
  );

  // Re-apply whichever cosmetic is active in the loaded save
  const cosmeticMap = {
    pet_item_toy_jerry:   () => { jerryEl.classList.add("glasses-3d", "fast-animate"); },
    pet_item_lord_jerry:  () => { jerryEl.classList.add("lord"); },
    pet_item_scuba_jerry: () => { jerryEl.classList.add("scuba"); },
    pet_item_iron_jerry:  () => { jerryEl.classList.add("iron"); },
    pet_item_angel_jerry: () => { jerryEl.classList.add("angel"); },
  };
  for (const [id, apply] of Object.entries(cosmeticMap)) {
    if (gameState.upgrades[id]?.count > 0) {
      apply();
      break; // only one cosmetic active at a time
    }
  }

  // Show / hide prestige UI
  if (gameState.upgrades["aspect_of_the_jerry"]?.count > 0) {
    document.getElementById("prestige-info").style.display = "block";
    document.getElementById("prestige-btn").disabled = false;
  } else {
    document.getElementById("prestige-info").style.display = "none";
    document.getElementById("prestige-btn").disabled = true;
  }

  // Full UI re-render (not just updateUI) so the upgrades panel reflects the new state
  initUI();
}

function loadFromLocalStorage() {
  try {
    const saved = localStorage.getItem("jerryClickerSave");
    if (saved) {
      const obfuscated = atob(saved);
      const key = 42;
      let data = "";
      for (let i = 0; i < obfuscated.length; i++) {
        data += String.fromCharCode(obfuscated.charCodeAt(i) ^ key);
      }
      const loaded = migrateSave(JSON.parse(data));
      applyLoadedState(loaded);
    }
  } catch (err) {
    console.error("Failed to load from localStorage:", err);
  }
}

function saveToLocalStorage() {
  try {
    const data = JSON.stringify(gameState);
    const key = 42;
    let obfuscated = "";
    for (let i = 0; i < data.length; i++) {
      obfuscated += String.fromCharCode(data.charCodeAt(i) ^ key);
    }
    const encoded = btoa(obfuscated);
    localStorage.setItem("jerryClickerSave", encoded);
  } catch (err) {
    console.error("Failed to save to localStorage:", err);
  }
}

function getActiveCosmetic() {
  const cosmeticIds = upgrades
    .filter((u) => u.type === "cosmetic")
    .map((u) => u.id);
  for (const id of cosmeticIds) {
    if (gameState.upgrades[id]?.count > 0) {
      return id;
    }
  }
  return null;
}

function renderUpgrades() {
  const list = document.getElementById("upgrades-list");
  list.innerHTML = "";

  upgrades.forEach((upgrade) => {
    const state = gameState.upgrades[upgrade.id];
    const owned = state?.count || 0;
    const cost = state?.currentCost || upgrade.cost;
    const canAfford = gameState.candies >= cost;
    const isSinglePurchase = upgrade.upgrade === true && owned >= 1;

    const lockedClass = !canAfford || isSinglePurchase ? "locked" : "";

    const div = document.createElement("div");
    div.className = `upgrade-item ${lockedClass} ${owned > 0 ? "owned" : ""}`;
    div.setAttribute("data-id", upgrade.id);

    // Disable click when maxed
    if (!isSinglePurchase) {
      div.onclick = () => buyUpgrade(upgrade.id);
    }

    div.innerHTML = `
      <div class="upgrade-icon" style="background-image: url('./api/resources/${upgrade.id}.png')"></div>
      <div class="upgrade-info">
        <div class="upgrade-name">${upgrade.name}</div>
        <div class="upgrade-effect">${upgrade.effect}</div>
        <div class="upgrade-cost">Cost: ${formatNumber(cost)} candies</div>
        ${owned > 0 ? `<div class="upgrade-owned">Owned: ${owned}${isSinglePurchase ? " (max)" : ""}</div>` : ""}
      </div>
    `;
    list.appendChild(div);
  });
}

function updateUpgradeLocks() {
  upgrades.forEach((upgrade) => {
    const state = gameState.upgrades[upgrade.id];
    const cost = state?.currentCost || upgrade.cost;
    const canAfford = gameState.candies >= cost;
    const isPrestigeSingle =
      (upgrade.type === "prestige" || upgrade.type === "prestigeMult") &&
      (state?.count || 0) >= 1;

    const el = document.querySelector(`.upgrade-item[data-id="${upgrade.id}"]`);
    if (el) {
      if (isPrestigeSingle || !canAfford) {
        el.classList.add("locked");
      } else {
        el.classList.remove("locked");
      }
    }
  });
}

function buyUpgrade(id) {
  const upgrade = upgrades.find((u) => u.id === id);
  const state = gameState.upgrades[id];

  // Block multiple purchases for single-purchase upgrades
  if (upgrade.upgrade === true && state.count >= 1) {
    return;
  }

  if (gameState.candies >= state.currentCost) {
    // Cosmetic swap logic
    if (upgrade.type === "cosmetic") {
      const activeCosmetic = getActiveCosmetic();
      if (activeCosmetic && activeCosmetic !== id) {
        const oldUpgrade = upgrades.find((u) => u.id === activeCosmetic);
        const oldState = gameState.upgrades[activeCosmetic];

        // Refund half the old cost
        const refund = Math.floor(oldState.currentCost / 2);
        gameState.candies += refund;

        // Reset old cosmetic
        oldState.count = 0;
        oldState.currentCost = oldUpgrade.cost;

        // Remove old cosmetic classes
        document
          .getElementById("jerry")
          .classList.remove("glasses-3d", "fast-animate", "lord", "scuba");
      }
    }

    // Deduct cost and apply new upgrade
    gameState.candies -= state.currentCost;
    state.count++;
    state.currentCost = state.currentCost * 1.5;

    applyUpgrade(upgrade);
    initUI();
  }
}

function applyUpgrade(upgrade) {
  const mult = gameState.prestigeMultiplier;

  switch (upgrade.type) {
    case "click":
      gameState.perClick += upgrade.value * mult;
      break;
    case "auto":
      gameState.autoRate += upgrade.value * mult;
      break;
    case "autoMult":
      gameState.autoRate *= upgrade.value;
      break;
    case "globalMult":
      gameState.perClick *= upgrade.value;
      gameState.autoRate *= upgrade.value;
      break;
    case "prestige":
      document.getElementById("prestige-info").style.display = "block";
      document.getElementById("prestige-btn").disabled = false;
      break;
    case "prestigeMult":
      // Applied during prestige
      break;
    case "cosmetic":
      if (upgrade.id === "pet_item_toy_jerry") {
        document.getElementById("jerry").classList.remove("lord");
        document.getElementById("jerry").classList.remove("scuba");
        document.getElementById("jerry").classList.remove("iron");
        document.getElementById("jerry").classList.remove("angel");

        document.getElementById("jerry").classList.add("glasses-3d");
        document.getElementById("jerry").classList.add("fast-animate");
      }
      if (upgrade.id === "pet_item_lord_jerry") {
        document.getElementById("jerry").classList.remove("glasses-3d");
        document.getElementById("jerry").classList.remove("fast-animate");
        document.getElementById("jerry").classList.remove("scuba");
        document.getElementById("jerry").classList.remove("iron");
        document.getElementById("jerry").classList.remove("angel");

        document.getElementById("jerry").classList.add("lord");
      }
      if (upgrade.id === "pet_item_scuba_jerry") {
        document.getElementById("jerry").classList.remove("glasses-3d");
        document.getElementById("jerry").classList.remove("fast-animate");
        document.getElementById("jerry").classList.remove("lord");
        document.getElementById("jerry").classList.remove("iron");
        document.getElementById("jerry").classList.remove("angel");

        document.getElementById("jerry").classList.add("scuba");
      }
      if (upgrade.id === "pet_item_iron_jerry") {
        document.getElementById("jerry").classList.remove("glasses-3d");
        document.getElementById("jerry").classList.remove("fast-animate");
        document.getElementById("jerry").classList.remove("lord");
        document.getElementById("jerry").classList.remove("scuba");
        document.getElementById("jerry").classList.remove("angel");

        document.getElementById("jerry").classList.add("iron");
      }
      if (upgrade.id === "pet_item_angel_jerry") {
        document.getElementById("jerry").classList.remove("glasses-3d");
        document.getElementById("jerry").classList.remove("fast-animate");
        document.getElementById("jerry").classList.remove("lord");
        document.getElementById("jerry").classList.remove("scuba");
        document.getElementById("jerry").classList.remove("iron");
        document.getElementById("jerry").classList.add("angel");
      }
      break;
  }
}

// Click Jerry
document.getElementById("jerry").addEventListener("click", (e) => {
  // Ensure the click handler runs with suppression active to avoid false positives
  runInternal(() => {
    const gain = Math.floor(gameState.perClick);
    gameState.candies += gain;

    const indicator = document.createElement("div");
    indicator.className = "click-indicator candy-icon-float";
    indicator.style.left = e.clientX - 32 + "px";
    indicator.style.top = e.clientY - 32 + "px";
    indicator.style.position = "fixed";

    document.querySelector(".click-area").appendChild(indicator);

    setTimeout(() => indicator.remove(), 1000);

    initUI();
  });
});

// Auto generation
setInterval(() => {
  // Auto gens should run suppressed (internal) so they don't trigger detection
  runInternal(() => {
    if (gameState.autoRate > 0) {
      gameState.candies += gameState.autoRate / 10;
      updateUI();
    }
  });
}, 100);

// Update UI
function updateUI() {
  document.getElementById("candies").textContent = formatNumber(
    Math.floor(gameState.candies),
  );
  document.getElementById("per-click").textContent = formatNumber(
    Math.floor(gameState.perClick),
  );
  document.getElementById("per-second").textContent = formatNumber(
    gameState.autoRate.toFixed(1),
  );
  document.getElementById("prestige-mult").textContent =
    `x${gameState.prestigeMultiplier.toFixed(1)}`;

  // 🔑 Update lock/unlock state without full re-render
  updateUpgradeLocks();
}

// Init UI
function initUI() {
  document.getElementById("candies").textContent = formatNumber(
    Math.floor(gameState.candies),
  );
  document.getElementById("per-click").textContent = formatNumber(
    Math.floor(gameState.perClick),
  );
  document.getElementById("per-second").textContent = formatNumber(
    gameState.autoRate.toFixed(1),
  );
  document.getElementById("prestige-mult").textContent =
    `x${gameState.prestigeMultiplier.toFixed(1)}`;
  renderUpgrades();
}

function formatNumber(num) {
  // Convert to a number explicitly
  num = Number(num);

  // Handle invalid inputs
  if (isNaN(num)) return "NaN";
  if (!isFinite(num)) return "Infinity";

  const suffixes = [
    "",
    "K",
    "M",
    "B",
    "T",
    "Quadrillion",
    "Quintillion",
    "Sextillion",
    "Septillion",
    "Octillion",
    "Nonillion",
    "Decillion",
    "Undecillion",
    "Duodecillion",
    "Tredecillion",
    "Quattuordecillion",
    "Quindecillion",
    "Googol",
    "Googolplex",
  ];

  let tier = 0;
  while (num >= 1000 && tier < suffixes.length - 1) {
    num /= 1000;
    tier++;
  }

  return num.toFixed(2) + (suffixes[tier] ? " " + suffixes[tier] : "");
}

// Prestige system
function prestige() {
  if (!gameState.upgrades["aspect_of_the_jerry"].count) return;

  const prestigeRequirement = 150_000_000 * gameState.prestigeMultiplier;

  if (gameState.candies < prestigeRequirement) {
    showPopup(
      `You need at least ${formatNumber(prestigeRequirement)} candies to prestige!`,
    );
    return;
  }

  const signatureCount =
    gameState.upgrades["aspect_of_the_jerry_signature"].count;
  const newMult =
    1 + (gameState.prestigeLevel + 1) * 0.1 * (signatureCount > 0 ? 2 : 1);

  showPopup(`Reset your progress for a ${newMult.toFixed(1)}x multiplier?`, {
    confirm: true,
  }).then((confirmed) => {
    if (confirmed) {
      runInternal(() => {
        gameState.prestigeLevel++;
        gameState.prestigeMultiplier = newMult;

        const keepAspect = gameState.upgrades["aspect_of_the_jerry"].count;
        const keepSignature =
          gameState.upgrades["aspect_of_the_jerry_signature"].count;

        const lord = document.getElementById("jerry");
        lord.classList.remove("glasses-3d");
        lord.classList.remove("fast-animate");

        gameState.candies = 0;
        gameState.perClick = 1;
        gameState.autoRate = 0;

        // Reset all upgrades
        upgrades.forEach((upgrade) => {
          gameState.upgrades[upgrade.id] = {
            count: 0,
            currentCost: upgrade.cost,
          };
        });

        renderUpgrades();
        updateUI();
      });
    }
  });
}

function exportSave() {
  const data = JSON.stringify(gameState);
  const key = 42;
  let obfuscated = "";
  for (let i = 0; i < data.length; i++) {
    obfuscated += String.fromCharCode(data.charCodeAt(i) ^ key);
  }
  const encoded = btoa(obfuscated);

  const blob = new Blob([encoded], { type: "text/plain" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = "JerryClicker.txt";
  a.click();
  URL.revokeObjectURL(url);

  showPopup("Save exported!");
}

function saveGame() {
  const data = JSON.stringify(gameState);
  const key = 42;
  let obfuscated = "";
  for (let i = 0; i < data.length; i++) {
    obfuscated += String.fromCharCode(data.charCodeAt(i) ^ key);
  }
  const encoded = btoa(obfuscated);

  const blob = new Blob([encoded], { type: "text/plain" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = "JerryClicker.txt";
  a.click();
  URL.revokeObjectURL(url);

  showPopup("Game saved!");
}

// Load game
function loadGame() {
  const input = document.createElement("input");
  input.type = "file";
  input.accept = ".txt";
  input.onchange = (e) => {
    const file = e.target.files[0];
    const reader = new FileReader();
    reader.onload = (event) => {
      try {
        const encoded = event.target.result;
        const obfuscated = atob(encoded);

        const key = 42;
        let data = "";
        for (let i = 0; i < obfuscated.length; i++) {
          data += String.fromCharCode(obfuscated.charCodeAt(i) ^ key);
        }

        const loaded = migrateSave(JSON.parse(data));
        applyLoadedState(loaded);
        showPopup("Game loaded!");
      } catch (err) {
        showPopup("Failed to load save file!");
      }
    };
    reader.readAsText(file);
  };
  input.click();
}

/* -------------------------
  Golden Jerry System
  Spawns randomly every 3-5 minutes at a random position on screen.
  Click it for one of three rewards:
    - Candy Windfall: big lump sum of candies
    - Frenzy: 2x production for 30 seconds
    - Lucky: bonus based on current auto rate
  Disappears after 13 seconds if not clicked.
-------------------------*/

let goldenJerryTimeout = null;
let goldenJerryDespawnTimeout = null;
let goldenJerryActive = false;
let frenzyActive = false;
let frenzyTimeout = null;

function scheduleGoldenJerry() {
  // Spawn every 3–5 minutes (180000–300000ms)
  const delay = 180000 + Math.random() * 120000;
  goldenJerryTimeout = setTimeout(() => {
    // runInternal: spawning reads no gameState but we suppress to be safe
    runInternal(spawnGoldenJerry);
  }, delay);
}

function spawnGoldenJerry() {
  // Must already be inside runInternal() when called
  if (goldenJerryActive) return;
  goldenJerryActive = true;

  const el = document.createElement("div");
  el.id = "golden-jerry";

  // Random position: keep away from edges (80px padding, avoid top header ~160px)
  const maxX = window.innerWidth - 100;
  const maxY = window.innerHeight - 100;
  const x = 80 + Math.random() * (maxX - 80);
  const y = 160 + Math.random() * (maxY - 220);

  el.style.left = x + "px";
  el.style.top = y + "px";

  document.body.appendChild(el);

  // Animate in — rAF fires outside suppression but only touches DOM, not gameState
  requestAnimationFrame(() => el.classList.add("golden-jerry-visible"));

  // Despawn after 13 seconds if not clicked — wrapped in runInternal
  goldenJerryDespawnTimeout = setTimeout(() => {
    runInternal(despawnGoldenJerry);
  }, 13000);

  // Click handler — wrap entire body in runInternal so every gameState write is suppressed
  el.addEventListener("click", (e) => {
    e.stopPropagation();
    runInternal(() => claimGoldenJerry(x, y));
  });
}

function despawnGoldenJerry() {
  // Must already be inside runInternal() when called
  const el = document.getElementById("golden-jerry");
  if (!el) return;
  el.classList.remove("golden-jerry-visible");
  el.classList.add("golden-jerry-fade");
  // DOM-only cleanup; no gameState touched here
  setTimeout(() => el.remove(), 400);
  goldenJerryActive = false;
  scheduleGoldenJerry();
}

function claimGoldenJerry(x, y) {
  // Must already be inside runInternal() when called (guaranteed by click handler above)
  clearTimeout(goldenJerryDespawnTimeout);

  const el = document.getElementById("golden-jerry");
  if (!el) return;

  // Pick a reward
  const roll = Math.random();
  let rewardText = "";
  let rewardSub = "";

  if (roll < 0.40) {
    // Candy Windfall: 15 minutes of auto-gen, min 30 * perClick
    const windfall = Math.max(
      Math.floor(gameState.perClick * 30),
      Math.floor(gameState.autoRate * 60 * 15)
    );
    gameState.candies += windfall;           // suppressed ✓
    rewardText = "Candy Windfall!";
    rewardSub = "+" + formatNumber(windfall) + " candies";
    updateUI();
  } else if (roll < 0.75) {
    // Frenzy: 2x all production for 30 seconds
    if (!frenzyActive) {
      frenzyActive = true;
      gameState.perClick *= 2;               // suppressed ✓
      gameState.autoRate *= 2;               // suppressed ✓
      document.body.classList.add("frenzy-active");
      showFrenzyTimer(30);
      // Undo frenzy after 30s — wrapped in its own runInternal
      frenzyTimeout = setTimeout(() => {
        runInternal(() => {
          gameState.perClick /= 2;           // suppressed ✓
          gameState.autoRate /= 2;           // suppressed ✓
          frenzyActive = false;
          document.body.classList.remove("frenzy-active");
          const timerEl = document.getElementById("frenzy-timer");
          if (timerEl) timerEl.remove();
          updateUI();
        });
      }, 30000);
    } else {
      // Already in frenzy — give windfall instead
      const windfall = Math.max(
        Math.floor(gameState.perClick * 20),
        Math.floor(gameState.autoRate * 60 * 8)
      );
      gameState.candies += windfall;         // suppressed ✓ (still inside outer runInternal)
      updateUI();
    }
    rewardText = "Frenzy!";
    rewardSub = "2x production for 30s";
  } else {
    // Lucky: 10% of current candies, min 30 * perClick
    const lucky = Math.max(
      Math.floor(gameState.perClick * 30),
      Math.floor(gameState.candies * 0.10)
    );
    gameState.candies += lucky;              // suppressed ✓
    rewardText = "Lucky!";
    rewardSub = "+" + formatNumber(lucky) + " candies";
    updateUI();
  }

  // Burst animation — pure DOM, no gameState
  showGoldenBurst(x, y, rewardText, rewardSub);

  // Remove element — pure DOM, setTimeout body touches no gameState
  el.classList.add("golden-jerry-burst");
  setTimeout(() => el.remove(), 300);

  goldenJerryActive = false;
  scheduleGoldenJerry();
}

function showFrenzyTimer(seconds) {
  // Called from within runInternal() — DOM only, no gameState writes
  let existing = document.getElementById("frenzy-timer");
  if (existing) existing.remove();

  const el = document.createElement("div");
  el.id = "frenzy-timer";
  el.innerHTML = `<span id="frenzy-timer-label">FRENZY</span><span id="frenzy-timer-secs">${seconds}</span>`;
  document.body.appendChild(el);

  let remaining = seconds;
  // setInterval fires outside suppression — wrap body in runInternal
  // (only touches DOM text content, but wrapping is cheap insurance)
  const tick = setInterval(() => {
    runInternal(() => {
      remaining--;
      const secEl = document.getElementById("frenzy-timer-secs");
      if (secEl) secEl.textContent = remaining + "s";
      if (remaining <= 0) {
        clearInterval(tick);
        const timerEl = document.getElementById("frenzy-timer");
        if (timerEl) timerEl.remove();
      }
    });
  }, 1000);
}

function showGoldenBurst(x, y, title, sub) {
  // Pure DOM — no gameState involved
  const el = document.createElement("div");
  el.className = "golden-burst";
  el.innerHTML = `<div class="golden-burst-title">${title}</div><div class="golden-burst-sub">${sub}</div>`;
  el.style.left = x + "px";
  el.style.top = (y - 20) + "px";
  document.body.appendChild(el);
  setTimeout(() => el.remove(), 2000);
}

// Kick off the first Golden Jerry — scheduleGoldenJerry itself touches no gameState,
// but wrap for consistency and future-proofing
runInternal(scheduleGoldenJerry);

/* -------------------------
  Finalize: apply any pending wraps, then safe-init load/save and UI
-------------------------*/

// Attempt to wrap any functions that were declared after the proxy setup
applyPendingWraps();

// Safely run loadFromLocalStorage() and initUI() inside suppression so they don't trigger anti-tamper
runInternal(() => {
  try {
    if (typeof loadFromLocalStorage === "function") loadFromLocalStorage();
  } catch (e) {
    console.error("loadFromLocalStorage error:", e);
  }

  try {
    if (typeof initUI === "function") initUI();
  } catch (e) {
    console.error("initUI error:", e);
  }
});

// Re-run pending wraps once more (in case some functions were declared during initialization)
applyPendingWraps();