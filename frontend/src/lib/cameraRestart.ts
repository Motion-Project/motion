// LocalStorage key for tracking camera restart timestamps
const RESTART_TIMESTAMP_KEY = 'motion-camera-restart-timestamps'

/**
 * Get the stored restart timestamp for a camera from localStorage.
 * Returns 0 if no timestamp is stored.
 */
export function getStoredRestartTimestamp(cameraId: number): number {
  try {
    const stored = localStorage.getItem(RESTART_TIMESTAMP_KEY)
    if (stored) {
      const timestamps = JSON.parse(stored) as Record<string, number>
      return timestamps[String(cameraId)] || 0
    }
  } catch {
    // Ignore parse errors
  }
  return 0
}

/**
 * Store a restart timestamp for a camera in localStorage.
 * This allows CameraStream to detect restarts even after navigation.
 */
export function setRestartTimestamp(cameraId: number): void {
  try {
    const stored = localStorage.getItem(RESTART_TIMESTAMP_KEY)
    const timestamps = stored ? (JSON.parse(stored) as Record<string, number>) : {}
    timestamps[String(cameraId)] = Date.now()
    // Also set for camera 0 (all cameras) if specific camera
    if (cameraId !== 0) {
      timestamps['0'] = Date.now()
    }
    localStorage.setItem(RESTART_TIMESTAMP_KEY, JSON.stringify(timestamps))
  } catch {
    // Ignore storage errors
  }
}
