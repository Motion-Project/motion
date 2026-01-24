import { type ReactNode } from 'react';
import { useQuery, useQueryClient } from '@tanstack/react-query';
import { useNavigate } from 'react-router-dom';
import { getAuthStatus } from '@/api/auth';
import { restoreSession } from '@/api/session';
import { LoginPage } from './LoginPage';

// Restore session synchronously on module load
restoreSession();

interface AuthGateProps {
  children: ReactNode;
}

export function AuthGate({ children }: AuthGateProps) {
  const queryClient = useQueryClient();
  const navigate = useNavigate();

  // Check auth status from server
  const { data: authStatus, isLoading, error } = useQuery({
    queryKey: ['auth', 'status'],
    queryFn: getAuthStatus,
    retry: 1,
    staleTime: 30000,  // 30 seconds
  });

  // Handle successful login - redirect to Dashboard
  const handleLoginSuccess = () => {
    queryClient.invalidateQueries({ queryKey: ['auth'] });
    navigate('/');  // Redirect to Dashboard after login
  };

  // Still loading
  if (isLoading) {
    return (
      <div className="min-h-screen bg-surface flex items-center justify-center">
        <div className="animate-pulse text-text-secondary">Loading...</div>
      </div>
    );
  }

  // Connection error
  if (error) {
    return (
      <div className="min-h-screen bg-surface flex items-center justify-center">
        <div className="text-red-500">
          Unable to connect to Motion. Please check the server.
        </div>
      </div>
    );
  }

  // Auth not required - show app
  if (!authStatus?.auth_required) {
    return <>{children}</>;
  }

  // Auth required - check if authenticated (trust the query result)
  if (!authStatus?.authenticated) {
    return <LoginPage onSuccess={handleLoginSuccess} />;
  }

  // Authenticated - show app
  return <>{children}</>;
}
