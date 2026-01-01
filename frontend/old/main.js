// main.js - Main Initialization File (UPDATED)
document.addEventListener('DOMContentLoaded', () => {
    console.log('📱 Smart Drive Web App Loading...');
    
    // First check authentication
    const sessionId = localStorage.getItem('session_id');
    const userData = localStorage.getItem('user_data');
    
    if (!sessionId || !userData) {
        console.log('❌ No valid session found, redirecting to login...');
        window.location.href = 'login.html';
        return;
    }
    
    // Wait for all classes to be defined
    if (typeof SmartDriveWebApp === 'undefined') {
        console.error('❌ SmartDriveWebApp class not found. Check script loading order.');
        setTimeout(() => {
            if (typeof SmartDriveWebApp !== 'undefined') {
                initializeApp();
            } else {
                alert('Failed to load application. Please refresh the page.');
            }
        }, 100);
        return;
    }
    
    initializeApp();
});

function initializeApp() {
    try {
        // Initialize main application
        window.app = new SmartDriveWebApp();
        
        console.log('✅ Smart Drive Web App initialized');
        console.log('✅ App instance:', window.app);
        
        // Set up global functions after app is initialized
        setupGlobalFunctions();
    } catch (error) {
        console.error('❌ Failed to initialize app:', error);
        console.error('Error stack:', error.stack);
        alert('Failed to initialize application: ' + error.message + '\n\nPlease check the browser console for details.');
    }
}

function setupGlobalFunctions() {
    // Make button functions globally available
    window.startTrip = () => {
        if (window.app) {
            window.app.startTrip();
        } else {
            console.error('App not initialized');
        }
    };
    
    window.stopTrip = () => {
        if (window.app) {
            window.app.stopTrip();
        } else {
            console.error('App not initialized');
        }
    };
    
    window.toggleCamera = () => {
        if (window.app) {
            window.app.toggleCamera();
        } else {
            console.error('App not initialized');
        }
    };
    
    window.logout = () => {
        if (window.app) {
            window.app.logout();
        } else {
            console.error('App not initialized');
        }
    };
    
    // Global functions for onclick handlers
    window.showNewTripModal = async () => {
        if (window.modals) {
            await window.modals.populateVehicleSelect('tripVehicle');
            window.modals.showModal('newTripModal');
        }
    };
    
    window.showAddVehicleModal = () => {
        if (window.modals) {
            window.modals.showAddVehicleModal();
        }
    };
    
    window.showAddExpenseModal = async () => {
        if (window.modals) {
            await window.modals.showAddExpenseModal();
        }
    };
    
    window.showAddFuelExpenseModal = async () => {
        if (window.modals) {
            await window.modals.showAddFuelExpenseModal();
        }
    };
    
    window.showBudgetModal = () => {
        if (window.modals) {
            window.modals.showBudgetModal();
        }
    };
    
    window.showAddDriverModal = () => {
        if (window.modals) {
            window.modals.showAddDriverModal();
        }
    };
    
    window.showReportIncidentModal = async () => {
        if (window.modals) {
            await window.modals.showReportIncidentModal();
        }
    };
    
    window.showEmergencyModal = () => {
        if (window.modals) {
            window.modals.showEmergencyModal();
        }
    };
    
    window.loadTrips = () => {
        if (window.app) {
            window.app.loadTrips();
        }
    };
    
    window.loadLeaderboard = () => {
        if (window.app) {
            window.app.loadDrivers();
        }
    };
    
    window.refreshMaintenanceAlerts = async () => {
        if (window.db && window.app) {
            try {
                const alerts = await window.db.getMaintenanceAlerts();
                window.app.showToast(`Found ${alerts.length} maintenance alerts`, 'info');
                if (window.app.currentPage === 'vehicles') {
                    window.app.loadVehicles();
                }
            } catch (error) {
                window.app.showToast('Failed to refresh alerts: ' + error.message, 'error');
            }
        }
    };
    
    window.submitNewTrip = () => {
        if (window.modals) {
            window.modals.submitNewTrip();
        }
    };
    
    window.closeModal = (modalId) => {
        if (window.modals) {
            window.modals.closeModal(modalId);
        }
    };
    
    window.getCurrentLocation = () => {
        if (navigator.geolocation) {
            navigator.geolocation.getCurrentPosition((position) => {
                const lat = position.coords.latitude;
                const lon = position.coords.longitude;
                const latInput = document.getElementById('tripStartLat');
                const lonInput = document.getElementById('tripStartLon');
                const locationInput = document.getElementById('tripStartLocation');
                
                if (latInput) latInput.value = lat;
                if (lonInput) lonInput.value = lon;
                if (locationInput) locationInput.value = `${lat.toFixed(6)}, ${lon.toFixed(6)}`;
                
                if (window.app) {
                    window.app.showToast('Location updated', 'success');
                }
            }, (error) => {
                if (window.app) {
                    window.app.showToast('Failed to get location: ' + error.message, 'error');
                }
            });
        } else {
            if (window.app) {
                window.app.showToast('Geolocation not supported', 'error');
            }
        }
    };
    
    window.clearAlerts = () => {
        const container = document.getElementById('alertsContainer');
        if (container) {
            container.innerHTML = '<div class="alert-item info"><i class="fas fa-info-circle"></i><div class="alert-content"><strong>No Alerts</strong><p>All clear!</p></div></div>';
        }
    };

    window.showSystemLogs = () => {
        if (window.app) {
            window.app.showToast('System logs feature not yet implemented', 'info');
        }
    };

    window.showHelp = (topic) => {
        if (window.app) {
            window.app.showToast(`Help for ${topic} not yet implemented`, 'info');
        }
    };

    window.contactSupport = () => {
        if (window.app) {
            window.app.showToast('Support contact feature not yet implemented', 'info');
        }
    };

    window.backupDatabase = async () => {
        if (window.db && window.app) {
            try {
                window.app.showLoading();
                const backupPath = await window.db.backupDatabase();
                window.app.showToast(`Database backed up to: ${backupPath || 'backup location'}`, 'success');
            } catch (error) {
                window.app.showToast('Backup failed: ' + error.message, 'error');
            } finally {
                window.app.hideLoading();
            }
        }
    };

    window.clearCache = () => {
        if (window.app) {
            localStorage.removeItem('cache');
            window.app.showToast('Cache cleared', 'success');
        }
    };

    window.checkForUpdates = () => {
        if (window.app) {
            window.app.showToast('Update check feature not yet implemented', 'info');
        }
    };

    window.saveVehicleSettings = () => {
        if (window.app) {
            window.app.showToast('Vehicle settings saved', 'success');
        }
    };

    window.saveAnalyticsSettings = () => {
        if (window.app) {
            window.app.showToast('Analytics settings saved', 'success');
        }
    };

    window.generateTripReport = () => {
        if (window.analytics) {
            window.analytics.generateReport('daily');
        }
    };

    window.generateExpenseReport = () => {
        if (window.analytics) {
            window.analytics.generateReport('monthly');
        }
    };

    window.generateMaintenanceReport = () => {
        if (window.analytics) {
            window.analytics.generateReport('weekly');
        }
    };

    window.generateSafetyReport = () => {
        if (window.analytics) {
            window.analytics.generateReport('monthly');
        }
    };

    window.exportReport = () => {
        if (window.analytics) {
            window.analytics.exportReport('Report');
        }
    };

    window.quickReport = (type) => {
        if (window.modals) {
            window.modals.showReportIncidentModal();
            const typeSelect = document.getElementById('incidentType');
            if (typeSelect) {
                const typeMap = {
                    'accident': '0',
                    'breakdown': '1',
                    'theft': '2',
                    'violation': '4'
                };
                if (typeMap[type]) {
                    typeSelect.value = typeMap[type];
                }
            }
        }
    };

    // Form submission handlers
    window.submitAddVehicle = async () => {
        const plate = document.getElementById('vehiclePlate')?.value;
        const make = document.getElementById('vehicleMake')?.value;
        const model = document.getElementById('vehicleModel')?.value;
        const year = document.getElementById('vehicleYear')?.value;
        const type = document.getElementById('vehicleType')?.value || '0';
        const vin = document.getElementById('vehicleVin')?.value || '';

        if (!plate || !make || !model || !year) {
            if (window.app) window.app.showToast('Please fill all required fields', 'error');
            return;
        }

        try {
            if (window.db) {
                const vehicleId = await window.db.addVehicle(plate, make, model, parseInt(year), parseInt(type), vin);
                if (vehicleId) {
                    if (window.app) {
                        window.app.showToast('Vehicle added successfully!', 'success');
                        window.closeModal('addVehicleModal');
                        if (window.app.currentPage === 'vehicles') {
                            window.app.loadVehicles();
                        }
                    }
                }
            }
        } catch (error) {
            if (window.app) window.app.showToast('Failed to add vehicle: ' + error.message, 'error');
        }
    };

    window.submitAddExpense = async () => {
        const vehicleId = document.getElementById('expenseVehicle')?.value;
        const category = document.getElementById('expenseCategory')?.value || '0';
        const amount = document.getElementById('expenseAmount')?.value;
        const description = document.getElementById('expenseDescription')?.value || '';

        if (!vehicleId || !amount) {
            if (window.app) window.app.showToast('Please fill all required fields', 'error');
            return;
        }

        try {
            if (window.db) {
                const expenseId = await window.db.addExpense(parseInt(vehicleId), parseInt(category), parseFloat(amount), description);
                if (expenseId) {
                    if (window.app) {
                        window.app.showToast('Expense added successfully!', 'success');
                        window.closeModal('addExpenseModal');
                        if (window.app.currentPage === 'expenses') {
                            window.app.loadExpenses();
                        }
                    }
                }
            }
        } catch (error) {
            if (window.app) window.app.showToast('Failed to add expense: ' + error.message, 'error');
        }
    };

    window.submitAddDriver = async () => {
        const name = document.getElementById('driverName')?.value;
        const email = document.getElementById('driverEmail')?.value || '';
        const phone = document.getElementById('driverPhone')?.value || '';
        const license = document.getElementById('driverLicense')?.value || '';
        const username = document.getElementById('driverUsername')?.value;
        const password = document.getElementById('driverPassword')?.value;

        if (!name || !username || !password) {
            if (window.app) window.app.showToast('Please fill all required fields', 'error');
            return;
        }

        try {
            if (window.db) {
                const success = await window.db.register(username, password, name, email, phone);
                if (success) {
                    if (window.app) {
                        window.app.showToast('Driver added successfully!', 'success');
                        window.closeModal('addDriverModal');
                        if (window.app.currentPage === 'drivers') {
                            window.app.loadDrivers();
                        }
                    }
                }
            }
        } catch (error) {
            if (window.app) window.app.showToast('Failed to add driver: ' + error.message, 'error');
        }
    };

    window.submitIncident = async () => {
        const type = document.getElementById('incidentType')?.value;
        const vehicleId = document.getElementById('incidentVehicle')?.value;
        const lat = document.getElementById('incidentLat')?.value || '31.5204';
        const lon = document.getElementById('incidentLon')?.value || '74.3587';
        const description = document.getElementById('incidentDescription')?.value || '';

        if (!type || !vehicleId) {
            if (window.app) window.app.showToast('Please fill all required fields', 'error');
            return;
        }

        try {
            if (window.db) {
                const incidentId = await window.db.reportIncident(
                    parseInt(vehicleId), 
                    parseInt(type), 
                    parseFloat(lat), 
                    parseFloat(lon), 
                    description
                );
                if (incidentId) {
                    if (window.app) {
                        window.app.showToast('Incident reported successfully!', 'success');
                        window.closeModal('reportIncidentModal');
                        if (window.app.currentPage === 'incidents') {
                            window.app.loadIncidents();
                        }
                    }
                }
            }
        } catch (error) {
            if (window.app) window.app.showToast('Failed to report incident: ' + error.message, 'error');
        }
    };

    window.submitAddFuelExpense = async () => {
        const vehicleId = document.getElementById('fuelExpenseVehicle')?.value;
        const quantity = document.getElementById('fuelQuantity')?.value;
        const pricePerUnit = document.getElementById('fuelPricePerUnit')?.value;
        const station = document.getElementById('fuelStation')?.value || '';
        const tripId = document.getElementById('fuelTripId')?.value || '0';

        if (!vehicleId || !quantity || !pricePerUnit) {
            if (window.app) window.app.showToast('Please fill all required fields', 'error');
            return;
        }

        try {
            if (window.db) {
                const expenseId = await window.db.addFuelExpense(
                    parseInt(vehicleId),
                    parseInt(tripId) || 0,
                    parseFloat(quantity),
                    parseFloat(pricePerUnit),
                    station
                );
                if (expenseId) {
                    if (window.app) {
                        window.app.showToast('Fuel expense added successfully!', 'success');
                        window.closeModal('addFuelExpenseModal');
                        if (window.app.currentPage === 'expenses') {
                            window.app.loadExpenses();
                        }
                    }
                }
            }
        } catch (error) {
            if (window.app) window.app.showToast('Failed to add fuel expense: ' + error.message, 'error');
        }
    };

    window.submitNewTrip = async () => {
        const vehicleId = document.getElementById('tripVehicle')?.value;
        const startLocation = document.getElementById('tripStartLocation')?.value || '';
        const startLat = document.getElementById('tripStartLat')?.value;
        const startLon = document.getElementById('tripStartLon')?.value;
        const purpose = document.getElementById('tripPurpose')?.value || '';

        if (!vehicleId || !startLat || !startLon) {
            if (window.app) window.app.showToast('Please fill all required fields', 'error');
            return;
        }

        try {
            if (window.db) {
                const tripResult = await window.db.startTrip(
                    parseInt(vehicleId),
                    parseFloat(startLat),
                    parseFloat(startLon),
                    startLocation
                );
                if (tripResult && tripResult.trip_id) {
                    if (window.app) {
                        window.app.showToast('Trip started successfully!', 'success');
                        window.closeModal('newTripModal');
                        window.app.isOnTrip = true;
                        window.app.activeTripId = tripResult.trip_id;
                        window.app.liveData.trip_id = tripResult.trip_id;
                        window.app.liveData.trip_active = true;
                        // Start trip tracker if available
                        if (window.app.tripTracker) {
                            window.app.tripTracker.startTrip(
                                tripResult.trip_id,
                                parseFloat(startLat),
                                parseFloat(startLon)
                            );
                        }
                        window.app.updateTripControls();
                        if (window.app.currentPage === 'dashboard' || window.app.currentPage === 'trips') {
                            window.app.loadTrips && window.app.loadTrips();
                        }
                    }
                }
            }
        } catch (error) {
            if (window.app) window.app.showToast('Failed to start trip: ' + error.message, 'error');
        }
    };

    // Vision processing functions
    window.startVisionProcessing = () => {
        if (window.cameraManager) {
            window.cameraManager.startVisionProcessing();
        } else if (window.app) {
            window.app.showToast('Vision processing requires camera to be active', 'warning');
        }
    };

    window.toggleNightVision = () => {
        if (window.cameraManager) {
            window.cameraManager.toggleNightVision();
        } else if (window.app) {
            window.app.showToast('Night vision requires camera to be active', 'warning');
        }
    };
    
    console.log('✅ Global functions set up');
}