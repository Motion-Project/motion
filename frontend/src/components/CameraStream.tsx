import { useState, useCallback, useEffect, useRef } from 'react'
import { useMjpegStream } from '@/hooks/useMjpegStream'
import { getStoredRestartTimestamp } from '@/lib/cameraRestart'

interface CameraStreamProps {
  cameraId: number
  className?: string
  onStreamFpsChange?: (fps: number) => void // Callback for streaming FPS updates
}

// Custom event name for camera restart notification
export const CAMERA_RESTARTED_EVENT = 'camera-restarted'

export function CameraStream({ cameraId, className = '', onStreamFpsChange }: CameraStreamProps) {
  // Track the last known restart timestamp to detect changes
  const lastKnownRestartRef = useRef<number>(getStoredRestartTimestamp(cameraId))

  // Initialize streamKey from stored restart timestamp
  // This ensures fresh connections after navigation back from Settings
  const [streamKey, setStreamKey] = useState(() => getStoredRestartTimestamp(cameraId))

  // Use MJPEG parser for client-side frame counting
  const { imageUrl, streamFps, isConnected, error } = useMjpegStream(cameraId, streamKey)

  // Store callback in ref to avoid triggering effect when callback reference changes
  // This prevents render loops when parent passes inline arrow functions
  const onStreamFpsChangeRef = useRef(onStreamFpsChange)

  // Update ref whenever callback changes (synchronous to avoid stale closures)
  useEffect(() => {
    onStreamFpsChangeRef.current = onStreamFpsChange
  })

  // Notify parent of streaming FPS changes - only triggers when fps actually changes
  useEffect(() => {
    if (onStreamFpsChangeRef.current) {
      onStreamFpsChangeRef.current(streamFps)
    }
  }, [streamFps])  // ← Only streamFps in deps, NOT onStreamFpsChange

  // Force stream reconnection by changing key
  const handleReconnect = useCallback(() => {
    // Add small delay before retry to avoid hammering
    setTimeout(() => {
      setStreamKey((k) => k + 1)
    }, 2000)
  }, [])

  // Listen for camera restart events to force reconnection (same-page scenario)
  useEffect(() => {
    const handleCameraRestarted = (event: CustomEvent<{ cameraId?: number }>) => {
      // Reconnect if event is for this camera or all cameras (cameraId undefined or 0)
      const eventCamId = event.detail?.cameraId
      if (!eventCamId || eventCamId === 0 || eventCamId === cameraId) {
        // Force new connection by using current timestamp
        const newTimestamp = Date.now()
        lastKnownRestartRef.current = newTimestamp
        setStreamKey(newTimestamp)
      }
    }

    window.addEventListener(CAMERA_RESTARTED_EVENT, handleCameraRestarted as EventListener)
    return () => {
      window.removeEventListener(CAMERA_RESTARTED_EVENT, handleCameraRestarted as EventListener)
    }
  }, [cameraId])

  // Check for restart timestamp changes on mount, when camera changes,
  // and periodically. This handles:
  // - Cross-navigation: Settings restarts camera while Dashboard is unmounted
  // - Multi-tab: Another tab triggers restart
  // - Edge cases: React batching delays event handling
  useEffect(() => {
    const checkForRestart = () => {
      const storedTimestamp = getStoredRestartTimestamp(cameraId)
      // Also check global timestamp (camera 0) for "restart all" scenarios
      const globalTimestamp = cameraId !== 0 ? getStoredRestartTimestamp(0) : 0
      const latestTimestamp = Math.max(storedTimestamp, globalTimestamp)

      if (latestTimestamp > lastKnownRestartRef.current) {
        // A restart happened while we were unmounted or in another tab
        lastKnownRestartRef.current = latestTimestamp
        setStreamKey(latestTimestamp)
      }
    }

    // Check immediately on mount
    checkForRestart()

    // Periodically check for restart changes (every 5 seconds)
    // This handles multi-tab scenarios and edge cases
    const intervalId = setInterval(checkForRestart, 5000)

    return () => clearInterval(intervalId)
  }, [cameraId])

  // Handle connection errors with auto-retry
  useEffect(() => {
    if (error && !isConnected) {
      handleReconnect()
    }
  }, [error, isConnected, handleReconnect])

  if (error && !imageUrl) {
    return (
      <div className={`w-full ${className}`}>
        <div className="aspect-video flex items-center justify-center bg-gray-900 rounded-lg">
          <div className="text-center p-4">
            <svg className="w-12 h-12 mx-auto text-red-500 mb-2" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" />
            </svg>
            <p className="text-red-500 text-sm">{error}</p>
          </div>
        </div>
      </div>
    )
  }

  if (!imageUrl) {
    return (
      <div className={`w-full ${className}`}>
        <div className="relative aspect-video bg-gray-900 animate-pulse rounded-lg">
          {/* Loading spinner in top-right corner */}
          <div className="absolute top-4 right-4">
            <svg className="w-8 h-8 text-gray-600 animate-spin" fill="none" viewBox="0 0 24 24">
              <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
              <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
            </svg>
          </div>
        </div>
      </div>
    )
  }

  return (
    <div className={`w-full ${className}`}>
      <div className="relative aspect-video bg-black rounded-lg overflow-hidden">
        <img
          src={imageUrl}
          alt={`Camera ${cameraId} stream`}
          className="absolute inset-0 w-full h-full object-contain"
        />
      </div>
    </div>
  )
}
