const TEST_MODE = false;
const A4 = 440;

let audioCtx   = null;
let masterGain = null;
let masterVolume = parseFloat(localStorage.getItem('volume') || '1');

function _ensureAudio() {
    if (!audioCtx) {
        audioCtx   = new (window.AudioContext || window.webkitAudioContext)();
        masterGain = audioCtx.createGain();
        masterGain.connect(audioCtx.destination);
    }
    masterGain.gain.value = masterVolume;
    if (audioCtx.state === 'suspended') audioCtx.resume();
}

function playDing() {
    if (masterVolume <= 0) return;
    _ensureAudio();
    const osc  = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.connect(gain);
    gain.connect(masterGain);
    osc.type = 'sine';
    osc.frequency.value = 880;
    const t = audioCtx.currentTime;
    gain.gain.setValueAtTime(0, t);
    gain.gain.linearRampToValueAtTime(0.4, t + 0.01);
    gain.gain.exponentialRampToValueAtTime(0.001, t + 0.35);
    osc.start(t);
    osc.stop(t + 0.35);
}
