import { useState, useEffect, useRef, useCallback } from 'react'
import { getSessionToken } from '@/api/session'

interface MjpegStreamState {
  imageUrl: string | null
  streamFps: number
  isConnected: boolean
  error: string | null
}

/**
 * MJPEG stream parser that counts frames client-side for accurate FPS measurement.
 * Parses the multipart/x-mixed-replace stream and extracts JPEG frames.
 */
export function useMjpegStream(cameraId: number, streamKey: number) {
  const [state, setState] = useState<MjpegStreamState>({
    imageUrl: null,
    streamFps: 0,
    isConnected: false,
    error: null,
  })

  // Track frame timestamps for FPS calculation
  const frameTimestamps = useRef<number[]>([])
  const abortControllerRef = useRef<AbortController | null>(null)
  const currentImageUrl = useRef<string | null>(null)

  // Calculate FPS from recent frames (called every second)
  const calculateFps = useCallback(() => {
    const now = Date.now()
    const oneSecondAgo = now - 1000
    // Keep only frames from last second
    frameTimestamps.current = frameTimestamps.current.filter((t) => t > oneSecondAgo)
    return frameTimestamps.current.length
  }, [])

  useEffect(() => {
    // FPS update interval
    const fpsInterval = setInterval(() => {
      const fps = calculateFps()
      setState((prev) => ({ ...prev, streamFps: fps }))
    }, 1000)

    return () => clearInterval(fpsInterval)
  }, [calculateFps])

  useEffect(() => {
    // Abort any existing stream
    if (abortControllerRef.current) {
      abortControllerRef.current.abort()
    }

    // Clean up previous image URL
    if (currentImageUrl.current) {
      URL.revokeObjectURL(currentImageUrl.current)
      currentImageUrl.current = null
    }

    // Reset state
    frameTimestamps.current = []
    setState({
      imageUrl: null,
      streamFps: 0,
      isConnected: false,
      error: null,
    })

    const abortController = new AbortController()
    abortControllerRef.current = abortController

    const startStream = async () => {
      try {
        const token = getSessionToken()
        let url = `/${cameraId}/mjpg/stream`
        const params = new URLSearchParams()
        if (token) params.set('token', token)
        params.set('_k', String(streamKey))
        url += '?' + params.toString()

        const response = await fetch(url, {
          signal: abortController.signal,
          credentials: 'same-origin',
        })

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`)
        }

        if (!response.body) {
          throw new Error('No response body')
        }

        setState((prev) => ({ ...prev, isConnected: true, error: null }))

        // Parse the multipart stream
        const reader = response.body.getReader()
        let buffer = new Uint8Array(0)

        // Parse MJPEG stream looking for JPEG frame boundaries
        // JPEG markers: SOI = 0xFF 0xD8 (start), EOI = 0xFF 0xD9 (end)
        while (true) {
          const { done, value } = await reader.read()

          if (done) {
            break
          }

          // Append new data to buffer
          const newBuffer = new Uint8Array(buffer.length + value.length)
          newBuffer.set(buffer)
          newBuffer.set(value, buffer.length)
          buffer = newBuffer

          // Look for complete JPEG frames in buffer
          let searchStart = 0
          while (searchStart < buffer.length - 1) {
            // Find SOI marker (0xFF 0xD8)
            let soiIndex = -1
            for (let i = searchStart; i < buffer.length - 1; i++) {
              if (buffer[i] === 0xff && buffer[i + 1] === 0xd8) {
                soiIndex = i
                break
              }
            }

            if (soiIndex === -1) {
              // No SOI found, keep last byte (might be partial marker)
              buffer = buffer.slice(Math.max(0, buffer.length - 1))
              break
            }

            // Find EOI marker (0xFF 0xD9) after SOI
            let eoiIndex = -1
            for (let i = soiIndex + 2; i < buffer.length - 1; i++) {
              if (buffer[i] === 0xff && buffer[i + 1] === 0xd9) {
                eoiIndex = i
                break
              }
            }

            if (eoiIndex === -1) {
              // No complete frame yet, keep from SOI onwards
              buffer = buffer.slice(soiIndex)
              break
            }

            // Extract complete JPEG frame (SOI to EOI inclusive)
            const jpegData = buffer.slice(soiIndex, eoiIndex + 2)

            // Record frame timestamp for FPS calculation
            frameTimestamps.current.push(Date.now())

            // Create blob URL for the frame
            const blob = new Blob([jpegData], { type: 'image/jpeg' })
            const newUrl = URL.createObjectURL(blob)

            // Revoke previous URL to prevent memory leak
            if (currentImageUrl.current) {
              URL.revokeObjectURL(currentImageUrl.current)
            }
            currentImageUrl.current = newUrl

            setState((prev) => ({
              ...prev,
              imageUrl: newUrl,
            }))

            // Continue searching after this frame
            searchStart = eoiIndex + 2
            buffer = buffer.slice(searchStart)
            searchStart = 0
          }
        }
      } catch (err) {
        if (err instanceof Error && err.name === 'AbortError') {
          // Stream was intentionally aborted
          return
        }

        console.error('MJPEG stream error:', err)
        setState((prev) => ({
          ...prev,
          isConnected: false,
          error: err instanceof Error ? err.message : 'Stream error',
        }))
      }
    }

    startStream()

    return () => {
      abortController.abort()
      if (currentImageUrl.current) {
        URL.revokeObjectURL(currentImageUrl.current)
        currentImageUrl.current = null
      }
    }
  }, [cameraId, streamKey])

  return state
}
