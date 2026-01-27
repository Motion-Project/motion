import { FormSection, FormInput, FormSelect, FormToggle } from '@/components/form'
import { useToast } from '@/components/Toast'
import { systemReboot, systemShutdown, systemServiceRestart } from '@/api/system'
import type { SystemStatus } from '@/api/types'

export interface SystemSettingsProps {
  config: Record<string, { value: string | number | boolean; password_set?: boolean }>
  onChange: (param: string, value: string | number | boolean) => void
  getError?: (param: string) => string | undefined
  /** Original config from server (without pending changes) - used for modified indicators */
  originalConfig?: Record<string, { value: string | number | boolean; password_set?: boolean }>
  /** System status for action availability */
  systemStatus?: SystemStatus
}

export function SystemSettings({ config, onChange, getError, originalConfig, systemStatus }: SystemSettingsProps) {
  const { addToast } = useToast()

  // Extract action availability flags
  const serviceEnabled = systemStatus?.actions?.service ?? false
  const powerEnabled = systemStatus?.actions?.power ?? false

  const getValue = (param: string, defaultValue: string | number | boolean = '') => {
    return config[param]?.value ?? defaultValue
  }

  const getOriginalValue = (param: string, defaultValue: string | number | boolean = '') => {
    return originalConfig?.[param]?.value ?? defaultValue
  }

  /** Check if a password is set for an authentication parameter */
  const isPasswordSet = (param: string) => {
    return config[param]?.password_set === true
  }

  const isOriginalPasswordSet = (param: string) => {
    return originalConfig?.[param]?.password_set === true
  }

  const handleReboot = async () => {
    if (window.confirm('Are you sure you want to reboot the Pi? The system will restart and be unavailable for about a minute.')) {
      try {
        await systemReboot()
        addToast('Rebooting... The system will be back online shortly.', 'info')
      } catch (error: unknown) {
        const err = error as { message?: string }
        addToast(
          err.message || 'Failed to reboot. Power control may be disabled in config.',
          'error'
        )
      }
    }
  }

  const handleShutdown = async () => {
    if (window.confirm('Are you sure you want to shutdown the Pi? You will need to physically power it back on.')) {
      try {
        await systemShutdown()
        addToast('Shutting down... The system will power off.', 'warning')
      } catch (error: unknown) {
        const err = error as { message?: string }
        addToast(
          err.message || 'Failed to shutdown. Power control may be disabled in config.',
          'error'
        )
      }
    }
  }

  const handleServiceRestart = async () => {
    if (window.confirm('Are you sure you want to restart the Motion service? Active streams will be interrupted briefly.')) {
      try {
        await systemServiceRestart()
        addToast('Restarting Motion... Streams will resume shortly.', 'info')
      } catch (error: unknown) {
        const err = error as { message?: string }
        addToast(
          err.message || 'Failed to restart service. Service control may be disabled in config.',
          'error'
        )
      }
    }
  }

  return (
    <>
      {/* Device Controls - FIRST section for quick access */}
      <FormSection
        title="Device Controls"
        description="Service and system power management"
        collapsible
        defaultOpen={false}
      >
        <div className="flex flex-col gap-4">
          {/* Service Controls */}
          <div>
            <p className="text-xs text-gray-400 mb-2">Service Control</p>
            <button
              onClick={handleServiceRestart}
              disabled={!serviceEnabled}
              className={`px-4 py-2 rounded-lg text-sm transition-colors ${
                serviceEnabled
                  ? 'bg-blue-600/20 text-blue-300 hover:bg-blue-600/30'
                  : 'bg-gray-600/20 text-gray-500 cursor-not-allowed'
              }`}
              title={!serviceEnabled ? 'Enable with webcontrol_actions service=on' : undefined}
            >
              Restart Motion
            </button>
            {!serviceEnabled && (
              <p className="text-xs text-amber-500 mt-1">
                Disabled - add <code className="text-xs bg-surface-base px-1 rounded">webcontrol_actions service=on</code> to enable
              </p>
            )}
          </div>

          {/* System Power Controls */}
          <div>
            <p className="text-xs text-gray-400 mb-2">System Power</p>
            <div className="flex gap-3">
              <button
                onClick={handleReboot}
                disabled={!powerEnabled}
                className={`px-4 py-2 rounded-lg text-sm transition-colors ${
                  powerEnabled
                    ? 'bg-yellow-600/20 text-yellow-300 hover:bg-yellow-600/30'
                    : 'bg-gray-600/20 text-gray-500 cursor-not-allowed'
                }`}
                title={!powerEnabled ? 'Enable with webcontrol_actions power=on' : undefined}
              >
                Restart Pi
              </button>
              <button
                onClick={handleShutdown}
                disabled={!powerEnabled}
                className={`px-4 py-2 rounded-lg text-sm transition-colors ${
                  powerEnabled
                    ? 'bg-red-600/20 text-red-300 hover:bg-red-600/30'
                    : 'bg-gray-600/20 text-gray-500 cursor-not-allowed'
                }`}
                title={!powerEnabled ? 'Enable with webcontrol_actions power=on' : undefined}
              >
                Shutdown Pi
              </button>
            </div>
            {!powerEnabled && (
              <p className="text-xs text-amber-500 mt-1">
                Disabled - add <code className="text-xs bg-surface-base px-1 rounded">webcontrol_actions power=on</code> to enable
              </p>
            )}
          </div>
        </div>
      </FormSection>

      {/* Authentication Section */}
      <FormSection
        title="Authentication"
        description="Web interface and stream access credentials"
        collapsible
        defaultOpen={true}
      >
        {/* Web Interface Subsection */}
        <div className="mb-6">
          <h4 className="font-medium text-sm mb-3 text-gray-300">Web Interface</h4>
          <p className="text-xs text-gray-400 mb-4">
            Credentials for logging into this web interface. Format: username:password
          </p>

          {/* Initial Setup Banner */}
          {getOriginalValue('webcontrol_authentication', '') === '' &&
           getOriginalValue('webcontrol_user_authentication', '') === '' && (
            <div className="mb-4 p-3 bg-blue-500/10 border border-blue-500/20 rounded-lg">
              <div className="flex items-start gap-2">
                <svg className="w-5 h-5 text-blue-400 flex-shrink-0 mt-0.5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2}
                    d="M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
                </svg>
                <div className="flex-1">
                  <p className="text-sm font-medium text-blue-300 mb-1">Initial Setup Available</p>
                  <p className="text-xs text-blue-300/80">
                    Configure authentication now to secure your Motion installation.
                    During initial setup, you can set credentials without changing{' '}
                    <code className="text-xs bg-surface px-1 rounded">webcontrol_parms</code> in the config file.
                    Once authentication is configured, it will require restart to apply.
                  </p>
                </div>
              </div>
            </div>
          )}

          <div className="mb-4">
            <label className="block text-sm font-medium mb-1 text-gray-300">
              Admin Username
            </label>
            <input
              type="text"
              value="admin"
              disabled
              className="w-full px-3 py-2 bg-surface-elevated border border-gray-700 rounded-lg
                       text-gray-500 cursor-not-allowed"
            />
            <p className="text-xs text-gray-500 mt-1">
              Admin username is fixed for security
            </p>
          </div>
          <FormInput
            label="Admin Password"
            value={String(getValue('webcontrol_authentication', '')).split(':')[1] || ''}
            onChange={(val) => {
              const currentUser = String(getValue('webcontrol_authentication', '')).split(':')[0] || 'admin';
              onChange('webcontrol_authentication', `${currentUser}:${val}`);
            }}
            type="password"
            placeholder={isPasswordSet('webcontrol_authentication') ? 'Password set (enter new to change)' : 'Enter password'}
            helpText={isPasswordSet('webcontrol_authentication')
              ? "Password is configured. Enter a new password to change it."
              : "Administrator password (click eye icon to reveal)"}
            originalValue={isOriginalPasswordSet('webcontrol_authentication') ? '[set]' : ''}
          />
          <FormInput
            label="Viewer Username"
            value={String(getValue('webcontrol_user_authentication', '')).split(':')[0] || ''}
            onChange={(val) => {
              const currentPass = String(getValue('webcontrol_user_authentication', '')).split(':')[1] || '';
              onChange('webcontrol_user_authentication', val ? `${val}:${currentPass}` : '');
            }}
            helpText="View-only username (can view streams but not change settings)"
            error={getError?.('webcontrol_user_authentication')}
            originalValue={String(getOriginalValue('webcontrol_user_authentication', '')).split(':')[0] || ''}
            showVisibilityToggle={false}
          />
          <FormInput
            label="Viewer Password"
            value={String(getValue('webcontrol_user_authentication', '')).split(':')[1] || ''}
            onChange={(val) => {
              const currentUser = String(getValue('webcontrol_user_authentication', '')).split(':')[0] || '';
              onChange('webcontrol_user_authentication', currentUser ? `${currentUser}:${val}` : '');
            }}
            type="password"
            placeholder={isPasswordSet('webcontrol_user_authentication') ? 'Password set (enter new to change)' : 'Enter password'}
            helpText={isPasswordSet('webcontrol_user_authentication')
              ? "Password is configured. Enter a new password to change it."
              : "View-only password (click eye icon to reveal)"}
            originalValue={isOriginalPasswordSet('webcontrol_user_authentication') ? '[set]' : ''}
          />
        </div>

        {/* Direct Stream Subsection */}
        <div className="border-t border-surface-elevated pt-4">
          <h4 className="font-medium text-sm mb-3 text-gray-300">Direct Stream Access</h4>
          <p className="text-xs text-gray-400 mb-4">
            Authentication for direct stream URLs (embedded in websites, VLC, home automation)
          </p>
          <FormSelect
            label="Authentication Mode"
            value={String(getValue('webcontrol_auth_method', '0'))}
            onChange={(val) => onChange('webcontrol_auth_method', val)}
            options={[
              { value: '0', label: 'None - No authentication required' },
              { value: '1', label: 'Basic - Simple username/password (use with HTTPS)' },
              { value: '2', label: 'Digest - Secure hash-based (recommended)' },
            ]}
            helpText="Controls authentication for direct stream access and external API clients. The web UI uses session-based login instead."
            error={getError?.('webcontrol_auth_method')}
          />
        </div>
      </FormSection>

      {/* Daemon Settings Section */}
      <FormSection
        title="Daemon"
        description="Motion process settings"
        collapsible
        defaultOpen={false}
      >
        <FormToggle
          label="Run as Daemon"
          value={getValue('daemon', false) as boolean}
          onChange={(val) => onChange('daemon', val)}
          helpText="Run Motion in background mode"
        />
        <FormInput
          label="PID File"
          value={String(getValue('pid_file', ''))}
          onChange={(val) => onChange('pid_file', val)}
          helpText="Path to process ID file. Leave empty to let systemd manage the PID."
          error={getError?.('pid_file')}
          originalValue={String(getOriginalValue('pid_file', ''))}
        />
        <FormInput
          label="Log File"
          value={String(getValue('log_file', ''))}
          onChange={(val) => onChange('log_file', val)}
          helpText="Path to log file. Leave empty to use journald (view with: journalctl -u motion)."
          error={getError?.('log_file')}
          originalValue={String(getOriginalValue('log_file', ''))}
        />
        <FormSelect
          label="Log Level"
          value={String(getValue('log_level', '6'))}
          onChange={(val) => onChange('log_level', val)}
          options={[
            { value: '1', label: 'Emergency' },
            { value: '2', label: 'Alert' },
            { value: '3', label: 'Critical' },
            { value: '4', label: 'Error' },
            { value: '5', label: 'Warning' },
            { value: '6', label: 'Notice' },
            { value: '7', label: 'Info' },
            { value: '8', label: 'Debug' },
            { value: '9', label: 'All' },
          ]}
          helpText="Verbosity level for logging"
          error={getError?.('log_level')}
        />
      </FormSection>

      {/* Web Server Section */}
      <FormSection
        title="Web Server"
        description="API server configuration"
        collapsible
        defaultOpen={false}
      >
        <FormInput
          label="Port"
          value={String(getValue('webcontrol_port', '8080'))}
          onChange={(val) => onChange('webcontrol_port', val)}
          type="number"
          helpText="Primary web server port"
          error={getError?.('webcontrol_port')}
          originalValue={String(getOriginalValue('webcontrol_port', '8080'))}
        />
        <FormToggle
          label="Localhost Only"
          value={getValue('webcontrol_localhost', false) as boolean}
          onChange={(val) => onChange('webcontrol_localhost', val)}
          helpText="Restrict access to localhost only (127.0.0.1)"
        />
        <FormToggle
          label="TLS/HTTPS"
          value={getValue('webcontrol_tls', false) as boolean}
          onChange={(val) => onChange('webcontrol_tls', val)}
          helpText="Enable HTTPS encryption"
        />
        {getValue('webcontrol_tls', false) && (
          <>
            <FormInput
              label="TLS Certificate"
              value={String(getValue('webcontrol_cert', ''))}
              onChange={(val) => onChange('webcontrol_cert', val)}
              helpText="Path to TLS certificate file (.crt or .pem)"
              error={getError?.('webcontrol_cert')}
            />
            <FormInput
              label="TLS Private Key"
              value={String(getValue('webcontrol_key', ''))}
              onChange={(val) => onChange('webcontrol_key', val)}
              helpText="Path to TLS private key file (.key or .pem)"
              error={getError?.('webcontrol_key')}
            />
          </>
        )}
      </FormSection>
    </>
  )
}
