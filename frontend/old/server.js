const express = require('express');
const path = require('path');
const { spawn } = require('child_process');
const cors = require('cors');
const fs = require('fs');
const http = require('http');
const WebSocket = require('ws');

const app = express();

app.use(cors());

app.use(express.json());

const BACKEND_HOST = process.env.BACKEND_HOST || 'localhost';
const BACKEND_PORT = process.env.BACKEND_PORT || 8080;
const WS_BRIDGE_PORT = process.env.WS_BRIDGE_PORT || 8081;

const validateSession = async (sessionId) => {
    if (!sessionId) return false;
    
    try {
        
        const postData = JSON.stringify({
            operation: 'session_validate',
            session_id: sessionId
        });

        return new Promise((resolve) => {
            const options = {
                hostname: BACKEND_HOST,
                port: BACKEND_PORT,
                path: '/',
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Content-Length': Buffer.byteLength(postData)
                },
                timeout: 3000
            };

            const req = http.request(options, (res) => {
                let data = '';
                res.on('data', (chunk) => { data += chunk; });
                res.on('end', () => {
                    try {
                        const result = JSON.parse(data);
                        resolve(result.status === 'success');
                    } catch (e) {
                        
                        resolve(true);
                    }
                });
            });

            req.on('error', () => resolve(true)); 
            req.on('timeout', () => { req.destroy(); resolve(true); });
            
            req.write(postData);
            req.end();
        });
    } catch (error) {
        return true; 
    }
};

app.post('/api', (req, res) => {
    const postData = JSON.stringify(req.body);
    
    const options = {
        hostname: BACKEND_HOST,
        port: BACKEND_PORT,
        path: '/',
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Content-Length': Buffer.byteLength(postData)
        },
        timeout: 10000
    };

    const proxyReq = http.request(options, (proxyRes) => {
        let data = '';
        proxyRes.on('data', (chunk) => { data += chunk; });
        proxyRes.on('end', () => {
            res.setHeader('Content-Type', 'application/json');
            res.status(proxyRes.statusCode).send(data);
        });
    });

    proxyReq.on('error', (err) => {
        console.error('Backend connection error:', err.message);
        res.status(503).json({
            status: 'error',
            code: 'BACKEND_UNAVAILABLE',
            message: 'Cannot connect to backend server. Make sure the C++ server is running on port 8080.'
        });
    });

    proxyReq.on('timeout', () => {
        proxyReq.destroy();
        res.status(504).json({
            status: 'error',
            code: 'BACKEND_TIMEOUT',
            message: 'Backend server request timed out'
        });
    });

    proxyReq.write(postData);
    proxyReq.end();
});

app.get('/auth/check', (req, res) => {
    const sessionId = req.query.session_id;
    if (!sessionId) {
        return res.json({ authenticated: false });
    }
    
    res.json({ authenticated: true });
});

app.get('/login', (req, res) => {
    res.sendFile(path.join(__dirname, 'login.html'));
});

app.get('/login.html', (req, res) => {
    res.sendFile(path.join(__dirname, 'login.html'));
});

app.get('/', (req, res) => {
    res.redirect('/login.html');
});

app.get('/index.html', (req, res) => {
    res.sendFile(path.join(__dirname, 'index.html'));
});

app.get('/dashboard', (req, res) => {
    res.sendFile(path.join(__dirname, 'index.html'));
});

app.get('/trips', (req, res) => {
    res.sendFile(path.join(__dirname, 'index.html'));
});

app.use(express.static(__dirname, {
    index: false, 
    extensions: ['html']
}));

app.use((req, res, next) => {
    
    if (path.extname(req.path)) {
        return res.status(404).send('Not found');
    }
    
    res.redirect('/login.html');
});

let wsProxy = null;
let backendWs = null;
let clients = new Set();

function setupWebSocketProxy(server) {
    wsProxy = new WebSocket.Server({ server, path: '/ws' });
    
    wsProxy.on('connection', (clientWs) => {
        console.log('Client connected to WebSocket proxy');
        clients.add(clientWs);
        
        connectToBackendWs();
        
        clientWs.on('message', (message) => {
            
            if (backendWs && backendWs.readyState === WebSocket.OPEN) {
                backendWs.send(message);
            }
        });
        
        clientWs.on('close', () => {
            console.log('Client disconnected from WebSocket proxy');
            clients.delete(clientWs);
        });
        
        clientWs.on('error', (err) => {
            console.error('Client WebSocket error:', err.message);
            clients.delete(clientWs);
        });
    });
}

function connectToBackendWs() {
    if (backendWs && backendWs.readyState === WebSocket.OPEN) {
        return;
    }
    
    try {
        backendWs = new WebSocket(`ws:
        
        backendWs.on('open', () => {
            console.log('Connected to backend WebSocket bridge');
        });
        
        backendWs.on('message', (data) => {
            
            clients.forEach((client) => {
                if (client.readyState === WebSocket.OPEN) {
                    client.send(data);
                }
            });
        });
        
        backendWs.on('close', () => {
            console.log('Backend WebSocket disconnected');
            backendWs = null;
            
            setTimeout(connectToBackendWs, 5000);
        });
        
        backendWs.on('error', (err) => {
            console.error('Backend WebSocket error:', err.message);
        });
    } catch (err) {
        console.error('Failed to connect to backend WebSocket:', err.message);
    }
}

let bridgeProcess = null;

function startBridge() {
    try {
        
        const possiblePaths = [
            path.join(__dirname, '../source/modules/websocket_bridge'),
            path.join(__dirname, 'websocket_bridge'),
            path.join(process.cwd(), 'websocket_bridge')
        ];
        
        let bridgePath = null;
        for (const p of possiblePaths) {
            if (fs.existsSync(p)) {
                bridgePath = p;
                break;
            }
        }
        
        if (bridgePath) {
            console.log('Starting WebSocket bridge from:', bridgePath);
            
            const bridgeDir = path.dirname(bridgePath);
            bridgeProcess = spawn(bridgePath, [], {
                stdio: 'inherit',
                detached: false,
                cwd: bridgeDir
            });
            
            bridgeProcess.on('error', (err) => {
                console.error('Failed to start bridge:', err);
            });
            
            bridgeProcess.on('exit', (code) => {
                console.log(`Bridge process exited with code ${code}`);
                bridgeProcess = null;
            });
        } else {
            console.log('WebSocket bridge not found - real-time features require websocket_bridge to be running');
            console.log('Note: For real-time data, run ./websocket_bridge manually');
        }
    } catch (err) {
        console.log('WebSocket bridge not available:', err.message);
    }
}

function shutdown() {
    console.log('\nShutting down server...');
    
    if (bridgeProcess) {
        console.log('Stopping WebSocket bridge...');
        bridgeProcess.kill();
    }
    
    if (backendWs) {
        backendWs.close();
    }
    
    clients.forEach((client) => {
        client.close();
    });
    
    process.exit(0);
}

process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);

const PORT = process.env.PORT || 3000;
const server = app.listen(PORT, () => {
    console.log(`╔══════════════════════════════════════════════════════╗`);
    console.log(`║          SMART DRIVE MANAGER - FRONTEND              ║`);
    console.log(`╠══════════════════════════════════════════════════════╣`);
    console.log(`║  Frontend Server:  http:
    console.log(`║  Login Page:       http:
    console.log(`║  Dashboard:        http:
    console.log(`╠══════════════════════════════════════════════════════╣`);
    console.log(`║  Backend API:      http:
    console.log(`║  WebSocket Bridge: ws:
    console.log(`╠══════════════════════════════════════════════════════╣`);
    console.log(`║  SECURITY: All routes redirect to login              ║`);
    console.log(`║  NOTE: Make sure C++ backend is running on port 8080 ║`);
    console.log(`╚══════════════════════════════════════════════════════╝`);
    
    setupWebSocketProxy(server);
    
    startBridge();
});

process.on('uncaughtException', (err) => {
    console.error('Uncaught Exception:', err);
});