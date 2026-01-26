import { useMemo } from 'react';
import { useSystemStatus } from '@/api/queries';
import type {
  CameraType,
  CameraCapabilities,
  V4L2Control,
  NetcamConnectionStatus,
} from '@/api/types';

export interface CameraInfo {
  isLoading: boolean;
  cameraType: CameraType;
  cameraDevice: string;
  isConnected: boolean;
  features: {
    hasLibcamControls: boolean;
    hasV4L2Controls: boolean;
    hasNetcamConfig: boolean;
    hasDualStream: boolean;
    supportsPassthrough: boolean;
  };
  // Type-specific data
  libcamCapabilities?: CameraCapabilities;
  v4l2Controls?: V4L2Control[];
  netcamStatus?: NetcamConnectionStatus;
}

/**
 * Central camera info hook with feature detection
 *
 * Returns camera type, device info, connection status, and feature flags
 * for conditional rendering of type-specific settings components.
 *
 * @param cameraId - Camera ID (1-based)
 * @returns CameraInfo with type detection and feature flags
 *
 * @example
 * ```tsx
 * const cameraInfo = useCameraInfo(1);
 *
 * if (cameraInfo.features.hasLibcamControls) {
 *   return <LibcameraSettings ... />;
 * }
 *
 * if (cameraInfo.features.hasV4L2Controls) {
 *   return <V4L2Settings controls={cameraInfo.v4l2Controls} ... />;
 * }
 * ```
 */
export function useCameraInfo(cameraId: number): CameraInfo {
  const { data: status, isLoading } = useSystemStatus();

  return useMemo(() => {
    // Return loading state if data not yet fetched
    if (isLoading || !status?.status) {
      return {
        isLoading: true,
        cameraType: 'unknown',
        cameraDevice: '',
        isConnected: false,
        features: {
          hasLibcamControls: false,
          hasV4L2Controls: false,
          hasNetcamConfig: false,
          hasDualStream: false,
          supportsPassthrough: false,
        },
      };
    }

    // Get camera status from response
    const camKey = `cam${cameraId}` as `cam${number}`;
    const cam = status.status[camKey];

    // If camera not found, return unknown state
    if (!cam) {
      return {
        isLoading: false,
        cameraType: 'unknown',
        cameraDevice: '',
        isConnected: false,
        features: {
          hasLibcamControls: false,
          hasV4L2Controls: false,
          hasNetcamConfig: false,
          hasDualStream: false,
          supportsPassthrough: false,
        },
      };
    }

    const cameraType = cam.camera_type ?? 'unknown';

    return {
      isLoading: false,
      cameraType,
      cameraDevice: cam.camera_device ?? '',
      isConnected: !cam.lost_connection,
      features: {
        // libcam: Has supportedControls map
        hasLibcamControls: cameraType === 'libcam',
        // v4l2: Has v4l2_controls array
        hasV4L2Controls: cameraType === 'v4l2',
        // netcam: Has netcam_url and related config
        hasNetcamConfig: cameraType === 'netcam',
        // netcam dual stream: has_high_stream is true
        hasDualStream: cameraType === 'netcam' && cam.has_high_stream === true,
        // netcam passthrough: only netcam supports movie passthrough
        supportsPassthrough: cameraType === 'netcam',
      },
      // Type-specific data (conditionally included)
      ...(cameraType === 'libcam' && { libcamCapabilities: cam.supportedControls }),
      ...(cameraType === 'v4l2' && { v4l2Controls: cam.v4l2_controls }),
      ...(cameraType === 'netcam' && { netcamStatus: cam.netcam_status }),
    };
  }, [status, cameraId, isLoading]);
}
