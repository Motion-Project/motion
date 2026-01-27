import { FormSection, FormInput } from '@/components/form';
import type { NetcamConnectionStatus } from '@/api/types';

export interface NetcamSettingsProps {
  config: Record<string, { value: string | number | boolean }>;
  onChange: (param: string, value: string | number | boolean) => void;
  connectionStatus?: NetcamConnectionStatus;
  hasDualStream?: boolean;
  getError?: (param: string) => string | undefined;
}

/**
 * Network Camera Settings Component
 *
 * Configuration for IP cameras via FFmpeg (RTSP, HTTP, HTTPS, file://).
 * Supports authentication, custom FFmpeg parameters, and dual-stream configuration.
 *
 * @param config - Camera configuration
 * @param onChange - Configuration change handler
 * @param connectionStatus - Current NETCAM connection state
 * @param hasDualStream - Whether high-resolution stream is configured
 * @param getError - Error message getter
 */
export function NetcamSettings({
  config,
  onChange,
  connectionStatus,
  hasDualStream,
  getError,
}: NetcamSettingsProps) {
  const getValue = (param: string, defaultValue: string | number | boolean = '') => {
    return config[param]?.value ?? defaultValue;
  };

  return (
    <FormSection
      title="Network Camera"
      description="IP camera connection settings (RTSP/HTTP)"
      collapsible
      defaultOpen={false}
    >
      <div className="space-y-4">
        {/* Connection Status Indicator */}
        {connectionStatus && (
          <div className="flex items-center gap-3 pb-2">
            <span className="text-sm text-gray-400">Connection:</span>
            <NetcamStatusBadge status={connectionStatus} />
          </div>
        )}

        {/* Primary Stream URL */}
        <FormInput
          label="Stream URL"
          value={String(getValue('netcam_url', ''))}
          onChange={(val) => onChange('netcam_url', val)}
          placeholder="rtsp://192.168.1.100:554/stream"
          helpText="RTSP, HTTP, HTTPS, or file:// URL for the camera stream"
          error={getError?.('netcam_url')}
        />

        {/* Authentication */}
        <FormInput
          label="Credentials"
          value={String(getValue('netcam_userpass', ''))}
          onChange={(val) => onChange('netcam_userpass', val)}
          type="password"
          placeholder="username:password"
          helpText="Leave empty if camera doesn't require authentication"
          error={getError?.('netcam_userpass')}
        />

        {/* FFmpeg Parameters */}
        <FormInput
          label="FFmpeg Parameters"
          value={String(getValue('netcam_params', ''))}
          onChange={(val) => onChange('netcam_params', val)}
          placeholder="-rtsp_transport tcp"
          helpText="Advanced: Custom FFmpeg input options (e.g., -rtsp_transport tcp)"
          error={getError?.('netcam_params')}
        />

        {/* Dual Stream Configuration */}
        {hasDualStream && (
          <>
            <div className="border-t border-gray-700 my-4 pt-4">
              <h4 className="text-sm font-medium text-gray-300 mb-3">
                High Resolution Stream
              </h4>
              <p className="text-xs text-gray-400 mb-4">
                Optional secondary stream for higher quality recordings while using lower
                resolution for motion detection.
              </p>
            </div>

            <FormInput
              label="High-Res URL"
              value={String(getValue('netcam_high_url', ''))}
              onChange={(val) => onChange('netcam_high_url', val)}
              placeholder="rtsp://192.168.1.100:554/stream1"
              helpText="Optional: Higher resolution stream for recordings"
              error={getError?.('netcam_high_url')}
            />

            <FormInput
              label="High-Res FFmpeg Parameters"
              value={String(getValue('netcam_high_params', ''))}
              onChange={(val) => onChange('netcam_high_params', val)}
              placeholder="-rtsp_transport tcp"
              helpText="FFmpeg parameters for high-resolution stream"
              error={getError?.('netcam_high_params')}
            />
          </>
        )}

        {/* Information Box */}
        <div className="mt-4 p-4 bg-gray-800 border border-gray-700 rounded">
          <p className="text-sm text-gray-300 mb-2">
            <strong>Supported Protocols</strong>
          </p>
          <ul className="text-sm text-gray-400 list-disc list-inside space-y-1 ml-2">
            <li>
              <strong>RTSP:</strong> <code className="text-xs bg-gray-900 px-1 py-0.5 rounded">rtsp://</code> - Most IP cameras
            </li>
            <li>
              <strong>HTTP:</strong> <code className="text-xs bg-gray-900 px-1 py-0.5 rounded">http://</code> - MJPEG streams
            </li>
            <li>
              <strong>HTTPS:</strong> <code className="text-xs bg-gray-900 px-1 py-0.5 rounded">https://</code> - Secure streams
            </li>
            <li>
              <strong>File:</strong> <code className="text-xs bg-gray-900 px-1 py-0.5 rounded">file://</code> - Local video files
            </li>
          </ul>
        </div>
      </div>
    </FormSection>
  );
}

/**
 * NETCAM Status Badge Component
 *
 * Visual indicator for network camera connection state
 */
function NetcamStatusBadge({ status }: { status: NetcamConnectionStatus }) {
  const styles: Record<NetcamConnectionStatus, { color: string; text: string }> = {
    connected: { color: 'bg-green-500/20 text-green-300 border-green-500/30', text: 'Connected' },
    reading: { color: 'bg-blue-500/20 text-blue-300 border-blue-500/30', text: 'Reading' },
    not_connected: { color: 'bg-red-500/20 text-red-300 border-red-500/30', text: 'Not Connected' },
    reconnecting: { color: 'bg-yellow-500/20 text-yellow-300 border-yellow-500/30', text: 'Reconnecting' },
    unknown: { color: 'bg-gray-500/20 text-gray-400 border-gray-500/30', text: 'Unknown' },
  };

  const { color, text } = styles[status] || styles.unknown;

  return (
    <span className={`inline-flex items-center px-3 py-1 rounded border text-xs font-medium ${color}`}>
      <span
        className={`w-2 h-2 rounded-full mr-2 ${
          status === 'connected'
            ? 'bg-green-400'
            : status === 'reading'
            ? 'bg-blue-400 animate-pulse'
            : status === 'reconnecting'
            ? 'bg-yellow-400 animate-pulse'
            : 'bg-red-400'
        }`}
      />
      {text}
    </span>
  );
}
