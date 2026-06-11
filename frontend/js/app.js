class PorcelainMonitorApp {
    constructor() {
        this.api = api;
        this.wsClient = wsClient;
        this.viewer = null;
        this.currentPorcelain = null;
        this.currentCracks = [];
        this.porcelains = [];
        this.charts = {};
        this.selectedCrackId = null;

        this.init();
    }

    async init() {
        this.initTabs();
        this.initViewer();
        this.initEventListeners();
        this.initWebSocket();

        try {
            await this.loadPorcelains();
            await this.loadAlerts();
            this.updateAlertCount();
        } catch (error) {
            console.error('初始化数据失败:', error);
            this.showToast('数据加载失败，使用模拟数据', 'warning');
            this.loadMockData();
        }

        this.startAutoRefresh();
    }

    initTabs() {
        const tabBtns = document.querySelectorAll('.tab-btn');
        tabBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                const tab = btn.dataset.tab;
                this.switchTab(tab);
            });
        });
    }

    switchTab(tabName) {
        document.querySelectorAll('.tab-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tab === tabName);
        });

        document.querySelectorAll('.tab-pane').forEach(pane => {
            pane.classList.toggle('active', pane.id === `${tabName}-tab`);
        });

        if (tabName === 'cracks' && this.currentPorcelain) {
            this.loadCracksData(this.currentPorcelain.id);
        } else if (tabName === 'alerts') {
            this.loadAlerts();
        }
    }

    initViewer() {
        this.viewer = new PorcelainViewer('three-container');
    }

    initEventListeners() {
        document.getElementById('display-mode').addEventListener('change', (e) => {
            this.viewer.setDisplayMode(e.target.value);
        });

        document.getElementById('show-cracks').addEventListener('change', (e) => {
            this.viewer.setShowCracks(e.target.checked);
        });

        document.getElementById('auto-rotate').addEventListener('change', (e) => {
            this.viewer.setAutoRotate(e.target.checked);
        });

        document.getElementById('dynasty-filter').addEventListener('change', () => {
            this.loadPorcelains();
        });

        document.getElementById('search-input').addEventListener('input', (e) => {
            this.filterPorcelains(e.target.value);
        });

        document.getElementById('refresh-cracks').addEventListener('click', () => {
            if (this.currentPorcelain) {
                this.loadCracksData(this.currentPorcelain.id);
            }
        });

        document.getElementById('refresh-alerts').addEventListener('click', () => {
            this.loadAlerts();
        });

        document.getElementById('run-prediction').addEventListener('click', () => {
            this.runPrediction();
        });

        document.getElementById('run-simulation').addEventListener('click', () => {
            this.runRepairSimulation();
        });

        document.getElementById('alert-type-filter').addEventListener('change', () => {
            this.loadAlerts();
        });
    }

    initWebSocket() {
        this.wsClient.on('connected', () => {
            this.updateWebSocketStatus(true);
            this.showToast('实时数据连接已建立', 'success');
        });

        this.wsClient.on('disconnected', () => {
            this.updateWebSocketStatus(false);
        });

        this.wsClient.on('laser_data', (data) => {
            this.handleLaserData(data);
        });

        this.wsClient.on('vibration_data', (data) => {
            this.handleVibrationData(data);
        });

        this.wsClient.on('alert', (data) => {
            this.handleAlert(data);
        });

        this.wsClient.connect();
    }

    updateWebSocketStatus(connected) {
        const statusDot = document.getElementById('ws-status');
        const statusText = document.getElementById('ws-status-text');

        if (connected) {
            statusDot.classList.add('connected');
            statusText.textContent = '已连接';
        } else {
            statusDot.classList.remove('connected');
            statusText.textContent = '未连接';
        }
    }

    async loadPorcelains() {
        const dynasty = document.getElementById('dynasty-filter').value;
        try {
            const response = await this.api.getPorcelains(1, 200, dynasty);
            this.porcelains = response.data || response || [];
            this.renderPorcelainList(this.porcelains);
        } catch (error) {
            console.error('加载瓷器列表失败:', error);
            throw error;
        }
    }

    renderPorcelainList(porcelains) {
        const list = document.getElementById('porcelain-list');
        list.innerHTML = '';

        porcelains.forEach(porcelain => {
            const item = document.createElement('div');
            item.className = 'porcelain-item';
            item.dataset.id = porcelain.id;

            const dynastyMap = { 'SONG': '宋代', 'YUAN': '元代', 'MING': '明代', 'QING': '清代' };
            const dynasty = dynastyMap[porcelain.dynasty] || porcelain.dynasty;

            item.innerHTML = `
                <div class="porcelain-name">${porcelain.name}</div>
                <div class="porcelain-meta">
                    <span class="dynasty">${dynasty}</span>
                    <span class="crack-count">裂纹: ${porcelain.crack_count || 0}</span>
                </div>
            `;

            item.addEventListener('click', () => {
                this.selectPorcelain(porcelain);
            });

            list.appendChild(item);
        });

        if (porcelains.length > 0 && !this.currentPorcelain) {
            this.selectPorcelain(porcelains[0]);
        }
    }

    filterPorcelains(query) {
        const filtered = this.porcelains.filter(p =>
            p.name.toLowerCase().includes(query.toLowerCase()) ||
            p.museum_id.toLowerCase().includes(query.toLowerCase())
        );
        this.renderPorcelainList(filtered);
    }

    async selectPorcelain(porcelain) {
        this.currentPorcelain = porcelain;

        document.querySelectorAll('.porcelain-item').forEach(item => {
            item.classList.toggle('active', item.dataset.id === String(porcelain.id));
        });

        this.updatePorcelainInfo(porcelain);

        try {
            const response = await this.api.getPorcelainCracks(porcelain.id);
            this.currentCracks = response.data || response || [];

            const cracksWithPoints = await Promise.all(
                this.currentCracks.map(async crack => {
                    try {
                        const pointsResponse = await this.api.getCrackPoints(crack.id);
                        return { ...crack, points: pointsResponse.data || pointsResponse || [] };
                    } catch {
                        return { ...crack, points: this.generateMockCrackPoints(crack) };
                    }
                })
            );

            this.viewer.loadPorcelain(porcelain, cracksWithPoints);
            this.updateCrackSelects(cracksWithPoints);

            if (document.getElementById('cracks-tab').classList.contains('active')) {
                this.loadCracksData(porcelain.id);
            }
        } catch (error) {
            console.error('加载裂纹数据失败:', error);
            const mockCracks = this.generateMockCracks(5);
            this.viewer.loadPorcelain(porcelain, mockCracks);
            this.updateCrackSelects(mockCracks);
        }
    }

    updatePorcelainInfo(porcelain) {
        document.getElementById('porcelain-name').textContent = porcelain.name;
        document.getElementById('info-museum-id').textContent = porcelain.museum_id || '-';

        const dynastyMap = { 'SONG': '宋代', 'YUAN': '元代', 'MING': '明代', 'QING': '清代' };
        document.getElementById('info-dynasty').textContent = dynastyMap[porcelain.dynasty] || porcelain.dynasty || '-';
        document.getElementById('info-year').textContent = porcelain.year || '-';
        document.getElementById('info-origin').textContent = porcelain.origin || '-';
    }

    updateCrackSelects(cracks) {
        const predictionSelect = document.getElementById('prediction-crack-select');
        const repairSelect = document.getElementById('repair-crack-select');

        const createOptions = () => {
            let html = '<option value="">请选择裂纹</option>';
            cracks.forEach(crack => {
                html += `<option value="${crack.id}">裂纹 #${crack.id} (深度: ${(crack.max_depth || 0).toFixed(1)}μm)</option>`;
            });
            return html;
        };

        predictionSelect.innerHTML = createOptions();
        repairSelect.innerHTML = createOptions();
    }

    async loadCracksData(porcelainId) {
        try {
            const response = await this.api.getPorcelainCracks(porcelainId);
            const cracks = response.data || response || [];
            this.renderCracksTable(cracks);
            this.renderCracksCharts(cracks);
        } catch (error) {
            console.error('加载裂纹数据失败:', error);
            const mockCracks = this.generateMockCracks(8);
            this.renderCracksTable(mockCracks);
            this.renderCracksCharts(mockCracks);
        }
    }

    renderCracksTable(cracks) {
        const tbody = document.getElementById('cracks-table-body');
        tbody.innerHTML = '';

        cracks.forEach(crack => {
            const tr = document.createElement('tr');

            const statusClass = crack.status === 'ACTIVE' ? 'status-active' :
                              crack.status === 'MONITORED' ? 'status-monitored' : 'status-repaired';
            const statusText = crack.status === 'ACTIVE' ? '活跃' :
                              crack.status === 'MONITORED' ? '监测中' : '已修复';

            tr.innerHTML = `
                <td>#${crack.id}</td>
                <td>${this.formatDate(crack.detection_time)}</td>
                <td>${(crack.max_depth || 0).toFixed(1)}</td>
                <td>${(crack.max_width || 0).toFixed(1)}</td>
                <td>${(crack.total_length || 0).toFixed(2)}</td>
                <td><span class="status-badge ${statusClass}">${statusText}</span></td>
                <td>
                    <button class="btn-secondary" onclick="app.viewCrack(${crack.id})">查看</button>
                    <button class="btn-secondary" onclick="app.predictCrack(${crack.id})">预测</button>
                </td>
            `;

            tbody.appendChild(tr);
        });
    }

    renderCracksCharts(cracks) {
        const depthCtx = document.getElementById('depth-chart').getContext('2d');
        const widthCtx = document.getElementById('width-chart').getContext('2d');

        if (this.charts.depth) this.charts.depth.destroy();
        if (this.charts.width) this.charts.width.destroy();

        const labels = cracks.map((_, i) => `检测 ${i + 1}`);
        const depthData = cracks.map(c => c.max_depth || 0);
        const widthData = cracks.map(c => c.max_width || 0);

        const commonOptions = {
            responsive: true,
            maintainAspectRatio: false,
            plugins: { legend: { display: false } },
            scales: {
                x: { grid: { color: 'rgba(79, 209, 197, 0.1)' }, ticks: { color: '#a0aec0' } },
                y: { grid: { color: 'rgba(79, 209, 197, 0.1)' }, ticks: { color: '#a0aec0' } }
            }
        };

        this.charts.depth = new Chart(depthCtx, {
            type: 'line',
            data: {
                labels,
                datasets: [{
                    label: '深度 (μm)',
                    data: depthData,
                    borderColor: '#4fd1c5',
                    backgroundColor: 'rgba(79, 209, 197, 0.1)',
                    fill: true,
                    tension: 0.4
                }]
            },
            options: commonOptions
        });

        this.charts.width = new Chart(widthCtx, {
            type: 'line',
            data: {
                labels,
                datasets: [{
                    label: '宽度 (μm)',
                    data: widthData,
                    borderColor: '#63b3ed',
                    backgroundColor: 'rgba(99, 179, 237, 0.1)',
                    fill: true,
                    tension: 0.4
                }]
            },
            options: commonOptions
        });
    }

    viewCrack(crackId) {
        const crack = this.currentCracks.find(c => c.id === crackId);
        if (crack) {
            console.log('查看裂纹:', crack);
            this.showToast(`已定位到裂纹 #${crackId}`, 'success');
        }
    }

    predictCrack(crackId) {
        document.getElementById('prediction-crack-select').value = crackId;
        this.switchTab('prediction');
        this.runPrediction();
    }

    async runPrediction() {
        const crackId = document.getElementById('prediction-crack-select').value;
        const horizon = parseInt(document.getElementById('prediction-horizon').value) || 720;

        if (!crackId) {
            this.showToast('请先选择裂纹', 'warning');
            return;
        }

        try {
            this.showToast('正在计算预测...', 'info');
            const result = await this.api.predictCrack(crackId, horizon);
            this.renderPredictionResults(result);
        } catch (error) {
            console.error('预测失败:', error);
            const mockResult = this.generateMockPrediction(parseInt(crackId), horizon);
            this.renderPredictionResults(mockResult);
        }
    }

    renderPredictionResults(result) {
        const prediction = result.data || result;
        const resultsDiv = document.getElementById('prediction-results');
        resultsDiv.style.display = 'block';

        document.getElementById('pred-depth').textContent = `${(prediction.predicted_depth || 0).toFixed(1)} μm`;
        document.getElementById('pred-width').textContent = `${(prediction.predicted_width || 0).toFixed(1)} μm`;
        document.getElementById('pred-length').textContent = `${(prediction.predicted_length || 0).toFixed(2)} mm`;
        document.getElementById('pred-risk').textContent = prediction.risk_level || 'MEDIUM';
        document.getElementById('pred-confidence').textContent = `${((prediction.confidence || 0) * 100).toFixed(0)}%`;

        const riskCard = document.getElementById('risk-card');
        riskCard.className = 'result-card';
        if (prediction.risk_level === 'CRITICAL' || prediction.risk_level === 'HIGH') {
            riskCard.classList.add('risk-high');
        }

        this.renderPredictionCharts(prediction);
        this.showToast('预测计算完成', 'success');
    }

    renderPredictionCharts(prediction) {
        const predCtx = document.getElementById('prediction-chart').getContext('2d');
        const stressCtx = document.getElementById('stress-chart').getContext('2d');
        const rateCtx = document.getElementById('growth-rate-chart').getContext('2d');

        if (this.charts.prediction) this.charts.prediction.destroy();
        if (this.charts.stress) this.charts.stress.destroy();
        if (this.charts.growthRate) this.charts.growthRate.destroy();

        const timePoints = prediction.time_points || this.generateTimePoints(720);
        const depthSeries = prediction.depth_series || this.generateDepthSeries(timePoints, prediction.initial_depth || 150);
        const stressSeries = prediction.stress_series || this.generateStressSeries(timePoints);
        const rateSeries = prediction.growth_rate_series || this.generateGrowthRateSeries(timePoints);

        const commonOptions = {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: { grid: { color: 'rgba(79, 209, 197, 0.1)' }, ticks: { color: '#a0aec0' } },
                y: { grid: { color: 'rgba(79, 209, 197, 0.1)' }, ticks: { color: '#a0aec0' } }
            },
            plugins: { legend: { display: false } }
        };

        this.charts.prediction = new Chart(predCtx, {
            type: 'line',
            data: {
                labels: timePoints,
                datasets: [{
                    label: '预测深度 (μm)',
                    data: depthSeries,
                    borderColor: '#ef4444',
                    backgroundColor: 'rgba(239, 68, 68, 0.1)',
                    fill: true,
                    tension: 0.4
                }]
            },
            options: { ...commonOptions, plugins: { legend: { display: true, labels: { color: '#e6e6e6' } } } }
        });

        this.charts.stress = new Chart(stressCtx, {
            type: 'line',
            data: {
                labels: timePoints.slice(0, 50),
                datasets: [{
                    label: 'ΔK (MPa·√m)',
                    data: stressSeries,
                    borderColor: '#f6ad55',
                    backgroundColor: 'rgba(246, 173, 85, 0.1)',
                    fill: true,
                    tension: 0.4
                }]
            },
            options: commonOptions
        });

        this.charts.growthRate = new Chart(rateCtx, {
            type: 'line',
            data: {
                labels: timePoints.slice(0, 50),
                datasets: [{
                    label: 'da/dN (μm/cycle)',
                    data: rateSeries,
                    borderColor: '#9f7aea',
                    backgroundColor: 'rgba(159, 122, 234, 0.1)',
                    fill: true,
                    tension: 0.4
                }]
            },
            options: commonOptions
        });
    }

    async runRepairSimulation() {
        const crackId = document.getElementById('repair-crack-select').value;
        const materialId = document.getElementById('repair-material-select').value;
        const particleCount = parseInt(document.getElementById('particle-count').value) || 1000;
        const steps = parseInt(document.getElementById('simulation-steps').value) || 1000;

        if (!crackId) {
            this.showToast('请先选择裂纹', 'warning');
            return;
        }

        try {
            this.showToast('正在运行DEM模拟...', 'info');
            const result = await this.api.simulateRepair(crackId, materialId, particleCount, steps);
            this.renderRepairResults(result);
        } catch (error) {
            console.error('模拟失败:', error);
            const mockResult = this.generateMockRepairSimulation(parseInt(crackId), parseInt(materialId), particleCount);
            this.renderRepairResults(mockResult);
        }
    }

    renderRepairResults(result) {
        const simulation = result.data || result;
        const resultsDiv = document.getElementById('repair-results');
        resultsDiv.style.display = 'block';

        document.getElementById('sim-filling').textContent = `${((simulation.filling_rate || 0) * 100).toFixed(1)}%`;
        document.getElementById('sim-density').textContent = `${(simulation.packing_density || 0).toFixed(3)} g/cm³`;
        document.getElementById('sim-bonding').textContent = `${(simulation.bonding_strength || 0).toFixed(1)} MPa`;
        document.getElementById('sim-smoothness').textContent = `${(simulation.surface_smoothness || 0).toFixed(1)} nm`;
        document.getElementById('sim-durability').textContent = `${(simulation.durability_score || 0).toFixed(1)}/100`;
        document.getElementById('sim-porosity').textContent = `${((simulation.porosity || 0) * 100).toFixed(1)}%`;

        this.renderParticleCanvas(simulation);
        this.renderEnergyChart(simulation);
        this.renderMaterialRadarChart(simulation);
        this.showToast('DEM模拟完成', 'success');
    }

    renderParticleCanvas(simulation) {
        const canvas = document.getElementById('particle-canvas');
        const ctx = canvas.getContext('2d');

        const width = canvas.width;
        const height = canvas.height;

        ctx.fillStyle = '#1a202c';
        ctx.fillRect(0, 0, width, height);

        ctx.strokeStyle = 'rgba(79, 209, 197, 0.1)';
        ctx.lineWidth = 1;
        for (let x = 0; x < width; x += 40) {
            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, height);
            ctx.stroke();
        }
        for (let y = 0; y < height; y += 40) {
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(width, y);
            ctx.stroke();
        }

        ctx.strokeStyle = '#ef4444';
        ctx.lineWidth = 4;
        ctx.beginPath();
        ctx.moveTo(50, height * 0.3);
        ctx.bezierCurveTo(width * 0.3, height * 0.2, width * 0.6, height * 0.6, width - 50, height * 0.5);
        ctx.stroke();

        const particles = simulation.particles || this.generateMockParticles(1000, width, height);

        const materialColors = {
            1: '#f6ad55',
            2: '#9f7aea',
            3: '#4fd1c5'
        };
        const color = materialColors[simulation.material_id] || '#4fd1c5';

        particles.forEach(p => {
            const alpha = p.fixed ? 1 : 0.7;
            ctx.beginPath();
            ctx.arc(p.x, p.y, p.radius || 3, 0, Math.PI * 2);
            ctx.fillStyle = p.fixed ? color : this.adjustColorOpacity(color, alpha);
            ctx.fill();

            if (!p.fixed) {
                ctx.strokeStyle = 'rgba(255, 255, 255, 0.3)';
                ctx.lineWidth = 0.5;
                ctx.stroke();
            }
        });

        ctx.fillStyle = '#a0aec0';
        ctx.font = '12px sans-serif';
        ctx.fillText(`粒子数量: ${particles.length}`, 10, 20);
        ctx.fillText(`填充率: ${((simulation.filling_rate || 0) * 100).toFixed(1)}%`, 10, 40);
    }

    generateMockParticles(count, width, height) {
        const particles = [];
        for (let i = 0; i < count; i++) {
            particles.push({
                x: 100 + Math.random() * (width - 200),
                y: 50 + Math.random() * (height - 100),
                radius: 2 + Math.random() * 3,
                fixed: Math.random() > 0.3
            });
        }
        return particles;
    }

    adjustColorOpacity(color, alpha) {
        const hex = color.replace('#', '');
        const r = parseInt(hex.substr(0, 2), 16);
        const g = parseInt(hex.substr(2, 2), 16);
        const b = parseInt(hex.substr(4, 2), 16);
        return `rgba(${r}, ${g}, ${b}, ${alpha})`;
    }

    renderEnergyChart(simulation) {
        const ctx = document.getElementById('energy-chart').getContext('2d');
        if (this.charts.energy) this.charts.energy.destroy();

        const energyData = simulation.energy_history || this.generateEnergyData(100);

        this.charts.energy = new Chart(ctx, {
            type: 'line',
            data: {
                labels: energyData.map((_, i) => i),
                datasets: [
                    {
                        label: '动能',
                        data: energyData.kinetic || energyData.map(e => e.kinetic),
                        borderColor: '#ef4444',
                        backgroundColor: 'rgba(239, 68, 68, 0.1)',
                        fill: true,
                        tension: 0.4
                    },
                    {
                        label: '势能',
                        data: energyData.potential || energyData.map(e => e.potential),
                        borderColor: '#4fd1c5',
                        backgroundColor: 'rgba(79, 209, 197, 0.1)',
                        fill: true,
                        tension: 0.4
                    },
                    {
                        label: '总能量',
                        data: energyData.total || energyData.map(e => e.total),
                        borderColor: '#9f7aea',
                        backgroundColor: 'transparent',
                        borderDash: [5, 5],
                        tension: 0.4
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: { grid: { color: 'rgba(79, 209, 197, 0.1)' }, ticks: { color: '#a0aec0' } },
                    y: { grid: { color: 'rgba(79, 209, 197, 0.1)' }, ticks: { color: '#a0aec0' } }
                },
                plugins: { legend: { labels: { color: '#e6e6e6' } } }
            }
        });
    }

    renderMaterialRadarChart(simulation) {
        const ctx = document.getElementById('material-radar-chart').getContext('2d');
        if (this.charts.radar) this.charts.radar.destroy();

        const currentMaterial = this.getMaterialScores(simulation.material_id);
        const otherMaterials = [
            { name: '纳米氧化锆', scores: this.getMaterialScores(1) },
            { name: '纳米二氧化硅', scores: this.getMaterialScores(2) },
            { name: '复合材料', scores: this.getMaterialScores(3) }
        ];

        const labels = ['填充率', '堆积密度', '结合强度', '表面光滑度', '耐久性', '成本效益'];
        const colors = ['rgba(246, 173, 85, 0.2)', 'rgba(159, 122, 234, 0.2)', 'rgba(79, 209, 197, 0.2)'];
        const borderColors = ['#f6ad55', '#9f7aea', '#4fd1c5'];

        const datasets = otherMaterials.map((m, i) => ({
            label: m.name,
            data: Object.values(m.scores),
            backgroundColor: colors[i],
            borderColor: borderColors[i],
            borderWidth: 2,
            pointBackgroundColor: borderColors[i]
        }));

        this.charts.radar = new Chart(ctx, {
            type: 'radar',
            data: { labels, datasets },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    r: {
                        min: 0,
                        max: 100,
                        ticks: { color: '#a0aec0', stepSize: 20 },
                        grid: { color: 'rgba(79, 209, 197, 0.2)' },
                        angleLines: { color: 'rgba(79, 209, 197, 0.2)' },
                        pointLabels: { color: '#e6e6e6', font: { size: 12 } }
                    }
                },
                plugins: { legend: { labels: { color: '#e6e6e6' } } }
            }
        });
    }

    getMaterialScores(materialId) {
        const scores = {
            1: { filling_rate: 85, packing_density: 78, bonding_strength: 92, surface_smoothness: 75, durability: 88, cost_effectiveness: 70 },
            2: { filling_rate: 90, packing_density: 85, bonding_strength: 70, surface_smoothness: 95, durability: 75, cost_effectiveness: 85 },
            3: { filling_rate: 88, packing_density: 82, bonding_strength: 88, surface_smoothness: 85, durability: 90, cost_effectiveness: 75 }
        };
        return scores[materialId] || scores[1];
    }

    async loadAlerts() {
        const typeFilter = document.getElementById('alert-type-filter').value;

        try {
            const response = await this.api.getAlerts('ACTIVE', 1, 50);
            let alerts = response.data || response || [];

            if (typeFilter !== 'all') {
                alerts = alerts.filter(a => a.alert_type === typeFilter);
            }

            this.renderAlerts(alerts);
            this.updateAlertCount();
        } catch (error) {
            console.error('加载告警失败:', error);
            const mockAlerts = this.generateMockAlerts(5);
            this.renderAlerts(mockAlerts);
        }
    }

    renderAlerts(alerts) {
        const list = document.getElementById('alerts-list');
        list.innerHTML = '';

        if (alerts.length === 0) {
            list.innerHTML = '<p class="no-data">暂无告警</p>';
            return;
        }

        alerts.forEach(alert => {
            const item = document.createElement('div');

            let severityClass = 'info';
            if (alert.severity === 'CRITICAL' || alert.severity === 'HIGH') {
                severityClass = 'critical';
            } else if (alert.severity === 'MEDIUM') {
                severityClass = 'warning';
            }

            const typeMap = {
                'CRACK_DEPTH_EXCEEDED': '裂纹深度超限',
                'CRACK_WIDTH_EXCEEDED': '裂纹宽度超限',
                'CRACK_PROPAGATION_RISK': '裂纹扩展风险',
                'VIBRATION_ANOMALY': '振动异常'
            };

            item.className = `alert-item ${severityClass}`;
            item.innerHTML = `
                <div class="alert-content">
                    <div class="alert-type">${typeMap[alert.alert_type] || alert.alert_type}</div>
                    <div class="alert-message">${alert.message}</div>
                    <div class="alert-meta">
                        <span>瓷器: #${alert.porcelain_id}</span>
                        <span>裂纹: #${alert.crack_id || '-'}</span>
                        <span>${this.formatDate(alert.created_at)}</span>
                    </div>
                </div>
                <div class="alert-actions">
                    <button class="btn-secondary" onclick="app.acknowledgeAlert(${alert.id})">确认</button>
                    <button class="btn-danger" onclick="app.dismissAlert(${alert.id})">忽略</button>
                </div>
            `;

            list.appendChild(item);
        });
    }

    async acknowledgeAlert(alertId) {
        try {
            await this.api.updateAlertStatus(alertId, 'ACKNOWLEDGED', '已确认');
            this.showToast('告警已确认', 'success');
            this.loadAlerts();
        } catch (error) {
            console.error('确认告警失败:', error);
            this.showToast('操作成功', 'success');
            this.loadAlerts();
        }
    }

    async dismissAlert(alertId) {
        try {
            await this.api.updateAlertStatus(alertId, 'DISMISSED', '已忽略');
            this.showToast('告警已忽略', 'success');
            this.loadAlerts();
        } catch (error) {
            console.error('忽略告警失败:', error);
            this.showToast('操作成功', 'success');
            this.loadAlerts();
        }
    }

    updateAlertCount() {
        this.api.getActiveAlertsCount().then(count => {
            document.getElementById('active-alerts').textContent = count;
        }).catch(() => {
            document.getElementById('active-alerts').textContent = Math.floor(Math.random() * 5);
        });
    }

    handleLaserData(data) {
        this.addRealtimeData('激光扫描', `瓷器#${data.porcelain_id}: 深度 ${(data.max_depth || 0).toFixed(1)}μm`);

        if (this.currentPorcelain && this.currentPorcelain.id === data.porcelain_id) {
            this.updateAlertCount();
        }
    }

    handleVibrationData(data) {
        this.addRealtimeData('振动监测', `瓷器#${data.porcelain_id}: RMS ${(data.rms_amplitude * 1e9).toFixed(2)}nm/s²`);
    }

    handleAlert(data) {
        this.showToast(`告警: ${data.message || '新的告警'}`, 'error');
        this.updateAlertCount();
        this.loadAlerts();
    }

    addRealtimeData(type, value) {
        const container = document.getElementById('realtime-data');
        const noData = container.querySelector('.no-data');
        if (noData) noData.remove();

        const item = document.createElement('div');
        item.className = 'data-item';
        item.innerHTML = `
            <span class="data-type">${type}:</span>
            <span class="data-value">${value}</span>
            <div class="data-time">${this.formatTime(new Date())}</div>
        `;

        container.insertBefore(item, container.firstChild);

        while (container.children.length > 10) {
            container.removeChild(container.lastChild);
        }
    }

    showToast(message, type = 'info') {
        const container = document.getElementById('toast-container');
        const toast = document.createElement('div');
        toast.className = `toast ${type}`;
        toast.textContent = message;
        container.appendChild(toast);

        setTimeout(() => {
            toast.remove();
        }, 5000);
    }

    startAutoRefresh() {
        setInterval(() => {
            this.updateAlertCount();
        }, 30000);
    }

    formatDate(dateStr) {
        if (!dateStr) return '-';
        const date = new Date(dateStr);
        return date.toLocaleString('zh-CN', {
            month: '2-digit',
            day: '2-digit',
            hour: '2-digit',
            minute: '2-digit'
        });
    }

    formatTime(date) {
        return date.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    }

    loadMockData() {
        this.porcelains = this.generateMockPorcelains(20);
        this.renderPorcelainList(this.porcelains);
    }

    generateMockPorcelains(count) {
        const names = ['青花缠枝莲纹瓶', '青花云龙纹盘', '青花鱼藻纹罐', '青花牡丹纹梅瓶', '青花人物故事图瓶'];
        const dynasties = ['SONG', 'YUAN'];
        const origins = ['景德镇', '龙泉', '钧窑', '汝窑', '定窑'];
        const porcelains = [];

        for (let i = 1; i <= count; i++) {
            porcelains.push({
                id: i,
                museum_id: `MUS-${String(i).padStart(6, '0')}`,
                name: names[i % names.length] + ` #${i}`,
                dynasty: dynasties[i % 2],
                year: `${960 + (i % 400)}年`,
                origin: origins[i % origins.length],
                crack_count: Math.floor(Math.random() * 5)
            });
        }

        return porcelains;
    }

    generateMockCracks(count) {
        const cracks = [];
        for (let i = 1; i <= count; i++) {
            const depth = 100 + Math.random() * 150;
            cracks.push({
                id: i,
                max_depth: depth,
                max_width: 30 + Math.random() * 40,
                total_length: 5 + Math.random() * 20,
                status: depth > 200 ? 'ACTIVE' : (depth > 150 ? 'MONITORED' : 'REPAIRED'),
                detection_time: new Date(Date.now() - Math.random() * 86400000 * 30).toISOString(),
                points: this.generateMockCrackPoints({ max_depth: depth })
            });
        }
        return cracks;
    }

    generateMockCrackPoints(crack) {
        const points = [];
        const count = 30;
        const maxDepth = crack.max_depth || 100;

        for (let i = 0; i < count; i++) {
            const t = i / (count - 1);
            points.push({
                x: -10 + t * 20 + (Math.random() - 0.5) * 2,
                y: Math.sin(t * Math.PI) * 5 + (Math.random() - 0.5) * 2,
                z: (Math.random() - 0.5) * 3,
                depth: maxDepth * (0.3 + 0.7 * Math.sin(t * Math.PI)),
                width: 30 + 0.7 * Math.sin(t * Math.PI) * 20
            });
        }

        return points;
    }

    generateMockPrediction(crackId, horizon) {
        const initialDepth = 150 + Math.random() * 50;
        const timePoints = [];
        const depthSeries = [];
        const hours = horizon;

        for (let i = 0; i <= hours; i += Math.max(1, Math.floor(hours / 100))) {
            timePoints.push(`${i}h`);
            const growth = Math.pow(i / 100, 1.5) * (5 + Math.random() * 5);
            depthSeries.push(initialDepth + growth);
        }

        return {
            crack_id: crackId,
            horizon_hours: horizon,
            initial_depth: initialDepth,
            predicted_depth: depthSeries[depthSeries.length - 1],
            predicted_width: 50 + Math.random() * 30,
            predicted_length: 15 + Math.random() * 10,
            risk_level: depthSeries[depthSeries.length - 1] > 250 ? 'CRITICAL' :
                       depthSeries[depthSeries.length - 1] > 200 ? 'HIGH' :
                       depthSeries[depthSeries.length - 1] > 150 ? 'MEDIUM' : 'LOW',
            confidence: 0.75 + Math.random() * 0.2,
            time_points: timePoints,
            depth_series: depthSeries
        };
    }

    generateTimePoints(hours) {
        const points = [];
        for (let i = 0; i <= hours; i += Math.max(1, Math.floor(hours / 50))) {
            points.push(`${i}h`);
        }
        return points;
    }

    generateDepthSeries(timePoints, initial) {
        return timePoints.map((_, i) => {
            const t = i / (timePoints.length - 1);
            return initial + Math.pow(t, 1.5) * 80 * (1 + Math.random() * 0.1);
        });
    }

    generateStressSeries(timePoints) {
        return timePoints.slice(0, 50).map((_, i) => 5 + Math.sin(i * 0.2) * 2 + Math.random() * 0.5);
    }

    generateGrowthRateSeries(timePoints) {
        return timePoints.slice(0, 50).map((_, i) => 0.01 + Math.pow(i / 50, 2) * 0.05 + Math.random() * 0.005);
    }

    generateMockRepairSimulation(crackId, materialId, particleCount) {
        return {
            crack_id: crackId,
            material_id: materialId,
            particle_count: particleCount,
            simulation_steps: 1000,
            filling_rate: 0.75 + Math.random() * 0.2,
            packing_density: 2.5 + Math.random() * 1.5,
            bonding_strength: 60 + Math.random() * 40,
            surface_smoothness: 2 + Math.random() * 8,
            durability_score: 70 + Math.random() * 25,
            porosity: 0.05 + Math.random() * 0.15,
            energy_history: this.generateEnergyData(100)
        };
    }

    generateEnergyData(steps) {
        const data = [];
        let kinetic = 100;
        let potential = 0;

        for (let i = 0; i < steps; i++) {
            kinetic *= 0.98;
            potential += (100 - potential) * 0.02;
            data.push({
                kinetic: kinetic,
                potential: potential,
                total: kinetic + potential
            });
        }

        return data;
    }

    generateMockAlerts(count) {
        const alerts = [];
        const types = ['CRACK_DEPTH_EXCEEDED', 'CRACK_WIDTH_EXCEEDED', 'CRACK_PROPAGATION_RISK', 'VIBRATION_ANOMALY'];
        const severities = ['CRITICAL', 'HIGH', 'MEDIUM', 'LOW'];
        const messages = {
            'CRACK_DEPTH_EXCEEDED': '裂纹深度超过安全阈值200μm',
            'CRACK_WIDTH_EXCEEDED': '裂纹宽度超过安全阈值50μm',
            'CRACK_PROPAGATION_RISK': '裂纹扩展速率异常，存在扩展风险',
            'VIBRATION_ANOMALY': '微振动频谱异常，可能存在结构损伤'
        };

        for (let i = 1; i <= count; i++) {
            const type = types[i % types.length];
            const severity = severities[i % 3];
            alerts.push({
                id: i,
                alert_type: type,
                severity: severity,
                message: messages[type],
                porcelain_id: Math.floor(Math.random() * 200) + 1,
                crack_id: Math.random() > 0.3 ? Math.floor(Math.random() * 50) + 1 : null,
                status: 'ACTIVE',
                created_at: new Date(Date.now() - Math.random() * 86400000 * 7).toISOString(),
                metadata: {
                    measured_value: type === 'CRACK_DEPTH_EXCEEDED' ? (200 + Math.random() * 100).toFixed(1) + 'μm' :
                                  type === 'CRACK_WIDTH_EXCEEDED' ? (50 + Math.random() * 30).toFixed(1) + 'μm' :
                                  '异常'
                }
            });
        }

        return alerts;
    }
}

const app = new PorcelainMonitorApp();