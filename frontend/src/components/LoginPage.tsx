import { useState, useCallback, useRef, useEffect, type FormEvent } from 'react';
import { login } from '@/api/auth';

interface LoginPageProps {
  onSuccess: () => void;
}

export function LoginPage({ onSuccess }: LoginPageProps) {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [rememberMe, setRememberMe] = useState(false);
  const [error, setError] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const usernameRef = useRef<HTMLInputElement>(null);

  // Focus username on mount
  useEffect(() => {
    usernameRef.current?.focus();
  }, []);

  const handleSubmit = useCallback(async (e: FormEvent) => {
    e.preventDefault();
    setError('');
    setIsLoading(true);

    const result = await login(username, password, rememberMe);

    setIsLoading(false);

    if (result.success) {
      onSuccess();
    } else {
      setError(result.error ?? 'Login failed');
    }
  }, [username, password, rememberMe, onSuccess]);

  return (
    <div className="min-h-screen bg-surface flex items-center justify-center p-4">
      <div className="w-full max-w-md">
        {/* Logo */}
        <div className="text-center mb-8">
          <h1 className="text-3xl font-bold">Motion</h1>
          <p className="text-gray-400 mt-2">Video Motion Detection</p>
        </div>

        {/* Login Card */}
        <div className="bg-surface-elevated rounded-lg shadow-xl p-6">
          <h2 className="text-xl font-semibold mb-6">Sign In</h2>

          <form onSubmit={handleSubmit}>
            <div className="mb-4">
              <label htmlFor="username" className="block text-sm font-medium mb-1">
                Username
              </label>
              <input
                ref={usernameRef}
                id="username"
                type="text"
                value={username}
                onChange={(e) => setUsername(e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-gray-700 rounded-lg
                         focus:outline-none focus:ring-2 focus:ring-primary"
                required
                autoComplete="username"
                disabled={isLoading}
              />
            </div>

            <div className="mb-4">
              <label htmlFor="password" className="block text-sm font-medium mb-1">
                Password
              </label>
              <input
                id="password"
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-gray-700 rounded-lg
                         focus:outline-none focus:ring-2 focus:ring-primary"
                required
                autoComplete="current-password"
                disabled={isLoading}
              />
            </div>

            <div className="mb-6">
              <label className="flex items-center gap-2 cursor-pointer">
                <input
                  type="checkbox"
                  checked={rememberMe}
                  onChange={(e) => setRememberMe(e.target.checked)}
                  className="rounded border-gray-700 bg-surface text-primary
                           focus:ring-primary focus:ring-offset-0"
                  disabled={isLoading}
                />
                <span className="text-sm text-gray-400">
                  Remember me (until browser closes)
                </span>
              </label>
            </div>

            {error && (
              <div className="mb-4 p-3 bg-red-600/10 border border-red-600/30
                            rounded-lg text-red-200 text-sm">
                {error}
              </div>
            )}

            <button
              type="submit"
              className="w-full py-2 bg-primary hover:bg-primary-hover rounded-lg
                       transition-colors disabled:opacity-50"
              disabled={isLoading}
            >
              {isLoading ? 'Signing in...' : 'Sign In'}
            </button>
          </form>
        </div>

        {/* Security note */}
        <p className="text-xs text-gray-500 text-center mt-4">
          Use HTTPS in production for secure authentication
        </p>
      </div>
    </div>
  );
}
