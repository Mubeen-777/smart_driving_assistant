// Configuration
const API_URL = 'http://43.204.211.180:8080'; // Update with your cloud server IP
const UPDATE_INTERVAL = 500; // Update every 500ms

// State
let updateInterval = null;
let videoInterval = null;
let tripActive = false;
let cameraActive = false;
let sessionId = null;
let userData = null;

// Check authentication on page load
window.addEventListener('load', () => {
    checkAuthentication();
    initializeSpeedometer();
});

function checkAuthentication() {
    sessionId = localStorage.getItem('session_id');
    const userDataStr = localStorage.getItem('user_data');
    
    if (!sessionId || !userDataStr) {
        // Not logged in, redirect to login
        window.location.href = 'login.html';
        return;
    }
    
    try {
        userData = JSON.parse(userDataStr);
        document.getElementById('userName').textContent = userData.name;
    } catch (e) {
        console.error('Invalid user data');
        logout();
    }
}

function logout() {
    // Stop any active sessions
    if (tripActive) stopTrip();
    if (cameraActive) toggleCamera();
    
    // Clear local storage
    localStorage.removeItem('session_id');
    localStorage.removeItem('user_data');
    
    // Redirect to login
    window.location.href = 'login.html';
}

// Initialize speedometer
const canvas = document.getElementById('speedometer');
const ctx = canvas.getContext('2d');
canvas.width = 300;
canvas.height = 300;

function initializeSpeedometer() {
    drawSpeedometer(0);
}

function drawSpeedometer(speed) {
    const centerX = canvas.width / 2;
    const centerY = canvas.height / 2;
    const radius = 120;
    
    // Clear canvas
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    
    // Draw outer circle
    ctx.beginPath();
    ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI);
    ctx.strokeStyle = 'rgba(255,255,255,0.3)';
    ctx.lineWidth = 20;
    ctx.stroke();
    
    // Draw speed arc
    const maxSpeed = 200; // km/h
    const speedAngle = (Math.min(speed, maxSpeed) / maxSpeed) * 1.5 * Math.PI - 0.75 * Math.PI;
    
    // Color based on speed
    let color;
    if (speed < 60) color = '#00ff88';
    else if (speed < 100) color = '#ffd700';
    else color = '#ff4444';
    
    ctx.beginPath();
    ctx.arc(centerX, centerY, radius, -0.75 * Math.PI, speedAngle);
    ctx.strokeStyle = color;
    ctx.lineWidth = 20;
    ctx.stroke();
    
    // Draw center circle
    ctx.beginPath();
    ctx.arc(centerX, centerY, 15, 0, 2 * Math.PI);
    ctx.fillStyle = color;
    ctx.fill();
    
    // Draw needle
    ctx.save();
    ctx.translate(centerX, centerY);
    ctx.rotate(speedAngle);
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(radius - 30, 0);
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 3;
    ctx.stroke();
    ctx.restore();
    
    // Draw speed markers
    ctx.fillStyle = 'rgba(255,255,255,0.6)';
    ctx.font = '12px Arial';
    ctx.textAlign = 'center';
    
    for (let i = 0; i <= 200; i += 40) {
        const angle = (i / maxSpeed) * 1.5 * Math.PI - 0.75 * Math.PI;
        const x = centerX + (radius + 30) * Math.cos(angle);
        const y = centerY + (radius + 30) * Math.sin(angle);
        ctx.fillText(i, x, y);
    }
}

// API Functions
async function apiRequest(endpoint, method = 'POST', data = {}) {
    try {
        // Add session_id to all requests
        const requestData = {
            ...data,
            session_id: sessionId
        };
        
        const response = await fetch(`${API_URL}${endpoint}`, {
            method: method,
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(requestData)
        });
        
        if (response.status === 401) {
            // Session expired
            alert('Session expired. Please login again.');
            logout();
            return null;
        }
        
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        return await response.json();
    } catch (error) {
        console.error('API Error:', error);
        return null;
    }
}

async function startTrip() {
    const result = await apiRequest('/api/trip/start', 'POST', {
        operation: 'trip_start',
        driver_id: userData.driver_id,
        vehicle_id: 1 // Default vehicle
    });
    
    if (result && result.status === 'success') {
        tripActive = true;
        document.getElementById('startTripBtn').disabled = true;
        document.getElementById('stopTripBtn').disabled = false;
        document.getElementById('tripStatus').classList.add('active');
        document.getElementById('tripStatus').textContent = 'Trip: Active';
        
        // Start updating data
        if (!updateInterval) {
            updateInterval = setInterval(updateDashboard, UPDATE_INTERVAL);
        }
        
        console.log('Trip started');
    } else {
        alert('Failed to start trip: ' + (result?.message || 'Unknown error'));
    }
}

async function stopTrip() {
    const result = await apiRequest('/api/trip/stop', 'POST', {
        operation: 'trip_end'
    });
    
    if (result && result.status === 'success') {
        tripActive = false;
        document.getElementById('startTripBtn').disabled = false;
        document.getElementById('stopTripBtn').disabled = true;
        document.getElementById('tripStatus').classList.remove('active');
        document.getElementById('tripStatus').textContent = 'Trip: Inactive';
        
        // Stop updating
        if (updateInterval) {
            clearInterval(updateInterval);
            updateInterval = null;
        }
        
        console.log('Trip stopped');
    } else {
        alert('Failed to stop trip');
    }
}

async function toggleCamera() {
    const btn = document.getElementById('startCameraBtn');
    
    if (!cameraActive) {
        const result = await apiRequest('/api/camera/start', 'POST');
        
        if (result && result.status === 'success') {
            cameraActive = true;
            btn.textContent = 'Stop Camera';
            btn.classList.remove('primary');
            btn.classList.add('danger');
            document.getElementById('cameraStatus').classList.add('active');
            document.getElementById('cameraStatus').textContent = 'Camera: Active';
            
            // Show video elements
            document.getElementById('videoFeed').style.display = 'block';
            document.getElementById('noVideo').style.display = 'none';
            document.getElementById('laneStatus').style.display = 'block';
            
            // Start video stream
            startVideoStream();
        }
    } else {
        const result = await apiRequest('/api/camera/stop', 'POST');
        
        if (result && result.status === 'success') {
            cameraActive = false;
            btn.textContent = 'Start Camera';
            btn.classList.remove('danger');
            btn.classList.add('primary');
            document.getElementById('cameraStatus').classList.remove('active');
            document.getElementById('cameraStatus').textContent = 'Camera: Inactive';
            
            // Hide video elements
            document.getElementById('videoFeed').style.display = 'none';
            document.getElementById('noVideo').style.display = 'flex';
            document.getElementById('laneStatus').style.display = 'none';
            
            // Stop video stream
            stopVideoStream();
        }
    }
}

function startVideoStream() {
    // Update video feed every 100ms
    videoInterval = setInterval(() => {
        document.getElementById('videoFeed').src = 
            `${API_URL}/api/camera/frame?session_id=${sessionId}&t=${Date.now()}`;
    }, 100);
}

function stopVideoStream() {
    if (videoInterval) {
        clearInterval(videoInterval);
        videoInterval = null;
    }
    document.getElementById('videoFeed').src = '';
}

async function updateDashboard() {
    const stats = await apiRequest('/api/stats', 'GET');
    
    if (!stats) return;
    
    // Update speedometer
    const speed = Math.round(stats.speed || 0);
    document.getElementById('speedValue').textContent = speed;
    drawSpeedometer(speed);
    
    // Update acceleration
    const accel = (stats.acceleration || 0).toFixed(2);
    const accelElem = document.getElementById('accelValue');
    accelElem.textContent = `${accel} m/s²`;
    
    // Color code acceleration
    if (Math.abs(accel) > 3) {
        accelElem.style.color = '#ff4444';
    } else if (Math.abs(accel) > 2) {
        accelElem.style.color = '#ffd700';
    } else {
        accelElem.style.color = '#00ff88';
    }
    
    // Update lane status
    document.getElementById('laneStatus').textContent = 
        `Lane: ${stats.lane_status || 'CENTERED'}`;
    
    // Update counters
    document.getElementById('rapidAccelCount').textContent = 
        stats.rapid_accel_count || 0;
    document.getElementById('hardBrakeCount').textContent = 
        stats.hard_brake_count || 0;
    document.getElementById('laneDepartureCount').textContent = 
        stats.lane_departures || 0;
    document.getElementById('safetyScore').textContent = 
        stats.safety_score || 1000;
    
    // Update safety score color
    const safetyScore = stats.safety_score || 1000;
    const scoreElem = document.getElementById('safetyScore');
    if (safetyScore >= 800) {
        scoreElem.style.color = '#00ff88';
    } else if (safetyScore >= 600) {
        scoreElem.style.color = '#ffd700';
    } else {
        scoreElem.style.color = '#ff4444';
    }
    
    // Handle warnings
    const warningBanner = document.getElementById('warningBanner');
    if (stats.warning_active) {
        warningBanner.textContent = stats.warning_message;
        warningBanner.classList.add('active');
    } else {
        warningBanner.classList.remove('active');
    }
    
    // Update GPS status
    if (stats.trip_active) {
        document.getElementById('gpsStatus').classList.add('active');
        document.getElementById('gpsStatus').textContent = 'GPS: Connected';
    }
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (updateInterval) {
        clearInterval(updateInterval);
    }
    if (videoInterval) {
        clearInterval(videoInterval);
    }
});