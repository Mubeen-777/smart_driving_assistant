class SmartDriveWebSocket {
    constructor() {
        this.socket = null;
        this.connected = false;
        this.listeners = {};
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
    }
    
    connect() {
        try {
            this.socket = new WebSocket('ws:
            
            this.socket.onopen = () => {
                console.log('WebSocket connected');
                this.connected = true;
                this.reconnectAttempts = 0;
                
                if (this.onConnectionChange) {
                    this.onConnectionChange('connected');
                }
            };
            
            this.socket.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    
                    if (data.type === 'live_data' && this.onLiveData) {
                        this.onLiveData(data.data);
                    }
                    
                    if (data.type === 'trip_started') {
                        this.emit('trip_started', data.data);
                    } else if (data.type === 'trip_stopped') {
                        this.emit('trip_stopped', data.data);
                    } else if (data.type === 'lane_warning') {
                        
                        const laneEvent = new CustomEvent('smartdrive-lane-warning', {
                            detail: data.data
                        });
                        window.dispatchEvent(laneEvent);
                    } else if (data.type === 'warning') {
                        
                        const alertEvent = new CustomEvent('smartdrive-alert', {
                            detail: {
                                message: `${data.data.warning_type.toUpperCase()}: ${data.data.value}`,
                                type: 'warning'
                            }
                        });
                        window.dispatchEvent(alertEvent);
                    }
                    
                } catch (error) {
                    console.error('Error parsing WebSocket message:', error);
                }
            };
            
            this.socket.onerror = (error) => {
                console.error('WebSocket error:', error);
                if (this.onConnectionChange) {
                    this.onConnectionChange('disconnected');
                }
            };
            
            this.socket.onclose = () => {
                console.log('WebSocket disconnected');
                this.connected = false;
                
                if (this.onConnectionChange) {
                    this.onConnectionChange('disconnected');
                }
                
                if (this.reconnectAttempts < this.maxReconnectAttempts) {
                    this.reconnectAttempts++;
                    setTimeout(() => {
                        console.log(`Attempting to reconnect (${this.reconnectAttempts}/${this.maxReconnectAttempts})...`);
                        this.connect();
                    }, 3000);
                }
            };
            
        } catch (error) {
            console.error('Failed to create WebSocket:', error);
        }
    }
    
    disconnect() {
        if (this.socket) {
            this.socket.close();
            this.socket = null;
        }
        this.connected = false;
    }
    
    startTrip(driverId, vehicleId) {
        if (!this.connected) return false;
        
        const message = {
            command: 'start_trip',
            driver_id: driverId,
            vehicle_id: vehicleId
        };
        
        this.socket.send(JSON.stringify(message));
        return true;
    }
    
    stopTrip() {
        if (!this.connected) return false;
        
        const message = {
            command: 'stop_trip'
        };
        
        this.socket.send(JSON.stringify(message));
        return true;
    }
    
    toggleCamera() {
        if (!this.connected) return false;
        
        const message = {
            command: 'toggle_camera',
            enable: true
        };
        
        this.socket.send(JSON.stringify(message));
        return true;
    }
    
    on(event, callback) {
        if (!this.listeners[event]) {
            this.listeners[event] = [];
        }
        this.listeners[event].push(callback);
    }
    
    emit(event, data) {
        if (this.listeners[event]) {
            this.listeners[event].forEach(callback => callback(data));
        }
    }
}

window.smartDriveWS = new SmartDriveWebSocket();