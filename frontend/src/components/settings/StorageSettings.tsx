import { FormSection, FormInput } from '@/components/form';

export interface StorageSettingsProps {
  config: Record<string, { value: string | number | boolean }>;
  onChange: (param: string, value: string | number | boolean) => void;
  getError?: (param: string) => string | undefined;
  /** Original config from server (without pending changes) - used for modified indicators */
  originalConfig?: Record<string, { value: string | number | boolean }>;
}

export function StorageSettings({ config, onChange, getError, originalConfig }: StorageSettingsProps) {
  const getValue = (param: string, defaultValue: string | number | boolean = '') => {
    return config[param]?.value ?? defaultValue;
  };

  const getOriginalValue = (param: string, defaultValue: string | number | boolean = '') => {
    return originalConfig?.[param]?.value ?? defaultValue;
  };

  // Format code reference for help text
  const formatCodes = [
    '%Y - Year (4 digits)',
    '%m - Month (01-12)',
    '%d - Day (01-31)',
    '%H - Hour (00-23)',
    '%M - Minute (00-59)',
    '%S - Second (00-59)',
    '%q - Frame number',
    '%v - Event number',
    '%$ - Camera name',
  ].join(', ');

  return (
    <FormSection
      title="Storage"
      description="Base directory and periodic snapshot settings for this camera"
      collapsible
      defaultOpen={false}
    >
      <div className="space-y-4">
        <FormInput
          label="Base Storage Directory"
          value={String(getValue('target_dir', '/var/lib/motion'))}
          onChange={(val) => onChange('target_dir', val)}
          helpText="Root directory for ALL camera files. Picture and movie filename patterns (configured in their sections) create paths relative to this directory."
          error={getError?.('target_dir')}
          originalValue={String(getOriginalValue('target_dir', '/var/lib/motion'))}
        />

        <div className="border-t border-surface-elevated pt-4">
          <h4 className="font-medium mb-3">Filename Patterns</h4>

          <FormInput
            label="Snapshot Filename"
            value={String(getValue('snapshot_filename', '%Y%m%d%H%M%S-snapshot'))}
            onChange={(val) => onChange('snapshot_filename', val)}
            helpText="Format for periodic snapshot filenames (strftime syntax)"
            error={getError?.('snapshot_filename')}
          />

          <div className="text-xs text-gray-400 bg-surface-elevated p-3 rounded mb-4">
            <p><strong>Example:</strong> <code>%Y%m%d%H%M%S-snapshot</code> → <code>20250129143022-snapshot.jpg</code></p>
            <p><strong>With subdirs:</strong> <code>%$/%Y-%m-%d/snapshot-%H%M%S</code> → <code>Camera1/2025-01-29/snapshot-143022.jpg</code></p>
          </div>
        </div>

        <div className="border-t border-surface-elevated pt-4">
          <h4 className="font-medium mb-3 text-sm">Format Code Reference</h4>
          <div className="text-xs text-gray-400 space-y-1">
            <p>Available codes: {formatCodes}</p>
            <p className="mt-2 text-blue-200">
              <strong>How it works:</strong> The Base Storage Directory above sets where files go.
              Picture and Movie sections set filename patterns (which can include subdirectories like <code>%Y-%m-%d/</code>).
            </p>
          </div>
        </div>

        <div className="border-t border-surface-elevated pt-4">
          <h4 className="font-medium mb-3">File Cleanup (Future)</h4>
          <div className="text-xs text-gray-400 bg-surface-elevated p-3 rounded">
            <p>Automatic file retention and cleanup based on age/size will be available in a future update.</p>
            <p className="mt-2">For now, use <code>cleandir_params</code> in the Motion configuration file or manual cleanup scripts.</p>
          </div>
        </div>

        <div className="text-xs text-yellow-200 bg-yellow-600/10 border border-yellow-600/30 p-3 rounded">
          <strong>💡 Network Storage:</strong> For network shares (NFS, SMB), ensure target_dir points to a mounted directory.
          Test write permissions before starting recording.
        </div>
      </div>
    </FormSection>
  );
}
