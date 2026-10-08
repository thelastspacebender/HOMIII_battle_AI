// ========================================================
// HoMM3 Combat AI — Web Dashboard (HTML5 Canvas + SSE IPC)
// ========================================================

const canvas = document.getElementById('battleCanvas');
const ctx = canvas.getContext('2d');
const chartCanvas = document.getElementById('chartCanvas');
const chartCtx = chartCanvas.getContext('2d');

// Параметры классической сетки HoMM3 (11 рядов x 15 колонок = 165 гексов)
const BFIELD_COLS = 15;
const BFIELD_ROWS = 11;
const HEX_R = 30;
const HEX_W = Math.sqrt(3) * HEX_R;      // ~52 px
const HEX_H = 2 * HEX_R;                 // 60 px
const PITCH_Y = HEX_H * 0.75;            // 45 px
const OFFSET_X = 68;
const OFFSET_Y = 46;

let currentMode = 'bot';
let autoPlayTimer = null;
let autoPlaySpeed = 300;
let gameState = null;
let trainingHistory = [];
let floatingAnimations = []; // { text, x, y, alpha, vy, color }
let arrowAnimation = null;   // { fromX, fromY, toX, toY, progress }

// Координаты центра гекса в пикселях (Odd-R layout)
function getHexCenter(hexId) {
    const x = hexId % BFIELD_COLS;
    const y = Math.floor(hexId / BFIELD_COLS);
    const isOdd = (y % 2 !== 0);
    const cx = OFFSET_X + x * HEX_W + (isOdd ? HEX_W * 0.5 : 0);
    const cy = OFFSET_Y + y * PITCH_Y;
    return { cx, cy, x, y };
}

// Отрисовка правильного шестиугольника
function drawHexagon(cx, cy, r, fillColor, strokeColor, lineWidth = 1) {
    ctx.beginPath();
    for (let i = 0; i < 6; i++) {
        // Pointy-top hex angle: 30 deg offset
        const angle = (Math.PI / 180) * (60 * i - 30);
        const px = cx + r * Math.cos(angle);
        const py = cy + r * Math.sin(angle);
        if (i === 0) ctx.moveTo(px, py);
        else ctx.lineTo(px, py);
    }
    ctx.closePath();
    if (fillColor) {
        ctx.fillStyle = fillColor;
        ctx.fill();
    }
    if (strokeColor) {
        ctx.strokeStyle = strokeColor;
        ctx.lineWidth = lineWidth;
        ctx.stroke();
    }
}

// Предзагрузка аутентичных спрайтов и фона из HoMM3 LOD архивов
const assets = {
    bg: new Image(),
    archer: new Image(),
    pikeman: new Image(),
    zombie: new Image(),
    archerPort: new Image(),
    pikemanPort: new Image(),
    zombiePort: new Image()
};
assets.bg.src = '/assets/combat_bg_crypt.png';
assets.archer.src = '/assets/archer_sprite.png';
assets.pikeman.src = '/assets/pikeman_sprite.png';
assets.zombie.src = '/assets/zombie_sprite.png';
assets.archerPort.src = '/assets/archer_portrait.png';
assets.pikemanPort.src = '/assets/pikeman_portrait.png';
assets.zombiePort.src = '/assets/zombie_portrait.png';

// Основной цикл рендеринга
function render() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // 1. Фон поля боя (Аутентичный Погост / Crypt из h3bitmap.lod)
    if (assets.bg.complete && assets.bg.naturalWidth > 0) {
        ctx.drawImage(assets.bg, 0, 0, canvas.width, canvas.height);
        ctx.fillStyle = 'rgba(5, 10, 20, 0.35)'; // Мягкий контраст
        ctx.fillRect(0, 0, canvas.width, canvas.height);
    } else {
        const bgGrad = ctx.createRadialGradient(canvas.width / 2, canvas.height / 2, 50, canvas.width / 2, canvas.height / 2, 550);
        bgGrad.addColorStop(0, '#0c1322');
        bgGrad.addColorStop(1, '#05080f');
        ctx.fillStyle = bgGrad;
        ctx.fillRect(0, 0, canvas.width, canvas.height);
    }

    const obstaclesSet = new Set(gameState ? gameState.obstacles : []);

    // 2. Отрисовка 187 гексов
    for (let id = 0; id < BFIELD_COLS * BFIELD_ROWS; id++) {
        const { cx, cy, x } = getHexCenter(id);
        const isBorder = (x === 0 || x === BFIELD_COLS - 1);
        const isObstacle = obstaclesSet.has(id);

        let fill = isBorder ? 'rgba(0, 0, 0, 0.5)' : 'rgba(20, 30, 45, 0.12)';
        let stroke = isBorder ? 'rgba(255, 255, 255, 0.03)' : 'rgba(255, 235, 160, 0.18)';

        if (isObstacle) {
            fill = 'rgba(80, 20, 20, 0.45)';
            stroke = 'rgba(220, 60, 60, 0.6)';
        }

        drawHexagon(cx, cy, HEX_R - 1.5, fill, stroke, 1);

        // Препятствия (Камни / Могильные плиты)
        if (isObstacle) {
            ctx.font = '16px serif';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText('🪨', cx, cy);
        }

        // Номер гекса
        if (!isBorder && !isObstacle) {
            ctx.fillStyle = 'rgba(255, 255, 255, 0.18)';
            ctx.font = '9px "JetBrains Mono"';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(id, cx, cy + 15);
        }
    }

    if (!gameState) {
        requestAnimationFrame(render);
        return;
    }

    // 3. Отрисовка боевых отрядов
    for (const stack of gameState.stacks) {
        if (!stack.isAlive || stack.hex < 0) continue;

        const { cx, cy } = getHexCenter(stack.hex);
        const isAlly = stack.side === 'ALLY';
        const isActive = (gameState.activeStackId === stack.id);

        // Подсветка активного ходящего отряда
        if (isActive) {
            drawHexagon(cx, cy, HEX_R + 1, 'rgba(245, 158, 11, 0.25)', '#f59e0b', 2.5);
        }

        // Тень отряда
        ctx.beginPath();
        ctx.ellipse(cx, cy + 14, 18, 8, 0, 0, Math.PI * 2);
        ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
        ctx.fill();

        // Спрайт существа из оригинальной игры
        let sprite = null;
        if (stack.name === 'Archers') sprite = assets.archer;
        else if (stack.name === 'Pikeman') sprite = assets.pikeman;
        else if (stack.name.includes('Walking Dead') || stack.name.includes('Zombie')) sprite = assets.zombie;

        if (sprite && sprite.complete && sprite.naturalWidth > 0) {
            const sw = sprite.naturalWidth;
            const sh = sprite.naturalHeight;
            const scale = Math.min(52 / sw, 66 / sh);
            const drawW = sw * scale;
            const drawH = sh * scale;

            ctx.save();
            if (!isAlly) {
                // Зомби смотрят влево (в сторону нападающих)
                ctx.translate(cx, cy + 12);
                ctx.scale(-1, 1);
                ctx.drawImage(sprite, -drawW / 2, -drawH, drawW, drawH);
            } else {
                ctx.drawImage(sprite, cx - drawW / 2, cy + 12 - drawH, drawW, drawH);
            }
            ctx.restore();
        } else {
            // Фолбэк на круглый жетон
            let tokenColor = isAlly ? '#1e3a8a' : '#7f1d1d';
            let borderColor = isAlly ? '#3b82f6' : '#ef4444';
            let icon = stack.name === 'Archers' ? '🏹' : (stack.name === 'Pikeman' ? '🔱' : '💀');
            ctx.beginPath();
            ctx.arc(cx, cy - 2, 18, 0, Math.PI * 2);
            ctx.fillStyle = tokenColor;
            ctx.fill();
            ctx.lineWidth = 2;
            ctx.strokeStyle = borderColor;
            ctx.stroke();

            ctx.font = '16px serif';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(icon, cx, cy - 2);
        }

        // Бейдж тактической роли (BAIT / CORE)
        if (stack.role === 'BAIT') {
            ctx.fillStyle = '#10b981';
            ctx.font = 'bold 9px "JetBrains Mono"';
            ctx.textAlign = 'center';
            ctx.fillText('BAIT', cx, cy - 28);
        } else if (stack.role === 'CORE') {
            ctx.fillStyle = '#f59e0b';
            ctx.font = 'bold 9px "JetBrains Mono"';
            ctx.textAlign = 'center';
            ctx.fillText('CORE', cx, cy - 28);
        }

        // Плашка количества существ (классический стиль HoMM3)
        ctx.fillStyle = 'rgba(10, 15, 25, 0.9)';
        ctx.strokeStyle = isAlly ? '#3b82f6' : '#ef4444';
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        ctx.roundRect(cx - 16, cy + 13, 32, 14, 3);
        ctx.fill();
        ctx.stroke();

        ctx.fillStyle = '#ffffff';
        ctx.font = 'bold 10px "JetBrains Mono"';
        ctx.textAlign = 'center';
        ctx.fillText(stack.count, cx, cy + 20);

        // Полоска здоровья (Health Bar)
        const healthRatio = stack.totalHp / (stack.initialCount * stack.hpMax);
        const barW = 28;
        const barH = 3;
        ctx.fillStyle = 'rgba(0,0,0,0.7)';
        ctx.fillRect(cx - barW/2, cy + 29, barW, barH);
        ctx.fillStyle = isAlly ? '#10b981' : '#f43f5e';
        ctx.fillRect(cx - barW/2, cy + 29, barW * Math.max(0, healthRatio), barH);
    }

    // 4. Анимация стрелы
    if (arrowAnimation) {
        arrowAnimation.progress += 0.08;
        const { fromX, fromY, toX, toY, progress } = arrowAnimation;

        ctx.beginPath();
        ctx.setLineDash([4, 4]);
        ctx.moveTo(fromX, fromY);
        ctx.lineTo(toX, toY);
        ctx.strokeStyle = 'rgba(245, 158, 11, 0.4)';
        ctx.lineWidth = 2;
        ctx.stroke();
        ctx.setLineDash([]);

        // Наконечник стрелы
        const curX = fromX + (toX - fromX) * Math.min(1, progress);
        const curY = fromY + (toY - fromY) * Math.min(1, progress);

        ctx.beginPath();
        ctx.arc(curX, curY, 4, 0, Math.PI * 2);
        ctx.fillStyle = '#f59e0b';
        ctx.shadowColor = '#f59e0b';
        ctx.shadowBlur = 8;
        ctx.fill();
        ctx.shadowBlur = 0;

        if (arrowAnimation.progress >= 1.0) {
            arrowAnimation = null;
        }
    }

    // 5. Всплывающие цифры урона
    for (let i = floatingAnimations.length - 1; i >= 0; i--) {
        const anim = floatingAnimations[i];
        anim.y += anim.vy;
        anim.alpha -= 0.02;

        ctx.font = 'bold 15px "Cinzel", "Inter"';
        ctx.fillStyle = `rgba(${anim.color}, ${anim.alpha})`;
        ctx.shadowColor = `rgba(${anim.color}, 0.8)`;
        ctx.shadowBlur = 6;
        ctx.textAlign = 'center';
        ctx.fillText(anim.text, anim.x, anim.y);
        ctx.shadowBlur = 0;

        if (anim.alpha <= 0) {
            floatingAnimations.splice(i, 1);
        }
    }

    requestAnimationFrame(render);
}

// Отрисовка графика обучения на втором Canvas
function renderChart() {
    chartCtx.clearRect(0, 0, chartCanvas.width, chartCanvas.height);
    const w = chartCanvas.width;
    const h = chartCanvas.height;

    // Сетка
    chartCtx.strokeStyle = 'rgba(255, 255, 255, 0.06)';
    chartCtx.lineWidth = 1;
    for (let y = 20; y < h; y += 30) {
        chartCtx.beginPath();
        chartCtx.moveTo(0, y);
        chartCtx.lineTo(w, y);
        chartCtx.stroke();
    }

    if (trainingHistory.length < 2) {
        chartCtx.fillStyle = 'rgba(255, 255, 255, 0.2)';
        chartCtx.font = '11px "JetBrains Mono"';
        chartCtx.textAlign = 'center';
        chartCtx.fillText('Ожидание данных обучения...', w / 2, h / 2);
        return;
    }

    // Линия награды (Reward)
    const minR = Math.min(...trainingHistory.map(d => d.reward));
    const maxR = Math.max(...trainingHistory.map(d => d.reward), minR + 10);

    chartCtx.beginPath();
    chartCtx.strokeStyle = '#10b981';
    chartCtx.lineWidth = 2;

    trainingHistory.forEach((d, idx) => {
        const px = (idx / (trainingHistory.length - 1)) * (w - 30) + 15;
        const py = h - 20 - ((d.reward - minR) / (maxR - minR)) * (h - 40);
        if (idx === 0) chartCtx.moveTo(px, py);
        else chartCtx.lineTo(px, py);
    });
    chartCtx.stroke();

    // Точки
    trainingHistory.forEach((d, idx) => {
        const px = (idx / (trainingHistory.length - 1)) * (w - 30) + 15;
        const py = h - 20 - ((d.reward - minR) / (maxR - minR)) * (h - 40);
        chartCtx.beginPath();
        chartCtx.arc(px, py, 3, 0, Math.PI * 2);
        chartCtx.fillStyle = '#34d399';
        chartCtx.fill();
    });
}

// Обновление шкал весов в UI
function updateWeightsUI(weights) {
    if (!weights) return;

    document.getElementById('valBait').innerText = weights.bait_diversion.toFixed(2);
    document.getElementById('barBait').style.width = Math.min(100, (weights.bait_diversion / 6.0) * 100) + '%';

    document.getElementById('valKite').innerText = weights.kite_distance.toFixed(2);
    document.getElementById('barKite').style.width = Math.min(100, (weights.kite_distance / 6.0) * 100) + '%';

    document.getElementById('valStrip').innerText = weights.strip_retaliation.toFixed(2);
    document.getElementById('barStrip').style.width = Math.min(100, (weights.strip_retaliation / 5.0) * 100) + '%';

    document.getElementById('valScreen').innerText = weights.screen_core.toFixed(2);
    document.getElementById('barScreen').style.width = Math.min(100, (weights.screen_core / 5.0) * 100) + '%';

    document.getElementById('valDmg').innerText = weights.dmg_dealt.toFixed(2);
    document.getElementById('barDmg').style.width = Math.min(100, (weights.dmg_dealt / 4.0) * 100) + '%';
}

// Добавление строк в консоль
function appendLog(text, type = 'info') {
    const body = document.getElementById('consoleBody');
    const line = document.createElement('div');
    line.className = `log-line log-${type}`;
    line.innerText = text;
    body.appendChild(line);
    body.scrollTop = body.scrollHeight;
}

function clearConsole() {
    document.getElementById('consoleBody').innerHTML = '';
}

// Обработка данных от C++ движка
function handleEngineEvent(data) {
    if (data.event === 'engine_ready') {
        appendLog(`[ENGINE] Ядро запущено, версия: ${data.version}`, 'system');
        return;
    }

    if (data.event === 'state_update') {
        gameState = data;
        document.getElementById('roundVal').innerText = data.round;
        updateWeightsUI(data.weights);

        if (data.action && data.action.type) {
            const a = data.action;
            if (a.type === 'RANGED_ATTACK') {
                appendLog(`[TACTICAL] Лучники выстрелили во врага! Урон: -${a.damage} HP (погибло: ${a.killed}). ${a.reason}`, 'action');
                const from = getHexCenter(a.fromHex);
                const to = getHexCenter(a.targetHex);
                arrowAnimation = { fromX: from.cx, fromY: from.cy, toX: to.cx, toY: to.cy, progress: 0 };
                floatingAnimations.push({ text: `-${a.damage} HP`, x: to.cx, y: to.cy - 10, alpha: 1.0, vy: -1.2, color: '239, 68, 68' });
            } else if (a.type === 'MOVE') {
                appendLog(`[TACTICAL] Перемещение отряда #${a.unitId} -> гекс ${a.toHex}. ${a.reason}`, 'info');
            } else if (a.type === 'MELEE_ATTACK') {
                appendLog(`[TACTICAL] Удар в рукопашную! Урон: -${a.damage}. Ответный урон: ${a.retaliation}. ${a.reason}`, 'retal');
                const target = getHexCenter(a.targetHex);
                floatingAnimations.push({ text: `-${a.damage}`, x: target.cx, y: target.cy - 10, alpha: 1.0, vy: -1.5, color: '245, 158, 11' });
            } else if (a.type === 'ENEMY_MOVE') {
                appendLog(`[ENEMY AI] Зомби приближаются к цели. ${a.reason}`, 'info');
            } else if (a.type === 'ENEMY_ATTACK' || a.type === 'ENEMY_MOVE_ATTACK') {
                appendLog(`[ENEMY AI] Зомби атакуют! Урон: -${a.damage} (погибло: ${a.killed}). ${a.reason}`, 'damage');
                if (a.targetId) {
                    const victim = gameState.stacks.find(s => s.id === a.targetId);
                    if (victim) {
                        const vPos = getHexCenter(victim.hex);
                        floatingAnimations.push({ text: `-${a.damage} HP`, x: vPos.cx, y: vPos.cy - 10, alpha: 1.0, vy: -1.2, color: '239, 68, 68' });
                    }
                }
            } else if (a.type === 'GAME_OVER') {
                appendLog(`[BATTLE END] Бой завершен! Победитель: ${data.winner}`, 'system');
                if (autoPlayTimer) toggleAutoPlay();
            }
        }
    }

    if (data.event === 'training_epoch') {
        trainingHistory.push(data);
        document.getElementById('epochBadge').innerText = `Эпоха: ${data.epoch} / ${data.totalEpochs} (Победы: ${(data.winRate * 100).toFixed(0)}%)`;
        appendLog(`[ADAM TRAIN] Эпоха ${data.epoch}: Награда = ${data.reward.toFixed(1)}, Победы = ${(data.winRate * 100).toFixed(0)}%, 0 Потерь = ${(data.zeroLossRate * 100).toFixed(0)}%`, 'train');
        renderChart();
    }
}

// Подключение к Server-Sent Events (SSE)
function connectSSE() {
    const es = new EventSource('/api/stream');
    es.onopen = () => {
        document.getElementById('engineStatusPill').className = 'status-pill connected';
        document.getElementById('engineStatusText').innerText = 'C++ ДВИЖОК ПОДКЛЮЧЕН';
        appendLog('[IPC] Связь с C++ бэкендом установлена.', 'system');
    };
    es.onmessage = (e) => {
        try {
            const data = JSON.parse(e.data);
            handleEngineEvent(data);
        } catch (err) {
            console.error('SSE parse error:', err);
        }
    };
    es.onerror = () => {
        document.getElementById('engineStatusPill').className = 'status-pill';
        document.getElementById('engineStatusText').innerText = 'ПЕРЕПОДКЛЮЧЕНИЕ...';
    };
}

// Команды управления
async function stepTurn() {
    try {
        await fetch('/api/step', { method: 'POST' });
    } catch (e) {
        appendLog(`[ERROR] Ошибка выполнения шага: ${e.message}`, 'damage');
    }
}

function toggleAutoPlay() {
    if (autoPlayTimer) {
        clearInterval(autoPlayTimer);
        autoPlayTimer = null;
        document.getElementById('autoPlayLabel').innerText = 'Авто-бой (Play)';
        document.getElementById('btnAutoPlay').className = 'btn btn-secondary';
    } else {
        autoPlayTimer = setInterval(stepTurn, autoPlaySpeed);
        document.getElementById('autoPlayLabel').innerText = 'Пауза (Pause)';
        document.getElementById('btnAutoPlay').className = 'btn btn-danger';
    }
}

function changeSpeed(val) {
    autoPlaySpeed = parseInt(val, 10);
    if (autoPlayTimer) {
        clearInterval(autoPlayTimer);
        autoPlayTimer = setInterval(stepTurn, autoPlaySpeed);
    }
}

async function resetBattle() {
    if (autoPlayTimer) toggleAutoPlay();
    await fetch('/api/reset', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode: currentMode })
    });
    appendLog('[ENGINE] Поле боя сброшено в начальное состояние сценария Погоста.', 'system');
}

async function setMode(mode) {
    currentMode = mode;
    document.getElementById('btnModeBot').className = `mode-btn ${mode === 'bot' ? 'active' : ''}`;
    document.getElementById('btnModeHuman').className = `mode-btn ${mode === 'human' ? 'active' : ''}`;
    document.getElementById('weightsModeBadge').innerText = mode === 'bot' ? 'BattleAI Head' : 'Human Head';
    resetBattle();
}

async function startTraining() {
    const epochsSelect = document.getElementById('trainEpochsSelect');
    const epochs = epochsSelect ? parseInt(epochsSelect.value, 10) : 25;
    appendLog(`[TRAIN] Запущен цикл оптимизации Adam + NES на ${epochs} эпох...`, 'train');
    await fetch('/api/train', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ epochs })
    });
}

// Запуск при загрузке страницы
window.addEventListener('DOMContentLoaded', () => {
    connectSSE();
    render();
    renderChart();
});
