let gameState;
let memoizeTable;
let canvas, ctx;

function initializeGame() {
    gameState = Module.ccall('get_initial_game', 'number', [], []);
    memoizeTable = Module.ccall('init_memoize', 'number', [], []);
    canvas = document.getElementById('gameCanvas');
    ctx = canvas.getContext('2d');
    updateDisplay();
    canvas.addEventListener('click', handleCanvasClick);
}

function drawGrid() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.strokeStyle = 'black';
    ctx.lineWidth = 2;
    const cellSize = canvas.width / 5;
    for (let i = 0; i <= 5; i++) {
        ctx.beginPath();
        ctx.moveTo(i * cellSize, 0);
        ctx.lineTo(i * cellSize, canvas.height);
        ctx.stroke();
        ctx.beginPath();
        ctx.moveTo(0, i * cellSize);
        ctx.lineTo(canvas.width, i * cellSize);
        ctx.stroke();
    }
}

function drawGameState(state) {
    const buffer = Module._malloc(256);
    Module.ccall('display', null, ['number', 'number'], [state, buffer]);
    const displayStr = Module.UTF8ToString(buffer);
    Module._free(buffer);

    const lines = displayStr.split('\n');
    const wind = lines[0];
    const grid = lines.slice(1, 6);

    ctx.font = '20px Arial';
    ctx.fillStyle = 'black';
    ctx.textAlign = 'center';
    const cellSize = canvas.width / 5;
    for (let i = 0; i < 5; i++) {
        for (let j = 0; j < 5; j++) {
            const char = grid[i][j];
            ctx.fillText(char === '_' ? '' : char, j * cellSize + cellSize / 2, i * cellSize + cellSize / 2 + 8);
        }
    }

    ctx.font = '24px Arial';
    ctx.fillText('Wind: ' + wind, canvas.width / 2, canvas.height - 20);
}

function updateBestMoves(state) {
    const opts = Module._malloc(25 * 8);
    const len = Module.ccall('options', 'number', ['number', 'number', 'number'], [memoizeTable, opts, state]);
    let bestMoves = 'Best moves: ';
    const turn = Module.ccall('get_turn', 'number', ['number'], [state]);
    for (let i = 0; i < len; i++) {
        const move = Module.getValue(opts + i * 8, 'i32');
        const delta = Module.getValue(opts + i * 8 + 4, 'i32');
        if (turn % 2 === 0) {
            bestMoves += `${move}${delta ? ' [' + (-delta) + ' short]' : ''}${i < len - 1 ? ', ' : ''}`;
        } else {
            bestMoves += `${move}${delta < -1 ? ' [' + (-delta) + ' gaps]' : ''}${i < len - 1 ? ', ' : ''}`;
        }
    }
    if (len === 0) bestMoves += '(unwinnable)';
    document.getElementById('best-moves').textContent = bestMoves;
    Module._free(opts);
}

function updateDisplay() {
    drawGrid();
    drawGameState(gameState);
    updateBestMoves(gameState);
    const score = Module.ccall('get_score', 'number', ['number'], [gameState]);
    const turn = Module.ccall('get_turn', 'number', ['number'], [gameState]);
    const status = document.getElementById('status');
    const windButtons = document.getElementById('wind-buttons');
    if (score <= 0) {
        status.textContent = 'Dandelions win!';
        windButtons.style.display = 'none';
        canvas.removeEventListener('click', handleCanvasClick);
    } else if (turn >= 14) {
        status.textContent = 'Wind wins!';
        windButtons.style.display = 'none';
        canvas.removeEventListener('click', handleCanvasClick);
    } else if (turn % 2 === 0) {
        status.textContent = "Dandelions' turn: Click a grid cell to plant a flower";
        windButtons.style.display = 'none';
    } else {
        status.textContent = "Wind's turn: Choose a direction";
        windButtons.style.display = 'block';
    }
}

function handleCanvasClick(event) {
    const turn = Module.ccall('get_turn', 'number', ['number'], [gameState]);
    if (turn % 2 !== 0) return;
    const rect = canvas.getBoundingClientRect();
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;
    const cellSize = canvas.width / 5;
    const col = Math.floor(x / cellSize);
    const row = Math.floor(y / cellSize);
    const pos = row * 5 + col;
    gameState = Module.ccall('plant_move', 'number', ['number', 'number'], [gameState, pos]);
    updateDisplay();
}

function makeWindMove(dir) {
    const turn = Module.ccall('get_turn', 'number', ['number'], [gameState]);
    if (turn % 2 === 0) return;
    gameState = Module.ccall('blow_move', 'number', ['number', 'number'], [gameState, dir]);
    updateDisplay();
}

function restartGame() {
    Module.ccall('free_memoize', null, ['number'], [memoizeTable]);
    gameState = Module.ccall('get_initial_game', 'number', [], []);
    memoizeTable = Module.ccall('init_memoize', 'number', [], []);
    canvas.addEventListener('click', handleCanvasClick);
    updateDisplay();
}

Module.onRuntimeInitialized = initializeGame;
