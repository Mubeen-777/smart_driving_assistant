// database.js - Database API Integration (Fixed for operation-based API)
class DatabaseAPI {
    constructor(app) {
        this.app = app;
        // FIX (AGENT-1): Use environment variable or full URL
        // Development: http://localhost:8080/api
        // Production: https://api.yourdomain.com/api
        this.API_BASE = window.__API_BASE__ || 'http://localhost:8080/api';
    }

    async sendRequest(operation, data = {}) {
        try {
            if (!this.app.sessionId && operation !== 'user_login' && operation !== 'user_register') {
                throw new Error('No session. Please login.');
            }

            if (operation !== 'user_login' && operation !== 'user_register') {
                data.session_id = this.app.sessionId;
            }

            data.operation = operation;

            const response = await fetch(this.API_BASE, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(data),
                signal: AbortSignal.timeout(10000) // 10 second timeout
            });

            if (!response.ok) {
                let errorText = '';
                try {
                    errorText = await response.text();
                } catch (e) {
                    errorText = response.statusText;
                }
                console.error('HTTP Error Response:', response.status, errorText);

                // Try to parse as JSON for better error messages
                try {
                    const errorJson = JSON.parse(errorText);
                    if (errorJson.message) {
                        throw new Error(errorJson.message);
                    }
                } catch (e) {
                    // Not JSON, use status text
                }

                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            const result = await response.json();

            if (result.status === 'error') {
                throw new Error(result.message || result.code || 'Unknown error occurred');
            }

            return result;
        } catch (error) {
            console.error('API Request failed:', error);
            console.error('Operation:', operation);
            console.error('Data:', data);

            // Handle network errors
            if (error.message.includes('Failed to fetch') || error.name === 'TypeError' || error.name === 'AbortError') {
                const networkError = new Error('Cannot connect to backend server. Make sure it is running on port 8080.');
                networkError.name = 'NetworkError';

                if (this.app && this.app.showToast) {
                    this.app.showToast('Backend server not available. Please check if the C++ server is running.', 'error');
                }

                throw networkError;
            }

            if (error.message.includes('UNAUTHORIZED') || error.message.includes('Invalid session')) {
                if (this.app && this.app.showToast) {
                    this.app.showToast('Session expired. Please login again.', 'error');
                }
                setTimeout(() => {
                    if (this.app && this.app.logout) {
                        this.app.logout();
                    }
                }, 2000);
            }

            throw error;
        }
    }

    // Authentication
    async login(username, password) {
        const response = await this.sendRequest('user_login', {
            username: username,
            password: password
        });

        if (response.status === 'success' && response.data) {
            this.app.sessionId = response.data.session_id;
            this.app.userData = {
                driver_id: parseInt(response.data.driver_id) || 0,
                name: response.data.name || username,
                role: response.data.role || '0',
                username: username
            };

            localStorage.setItem('session_id', this.app.sessionId);
            localStorage.setItem('user_data', JSON.stringify(this.app.userData));

            if (this.app.showToast) {
                this.app.showToast('Login successful!', 'success');
            }

            return {
                session_id: response.data.session_id,
                driver_id: parseInt(response.data.driver_id) || 0,
                name: response.data.name || username,
                role: response.data.role || '0'
            };
        }

        throw new Error('Login failed');
    }

    async changePassword(oldPassword, newPassword) {
        const response = await this.sendRequest('user_change_password', {
            old_password: oldPassword,
            new_password: newPassword
        });

        return response.status === 'success';
    }

    async register(username, password, fullName, email, phone) {
        const response = await this.sendRequest('user_register', {
            username: username,
            password: password,
            full_name: fullName,
            email: email || '',
            phone: phone || ''
        });

        return response.status === 'success';
    }

    async logout() {
        try {
            await this.sendRequest('user_logout', {
                session_id: this.app.sessionId
            });
        } catch (error) {
            console.error('Logout error:', error);
        } finally {
            this.app.sessionId = null;
            this.app.userData = null;
            localStorage.removeItem('session_id');
            localStorage.removeItem('user_data');
        }
    }
    async startTrip(vehicleId, startLat, startLon, address = '') {
        // Validate inputs first
        if (!vehicleId || vehicleId === 0) {
            throw new Error('Invalid vehicle_id');
        }
        if (isNaN(startLat) || isNaN(startLon)) {
            throw new Error('Invalid starting coordinates');
        }

        console.log('📤 Starting trip:', {
            vehicle_id: vehicleId,
            latitude: startLat,
            longitude: startLon,
            address: address || '(empty)'
        });

        const response = await this.sendRequest('trip_start', {
            vehicle_id: vehicleId.toString(),
            latitude: startLat.toString(),
            longitude: startLon.toString(),
            address: address
        });

        if (response.status === 'success' && response.data) {
            const backendTripId = parseInt(response.data.trip_id);

            if (!backendTripId || backendTripId === 0) {
                throw new Error('Backend returned invalid trip_id');
            }

            console.log('✅ Trip started with backend ID:', backendTripId);

            return {
                trip_id: backendTripId,  // ✅ Use backend-assigned ID
                start_time: response.data.start_time || new Date().toISOString()
            };
        }

        throw new Error('Failed to start trip: ' + (response.message || 'Unknown error'));
    }
    async endTrip(tripId, endLat, endLon, address = '') {
        try {
            // ✅ FIX 1: Validate inputs BEFORE sending
            if (!tripId || tripId === 0 || isNaN(tripId)) {
                console.error('❌ Invalid trip_id:', tripId);
                throw new Error('Invalid trip_id: trip ID must be a positive number');
            }

            if (isNaN(endLat) || isNaN(endLon)) {
                console.error('❌ Invalid coordinates:', { endLat, endLon });
                throw new Error('Invalid coordinates: latitude and longitude must be valid numbers');
            }

            // ✅ FIX 2: Check if trip exists before trying to end it
            console.log('🔍 Checking if trip exists:', tripId);
            const tripExists = await this.checkTripExists(tripId);
            if (!tripExists) {
                console.error('❌ Trip does not exist in database:', tripId);
                throw new Error(`Trip ${tripId} not found in database`);
            }

            console.log('📤 Sending trip_end request:', {
                trip_id: tripId,
                latitude: endLat,
                longitude: endLon,
                address: address || '(empty)'
            });

            const response = await this.sendRequest('trip_end', {
                trip_id: tripId.toString(),
                latitude: endLat.toString(),
                longitude: endLon.toString(),
                address: address || ''
            });

            if (response.status === 'success') {
                console.log('✅ Trip ended successfully:', tripId);
                return true;
            } else {
                console.error('❌ Trip end returned non-success:', response);
                throw new Error(response.message || response.code || 'Trip end failed');
            }
        } catch (error) {
            console.error('❌ endTrip error:', error);
            console.error('Failed trip_id:', tripId);
            console.error('Coordinates:', { endLat, endLon });

            // ✅ FIX 3: Better error messages
            if (error.message.includes('not found')) {
                throw new Error(`Trip ${tripId} was not found. It may have already been stopped or never started.`);
            } else if (error.message.includes('Invalid')) {
                throw error; // Pass through validation errors
            } else if (error.name === 'NetworkError') {
                throw new Error('Cannot connect to backend. Ensure C++ server is running on port 8080.');
            } else {
                throw new Error(`Failed to end trip: ${error.message}`);
            }
        }
    }
    async logGPSPoint(tripId, latitude, longitude, speed) {
        // Validate trip is active
        if (!tripId || tripId === 0) {
            console.warn('⚠️ Cannot log GPS: No active trip');
            return false;
        }

        // Validate coordinates
        if (isNaN(latitude) || isNaN(longitude) || isNaN(speed)) {
            console.warn('⚠️ Invalid GPS data:', { latitude, longitude, speed });
            return false;
        }

        try {
            const response = await this.sendRequest('trip_log_gps', {
                trip_id: tripId.toString(),
                latitude: latitude.toString(),
                longitude: longitude.toString(),
                speed: speed.toString()
            });

            return response.status === 'success';
        } catch (error) {
            console.error('❌ Failed to log GPS point:', error);
            return false;
        }
    }

    async getTripHistory(limit = 20) {
        const response = await this.sendRequest('trip_get_history', {
            limit: limit.toString()
        });

        if (response.status === 'success') {
            if (response.data && response.data.trips) {
                return response.data.trips;
            }
            if (response.data && Array.isArray(response.data)) {
                return response.data;
            }
            return [];
        }

        return [];
    }

    async getTripStatistics() {
        const response = await this.sendRequest('trip_get_statistics', {});

        if (response.status === 'success' && response.data) {
            return {
                total_trips: parseInt(response.data.total_trips) || 0,
                total_distance: parseFloat(response.data.total_distance) || 0,
                avg_speed: parseFloat(response.data.avg_speed) || 0,
                safety_score: parseInt(response.data.safety_score) || 1000
            };
        }

        return null;
    }

    // Vehicle Operations
    async getVehicles() {
        const response = await this.sendRequest('vehicle_get_list', {});

        if (response.status === 'success') {
            if (response.data && response.data.vehicles) {
                return response.data.vehicles;
            }
            if (response.data && Array.isArray(response.data)) {
                return response.data;
            }
            return [];
        }

        return [];
    }

    async addVehicle(licensePlate, make, model, year, type, vin = '') {
        const response = await this.sendRequest('vehicle_add', {
            license_plate: licensePlate,
            make: make,
            model: model,
            year: year.toString(),
            type: type.toString(),
            vin: vin
        });

        if (response.status === 'success' && response.data) {
            return parseInt(response.data.vehicle_id) || null;
        }

        return null;
    }

    async updateVehicleOdometer(vehicleId, odometer) {
        const response = await this.sendRequest('vehicle_update_odometer', {
            vehicle_id: vehicleId.toString(),
            odometer: odometer.toString()
        });

        return response.status === 'success';
    }

    async addMaintenanceRecord(vehicleId, type, odometer, serviceCenter, description, cost) {
        const response = await this.sendRequest('vehicle_add_maintenance', {
            vehicle_id: vehicleId.toString(),
            type: type.toString(),
            odometer: odometer.toString(),
            service_center: serviceCenter,
            description: description,
            cost: cost.toString()
        });

        if (response.status === 'success' && response.data) {
            return parseInt(response.data.maintenance_id) || null;
        }

        return null;
    }

    async getMaintenanceAlerts() {
        const response = await this.sendRequest('vehicle_get_alerts', {});

        if (response.status === 'success') {
            if (response.data && response.data.alerts) {
                return response.data.alerts;
            }
            if (response.data && Array.isArray(response.data)) {
                return response.data;
            }
            return [];
        }

        return [];
    }

    async getMaintenanceHistory(vehicleId) {
        const response = await this.sendRequest('vehicle_get_maintenance_history', {
            vehicle_id: vehicleId.toString()
        });

        if (response.status === 'success') {
            if (response.data && response.data.maintenance) {
                return response.data.maintenance;
            }
            if (response.data && Array.isArray(response.data)) {
                return response.data;
            }
            return [];
        }

        return [];
    }

    // Expense Operations
    async addExpense(vehicleId, category, amount, description, tripId = null) {
        const data = {
            vehicle_id: vehicleId.toString(),
            category: category.toString(),
            amount: amount.toString(),
            description: description
        };

        if (tripId) {
            data.trip_id = tripId.toString();
        }

        const response = await this.sendRequest('expense_add', data);

        if (response.status === 'success' && response.data) {
            return parseInt(response.data.expense_id) || null;
        }

        return null;
    }

    async addFuelExpense(vehicleId, tripId, quantity, pricePerUnit, station) {
        const response = await this.sendRequest('expense_add_fuel', {
            vehicle_id: vehicleId.toString(),
            trip_id: tripId ? tripId.toString() : '0',
            quantity: quantity.toString(),
            price_per_unit: pricePerUnit.toString(),
            station: station
        });

        if (response.status === 'success' && response.data) {
            return parseInt(response.data.expense_id) || null;
        }

        return null;
    }

    async getExpenses(limit = 100) {
        const response = await this.sendRequest('expense_get_list', {
            limit: limit.toString()
        });

        if (response.status === 'success') {
            if (response.data && response.data.expenses) {
                return response.data.expenses;
            }
            if (response.data && Array.isArray(response.data)) {
                return response.data;
            }
            return [];
        }

        return [];
    }

    async getExpenseSummary(startDate = null, endDate = null) {
        const data = {};
        if (startDate) data.start_date = startDate.toString();
        if (endDate) data.end_date = endDate.toString();

        const response = await this.sendRequest('expense_get_summary', data);

        if (response.status === 'success' && response.data) {
            return {
                total_expenses: parseFloat(response.data.total_expenses) || 0,
                fuel_expenses: parseFloat(response.data.fuel_expenses) || 0,
                maintenance_expenses: parseFloat(response.data.maintenance_expenses) || 0,
                total_transactions: parseInt(response.data.total_transactions) || 0,
                average_daily_expense: parseFloat(response.data.avg_daily_expense) || 0
            };
        }

        return null;
    }

    async setBudgetLimit(category, monthlyLimit) {
        const response = await this.sendRequest('expense_set_budget', {
            category: category.toString(),
            monthly_limit: monthlyLimit.toString()
        });

        return response.status === 'success';
    }

    async getBudgetAlerts() {
        const response = await this.sendRequest('expense_get_budget_alerts', {});

        if (response.status === 'success') {
            if (response.data && response.data.alerts) {
                return response.data.alerts;
            }
            if (response.data && Array.isArray(response.data)) {
                return response.data;
            }
            return [];
        }

        return [];
    }
    async checkTripExists(tripId) {
        try {
            const trips = await this.getTripHistory(100);
            return trips.some(trip => parseInt(trip.trip_id) === parseInt(tripId));
        } catch (error) {
            console.warn('⚠️ Could not verify trip existence:', error);
            // If we can't check, assume it exists (let backend handle validation)
            return true;
        }
    }
    // Driver Operations
    async getDriverProfile() {
        const response = await this.sendRequest('driver_get_profile', {});

        if (response.status === 'success' && response.data) {
            return {
                driver_id: parseInt(response.data.driver_id) || 0,
                name: response.data.name || '',
                email: response.data.email || '',
                phone: response.data.phone || '',
                safety_score: parseInt(response.data.safety_score) || 1000,
                total_trips: parseInt(response.data.total_trips) || 0,
                total_distance: parseFloat(response.data.total_distance) || 0
            };
        }

        return null;
    }

    async updateDriverProfile(fullName, email, phone, licenseNumber = null) {
        const data = {
            full_name: fullName,
            email: email || '',
            phone: phone || ''
        };

        if (licenseNumber) {
            data.license_number = licenseNumber;
        }

        const response = await this.sendRequest('driver_update_profile', data);

        return response.status === 'success';
    }

    async getDriverBehavior() {
        const response = await this.sendRequest('driver_get_behavior', {});

        if (response.status === 'success' && response.data) {
            return {
                safety_score: parseInt(response.data.safety_score) || 1000,
                total_trips: parseInt(response.data.total_trips) || 0,
                total_distance: parseFloat(response.data.total_distance) || 0,
                harsh_braking_rate: parseFloat(response.data.harsh_braking_rate) || 0,
                avg_speed: parseFloat(response.data.avg_speed) || 0,
                rank: parseInt(response.data.rank) || 0,
                percentile: parseFloat(response.data.percentile) || 0
            };
        }

        return null;
    }

    async getDriverLeaderboard(limit = 10) {
        const response = await this.sendRequest('driver_get_leaderboard', {
            limit: limit.toString()
        });

        if (response.status === 'success') {
            if (response.data && response.data.leaderboard) {
                return response.data.leaderboard;
            }
            if (response.data && Array.isArray(response.data)) {
                return response.data;
            }
            return [];
        }

        return [];
    }

    async getImprovementRecommendations() {
        const response = await this.sendRequest('driver_get_recommendations', {});

        if (response.status === 'success') {
            if (response.data && response.data.recommendations) {
                return response.data.recommendations;
            }
            if (response.data && Array.isArray(response.data)) {
                return response.data;
            }
            return [];
        }

        return [];
    }

    // Incident Operations
    async reportIncident(vehicleId, type, latitude, longitude, description) {
        const response = await this.sendRequest('incident_report', {
            vehicle_id: vehicleId.toString(),
            type: type.toString(),
            latitude: latitude.toString(),
            longitude: longitude.toString(),
            description: description
        });

        if (response.status === 'success' && response.data) {
            return parseInt(response.data.incident_id) || null;
        }

        return null;
    }

    async getIncidents(limit = 20) {
        const response = await this.sendRequest('incident_get_list', {
            limit: limit.toString()
        });

        if (response.status === 'success') {
            if (response.data && response.data.incidents) {
                return response.data.incidents;
            }
            if (response.data && Array.isArray(response.data)) {
                return response.data;
            }
            return [];
        }

        return [];
    }

    async getIncidentStatistics() {
        const response = await this.sendRequest('incident_get_statistics', {});

        if (response.status === 'success' && response.data) {
            return {
                total_incidents: parseInt(response.data.total_incidents) || 0,
                total_accidents: parseInt(response.data.total_accidents) || 0,
                total_breakdowns: parseInt(response.data.total_breakdowns) || 0,
                unresolved_incidents: parseInt(response.data.unresolved_incidents) || 0,
                incident_free_days: parseInt(response.data.incident_free_days) || 0
            };
        }

        return null;
    }

    // Helper Methods
    async getVehicleOptions() {
        const vehicles = await this.getVehicles();
        return vehicles.map(v => ({
            value: v.vehicle_id || v.id,
            text: `${v.make || ''} ${v.model || ''} (${v.license_plate || v.plate || ''})`.trim()
        }));
    }

    getCategoryOptions() {
        return [
            { value: 0, text: 'Fuel' },
            { value: 1, text: 'Maintenance' },
            { value: 2, text: 'Insurance' },
            { value: 3, text: 'Toll' },
            { value: 4, text: 'Parking' },
            { value: 5, text: 'Other' }
        ];
    }

    getIncidentTypeOptions() {
        return [
            { value: 0, text: 'Accident' },
            { value: 1, text: 'Breakdown' },
            { value: 2, text: 'Theft' },
            { value: 3, text: 'Vandalism' },
            { value: 4, text: 'Traffic Violation' }
        ];
    }

    getMaintenanceTypeOptions() {
        return [
            { value: 0, text: 'Oil Change' },
            { value: 1, text: 'Tire Rotation' },
            { value: 2, text: 'Brake Service' },
            { value: 3, text: 'Engine Check' },
            { value: 4, text: 'Transmission Service' },
            { value: 5, text: 'General Service' }
        ];
    }
}

// Database API will be initialized by app.js after app is created
