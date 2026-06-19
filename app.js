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

// State
let currentPhotoIndex = 1;
const TOTAL_FACES = 8;
let isUpdatingFromFirebase = false;

// Initialize Prism Images
function initPrism() {
    for (let i = 1; i <= TOTAL_FACES; i++) {
        const item = document.createElement('div');
        item.classList.add('prism-item');
        
        if (i === TOTAL_FACES) {
            item.classList.add('black-screen');
        } else {
            const img = document.createElement('img');
            img.src = `https://picsum.photos/seed/memorybox${i}/400/400`;
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
}

// Firebase to UI Sync
deviceRef.on('value', (snapshot) => {
    const data = snapshot.val();
    
    if (data) {
        connectionStatus.classList.add('connected');
        connectionStatus.querySelector('.text').textContent = 'Bağlı';
        
        isUpdatingFromFirebase = true;
        
        if (data.photoIndex >= 1 && data.photoIndex <= TOTAL_FACES) {
            currentPhotoIndex = data.photoIndex;
            updatePrismView();
        }
        
        if (data.speed !== undefined) {
            speedSlider.value = data.speed;
            speedValue.textContent = data.speed;
        }
        
        if (data.led !== undefined) {
            ledToggle.checked = data.led;
        }
        
        isUpdatingFromFirebase = false;
    }
}, (error) => {
    console.error("Firebase Error: ", error);
    connectionStatus.classList.remove('connected');
    connectionStatus.querySelector('.text').textContent = 'Bağlantı Hatası';
});

// UI to Firebase Sync Functions
function updateFirebaseDevice(updates) {
    if (!isUpdatingFromFirebase) {
        deviceRef.update(updates).catch(err => console.error("Update failed:", err));
    }
}

// Event Listeners
prevBtn.addEventListener('click', () => {
    let newIndex = currentPhotoIndex - 1;
    if (newIndex < 1) newIndex = TOTAL_FACES;
    
    updateFirebaseDevice({ photoIndex: newIndex });
    currentPhotoIndex = newIndex;
    updatePrismView();
});

nextBtn.addEventListener('click', () => {
    let newIndex = currentPhotoIndex + 1;
    if (newIndex > TOTAL_FACES) newIndex = 1;
    
    updateFirebaseDevice({ photoIndex: newIndex });
    currentPhotoIndex = newIndex;
    updatePrismView();
});

speedSlider.addEventListener('input', (e) => {
    const val = parseInt(e.target.value);
    speedValue.textContent = val;
    updateFirebaseDevice({ speed: val });
});

ledToggle.addEventListener('change', (e) => {
    updateFirebaseDevice({ led: e.target.checked });
});

// Boot up
initPrism();
