import { useState } from 'react';
import { useQueryClient } from '@tanstack/react-query';
import { useProfiles, useApplyProfile, useDeleteProfile, useSetDefaultProfile } from '@/hooks/useProfiles';
import { ProfileSaveDialog } from '@/components/ProfileSaveDialog';
import { useToast } from '@/components/Toast';
import { FormSection } from '@/components/form';
import { applyRestartRequiredChanges } from '@/api/client';

interface ProfileManagerProps {
  cameraId: number;
}

/**
 * Profile Manager Component
 *
 * Full-featured profile management for Settings page:
 * - View all saved profiles
 * - Save new profiles
 * - Load/apply profiles
 * - Delete profiles
 * - Set default profile
 */
export function ProfileManager({ cameraId }: ProfileManagerProps) {
  const queryClient = useQueryClient();
  const { data: profiles, isLoading, error } = useProfiles(cameraId);
  const { mutate: applyProfile, isPending: isApplying } = useApplyProfile();
  const { mutate: deleteProfile, isPending: isDeleting } = useDeleteProfile();
  const { mutate: setDefaultProfile, isPending: isSettingDefault } = useSetDefaultProfile();
  const { addToast } = useToast();

  const [showSaveDialog, setShowSaveDialog] = useState(false);
  const [confirmDelete, setConfirmDelete] = useState<number | null>(null);

  const handleApply = (profileId: number, profileName: string) => {
    applyProfile(profileId, {
      onSuccess: async (requiresRestart) => {
        if (requiresRestart.length > 0) {
          addToast(
            `Profile "${profileName}" applied. Restarting camera for: ${requiresRestart.join(', ')}...`,
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
      },
      onError: (error) => {
        addToast(
          `Failed to apply profile: ${error instanceof Error ? error.message : 'Unknown error'}`,
          'error'
        );
      },
    });
  };

  const handleDelete = (profileId: number, profileName: string) => {
    deleteProfile(profileId, {
      onSuccess: () => {
        addToast(
          `Profile "${profileName}" deleted`,
          'success'
        );
        setConfirmDelete(null);
      },
      onError: (error) => {
        addToast(
          `Failed to delete profile: ${error instanceof Error ? error.message : 'Unknown error'}`,
          'error'
        );
      },
    });
  };

  const handleSetDefault = (profileId: number, profileName: string) => {
    setDefaultProfile(profileId, {
      onSuccess: () => {
        addToast(
          `"${profileName}" set as default profile`,
          'success'
        );
      },
      onError: (error) => {
        addToast(
          `Failed to set default: ${error instanceof Error ? error.message : 'Unknown error'}`,
          'error'
        );
      },
    });
  };

  if (error) {
    return (
      <FormSection title="Configuration Profiles">
        <div className="bg-danger/10 border border-danger rounded-lg p-4">
          <p className="text-sm text-danger font-medium">Failed to load profiles</p>
          <p className="text-xs text-gray-400 mt-1">
            {error instanceof Error ? error.message : 'Unable to connect to profiles API'}
          </p>
        </div>
      </FormSection>
    );
  }

  return (
    <FormSection title="Configuration Profiles">
      <div className="space-y-4">
        {/* Add new profile button */}
        <div>
          <button
            type="button"
            onClick={() => setShowSaveDialog(true)}
            className="px-4 py-2 bg-primary text-white rounded-lg hover:bg-primary-hover transition-colors"
          >
            + Save Current Settings as Profile
          </button>
        </div>

        {/* Profiles list */}
        {isLoading ? (
          <div className="text-sm text-gray-400">Loading profiles...</div>
        ) : !profiles || profiles.length === 0 ? (
          <div className="text-sm text-gray-400 italic">
            No profiles saved yet. Save your current camera settings as a profile to quickly switch configurations.
          </div>
        ) : (
          <div className="space-y-3">
            {profiles.map((profile) => (
              <div
                key={profile.profile_id}
                className="bg-surface-elevated border border-gray-700 rounded-lg p-4"
              >
                <div className="flex items-start justify-between">
                  <div className="flex-1">
                    <div className="flex items-center gap-2">
                      <h4 className="font-medium">
                        {profile.is_default && '⭐ '}
                        {profile.name}
                      </h4>
                      {profile.is_default && (
                        <span className="text-xs bg-primary/20 text-primary px-2 py-0.5 rounded">
                          Default
                        </span>
                      )}
                    </div>
                    {profile.description && (
                      <p className="text-sm text-gray-400 mt-1">{profile.description}</p>
                    )}
                    <p className="text-xs text-gray-500 mt-1">
                      Camera {profile.camera_id} • Created {new Date(profile.created_at).toLocaleDateString()}
                    </p>
                  </div>

                  <div className="flex gap-2 ml-4">
                    {/* Load button */}
                    <button
                      type="button"
                      onClick={() => handleApply(profile.profile_id, profile.name)}
                      disabled={isApplying}
                      className="px-3 py-1.5 bg-primary text-white text-sm rounded-lg disabled:bg-primary/30 disabled:cursor-not-allowed hover:bg-primary-hover transition-colors"
                      title="Apply this profile"
                    >
                      Load
                    </button>

                    {/* Set default button */}
                    {!profile.is_default && (
                      <button
                        type="button"
                        onClick={() => handleSetDefault(profile.profile_id, profile.name)}
                        disabled={isSettingDefault}
                        className="px-3 py-1.5 bg-surface border border-gray-600 text-white text-sm rounded-lg disabled:bg-surface/30 disabled:cursor-not-allowed hover:bg-surface-hover transition-colors"
                        title="Set as default profile"
                      >
                        Set Default
                      </button>
                    )}

                    {/* Delete button */}
                    {confirmDelete === profile.profile_id ? (
                      <div className="flex gap-1">
                        <button
                          type="button"
                          onClick={() => handleDelete(profile.profile_id, profile.name)}
                          disabled={isDeleting}
                          className="px-2 py-1.5 bg-danger text-white text-sm rounded-lg hover:bg-danger/80 transition-colors"
                          title="Confirm delete"
                        >
                          Confirm
                        </button>
                        <button
                          type="button"
                          onClick={() => setConfirmDelete(null)}
                          className="px-2 py-1.5 bg-surface border border-gray-600 text-white text-sm rounded-lg hover:bg-surface-hover transition-colors"
                          title="Cancel"
                        >
                          Cancel
                        </button>
                      </div>
                    ) : (
                      <button
                        type="button"
                        onClick={() => setConfirmDelete(profile.profile_id)}
                        className="px-3 py-1.5 bg-surface border border-gray-600 text-white text-sm rounded-lg hover:bg-danger/20 hover:border-danger transition-colors"
                        title="Delete this profile"
                      >
                        Delete
                      </button>
                    )}
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}

        <p className="text-xs text-gray-500">
          Profiles save all camera settings for quick configuration switching. Set a default profile to load automatically on startup.
        </p>
      </div>

      {/* Save dialog */}
      {showSaveDialog && (
        <ProfileSaveDialog
          cameraId={cameraId}
          onClose={() => setShowSaveDialog(false)}
        />
      )}
    </FormSection>
  );
}
