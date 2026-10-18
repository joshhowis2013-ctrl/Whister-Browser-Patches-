// 1. DYNAMIC CLOCK & TIME-OF-DAY GREETING ENGINE
function updateTimeAndGreeting() {
    const now = new Date();
    const hours = now.getHours();
    
    let greetingText = "Good evening";
    if (hours < 12) greetingText = "Good morning";
    else if (hours < 18) greetingText = "Good afternoon";
    
    document.getElementById('greeting').textContent = greetingText;

    document.getElementById('clock').textContent = now.toLocaleTimeString([], {
        hour: '2-digit', 
        minute: '2-digit', 
        second: '2-digit'
    });
}
setInterval(updateTimeAndGreeting, 1000);
updateTimeAndGreeting();

// 2. INTERACTIVE SEARCH & URL SMART ROUTER MATRIX
const searchInput = document.getElementById('search-input');
const engineSelector = document.getElementById('engine-selector');
const searchEnginePreferenceKey = 'whister-search-engine';

const placeholders = {
    ddg: "Search securely with DuckDuckGo...",
    brave: "Search tracker-free with Brave...",
    google: "Search upstream via Google..."
};

const searchTargets = {
    ddg: 'https://duckduckgo.com',
    brave: 'https://search.brave.com/search',
    google: 'https://www.google.com/search'
};

try {
    const savedEngine = localStorage.getItem(searchEnginePreferenceKey);
    if (Object.prototype.hasOwnProperty.call(searchTargets, savedEngine)) {
        engineSelector.value = savedEngine;
    }
} catch (error) {
    // Keep the chooser usable when browser storage is unavailable.
}

function updateSearchPlaceholder() {
    searchInput.placeholder = placeholders[engineSelector.value] || 'Choose an engine to search...';
}

updateSearchPlaceholder();

engineSelector.addEventListener('change', function() {
    updateSearchPlaceholder();
    try {
        localStorage.setItem(searchEnginePreferenceKey, engineSelector.value);
    } catch (error) {
        // The selection still applies for this tab if storage is unavailable.
    }
    searchInput.focus();
});

searchInput.addEventListener('keydown', function(e) {
    if (e.key === 'Enter') {
        const userText = searchInput.value.trim();
        if (userText === '') return;

        // Expanded Domain Check: Automatically catches text ending in popular tlds, or standalone domains like youtube
        const commonTlds = /\.(com|org|net|edu|gov|uk|io|me|tv|co|info|ca|au)$/i;
        const knownShortcuts = /^(youtube|google|github|wikipedia|facebook|twitter|x|amazon|netflix|apple)$/i;
        const urlPattern = /^(https?:\/\/)?([\da-z\.-]+)\.([a-z\.]{2,6})([\/\w \.-]*)*\/?$/i;
        const isLocalhost = /^localhost(:\d+)?(\/.*)?$/i.test(userText);

        if (urlPattern.test(userText) || commonTlds.test(userText) || knownShortcuts.test(userText) || isLocalhost) {
            // It's a website url or direct destination! Route immediately
            let targetUrl = userText;
            
            // If it's a known shortcut word without a TLD (like just typing "youtube"), append .com automatically
            if (knownShortcuts.test(userText) && !userText.includes('.')) {
                targetUrl = userText + '.com';
            }
            
            if (!/^https?:\/\//i.test(targetUrl)) {
                targetUrl = 'https://' + targetUrl;
            }
            chrome.send('navigateToUrl', [targetUrl]);
        } else {
            const searchTarget = searchTargets[engineSelector.value];
            if (!searchTarget) {
                engineSelector.focus();
                searchInput.placeholder = 'Choose a search engine first...';
                return;
            }

            const urlObj = new URL(searchTarget);
            urlObj.searchParams.set('q', userText);
            
            chrome.send('navigateToUrl', [urlObj.toString()]);
        }

    }
});


// External weather requests are blocked from chrome:// WebUI pages by design.
document.getElementById('weather-temp').textContent = '--°C';
document.getElementById('weather-cond').textContent = 'Weather unavailable in this build';

window.whisterSettings = {
    onSettings(values) {
        const theme = Number(values['browser.theme.color_scheme2']);
        const prefersLight = window.matchMedia('(prefers-color-scheme: light)').matches;
        document.documentElement.dataset.theme = theme === 1 ? 'light' : theme === 2 ? 'dark' : prefersLight ? 'light' : 'dark';
    }
};
if (window.chrome && typeof window.chrome.send === 'function') {
    window.chrome.send('getSettings');
}

const customizationStorageKey = 'whister-newtab-customization';
const wallpapers = ['dusk', 'aurora', 'ocean', 'grove', 'ember', 'rose', 'stone'];
const accents = ['violet', 'cyan', 'mint', 'coral'];
const visibilityDefaults = { greeting: true, clock: true, weather: true };
const widgetIds = { greeting: 'greeting', clock: 'clock', weather: 'weather-card' };
const customizeToggle = document.getElementById('customize-toggle');
const customizePanel = document.getElementById('customize-panel');
const customizeClose = document.getElementById('customize-close');
const backgroundFile = document.getElementById('custom-background-file');
const removeBackground = document.getElementById('remove-custom-background');
const backgroundStatus = document.getElementById('background-status');

function saveCustomization(value) {
    try {
        localStorage.setItem(customizationStorageKey, JSON.stringify(value));
        return true;
    } catch (error) {
        backgroundStatus.textContent = 'This browser profile could not save the customization';
        return false;
    }
}

function applyWallpaper(value) {
    const wallpaper = wallpapers.includes(value) ? value : 'dusk';
    document.documentElement.dataset.wallpaper = wallpaper;
    document.querySelectorAll('[data-wallpaper-choice]').forEach(function(button) {
        button.setAttribute('aria-pressed', String(button.dataset.wallpaperChoice === wallpaper));
    });
    return wallpaper;
}

function applyCustomBackground(photo) {
    const validPhoto = typeof photo === 'string' &&
        photo.length <= 1800000 &&
        /^data:image\/jpeg;base64,[A-Za-z0-9+/]+=*$/.test(photo);
    if (validPhoto) {
        document.documentElement.style.setProperty('--custom-wallpaper', 'url("' + photo + '")');
        document.documentElement.dataset.customBackground = 'true';
        document.querySelectorAll('[data-wallpaper-choice]').forEach(function(button) {
            button.setAttribute('aria-pressed', 'false');
        });
        removeBackground.hidden = false;
        backgroundStatus.textContent = 'Photo saved only in this browser profile';
        return photo;
    }
    document.documentElement.style.removeProperty('--custom-wallpaper');
    delete document.documentElement.dataset.customBackground;
    removeBackground.hidden = true;
    backgroundStatus.textContent = 'Saved only in this browser profile';
    return '';
}

function applyAccent(value) {
    const accent = accents.includes(value) ? value : 'violet';
    document.documentElement.dataset.accent = accent;
    document.querySelectorAll('[data-accent-choice]').forEach(function(button) {
        button.setAttribute('aria-pressed', String(button.dataset.accentChoice === accent));
    });
    return accent;
}

function openCustomization() {
    customizePanel.hidden = false;
    customizeToggle.setAttribute('aria-expanded', 'true');
    customizeClose.focus();
}

function closeCustomization(returnFocus) {
    customizePanel.hidden = true;
    customizeToggle.setAttribute('aria-expanded', 'false');
    if (returnFocus) customizeToggle.focus();
}

function initCustomization() {
    let saved = {};
    try {
        saved = JSON.parse(localStorage.getItem(customizationStorageKey) || '{}');
    } catch (error) {
        saved = {};
    }
    if (!saved || typeof saved !== 'object') saved = {};

    const state = {
        wallpaper: applyWallpaper(saved.wallpaper),
        accent: applyAccent(saved.accent),
        visibility: Object.assign({}, visibilityDefaults, saved.visibility || {}),
        photo: applyCustomBackground(saved.photo)
    };

    document.querySelectorAll('[data-visibility]').forEach(function(input) {
        const key = input.dataset.visibility;
        if (!Object.prototype.hasOwnProperty.call(visibilityDefaults, key)) return;
        input.checked = state.visibility[key] !== false;
        const widget = document.getElementById(widgetIds[key]);
        if (widget) widget.hidden = !input.checked;
        input.addEventListener('change', function() {
            state.visibility[key] = input.checked;
            if (widget) widget.hidden = !input.checked;
            saveCustomization(state);
        });
    });

    document.querySelectorAll('[data-wallpaper-choice]').forEach(function(button) {
        button.addEventListener('click', function() {
            state.wallpaper = applyWallpaper(button.dataset.wallpaperChoice);
            state.photo = applyCustomBackground('');
            applyWallpaper(state.wallpaper);
            saveCustomization(state);
        });
    });

    backgroundFile.addEventListener('change', function() {
        const file = backgroundFile.files && backgroundFile.files[0];
        if (!file) return;
        if (!['image/jpeg', 'image/png', 'image/webp'].includes(file.type) || file.size > 12 * 1024 * 1024) {
            backgroundStatus.textContent = 'Choose a JPG, PNG, or WebP image under 12 MB';
            backgroundFile.value = '';
            return;
        }

        const imageUrl = URL.createObjectURL(file);
        const image = new Image();
        image.onload = function() {
            URL.revokeObjectURL(imageUrl);
            if (image.width * image.height > 40000000) {
                backgroundStatus.textContent = 'That image is too large to use here';
                backgroundFile.value = '';
                return;
            }
            const scale = Math.min(1, 1280 / image.width, 800 / image.height);
            const canvas = document.createElement('canvas');
            canvas.width = Math.max(1, Math.round(image.width * scale));
            canvas.height = Math.max(1, Math.round(image.height * scale));
            canvas.getContext('2d').drawImage(image, 0, 0, canvas.width, canvas.height);
            let photo = canvas.toDataURL('image/jpeg', 0.72);
            if (photo.length > 1800000) photo = canvas.toDataURL('image/jpeg', 0.52);
            if (photo.length > 1800000) {
                backgroundStatus.textContent = 'Could not compress that image enough to save';
                backgroundFile.value = '';
                return;
            }
            state.photo = applyCustomBackground(photo);
            saveCustomization(state);
            backgroundFile.value = '';
        };
        image.onerror = function() {
            URL.revokeObjectURL(imageUrl);
            backgroundStatus.textContent = 'That image could not be opened';
            backgroundFile.value = '';
        };
        image.src = imageUrl;
    });

    removeBackground.addEventListener('click', function() {
        state.photo = applyCustomBackground('');
        applyWallpaper(state.wallpaper);
        saveCustomization(state);
        backgroundFile.value = '';
    });

    document.querySelectorAll('[data-accent-choice]').forEach(function(button) {
        button.addEventListener('click', function() {
            state.accent = applyAccent(button.dataset.accentChoice);
            saveCustomization(state);
        });
    });

    customizeToggle.addEventListener('click', function() {
        if (customizePanel.hidden) openCustomization();
        else closeCustomization(false);
    });
    customizeClose.addEventListener('click', function() { closeCustomization(true); });
    document.addEventListener('keydown', function(event) {
        if (event.key === 'Escape' && !customizePanel.hidden) closeCustomization(true);
    });
    document.addEventListener('pointerdown', function(event) {
        if (!customizePanel.hidden && !customizePanel.contains(event.target) &&
            !customizeToggle.contains(event.target)) {
            closeCustomization(false);
        }
    });
}

initCustomization();
