// Firebase Configuration
const firebaseConfig = {
  apiKey: "AIzaSyD9yBVo4_AkmdyKwnceY51-PPMTHwJclEI",
  authDomain: "memory-box-c2ef8.firebaseapp.com",
  databaseURL: "https://memory-box-c2ef8-default-rtdb.europe-west1.firebasedatabase.app/",
  projectId: "memory-box-c2ef8",
  storageBucket: "memory-box-c2ef8.firebasestorage.app",
  messagingSenderId: "72070979083",
  appId: "1:72070979083:web:5a73cc7d8799ce5d98f10e",
  measurementId: "G-PS02ZZL0R9"
};

// Initialize Firebase (Using Compat syntax for file:// protocol support)
firebase.initializeApp(firebaseConfig);
const database = firebase.database();
const deviceRef = database.ref('device');

// DOM Elements
const connectionStatus = document.getElementById('connection-status');
const prismContainer = document.getElementById('prism');
const prevBtn = document.getElementById('prev-btn');
const nextBtn = document.getElementById('next-btn');
const indexDisplay = document.getElementById('index-display');
const speedSlider = document.getElementById('speed-slider');
const speedValue = document.getElementById('speed-value');
const ledToggle = document.getElementById('led-toggle');
const powerBtn = document.getElementById('power-btn');
const settingsBtn = document.getElementById('settings-btn');
const slideToggle = document.getElementById('slide-toggle');
const slideTime = document.getElementById('slide-time');
const slideTimeGroup = document.getElementById('slide-time-group');
const settingsModal = document.getElementById('settings-modal');
const settingsCloseBtn = document.getElementById('settings-close-btn');
const calConfirmBtn = document.getElementById('cal-confirm-btn');
const splashScreen = document.getElementById('splash-screen');

// State
let currentPhotoIndex = 1;
const TOTAL_FACES = 8;
let isUpdatingFromFirebase = false;

// Splash Screen Logic
let hasInitialData = false;
let isConnected = false;
let splashHidden = false;

function checkAndHideSplash() {
    if (!splashHidden && isConnected && hasInitialData) {
        splashHidden = true;
        setTimeout(() => {
            splashScreen.classList.add('hidden');
        }, 800); // Minimum wait for aesthetic feel
    }
}

// Fallback: always hide splash after 5 seconds just in case of network issues
setTimeout(() => {
    if (!splashHidden) {
        splashHidden = true;
        splashScreen.classList.add('hidden');
    }
}, 5000);

// Initialize Prism Images
function initPrism() {
    for (let i = 1; i <= TOTAL_FACES; i++) {
        const item = document.createElement('div');
        item.classList.add('prism-item');
        
        if (i === TOTAL_FACES) {
            item.classList.add('black-screen');
        } else {
            const img = document.createElement('img');
            img.src = `./src/${i}.jpg`;
            img.alt = `Photo ${i}`;
            item.appendChild(img);
        }
        
        prismContainer.appendChild(item);
    }
    updatePrismView();
}

function updatePrismView() {
    const items = prismContainer.children;
    
    for (let i = 0; i < items.length; i++) {
        const item = items[i];
        const index = i + 1;
        
        item.classList.remove('active', 'prev', 'next', 'hidden');
        
        if (index === currentPhotoIndex) {
            item.classList.add('active');
        } else if (index === currentPhotoIndex - 1 || (currentPhotoIndex === 1 && index === TOTAL_FACES)) {
            item.classList.add('prev');
        } else if (index === currentPhotoIndex + 1 || (currentPhotoIndex === TOTAL_FACES && index === 1)) {
            item.classList.add('next');
        } else {
            item.classList.add('hidden');
        }
    }
    
    indexDisplay.textContent = `${currentPhotoIndex} / ${TOTAL_FACES}`;
    updatePowerBtnState();
}

function updatePowerBtnState() {
    // Device is considered OFF only if on black screen (8) AND led is off
    const isOff = (currentPhotoIndex === 8 && !ledToggle.checked);
    if (isOff) {
        powerBtn.className = 'power-icon-btn off';
    } else {
        powerBtn.className = 'power-icon-btn on';
    }
}

// Connection Status Logic
const connectedRef = firebase.database().ref(".info/connected");
connectedRef.on("value", (snap) => {
    if (snap.val() === true) {
        isConnected = true;
        checkAndHideSplash();
        
        connectionStatus.classList.add('connected');
        connectionStatus.querySelector('.text').textContent = 'Bağlı';
    } else {
        isConnected = false;
        connectionStatus.classList.remove('connected');
        connectionStatus.querySelector('.text').textContent = 'Bağlantı Kesildi...';
    }
});

function applyFirebaseData(data) {
    if (data.photoIndex !== undefined) {
        const pIndex = parseInt(data.photoIndex, 10);
        if (!isNaN(pIndex) && pIndex >= 1 && pIndex <= TOTAL_FACES) {
            currentPhotoIndex = pIndex;
            updatePrismView();
        }
    }
    
    if (data.speed !== undefined) {
        const spd = parseInt(data.speed, 10);
        if (!isNaN(spd)) {
            speedSlider.value = spd;
            speedValue.textContent = spd;
        }
    }
    
    if (data.led !== undefined) {
        ledToggle.checked = data.led;
    }
    
    if (data.slideMode !== undefined) {
        slideToggle.checked = data.slideMode;
    }
    
    if (data.slideInterval !== undefined) {
        slideTime.value = data.slideInterval;
    }
    
    updatePowerBtnState();
    updateSlideTimeVisibility();
}

// Firebase to UI Sync
deviceRef.on('value', (snapshot) => {
    const data = snapshot.val();
    
    if (data) {
        isUpdatingFromFirebase = true;
        applyFirebaseData(data);
        isUpdatingFromFirebase = false;
        
        if (!hasInitialData) {
            hasInitialData = true;
            checkAndHideSplash();
        }
    }
}, (error) => {
    console.error("Firebase Error: ", error);
});

// Periodic Fallback Sync (1 minute)
setInterval(() => {
    if (!isUpdatingFromFirebase) {
        deviceRef.get().then((snapshot) => {
            if (snapshot.exists()) {
                isUpdatingFromFirebase = true;
                applyFirebaseData(snapshot.val());
                isUpdatingFromFirebase = false;
            }
        });
    }
}, 60000);

// UI to Firebase Sync Functions
function updateFirebaseDevice(updates) {
    if (!isUpdatingFromFirebase) {
        deviceRef.update(updates).catch(err => console.error("Update failed:", err));
    }
}

// Event Listeners
prevBtn.addEventListener('click', (e) => {
    e.preventDefault();
    let newIndex = currentPhotoIndex - 1;
    if (newIndex < 1) newIndex = TOTAL_FACES;
    
    updateFirebaseDevice({ photoIndex: newIndex });
    currentPhotoIndex = newIndex;
    updatePrismView();
});

nextBtn.addEventListener('click', (e) => {
    e.preventDefault();
    let newIndex = currentPhotoIndex + 1;
    if (newIndex > TOTAL_FACES) newIndex = 1;
    
    updateFirebaseDevice({ photoIndex: newIndex });
    currentPhotoIndex = newIndex;
    updatePrismView();
});

let speedDebounceTimer;
speedSlider.addEventListener('input', (e) => {
    const val = parseInt(e.target.value);
    speedValue.textContent = val; // Ekranda sayıyı anında güncelle
    
    // Firebase'e göndermeyi geciktir (Debounce)
    clearTimeout(speedDebounceTimer);
    speedDebounceTimer = setTimeout(() => {
        updateFirebaseDevice({ speed: val });
    }, 400); // 400ms hareketsizlikten sonra gönder
});

ledToggle.addEventListener('change', (e) => {
    updateFirebaseDevice({ led: e.target.checked });
    updatePowerBtnState();
});

function updateSlideTimeVisibility() {
    const note = document.getElementById('slide-note');
    if (slideToggle.checked) {
        slideTime.disabled = false;
        slideTime.style.opacity = '1';
        slideTime.style.pointerEvents = 'auto';
        if (note) note.style.display = 'none';
        
        prevBtn.disabled = true;
        nextBtn.disabled = true;
        prevBtn.style.opacity = '0.3';
        nextBtn.style.opacity = '0.3';
        prevBtn.style.pointerEvents = 'none';
        nextBtn.style.pointerEvents = 'none';
    } else {
        slideTime.disabled = true;
        slideTime.style.opacity = '0.5';
        slideTime.style.pointerEvents = 'none';
        if (note) note.style.display = 'block';
        
        prevBtn.disabled = false;
        nextBtn.disabled = false;
        prevBtn.style.opacity = '1';
        nextBtn.style.opacity = '1';
        prevBtn.style.pointerEvents = 'auto';
        nextBtn.style.pointerEvents = 'auto';
    }
}

slideToggle.addEventListener('change', (e) => {
    updateFirebaseDevice({ slideMode: e.target.checked });
    updateSlideTimeVisibility();
});

slideTime.addEventListener('change', (e) => {
    updateFirebaseDevice({ slideInterval: parseInt(e.target.value) });
});

powerBtn.addEventListener('click', (e) => {
    e.preventDefault();
    const isOff = (currentPhotoIndex === 8 && !ledToggle.checked);
    
    if (isOff) {
        updateFirebaseDevice({ photoIndex: 1, led: true });
        currentPhotoIndex = 1;
        ledToggle.checked = true;
    } else {
        updateFirebaseDevice({ photoIndex: 8, led: false });
        currentPhotoIndex = 8;
        ledToggle.checked = false;
    }
    
    updatePrismView();
});

// Modal Logic
settingsBtn.addEventListener('click', () => {
    settingsModal.classList.add('active');
});

settingsCloseBtn.addEventListener('click', () => {
    settingsModal.classList.remove('active');
});

calConfirmBtn.addEventListener('click', () => {
    calConfirmBtn.textContent = 'Ayarlanıyor...';
    calConfirmBtn.disabled = true;
    
    updateFirebaseDevice({ calibrate: true });
    
    setTimeout(() => {
        window.location.reload();
    }, 3000);
});

// Prevent Double-Tap Zoom on iOS / Mobile Safari
let lastTouchEnd = 0;
document.addEventListener('touchend', function(event) {
    let now = (new Date()).getTime();
    if (now - lastTouchEnd <= 300) {
        event.preventDefault();
    }
    lastTouchEnd = now;
}, false);

document.addEventListener('dblclick', function(event) {
    event.preventDefault();
}, { passive: false });

// Boot up
initPrism();
