// gps_udp.js - GPS Manager using UDP data from device (NO browser GPS)
class GPSManager {
    constructor(app) {
        this.app = app;
        this.lastPosition = null;
        this.lastTime = null;
        this.lastSpeed = 0; // m/s from device
        
        // Device data tracking
        this.deviceConnected = false;
        this.lastDataTime = null;
        this.dataTimeout = 5000; // 5 seconds without data = disconnected
        
        // Safety event tracking (from device)
        this.eventCounts = {
            hardBrake: 0,
            rapidAccel: 0,
            crash: 0,
            impact: 0
        };
        
        console.log('🚗 GPS Manager initialized (UDP mode - waiting for device data)');
    }
    
    // Called when UDP data arrives via WebSocket
    processUDPData(data) {
        if (!data) {
            console.warn('⚠️ Invalid UDP data received');
            return;
        }
        
        this.deviceConnected = true;
        this.lastDataTime = Date.now();
        
        const {
            latitude,
            longitude,
            speed,           // km/h from device
            gps_speed,       // km/h backup
            acceleration,    // m/s² (accel_y from device)
            accel_x,
            accel_y,
            accel_z,
            gyro_x,
            gyro_y,
            gyro_z,
            timestamp
        } = data;
        
        // Validate data
        if (!latitude || !longitude || isNaN(latitude) || isNaN(longitude)) {
            console.warn('⚠️ Invalid GPS coordinates from device:', { latitude, longitude });
            return;
        }
        
        const currentTime = Date.now();
        
        // Use device-calculated speed (already in km/h)
        const currentSpeed = speed || gps_speed || 0;
        const speedMs = currentSpeed / 3.6; // Convert to m/s
        
        // Log data every 2 seconds
        if (!this.lastDataTime || currentTime - this.lastDataTime > 2000) {
            console.log('📊 Device Data:', {
                lat: latitude.toFixed(6),
                lon: longitude.toFixed(6),
                speed: currentSpeed.toFixed(1) + ' km/h',
                accel: acceleration ? acceleration.toFixed(2) + ' m/s²' : 'N/A',
                timestamp: new Date(timestamp || currentTime).toLocaleTimeString()
            });
        }
        
        // Update app's live data
        if (this.app && this.app.updateLiveData) {
            this.app.updateLiveData({
                speed: currentSpeed,
                acceleration: acceleration || 0,
                latitude: latitude,
                longitude: longitude,
                gps_speed: gps_speed || currentSpeed,
                accel_x: accel_x || 0,
                accel_y: accel_y || 0,
                accel_z: accel_z || 0,
                gyro_x: gyro_x || 0,
                gyro_y: gyro_y || 0,
                gyro_z: gyro_z || 0
            });
        }
        
        // Update location UI immediately
        if (this.app && this.app.updateLocationUI) {
            this.app.updateLocationUI(latitude, longitude, 10); // Assume 10m accuracy from device
        }
        
        // Record position in trip tracker if trip is active
        if (this.app && this.app.tripTracker && this.app.tripTracker.isActive()) {
            this.app.tripTracker.recordPosition(latitude, longitude, currentSpeed);
        }
        
        // Store for next iteration
        this.lastPosition = { lat: latitude, lon: longitude };
        this.lastTime = currentTime;
        this.lastSpeed = speedMs;
    }
    
    // Handle safety events from device
    handleDeviceEvent(event) {
        if (!event || !event.warning_type) {
            console.warn('⚠️ Invalid event data:', event);
            return;
        }
        
        const { warning_type, value, latitude, longitude, timestamp } = event;
        
        console.log(`🚨 DEVICE EVENT: ${warning_type}`, {
            value: value?.toFixed(2),
            location: `${latitude?.toFixed(6)}, ${longitude?.toFixed(6)}`
        });
        
        // Update event counters
        switch(warning_type) {
            case 'HARD_BRAKE':
                this.eventCounts.hardBrake++;
                if (this.app && this.app.liveData) {
                    this.app.liveData.hard_brake_count = this.eventCounts.hardBrake;
                }
                this.showAlert('⚠️ Hard Braking Detected!', 'warning');
                break;
                
            case 'RAPID_ACCEL':
                this.eventCounts.rapidAccel++;
                if (this.app && this.app.liveData) {
                    this.app.liveData.rapid_accel_count = this.eventCounts.rapidAccel;
                }
                this.showAlert('⚠️ Rapid Acceleration!', 'warning');
                break;
                
            case 'CRASH':
            case 'IMPACT':
                this.eventCounts.crash++;
                this.showAlert('🚨 CRASH DETECTED!', 'error');
                this.triggerCrashSequence(latitude, longitude, value);
                break;
        }
        
        // Update dashboard
        if (this.app && this.app.updateDashboard) {
            this.app.updateDashboard();
        }
        
        // Play alert sound
        if (this.app && this.app.playAlertSound) {
            this.app.playAlertSound();
        }
    }
    
    showAlert(message, type) {
        if (this.app && this.app.showToast) {
            this.app.showToast(message, type);
        }
    }
    
    triggerCrashSequence(lat, lon, severity) {
        console.log('🚨 Initiating crash response sequence');
        
        // Show SOS modal
        const modal = document.getElementById('sosModal');
        if (modal) {
            modal.style.display = 'flex';
            
            // Auto-send SOS after 10 seconds
            let countdown = 10;
            const countdownEl = document.getElementById('sosCountdown');
            
            const timer = setInterval(() => {
                countdown--;
                if (countdownEl) countdownEl.textContent = countdown;
                
                if (countdown <= 0) {
                    clearInterval(timer);
                    this.sendSOS(lat, lon, severity);
                }
            }, 1000);
            
            // Store timer so it can be cancelled
            modal.dataset.sosTimer = timer;
        }
    }
    
    async sendSOS(lat, lon, severity) {
        console.log('📞 Sending SOS with location:', lat, lon);
        
        if (this.app && this.app.showToast) {
            this.app.showToast('🚨 Sending Emergency SOS...', 'error');
        }
        
        try {
            // Report incident to database
            if (window.db) {
                const vehicleId = this.app?.userData?.vehicle_id || 0;
                await window.db.reportIncident(
                    vehicleId,
                    0, // Accident type
                    lat,
                    lon,
                    `Automatic crash detection - Severity: ${severity?.toFixed(2) || 'Unknown'}`
                );
            }
            
            // Close modal
            const modal = document.getElementById('sosModal');
            if (modal) {
                modal.style.display = 'none';
            }
            
            if (this.app && this.app.showToast) {
                this.app.showToast('✅ Emergency services notified', 'success');
            }
            
        } catch (error) {
            console.error('❌ Failed to send SOS:', error);
            if (this.app && this.app.showToast) {
                this.app.showToast('❌ Failed to send SOS: ' + error.message, 'error');
            }
        }
    }
    
    cancelCrashSequence() {
        const modal = document.getElementById('sosModal');
        if (modal) {
            // Clear timer
            if (modal.dataset.sosTimer) {
                clearInterval(parseInt(modal.dataset.sosTimer));
            }
            modal.style.display = 'none';
        }
        
        if (this.app && this.app.showToast) {
            this.app.showToast('SOS Cancelled', 'info');
        }
    }
    
    // Check device connection status
    checkConnectionStatus() {
        if (!this.lastDataTime) {
            this.deviceConnected = false;
            return false;
        }
        
        const timeSinceData = Date.now() - this.lastDataTime;
        
        if (timeSinceData > this.dataTimeout) {
            if (this.deviceConnected) {
                console.warn('⚠️ Device disconnected - no data for ' + (timeSinceData / 1000) + 's');
                this.deviceConnected = false;
                
                if (this.app && this.app.showToast) {
                    this.app.showToast('⚠️ Device disconnected. Check your device connection.', 'warning');
                }
            }
            return false;
        }
        
        return true;
    }
    
    // Get connection status for UI
    getStatus() {
        return {
            connected: this.deviceConnected,
            lastUpdate: this.lastDataTime,
            timeSinceUpdate: this.lastDataTime ? Date.now() - this.lastDataTime : null,
            location: this.lastPosition,
            speed: this.lastSpeed * 3.6, // Convert to km/h
            eventCounts: this.eventCounts
        };
    }
    
    // No startTracking/stopTracking needed - device handles everything
    startTracking() {
        console.log('📱 Waiting for device data via UDP...');
        
        // Start connection check interval
        this.connectionCheckInterval = setInterval(() => {
            this.checkConnectionStatus();
        }, 2000);
        
        return true;
    }
    
    stopTracking() {
        console.log('🛑 Stopping GPS tracking');
        
        if (this.connectionCheckInterval) {
            clearInterval(this.connectionCheckInterval);
            this.connectionCheckInterval = null;
        }
        
        this.deviceConnected = false;
    }
}

// Make globally accessible
console.log('✅ GPS Manager (UDP Mode) loaded');