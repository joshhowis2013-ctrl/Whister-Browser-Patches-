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

const placeholders = {
    ddg: "Search securely with DuckDuckGo...",
    brave: "Search tracker-free with Brave...",
    google: "Search upstream via Google..."
};

engineSelector.addEventListener('change', function() {
    searchInput.placeholder = placeholders[engineSelector.value];
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
            window.location.href = targetUrl;
                } else {
            // It's a true search term! Use native browser object API mapping to build the URL safely
            let searchTarget = "https://duckduckgo.com";
            
            // Setting the correct search paths so engines parse query variables
            if (engineSelector.value === 'brave') {
                searchTarget = "https://brave.com";
            } else if (engineSelector.value === 'google') {
                searchTarget = "https://google.com";
            }

            const urlObj = new URL(searchTarget);
            urlObj.searchParams.set('q', userText);
            
            window.location.href = urlObj.toString();
        }

    }
});


// 3. LIVE FREE WEATHER TELEMETRY POLL
async function fetchLiveWeather() {
    try {
        let lat = 52.56; 
        let lon = -1.82;

        if (navigator.geolocation) {
            navigator.geolocation.getCurrentPosition(async function(position) {
                lat = position.coords.latitude;
                lon = position.coords.longitude;
                await pullWeatherData(lat, lon);
            }, async function() {
                await pullWeatherData(lat, lon); 
            });
        } else {
            await pullWeatherData(lat, lon);
        }
    } catch (err) {
        document.getElementById('weather-cond').textContent = "Offline";
    }
}

async function pullWeatherData(lat, lon) {
    const url = 'https://open-meteo.com' + lat + '&longitude=' + lon + '&current_weather=true';
    const response = await fetch(url);
    const data = await response.json();
    
    if (data && data.current_weather) {
        const temp = Math.round(data.current_weather.temperature);
        const code = data.current_weather.weathercode;
        
        let conditionText = "Clear Sky";
        if (code > 0 && code <= 3) conditionText = "Partly Cloudy";
        else if (code >= 45 && code <= 48) conditionText = "Foggy Conditions";
        else if (code >= 51 && code <= 67) conditionText = "Rain Showers";
        else if (code >= 71 && code <= 77) conditionText = "Snowfall";
        else if (code >= 95) conditionText = "Thunderstorms";

        document.getElementById('weather-temp').textContent = temp + '°C';
        document.getElementById('weather-cond').textContent = conditionText;
    }
}
fetchLiveWeather();
setInterval(fetchLiveWeather, 900000);
