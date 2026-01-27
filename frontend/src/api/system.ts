import { apiPost } from './client';

export interface SystemPowerResponse {
  success: true;
  operation: 'reboot' | 'shutdown' | 'service-restart';
  message: string;
}

export interface SystemPowerError {
  error: string;
}

/**
 * Reboot the Raspberry Pi
 * Requires power control to be enabled via webcontrol_actions power=on
 */
export async function systemReboot(): Promise<SystemPowerResponse> {
  const response = await apiPost<SystemPowerResponse>('/0/api/system/reboot', {});
  return response;
}

/**
 * Shutdown the Raspberry Pi
 * Requires power control to be enabled via webcontrol_actions power=on
 */
export async function systemShutdown(): Promise<SystemPowerResponse> {
  const response = await apiPost<SystemPowerResponse>('/0/api/system/shutdown', {});
  return response;
}

/**
 * Restart the Motion service
 * Requires service control to be enabled via webcontrol_actions service=on
 */
export async function systemServiceRestart(): Promise<SystemPowerResponse> {
  const response = await apiPost<SystemPowerResponse>('/0/api/system/service-restart', {});
  return response;
}
