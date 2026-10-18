



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
  onUblockInstallRequested(requestStarted) {
    if (requestStarted) {
      window.location.href = 'chrome://newtab';
      return;
    }
    sendMessage('navigateToUrl', [
      'https://chromewebstore.google.com/detail/ublock-origin-lite/ddkjiahejlhfcafbddmgiahcphecmpfh',
    ]);
  },

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
  onPatchCheckComplete(status, patchFileName) {
    const button = document.getElementById('check-updates-btn');
    const statusText = document.getElementById('update-status-text');
    if (!button || !statusText) return;

    // Remove the loading animation loop
    button.classList.remove('loading');

    if (status === 'patch_found') {
      button.innerText = "Apply Patch";
      statusText.innerHTML = `New workspace update found: <code>${patchFileName}</code>`;
      
      // Override the button click to handle applying the local text diff patch
      button.onclick = () => {
        button.classList.add('loading');
        statusText.innerText = "Applying unified source code transformations...";
        sendMessage('applyLocalPatch', [patchFileName]);
      };
    } else if (status === 'no_patches') {
      button.innerText = "Check version";
      statusText.innerText = "Whister is completely up to date.";
    } else if (status === 'success') {
      button.innerText = "Rebuild Ready";
      button.disabled = true;
      statusText.innerText = "Patch successfully applied! Run siso/autoninja to compile your build.";
    } else if (status === 'error') {
      button.innerText = "Retry Check";
      statusText.innerText = "Failed to access local patch target files.";
    }
  },

  onWhisterUpdateResult(status, details) {
    const button = document.getElementById('check-updates-btn');
    const statusText = document.getElementById('update-status-text');
    if (!button || !statusText) return;

    // Turn off the spinning loader ring animation instantly
    button.classList.remove('loading');

    switch (status) {
      case 'noupdate':
        button.innerText = "Check version";
        statusText.innerText = "Whister is completely up to date.";
        break;

      case 'patch_available':
        button.innerText = "Apply Patch";
        statusText.innerHTML = `Smart Update Ready: Source patch <code>${details}</code> discovered.`;
        
        // Arm the button to trigger the smart download phase on next click
        button.onclick = () => {
          button.classList.add('loading');
          statusText.innerText = "Downloading unified source diff files...";
          sendMessage('executeSmartWhisterDownload', [details]);
        };
        break;

      case 'download_failed':
        button.innerText = "Retry Check";
        statusText.innerText = "Error: File download interrupted or stream corrupted.";
        break;

      case 'network_error':
        button.innerText = "Retry Check";
        statusText.innerText = "Error: Could not establish a connection to ://whisterbrowser.com.";
        break;

      case 'success':
        button.innerText = "Almost Done";
        button.disabled = true;
        statusText.innerText = "Restart whister to complete the update.";
        break;

      default:
        button.innerText = "Check version";
        statusText.innerText = "System error encountered during update validation pass.";
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
}

if (steps.length) {
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
    const consent = document.getElementById('install-ublock-lite');
    if (consent && consent.checked) {
      const sent = sendMessage('installUblockLite', [true]);
      if (!sent) {
        window.location.href = 'https://chromewebstore.google.com/detail/ublock-origin-lite/ddkjiahejlhfcafbddmgiahcphecmpfh';
      }
    } else {
      window.location.href = 'chrome://newtab';
    }
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
