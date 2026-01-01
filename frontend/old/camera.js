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
                animation: false, 
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

        this.updateDetectionDisplay(lane_deviation, objects_detected, driver_attention, drowsiness_level);

        this.addToDetectionLog(lane_deviation, objects_detected, lane_status);

        this.updateVisionChart(lane_deviation, driver_attention);

        if (detected_objects && detected_objects.length > 0) {
            this.updateObjectDetection(detected_objects);
        }

        this.checkForWarnings(lane_deviation, driver_attention, drowsiness_level);

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

        while (logContainer.children.length > 50) {
            logContainer.removeChild(logContainer.lastChild);
        }

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

        this.visionChart.data.labels.push(now);
        this.visionChart.data.datasets[0].data.push(laneDeviation);
        this.visionChart.data.datasets[1].data.push(driverAttention);

        while (this.visionChart.data.labels.length > this.maxDataPoints) {
            this.visionChart.data.labels.shift();
            this.visionChart.data.datasets[0].data.shift();
            this.visionChart.data.datasets[1].data.shift();
        }

        this.visionChart.update('none');
    }

    updateLaneVisualization(laneStatus, deviation) {
        
        const centerLine = document.querySelector('.lane-center');
        const leftBoundary = document.querySelector('.lane-left');
        const rightBoundary = document.querySelector('.lane-right');
        
        if (centerLine) {
            
            const offset = (deviation / 100) * 50; 
            centerLine.style.transform = `translateX(${offset}px)`;
            
            if (laneStatus === 'LEFT' || laneStatus === 'RIGHT') {
                centerLine.style.backgroundColor = 'rgba(248, 150, 30, 0.9)'; 
            } else if (laneStatus === 'DEPARTURE') {
                centerLine.style.backgroundColor = 'rgba(247, 37, 133, 0.9)'; 
            } else {
                centerLine.style.backgroundColor = 'rgba(76, 201, 240, 0.9)'; 
            }
        }

        const laneStatusIndicator = document.getElementById('laneStatusIndicator');
        if (laneStatusIndicator) {
            const statusText = laneStatus === 'CENTERED' ? 'Centered' : 
                              laneStatus === 'LEFT' ? 'Left' :
                              laneStatus === 'RIGHT' ? 'Right' : 'Departure';
            laneStatusIndicator.innerHTML = `<i class="fas fa-road"></i> Lane: ${statusText}`;
            
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

        if (this.app && this.app.liveData) {
            this.app.liveData.lane_status = laneStatus;
            if (this.app.updateDashboard) {
                this.app.updateDashboard();
            }
        }
    }

    checkForWarnings(laneDeviation, driverAttention, drowsinessLevel) {
        if (!this.app) return;
        
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
            
            if (obj.type === 'Pedestrian') {
                box.style.borderColor = '#f72585';
            } else if (obj.type === 'Car' || obj.type === 'Truck') {
                box.style.borderColor = '#4361ee';
            }
            
            box.innerHTML = `<span>${obj.type} ${obj.confidence}%</span>`;
            container.appendChild(box);
        });
    }

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

if (window.app) {
    window.cameraManager = new CameraManager(window.app);
}