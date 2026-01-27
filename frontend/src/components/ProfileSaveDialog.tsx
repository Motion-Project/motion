import { useState } from 'react';
import { useCreateProfile } from '../hooks/useProfiles';

interface ProfileSaveDialogProps {
  cameraId: number;
  onClose: () => void;
}

/**
 * Dialog for saving current camera settings as a new profile
 */
export function ProfileSaveDialog({ cameraId, onClose }: ProfileSaveDialogProps) {
  const [name, setName] = useState('');
  const [description, setDescription] = useState('');
  const { mutate: createProfile, isPending } = useCreateProfile();

  const handleSave = () => {
    if (!name.trim()) {
      return;
    }

    createProfile(
      {
        name: name.trim(),
        description: description.trim() || undefined,
        camera_id: cameraId,
        snapshot_current: true, // Capture current settings
      },
      {
        onSuccess: () => {
          onClose();
        },
        onError: (error) => {
          console.error('Failed to save profile:', error);
          // TODO: Show error notification
        },
      }
    );
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSave();
    } else if (e.key === 'Escape') {
      onClose();
    }
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50">
      <div
        className="bg-surface-base border border-gray-700 rounded-lg shadow-xl max-w-md w-full mx-4 p-6"
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div className="flex items-center justify-between mb-4">
          <h2 className="text-lg font-semibold text-white">Save Configuration Preset</h2>
          <button
            onClick={onClose}
            className="text-gray-400 hover:text-white transition-colors"
            aria-label="Close dialog"
          >
            <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={2}
                d="M6 18L18 6M6 6l12 12"
              />
            </svg>
          </button>
        </div>

        {/* Form */}
        <div className="space-y-4">
          {/* Name field */}
          <div>
            <label htmlFor="profile-name" className="block text-sm font-medium text-gray-400 mb-1">
              Preset Name <span className="text-red-400">*</span>
            </label>
            <input
              id="profile-name"
              type="text"
              value={name}
              onChange={(e) => setName(e.target.value)}
              onKeyDown={handleKeyDown}
              placeholder="e.g., Daytime, Nighttime"
              className="w-full px-3 py-2 bg-surface-elevated border border-gray-600 rounded-lg text-white placeholder-gray-500 focus:outline-none focus:ring-2 focus:ring-primary"
              autoFocus
              maxLength={50}
            />
          </div>

          {/* Description field */}
          <div>
            <label htmlFor="profile-description" className="block text-sm font-medium text-gray-400 mb-1">
              Description (optional)
            </label>
            <textarea
              id="profile-description"
              value={description}
              onChange={(e) => setDescription(e.target.value)}
              placeholder="Brief description of this preset"
              className="w-full px-3 py-2 bg-surface-elevated border border-gray-600 rounded-lg text-white placeholder-gray-500 focus:outline-none focus:ring-2 focus:ring-primary resize-none"
              rows={2}
              maxLength={200}
            />
          </div>

          {/* Info message */}
          <p className="text-xs text-gray-500">
            This will save your current camera settings including brightness, motion detection, and framerate.
          </p>
        </div>

        {/* Actions */}
        <div className="flex gap-3 mt-6">
          <button
            onClick={onClose}
            disabled={isPending}
            className="flex-1 px-4 py-2 bg-surface-elevated border border-gray-600 text-white rounded-lg hover:bg-surface-hover transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
          >
            Cancel
          </button>
          <button
            onClick={handleSave}
            disabled={!name.trim() || isPending}
            className="flex-1 px-4 py-2 bg-primary text-white rounded-lg hover:bg-primary-hover transition-colors disabled:bg-primary/30 disabled:cursor-not-allowed"
          >
            {isPending ? 'Saving...' : 'Save'}
          </button>
        </div>
      </div>
    </div>
  );
}
