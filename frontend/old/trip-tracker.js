// trip-tracker.js - AGENT-3: Trip Distance & Duration Tracking
// Tracks active trip: distance accumulation, duration counter, waypoints

class TripTracker {
    constructor(app) {
        this.app = app;
        this.activeTrip = null;
        this.tripStartTime = null;
        this.tripStartPosition = null;
        this.tripDistance = 0;
        this.timerInterval = null;
        this.recordingInterval = null;
    }

    // Start trip tracking (called when trip starts)
    startTrip(tripId, startLat, startLon) {
        console.log('🚗 AGENT-3: Trip tracking started:', tripId);
        
        this.activeTrip = {
            id: tripId,
            startTime: Date.now(),
            startLat: startLat,
            startLon: startLon,
            endTime: null,
            totalDistance: 0,
            waypoints: [{lat: startLat, lon: startLon, time: Date.now()}],
            speeds: [],
            maxSpeed: 0,
            avgSpeed: 0
        };

        this.tripStartTime = Date.now();
        this.tripStartPosition = {lat: startLat, lon: startLon};
        this.tripDistance = 0;

        // Start timer for duration
        this.startDurationTimer();

        // Start recording waypoints
        this.startWaypointRecording();

        console.log('✅ Trip tracker initialized');
        return true;
    }

    // Update trip with new GPS position
    recordPosition(latitude, longitude, speed) {
        if (!this.activeTrip) {
            console.warn('⚠️ recordPosition called but no active trip');
            return;
        }

        const currentTime = Date.now();
        const lastWaypoint = this.activeTrip.waypoints[this.activeTrip.waypoints.length - 1];

        // Calculate distance from last waypoint using Haversine formula (returns km)
        const distance = this.calculateDistance(
            lastWaypoint.lat, lastWaypoint.lon,
            latitude, longitude
        );

        // AGENT-3 FIX: Lowered threshold from 5m (0.005km) to 1m (0.001km) for better precision
        // Only record if moved more than 1 meter (avoid noise but capture more points)
        if (distance > 0.001) {  // 1 meter threshold
            this.activeTrip.waypoints.push({
                lat: latitude,
                lon: longitude,
                time: currentTime,
                speed: speed
            });

            // Accumulate distance in kilometers
            this.activeTrip.totalDistance += distance;
            this.tripDistance = this.activeTrip.totalDistance;
            
            console.log(`📍 Distance recorded: ${distance.toFixed(6)} km, Total: ${this.activeTrip.totalDistance.toFixed(4)} km, Speed: ${speed.toFixed(1)} km/h`);

            // Track speed
            if (speed > 0) {
                this.activeTrip.speeds.push(speed);
                if (speed > this.activeTrip.maxSpeed) {
                    this.activeTrip.maxSpeed = speed;
                }
            }

            // Update UI immediately
            this.updateTripUI();
        } else if (distance > 0) {
            console.log(`⚠️ Movement too small (${(distance * 1000).toFixed(1)}m), skipping...`);
        }
    }

    // Timer for duration (updates every second)
    startDurationTimer() {
        console.log('⏱️ Starting duration timer for trip:', this.activeTrip?.id);
        
        this.timerInterval = setInterval(() => {
            if (!this.activeTrip) {
                console.warn('⚠️ Duration timer running but activeTrip is null!');
                return;
            }

            const duration = Date.now() - this.activeTrip.startTime;
            const seconds = Math.floor(duration / 1000);
            console.log(`⏱️ Trip duration: ${seconds}s (${(seconds/60).toFixed(1)} min)`);
            
            this.updateDurationUI(duration);
        }, 1000);  // Update every second
    }

    // Record waypoints to backend periodically
    startWaypointRecording() {
        this.recordingInterval = setInterval(() => {
            if (!this.activeTrip || this.activeTrip.waypoints.length < 2) return;

            // Send last 10 waypoints to backend for saving
            const waypointsToSend = this.activeTrip.waypoints.slice(-10);
            this.syncWaypointsToBackend(this.activeTrip.id, waypointsToSend);
        }, 30000);  // Every 30 seconds
    }

    // End trip and save data
    async stopTrip() {
        if (!this.activeTrip) {
            console.warn('⚠️ stopTrip called but no active trip');
            return false;
        }

        const tripId = this.activeTrip.id;
        console.log('🛑 Trip tracking ended:', tripId);

        this.activeTrip.endTime = Date.now();

        // Calculate statistics
        const duration = this.activeTrip.endTime - this.activeTrip.startTime;
        const distance = this.activeTrip.totalDistance;
        const avgSpeed = this.activeTrip.speeds.length > 0
            ? (this.activeTrip.speeds.reduce((a, b) => a + b, 0) / this.activeTrip.speeds.length)
            : 0;

        // Stop timers immediately
        clearInterval(this.timerInterval);
        clearInterval(this.recordingInterval);
        this.timerInterval = null;
        this.recordingInterval = null;

        // Store trip data before clearing (for summary)
        const tripData = {
            trip_id: tripId,
            duration: duration,
            distance: distance,
            waypoints: this.activeTrip.waypoints,
            max_speed: this.activeTrip.maxSpeed,
            avg_speed: avgSpeed,
            total_events: 0  // TODO: Count safety events
        };

        // Store in app for summary
        if (this.app) {
            this.app.lastTripData = {
                distance: distance,
                duration: duration,
                avgSpeed: avgSpeed,
                maxSpeed: this.activeTrip.maxSpeed,
                waypoints: this.activeTrip.waypoints
            };
        }

        // Clear active trip state (but keep tripStartPosition for fallback)
        const savedStartPosition = this.tripStartPosition;
        this.activeTrip = null;
        this.tripStartTime = null;
        this.tripDistance = 0;

        // Save complete trip to backend (await to ensure it completes)
        try {
            await this.saveTripToBackend(tripData);
            console.log('✅ Trip data saved successfully');
            return true;
        } catch (error) {
            console.error('❌ Failed to save trip data:', error);
            // Restore trip state if save failed (optional - might want to keep it cleared)
            // For now, we'll just log the error and return false
            return false;
        }
    }

    // Calculate distance between two points (Haversine formula)
    calculateDistance(lat1, lon1, lat2, lon2) {
        if (lat1 === lat2 && lon1 === lon2) return 0;

        const R = 6371;  // Earth radius in km
        const φ1 = lat1 * Math.PI / 180;
        const φ2 = lat2 * Math.PI / 180;
        const Δφ = (lat2 - lat1) * Math.PI / 180;
        const Δλ = (lon2 - lon1) * Math.PI / 180;

        const a = Math.sin(Δφ / 2) * Math.sin(Δφ / 2) +
                  Math.cos(φ1) * Math.cos(φ2) *
                  Math.sin(Δλ / 2) * Math.sin(Δλ / 2);
        const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));

        return R * c;  // Return in km
    }

    // Update UI with distance
    updateTripUI() {
        const distanceEl = document.getElementById('tripDistance');
        const speedEl = document.getElementById('currentSpeed');

        if (distanceEl && this.tripDistance) {
            const distanceKm = this.tripDistance.toFixed(2);
            distanceEl.textContent = distanceKm + ' km';
        }

        if (speedEl && this.app) {
            const speed = this.app.liveData.speed || 0;
            speedEl.textContent = speed.toFixed(1) + ' km/h';
        }
    }

    // Update UI with duration (every second)
    updateDurationUI(durationMs) {
        const durationEl = document.getElementById('tripDuration');
        
        if (!durationEl) {
            console.error('❌ tripDuration element not found in DOM!');
            return;
        }

        const totalSeconds = Math.floor(durationMs / 1000);
        const hours = Math.floor(totalSeconds / 3600);
        const minutes = Math.floor((totalSeconds % 3600) / 60);
        const seconds = totalSeconds % 60;

        const timeString = `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
        durationEl.textContent = timeString;
        
        // Debug logging every 5 seconds
        if (totalSeconds % 5 === 0) {
            console.log(`📊 Trip duration updated: ${timeString}`);
        }
    }

    // Sync waypoints to backend
    async syncWaypointsToBackend(tripId, waypoints) {
        try {
            if (!window.db) {
                console.warn('⚠️ DatabaseAPI not initialized');
                return;
            }

            // AGENT-3 FIX: Use DatabaseAPI instead of raw fetch
            // Send latest waypoint with GPS data
            if (waypoints.length > 0) {
                const lastWaypoint = waypoints[waypoints.length - 1];
                const result = await window.db.logGPSPoint(
                    tripId,
                    lastWaypoint.lat,
                    lastWaypoint.lon,
                    lastWaypoint.speed || 0
                );
                
                if (result) {
                    console.log('✅ Waypoint synced to backend');
                }
            }
        } catch (error) {
            console.error('⚠️ Failed to sync waypoints:', error);
            // Don't retry here - will be saved when trip ends
        }
    }

    // Save complete trip to backend
    async saveTripToBackend(tripData) {
        if (!window.db) {
            const error = new Error('Database API not available');
            console.error('❌ DatabaseAPI not initialized - cannot save trip');
            if (this.app && this.app.showToast) {
                this.app.showToast('Failed to save trip: Database API not available', 'error');
            }
            throw error;
        }

        // Validate trip data
        if (!tripData || !tripData.trip_id) {
            const error = new Error('Invalid trip data: missing trip_id');
            console.error('❌', error.message);
            throw error;
        }

        // Get end coordinates from waypoints, or fallback to start position
        let endLat, endLon;
        if (tripData.waypoints && Array.isArray(tripData.waypoints) && tripData.waypoints.length > 0) {
            const lastWaypoint = tripData.waypoints[tripData.waypoints.length - 1];
            endLat = lastWaypoint.lat;
            endLon = lastWaypoint.lon;
        } else if (this.tripStartPosition) {
            // Fallback to start position if no waypoints recorded
            console.warn('⚠️ No waypoints recorded, using start position as end position');
            endLat = this.tripStartPosition.lat;
            endLon = this.tripStartPosition.lon;
        } else if (this.app && this.app.liveData) {
            // Fallback to current GPS position
            endLat = this.app.liveData.latitude;
            endLon = this.app.liveData.longitude;
        } else {
            const error = new Error('No valid coordinates available for trip end');
            console.error('❌', error.message);
            throw error;
        }

        // Validate coordinates
        if (!endLat || !endLon || isNaN(endLat) || isNaN(endLon)) {
            const error = new Error('Invalid coordinates for trip end');
            console.error('❌', error.message, { endLat, endLon });
            throw error;
        }

        try {
            // Check if we have a session (required for API calls)
            if (this.app && !this.app.sessionId) {
                throw new Error('No active session. Please login again.');
            }

            console.log(`💾 Saving trip ${tripData.trip_id} to backend with coordinates:`, { 
                endLat, 
                endLon,
                trip_id: tripData.trip_id,
                hasSession: !!(this.app && this.app.sessionId)
            });
            
            // AGENT-3 FIX: Use proper DatabaseAPI method instead of raw fetch
            const result = await window.db.endTrip(
                tripData.trip_id,
                endLat,
                endLon,
                ''  // address (optional)
            );

            if (result) {
                console.log('✅ Trip saved to backend successfully');
                if (this.app && this.app.showToast) {
                    const distance = tripData.distance ? tripData.distance.toFixed(2) : '0.00';
                    this.app.showToast(`✅ Trip saved! Distance: ${distance} km`, 'success');
                }
            } else {
                throw new Error('Backend returned false - trip may not exist in database or end operation failed');
            }
        } catch (error) {
            console.error('❌ Failed to save trip:', error);
            console.error('Trip data:', {
                trip_id: tripData.trip_id,
                distance: tripData.distance,
                waypoints_count: tripData.waypoints ? tripData.waypoints.length : 0,
                hasSession: !!(this.app && this.app.sessionId),
                errorMessage: error.message,
                errorName: error.name
            });
            
            // Provide more helpful error messages
            let userMessage = error.message;
            if (error.message.includes('trip may not exist')) {
                userMessage = 'Trip not found in database. It may not have been properly started.';
            } else if (error.message.includes('session') || error.message.includes('login')) {
                userMessage = 'Session expired. Please login again and try stopping the trip.';
            } else if (error.message.includes('connect') || error.message.includes('network')) {
                userMessage = 'Cannot connect to backend server. Please check your connection.';
            }
            
            if (this.app && this.app.showToast) {
                this.app.showToast('Failed to save trip data: ' + userMessage, 'error');
            }
            // Re-throw to allow caller to handle
            throw error;
        }
    }

    // Get active trip info
    getActiveTripInfo() {
        if (!this.activeTrip) return null;

        return {
            tripId: this.activeTrip.id,
            duration: Date.now() - this.activeTrip.startTime,
            distance: this.activeTrip.totalDistance,
            currentSpeed: this.app ? this.app.liveData.speed : 0,
            waypoints: this.activeTrip.waypoints,
            maxSpeed: this.activeTrip.maxSpeed
        };
    }

    // Check if trip is active
    isActive() {
        return this.activeTrip !== null;
    }
}

console.log('✅ TripTracker class loaded - AGENT-3 ready');
