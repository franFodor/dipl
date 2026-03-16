const TEST_MODE = true; // Set to false to use real ESP data
let lastChord = "None";

function updateChordDisplay(chord) {
    const chordNameEl = document.getElementById('chord-name');
    if (chord && chord !== "None") {
        chordNameEl.textContent = chord;
        chordNameEl.classList.add('detected');
    } else {
        chordNameEl.textContent = "--";
        chordNameEl.classList.remove('detected');
    }
}

async function fetchChord() {
    try {
        let data;
        if (TEST_MODE) {
            data = { chord: "A major" }; // Example test data
        } else {
            const response = await fetch('/api/chord');
            data = await response.json();
        }
        if (data.chord !== lastChord) {
            lastChord = data.chord;
            updateChordDisplay(data.chord);
        }
    } catch (error) {
        console.error('Error fetching chord:', error);
    }
}

// Chord dropdown and fretboard rendering
const chordNames = Object.keys(CHORDS);
let selectedChord = chordNames[0];

function renderDropdown() {
    const dropdown = document.getElementById('chord-dropdown');
    dropdown.innerHTML = '';
    chordNames.forEach(name => {
        const option = document.createElement('option');
        option.value = name;
        option.textContent = name;
        dropdown.appendChild(option);
    });
    dropdown.value = selectedChord;
}

function handleDropdownChange(e) {
    selectedChord = e.target.value;
    renderFretboard(CHORDS[selectedChord]);
}

function renderFretboard(chordPositions = null) {
    const fretboard = document.getElementById('fretboard');
    fretboard.innerHTML = '';
    for (let string = 5; string >= 0; string--) {
        // Render X/O marker as first cell in grid row
        const xoCell = document.createElement('div');
        xoCell.className = 'fret-xo';
        let mark = null;
        if (chordPositions) {
            if (chordPositions[string][0] === null) mark = '×';
            else if (chordPositions[string][0] === 0) mark = 'O';
        }
        xoCell.textContent = mark ? mark : '';
        fretboard.appendChild(xoCell);
        for (let fret = 0; fret < 12; fret++) {
            const cell = document.createElement('div');
            cell.className = 'fret-cell';

            if (string === 0) {
                cell.classList.add('last-row');
            }

            if (string === 0 && (fret === 0 || fret == 2 || fret == 4 || fret == 6 || fret == 8 || fret === 11)) {
                cell.innerHTML = `<span class='fret-label'>${fret+1}</span>`;
            }

            if (chordPositions) {
                const pos = chordPositions[string][0];
                const finger = chordPositions[string][1];
                if (pos !== null && pos > 0 && pos-1 === fret) {
                    const circle = document.createElement('div');
                    circle.className = 'fret-circle';
                    if (finger) circle.textContent = finger;
                    cell.appendChild(circle);
                }
            }

            fretboard.appendChild(cell);
        }
        }
    }


$(document).ready(function() {
    // Load navigation
    fetch('nav.html')
        .then(response => response.text())
        .then(data => {
            $('#navbar').html(data);
            $('#nav-chord').addClass('active').append('<span class="sr-only">(current)</span>');
        });

    // Start polling for chord data
    setInterval(fetchChord, 200);

    // Initialize dropdown and fretboard
    renderDropdown();
    renderFretboard(CHORDS[selectedChord]);
    document.getElementById('chord-dropdown').addEventListener('change', handleDropdownChange);
});
