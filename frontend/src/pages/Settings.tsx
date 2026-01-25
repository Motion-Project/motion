import { useState, useEffect, useCallback, useMemo } from 'react'
import { useQuery, useQueryClient } from '@tanstack/react-query'
import { apiGet, applyRestartRequiredChanges } from '@/api/client'
import { CAMERA_RESTARTED_EVENT } from '@/components/CameraStream'
import { setRestartTimestamp } from '@/lib/cameraRestart'
import { updateSessionCsrf } from '@/api/session'
import { FormSection } from '@/components/form'
import { useToast } from '@/components/Toast'
import { useBatchUpdateConfig, useSystemStatus } from '@/api/queries'
import { validateConfigParam } from '@/lib/validation'
import { SystemSettings } from '@/components/settings/SystemSettings'
import { DeviceSettings } from '@/components/settings/DeviceSettings'
import { CameraSourceSettings } from '@/components/settings/CameraSourceSettings'
import { LibcameraSettings } from '@/components/settings/LibcameraSettings'
import { V4L2Settings } from '@/components/settings/V4L2Settings'
import { NetcamSettings } from '@/components/settings/NetcamSettings'
import { OverlaySettings } from '@/components/settings/OverlaySettings'
import { StreamSettings } from '@/components/settings/StreamSettings'
import { MotionSettings } from '@/components/settings/MotionSettings'
import { PictureSettings } from '@/components/settings/PictureSettings'
import { MovieSettings } from '@/components/settings/MovieSettings'
import { StorageSettings } from '@/components/settings/StorageSettings'
import { ScheduleSettings } from '@/components/settings/ScheduleSettings'
import { PreferencesSettings } from '@/components/settings/PreferencesSettings'
import { PlaybackSettings } from '@/components/settings/PlaybackSettings'
import { MaskEditor } from '@/components/settings/MaskEditor'
import { NotificationSettings } from '@/components/settings/NotificationSettings'
import { UploadSettings } from '@/components/settings/UploadSettings'
import { ConfigurationPresets } from '@/components/ConfigurationPresets'
import { useAuthContext } from '@/contexts/AuthContext'
import { useCameraCapabilities } from '@/hooks/useCameraCapabilities'
import { useCameraInfo } from '@/hooks/useCameraInfo'

interface ConfigParam {
  value: string | number | boolean
  enabled: boolean
  category: number
  type: string
  list?: string[]
}

interface CameraInfo {
  id: number
  name: string
  url: string
}

interface MotionConfig {
  version: string
  cameras: Record<string, CameraInfo | number>  // "count" is number, rest are CameraInfo
  configuration: {
    default: Record<string, ConfigParam>
    [key: string]: Record<string, ConfigParam>  // cam1, cam2, etc.
  }
  categories: Record<string, { name: string; display: string }>
}

type ConfigChanges = Record<string, string | number | boolean>
type ValidationErrors = Record<string, string>

export function Settings() {
  const { role } = useAuthContext()
  const { addToast } = useToast()
  const [selectedCamera, setSelectedCamera] = useState('0')
  const [changes, setChanges] = useState<ConfigChanges>({})
  const [validationErrors, setValidationErrors] = useState<ValidationErrors>({})
  const [isSaving, setIsSaving] = useState(false)

  // All hooks must be called before any conditional returns
  const queryClient = useQueryClient()
  const { data: config, isLoading, error } = useQuery({
    queryKey: ['config'],
    queryFn: async () => {
      const cfg = await apiGet<MotionConfig & { csrf_token?: string }>('/0/api/config')
      // Update session CSRF token when config responses include new tokens
      if (cfg.csrf_token) {
        updateSessionCsrf(cfg.csrf_token)
      }
      return cfg
    },
  })

  const batchUpdateConfigMutation = useBatchUpdateConfig()

  // Fetch system status for action availability
  const { data: systemStatus } = useSystemStatus()

  // Fetch camera capabilities for conditional UI rendering (e.g., autofocus controls)
  const { data: capabilities } = useCameraCapabilities(Number(selectedCamera))

  // Fetch camera info for multi-camera type support
  const cameraInfo = useCameraInfo(Number(selectedCamera))

  // Clear changes and errors when camera selection changes
  // This prevents race conditions where settings from one camera could be
  // applied to another if the user switches cameras before saving
  useEffect(() => {
    setChanges({})
    setValidationErrors({})
  }, [selectedCamera])

  const handleChange = useCallback((param: string, value: string | number | boolean) => {
    setChanges((prev) => ({ ...prev, [param]: value }))

    // Validate the new value
    const result = validateConfigParam(param, String(value))
    setValidationErrors((prev) => {
      if (result.success) {
        // Remove error if validation passes
        // eslint-disable-next-line @typescript-eslint/no-unused-vars
        const { [param]: _, ...rest } = prev
        return rest
      } else {
        // Add error
        return { ...prev, [param]: result.error ?? 'Invalid value' }
      }
    })
  }, [])

  const isDirty = Object.keys(changes).length > 0
  const hasValidationErrors = Object.keys(validationErrors).length > 0

  // Get the original config for the selected camera (without pending changes)
  // Used to detect modified fields and show indicators
  const originalConfig = useMemo(() => {
    if (!config) return {}
    const defaultConfig = config.configuration.default || {}

    if (selectedCamera === '0') {
      return defaultConfig
    } else {
      const cameraConfig = config.configuration[`cam${selectedCamera}`] || {}
      // Merge: camera-specific values override defaults
      return { ...defaultConfig, ...cameraConfig }
    }
  }, [config, selectedCamera])

  // Get the active config for the selected camera
  // Merges camera-specific config with defaults (camera values override defaults)
  // Also merges in pending changes so UI reflects unsaved edits
  const activeConfig = useMemo(() => {
    if (!config) return {}

    // Start with originalConfig and merge pending changes
    const mergedConfig = { ...originalConfig }
    for (const [param, value] of Object.entries(changes)) {
      if (mergedConfig[param]) {
        mergedConfig[param] = { ...mergedConfig[param], value }
      } else {
        // Create a new entry for params not in original config
        mergedConfig[param] = { value, enabled: true, category: 0, type: 'string' }
      }
    }

    return mergedConfig
  }, [config, originalConfig, changes])

  const handleSave = async () => {
    if (!isDirty) {
      addToast('No changes to save', 'info')
      return
    }

    // Block save if there are validation errors
    if (hasValidationErrors) {
      addToast('Please fix validation errors before saving', 'error')
      return
    }

    setIsSaving(true)
    const camId = parseInt(selectedCamera, 10)

    try {
      // Use batch API - send all changes in one request
      const response = await batchUpdateConfigMutation.mutateAsync({
        camId,
        changes,
      }) as {
        status?: string
        applied?: Array<{ param: string; error?: string; hot_reload?: boolean }>
        summary?: { total: number; success: number; errors: number }
      } | undefined

      // Wait for config to be refetched before clearing changes
      // This prevents the UI from reverting to old values
      await queryClient.invalidateQueries({ queryKey: ['config'] })
      await queryClient.refetchQueries({ queryKey: ['config'] })

      // Check response for partial failures
      const summary = response?.summary
      const applied = response?.applied || []

      if (!summary) {
        // No summary in response - assume success
        addToast(`Saved ${Object.keys(changes).length} setting(s)`, 'success')
        setChanges({})
      } else {
        // Find which parameters require restart (hot_reload: false, no error)
        const restartParams = applied
          .filter((p) => !p.error && p.hot_reload === false)
          .map((p) => p.param)

        if (restartParams.length > 0) {
          // Auto-restart for restart-required parameters
          addToast(
            `Restarting camera to apply ${restartParams.length} setting(s): ${restartParams.join(', ')}...`,
            'info'
          )

          // Clear changes now since config is already saved
          setChanges({})

          // Write config and restart camera (fire-and-forget with polling)
          const cameBackOnline = await applyRestartRequiredChanges(camId)

          // Store restart timestamp in localStorage for cross-navigation detection
          // This allows CameraStream to detect the restart even if user navigates
          // back to Dashboard after Settings triggered the restart
          setRestartTimestamp(camId)

          // Notify stream components to reconnect (for same-page scenarios)
          window.dispatchEvent(
            new CustomEvent(CAMERA_RESTARTED_EVENT, { detail: { cameraId: camId } })
          )

          // Refetch config after restart
          await queryClient.invalidateQueries({ queryKey: ['config'] })
          await queryClient.refetchQueries({ queryKey: ['config'] })

          if (cameBackOnline) {
            addToast(
              `Applied ${restartParams.length} setting(s). Camera restarted successfully.`,
              'success'
            )
          } else {
            addToast(
              `Settings saved. Camera is restarting - refresh page if stream doesn't recover.`,
              'warning'
            )
          }
        } else if (summary.errors > 0) {
          // Handle errors (not restart-related)
          const failedParams = applied
            .filter((p) => p.error)
            .map((p) => p.param)

          // Only clear changes that succeeded
          const successfulParams = applied
            .filter((p) => !p.error)
            .map((p) => p.param)

          const remainingChanges: ConfigChanges = {}
          for (const [param, value] of Object.entries(changes)) {
            if (!successfulParams.includes(param)) {
              remainingChanges[param] = value
            }
          }
          setChanges(remainingChanges)

          if (summary.success > 0) {
            addToast(
              `Saved ${summary.success} setting(s). ${summary.errors} failed: ${failedParams.join(', ')}`,
              'warning'
            )
          } else {
            addToast(
              `Failed to save settings: ${failedParams.join(', ')}`,
              'error'
            )
          }
        } else {
          // All params applied successfully with no restart needed
          addToast(`Successfully saved ${summary.success} setting(s)`, 'success')
          setChanges({})
        }
      }
    } catch (err) {
      console.error('Failed to save settings:', err)
      addToast('Failed to save settings. Check browser console for details.', 'error')
    } finally {
      setIsSaving(false)
    }
  }

  const handleReset = () => {
    setChanges({})
    setValidationErrors({})
    addToast('Changes discarded', 'info')
  }

  // Helper to get error for a parameter
  const getError = (param: string): string | undefined => {
    return validationErrors[param]
  }

  // Require admin access - check after all hooks
  if (role !== 'admin') {
    return (
      <div className="p-4 sm:p-6">
        <div className="bg-surface-elevated rounded-lg p-8 text-center max-w-2xl mx-auto">
          <svg className="w-16 h-16 mx-auto text-yellow-500 mb-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" />
          </svg>
          <h1 className="text-2xl font-bold mb-2">Admin Access Required</h1>
          <p className="text-gray-400">You must be logged in as an administrator to access settings.</p>
        </div>
      </div>
    )
  }

  if (isLoading) {
    return (
      <div className="p-6">
        <h2 className="text-3xl font-bold mb-6">Settings</h2>
        <div className="animate-pulse">
          <div className="h-32 bg-surface-elevated rounded-lg mb-4"></div>
          <div className="h-32 bg-surface-elevated rounded-lg mb-4"></div>
        </div>
      </div>
    )
  }

  if (error) {
    return (
      <div className="p-6">
        <h2 className="text-3xl font-bold mb-6">Settings</h2>
        <div className="bg-danger/10 border border-danger rounded-lg p-4">
          <p className="text-danger">Failed to load configuration</p>
        </div>
      </div>
    )
  }

  if (!config) {
    return null
  }

  return (
    <div className="p-6">
      {/* Sticky sub-header for save controls */}
      <div className="sticky top-[73px] z-40 -mx-6 px-6 py-3 bg-surface/95 backdrop-blur border-b border-gray-800 mb-6">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-4">
            <h2 className="text-2xl font-bold">Settings</h2>
            <div>
              <select
                value={selectedCamera}
                onChange={(e) => setSelectedCamera(e.target.value)}
                className="px-3 py-1.5 bg-surface-elevated border border-gray-700 rounded-lg text-sm"
              >
                <option value="0">Global Settings</option>
                {config.cameras && Object.entries(config.cameras).map(([key, cam]) => {
                  // Skip the 'count' property
                  if (key === 'count' || typeof cam === 'number') return null
                  const camera = cam as CameraInfo
                  return (
                    <option key={camera.id} value={String(camera.id)}>
                      {camera.name || `Camera ${camera.id}`}
                    </option>
                  )
                })}
              </select>
            </div>
          </div>
          <div className="flex items-center gap-3">
            {isDirty && !hasValidationErrors && (
              <span className="text-yellow-200 text-sm">Unsaved changes</span>
            )}
            {hasValidationErrors && (
              <span className="text-red-200 text-sm">Fix errors below</span>
            )}
            {isDirty && (
              <button
                onClick={handleReset}
                disabled={isSaving}
                className="px-3 py-1.5 text-sm bg-surface-elevated hover:bg-surface rounded-lg transition-colors disabled:opacity-50"
              >
                Discard
              </button>
            )}
            <button
              onClick={handleSave}
              disabled={!isDirty || isSaving || hasValidationErrors}
              className="px-4 py-1.5 text-sm bg-primary hover:bg-primary-hover rounded-lg transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
            >
              {isSaving ? 'Saving...' : isDirty ? 'Save Changes' : 'Saved'}
            </button>
          </div>
        </div>
      </div>

      {/* Global-only sections - show when selectedCamera === '0' */}
      {selectedCamera === '0' && (
        <>
          <div className="bg-blue-500/10 border border-blue-500/30 rounded-lg p-4 mb-6">
            <p className="text-sm text-blue-200">
              Global settings apply to the Motion daemon and web server.
              To configure camera-specific settings, select a camera from the dropdown above.
            </p>
          </div>
          <SystemSettings config={activeConfig} onChange={handleChange} getError={getError} originalConfig={originalConfig} systemStatus={systemStatus} />

          {/* UI Preferences - Global only */}
          <PreferencesSettings />

          {/* About - Global only */}
          <FormSection
            title="About"
            description="Motion version information"
            collapsible
            defaultOpen={false}
          >
            <p className="text-sm text-gray-400">
              Motion Version: {config.version}
            </p>
          </FormSection>
        </>
      )}

      {/* Camera-specific sections - show when selectedCamera !== '0' */}
      {selectedCamera !== '0' && (
        <>
          {/* 1. Configuration Presets */}
          <div className="bg-surface-elevated rounded-lg p-4 mb-6">
            <ConfigurationPresets cameraId={Number(selectedCamera)} readOnly={false} />
          </div>

          {/* 2. Camera Source */}
          <CameraSourceSettings
            cameraId={Number(selectedCamera)}
            config={activeConfig}
            onChange={handleChange}
          />

          {/* 3. Type-Specific Camera Controls */}
          {cameraInfo.features.hasLibcamControls && (
            <LibcameraSettings
              config={activeConfig}
              onChange={handleChange}
              getError={getError}
              capabilities={capabilities}
              originalConfig={originalConfig}
            />
          )}

          {cameraInfo.features.hasV4L2Controls && (
            <V4L2Settings
              config={activeConfig}
              onChange={handleChange}
              controls={cameraInfo.v4l2Controls}
              getError={getError}
            />
          )}

          {cameraInfo.features.hasNetcamConfig && (
            <NetcamSettings
              config={activeConfig}
              onChange={handleChange}
              connectionStatus={cameraInfo.netcamStatus}
              hasDualStream={cameraInfo.features.hasDualStream}
              getError={getError}
            />
          )}

          {/* 4. Device Settings */}
          <DeviceSettings
            config={activeConfig}
            onChange={handleChange}
            getError={getError}
          />

          {/* 5. Movie Settings */}
          <MovieSettings
            config={activeConfig}
            onChange={handleChange}
            getError={getError}
            showPassthrough={cameraInfo.features.supportsPassthrough}
          />

          {/* 5. Video Streaming */}
          <StreamSettings
            config={activeConfig}
            onChange={handleChange}
            getError={getError}
          />

          {/* 6. Picture Settings */}
          <PictureSettings
            config={activeConfig}
            onChange={handleChange}
            getError={getError}
          />

          {/* 7. Motion Detection */}
          <MotionSettings
            config={activeConfig}
            onChange={handleChange}
            getError={getError}
          />

          {/* 8. Mask Editor */}
          <MaskEditor cameraId={parseInt(selectedCamera, 10)} />

          {/* 9. Schedule */}
          <ScheduleSettings
            config={activeConfig}
            onChange={handleChange}
            getError={getError}
          />

          {/* 10. Storage */}
          <StorageSettings
            config={activeConfig}
            onChange={handleChange}
            getError={getError}
            originalConfig={originalConfig}
          />

          {/* 11. Text Overlay */}
          <OverlaySettings
            config={activeConfig}
            onChange={handleChange}
            getError={getError}
          />

          {/* 12. Notification & Scripts */}
          <NotificationSettings
            config={activeConfig}
            onChange={handleChange}
            getError={getError}
          />

          {/* 13. Cloud Upload */}
          <UploadSettings
            config={activeConfig}
            onChange={handleChange}
            getError={getError}
          />

          {/* 14. Playback Settings - Camera only (extracted from UI Preferences) */}
          <PlaybackSettings />
        </>
      )}
    </div>
  )
}
