



const filter = document.getElementById('filter');
const empty = document.getElementById('empty');
const items = Array.from(document.querySelectorAll('.card, .row'));
const sections = Array.from(document.querySelectorAll('section'));
const controls = Array.from(document.querySelectorAll('[data-key]'));
function applyTheme(value) {
  const theme = Number(value);
  const prefersLight = window.matchMedia('(prefers-color-scheme: light)').matches;
  document.documentElement.dataset.theme = theme === 1 ? 'light' : theme === 2 ? 'dark' : prefersLight ? 'light' : 'dark';
}

function sendMessage(name, args) {
  if (window.chrome && typeof window.chrome.send === 'function') {
    window.chrome.send(name, args || []);
    return true;
  }
  return false;
}

window.whisterSettings = {
  onAbout(info) {
    for (const el of document.querySelectorAll('[data-about]')) {
      el.textContent = info[el.dataset.about] ?? '';
    }
  },

  onSearchEngines(engines) {
    for (const select of document.querySelectorAll('select.engine')) {
      select.replaceChildren();
      for (const engine of engines) {
        const option = document.createElement('option');
        option.value = engine.id;
        option.textContent = engine.name;
        option.selected = engine.selected;
        select.appendChild(option);
      }
    }
  },

  onSettings(values) {
    for (const el of controls) {
      const value = values[el.dataset.key];
      if (value === undefined) {
        continue;
      }
      if (el.type === 'checkbox') {
        el.checked = value;
      } else {
        el.value = value;
      }
    }
    applyTheme(values['browser.theme.color_scheme2']);
  },
  onUpdateCheckResult(status, version, installerUrl, sha256, size) {
    const button = document.getElementById('check-updates-btn');
    const statusText = document.getElementById('update-status-text');
    const downloadRow = document.getElementById('update-download-row');
    const downloadButton = document.getElementById('download-verify-btn');
    const installRow = document.getElementById('update-install-row');
    const latestLink = document.getElementById('latest-build-link');
    if (!button || !statusText || !downloadRow || !downloadButton ||
        !installRow || !latestLink) return;

    button.classList.remove('loading');
    button.disabled = false;
    switch (status) {
      case 'up_to_date':
        statusText.textContent = `Whister is up to date (version ${version}).`;
        latestLink.href = 'https://beta-updates.whisterbrowser.com/';
        latestLink.textContent = 'Get latest build';
        downloadRow.hidden = true;
        installRow.hidden = true;
        break;
      case 'update_available':
        statusText.textContent = `Whister ${version} is available. Download and verify it before installing.`;
        latestLink.href = installerUrl;
        latestLink.textContent = `Download ${version}`;
        downloadRow.hidden = false;
        installRow.hidden = true;
        downloadButton.disabled = false;
        downloadButton.dataset.expectedHash = sha256;
        downloadButton.dataset.expectedSize = String(size);
        break;
      case 'error':
      default:
        statusText.textContent = 'Could not validate the beta update manifest. Try again later.';
        latestLink.href = 'https://beta-updates.whisterbrowser.com/';
        latestLink.textContent = 'Get latest build';
        downloadRow.hidden = true;
        installRow.hidden = true;
        break;
    }
  },

  onUpdateDownloadResult(status, version, hash) {
    const statusText = document.getElementById('update-status-text');
    const downloadButton = document.getElementById('download-verify-btn');
    const checkButton = document.getElementById('check-updates-btn');
    const downloadRow = document.getElementById('update-download-row');
    const installRow = document.getElementById('update-install-row');
    const hashText = document.getElementById('update-sha256');
    if (!statusText || !downloadButton || !checkButton || !downloadRow || !installRow ||
        !hashText) return;

    downloadButton.disabled = status === 'downloading';
    checkButton.disabled = status === 'downloading';
    switch (status) {
      case 'downloading':
        statusText.textContent = 'Downloading the installer for SHA-256 verification…';
        break;
      case 'verified':
        statusText.textContent = `Whister ${version} downloaded and SHA-256 verified.`;
        hashText.textContent = hash;
        downloadRow.hidden = true;
        installRow.hidden = false;
        break;
      case 'checksum_mismatch':
        statusText.textContent = `SHA-256 verification failed. The downloaded file was deleted. Expected: ${hash}`;
        installRow.hidden = true;
        break;
      case 'installer_started':
        statusText.textContent = `The verified ${version} installer was opened. Follow its prompts to update.`;
        installRow.hidden = true;
        break;
      case 'error':
      default:
        statusText.textContent = 'The installer could not be downloaded or verified. No update was installed.';
        downloadButton.disabled = false;
        break;
    }
  }
};

for (const el of controls) {
  el.addEventListener('change', () => {
    let value;
    if (el.type === 'checkbox') {
      value = el.checked;
    } else if (el.dataset.type === 'int') {
      value = Number(el.value);
    } else {
      value = el.value.trim();
    }
    sendMessage('setSetting', [el.dataset.key, value]);
  });
}

sendMessage('getSettings');
sendMessage('getAboutInfo');
sendMessage('getSearchEngines');

for (const select of document.querySelectorAll('select.engine')) {
  select.addEventListener('change', () => {
    sendMessage('setSearchEngine', [Number(select.value)]);
  });
}

for (const link of document.querySelectorAll('a[data-webui-navigate]')) {
  link.addEventListener('click', (event) => {
    event.preventDefault();
    sendMessage('navigateToUrl', [link.href]);
  });
}

const updateButton = document.getElementById('check-updates-btn');
if (updateButton) {
  updateButton.addEventListener('click', () => {
    updateButton.classList.add('loading');
    updateButton.disabled = true;
    document.getElementById('update-download-row').hidden = true;
    document.getElementById('update-install-row').hidden = true;
    sendMessage('checkWhisterUpdates');
  });
}

const verifyUpdateButton = document.getElementById('download-verify-btn');
if (verifyUpdateButton) {
  verifyUpdateButton.addEventListener('click', () => {
    verifyUpdateButton.disabled = true;
    document.getElementById('check-updates-btn').disabled = true;
    sendMessage('downloadAndVerifyWhisterUpdate');
  });
}

const installUpdateButton = document.getElementById('install-verified-btn');
if (installUpdateButton) {
  installUpdateButton.addEventListener('click', () => {
    sendMessage('installVerifiedWhisterUpdate');
  });
}

const steps = Array.from(document.querySelectorAll('.step'));
let currentStep = 0;

function showStep(index) {
  currentStep = Math.max(0, Math.min(steps.length - 1, index));
  steps.forEach((step, stepIndex) => {
    step.hidden = stepIndex !== currentStep;
  });
  document.querySelectorAll('.dots span').forEach((dot, dotIndex) => {
    dot.classList.toggle('on', dotIndex <= currentStep);
    dot.classList.toggle('active', dotIndex === currentStep);
  });
  const heading = steps[currentStep].querySelector('h1');
  if (heading) heading.focus({ preventScroll: true });
  const progress = document.querySelector('.dots');
  if (progress) {
    progress.setAttribute('aria-valuenow', String(currentStep + 1));
    progress.setAttribute('aria-valuetext', `Step ${currentStep + 1} of ${steps.length}`);
  }
}

if (steps.length) {
  const progress = document.querySelector('.dots');
  if (progress) {
    progress.setAttribute('role', 'progressbar');
    progress.setAttribute('aria-valuemin', '1');
    progress.setAttribute('aria-valuemax', String(steps.length));
  }
  for (const button of document.querySelectorAll('[data-next]')) {
    button.addEventListener('click', () => showStep(currentStep + 1));
  }
  for (const button of document.querySelectorAll('[data-back]')) {
    button.addEventListener('click', () => showStep(currentStep - 1));
  }
  showStep(0);
}

const finishButton = document.getElementById('finish');
if (finishButton) {
  finishButton.addEventListener('click', () => {
    window.location.href = 'whister://newtab';
  });
}

const navLinks = Array.from(document.querySelectorAll('.side a'));
function route() {
  if (!sections.length || !navLinks.length) {
    return;
  }
  const wanted = location.hash.slice(1);
  const name = sections.some((section) => section.id === wanted) ? wanted : sections[0].id;
  for (const section of sections) {
    section.hidden = section.id !== name;
  }
  for (const link of navLinks) {
    link.classList.toggle('active', link.getAttribute('href') === '#' + name);
  }
  empty.hidden = true;
}
window.addEventListener('hashchange', route);
route();

if (filter) {
  filter.addEventListener('input', () => {
    const query = filter.value.trim().toLowerCase();
    if (query === '') {
      for (const item of items) item.hidden = false;
      route();
      return;
    }
    for (const item of items) {
      item.hidden = !item.textContent.toLowerCase().includes(query);
    }
    let anyVisible = false;
    for (const section of sections) {
      const visible = section.querySelector('.card:not([hidden]), .row:not([hidden])') !== null;
      section.hidden = !visible;
      anyVisible = anyVisible || visible;
    }
    empty.hidden = anyVisible;
  });
}
