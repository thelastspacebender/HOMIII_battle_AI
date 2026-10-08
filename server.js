const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

const PORT = 3000;
const ENGINE_PATH = path.join(__dirname, 'engine_server.exe');

// Запуск C++ процесса engine_server.exe
console.log(`[Bridge] Starting C++ Battle Engine from: ${ENGINE_PATH}`);
const engine = spawn(ENGINE_PATH, [], {
    stdio: ['pipe', 'pipe', 'inherit'],
    cwd: __dirname
});

let sseClients = [];
let lastState = null;
let stdoutBuffer = '';

// Построчная обработка NDJSON потока из C++
engine.stdout.on('data', (chunk) => {
    stdoutBuffer += chunk.toString();
    const lines = stdoutBuffer.split('\n');
    stdoutBuffer = lines.pop(); // Оставляем незавершенную строку

    for (const rawLine of lines) {
        const line = rawLine.trim();
        if (!line) continue;

        try {
            const parsed = JSON.parse(line);
            if (parsed.event === 'state_update') {
                lastState = parsed;
            }
            // Рассылаем событие всем активным браузерам через SSE
            broadcastSSE(parsed);
        } catch (e) {
            console.error('[Bridge] Failed to parse engine JSON line:', line, e.message);
        }
    }
});

engine.on('exit', (code) => {
    console.log(`[Bridge] C++ Engine process exited with code ${code}`);
});

function sendCommand(cmd) {
    if (engine && engine.stdin && !engine.stdin.destroyed) {
        engine.stdin.write(cmd + '\n');
    } else {
        console.error('[Bridge] Engine process not writable');
    }
}

function broadcastSSE(data) {
    const payload = `data: ${JSON.stringify(data)}\n\n`;
    sseClients.forEach((client) => client.res.write(payload));
}

// MIME типы для статики
const MIME_TYPES = {
    '.html': 'text/html; charset=utf-8',
    '.css': 'text/css; charset=utf-8',
    '.js': 'application/javascript; charset=utf-8',
    '.json': 'application/json; charset=utf-8',
    '.png': 'image/png',
    '.svg': 'image/svg+xml'
};

const server = http.createServer((req, res) => {
    const parsedUrl = new URL(req.url, `http://${req.headers.host}`);
    const pathname = parsedUrl.pathname;

    // CORS заголовки
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

    if (req.method === 'OPTIONS') {
        res.writeHead(204);
        res.end();
        return;
    }

    // 1. Server-Sent Events (SSE) стриминг
    if (pathname === '/api/stream') {
        res.writeHead(200, {
            'Content-Type': 'text/event-stream',
            'Cache-Control': 'no-cache',
            'Connection': 'keep-alive'
        });
        res.write('retry: 1000\n\n');

        const client = { id: Date.now(), res };
        sseClients.push(client);

        if (lastState) {
            res.write(`data: ${JSON.stringify(lastState)}\n\n`);
        }

        req.on('close', () => {
            sseClients = sseClients.filter((c) => c.id !== client.id);
        });
        return;
    }

    // 2. API: Шаг хода
    if (pathname === '/api/step' && req.method === 'POST') {
        sendCommand('CMD:STEP');
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ status: 'sent', cmd: 'CMD:STEP' }));
        return;
    }

    // 3. API: Сброс / Инициализация
    if (pathname === '/api/reset' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            let mode = 'bot';
            try {
                if (body) {
                    const data = JSON.parse(body);
                    if (data.mode) mode = data.mode;
                }
            } catch (_) {}
            sendCommand(`CMD:RESET ${mode}`);
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ status: 'sent', mode }));
        });
        return;
    }

    // 4. API: Обучение
    if (pathname === '/api/train' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            let epochs = 10;
            try {
                if (body) {
                    const data = JSON.parse(body);
                    if (data.epochs) epochs = data.epochs;
                }
            } catch (_) {}
            sendCommand(`CMD:TRAIN ${epochs}`);
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ status: 'training_started', epochs }));
        });
        return;
    }

    // 5. API: Получение последнего состояния
    if (pathname === '/api/state' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify(lastState || {}));
        return;
    }

    // 6. Статика (index.html, style.css, app.js)
    let filePath = path.join(__dirname, pathname === '/' ? 'index.html' : pathname);
    const ext = path.extname(filePath);

    fs.readFile(filePath, (err, content) => {
        if (err) {
            res.writeHead(404, { 'Content-Type': 'text/plain' });
            res.end(`File not found: ${pathname}`);
        } else {
            const mime = MIME_TYPES[ext] || 'application/octet-stream';
            res.writeHead(200, { 'Content-Type': mime });
            res.end(content);
        }
    });
});

server.listen(PORT, () => {
    console.log(`[Bridge] Server listening on http://localhost:${PORT}`);
});
