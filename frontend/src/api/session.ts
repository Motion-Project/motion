/**
 * Session Token Management
 *
 * Stores session tokens in memory by default (most secure).
 * Can optionally use sessionStorage for persistence across page reloads.
 */

interface SessionData {
  sessionToken: string;
  csrfToken: string;
  role: 'admin' | 'user';
  expiresAt: number;  // Unix timestamp
}

// In-memory storage (default - clears on page close)
let session: SessionData | null = null;

// Storage key for optional persistence
const STORAGE_KEY = 'motion_session';

/**
 * Store session after successful login
 */
export function setSession(data: {
  session_token: string;
  csrf_token: string;
  role: 'admin' | 'user';
  expires_in: number;
}, persist: boolean = false): void {
  session = {
    sessionToken: data.session_token,
    csrfToken: data.csrf_token,
    role: data.role,
    expiresAt: Date.now() + (data.expires_in * 1000),
  };

  if (persist) {
    try {
      sessionStorage.setItem(STORAGE_KEY, JSON.stringify(session));
    } catch {
      // sessionStorage unavailable (private browsing, etc.)
    }
  }
}

/**
 * Get current session token for API requests
 */
export function getSessionToken(): string | null {
  const s = getSession();
  return s?.sessionToken ?? null;
}

/**
 * Get CSRF token for state-changing requests
 */
export function getCsrfToken(): string | null {
  const s = getSession();
  return s?.csrfToken ?? null;
}

/**
 * Update the session's CSRF token
 * Called when config responses include a newer CSRF token
 */
export function updateSessionCsrf(newCsrfToken: string): void {
  if (!session) return;

  session.csrfToken = newCsrfToken;

  // Persist if using sessionStorage
  try {
    const stored = sessionStorage.getItem(STORAGE_KEY);
    if (stored) {
      sessionStorage.setItem(STORAGE_KEY, JSON.stringify(session));
    }
  } catch {
    // Ignore storage errors
  }
}

/**
 * Get user role
 */
export function getRole(): 'admin' | 'user' | null {
  const s = getSession();
  return s?.role ?? null;
}

/**
 * Check if user is authenticated
 */
export function isAuthenticated(): boolean {
  const s = getSession();
  if (!s) return false;

  // Check expiration
  if (Date.now() > s.expiresAt) {
    clearSession();
    return false;
  }

  return true;
}

/**
 * Clear session (logout)
 */
export function clearSession(): void {
  session = null;
  try {
    sessionStorage.removeItem(STORAGE_KEY);
  } catch {
    // Ignore
  }
}

/**
 * Restore session from sessionStorage if available
 */
export function restoreSession(): boolean {
  if (session) return true;

  try {
    const stored = sessionStorage.getItem(STORAGE_KEY);
    if (stored) {
      session = JSON.parse(stored);

      // Validate expiration
      if (session && Date.now() > session.expiresAt) {
        clearSession();
        return false;
      }

      return session !== null;
    }
  } catch {
    // Ignore parse errors
  }

  return false;
}

/**
 * Get full session data
 */
function getSession(): SessionData | null {
  if (!session) {
    restoreSession();
  }
  return session;
}
