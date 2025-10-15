// Playback controls component

class PlaybackController {
    constructor(stateHistory) {
        this.stateHistory = stateHistory;
        this.currentIndex = 0;
        this.isPlaying = false;
        this.playInterval = null;
        this.speed = 500; // ms between steps
    }

    play() {
        if (this.isPlaying) return;

        this.isPlaying = true;
        this.playInterval = setInterval(() => {
            if (this.currentIndex < this.stateHistory.length - 1) {
                this.stepForward();
            } else {
                this.pause();
            }
        }, this.speed);
    }

    pause() {
        this.isPlaying = false;
        if (this.playInterval) {
            clearInterval(this.playInterval);
            this.playInterval = null;
        }
    }

    stepForward() {
        if (this.currentIndex < this.stateHistory.length - 1) {
            this.currentIndex++;
            return this.stateHistory[this.currentIndex];
        }
        return null;
    }

    stepBackward() {
        if (this.currentIndex > 0) {
            this.currentIndex--;
            return this.stateHistory[this.currentIndex];
        }
        return null;
    }

    reset() {
        this.pause();
        this.currentIndex = 0;
        return this.stateHistory[0];
    }

    goToEnd() {
        this.pause();
        this.currentIndex = this.stateHistory.length - 1;
        return this.stateHistory[this.currentIndex];
    }

    setSpeed(speedMs) {
        this.speed = speedMs;
        if (this.isPlaying) {
            this.pause();
            this.play();
        }
    }

    getProgress() {
        return {
            current: this.currentIndex,
            total: this.stateHistory.length,
            percentage: this.stateHistory.length > 0
                ? (this.currentIndex / this.stateHistory.length) * 100
                : 0
        };
    }
}
