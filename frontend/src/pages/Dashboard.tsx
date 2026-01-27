import { useState, useMemo } from 'react'
import { useQuery } from '@tanstack/react-query'
import { CameraStream } from '@/components/CameraStream'
import { BottomSheet } from '@/components/BottomSheet'
import { QuickSettings } from '@/components/QuickSettings'
import { FullscreenButton } from '@/components/FullscreenButton'
import { SettingsButton } from '@/components/SettingsButton'
import { SnapshotButton } from '@/components/SnapshotButton'
import { CameraSwitcher } from '@/components/CameraSwitcher'
import { useCameras, useCameraStatus } from '@/api/queries'
import { apiGet } from '@/api/client'
import { updateSessionCsrf } from '@/api/session'
import { useAuthContext } from '@/contexts/AuthContext'

interface ConfigParam {
  value: string | number | boolean
  enabled: boolean
  category: number
  type: string
}

interface DashboardConfig {
  configuration: {
    default: Record<string, ConfigParam>
    [key: string]: Record<string, ConfigParam>
  }
  csrf_token?: string
}

export function Dashboard() {
  const { data: cameras, isLoading, error } = useCameras()
  const { role, isAuthenticated, authRequired } = useAuthContext()
  // Only poll for camera status when authenticated (or auth not required)
  const { data: cameraStatuses } = useCameraStatus({
    enabled: !authRequired || isAuthenticated,
  })
  const [sheetOpen, setSheetOpen] = useState(false)
  const [selectedCameraId, setSelectedCameraId] = useState<number | null>(null)
  // Track streaming FPS per camera (client-side calculated)
  const [streamingFps, setStreamingFps] = useState<Record<number, number>>({})

  // Get capture FPS from server-provided camera status
  const getCaptureFps = (cameraId: number) => {
    return cameraStatuses?.find((c) => c.id === cameraId)?.fps ?? 0
  }

  // Get streaming FPS (client-side calculated)
  const getStreamingFps = (cameraId: number) => {
    return streamingFps[cameraId] ?? 0
  }

  // Handle streaming FPS updates from CameraStream component
  const handleStreamingFpsChange = (cameraId: number) => (fps: number) => {
    setStreamingFps((prev) => ({ ...prev, [cameraId]: fps }))
  }

  // Fetch config when sheet is open
  const { data: configData } = useQuery({
    queryKey: ['config'],
    queryFn: async () => {
      const cfg = await apiGet<DashboardConfig>('/0/api/config')
      // Update session CSRF token when config responses include new tokens
      if (cfg.csrf_token) {
        updateSessionCsrf(cfg.csrf_token)
      }
      return cfg
    },
    enabled: sheetOpen, // Only fetch when sheet is open
    staleTime: 30000,
  })

  const openQuickSettings = (cameraId: number) => {
    setSelectedCameraId(cameraId)
    setSheetOpen(true)
  }

  const closeQuickSettings = () => {
    setSheetOpen(false)
  }

  // Camera switcher for bottom sheet header (only shown with multiple cameras)
  const cameraSwitcher = cameras && cameras.length > 1 ? (
    <CameraSwitcher
      cameras={cameras}
      selectedId={selectedCameraId}
      onSelect={setSelectedCameraId}
    />
  ) : null

  // Build config for selected camera (merge camera-specific with defaults)
  const configForCamera = useMemo(() => {
    if (!configData || !selectedCameraId) return {}

    const defaultConfig = configData.configuration?.default || {}
    const cameraConfig = configData.configuration?.[`cam${selectedCameraId}`] || {}

    // Merge: camera-specific overrides global
    return { ...defaultConfig, ...cameraConfig }
  }, [configData, selectedCameraId])

  if (isLoading) {
    return (
      <div className="p-4 sm:p-6">
        <h2 className="text-2xl sm:text-3xl font-bold mb-4 sm:mb-6">Camera Dashboard</h2>
        <div className="flex flex-col items-center gap-6">
          {[1].map((i) => (
            <div
              key={i}
              className="bg-surface-elevated rounded-lg p-4 animate-pulse w-full max-w-4xl"
            >
              <div className="h-6 bg-surface rounded w-1/3 mb-4"></div>
              <div className="aspect-video bg-surface rounded"></div>
            </div>
          ))}
        </div>
      </div>
    )
  }

  if (error) {
    return (
      <div className="p-4 sm:p-6">
        <h2 className="text-2xl sm:text-3xl font-bold mb-4 sm:mb-6">Camera Dashboard</h2>
        <div className="bg-danger/10 border border-danger rounded-lg p-4 max-w-2xl mx-auto">
          <p className="text-danger">
            Failed to load cameras: {error instanceof Error ? error.message : 'Unknown error'}
          </p>
          <button
            className="mt-2 text-sm text-primary hover:underline"
            onClick={() => window.location.reload()}
          >
            Retry
          </button>
        </div>
      </div>
    )
  }

  if (!cameras || cameras.length === 0) {
    return (
      <div className="p-4 sm:p-6">
        <h2 className="text-2xl sm:text-3xl font-bold mb-4 sm:mb-6">Camera Dashboard</h2>
        <div className="bg-surface-elevated rounded-lg p-8 text-center max-w-2xl mx-auto">
          <svg className="w-16 h-16 mx-auto text-gray-600 mb-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M15 10l4.553-2.276A1 1 0 0121 8.618v6.764a1 1 0 01-1.447.894L15 14M5 18h8a2 2 0 002-2V8a2 2 0 00-2-2H5a2 2 0 00-2 2v8a2 2 0 002 2z" />
          </svg>
          <p className="text-gray-400 text-lg">No cameras configured</p>
          <p className="text-sm text-gray-500 mt-2">
            Add cameras in Motion's configuration file
          </p>
        </div>
      </div>
    )
  }

  // Determine layout based on camera count
  const cameraCount = cameras.length

  // Single camera: streamlined layout without extra headers
  if (cameraCount === 1) {
    const camera = cameras[0]
    const captureFps = getCaptureFps(camera.id)
    const streamFps = getStreamingFps(camera.id)
    return (
      <div className="p-4 sm:p-6">
        <div className="max-w-5xl mx-auto">
          <div className="bg-surface-elevated rounded-lg overflow-hidden shadow-lg" data-camera-id={camera.id}>
            {/* Camera header */}
            <div className="px-4 py-3 border-b border-surface flex items-center justify-between">
              <div className="flex items-center gap-2">
                <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse"></div>
                <h3 className="font-medium">{camera.name}</h3>
              </div>
              <div className="flex items-center gap-3">
                {camera.width && camera.height && (
                  <span className="text-xs text-gray-500">
                    {camera.width}x{camera.height}
                  </span>
                )}
                {(streamFps > 0 || captureFps > 0) && (
                  <span
                    className="text-xs font-mono text-gray-400 cursor-help"
                    title="streaming / capture frame rate"
                  >
                    {streamFps} / {captureFps} fps
                  </span>
                )}
                {role === 'admin' && <SnapshotButton cameraId={camera.id} />}
                <FullscreenButton cameraId={camera.id} />
                {role === 'admin' && <SettingsButton cameraId={camera.id} onClick={openQuickSettings} />}
              </div>
            </div>

            {/* Camera stream */}
            <CameraStream cameraId={camera.id} onStreamFpsChange={handleStreamingFpsChange(camera.id)} />
          </div>
        </div>

        {/* Quick Settings Bottom Sheet */}
        <BottomSheet
          isOpen={sheetOpen}
          onClose={closeQuickSettings}
          title="Quick Settings"
          headerRight={cameraSwitcher}
        >
          {selectedCameraId && (
            <QuickSettings
              cameraId={selectedCameraId}
              config={configForCamera}
            />
          )}
        </BottomSheet>
      </div>
    )
  }

  // Multiple cameras: responsive grid
  const getGridClasses = () => {
    if (cameraCount === 2) {
      return 'grid grid-cols-1 lg:grid-cols-2 gap-4 sm:gap-6'
    } else if (cameraCount <= 4) {
      return 'grid grid-cols-1 md:grid-cols-2 gap-4 sm:gap-6'
    } else {
      return 'grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-4 sm:gap-6'
    }
  }

  return (
    <div className="p-4 sm:p-6">
      <h2 className="text-2xl sm:text-3xl font-bold mb-4 sm:mb-6">
        Cameras ({cameraCount})
      </h2>

      <div className={getGridClasses()}>
        {cameras.map((camera) => {
          const captureFps = getCaptureFps(camera.id)
          const streamFps = getStreamingFps(camera.id)
          return (
            <div
              key={camera.id}
              className="bg-surface-elevated rounded-lg overflow-hidden shadow-lg"
              data-camera-id={camera.id}
            >
              {/* Camera header */}
              <div className="px-4 py-3 border-b border-surface flex items-center justify-between">
                <div className="flex items-center gap-2">
                  <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse"></div>
                  <h3 className="font-medium text-sm sm:text-base">{camera.name}</h3>
                </div>
                <div className="flex items-center gap-2">
                  {camera.width && camera.height && (
                    <span className="text-xs text-gray-500">
                      {camera.width}x{camera.height}
                    </span>
                  )}
                  {(streamFps > 0 || captureFps > 0) && (
                    <span
                      className="text-xs font-mono text-gray-400 cursor-help"
                      title="streaming / capture frame rate"
                    >
                      {streamFps} / {captureFps} fps
                    </span>
                  )}
                  {role === 'admin' && <SnapshotButton cameraId={camera.id} />}
                  <FullscreenButton cameraId={camera.id} />
                  {role === 'admin' && <SettingsButton cameraId={camera.id} onClick={openQuickSettings} />}
                </div>
              </div>

              {/* Camera stream */}
              <CameraStream cameraId={camera.id} onStreamFpsChange={handleStreamingFpsChange(camera.id)} />
            </div>
          )
        })}
      </div>

      {/* Quick Settings Bottom Sheet */}
      <BottomSheet
        isOpen={sheetOpen}
        onClose={closeQuickSettings}
        title="Quick Settings"
        headerRight={cameraSwitcher}
      >
        {selectedCameraId && (
          <QuickSettings
            cameraId={selectedCameraId}
            config={configForCamera}
          />
        )}
      </BottomSheet>
    </div>
  )
}
