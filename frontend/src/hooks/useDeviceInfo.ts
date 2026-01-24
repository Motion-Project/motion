import { useQuery } from '@tanstack/react-query';

export interface DeviceInfo {
  device_model?: string;
  pi_generation?: number;
  hardware_encoders?: {
    h264_v4l2m2m: boolean;
  };
  temperature?: {
    celsius: number;
    fahrenheit: number;
  };
  uptime?: {
    seconds: number;
    days: number;
    hours: number;
  };
  memory?: {
    total: number;
    used: number;
    free: number;
    available: number;
    percent: number;
  };
  disk?: {
    total: number;
    used: number;
    free: number;
    available: number;
    percent: number;
  };
  version?: string;
}

export function useDeviceInfo() {
  return useQuery({
    queryKey: ['deviceInfo'],
    queryFn: async (): Promise<DeviceInfo> => {
      const response = await fetch('/0/api/system/status');
      if (!response.ok) {
        throw new Error('Failed to fetch device info');
      }
      return response.json();
    },
    staleTime: 60000, // Cache for 1 minute
    retry: 1,
  });
}

// Helper functions for device detection
export function isPi5(deviceInfo?: DeviceInfo): boolean {
  return deviceInfo?.pi_generation === 5;
}

export function isPi4(deviceInfo?: DeviceInfo): boolean {
  return deviceInfo?.pi_generation === 4;
}

export function isPi3(deviceInfo?: DeviceInfo): boolean {
  return deviceInfo?.pi_generation === 3;
}

export function isRaspberryPi(deviceInfo?: DeviceInfo): boolean {
  return (deviceInfo?.pi_generation ?? 0) > 0;
}

export function hasHardwareEncoder(deviceInfo?: DeviceInfo): boolean {
  return deviceInfo?.hardware_encoders?.h264_v4l2m2m === true;
}

export function isHighTemperature(deviceInfo?: DeviceInfo, threshold = 70): boolean {
  return (deviceInfo?.temperature?.celsius ?? 0) > threshold;
}
