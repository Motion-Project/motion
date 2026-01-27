import { useQuery } from '@tanstack/react-query';
import { apiGet } from '../api/client';
import type { CameraCapabilities } from '../api/types';

/**
 * Fetch camera capabilities from status.json
 * Returns supportedControls map for the specified camera
 */
export function useCameraCapabilities(cameraId: number) {
  return useQuery({
    queryKey: ['cameraCapabilities', cameraId],
    queryFn: async (): Promise<CameraCapabilities> => {
      const response = await apiGet<{ status: Record<string, unknown> }>('/0/status.json');

      // Find the camera in the response
      const camKey = `cam${cameraId}`;
      const cameraStatus = response.status[camKey] as { supportedControls?: CameraCapabilities } | undefined;

      if (cameraStatus && typeof cameraStatus === 'object') {
        return cameraStatus.supportedControls || {};
      }

      // Camera not found or no capabilities - return empty (show all controls as fallback)
      return {};
    },
    staleTime: 60000, // Cache for 1 minute (capabilities don't change at runtime)
    retry: 1,
    enabled: cameraId > 0, // Only fetch for actual cameras, not global (0)
  });
}

// Helper functions for common capability checks
export function hasAutofocus(capabilities?: CameraCapabilities): boolean {
  return capabilities?.AfMode === true;
}

export function hasManualFocus(capabilities?: CameraCapabilities): boolean {
  return capabilities?.LensPosition === true;
}
