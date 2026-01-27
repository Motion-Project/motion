import { useMemo } from 'react'
import { getSessionToken } from '@/api/session'

export function useCameraStream(cameraId: number) {
  // Build MJPEG stream URL - Motion uses /camId/mjpg/stream
  // Include session token as query param for authentication
  // (img tags can't send custom headers)
  const streamUrl = useMemo(() => {
    const token = getSessionToken()
    const baseUrl = `/${cameraId}/mjpg/stream`
    return token ? `${baseUrl}?token=${encodeURIComponent(token)}` : baseUrl
  }, [cameraId])

  // No loading state needed - URL is computed synchronously
  // The CameraStream component handles errors via onError
  return { streamUrl, isLoading: false, error: null }
}
