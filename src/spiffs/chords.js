// Chord definitions: [mute/open/fret, finger number]
// null = not played, 0 = open, 1+ = fret number; finger: null or 1-4
const CHORDS = {
    "A major": [
        [null, null], // E
        [0, null],    // A
        [2, 1],       // D
        [2, 2],       // G
        [2, 3],       // B
        [0, null]     // e
    ],
    "E major": [
        [0, null],    // E
        [2, 2],       // A
        [2, 3],       // D
        [1, 1],       // G
        [0, null],    // B
        [0, null]     // e
    ],
    "D major": [
        [null, null], // E
        [null, null], // A
        [0, null],    // D
        [2, 1],       // G
        [3, 3],       // B
        [2, 2]        // e
    ],
    "C major": [
        [null, null], // E
        [3, 3],       // A
        [2, 2],       // D
        [0, null],    // G
        [1, 1],       // B
        [0, null]     // e
    ],
    "G major": [
        [3, 2],       // E
        [2, 1],       // A
        [0, null],    // D
        [0, null],    // G
        [0, null],    // B
        [3, 3]        // e
    ]
};