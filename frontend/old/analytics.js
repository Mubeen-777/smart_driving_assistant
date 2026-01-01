class AnalyticsManager {
    constructor(app) {
        this.app = app;
        this.charts = {};
        this.performanceData = {
            safetyScores: [],
            distances: [],
            fuelEfficiency: [],
            harshEvents: []
        };
        this.tripStats = null;
    }

    async init() {
        this.setupEventListeners();
        await this.loadRealData();
    }

    setupEventListeners() {
        const performancePeriod = document.getElementById('performancePeriod');
        if (performancePeriod) {
            performancePeriod.addEventListener('change', () => this.updatePerformanceChart());
        }

        const startDate = document.getElementById('reportStartDate');
        const endDate = document.getElementById('reportEndDate');
        
        if (startDate && endDate) {
            const end = new Date();
            const start = new Date();
            start.setDate(start.getDate() - 30);
            
            startDate.valueAsDate = start;
            endDate.valueAsDate = end;
            
            startDate.addEventListener('change', () => this.updateCharts());
            endDate.addEventListener('change', () => this.updateCharts());
        }
    }

    async loadRealData() {
        if (!window.db) {
            console.warn('Database API not available - waiting for initialization');
            return;
        }

        try {
            console.log('📊 Loading real analytics data from database...');
            
            const trips = await window.db.getTripHistory(50);
            const stats = await window.db.getTripStatistics();

            if (trips && trips.length > 0) {
                this.performanceData = {
                    safetyScores: trips.map(t => parseInt(t.safety_score) || 1000),
                    distances: trips.map(t => parseFloat(t.distance) || 0),
                    fuelEfficiency: trips.map(t => {
                        const dist = parseFloat(t.distance) || 0;
                        const fuel = parseFloat(t.fuel_consumed) || 1;
                        return fuel > 0 ? dist / fuel : 0;
                    }),
                    harshEvents: trips.map(t => 
                        (parseInt(t.harsh_braking_count) || 0) + 
                        (parseInt(t.rapid_acceleration_count) || 0) +
                        (parseInt(t.speeding_count) || 0)
                    ),
                    dates: trips.map(t => {
                        if (t.start_time) {
                            return new Date(parseInt(t.start_time) / 1000000);
                        }
                        return new Date();
                    })
                };
                console.log(`✅ Loaded ${trips.length} trips for analytics`);
            } else {
                console.log('ℹ️ No trip data available yet');
            }

            if (stats) {
                this.tripStats = stats;
            }

            if (this.app.currentPage === 'reports') {
                this.initCharts();
            }
        } catch (error) {
            console.error('❌ Failed to load analytics data:', error);
        }
    }

    initCharts() {
        this.initPerformanceChart();
        this.initSafetyChart();
        this.initFuelChart();
        this.initRadarChart();
        this.updateMetrics();
    }

    initPerformanceChart() {
        const canvas = document.getElementById('performanceChart');
        if (!canvas) return;

        const ctx = canvas.getContext('2d');
        
        const labels = this.performanceData.dates?.map(d => 
            d.getDate().toString().padStart(2, '0')
        ) || [];

        this.charts.performance = new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [
                    {
                        label: 'Safety Score',
                        data: this.performanceData.safetyScores,
                        borderColor: '#4361ee',
                        backgroundColor: 'rgba(67, 97, 238, 0.1)',
                        fill: true,
                        tension: 0.4,
                        yAxisID: 'y'
                    },
                    {
                        label: 'Distance (km)',
                        data: this.performanceData.distances,
                        borderColor: '#4cc9f0',
                        backgroundColor: 'rgba(76, 201, 240, 0.1)',
                        fill: true,
                        tension: 0.4,
                        yAxisID: 'y1'
                    },
                    {
                        label: 'Harsh Events',
                        data: this.performanceData.harshEvents,
                        borderColor: '#f72585',
                        backgroundColor: 'rgba(247, 37, 133, 0.1)',
                        fill: true,
                        tension: 0.4,
                        yAxisID: 'y2'
                    }
                ]
            },
            options: {
                responsive: true,
                animation: false,
                interaction: {
                    mode: 'index',
                    intersect: false
                },
                scales: {
                    x: {
                        title: { display: true, text: 'Days' }
                    },
                    y: {
                        type: 'linear',
                        display: true,
                        position: 'left',
                        title: { display: true, text: 'Safety Score' },
                        min: 0,
                        max: 1000
                    },
                    y1: {
                        type: 'linear',
                        display: true,
                        position: 'right',
                        title: { display: true, text: 'Distance (km)' },
                        grid: { drawOnChartArea: false }
                    },
                    y2: {
                        type: 'linear',
                        display: false,
                        position: 'right',
                        grid: { drawOnChartArea: false }
                    }
                },
                plugins: {
                    legend: { position: 'top' },
                    tooltip: { mode: 'index', intersect: false }
                }
            }
        });
    }

    initSafetyChart() {
        const canvas = document.getElementById('safetyChart');
        if (!canvas) return;

        const ctx = canvas.getContext('2d');
        
        const safetyData = this.performanceData.safetyScores.slice(-12);
        const labels = this.performanceData.dates?.slice(-12).map(d => 
            d.toLocaleDateString('en-US', { month: 'short', day: 'numeric' })
        ) || [];

        this.charts.safety = new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels.length > 0 ? labels : ['No Data'],
                datasets: [{
                    label: 'Safety Score',
                    data: safetyData.length > 0 ? safetyData : [0],
                    borderColor: '#4cc9f0',
                    backgroundColor: 'rgba(76, 201, 240, 0.2)',
                    fill: true,
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                animation: false,
                scales: {
                    y: {
                        beginAtZero: false,
                        min: 0,
                        max: 1000,
                        title: { display: true, text: 'Safety Score' }
                    }
                },
                plugins: { legend: { display: false } }
            }
        });

        const currentScoreEl = document.getElementById('currentSafetyScore');
        if (currentScoreEl) {
            const currentScore = safetyData.length > 0 ? safetyData[safetyData.length - 1] : (this.tripStats?.safety_score || 1000);
            currentScoreEl.textContent = Math.round(currentScore);
        }
    }

    initFuelChart() {
        const canvas = document.getElementById('fuelChart');
        if (!canvas) return;

        const ctx = canvas.getContext('2d');
        
        const fuelData = this.performanceData.fuelEfficiency.slice(-8);
        const labels = this.performanceData.dates?.slice(-8).map(d => 
            d.toLocaleDateString('en-US', { month: 'short', day: 'numeric' })
        ) || [];

        this.charts.fuel = new Chart(ctx, {
            type: 'bar',
            data: {
                labels: labels.length > 0 ? labels : ['No Data'],
                datasets: [{
                    label: 'Fuel Efficiency (km/L)',
                    data: fuelData.length > 0 ? fuelData : [0],
                    backgroundColor: 'rgba(76, 201, 240, 0.7)',
                    borderColor: 'rgb(76, 201, 240)',
                    borderWidth: 1
                }]
            },
            options: {
                responsive: true,
                animation: false,
                scales: {
                    y: {
                        beginAtZero: true,
                        title: { display: true, text: 'km per Liter' }
                    }
                },
                plugins: { legend: { display: false } }
            }
        });
    }

    initRadarChart() {
        const canvas = document.getElementById('performanceRadar');
        if (!canvas) return;

        const ctx = canvas.getContext('2d');
        
        const stats = this.tripStats || {};
        const safetyScore = (stats.safety_score || 1000) / 10; 
        const avgDistance = this.performanceData.distances.length > 0 
            ? Math.min(100, (this.performanceData.distances.reduce((a,b) => a+b, 0) / this.performanceData.distances.length) * 2)
            : 0;
        const efficiency = this.performanceData.fuelEfficiency.length > 0
            ? Math.min(100, this.performanceData.fuelEfficiency.reduce((a,b) => a+b, 0) / this.performanceData.fuelEfficiency.length * 8)
            : 0;
        const harshEventsAvg = this.performanceData.harshEvents.length > 0
            ? this.performanceData.harshEvents.reduce((a,b) => a+b, 0) / this.performanceData.harshEvents.length
            : 0;
        const consistency = Math.max(0, 100 - harshEventsAvg * 10);

        this.charts.radar = new Chart(ctx, {
            type: 'radar',
            data: {
                labels: ['Safety', 'Distance', 'Efficiency', 'Consistency', 'Compliance', 'Economy'],
                datasets: [
                    {
                        label: 'Your Performance',
                        data: [safetyScore, avgDistance, efficiency, consistency, safetyScore * 0.9, efficiency * 0.95],
                        backgroundColor: 'rgba(67, 97, 238, 0.2)',
                        borderColor: 'rgba(67, 97, 238, 1)',
                        borderWidth: 2,
                        pointBackgroundColor: 'rgba(67, 97, 238, 1)'
                    }
                ]
            },
            options: {
                responsive: true,
                animation: false,
                scales: {
                    r: {
                        angleLines: { display: true },
                        suggestedMin: 0,
                        suggestedMax: 100,
                        ticks: { stepSize: 20 }
                    }
                },
                plugins: { legend: { position: 'top' } }
            }
        });
    }

    async updatePerformanceChart() {
        if (!this.charts.performance || !window.db) return;

        const period = document.getElementById('performancePeriod')?.value || '30';
        const days = parseInt(period);

        try {
            const trips = await window.db.getTripHistory(days);
            
            const labels = [];
            const safetyData = [];
            const distanceData = [];
            const eventsData = [];
            
            trips.forEach((trip) => {
                const date = trip.start_time ? new Date(parseInt(trip.start_time) / 1000000) : new Date();
                labels.push(date.getDate().toString().padStart(2, '0'));
                safetyData.push(parseInt(trip.safety_score) || 1000);
                distanceData.push(parseFloat(trip.distance) || 0);
                eventsData.push((parseInt(trip.harsh_braking_count) || 0) + (parseInt(trip.rapid_acceleration_count) || 0));
            });
            
            this.charts.performance.data.labels = labels;
            this.charts.performance.data.datasets[0].data = safetyData;
            this.charts.performance.data.datasets[1].data = distanceData;
            this.charts.performance.data.datasets[2].data = eventsData;
            
            this.charts.performance.update('none');
        } catch (error) {
            console.error('Failed to update performance chart:', error);
        }
    }

    async updateMetrics() {
        if (!window.db) return;

        try {
            const stats = await window.db.getTripStatistics();
            if (!stats) return;

            const avgDistance = stats.total_trips > 0 ? stats.total_distance / stats.total_trips : 0;
            const avgSpeed = stats.avg_speed || 0;
            const avgFuelEfficiency = this.performanceData.fuelEfficiency.length > 0
                ? this.performanceData.fuelEfficiency.reduce((a,b) => a+b, 0) / this.performanceData.fuelEfficiency.length
                : 0;
            
            const elements = {
                avgTripDistance: avgDistance.toFixed(1) + ' km',
                averageSpeed: avgSpeed.toFixed(1) + ' km/h',
                fuelEfficiency: avgFuelEfficiency.toFixed(1) + ' km/L'
            };

            Object.entries(elements).forEach(([id, value]) => {
                const el = document.getElementById(id);
                if (el) el.textContent = value;
            });
        } catch (error) {
            console.error('Failed to update metrics:', error);
        }
    }

    async generateReport(type) {
        if (this.app.showLoading) this.app.showLoading();
        
        try {
            let reportData = {};
            
            switch(type) {
                case 'daily':
                    reportData = await this.createDailyReport();
                    break;
                case 'weekly':
                    reportData = await this.createWeeklyReport();
                    break;
                case 'monthly':
                    reportData = await this.createMonthlyReport();
                    break;
                default:
                    reportData = await this.createCustomReport();
            }
            
            this.displayReport(reportData);
        } catch (error) {
            console.error('Failed to generate report:', error);
            if (this.app.showToast) {
                this.app.showToast('Failed to generate report: ' + error.message, 'error');
            }
        } finally {
            if (this.app.hideLoading) this.app.hideLoading();
        }
    }

    async createDailyReport() {
        if (!window.db) return { title: 'Daily Report', summary: { error: 'Database not available' } };
        
        const today = new Date();
        const trips = await window.db.getTripHistory(100);
        const todayTrips = trips.filter(t => {
            if (!t.start_time) return false;
            const tripDate = new Date(parseInt(t.start_time) / 1000000);
            return tripDate.toDateString() === today.toDateString();
        });
        
        const totalDistance = todayTrips.reduce((sum, t) => sum + (parseFloat(t.distance) || 0), 0);
        const totalDuration = todayTrips.reduce((sum, t) => sum + (parseInt(t.duration) || 0), 0);
        const avgSafety = todayTrips.length > 0 
            ? todayTrips.reduce((sum, t) => sum + (parseInt(t.safety_score) || 1000), 0) / todayTrips.length 
            : 1000;
        
        return {
            title: `Daily Report - ${today.toLocaleDateString()}`,
            summary: {
                'Total Trips': todayTrips.length,
                'Total Distance': totalDistance.toFixed(1) + ' km',
                'Total Duration': this.formatDuration(totalDuration),
                'Safety Score': Math.round(avgSafety)
            },
            highlights: todayTrips.length > 0 
                ? [`Completed ${todayTrips.length} trip(s)`, `Covered ${totalDistance.toFixed(1)} km`]
                : ['No trips recorded today']
        };
    }

    async createWeeklyReport() {
        if (!window.db) return { title: 'Weekly Report', summary: { error: 'Database not available' } };
        
        const lastWeek = new Date();
        lastWeek.setDate(lastWeek.getDate() - 7);
        
        const trips = await window.db.getTripHistory(200);
        const stats = await window.db.getTripStatistics();
        
        const weekTrips = trips.filter(t => {
            if (!t.start_time) return false;
            return new Date(parseInt(t.start_time) / 1000000) >= lastWeek;
        });
        
        const totalDistance = weekTrips.reduce((sum, t) => sum + (parseFloat(t.distance) || 0), 0);
        
        return {
            title: `Weekly Report`,
            summary: {
                'Total Trips': weekTrips.length,
                'Total Distance': totalDistance.toFixed(1) + ' km',
                'Safety Score': stats?.safety_score || 1000
            },
            trends: [`${weekTrips.length} trips this week`, `${totalDistance.toFixed(1)} km traveled`]
        };
    }

    async createMonthlyReport() {
        if (!window.db) return { title: 'Monthly Report', summary: { error: 'Database not available' } };
        
        const stats = await window.db.getTripStatistics();
        
        return {
            title: `Monthly Report - ${new Date().toLocaleDateString('en-US', { month: 'long', year: 'numeric' })}`,
            summary: {
                'Total Trips': stats?.total_trips || 0,
                'Total Distance': (stats?.total_distance || 0).toFixed(1) + ' km',
                'Safety Score': stats?.safety_score || 1000,
                'Average Speed': (stats?.avg_speed || 0).toFixed(1) + ' km/h'
            },
            achievements: [
                `Completed ${stats?.total_trips || 0} trips`,
                `Total distance: ${(stats?.total_distance || 0).toFixed(1)} km`
            ]
        };
    }

    async createCustomReport() {
        if (!window.db) return { title: 'Custom Report', summary: {} };
        const stats = await window.db.getTripStatistics();
        return {
            title: `Custom Report - ${new Date().toLocaleDateString()}`,
            summary: {
                'Trips': stats?.total_trips || 0,
                'Distance': (stats?.total_distance || 0).toFixed(1) + ' km',
                'Safety': stats?.safety_score || 1000
            }
        };
    }

    displayReport(reportData) {
        const modal = document.createElement('div');
        modal.className = 'modal active';
        modal.style.display = 'flex';
        modal.id = 'reportModal';
        
        modal.innerHTML = `
            <div class="modal-content" style="max-width: 600px;">
                <div class="modal-header">
                    <h3>${reportData.title}</h3>
                    <button class="close" onclick="document.getElementById('reportModal').remove()">&times;</button>
                </div>
                <div class="modal-body">
                    <div class="report-summary">
                        <h4>Summary</h4>
                        ${Object.entries(reportData.summary).map(([key, value]) => `
                            <p><strong>${key}:</strong> ${value}</p>
                        `).join('')}
                    </div>
                    ${reportData.highlights ? `
                        <div class="report-section">
                            <h4>Highlights</h4>
                            <ul>${reportData.highlights.map(h => `<li>${h}</li>`).join('')}</ul>
                        </div>
                    ` : ''}
                    ${reportData.trends ? `
                        <div class="report-section">
                            <h4>Trends</h4>
                            <ul>${reportData.trends.map(t => `<li>${t}</li>`).join('')}</ul>
                        </div>
                    ` : ''}
                    ${reportData.achievements ? `
                        <div class="report-section">
                            <h4>Achievements</h4>
                            <ul>${reportData.achievements.map(a => `<li>${a}</li>`).join('')}</ul>
                        </div>
                    ` : ''}
                </div>
                <div class="modal-footer">
                    <button class="btn btn-secondary" onclick="document.getElementById('reportModal').remove()">Close</button>
                </div>
            </div>
        `;
        
        document.body.appendChild(modal);
    }

    formatDuration(seconds) {
        if (!seconds || seconds <= 0) return '0m';
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        if (hours > 0) return `${hours}h ${minutes}m`;
        return `${minutes}m`;
    }

    async updateCharts() {
        if (this.app.currentPage === 'reports') {
            await this.loadRealData();
            await this.updatePerformanceChart();
            await this.updateMetrics();
        }
    }

    async exportReport(name = 'Report') {
        try {
            if (this.app.showLoading) this.app.showLoading();
            
            const stats = await window.db?.getTripStatistics() || {};
            const trips = await window.db?.getTripHistory(100) || [];
            const expenses = await window.db?.getExpenseSummary() || {};
            
            const csvContent = this.generateCSV(stats, trips, expenses);
            
            const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
            const url = URL.createObjectURL(blob);
            const link = document.createElement('a');
            link.href = url;
            link.download = `SmartDrive_${name}_${new Date().toISOString().split('T')[0]}.csv`;
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
            URL.revokeObjectURL(url);
            
            if (this.app.showToast) {
                this.app.showToast('Report exported successfully!', 'success');
            }
        } catch (error) {
            console.error('Export failed:', error);
            if (this.app.showToast) {
                this.app.showToast('Export failed: ' + error.message, 'error');
            }
        } finally {
            if (this.app.hideLoading) this.app.hideLoading();
        }
    }

    generateCSV(stats, trips, expenses) {
        let csv = 'Smart Drive Manager - Data Export\n';
        csv += `Generated: ${new Date().toLocaleString()}\n\n`;
        
        csv += 'STATISTICS\n';
        csv += 'Metric,Value\n';
        csv += `Total Trips,${stats.total_trips || 0}\n`;
        csv += `Total Distance,${(stats.total_distance || 0).toFixed(2)} km\n`;
        csv += `Average Speed,${(stats.avg_speed || 0).toFixed(2)} km/h\n`;
        csv += `Safety Score,${stats.safety_score || 1000}\n`;
        csv += `Total Expenses,$${(expenses.total_expenses || 0).toFixed(2)}\n\n`;
        
        if (trips.length > 0) {
            csv += 'TRIP HISTORY\n';
            csv += 'Trip ID,Date,Distance (km),Duration,Avg Speed (km/h),Safety Score\n';
            trips.forEach(trip => {
                const date = trip.start_time 
                    ? new Date(parseInt(trip.start_time) / 1000000).toLocaleDateString() 
                    : 'N/A';
                csv += `${trip.trip_id || 'N/A'},${date},${(parseFloat(trip.distance) || 0).toFixed(2)},`;
                csv += `${this.formatDuration(trip.duration || 0)},${(parseFloat(trip.avg_speed) || 0).toFixed(1)},`;
                csv += `${trip.safety_score || 'N/A'}\n`;
            });
        }
        
        return csv;
    }
}

window.analytics = null;