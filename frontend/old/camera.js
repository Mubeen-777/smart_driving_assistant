// camera.js - Camera and Vision System (REAL DATA ONLY - NO SIMULATION)
// This module displays REAL data from the WebSocket bridge (C++ OpenCV backend)
// All detection data comes from the actual camera and computer vision processing

class CameraManager {
    constructor(app) {
        this.app = app;
        this.visionChart = null;
        this.detectionLog = [];
        this.chartData = {
            laneDeviation: [],
            driverAttention: []
        };
        this.maxDataPoints = 20;
    }

    initVisionSystem() {
        this.initVisionChart();
        this.setupDetectionOverlay();
        console.log('📷 Vision system initialized - waiting for real camera data from WebSocket');
    }

    initVisionChart() {
        const canvas = document.getElementById('visionChart');
        if (!canvas) return;

        const ctx = canvas.getContext('2d');
        
        // Initialize with empty data - will be populated by real WebSocket data
        this.visionChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    {
                        label: 'Lane Deviation %',
                        data: [],
                        borderColor: '#4361ee',
                        backgroundColor: 'rgba(67, 97, 238, 0.1)',
                        fill: true,
                        tension: 0.4
                    },
                    {
                        label: 'Driver Attention %',
                        data: [],
                        borderColor: '#4cc9f0',
                        backgroundColor: 'rgba(76, 201, 240, 0.1)',
                        fill: true,
                        tension: 0.4
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false, // Disable animation for real-time performance
                plugins: {
                    legend: {
                        display: true,
                        position: 'top'
                    }
                },
                scales: {
                    y: {
                        beginAtZero: true,
                        max: 100,
                        title: {
                            display: true,
                            text: 'Percentage'
                        }
                    },
                    x: {
                        title: {
                            display: true,
                            text: 'Time'
                        }
                    }
                }
            }
        });
    }

    setupDetectionOverlay() {
        const container = document.querySelector('.vision-container');
        if (!container) return;

        // Create lane visualization elements (will be updated by real data)
        const centerLine = document.createElement('div');
        centerLine.className = 'lane-center-line';
        centerLine.id = 'laneCenterLine';
        container.appendChild(centerLine);

        const leftBoundary = document.createElement('div');
        leftBoundary.className = 'lane-boundary lane-left';
        leftBoundary.id = 'laneLeftBoundary';
        container.appendChild(leftBoundary);

        const rightBoundary = document.createElement('div');
        rightBoundary.className = 'lane-boundary lane-right';
        rightBoundary.id = 'laneRightBoundary';
        container.appendChild(rightBoundary);
    }

    // Called by WebSocket when real detection data arrives from C++ backend
    updateFromWebSocket(detectionData) {
        if (!detectionData) return;

        const {
            lane_deviation = 0,
            objects_detected = 0,
            driver_attention = 100,
            drowsiness_level = 0,
            lane_status = 'CENTERED',
            detected_objects = []
        } = detectionData;

        // Update display with REAL data
        this.updateDetectionDisplay(lane_deviation, objects_detected, driver_attention, drowsiness_level);

        // Add to log
        this.addToDetectionLog(lane_deviation, objects_detected, lane_status);

        // Update chart with REAL data
        this.updateVisionChart(lane_deviation, driver_attention);

        // Update object detection overlay with REAL detected objects
        if (detected_objects && detected_objects.length > 0) {
            this.updateObjectDetection(detected_objects);
        }

        // Check for warnings based on REAL data
        this.checkForWarnings(lane_deviation, driver_attention, drowsiness_level);

        // Update lane visualization
        this.updateLaneVisualization(lane_status, lane_deviation);
    }

    updateDetectionDisplay(laneDeviation, objectsDetected, driverAttention, drowsinessLevel) {
        const laneDeviationEl = document.getElementById('laneDeviation');
        const objectsDetectedEl = document.getElementById('objectsDetected');
        const driverAttentionEl = document.getElementById('driverAttention');
        const drowsinessLevelEl = document.getElementById('drowsinessLevel');

        if (laneDeviationEl) laneDeviationEl.textContent = laneDeviation.toFixed(1) + '%';
        if (objectsDetectedEl) objectsDetectedEl.textContent = objectsDetected.toString();
        if (driverAttentionEl) driverAttentionEl.textContent = driverAttention.toFixed(0) + '%';
        if (drowsinessLevelEl) drowsinessLevelEl.textContent = drowsinessLevel.toFixed(1) + '%';
    }

    addToDetectionLog(laneDeviation, objectsDetected, laneStatus) {
        const logContainer = document.getElementById('detectionLog');
        if (!logContainer) return;

        const now = new Date();
        const timeString = now.toLocaleTimeString();

        const logEntry = document.createElement('div');
        logEntry.className = 'log-entry';

        let message = '';
        let type = 'info';

        if (laneDeviation > 10) {
            message = `⚠️ High lane deviation: ${laneDeviation.toFixed(1)}%`;
            type = 'warning';
        } else if (objectsDetected > 3) {
            message = `Multiple objects detected: ${objectsDetected}`;
            type = 'info';
        } else {
            message = `Lane: ${laneStatus} | Deviation: ${laneDeviation.toFixed(1)}%`;
            type = 'info';
        }

        logEntry.innerHTML = `
            <span class="log-time">${timeString}</span>
            <span class="log-type ${type}">${type.toUpperCase()}</span>
            <span class="log-message">${message}</span>
        `;

        logContainer.insertBefore(logEntry, logContainer.firstChild);

        // Limit log entries
        while (logContainer.children.length > 50) {
            logContainer.removeChild(logContainer.lastChild);
        }

        // Store in memory
        this.detectionLog.unshift({
            time: now,
            laneDeviation,
            objectsDetected,
            type,
            message
        });

        if (this.detectionLog.length > 100) {
            this.detectionLog.pop();
        }
    }

    updateVisionChart(laneDeviation, driverAttention) {
        if (!this.visionChart) return;

        const now = new Date().toLocaleTimeString();

        // Add new data point
        this.visionChart.data.labels.push(now);
        this.visionChart.data.datasets[0].data.push(laneDeviation);
        this.visionChart.data.datasets[1].data.push(driverAttention);

        // Keep only last N data points
        while (this.visionChart.data.labels.length > this.maxDataPoints) {
            this.visionChart.data.labels.shift();
            this.visionChart.data.datasets[0].data.shift();
            this.visionChart.data.datasets[1].data.shift();
        }

        this.visionChart.update('none');
    }

    updateLaneVisualization(laneStatus, deviation) {
        // Update lane center line in vision overlay
        const centerLine = document.querySelector('.lane-center');
        const leftBoundary = document.querySelector('.lane-left');
        const rightBoundary = document.querySelector('.lane-right');
        
        if (centerLine) {
            // Shift center line based on real deviation
            const offset = (deviation / 100) * 50; // Max 50% offset
            centerLine.style.transform = `translateX(${offset}px)`;
            
            // Color based on status
            if (laneStatus === 'LEFT' || laneStatus === 'RIGHT') {
                centerLine.style.backgroundColor = 'rgba(248, 150, 30, 0.9)'; // Warning
            } else if (laneStatus === 'DEPARTURE') {
                centerLine.style.backgroundColor = 'rgba(247, 37, 133, 0.9)'; // Danger
            } else {
                centerLine.style.backgroundColor = 'rgba(76, 201, 240, 0.9)'; // Normal
            }
        }

        // Update lane status indicator
        const laneStatusIndicator = document.getElementById('laneStatusIndicator');
        if (laneStatusIndicator) {
            const statusText = laneStatus === 'CENTERED' ? 'Centered' : 
                              laneStatus === 'LEFT' ? 'Left' :
                              laneStatus === 'RIGHT' ? 'Right' : 'Departure';
            laneStatusIndicator.innerHTML = `<i class="fas fa-road"></i> Lane: ${statusText}`;
            
            // Update color based on status
            if (laneStatus === 'LEFT' || laneStatus === 'RIGHT') {
                laneStatusIndicator.style.background = 'rgba(248, 150, 30, 0.1)';
                laneStatusIndicator.style.color = 'var(--warning-color)';
            } else if (laneStatus === 'DEPARTURE') {
                laneStatusIndicator.style.background = 'rgba(247, 37, 133, 0.1)';
                laneStatusIndicator.style.color = 'var(--danger-color)';
            } else {
                laneStatusIndicator.style.background = 'rgba(76, 201, 240, 0.1)';
                laneStatusIndicator.style.color = 'var(--success-color)';
            }
        }

        // Update app's live data
        if (this.app && this.app.liveData) {
            this.app.liveData.lane_status = laneStatus;
            if (this.app.updateDashboard) {
                this.app.updateDashboard();
            }
        }
    }

    checkForWarnings(laneDeviation, driverAttention, drowsinessLevel) {
        if (!this.app) return;
        
        // Only alert on significant real events
        if (laneDeviation > 12) {
            if (this.app.showToast) {
                this.app.showToast(`⚠️ High lane deviation: ${laneDeviation.toFixed(1)}%`, 'warning');
            }
            if (this.app.playAlertSound) this.app.playAlertSound();
        }

        if (driverAttention < 70) {
            if (this.app.showToast) {
                this.app.showToast(`⚠️ Low driver attention: ${driverAttention.toFixed(0)}%`, 'warning');
            }
            if (this.app.playAlertSound) this.app.playAlertSound();
        }

        if (drowsinessLevel > 8) {
            if (this.app.showToast) {
                this.app.showToast(`🚨 High drowsiness detected: ${drowsinessLevel.toFixed(1)}%`, 'error');
            }
            if (this.app.playAlertSound) this.app.playAlertSound();
        }
    }

    updateObjectDetection(objects) {
        const container = document.getElementById('objectDetection');
        if (!container) return;

        container.innerHTML = '';

        objects.forEach(obj => {
            const box = document.createElement('div');
            box.className = 'object-box';
            box.style.left = `${obj.x}%`;
            box.style.top = `${obj.y}%`;
            box.style.width = `${obj.width}%`;
            box.style.height = `${obj.height}%`;
            
            // Color based on object type
            if (obj.type === 'Pedestrian') {
                box.style.borderColor = '#f72585';
            } else if (obj.type === 'Car' || obj.type === 'Truck') {
                box.style.borderColor = '#4361ee';
            }
            
            box.innerHTML = `<span>${obj.type} ${obj.confidence}%</span>`;
            container.appendChild(box);
        });
    }

    // Public methods
    startVisionProcessing() {
        console.log('📷 Vision processing active - receiving real data from WebSocket');
        this.app.showToast('Vision processing active', 'success');
    }

    stopVisionProcessing() {
        console.log('📷 Vision processing stopped');
        this.app.showToast('Vision processing stopped', 'info');
    }

    toggleNightVision() {
        const nightModeToggle = document.getElementById('nightModeToggle');
        const isNightVision = nightModeToggle ? nightModeToggle.checked : false;
        
        const visionFeed = document.getElementById('visionFeed');
        if (visionFeed) {
            if (isNightVision) {
                visionFeed.style.filter = 'grayscale(100%) brightness(0.5) contrast(1.5)';
            } else {
                visionFeed.style.filter = 'none';
            }
        }
        
        this.app.showToast(`Night vision ${isNightVision ? 'enabled' : 'disabled'}`, 'info');
    }

    // Clear all data
    reset() {
        this.detectionLog = [];
        if (this.visionChart) {
            this.visionChart.data.labels = [];
            this.visionChart.data.datasets[0].data = [];
            this.visionChart.data.datasets[1].data = [];
            this.visionChart.update();
        }
    }
}

// Initialize camera manager when app is ready
if (window.app) {
    window.cameraManager = new CameraManager(window.app);
}
