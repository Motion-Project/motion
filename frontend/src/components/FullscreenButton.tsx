import { useState, useEffect } from 'react'

interface FullscreenButtonProps {
  cameraId: number
}

export function FullscreenButton({ cameraId }: FullscreenButtonProps) {
  const [isFullscreen, setIsFullscreen] = useState(false)

  const toggleFullscreen = (e: React.MouseEvent) => {
    e.stopPropagation()
    // Find the camera stream container
    const container = document.querySelector(`[data-camera-id="${cameraId}"]`)
    if (!container) return

    if (!isFullscreen) {
      if (container.requestFullscreen) {
        container.requestFullscreen()
      }
    } else {
      if (document.exitFullscreen) {
        document.exitFullscreen()
      }
    }
  }

  // Listen for fullscreen changes
  useEffect(() => {
    const handleFullscreenChange = () => {
      setIsFullscreen(!!document.fullscreenElement)
    }

    document.addEventListener('fullscreenchange', handleFullscreenChange)
    return () => {
      document.removeEventListener('fullscreenchange', handleFullscreenChange)
    }
  }, [])

  return (
    <button
      type="button"
      onClick={toggleFullscreen}
      className="p-1.5 hover:bg-surface rounded-full transition-colors"
      aria-label="Toggle fullscreen"
      title="Toggle fullscreen"
    >
      <svg
        className="w-5 h-5 text-gray-400 hover:text-gray-200"
        fill="none"
        stroke="currentColor"
        viewBox="0 0 24 24"
      >
        {isFullscreen ? (
          <path
            strokeLinecap="round"
            strokeLinejoin="round"
            strokeWidth={2}
            d="M9 9V4.5M9 9H4.5M9 9L3.75 3.75M9 15v4.5M9 15H4.5M9 15l-5.25 5.25M15 9h4.5M15 9V4.5M15 9l5.25-5.25M15 15h4.5M15 15v4.5m0-4.5l5.25 5.25"
          />
        ) : (
          <path
            strokeLinecap="round"
            strokeLinejoin="round"
            strokeWidth={2}
            d="M4 8V4m0 0h4M4 4l5 5m11-1V4m0 0h-4m4 0l-5 5M4 16v4m0 0h4m-4 0l5-5m11 5l-5-5m5 5v-4m0 4h-4"
          />
        )}
      </svg>
    </button>
  )
}
