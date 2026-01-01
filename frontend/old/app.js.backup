// app.js - FIXED VERSION WITH WORKING BUTTONS AND REAL-TIME DATA
class SmartDriveWebApp {
    constructor() {
        this.WS_URL = 'ws://localhost:8081';
        this.sessionId = localStorage.getItem('session_id');
        this.userData = JSON.parse(localStorage.getItem('user_data') || '{}');
        this.ws = null;
        this.isConnected = false;
        this.isCameraActive = false;
        this.cameraWasStopped = false;
        this.isTripActive = false;
        this.currentPage = 'dashboard';
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.dashboardRefreshInterval = null;

        // Real-time data from WebSocket
        this.liveData = {
            speed: 0,
            acceleration: 0,
            safety_score: 1000,
            lane_status: 'CENTERED',
            rapid_accel_count: 0,
            hard_brake_count: 0,
            lane_departures: 0,
            trip_active: false,
            trip_id: 0,
            latitude: 31.5204,
            longitude: 74.3587
        };

        // FPS tracking
        this.frameCount = 0;
        this.lastFpsUpdate = Date.now();
        this.fpsCounter = 0;

        this.init();
    }

    async init() {
        console.log('🚀 Starting Smart Drive Application...');

        if (!this.checkAuth()) {
            console.log('❌ No session, redirecting to login...');
            window.location.href = 'login.html';
            return;
        }

        this.setupEventListeners();
        this.updateUserUI();
        
        // Initialize GPS Manager
        if (typeof GPSManager !== 'undefined') {
            try {
                this.gpsManager = new GPSManager(this);
                window.gpsManager = this.gpsManager; // Make globally accessible
                const gpsStarted = this.gpsManager.startTracking();
                if (gpsStarted) {
                    console.log('✅ GPS Manager initialized and started');
                } else {
                    console.warn('⚠️ GPS Manager initialized but failed to start tracking');
                }
            } catch (error) {
                console.error('❌ Failed to initialize GPS Manager:', error);
                this.showToast('Failed to initialize GPS: ' + error.message, 'error');
            }
        } else {
            console.warn('⚠️ GPSManager class not found. GPS tracking will not work.');
        }
        
        this.loadPage('dashboard');
        this.connectWebSocket();

        // Initialize database API
        if (!window.db) {
            window.db = new DatabaseAPI(this);
        }

        // Initialize analytics manager
        if (!window.analytics) {
            window.analytics = new AnalyticsManager(this);
        }

        // Initialize modal manager (if available)
        if (typeof ModalManager !== 'undefined' && !window.modals) {
            window.modals = new ModalManager(this);
        }

        // Check backend connection
        this.checkBackendConnection();
        
        // Auto-start camera after login (camera runs permanently)
        this.autoStartCamera();

        console.log('✅ App initialized');
        console.log('✅ window.app set:', window.app === this);
    }
    
    // Auto-start camera - runs permanently after login
    autoStartCamera() {
        // Wait for WebSocket to connect, then start camera
        const startCameraWhenReady = () => {
            if (this.isConnected && this.ws && this.ws.readyState === WebSocket.OPEN) {
                console.log('📹 Auto-starting camera...');
                this.ws.send(JSON.stringify({
                    command: 'toggle_camera',
                    enable: true
                }));
                this.isCameraActive = true;
                this.cameraWasStopped = false;
                this.updateCameraStatus(true);
                this.prepareVideoFeeds();
                this.showToast('Camera started automatically', 'success');
            } else {
                // Retry in 500ms if not connected yet
                setTimeout(startCameraWhenReady, 500);
            }
        };
        
        // Start checking after a brief delay to allow WebSocket connection
        setTimeout(startCameraWhenReady, 1000);
    }

    async checkBackendConnection() {
        try {
            // Use proxy endpoint to check backend
            const response = await fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ operation: 'user_login', username: 'test', password: 'test' })
            });
            const result = await response.json();
            // Even if login fails, if we get a JSON response, backend is reachable
            if (result.code === 'BACKEND_UNAVAILABLE') {
                throw new Error('Backend unavailable');
            }
            console.log('✅ Backend server is reachable');
        } catch (error) {
            console.warn('⚠️ Backend server not reachable:', error.message);
            this.showToast('Backend server not running. Make sure C++ server is running on port 8080.', 'warning');
        }
    }

    checkAuth() {
        return this.sessionId && this.userData.driver_id;
    }

    connectWebSocket() {
        console.log('🔌 Connecting to WebSocket:', this.WS_URL);

        try {
            this.ws = new WebSocket(this.WS_URL);

            this.ws.onopen = () => {
                console.log('✅ WebSocket Connected');
                this.isConnected = true;
                this.reconnectAttempts = 0;
                this.updateConnectionStatus(true);
                this.showToast('Connected to backend system', 'success');

                // Always restart camera on reconnection (camera runs permanently)
                setTimeout(() => {
                    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
                        this.ws.send(JSON.stringify({
                            command: 'toggle_camera',
                            enable: true
                        }));
                        this.isCameraActive = true;
                        this.updateCameraStatus(true);
                        this.prepareVideoFeeds();
                        console.log('📹 Camera started/restarted after WebSocket connection');
                    }
                }, 500);

                // Keep-alive ping
                if (this.keepAliveInterval) {
                    clearInterval(this.keepAliveInterval);
                }
                this.keepAliveInterval = setInterval(() => {
                    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
                        this.ws.send(JSON.stringify({ command: 'ping' }));
                    }
                }, 30000);
            };

            this.ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    this.handleWebSocketMessage(data);
                } catch (error) {
                    console.error('Error parsing message:', error);
                }
            };

            this.ws.onerror = (error) => {
                console.error('❌ WebSocket error:', error);
                this.updateConnectionStatus(false);
            };

            this.ws.onclose = () => {
                console.log('🔌 WebSocket disconnected');
                this.isConnected = false;
                this.updateConnectionStatus(false);

                if (this.reconnectAttempts < this.maxReconnectAttempts) {
                    this.reconnectAttempts++;
                    console.log(`🔄 Reconnecting... (${this.reconnectAttempts}/${this.maxReconnectAttempts})`);
                    setTimeout(() => this.connectWebSocket(), 3000);
                }
            };

        } catch (error) {
            console.error('❌ WebSocket creation failed:', error);
            this.showToast('Cannot connect to backend. Check if websocket_bridge is running.', 'error');
        }
    }

    handleWebSocketMessage(data) {
        switch (data.type) {
            case 'video_frame':
                this.displayVideoFrame(data.data, data.timestamp);
                break;

            case 'live_data':
                if (data.data) {
                    this.updateLiveData(data.data);
                }
                break;

            case 'detection_data':
                // Handle lane detection and vision data
                if (window.cameraManager && data.data) {
                    window.cameraManager.updateFromWebSocket(data.data);
                }
                break;

            case 'lane_warning':
                this.showToast(`⚠️ LANE DEPARTURE: ${data.data.direction}`, 'warning');
                this.playAlertSound();
                this.liveData.lane_departures++;
                this.updateDashboard();
                break;

            case 'warning':
                this.showToast(`⚠️ ${data.data.warning_type}: ${data.data.value.toFixed(2)}`, 'warning');
                this.playAlertSound();
                break;

            case 'trip_started':
                this.isTripActive = true;
                this.liveData.trip_active = true;
                if (data.data && data.data.trip_id) {
                    this.liveData.trip_id = parseInt(data.data.trip_id) || 0;
                }
                this.updateTripControls();
                this.showToast('Trip started successfully!', 'success');
                if (this.currentPage === 'trips') {
                    this.loadTrips();
                }
                break;

            case 'trip_stopped':
                this.isTripActive = false;
                this.liveData.trip_active = false;
                this.liveData.trip_id = 0;
                this.updateTripControls();
                this.showToast(`Trip ended. Distance: ${data.data?.distance || 'N/A'}`, 'success');
                if (this.currentPage === 'trips') {
                    this.loadTrips();
                }
                break;

            case 'camera_status':
                const wasActive = this.isCameraActive;
                this.isCameraActive = data.data.enabled;
                this.updateCameraStatus(data.data.enabled);
                
                // If camera just became active, ensure video feeds are ready
                if (!wasActive && data.data.enabled) {
                    this.prepareVideoFeeds();
                }
                break;
            
            case 'camera_reset':
                // Camera was reset, ready for new stream
                console.log('Camera reset acknowledged');
                this.cameraWasStopped = false;
                break;
        }
    }

    updateLiveData(data) {
        if (!data) {
            console.warn('⚠️ updateLiveData called with null/undefined data');
            return;
        }

        let dataChanged = false;

        if (data.speed !== undefined && !isNaN(data.speed)) {
            this.liveData.speed = Math.max(0, data.speed);
            dataChanged = true;
        }
        if (data.acceleration !== undefined && !isNaN(data.acceleration)) {
            this.liveData.acceleration = data.acceleration;
            dataChanged = true;
        }
        if (data.safety_score !== undefined) this.liveData.safety_score = data.safety_score;
        if (data.lane_status !== undefined) this.liveData.lane_status = data.lane_status;
        if (data.rapid_accel_count !== undefined) this.liveData.rapid_accel_count = data.rapid_accel_count;
        if (data.hard_brake_count !== undefined) this.liveData.hard_brake_count = data.hard_brake_count;
        if (data.lane_departures !== undefined) this.liveData.lane_departures = data.lane_departures;
        if (data.trip_active !== undefined) this.liveData.trip_active = data.trip_active;
        if (data.trip_id !== undefined) this.liveData.trip_id = data.trip_id;
        if (data.latitude !== undefined && !isNaN(data.latitude)) {
            this.liveData.latitude = data.latitude;
            dataChanged = true;
        }
        if (data.longitude !== undefined && !isNaN(data.longitude)) {
            this.liveData.longitude = data.longitude;
            dataChanged = true;
        }
        
        // Update GPS if available
        if (this.gpsManager && data.latitude && data.longitude) {
            this.gpsManager.lastPosition = { lat: data.latitude, lon: data.longitude };
        }
        
        // Only update dashboard if data actually changed (to avoid unnecessary redraws)
        if (dataChanged) {
            this.updateDashboard();
        }
    }

    updateLocationUI(lat, lon, accuracy) {
        // Update location data immediately
        this.liveData.latitude = lat;
        this.liveData.longitude = lon;
        
        // Update GPS display elements directly for immediate feedback
        const gpsLat = document.getElementById('gpsLat');
        const gpsLon = document.getElementById('gpsLon');
        if (gpsLat) gpsLat.textContent = lat.toFixed(6) + '°';
        if (gpsLon) gpsLon.textContent = lon.toFixed(6) + '°';
        
        // Note: updateLiveData will also update these, but this ensures immediate UI feedback
    }

    // In app.js, replace the displayVideoFrame function:

    displayVideoFrame(base64Data, timestamp) {
        // Only display if camera is active
        if (!this.isCameraActive) {
            return;
        }

        // For dashboard
        const videoFeed = document.getElementById('videoFeed');
        const noVideo = document.getElementById('noVideo');

        if (videoFeed && noVideo) {
            try {
                videoFeed.src = 'data:image/jpeg;base64,' + base64Data;
                videoFeed.style.display = 'block';
                noVideo.style.display = 'none';
            } catch (error) {
                console.error('Error displaying video frame:', error);
            }
        }

        // For vision page (camera template)
        const visionFeed = document.getElementById('visionFeed');
        const visionPlaceholder = document.getElementById('visionPlaceholder');

        if (visionFeed && visionPlaceholder) {
            try {
                visionFeed.src = 'data:image/jpeg;base64,' + base64Data;
                visionFeed.style.display = 'block';
                visionPlaceholder.style.display = 'none';
            } catch (error) {
                console.error('Error displaying vision frame:', error);
            }
        }

        // Update FPS
        this.frameCount++;
        this.fpsCounter++;
        const now = Date.now();
        const elapsed = now - this.lastFpsUpdate;

        if (elapsed >= 1000) {
            const fps = Math.round(this.fpsCounter * 1000 / elapsed);

            // Dashboard FPS
            const fpsDisplay = document.getElementById('fpsDisplay');
            if (fpsDisplay) fpsDisplay.textContent = `FPS: ${fps}`;

            // Vision page FPS
            const visionFps = document.getElementById('visionFps');
            if (visionFps) visionFps.textContent = `FPS: ${fps}`;

            this.fpsCounter = 0;
            this.lastFpsUpdate = now;
        }
    }

    // BUTTON HANDLERS - These are the real working functions
    startTrip() {
        console.log('Start trip clicked');
        
        if (!this.isConnected && !this.ws) {
            this.showToast('Not connected to WebSocket! Trying to connect...', 'warning');
            this.connectWebSocket();
            setTimeout(() => {
                if (this.isConnected) {
                    this.startTrip();
                } else {
                    this.showToast('Failed to connect. Please check if websocket_bridge is running.', 'error');
                }
            }, 2000);
            return;
        }

        const vehicleId = document.getElementById('vehicleSelect')?.value || 1;
        
        if (!vehicleId || vehicleId === '') {
            this.showToast('Please select a vehicle first', 'error');
            return;
        }

        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify({
                command: 'start_trip',
                driver_id: this.userData.driver_id || 1,
                vehicle_id: parseInt(vehicleId)
            }));
            this.showToast('Starting trip...', 'info');
        } else {
            this.showToast('WebSocket not connected. Please wait...', 'warning');
        }
    }

    stopTrip() {
        console.log('Stop trip clicked');
        
        if (!this.isConnected && !this.ws) {
            this.showToast('Not connected to WebSocket!', 'error');
            return;
        }

        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify({
                command: 'stop_trip'
            }));
            this.showToast('Stopping trip...', 'info');
        } else {
            this.showToast('WebSocket not connected. Please wait...', 'warning');
        }
    }

    toggleCamera() {
        console.log('Toggle camera clicked, current state:', this.isCameraActive);
        
        if (!this.isConnected && !this.ws) {
            this.showToast('Not connected to WebSocket! Trying to connect...', 'warning');
            this.connectWebSocket();
            setTimeout(() => {
                if (this.isConnected) {
                    this.toggleCamera();
                } else {
                    this.showToast('Failed to connect. Please check if websocket_bridge is running.', 'error');
                }
            }, 2000);
            return;
        }

        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            const newState = !this.isCameraActive;
            
            // If starting camera after it was stopped, send a reset command first
            if (newState && this.cameraWasStopped) {
                this.ws.send(JSON.stringify({
                    command: 'reset_camera'
                }));
                // Small delay before starting
                setTimeout(() => {
                    this.ws.send(JSON.stringify({
                        command: 'toggle_camera',
                        enable: true
                    }));
                }, 100);
            } else {
                this.ws.send(JSON.stringify({
                    command: 'toggle_camera',
                    enable: newState
                }));
            }
            
            // Track if camera was stopped for restart handling
            if (!newState) {
                this.cameraWasStopped = true;
            } else {
                this.cameraWasStopped = false;
            }
            
            // Reset video feed elements when stopping
            if (!newState) {
                this.resetVideoFeeds();
            }
            
            // Optimistically update UI immediately
            this.isCameraActive = newState;
            this.updateCameraStatus(newState);
            
            this.showToast(newState ? 'Starting camera...' : 'Stopping camera...', 'info');
        } else {
            this.showToast('WebSocket not connected. Please wait...', 'warning');
        }
    }

    resetVideoFeeds() {
        // Reset dashboard video feed
        const videoFeed = document.getElementById('videoFeed');
        const noVideo = document.getElementById('noVideo');
        if (videoFeed) {
            videoFeed.src = '';
            videoFeed.style.display = 'none';
        }
        if (noVideo) {
            noVideo.style.display = 'flex';
        }
        
        // Reset vision page feed
        const visionFeed = document.getElementById('visionFeed');
        const visionPlaceholder = document.getElementById('visionPlaceholder');
        if (visionFeed) {
            visionFeed.src = '';
            visionFeed.style.display = 'none';
        }
        if (visionPlaceholder) {
            visionPlaceholder.style.display = 'flex';
        }
        
        // Reset FPS counters
        this.frameCount = 0;
        this.fpsCounter = 0;
        const fpsDisplay = document.getElementById('fpsDisplay');
        const visionFps = document.getElementById('visionFps');
        if (fpsDisplay) fpsDisplay.textContent = 'FPS: 0';
        if (visionFps) visionFps.textContent = 'FPS: 0';
    }

    prepareVideoFeeds() {
        // Prepare dashboard video feed for receiving frames
        const videoFeed = document.getElementById('videoFeed');
        const noVideo = document.getElementById('noVideo');
        if (videoFeed) {
            videoFeed.src = '';
            videoFeed.style.display = 'block';
        }
        if (noVideo) {
            noVideo.style.display = 'none';
        }
        
        // Prepare vision page feed
        const visionFeed = document.getElementById('visionFeed');
        const visionPlaceholder = document.getElementById('visionPlaceholder');
        if (visionFeed) {
            visionFeed.src = '';
            visionFeed.style.display = 'block';
        }
        if (visionPlaceholder) {
            visionPlaceholder.style.display = 'none';
        }
        
        // Reset FPS counter for new stream
        this.frameCount = 0;
        this.fpsCounter = 0;
        this.lastFpsUpdate = Date.now();
    }

    // UI UPDATE FUNCTIONS
    updateDashboard() {
        // Update speed
        const speedValue = document.getElementById('speedValue');
        if (speedValue) speedValue.textContent = Math.round(this.liveData.speed);

        // Update acceleration
        const accelValue = document.getElementById('accelValue');
        if (accelValue) accelValue.textContent = this.liveData.acceleration.toFixed(2);

        // Update safety score
        const safetyScore = document.getElementById('safetyScore');
        if (safetyScore) safetyScore.textContent = this.liveData.safety_score;

        // Update GPS
        const gpsLat = document.getElementById('gpsLat');
        const gpsLon = document.getElementById('gpsLon');
        if (gpsLat) gpsLat.textContent = this.liveData.latitude.toFixed(6) + '°';
        if (gpsLon) gpsLon.textContent = this.liveData.longitude.toFixed(6) + '°';

        // Update event counters
        const harshBrakeCount = document.getElementById('harshBrakeCount');
        if (harshBrakeCount) harshBrakeCount.textContent = this.liveData.hard_brake_count;

        const rapidAccelCount = document.getElementById('rapidAccelCount');
        if (rapidAccelCount) rapidAccelCount.textContent = this.liveData.rapid_accel_count;

        const laneDepartureCount = document.getElementById('laneDepartureCount');
        if (laneDepartureCount) laneDepartureCount.textContent = this.liveData.lane_departures;

        // Update top bar stats
        const liveSpeed = document.getElementById('liveSpeed');
        const liveAccel = document.getElementById('liveAccel');
        const liveSafety = document.getElementById('liveSafety');

        if (liveSpeed) liveSpeed.textContent = Math.round(this.liveData.speed);
        if (liveAccel) liveAccel.textContent = this.liveData.acceleration.toFixed(1);
        if (liveSafety) liveSafety.textContent = this.liveData.safety_score;

        // Draw speedometer
        this.drawSpeedometer(this.liveData.speed);
    }

    drawSpeedometer(speed) {
        const canvas = document.getElementById('speedometer');
        if (!canvas) {
            console.warn('⚠️ Speedometer canvas not found');
            return;
        }

        // Ensure speed is a valid number
        if (typeof speed !== 'number' || isNaN(speed)) {
            speed = 0;
        }
        speed = Math.max(0, Math.min(speed, 300)); // Clamp between 0 and 300 km/h

        const ctx = canvas.getContext('2d');
        if (!ctx) {
            console.error('❌ Failed to get canvas context');
            return;
        }

        const centerX = canvas.width / 2;
        const centerY = canvas.height / 2;
        const radius = 100;

        ctx.clearRect(0, 0, canvas.width, canvas.height);

        // Outer circle
        ctx.beginPath();
        ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI);
        ctx.strokeStyle = 'rgba(67, 97, 238, 0.2)';
        ctx.lineWidth = 15;
        ctx.stroke();

        // Speed arc
        const maxSpeed = 200;
        const speedAngle = (Math.min(speed, maxSpeed) / maxSpeed) * 1.5 * Math.PI - 0.75 * Math.PI;
        const color = speed < 60 ? '#4cc9f0' : speed < 100 ? '#f8961e' : '#f72585';

        ctx.beginPath();
        ctx.arc(centerX, centerY, radius, -0.75 * Math.PI, speedAngle);
        ctx.strokeStyle = color;
        ctx.lineWidth = 15;
        ctx.stroke();

        // Center dot
        ctx.beginPath();
        ctx.arc(centerX, centerY, 10, 0, 2 * Math.PI);
        ctx.fillStyle = color;
        ctx.fill();

        // Needle
        ctx.save();
        ctx.translate(centerX, centerY);
        ctx.rotate(speedAngle);
        ctx.beginPath();
        ctx.moveTo(0, 0);
        ctx.lineTo(radius - 25, 0);
        ctx.strokeStyle = '#333';
        ctx.lineWidth = 4;
        ctx.stroke();
        ctx.restore();

        // Speed markers
        ctx.fillStyle = '#666';
        ctx.font = '12px Arial';
        ctx.textAlign = 'center';

        for (let i = 0; i <= 200; i += 40) {
            const angle = (i / maxSpeed) * 1.5 * Math.PI - 0.75 * Math.PI;
            const x = centerX + (radius + 20) * Math.cos(angle);
            const y = centerY + (radius + 20) * Math.sin(angle);
            ctx.fillText(i, x, y);
        }
    }

    updateTripControls() {
        const startBtn = document.getElementById('startTripBtn');
        const stopBtn = document.getElementById('stopTripBtn');
        const tripStatus = document.getElementById('tripStatus');
        const currentTripInfo = document.getElementById('currentTripInfo');
        const activeTripId = document.getElementById('activeTripId');

        if (startBtn) startBtn.disabled = this.isTripActive;
        if (stopBtn) stopBtn.disabled = !this.isTripActive;
        if (tripStatus) {
            tripStatus.textContent = this.isTripActive ? 'Active' : 'Inactive';
            tripStatus.parentElement.classList.toggle('active', this.isTripActive);
        }
        if (currentTripInfo) {
            currentTripInfo.style.display = this.isTripActive ? 'block' : 'none';
        }
        if (activeTripId) {
            activeTripId.textContent = this.liveData.trip_id || '0';
        }
    }

    // Update camera status display (camera runs permanently - no toggle)
    updateCameraStatus(enabled) {
        this.isCameraActive = enabled;

        // Update dashboard camera status badge
        const cameraStatusBadge = document.getElementById('cameraStatusBadge');

        if (cameraStatusBadge) {
            if (enabled) {
                cameraStatusBadge.innerHTML = '<i class="fas fa-circle" style="font-size: 8px; animation: pulse-dot 2s infinite;"></i> Active';
                cameraStatusBadge.className = 'status-badge success';
                
                // Show video feed when camera is enabled
                const videoFeed = document.getElementById('videoFeed');
                const noVideo = document.getElementById('noVideo');
                if (videoFeed && noVideo) {
                    videoFeed.src = '';
                    videoFeed.style.display = 'block';
                    noVideo.style.display = 'none';
                }
            } else {
                cameraStatusBadge.innerHTML = '<i class="fas fa-circle" style="font-size: 8px;"></i> Reconnecting...';
                cameraStatusBadge.className = 'status-badge warning';

                // Show loading placeholder
                const videoFeed = document.getElementById('videoFeed');
                const noVideo = document.getElementById('noVideo');
                if (videoFeed && noVideo) {
                    videoFeed.style.display = 'none';
                    noVideo.style.display = 'flex';
                    noVideo.innerHTML = `
                        <i class="fas fa-spinner fa-spin"></i>
                        <p>Reconnecting camera...</p>
                        <p class="small">Please wait</p>
                    `;
                }
            }
        }

        // Update vision page feed (also runs permanently)
        const visionFeed = document.getElementById('visionFeed');
        const visionPlaceholder = document.getElementById('visionPlaceholder');
        const visionStatusBadge = document.getElementById('visionStatusBadge');
        
        if (visionStatusBadge) {
            if (enabled) {
                visionStatusBadge.innerHTML = '<i class="fas fa-circle" style="font-size: 8px; animation: pulse-dot 2s infinite;"></i> Active';
                visionStatusBadge.className = 'status-badge success';
            } else {
                visionStatusBadge.innerHTML = '<i class="fas fa-circle" style="font-size: 8px;"></i> Reconnecting...';
                visionStatusBadge.className = 'status-badge warning';
            }
        }
        
        if (visionFeed && visionPlaceholder) {
            if (enabled) {
                visionFeed.src = '';
                visionFeed.style.display = 'block';
                visionPlaceholder.style.display = 'none';
            } else {
                visionFeed.style.display = 'none';
                visionPlaceholder.style.display = 'flex';
            }
        }
    }

    updateConnectionStatus(connected) {
        this.isConnected = connected;
        const statusDot = document.getElementById('wsStatusDot');
        const statusText = document.getElementById('wsStatus');

        if (statusDot) {
            if (connected) {
                statusDot.classList.add('connected');
            } else {
                statusDot.classList.remove('connected');
            }
        }

        if (statusText) {
            statusText.textContent = connected ? 'Connected' : 'Disconnected';
        }
    }

    // PAGE NAVIGATION
    loadPage(pageName) {
        this.currentPage = pageName;

        // Update active nav
        document.querySelectorAll('.nav-item').forEach(item => {
            item.classList.remove('active');
            if (item.dataset.page === pageName) {
                item.classList.add('active');
            }
        });

        // Update page title
        const titles = {
            dashboard: 'Dashboard',
            lane_detection: 'Lane Detection System',
            camera: 'Vision System',
            trips: 'Trip Management',
            vehicles: 'Vehicle Management',
            expenses: 'Expense Tracking',
            drivers: 'Driver Profiles',
            incidents: 'Incident Management',
            reports: 'Analytics & Reports',
            settings: 'Settings'
        };

        const title = document.getElementById('pageTitle');
        const subtitle = document.getElementById('pageSubtitle');

        if (title) title.textContent = titles[pageName] || pageName;
        if (subtitle) {
            const subtitles = {
                dashboard: 'Real-time monitoring and GPS tracking',
                lane_detection: 'Computer vision and lane departure warnings',
                camera: 'Computer vision and lane departure warnings',
                trips: 'View and manage your driving trips',
                vehicles: 'Manage your vehicles and maintenance',
                expenses: 'Track and manage vehicle expenses',
                drivers: 'Driver profiles and behavior',
                incidents: 'Report and track incidents',
                reports: 'Comprehensive analytics and reports',
                settings: 'System configuration'
            };
            subtitle.textContent = subtitles[pageName] || '';
        }

        // Load page content
        // Handle special case for camera page
        const templateId = pageName === 'camera' ? 'cameraTemplate' : pageName + 'Template';
        const template = document.getElementById(templateId);
        const contentArea = document.getElementById('contentArea');

        if (template && contentArea) {
            contentArea.innerHTML = template.innerHTML;
            this.initPageComponents(pageName);
            // Re-attach event listeners after content is loaded
            this.attachPageListeners();
        } else {
            console.warn(`Template not found: ${templateId}`);
            if (contentArea) {
                contentArea.innerHTML = `<div class="empty-state"><h3>Page not found</h3><p>Template for "${pageName}" is missing.</p></div>`;
            }
        }
    }

    initPageComponents(pageName) {
        switch (pageName) {
            case 'dashboard':
                this.initDashboard();
                break;
            case 'trips':
                this.loadTrips();
                break;
            case 'vehicles':
                this.loadVehicles();
                break;
            case 'expenses':
                this.loadExpenses();
                break;
            case 'drivers':
                this.loadDrivers();
                break;
            case 'incidents':
                this.loadIncidents();
                break;
            case 'reports':
                this.loadReports();
                break;
            case 'camera':
                this.initLaneDetection();
                break;
        }
    }

    initLaneDetection() {
        // Initialize camera manager for vision system
        if (typeof CameraManager !== 'undefined' && !window.cameraManager) {
            window.cameraManager = new CameraManager(this);
            window.cameraManager.initVisionSystem();
        }
        
        // Update camera status if already active
        if (this.isCameraActive) {
            const visionFeed = document.getElementById('visionFeed');
            const visionPlaceholder = document.getElementById('visionPlaceholder');
            if (visionFeed && visionPlaceholder) {
                visionFeed.style.display = 'block';
                visionPlaceholder.style.display = 'none';
            }
        }

        // Setup night vision toggle
        const nightModeToggle = document.getElementById('nightModeToggle');
        if (nightModeToggle) {
            nightModeToggle.addEventListener('change', () => {
                if (window.cameraManager) {
                    window.cameraManager.toggleNightVision();
                }
            });
        }
    }

    async loadTrips() {
        if (!window.db) {
            console.error('Database API not available');
            return;
        }

        try {
            const trips = await window.db.getTripHistory(50);
            this.renderTripsTable(trips);

            const stats = await window.db.getTripStatistics();
            if (stats) {
                this.updateTripStats(stats);
            }
        } catch (error) {
            console.error('Failed to load trips:', error);
            this.showToast('Failed to load trips: ' + error.message, 'error');
        }
    }

    renderTripsTable(trips) {
        const tbody = document.querySelector('#tripsTable tbody');
        if (!tbody) return;

        tbody.innerHTML = '';

        if (trips.length === 0) {
            tbody.innerHTML = '<tr><td colspan="8" style="text-align: center;">No trips found</td></tr>';
            return;
        }

        trips.forEach(trip => {
            const row = document.createElement('tr');
            const startTime = trip.start_time ? new Date(parseInt(trip.start_time) / 1000000).toLocaleString() : 'N/A';
            const endTime = trip.end_time ? new Date(parseInt(trip.end_time) / 1000000).toLocaleString() : 'N/A';
            const duration = trip.duration ? this.formatDuration(trip.duration) : 'N/A';
            
            row.innerHTML = `
                <td>${trip.trip_id || 'N/A'}</td>
                <td>${startTime}</td>
                <td>${trip.vehicle_id || 'N/A'}</td>
                <td>${parseFloat(trip.distance || 0).toFixed(2)} km</td>
                <td>${duration}</td>
                <td>${parseFloat(trip.avg_speed || 0).toFixed(1)} km/h</td>
                <td>${trip.safety_score || 'N/A'}</td>
                <td>
                    <button class="btn btn-sm btn-primary" onclick="app.viewTripDetails(${trip.trip_id})">
                        <i class="fas fa-eye"></i> View
                    </button>
                </td>
            `;
            tbody.appendChild(row);
        });
    }

    updateTripStats(stats) {
        const elements = {
            statsTotalTrips: stats.total_trips || 0,
            statsTotalDistance: (stats.total_distance || 0).toFixed(1) + ' km',
            statsAvgSpeed: (stats.avg_speed || 0).toFixed(1) + ' km/h',
            statsFuelEfficiency: '12.5 km/L'
        };

        Object.entries(elements).forEach(([id, value]) => {
            const el = document.getElementById(id);
            if (el) el.textContent = value;
        });
    }

    async loadVehicles() {
        if (!window.db) {
            console.error('Database API not available');
            return;
        }

        try {
            const vehicles = await window.db.getVehicles();
            this.renderVehiclesGrid(vehicles);
            this.populateVehicleSelects(vehicles);
            
            const alerts = await window.db.getMaintenanceAlerts();
            this.renderMaintenanceAlerts(alerts);
            
            if (vehicles.length > 0) {
                await this.loadMaintenanceHistory(vehicles[0].vehicle_id);
            }
        } catch (error) {
            console.error('Failed to load vehicles:', error);
            this.showToast('Failed to load vehicles: ' + error.message, 'error');
        }
    }

    async loadMaintenanceHistory(vehicleId) {
        if (!window.db) return;

        try {
            const maintenance = await window.db.getMaintenanceHistory(vehicleId);
            this.renderMaintenanceTable(maintenance);
        } catch (error) {
            console.error('Failed to load maintenance history:', error);
        }
    }

    renderMaintenanceAlerts(alerts) {
        const container = document.getElementById('maintenanceAlerts');
        if (!container) return;

        const alertCount = document.getElementById('alertCount');
        if (alertCount) {
            alertCount.textContent = alerts.length;
        }

        if (alerts.length === 0) {
            container.innerHTML = '<p style="text-align: center; color: green;">No maintenance alerts</p>';
            return;
        }

        container.innerHTML = alerts.map(alert => {
            return `
                <div class="alert-item warning">
                    <i class="fas fa-exclamation-triangle"></i>
                    <div class="alert-content">
                        <strong>Vehicle ${alert.vehicle_id}</strong>
                        <p>${alert.type || alert.description || 'Maintenance required'}</p>
                    </div>
                </div>
            `;
        }).join('');
    }

    renderMaintenanceTable(maintenance) {
        const tbody = document.querySelector('#maintenanceTable tbody');
        if (!tbody) return;

        tbody.innerHTML = '';

        if (maintenance.length === 0) {
            tbody.innerHTML = '<tr><td colspan="7" style="text-align: center;">No maintenance records</td></tr>';
            return;
        }

        const typeNames = ['Oil Change', 'Tire Rotation', 'Brake Service', 'Engine Check', 'Transmission Service', 'General Service'];

        maintenance.forEach(m => {
            const row = document.createElement('tr');
            const serviceDate = m.service_date ? new Date(parseInt(m.service_date) / 1000000).toLocaleDateString() : 'N/A';
            const type = typeNames[parseInt(m.type) || 0] || 'Unknown';
            
            row.innerHTML = `
                <td>${serviceDate}</td>
                <td>${m.vehicle_id || 'N/A'}</td>
                <td>${type}</td>
                <td>${parseFloat(m.odometer_reading || 0).toFixed(0)} km</td>
                <td>${m.service_center || 'N/A'}</td>
                <td>$${parseFloat(m.total_cost || 0).toFixed(2)}</td>
                <td>N/A</td>
            `;
            tbody.appendChild(row);
        });
    }

    renderVehiclesGrid(vehicles) {
        const grid = document.getElementById('vehiclesGrid');
        if (!grid) return;

        grid.innerHTML = '';

        if (vehicles.length === 0) {
            grid.innerHTML = '<p style="text-align: center; padding: 20px;">No vehicles found. Add your first vehicle!</p>';
            return;
        }

        vehicles.forEach(vehicle => {
            const card = document.createElement('div');
            card.className = 'vehicle-card';
            card.innerHTML = `
                <div class="vehicle-header">
                    <h4>${vehicle.make || ''} ${vehicle.model || ''}</h4>
                    <span class="badge">${vehicle.license_plate || 'N/A'}</span>
                </div>
                <div class="vehicle-details">
                    <p><strong>Year:</strong> ${vehicle.year || 'N/A'}</p>
                    <p><strong>Odometer:</strong> ${parseFloat(vehicle.current_odometer || 0).toFixed(0)} km</p>
                    <p><strong>Fuel Type:</strong> ${vehicle.fuel_type || 'N/A'}</p>
                </div>
                <div class="vehicle-actions">
                    <button class="btn btn-sm btn-primary" onclick="app.viewVehicleDetails(${vehicle.vehicle_id})">
                        <i class="fas fa-info-circle"></i> Details
                    </button>
                </div>
            `;
            grid.appendChild(card);
        });
    }

    populateVehicleSelects(vehicles) {
        const selects = ['vehicleSelect', 'tripVehicle', 'expenseVehicle', 'incidentVehicle'];
        selects.forEach(selectId => {
            const select = document.getElementById(selectId);
            if (!select) return;

            const currentValue = select.value;
            select.innerHTML = '<option value="">Select Vehicle</option>';
            
            vehicles.forEach(vehicle => {
                const option = document.createElement('option');
                option.value = vehicle.vehicle_id;
                option.textContent = `${vehicle.make || ''} ${vehicle.model || ''} (${vehicle.license_plate || ''})`.trim();
                select.appendChild(option);
            });

            if (currentValue) {
                select.value = currentValue;
            }
        });
    }

    async loadExpenses() {
        if (!window.db) {
            console.error('Database API not available');
            return;
        }

        try {
            const expenses = await window.db.getExpenses(100);
            this.renderExpensesTable(expenses);

            const summary = await window.db.getExpenseSummary();
            if (summary) {
                this.updateExpenseSummary(summary);
            }

            const alerts = await window.db.getBudgetAlerts();
            this.renderBudgetAlerts(alerts);
        } catch (error) {
            console.error('Failed to load expenses:', error);
            this.showToast('Failed to load expenses: ' + error.message, 'error');
        }
    }

    renderExpensesTable(expenses) {
        const tbody = document.querySelector('#expensesTable tbody');
        if (!tbody) return;

        tbody.innerHTML = '';

        if (expenses.length === 0) {
            tbody.innerHTML = '<tr><td colspan="7" style="text-align: center;">No expenses found</td></tr>';
            return;
        }

        const categoryNames = ['Fuel', 'Maintenance', 'Insurance', 'Toll', 'Parking', 'Other'];

        expenses.forEach(expense => {
            const row = document.createElement('tr');
            const expenseDate = expense.expense_date ? new Date(parseInt(expense.expense_date) / 1000000).toLocaleDateString() : 'N/A';
            const category = categoryNames[parseInt(expense.category) || 0] || 'Unknown';
            
            row.innerHTML = `
                <td>${expenseDate}</td>
                <td>${category}</td>
                <td>${expense.vehicle_id || 'N/A'}</td>
                <td>${expense.description || 'N/A'}</td>
                <td>$${parseFloat(expense.amount || 0).toFixed(2)}</td>
                <td>${expense.trip_id || 'N/A'}</td>
                <td>
                    <button class="btn btn-sm btn-primary" onclick="app.viewExpenseDetails(${expense.expense_id})">
                        <i class="fas fa-eye"></i> View
                    </button>
                </td>
            `;
            tbody.appendChild(row);
        });
    }

    viewExpenseDetails(expenseId) {
        this.showToast('Expense details view not yet implemented', 'info');
    }

    updateExpenseSummary(summary) {
        const elements = {
            totalExpenseAmount: '$' + (summary.total_expenses || 0).toFixed(2),
            fuelExpense: '$' + (summary.fuel_expenses || 0).toFixed(2),
            maintenanceExpense: '$' + (summary.maintenance_expenses || 0).toFixed(2),
            insuranceExpense: '$0.00',
            otherExpense: '$0.00'
        };

        Object.entries(elements).forEach(([id, value]) => {
            const el = document.getElementById(id);
            if (el) el.textContent = value;
        });
    }

    renderBudgetAlerts(alerts) {
        const container = document.getElementById('budgetStatus');
        if (!container) return;

        if (alerts.length === 0) {
            container.innerHTML = '<p style="text-align: center; color: green;">All budgets within limits</p>';
            return;
        }

        container.innerHTML = alerts.map(alert => {
            const categoryNames = ['Fuel', 'Maintenance', 'Insurance', 'Toll', 'Parking', 'Other'];
            const category = categoryNames[parseInt(alert.category) || 0] || 'Unknown';
            const isOver = alert.over_budget === '1' || alert.over_budget === true;
            
            return `
                <div class="budget-alert ${isOver ? 'over-budget' : 'warning'}">
                    <strong>${category}</strong>
                    <p>Spent: $${parseFloat(alert.spent || 0).toFixed(2)} / $${parseFloat(alert.limit || 0).toFixed(2)}</p>
                    <p>${parseFloat(alert.percentage_used || 0).toFixed(1)}% used</p>
                </div>
            `;
        }).join('');
    }

    async loadDrivers() {
        if (!window.db) {
            console.error('Database API not available');
            return;
        }

        try {
            const profile = await window.db.getDriverProfile();
            if (profile) {
                this.renderDriverProfile(profile);
            }

            const behavior = await window.db.getDriverBehavior();
            if (behavior) {
                this.updateDriverBehavior(behavior);
            }

            const leaderboard = await window.db.getDriverLeaderboard(10);
            this.renderLeaderboard(leaderboard);

            const recommendations = await window.db.getImprovementRecommendations();
            this.renderRecommendations(recommendations);
        } catch (error) {
            console.error('Failed to load driver data:', error);
            this.showToast('Failed to load driver data: ' + error.message, 'error');
        }
    }

    renderDriverProfile(profile) {
        const container = document.getElementById('driverProfile');
        if (!container) return;

        container.innerHTML = `
            <div class="profile-section">
                <h4>${profile.name || 'N/A'}</h4>
                <p><strong>Email:</strong> ${profile.email || 'N/A'}</p>
                <p><strong>Phone:</strong> ${profile.phone || 'N/A'}</p>
                <p><strong>Safety Score:</strong> ${profile.safety_score || 1000}/1000</p>
                <p><strong>Total Trips:</strong> ${profile.total_trips || 0}</p>
                <p><strong>Total Distance:</strong> ${parseFloat(profile.total_distance || 0).toFixed(1)} km</p>
            </div>
        `;
    }

    updateDriverBehavior(behavior) {
        const elements = {
            behaviorScore: `${behavior.safety_score || 1000}/1000`,
            behaviorDistance: `${parseFloat(behavior.total_distance || 0).toFixed(1)} km`,
            behaviorBraking: `${parseFloat(behavior.harsh_braking_rate || 0).toFixed(1)}/100km`,
            behaviorRank: `#${behavior.rank || 'N/A'}`
        };

        Object.entries(elements).forEach(([id, value]) => {
            const el = document.getElementById(id);
            if (el) el.textContent = value;
        });
    }

    renderLeaderboard(leaderboard) {
        const tbody = document.querySelector('#leaderboardTable tbody');
        if (!tbody) return;

        tbody.innerHTML = '';

        if (leaderboard.length === 0) {
            tbody.innerHTML = '<tr><td colspan="6" style="text-align: center;">No leaderboard data</td></tr>';
            return;
        }

        leaderboard.forEach((driver, index) => {
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${driver.rank || index + 1}</td>
                <td>${driver.driver_name || 'N/A'}</td>
                <td>${driver.safety_score || 0}</td>
                <td>${parseFloat(driver.total_distance || 0).toFixed(1)} km</td>
                <td>0</td>
                <td>0 km/h</td>
            `;
            tbody.appendChild(row);
        });
    }

    renderRecommendations(recommendations) {
        const container = document.getElementById('recommendationsList');
        if (!container) return;

        if (recommendations.length === 0) {
            container.innerHTML = '<p style="text-align: center; color: green;">No recommendations at this time. Keep up the good driving!</p>';
            return;
        }

        container.innerHTML = recommendations.map(rec => `
            <div class="recommendation-item">
                <h5>${rec.category || 'General'}</h5>
                <p>${rec.recommendation || 'N/A'}</p>
                <span class="priority priority-${rec.priority || 1}">Priority: ${rec.priority || 1}</span>
            </div>
        `).join('');
    }

    async loadIncidents() {
        if (!window.db) {
            console.error('Database API not available');
            return;
        }

        try {
            const incidents = await window.db.getIncidents(50);
            this.renderIncidentsTable(incidents);

            const stats = await window.db.getIncidentStatistics();
            if (stats) {
                this.updateIncidentStats(stats);
            }
        } catch (error) {
            console.error('Failed to load incidents:', error);
            this.showToast('Failed to load incidents: ' + error.message, 'error');
        }
    }

    renderIncidentsTable(incidents) {
        const tbody = document.querySelector('#incidentsTable tbody');
        if (!tbody) return;

        tbody.innerHTML = '';

        if (incidents.length === 0) {
            tbody.innerHTML = '<tr><td colspan="7" style="text-align: center;">No incidents reported</td></tr>';
            return;
        }

        const typeNames = ['Accident', 'Breakdown', 'Theft', 'Vandalism', 'Traffic Violation'];
        
        incidents.forEach(incident => {
            const row = document.createElement('tr');
            const incidentTime = incident.incident_time ? new Date(parseInt(incident.incident_time) / 1000000).toLocaleString() : 'N/A';
            const type = typeNames[parseInt(incident.type) || 0] || 'Unknown';
            const isResolved = incident.is_resolved === '1' || incident.is_resolved === true;
            
            row.innerHTML = `
                <td>${incidentTime}</td>
                <td>${type}</td>
                <td>${incident.vehicle_id || 'N/A'}</td>
                <td>${incident.location_address || `${incident.latitude || 0}, ${incident.longitude || 0}`}</td>
                <td>${incident.description || 'N/A'}</td>
                <td><span class="badge ${isResolved ? 'success' : 'warning'}">${isResolved ? 'Resolved' : 'Open'}</span></td>
                <td>
                    <button class="btn btn-sm btn-primary" onclick="app.viewIncidentDetails(${incident.incident_id})">
                        <i class="fas fa-eye"></i> View
                    </button>
                </td>
            `;
            tbody.appendChild(row);
        });
    }

    updateIncidentStats(stats) {
        const elements = {
            totalIncidents: stats.total_incidents || 0,
            totalAccidents: stats.total_accidents || 0,
            totalBreakdowns: stats.total_breakdowns || 0,
            totalViolations: '0',
            incidentFree: stats.incident_free_days || 0
        };

        Object.entries(elements).forEach(([id, value]) => {
            const el = document.getElementById(id);
            if (el) el.textContent = value;
        });
    }

    async loadReports() {
        if (window.analytics) {
            window.analytics.initCharts();
        }
    }

    formatDuration(seconds) {
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        return `${hours}h ${minutes}m`;
    }

    viewTripDetails(tripId) {
        this.showToast('Trip details view not yet implemented', 'info');
    }

    viewVehicleDetails(vehicleId) {
        this.showToast('Vehicle details view not yet implemented', 'info');
    }

    viewIncidentDetails(incidentId) {
        this.showToast('Incident details view not yet implemented', 'info');
    }

    initDashboard() {
        // Initialize speedometer with current speed (or 0)
        const initialSpeed = this.liveData.speed || 0;
        this.drawSpeedometer(initialSpeed);
        console.log('📊 Speedometer initialized with speed:', initialSpeed, 'km/h');

        this.updateDashboard();
        this.updateTripControls();

        // Update camera button based on current status
        this.updateCameraStatus(this.isCameraActive);

        // Load dashboard statistics
        this.loadDashboardStats();

        // Check GPS status
        if (!this.gpsManager) {
            console.warn('⚠️ GPS Manager not initialized');
            this.showToast('GPS not available. Speed and location tracking may be limited.', 'warning');
        } else if (!navigator.geolocation) {
            console.warn('⚠️ Browser does not support geolocation');
            this.showToast('Your browser does not support GPS tracking', 'warning');
        } else {
            // Ensure GPS is tracking
            if (!this.gpsManager.watchId && !this.gpsManager.pollInterval) {
                console.log('🔄 Restarting GPS tracking...');
                this.gpsManager.startTracking();
            }
        }
        
        // Set up periodic dashboard refresh to ensure UI stays updated
        // This helps when GPS updates are slow
        if (this.dashboardRefreshInterval) {
            clearInterval(this.dashboardRefreshInterval);
        }
        this.dashboardRefreshInterval = setInterval(() => {
            // Refresh dashboard every 500ms to ensure smooth updates
            if (this.currentPage === 'dashboard') {
                this.updateDashboard();
            }
        }, 500);
    }

    async loadDashboardStats() {
        if (!window.db) return;

        try {
            const stats = await window.db.getTripStatistics();
            if (stats) {
                const elements = {
                    totalTrips: stats.total_trips || 0,
                    totalDistance: (stats.total_distance || 0).toFixed(1) + ' km',
                    fuelEfficiency: '12.5',
                    totalVehicles: '0',
                    incidentFreeDays: '0'
                };

                Object.entries(elements).forEach(([id, value]) => {
                    const el = document.getElementById(id);
                    if (el) el.textContent = value;
                });
            }

            const vehicles = await window.db.getVehicles();
            const vehiclesEl = document.getElementById('totalVehicles');
            if (vehiclesEl) {
                vehiclesEl.textContent = vehicles.length;
            }

            const expenseSummary = await window.db.getExpenseSummary();
            const totalExpensesEl = document.getElementById('totalExpenses');
            if (totalExpensesEl && expenseSummary) {
                totalExpensesEl.textContent = '$' + (expenseSummary.total_expenses || 0).toFixed(2);
            }

            const incidentStats = await window.db.getIncidentStatistics();
            const incidentFreeEl = document.getElementById('incidentFreeDays');
            if (incidentFreeEl && incidentStats) {
                incidentFreeEl.textContent = incidentStats.incident_free_days || 0;
            }
        } catch (error) {
            console.error('Failed to load dashboard stats:', error);
        }
    }


    // EVENT LISTENERS
    setupEventListeners() {
        // Use event delegation for navigation (works with dynamically loaded content)
        const sidebar = document.querySelector('.sidebar');
        if (sidebar) {
            sidebar.addEventListener('click', (e) => {
                const navItem = e.target.closest('.nav-item');
                if (navItem && navItem.dataset.page) {
                    e.preventDefault();
                    this.loadPage(navItem.dataset.page);
                }
            });
        }

        // Logout button
        document.addEventListener('click', (e) => {
            if (e.target.closest('.btn-logout')) {
                e.preventDefault();
                this.logout();
            }
        });

        // Menu toggle
        const menuToggle = document.getElementById('menuToggle');
        if (menuToggle) {
            menuToggle.addEventListener('click', () => {
                document.body.classList.toggle('sidebar-collapsed');
            });
        }

        // Re-attach listeners after page load
        this.attachPageListeners();
    }

    attachPageListeners() {
        // Dashboard buttons
        const startTripBtn = document.getElementById('startTripBtn');
        if (startTripBtn) {
            startTripBtn.onclick = () => this.startTrip();
        }

        const stopTripBtn = document.getElementById('stopTripBtn');
        if (stopTripBtn) {
            stopTripBtn.onclick = () => this.stopTrip();
        }

        const toggleCameraBtn = document.getElementById('toggleCameraBtn');
        if (toggleCameraBtn) {
            toggleCameraBtn.onclick = () => this.toggleCamera();
        }

        // Camera toggle buttons (multiple instances)
        const cameraToggleBtns = document.querySelectorAll('#cameraToggleBtn, #visionCameraBtn');
        cameraToggleBtns.forEach(btn => {
            btn.onclick = () => this.toggleCamera();
        });
    }

    updateUserUI() {
        const userName = document.getElementById('userName');
        const userAvatar = document.getElementById('userAvatar');

        if (userName) userName.textContent = this.userData.name || 'User';
        if (userAvatar) {
            const initials = (this.userData.name || 'U').split(' ').map(n => n[0]).join('');
            userAvatar.textContent = initials;
        }
    }

    // UTILITIES
    showToast(message, type = 'info') {
        const container = document.getElementById('toastContainer') || this.createToastContainer();

        const toast = document.createElement('div');
        toast.className = `toast ${type}`;

        const icons = {
            success: 'fa-check-circle',
            error: 'fa-exclamation-circle',
            warning: 'fa-exclamation-triangle',
            info: 'fa-info-circle'
        };

        toast.innerHTML = `
            <i class="fas ${icons[type] || icons.info}"></i>
            <span>${message}</span>
        `;

        container.appendChild(toast);

        setTimeout(() => toast.remove(), 5000);
    }

    createToastContainer() {
        const container = document.createElement('div');
        container.id = 'toastContainer';
        container.className = 'toast-container';
        document.body.appendChild(container);
        return container;
    }

    playAlertSound() {
        try {
            const ctx = new (window.AudioContext || window.webkitAudioContext)();
            const osc = ctx.createOscillator();
            const gain = ctx.createGain();

            osc.connect(gain);
            gain.connect(ctx.destination);

            osc.frequency.value = 800;
            osc.type = 'sine';
            gain.gain.value = 0.3;

            osc.start();
            setTimeout(() => osc.stop(), 200);
        } catch (e) {
            console.log('Audio not available');
        }
    }

    logout() {
        if (this.ws) this.ws.close();
        localStorage.clear();
        window.location.href = 'login.html';
    }

    showLoading() {
        const spinner = document.getElementById('loadingSpinner');
        if (spinner) {
            spinner.style.display = 'flex';
        }
    }

    hideLoading() {
        const spinner = document.getElementById('loadingSpinner');
        if (spinner) {
            spinner.style.display = 'none';
        }
    }

    showModal(modalId) {
        const modal = document.getElementById(modalId);
        if (modal) {
            modal.classList.add('active');
            modal.style.display = 'flex';
        }
    }

    closeModal(modalId) {
        const modal = document.getElementById(modalId);
        if (modal) {
            modal.classList.remove('active');
            modal.style.display = 'none';
        }
    }

    showAlert(message, type = 'warning') {
        this.showToast(message, type);
        if (type === 'warning' || type === 'error') {
            this.playAlertSound();
        }
    }
}

// Note: App initialization is handled by main.js
// Global functions are set up by main.js after app initialization