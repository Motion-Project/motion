import { getSessionToken, getCsrfToken, clearSession, updateSessionCsrf } from './session';

/**
 * Request timeout in milliseconds
 * Set to 10 seconds to balance responsiveness vs slow network conditions
 * Can be overridden via VITE_API_TIMEOUT environment variable
 */
const API_TIMEOUT = parseInt(import.meta.env.VITE_API_TIMEOUT ?? '10000', 10);
/**
 * Longer timeout for action commands (config_write, restart)
 * These involve disk I/O and process management which can take longer
 */
const ACTION_TIMEOUT = 30000;
const MAX_RETRIES = 1; // Max retries for transient failures

/**
 * Error code to user-friendly message mapping
 * Based on HTTP status codes and Motion-specific error patterns
 */
const ERROR_MESSAGES: Record<number, string> = {
  400: 'Invalid request. Please check your input.',
  401: 'Authentication required. Please sign in.',
  403: 'Access denied. You do not have permission for this action.',
  404: 'Resource not found.',
  408: 'Request timed out. Please try again.',
  500: 'Server error. Please try again later.',
  502: 'Server unavailable. Motion may be restarting.',
  503: 'Service unavailable. Motion may be restarting.',
  504: 'Gateway timeout. Please try again.',
};

/**
 * Callback type for authentication errors
 * Called when 401/403 is received and auth may be required
 */
type AuthErrorCallback = (status: number) => void;

/** Global auth error callback - set by AuthProvider */
let authErrorCallback: AuthErrorCallback | null = null;

/**
 * Register a callback to be called on authentication errors
 */
export function setAuthErrorCallback(callback: AuthErrorCallback | null): void {
  authErrorCallback = callback;
}

class ApiClientError extends Error {
  status?: number;
  userMessage: string;

  constructor(message: string, status?: number) {
    super(message);
    this.name = 'ApiClientError';
    this.status = status;
    // User-friendly message
    this.userMessage = status ? (ERROR_MESSAGES[status] ?? message) : message;
  }
}

/**
 * Safely parse JSON from response, wrapping parse errors in ApiClientError
 */
async function safeJsonParse<T>(response: Response): Promise<T> {
  try {
    return await response.json();
  } catch {
    throw new ApiClientError('Invalid JSON response from server', response.status);
  }
}

/**
 * Safely parse JSON text, wrapping parse errors in ApiClientError
 */
function safeParseText<T>(text: string, status?: number): T {
  if (!text) return {} as T;
  try {
    return JSON.parse(text);
  } catch {
    throw new ApiClientError('Invalid JSON response from server', status);
  }
}

/**
 * Check if error is transient and should be retried
 */
function isTransientError(status: number): boolean {
  return status === 502 || status === 503 || status === 504;
}

/**
 * Sleep for specified milliseconds
 */
function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export async function apiGet<T>(endpoint: string, retryCount = 0): Promise<T> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), API_TIMEOUT);

  const sessionToken = getSessionToken();

  try {
    const response = await fetch(endpoint, {
      method: 'GET',
      headers: {
        'Accept': 'application/json',
        ...(sessionToken && { 'X-Session-Token': sessionToken }),
      },
      credentials: 'same-origin',
      signal: controller.signal,
    });

    clearTimeout(timeoutId);

    if (!response.ok) {
      // Handle authentication errors
      if (response.status === 401) {
        // Session expired or invalid
        clearSession();
        authErrorCallback?.(401);
        throw new ApiClientError('Session expired', 401);
      }

      // Handle transient errors with retry
      if (isTransientError(response.status) && retryCount < MAX_RETRIES) {
        await sleep(1000 * (retryCount + 1)); // Exponential backoff
        return apiGet<T>(endpoint, retryCount + 1);
      }

      throw new ApiClientError(`HTTP ${response.status}: ${response.statusText}`, response.status);
    }

    return safeJsonParse<T>(response);
  } catch (error) {
    clearTimeout(timeoutId);
    if (error instanceof ApiClientError) throw error;
    if (error instanceof Error && error.name === 'AbortError') {
      throw new ApiClientError('Request timeout', 408);
    }
    throw new ApiClientError(error instanceof Error ? error.message : 'Unknown error');
  }
}

export async function apiPost<T>(
  endpoint: string,
  data: Record<string, unknown>,
  retryCount = 0,
  timeout = API_TIMEOUT
): Promise<T> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), timeout);

  const sessionToken = getSessionToken();
  const csrfToken = getCsrfToken();

  try {
    const response = await fetch(endpoint, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
        ...(sessionToken && { 'X-Session-Token': sessionToken }),
        ...(csrfToken && { 'X-CSRF-Token': csrfToken }),
      },
      credentials: 'same-origin',
      body: JSON.stringify(data),
      signal: controller.signal,
    });

    clearTimeout(timeoutId);

    // Handle authentication errors
    if (response.status === 401) {
      clearSession();
      authErrorCallback?.(401);
      throw new ApiClientError('Session expired', 401);
    }

    // Handle 403 - CSRF token may be stale
    if (response.status === 403) {
      // Don't clear session! CSRF mismatch doesn't mean session is invalid.
      // Fetch fresh CSRF token from config endpoint
      try {
        const configResponse = await fetch('/0/api/config', {
          headers: {
            'Accept': 'application/json',
            ...(sessionToken && { 'X-Session-Token': sessionToken }),
          },
          signal: controller.signal,
        });

        if (configResponse.ok) {
          const config = await safeJsonParse<{ csrf_token?: string }>(configResponse);
          if (config.csrf_token) {
            // Update session's CSRF token
            updateSessionCsrf(config.csrf_token);

            // Retry with new CSRF token AND session token
            const retryResponse = await fetch(endpoint, {
              method: 'POST',
              headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json',
                ...(sessionToken && { 'X-Session-Token': sessionToken }),
                'X-CSRF-Token': config.csrf_token,
              },
              credentials: 'same-origin',
              body: JSON.stringify(data),
              signal: controller.signal,
            });

            if (retryResponse.ok) {
              const text = await retryResponse.text();
              return safeParseText<T>(text, retryResponse.status);
            }

            // Retry failed - now we can report the error
            if (retryResponse.status === 401 || retryResponse.status === 403) {
              // Session is truly invalid
              clearSession();
              authErrorCallback?.(retryResponse.status);
            }
            throw new ApiClientError('Request failed after CSRF refresh', retryResponse.status);
          }
        }
      } catch (refreshError) {
        // If CSRF refresh itself fails, report original 403
        if (refreshError instanceof ApiClientError) throw refreshError;
      }

      // Couldn't refresh CSRF - session may be invalid
      clearSession();
      authErrorCallback?.(403);
      throw new ApiClientError('CSRF validation failed', 403);
    }

    // Handle transient errors with retry
    if (isTransientError(response.status) && retryCount < MAX_RETRIES) {
      await sleep(1000 * (retryCount + 1)); // Exponential backoff
      return apiPost<T>(endpoint, data, retryCount + 1, timeout);
    }

    if (!response.ok) {
      throw new ApiClientError(`HTTP ${response.status}: ${response.statusText}`, response.status);
    }

    // Some endpoints return empty response
    const text = await response.text();
    return safeParseText<T>(text, response.status);
  } catch (error) {
    clearTimeout(timeoutId);
    if (error instanceof ApiClientError) throw error;
    if (error instanceof Error && error.name === 'AbortError') {
      throw new ApiClientError('Request timeout', 408);
    }
    throw new ApiClientError(error instanceof Error ? error.message : 'Unknown error');
  }
}

/**
 * PATCH request for batch configuration updates
 */
export async function apiPatch<T>(
  endpoint: string,
  data: Record<string, unknown>,
  retryCount = 0
): Promise<T> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), API_TIMEOUT);

  const sessionToken = getSessionToken();
  const csrfToken = getCsrfToken();

  try {
    const response = await fetch(endpoint, {
      method: 'PATCH',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
        ...(sessionToken && { 'X-Session-Token': sessionToken }),
        ...(csrfToken && { 'X-CSRF-Token': csrfToken }),
      },
      credentials: 'same-origin',
      body: JSON.stringify(data),
      signal: controller.signal,
    });

    clearTimeout(timeoutId);

    // Handle authentication errors
    if (response.status === 401) {
      clearSession();
      authErrorCallback?.(401);
      throw new ApiClientError('Session expired', 401);
    }

    // Handle 403 - CSRF token may be stale
    if (response.status === 403) {
      // Don't clear session! CSRF mismatch doesn't mean session is invalid.
      // Fetch fresh CSRF token from config endpoint
      try {
        const configResponse = await fetch('/0/api/config', {
          headers: {
            'Accept': 'application/json',
            ...(sessionToken && { 'X-Session-Token': sessionToken }),
          },
          signal: controller.signal,
        });

        if (configResponse.ok) {
          const config = await safeJsonParse<{ csrf_token?: string }>(configResponse);
          if (config.csrf_token) {
            // Update session's CSRF token
            updateSessionCsrf(config.csrf_token);

            // Retry with new CSRF token AND session token
            const retryResponse = await fetch(endpoint, {
              method: 'PATCH',
              headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json',
                ...(sessionToken && { 'X-Session-Token': sessionToken }),
                'X-CSRF-Token': config.csrf_token,
              },
              credentials: 'same-origin',
              body: JSON.stringify(data),
              signal: controller.signal,
            });

            if (retryResponse.ok) {
              const text = await retryResponse.text();
              return safeParseText<T>(text, retryResponse.status);
            }

            // Retry failed - now we can report the error
            if (retryResponse.status === 401 || retryResponse.status === 403) {
              // Session is truly invalid
              clearSession();
              authErrorCallback?.(retryResponse.status);
            }
            throw new ApiClientError('Request failed after CSRF refresh', retryResponse.status);
          }
        }
      } catch (refreshError) {
        // If CSRF refresh itself fails, report original 403
        if (refreshError instanceof ApiClientError) throw refreshError;
      }

      // Couldn't refresh CSRF - session may be invalid
      clearSession();
      authErrorCallback?.(403);
      throw new ApiClientError('CSRF validation failed', 403);
    }

    // Handle transient errors with retry
    if (isTransientError(response.status) && retryCount < MAX_RETRIES) {
      await sleep(1000 * (retryCount + 1)); // Exponential backoff
      return apiPatch<T>(endpoint, data, retryCount + 1);
    }

    if (!response.ok) {
      throw new ApiClientError(`HTTP ${response.status}: ${response.statusText}`, response.status);
    }

    const text = await response.text();
    return safeParseText<T>(text, response.status);
  } catch (error) {
    clearTimeout(timeoutId);
    if (error instanceof ApiClientError) throw error;
    if (error instanceof Error && error.name === 'AbortError') {
      throw new ApiClientError('Request timeout', 408);
    }
    throw new ApiClientError(error instanceof Error ? error.message : 'Unknown error');
  }
}

/**
 * DELETE request for media file deletion
 */
export async function apiDelete<T>(endpoint: string): Promise<T> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), API_TIMEOUT);

  const sessionToken = getSessionToken();
  const csrfToken = getCsrfToken();

  try {
    const response = await fetch(endpoint, {
      method: 'DELETE',
      headers: {
        'Accept': 'application/json',
        ...(sessionToken && { 'X-Session-Token': sessionToken }),
        ...(csrfToken && { 'X-CSRF-Token': csrfToken }),
      },
      credentials: 'same-origin',
      signal: controller.signal,
    });

    clearTimeout(timeoutId);

    // Handle authentication errors
    if (response.status === 401) {
      clearSession();
      authErrorCallback?.(401);
      throw new ApiClientError('Session expired', 401);
    }

    // Handle 403 - CSRF token may be stale
    if (response.status === 403) {
      // Don't clear session! CSRF mismatch doesn't mean session is invalid.
      // Fetch fresh CSRF token from config endpoint
      try {
        const configResponse = await fetch('/0/api/config', {
          headers: {
            'Accept': 'application/json',
            ...(sessionToken && { 'X-Session-Token': sessionToken }),
          },
          signal: controller.signal,
        });

        if (configResponse.ok) {
          const config = await safeJsonParse<{ csrf_token?: string }>(configResponse);
          if (config.csrf_token) {
            // Update session's CSRF token
            updateSessionCsrf(config.csrf_token);

            // Retry with new CSRF token AND session token
            const retryResponse = await fetch(endpoint, {
              method: 'DELETE',
              headers: {
                'Accept': 'application/json',
                ...(sessionToken && { 'X-Session-Token': sessionToken }),
                'X-CSRF-Token': config.csrf_token,
              },
              credentials: 'same-origin',
              signal: controller.signal,
            });

            if (retryResponse.ok) {
              const text = await retryResponse.text();
              return safeParseText<T>(text, retryResponse.status);
            }

            // Retry failed - now we can report the error
            if (retryResponse.status === 401 || retryResponse.status === 403) {
              // Session is truly invalid
              clearSession();
              authErrorCallback?.(retryResponse.status);
            }
            throw new ApiClientError('Request failed after CSRF refresh', retryResponse.status);
          }
        }
      } catch (refreshError) {
        // If CSRF refresh itself fails, report original 403
        if (refreshError instanceof ApiClientError) throw refreshError;
      }

      // Couldn't refresh CSRF - session may be invalid
      clearSession();
      authErrorCallback?.(403);
      throw new ApiClientError('CSRF validation failed', 403);
    }

    if (!response.ok) {
      throw new ApiClientError(`HTTP ${response.status}: ${response.statusText}`, response.status);
    }

    const text = await response.text();
    return safeParseText<T>(text, response.status);
  } catch (error) {
    clearTimeout(timeoutId);
    if (error instanceof ApiClientError) throw error;
    if (error instanceof Error && error.name === 'AbortError') {
      throw new ApiClientError('Request timeout', 408);
    }
    throw new ApiClientError(error instanceof Error ? error.message : 'Unknown error');
  }
}

// ============================================================================
// Camera Action API Functions
// These use the new JSON API endpoints for camera control operations
// ============================================================================

interface ActionResponse {
  status: string;
}

/**
 * Write current configuration to disk
 * Uses JSON API: POST /0/api/config/write
 * Note: Config write is always global, camera ID is ignored
 */
export async function writeConfig(): Promise<void> {
  await apiPost<ActionResponse>('/0/api/config/write', {}, 0, ACTION_TIMEOUT);
}

/**
 * Fire-and-forget config write - sends command but doesn't wait for response.
 * Motion processes the command but may not return HTTP response reliably.
 * Note: Config write is always global, camera ID is ignored
 */
export function writeConfigFireAndForget(): void {
  const sessionToken = getSessionToken();
  const csrfToken = getCsrfToken();

  const controller = new AbortController();
  setTimeout(() => controller.abort(), 5000); // 5 second timeout

  fetch('/0/api/config/write', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Accept': 'application/json',
      ...(sessionToken && { 'X-Session-Token': sessionToken }),
      ...(csrfToken && { 'X-CSRF-Token': csrfToken }),
    },
    credentials: 'same-origin',
    body: JSON.stringify({}),
    signal: controller.signal,
  }).catch(() => {
    // Ignore errors - command was sent, Motion processes it
  });
}

/**
 * Restart a camera to apply configuration changes
 * Uses JSON API: POST /{camId}/api/camera/restart
 * @param camId Camera ID (0 for all cameras)
 */
export async function restartCamera(camId: number = 0): Promise<void> {
  await apiPost<ActionResponse>(`/${camId}/api/camera/restart`, {}, 0, ACTION_TIMEOUT);
}

/**
 * Fire-and-forget restart - sends command but doesn't wait for response.
 * Motion's restart command may not respond (it kills its own HTTP handler).
 * Uses JSON API: POST /{camId}/api/camera/restart
 * @param camId Camera ID (0 for all cameras)
 */
export function restartCameraFireAndForget(camId: number = 0): void {
  const sessionToken = getSessionToken();
  const csrfToken = getCsrfToken();

  const controller = new AbortController();
  setTimeout(() => controller.abort(), 2000); // 2 second timeout

  fetch(`/${camId}/api/camera/restart`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Accept': 'application/json',
      ...(sessionToken && { 'X-Session-Token': sessionToken }),
      ...(csrfToken && { 'X-CSRF-Token': csrfToken }),
    },
    credentials: 'same-origin',
    body: JSON.stringify({}),
    signal: controller.signal,
  }).catch(() => {
    // Ignore errors - expected during restart
  });
}

/**
 * Take a snapshot with the specified camera
 * Uses JSON API: POST /{camId}/api/camera/snapshot
 * @param camId Camera ID
 */
export async function takeSnapshot(camId: number): Promise<void> {
  await apiPost<ActionResponse>(`/${camId}/api/camera/snapshot`, {});
}

/**
 * Pause or unpause motion detection for a camera
 * Uses JSON API: POST /{camId}/api/camera/pause
 * @param camId Camera ID
 * @param action 'on' to pause, 'off' to unpause, 'schedule' for schedule-based
 */
export async function pauseCamera(
  camId: number,
  action: 'on' | 'off' | 'schedule'
): Promise<void> {
  await apiPost<ActionResponse>(`/${camId}/api/camera/pause`, { action });
}

/**
 * Stop a camera (will not restart automatically)
 * Uses JSON API: POST /{camId}/api/camera/stop
 * @param camId Camera ID
 */
export async function stopCamera(camId: number): Promise<void> {
  await apiPost<ActionResponse>(`/${camId}/api/camera/stop`, {});
}

/**
 * Manually trigger event start for a camera
 * Uses JSON API: POST /{camId}/api/camera/event/start
 * @param camId Camera ID
 */
export async function triggerEventStart(camId: number): Promise<void> {
  await apiPost<ActionResponse>(`/${camId}/api/camera/event/start`, {});
}

/**
 * Manually trigger event end for a camera
 * Uses JSON API: POST /{camId}/api/camera/event/end
 * @param camId Camera ID
 */
export async function triggerEventEnd(camId: number): Promise<void> {
  await apiPost<ActionResponse>(`/${camId}/api/camera/event/end`, {});
}

/**
 * Send PTZ (Pan/Tilt/Zoom) command to a camera
 * Uses JSON API: POST /{camId}/api/camera/ptz
 * @param camId Camera ID
 * @param action PTZ action (e.g., 'up', 'down', 'left', 'right', 'zoom_in', 'zoom_out')
 */
export async function sendPtzCommand(camId: number, action: string): Promise<void> {
  await apiPost<ActionResponse>(`/${camId}/api/camera/ptz`, { action });
}

/**
 * Poll camera status until it responds (camera is back online after restart)
 * @param camId Camera ID
 * @param maxAttempts Maximum polling attempts
 * @param intervalMs Polling interval in milliseconds
 * @returns true if camera came back online, false if timed out
 */
export async function waitForCameraOnline(
  _camId: number, // Currently unused - status endpoint is global
  maxAttempts: number = 30,
  intervalMs: number = 1000
): Promise<boolean> {
  const sessionToken = getSessionToken();

  for (let attempt = 0; attempt < maxAttempts; attempt++) {
    try {
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 3000);

      const response = await fetch('/0/api/system/status', {
        method: 'GET',
        headers: {
          Accept: 'application/json',
          ...(sessionToken && { 'X-Session-Token': sessionToken }),
        },
        credentials: 'same-origin',
        signal: controller.signal,
      });

      clearTimeout(timeoutId);

      if (response.ok) {
        // Camera is back online
        return true;
      }
    } catch {
      // Connection failed - camera still restarting, continue polling
    }

    // Wait before next attempt
    await new Promise((resolve) => setTimeout(resolve, intervalMs));
  }

  return false; // Timed out
}

/**
 * Write config and restart camera - used after applying restart-required parameters
 * Uses fire-and-forget for both operations since Motion may not respond reliably.
 * @param camId Camera ID
 * @returns true if camera came back online after restart
 */
export async function applyRestartRequiredChanges(camId: number): Promise<boolean> {
  // Fire config write command - don't wait for response (Motion processes but may not respond)
  writeConfigFireAndForget();

  // Brief delay to let config write complete before restart
  await new Promise((resolve) => setTimeout(resolve, 500));

  // Fire restart command - don't wait for response
  restartCameraFireAndForget(camId);

  // Wait a moment for Motion to start restarting
  await new Promise((resolve) => setTimeout(resolve, 1000));

  // Poll until camera is back online (max 30 seconds)
  return waitForCameraOnline(camId, 30, 1000);
}

export { ApiClientError };
