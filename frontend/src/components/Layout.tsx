import { useState, memo } from 'react'
import { Outlet, Link } from 'react-router-dom'
import { useQueryClient } from '@tanstack/react-query'
import { useAuthContext } from '@/contexts/AuthContext'
import { logout } from '@/api/auth'
import { SystemStatus, VersionDisplay } from '@/components/SystemStatus'

export const Layout = memo(function Layout() {
  const queryClient = useQueryClient()
  const { isAuthenticated, role, authRequired } = useAuthContext()
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false)

  const handleLogout = async () => {
    await logout()
    // Invalidate auth queries - this triggers AuthContext and AuthGate to update
    queryClient.invalidateQueries({ queryKey: ['auth'] })
  }

  return (
    <div className="min-h-screen bg-surface">
      <header className="bg-surface-elevated border-b border-gray-800 sticky top-0 z-[150]">
        <div className="container mx-auto px-4 py-3 md:py-4">
          <nav className="flex items-center justify-between">
            {/* Logo and version */}
            <div className="flex items-center gap-2 md:gap-4">
              <h1 className="text-xl md:text-2xl font-bold">Motion</h1>
              <VersionDisplay />
            </div>

            {/* Desktop navigation */}
            <div className="hidden md:flex items-center gap-6">
              <div className="flex items-center gap-4">
                <Link to="/" className="hover:text-primary">Dashboard</Link>
                {role === 'admin' && (
                  <Link to="/settings" className="hover:text-primary">Settings</Link>
                )}
                <Link to="/media" className="hover:text-primary">Media</Link>
                {authRequired && isAuthenticated && (
                  <button
                    onClick={handleLogout}
                    className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg hover:bg-surface-elevated transition-colors"
                    title="Logout"
                  >
                    <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M17 16l4-4m0 0l-4-4m4 4H7m6 4v1a3 3 0 01-3 3H6a3 3 0 01-3-3V7a3 3 0 013-3h4a3 3 0 013 3v1" />
                    </svg>
                    <span className="text-xs text-green-500">
                      {role === 'admin' ? 'Admin' : 'User'}
                    </span>
                  </button>
                )}
                {!authRequired && (
                  <div className="px-3 py-1.5 rounded-lg bg-yellow-500/10">
                    <span className="text-xs text-yellow-500">No Authentication</span>
                  </div>
                )}
              </div>
              <SystemStatus variant="desktop" />
            </div>

            {/* Mobile menu button */}
            <div className="flex items-center gap-2 md:hidden">
              {/* Compact system stats for mobile */}
              <SystemStatus variant="mobile" />
              <button
                onClick={() => setMobileMenuOpen(!mobileMenuOpen)}
                className="p-2 rounded-lg hover:bg-surface transition-colors"
                aria-label="Toggle menu"
              >
                {mobileMenuOpen ? (
                  <svg className="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
                  </svg>
                ) : (
                  <svg className="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 6h16M4 12h16M4 18h16" />
                  </svg>
                )}
              </button>
            </div>
          </nav>

          {/* Mobile menu dropdown */}
          {mobileMenuOpen && (
            <div className="md:hidden mt-3 pt-3 border-t border-gray-800">
              <div className="flex flex-col gap-2">
                <Link
                  to="/"
                  onClick={() => setMobileMenuOpen(false)}
                  className="px-3 py-2 rounded-lg hover:bg-surface transition-colors"
                >
                  Dashboard
                </Link>
                {role === 'admin' && (
                  <Link
                    to="/settings"
                    onClick={() => setMobileMenuOpen(false)}
                    className="px-3 py-2 rounded-lg hover:bg-surface transition-colors"
                  >
                    Settings
                  </Link>
                )}
                <Link
                  to="/media"
                  onClick={() => setMobileMenuOpen(false)}
                  className="px-3 py-2 rounded-lg hover:bg-surface transition-colors"
                >
                  Media
                </Link>
                {authRequired && isAuthenticated && (
                  <button
                    onClick={() => {
                      handleLogout()
                      setMobileMenuOpen(false)
                    }}
                    className="flex items-center gap-2 px-3 py-2 rounded-lg hover:bg-surface transition-colors text-left"
                  >
                    <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M17 16l4-4m0 0l-4-4m4 4H7m6 4v1a3 3 0 01-3 3H6a3 3 0 01-3-3V7a3 3 0 013-3h4a3 3 0 013 3v1" />
                    </svg>
                    <span className="text-green-500">
                      Logout ({role === 'admin' ? 'Admin' : 'User'})
                    </span>
                  </button>
                )}
                {!authRequired && (
                  <div className="px-3 py-2 rounded-lg bg-yellow-500/10">
                    <span className="text-xs text-yellow-500">No Authentication</span>
                  </div>
                )}

                {/* Mobile system stats */}
                <SystemStatus variant="mobile-menu" />
              </div>
            </div>
          )}
        </div>
      </header>

      <main className="container mx-auto px-4 py-4 md:py-8">
        <Outlet />
      </main>
    </div>
  )
})
