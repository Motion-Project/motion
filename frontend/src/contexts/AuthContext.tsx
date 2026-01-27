/**
 * Authentication Context for React UI
 *
 * Provides reactive authentication state management for the application.
 * Uses TanStack Query to automatically update when auth state changes.
 */

import {
  createContext,
  useContext,
  type ReactNode,
} from 'react'
import { useQuery } from '@tanstack/react-query'
import { getAuthStatus } from '@/api/auth'
import { isAuthenticated as checkSession, getRole as getSessionRole } from '@/api/session'

interface AuthContextValue {
  /** Whether user is currently authenticated */
  isAuthenticated: boolean
  /** User role (admin or user) or null if not authenticated */
  role: 'admin' | 'user' | null
  /** Whether auth status is still loading */
  isLoading: boolean
  /** Whether authentication is required (configured in Motion) */
  authRequired: boolean
}

const AuthContext = createContext<AuthContextValue | null>(null)

interface AuthProviderProps {
  children: ReactNode
}

export function AuthProvider({ children }: AuthProviderProps) {
  // Use TanStack Query - automatically updates when queries are invalidated
  // This fixes the reactivity issue where auth state wasn't updating after login
  const { data: authStatus, isLoading } = useQuery({
    queryKey: ['auth', 'status'],
    queryFn: getAuthStatus,
    staleTime: 30000,
    // Use session data as placeholder while loading
    placeholderData: () => ({
      auth_required: true,
      authenticated: checkSession(),
      role: getSessionRole() ?? undefined,
    }),
  })

  const value: AuthContextValue = {
    isAuthenticated: authStatus?.authenticated ?? false,
    role: authStatus?.role ?? null,
    isLoading,
    authRequired: authStatus?.auth_required ?? true,
  }

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>
}

/**
 * Hook to access authentication context
 * @throws Error if used outside of AuthProvider
 */
export function useAuthContext(): AuthContextValue {
  const context = useContext(AuthContext)
  if (!context) {
    throw new Error('useAuthContext must be used within an AuthProvider')
  }
  return context
}
