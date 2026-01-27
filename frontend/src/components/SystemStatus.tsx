import { memo } from 'react'
import { useSystemStatus } from '@/api/queries'

interface SystemStatusProps {
  /** Where to display the status - different layouts for desktop vs mobile */
  variant?: 'desktop' | 'mobile' | 'mobile-menu'
}

/**
 * System Status Display
 *
 * Shows real-time system metrics (temperature, RAM, disk usage).
 * Memoized to prevent Layout re-renders when metrics update.
 * Updates every 10 seconds via useSystemStatus hook.
 */
export const SystemStatus = memo(function SystemStatus({
  variant = 'desktop'
}: SystemStatusProps) {
  const { data: status } = useSystemStatus()

  const formatBytes = (bytes: number) => {
    if (bytes < 1024) return `${bytes} B`
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(0)} KB`
    if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(0)} MB`
    return `${(bytes / (1024 * 1024 * 1024)).toFixed(1)} GB`
  }

  const getTempColor = (celsius: number) => {
    if (celsius >= 80) return 'text-red-500'
    if (celsius >= 70) return 'text-yellow-500'
    return 'text-green-500'
  }

  if (!status) return null

  // Desktop status bar
  if (variant === 'desktop') {
    return (
      <div className="flex items-center gap-3 text-xs border-l border-gray-700 pl-4">
        {status.temperature && (
          <div className="flex items-center gap-1">
            <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 19v-6a2 2 0 00-2-2H5a2 2 0 00-2 2v6a2 2 0 002 2h2a2 2 0 002-2zm0 0V9a2 2 0 012-2h2a2 2 0 012 2v10m-6 0a2 2 0 002 2h2a2 2 0 002-2m0 0V5a2 2 0 012-2h2a2 2 0 012 2v14a2 2 0 01-2 2h-2a2 2 0 01-2-2z" />
            </svg>
            <span className={getTempColor(status.temperature.celsius)}>
              {status.temperature.celsius.toFixed(1)}°C
            </span>
          </div>
        )}
        {status.memory && (
          <div className="flex items-center gap-1">
            <span className="text-gray-400">RAM:</span>
            <span>{status.memory.percent.toFixed(0)}%</span>
          </div>
        )}
        {status.disk && (
          <div className="flex items-center gap-1">
            <span className="text-gray-400">Disk:</span>
            <span>{formatBytes(status.disk.used)} / {formatBytes(status.disk.total)}</span>
          </div>
        )}
      </div>
    )
  }

  // Mobile compact temperature (shown in header)
  if (variant === 'mobile') {
    return status.temperature ? (
      <span className={`text-xs ${getTempColor(status.temperature.celsius)}`}>
        {status.temperature.celsius.toFixed(0)}°C
      </span>
    ) : null
  }

  // Mobile menu full stats
  if (variant === 'mobile-menu') {
    return (
      <div className="flex flex-wrap gap-3 px-3 py-2 text-xs text-gray-400 border-t border-gray-800 mt-2 pt-3">
        {status.temperature && (
          <span className={getTempColor(status.temperature.celsius)}>
            Temp: {status.temperature.celsius.toFixed(1)}°C
          </span>
        )}
        {status.memory && (
          <span>RAM: {status.memory.percent.toFixed(0)}%</span>
        )}
        {status.disk && (
          <span>Disk: {formatBytes(status.disk.used)} / {formatBytes(status.disk.total)}</span>
        )}
      </div>
    )
  }

  return null
})

/**
 * Version Display Component
 *
 * Shows Motion version number.
 * Memoized separately from SystemStatus to avoid unnecessary re-renders.
 */
export const VersionDisplay = memo(function VersionDisplay() {
  const { data: status } = useSystemStatus()

  if (!status?.version) return null

  return (
    <span className="text-xs text-gray-500 hidden sm:inline">
      v{status.version}
    </span>
  )
})
