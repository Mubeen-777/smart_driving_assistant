class ModalManager {
    constructor(app) {
        this.app = app;
        this.currentModal = null;
        this.init();
    }

    init() {
        this.setupModalEvents();
        this.setupFormHandlers();
    }

    setupModalEvents() {
        
        document.addEventListener('click', (e) => {
            if (this.currentModal && e.target.classList.contains('modal')) {
                this.closeModal();
            }
        });

        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && this.currentModal) {
                this.closeModal();
            }
        });
    }

    setupFormHandlers() {
        
        const accountForm = document.getElementById('accountSettingsForm');
        if (accountForm) {
            accountForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.saveAccountSettings();
            });
        }

        const securityForm = document.getElementById('securitySettingsForm');
        if (securityForm) {
            securityForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.updateSecuritySettings();
            });
        }

        const tripForm = document.getElementById('newTripForm');
        if (tripForm) {
            tripForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.submitNewTrip();
            });
        }
    }

    showModal(modalId, data = {}) {
        
        if (this.currentModal && this.currentModal.id !== modalId) {
            this.closeModal();
        }
        
        const modal = document.getElementById(modalId);
        if (!modal) {
            console.error('Modal not found:', modalId);
            return;
        }

        this.currentModal = modal;
        modal.style.display = 'flex';
        modal.classList.add('active');
        
        if (data && Object.keys(data).length > 0) {
            this.populateForm(modalId, data);
        }
        
        setTimeout(() => {
            const firstInput = modal.querySelector('input, select, textarea');
            if (firstInput) firstInput.focus();
        }, 100);
    }

    closeModal(modalId = null) {
        let modalToClose = this.currentModal;
        
        if (modalId) {
            modalToClose = document.getElementById(modalId);
        }
        
        if (modalToClose) {
            modalToClose.classList.remove('active');
            modalToClose.style.display = 'none';
            
            if (modalToClose === this.currentModal) {
                this.currentModal = null;
            }
        }
    }

    populateForm(modalId, data) {
        const modal = document.getElementById(modalId);
        if (!modal) return;

        Object.entries(data).forEach(([key, value]) => {
            const input = modal.querySelector(`[name="${key}"], [id="${key}"]`);
            if (input) {
                if (input.type === 'checkbox') {
                    input.checked = value;
                } else {
                    input.value = value;
                }
            }
        });
    }

    async saveAccountSettings() {
        const fullName = document.getElementById('userFullName').value;
        const email = document.getElementById('userEmail').value;
        const phone = document.getElementById('userPhone').value;
        const license = document.getElementById('userLicense').value;

        this.app.showLoading();
        
        try {
            if (window.db) {
                const success = await window.db.updateDriverProfile(fullName, email, phone, license);
                
                if (success) {
                    this.app.userData.name = fullName;
                    this.app.updateUserUI();
                    this.app.showToast('Account settings saved successfully!', 'success');
                    this.closeModal();
                } else {
                    throw new Error('Failed to save settings');
                }
            } else {
                throw new Error('Database API not available');
            }
        } catch (error) {
            this.app.showToast('Failed to save settings: ' + error.message, 'error');
        } finally {
            this.app.hideLoading();
        }
    }

    async updateSecuritySettings() {
        const currentPassword = document.getElementById('currentPassword').value;
        const newPassword = document.getElementById('newPassword').value;
        const confirmPassword = document.getElementById('confirmPassword').value;

        if (!currentPassword) {
            this.app.showToast('Please enter current password', 'error');
            return;
        }

        if (newPassword !== confirmPassword) {
            this.app.showToast('New passwords do not match', 'error');
            return;
        }

        if (newPassword.length < 6) {
            this.app.showToast('Password must be at least 6 characters', 'error');
            return;
        }

        this.app.showLoading();
        
        try {
            
            if (window.db) {
                const success = await window.db.changePassword(currentPassword, newPassword);
                if (success) {
                    this.app.showToast('Password updated successfully!', 'success');
                    
                    document.getElementById('currentPassword').value = '';
                    document.getElementById('newPassword').value = '';
                    document.getElementById('confirmPassword').value = '';
                    
                    this.closeModal();
                } else {
                    throw new Error('Password change failed');
                }
            } else {
                throw new Error('Database not available');
            }
        } catch (error) {
            this.app.showToast('Failed to update password: ' + error.message, 'error');
        } finally {
            this.app.hideLoading();
        }
    }

    async submitNewTrip() {
        const vehicleId = document.getElementById('tripVehicle').value;
        const startLocation = document.getElementById('tripStartLocation').value;
        const startLat = document.getElementById('tripStartLat').value;
        const startLon = document.getElementById('tripStartLon').value;
        const purpose = document.getElementById('tripPurpose').value;

        if (!vehicleId || !startLat || !startLon) {
            this.app.showToast('Please fill all required fields', 'error');
            return;
        }

        this.app.showLoading();
        
        try {
            if (window.db) {
                const result = await window.db.startTrip(vehicleId, parseFloat(startLat), parseFloat(startLon), startLocation);
                
                if (result && result.trip_id) {
                    this.app.liveData.trip_active = true;
                    this.app.liveData.trip_id = result.trip_id;
                    this.app.activeTripId = result.trip_id;
                    
                    if (this.app.tripTracker) {
                        this.app.tripTracker.startTrip(
                            result.trip_id,
                            parseFloat(startLat),
                            parseFloat(startLon)
                        );
                    }
                    this.app.updateTripControls();
                    this.app.updateDashboard();
                    
                    this.app.showToast('Trip started successfully!', 'success');
                    this.closeModal();
                } else {
                    throw new Error('Failed to start trip');
                }
            } else {
                throw new Error('Database API not available');
            }
        } catch (error) {
            this.app.showToast('Failed to start trip: ' + error.message, 'error');
        } finally {
            this.app.hideLoading();
        }
    }

    showAddVehicleModal() {
        this.showModal('addVehicleModal');
    }

    async showAddExpenseModal() {
        await this.populateVehicleSelect('expenseVehicle');
        this.showModal('addExpenseModal');
    }

    async showAddFuelExpenseModal() {
        await this.populateVehicleSelect('fuelExpenseVehicle');
        this.showModal('addFuelExpenseModal');
    }

    showBudgetModal() {
        this.showModal('budgetModal');
    }

    showAddDriverModal() {
        this.showModal('addDriverModal');
    }

    async showReportIncidentModal() {
        await this.populateVehicleSelect('incidentVehicle');
        this.showModal('reportIncidentModal');
    }

    showEmergencyModal() {
        this.showModal('emergencyModal');
    }

    async submitAddVehicle() {
        const plate = document.getElementById('vehiclePlate').value;
        const make = document.getElementById('vehicleMake').value;
        const model = document.getElementById('vehicleModel').value;
        const year = document.getElementById('vehicleYear').value;
        const type = document.getElementById('vehicleType').value;
        const vin = document.getElementById('vehicleVIN').value;

        if (!plate || !make || !model || !year) {
            this.app.showToast('Please fill all required fields', 'error');
            return;
        }

        this.app.showLoading();
        
        try {
            if (window.db) {
                const vehicleId = await window.db.addVehicle(plate, make, model, parseInt(year), parseInt(type), vin);
                
                if (vehicleId) {
                    this.app.showToast('Vehicle added successfully!', 'success');
                    this.closeModal();
                    
                    if (this.app.currentPage === 'vehicles') {
                        this.app.loadVehicles();
                    }
                } else {
                    throw new Error('License plate may already exist');
                }
            } else {
                throw new Error('Database API not available');
            }
        } catch (error) {
            this.app.showToast('Failed to add vehicle: ' + error.message, 'error');
        } finally {
            this.app.hideLoading();
        }
    }

    async submitAddExpense() {
        const vehicleId = document.getElementById('expenseVehicle').value;
        const category = document.getElementById('expenseCategory').value;
        const amount = document.getElementById('expenseAmount').value;
        const description = document.getElementById('expenseDescription').value;
        const date = document.getElementById('expenseDate').value;

        if (!vehicleId || !category || !amount || !description) {
            this.app.showToast('Please fill all required fields', 'error');
            return;
        }

        this.app.showLoading();
        
        try {
            if (window.db) {
                const expenseId = await window.db.addExpense(
                    vehicleId,
                    parseInt(category),
                    parseFloat(amount),
                    description
                );
                
                if (expenseId) {
                    this.app.showToast('Expense added successfully!', 'success');
                    this.closeModal();
                    
                    if (this.app.currentPage === 'expenses') {
                        this.app.loadExpenses();
                    }
                } else {
                    throw new Error('Failed to add expense');
                }
            } else {
                throw new Error('Database API not available');
            }
        } catch (error) {
            this.app.showToast('Failed to add expense: ' + error.message, 'error');
        } finally {
            this.app.hideLoading();
        }
    }

    async submitReportIncident() {
        const vehicleId = document.getElementById('incidentVehicle').value;
        const type = document.getElementById('incidentType').value;
        const lat = document.getElementById('incidentLat').value;
        const lon = document.getElementById('incidentLon').value;
        const description = document.getElementById('incidentDescription').value;

        if (!vehicleId || !type || !description) {
            this.app.showToast('Please fill all required fields', 'error');
            return;
        }

        this.app.showLoading();
        
        try {
            if (window.db) {
                const incidentId = await window.db.reportIncident(
                    vehicleId,
                    parseInt(type),
                    parseFloat(lat || this.app.liveData.latitude),
                    parseFloat(lon || this.app.liveData.longitude),
                    description
                );
                
                if (incidentId) {
                    this.app.showToast('Incident reported successfully!', 'success');
                    this.closeModal();
                    
                    if (this.app.currentPage === 'incidents') {
                        this.app.loadIncidents();
                    }
                } else {
                    throw new Error('Failed to report incident');
                }
            } else {
                throw new Error('Database API not available');
            }
        } catch (error) {
            this.app.showToast('Failed to report incident: ' + error.message, 'error');
        } finally {
            this.app.hideLoading();
        }
    }

    async populateVehicleSelect(selectId, includeEmpty = true) {
        const select = document.getElementById(selectId);
        if (!select) return;

        select.innerHTML = '';
        
        if (includeEmpty) {
            const option = document.createElement('option');
            option.value = '';
            option.textContent = 'Select Vehicle';
            select.appendChild(option);
        }

        if (window.db) {
            try {
                const vehicles = await window.db.getVehicleOptions();
                vehicles.forEach(vehicle => {
                    const option = document.createElement('option');
                    option.value = vehicle.value;
                    option.textContent = vehicle.text;
                    select.appendChild(option);
                });
            } catch (error) {
                console.error('Failed to load vehicles:', error);
            }
        }
    }

    populateCategorySelect(selectId, includeEmpty = true) {
        const select = document.getElementById(selectId);
        if (!select) return;

        select.innerHTML = '';
        
        if (includeEmpty) {
            const option = document.createElement('option');
            option.value = '';
            option.textContent = 'Select Category';
            select.appendChild(option);
        }

        const categories = [
            { id: 0, text: 'Fuel' },
            { id: 1, text: 'Maintenance' },
            { id: 2, text: 'Insurance' },
            { id: 3, text: 'Toll' },
            { id: 4, text: 'Parking' },
            { id: 5, text: 'Other' }
        ];

        categories.forEach(category => {
            const option = document.createElement('option');
            option.value = category.id;
            option.textContent = category.text;
            select.appendChild(option);
        });
    }

    populateIncidentTypeSelect(selectId, includeEmpty = true) {
        const select = document.getElementById(selectId);
        if (!select) return;

        select.innerHTML = '';
        
        if (includeEmpty) {
            const option = document.createElement('option');
            option.value = '';
            option.textContent = 'Select Incident Type';
            select.appendChild(option);
        }

        const types = [
            { id: 0, text: 'Accident' },
            { id: 1, text: 'Breakdown' },
            { id: 2, text: 'Theft' },
            { id: 3, text: 'Vandalism' },
            { id: 4, text: 'Traffic Violation' }
        ];

        types.forEach(type => {
            const option = document.createElement('option');
            option.value = type.id;
            option.textContent = type.text;
            select.appendChild(option);
        });
    }
}

if (window.app) {
    window.modals = new ModalManager(window.app);
}