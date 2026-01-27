import { useState } from 'react';
import { useQueryClient } from '@tanstack/react-query';
import { useProfiles, useApplyProfile } from '../hooks/useProfiles';
import { ProfileSaveDialog } from './ProfileSaveDialog';
import { useToast } from './Toast';
import { applyRestartRequiredChanges } from '@/api/client';

interface ConfigurationPresetsProps {
  cameraId: number;
  readOnly?: boolean;  // Hide save button when true (for Dashboard bottom sheet)
}

/**
 * Configuration Presets Component
 *
 * Allows users to:
 * - Select from saved configuration profiles
 * - Apply a profile to quickly change camera settings
 * - Save current settings as a new profile
 * - Manage existing profiles (delete, set as default)
 */
export function ConfigurationPresets({ cameraId, readOnly = false }: ConfigurationPresetsProps) {
  const queryClient = useQueryClient();
  const { data: profiles, isLoading, error } = useProfiles(cameraId);
  const { mutate: applyProfile, isPending: isApplying } = useApplyProfile();
  const { addToast } = useToast();

  const [selectedProfileId, setSelectedProfileId] = useState<number | null>(null);
  const [showSaveDialog, setShowSaveDialog] = useState(false);

  const handleApply = () => {
    if (selectedProfileId) {
      const profile = profiles?.find(p => p.profile_id === selectedProfileId);
      const profileName = profile?.name || 'profile';

      applyProfile(selectedProfileId, {
        onSuccess: async (requiresRestart) => {
          if (requiresRestart.length > 0) {
            addToast(
              `Profile "${profileName}" applied. Restarting camera...`,
              'info'
            );
            try {
              await applyRestartRequiredChanges(cameraId);
              await new Promise(resolve => setTimeout(resolve, 2000));
              await queryClient.invalidateQueries({ queryKey: ['config'] });
              addToast(
                `Profile "${profileName}" applied. Camera restarted.`,
                'success'
              );
            } catch (err) {
              console.error('Failed to restart camera:', err);
              addToast(
                `Profile applied but camera restart failed. Please restart manually.`,
                'warning'
              );
            }
          } else {
            addToast(
              `Profile "${profileName}" applied successfully`,
              'success'
            );
          }
          setSelectedProfileId(null);
        },
        onError: (error) => {
          addToast(
            `Failed to apply profile: ${error instanceof Error ? error.message : 'Unknown error'}`,
            'error'
          );
        },
      });
    }
  };

  if (error) {
    return (
      <div className="mb-4 pb-4 border-b border-surface-elevated">
        <div className="bg-danger/10 border border-danger rounded-lg p-3">
          <p className="text-sm text-danger font-medium">Failed to load profiles</p>
          <p className="text-xs text-gray-400 mt-1">
            {error instanceof Error ? error.message : 'Unable to connect to profiles API'}
          </p>
        </div>
      </div>
    );
  }

  return (
    <>
      <div className="mb-4 pb-4 border-b border-surface-elevated">
        <label className="block text-sm font-medium text-gray-400 mb-2">
          Configuration Preset
        </label>
        <div className="flex gap-2">
          {/* Preset selector dropdown */}
          <div className="relative flex-1">
            <select
              disabled={isLoading || !profiles || profiles.length === 0}
              value={selectedProfileId ?? ''}
              onChange={(e) => setSelectedProfileId(Number(e.target.value) || null)}
              className="w-full px-3 py-2 bg-surface-elevated border border-gray-600 rounded-lg text-white disabled:text-gray-500 disabled:cursor-not-allowed appearance-none focus:outline-none focus:ring-2 focus:ring-primary"
            >
              <option value="">
                {isLoading
                  ? 'Loading...'
                  : !profiles || profiles.length === 0
                  ? 'No presets available'
                  : 'Select a preset'}
              </option>
              {profiles?.map((profile) => (
                <option key={profile.profile_id} value={profile.profile_id}>
                  {profile.is_default ? '⭐ ' : ''}
                  {profile.name}
                  {profile.description ? ` - ${profile.description}` : ''}
                </option>
              ))}
            </select>
            <div className="absolute inset-y-0 right-0 flex items-center pr-3 pointer-events-none">
              <svg
                className="w-4 h-4 text-gray-400"
                fill="none"
                stroke="currentColor"
                viewBox="0 0 24 24"
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth={2}
                  d="M19 9l-7 7-7-7"
                />
              </svg>
            </div>
          </div>

          {/* Save button (hidden in read-only mode) */}
          {!readOnly && (
            <button
              type="button"
              onClick={() => setShowSaveDialog(true)}
              className="px-4 py-2 bg-surface-elevated border border-gray-600 text-white rounded-lg hover:bg-surface-hover transition-colors"
              title="Save current settings as a new preset"
            >
              Save
            </button>
          )}

          {/* Apply button */}
          <button
            type="button"
            onClick={handleApply}
            disabled={!selectedProfileId || isApplying}
            className="px-4 py-2 bg-primary text-white rounded-lg disabled:bg-primary/30 disabled:text-gray-500 disabled:cursor-not-allowed hover:bg-primary-hover transition-colors"
            title="Apply selected preset to camera"
          >
            {isApplying ? 'Applying...' : 'Apply'}
          </button>
        </div>
        <p className="mt-1 text-xs text-gray-500">
          Quick switch camera settings • {profiles?.length || 0} preset{profiles?.length !== 1 ? 's' : ''}
        </p>
      </div>

      {/* Save dialog */}
      {showSaveDialog && (
        <ProfileSaveDialog
          cameraId={cameraId}
          onClose={() => setShowSaveDialog(false)}
        />
      )}
    </>
  );
}
