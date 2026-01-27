import { FormSection } from '@/components/form';
import { useCameraInfo } from '@/hooks/useCameraInfo';
import type { CameraType } from '@/api/types';

export interface CameraSourceSettingsProps {
  cameraId: number;
}

/**
 * Camera Source Settings Component
 *
 * Displays the active camera type and device identifier.
 * Shows a badge indicating the camera backend (libcam/v4l2/netcam/unknown).
 *
 * @param cameraId - Camera ID
 */
export function CameraSourceSettings({ cameraId }: CameraSourceSettingsProps) {
  const { cameraType, cameraDevice, isConnected } = useCameraInfo(cameraId);

  return (
    <FormSection
      title="Camera Source"
      description="Camera connection and type information"
      collapsible
      defaultOpen
    >
      <div className="space-y-4">
        {/* Camera Type Badge */}
        <div className="flex items-center gap-3">
          <span className="text-sm text-gray-400">Type:</span>
          <CameraTypeBadge type={cameraType} />
        </div>

        {/* Device/URL Info */}
        <div className="flex items-start gap-3">
          <span className="text-sm text-gray-400 pt-0.5">Device:</span>
          {cameraDevice ? (
            <div className="flex-1">
              <code className="text-sm text-gray-200 bg-gray-800 px-2 py-1 rounded">
                {cameraDevice}
              </code>
            </div>
          ) : (
            <span className="text-sm text-gray-500 italic">Not configured</span>
          )}
        </div>

        {/* Connection Status */}
        <div className="flex items-center gap-3">
          <span className="text-sm text-gray-400">Status:</span>
          <ConnectionStatusBadge isConnected={isConnected} />
        </div>

        {/* Setup guidance for unknown camera type */}
        {cameraType === 'unknown' && (
          <div className="mt-4 p-4 bg-gray-800 border border-gray-700 rounded">
            <p className="text-sm text-gray-300 mb-2">
              <strong>No camera configured</strong>
            </p>
            <p className="text-sm text-gray-400 mb-3">
              Configure a camera by setting one of the following:
            </p>
            <ul className="text-sm text-gray-400 list-disc list-inside space-y-1 ml-2">
              <li>
                <code className="text-xs bg-gray-900 px-1 py-0.5 rounded">libcam_device</code> - Raspberry Pi camera
              </li>
              <li>
                <code className="text-xs bg-gray-900 px-1 py-0.5 rounded">v4l2_device</code> - USB webcam (e.g., /dev/video0)
              </li>
              <li>
                <code className="text-xs bg-gray-900 px-1 py-0.5 rounded">netcam_url</code> - IP camera (RTSP/HTTP URL)
              </li>
            </ul>
          </div>
        )}
      </div>
    </FormSection>
  );
}

/**
 * Camera Type Badge Component
 *
 * Visual indicator for camera backend type
 */
function CameraTypeBadge({ type }: { type: CameraType }) {
  const badgeStyles = {
    libcam: 'bg-purple-500/20 text-purple-300 border-purple-500/30',
    v4l2: 'bg-blue-500/20 text-blue-300 border-blue-500/30',
    netcam: 'bg-green-500/20 text-green-300 border-green-500/30',
    unknown: 'bg-gray-500/20 text-gray-400 border-gray-500/30',
  };

  const labels = {
    libcam: 'libcamera (Pi Camera)',
    v4l2: 'V4L2 (USB Camera)',
    netcam: 'Network Camera (IP)',
    unknown: 'Not Configured',
  };

  return (
    <span
      className={`inline-flex items-center px-3 py-1 rounded border text-xs font-medium ${badgeStyles[type]}`}
    >
      {labels[type]}
    </span>
  );
}

/**
 * Connection Status Badge Component
 *
 * Shows whether camera is currently connected
 */
function ConnectionStatusBadge({ isConnected }: { isConnected: boolean }) {
  return (
    <span
      className={`inline-flex items-center px-3 py-1 rounded border text-xs font-medium ${
        isConnected
          ? 'bg-green-500/20 text-green-300 border-green-500/30'
          : 'bg-red-500/20 text-red-300 border-red-500/30'
      }`}
    >
      <span
        className={`w-2 h-2 rounded-full mr-2 ${isConnected ? 'bg-green-400' : 'bg-red-400'}`}
      />
      {isConnected ? 'Connected' : 'Disconnected'}
    </span>
  );
}
