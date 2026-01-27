import { useState } from 'react';
import { FormSection, FormSlider } from '@/components/form';

interface PlaybackPreferences {
  playbackFramerateFactor: number;
  playbackResolutionFactor: number;
}

const DEFAULT_PLAYBACK: PlaybackPreferences = {
  playbackFramerateFactor: 1.0,
  playbackResolutionFactor: 1.0,
};

function loadPlaybackPreferences(): PlaybackPreferences {
  const stored = localStorage.getItem('motion-ui-preferences');
  if (stored) {
    try {
      const parsed = JSON.parse(stored);
      return {
        playbackFramerateFactor: parsed.playbackFramerateFactor ?? DEFAULT_PLAYBACK.playbackFramerateFactor,
        playbackResolutionFactor: parsed.playbackResolutionFactor ?? DEFAULT_PLAYBACK.playbackResolutionFactor,
      };
    } catch (e) {
      console.error('Failed to parse preferences:', e);
    }
  }
  return DEFAULT_PLAYBACK;
}

/**
 * Standalone Playback Settings component for Camera Settings view.
 * Manages playback-related preferences (framerate, resolution) stored in localStorage.
 * Note: Full UI Preferences including Dashboard Layout and Appearance are in Global Settings.
 */
export function PlaybackSettings() {
  const [preferences, setPreferences] = useState<PlaybackPreferences>(loadPlaybackPreferences);

  const updatePreference = <K extends keyof PlaybackPreferences>(
    key: K,
    value: PlaybackPreferences[K]
  ) => {
    const updated = { ...preferences, [key]: value };
    setPreferences(updated);

    // Merge with existing preferences in localStorage (preserve other settings)
    const stored = localStorage.getItem('motion-ui-preferences');
    let existing = {};
    if (stored) {
      try {
        existing = JSON.parse(stored);
      } catch (e) {
        console.error('Failed to parse existing preferences:', e);
      }
    }
    localStorage.setItem('motion-ui-preferences', JSON.stringify({ ...existing, ...updated }));
  };

  return (
    <FormSection
      title="Playback Settings"
      description="Video playback preferences for this camera (stored locally in browser)"
      collapsible
      defaultOpen={false}
    >
      <div className="space-y-4">
        <div className="text-xs text-gray-400 bg-surface-elevated p-3 rounded mb-4">
          <strong>Note:</strong> These preferences are stored in your browser's localStorage and
          are not saved to the Motion server. They are specific to this browser/device.
        </div>

        <FormSlider
          label="Framerate Factor"
          value={preferences.playbackFramerateFactor}
          onChange={(val) => updatePreference('playbackFramerateFactor', val)}
          min={0.1}
          max={4.0}
          step={0.1}
          unit="x"
          helpText="Playback speed multiplier (0.5 = half speed, 2.0 = double speed)"
        />

        <FormSlider
          label="Resolution Factor"
          value={preferences.playbackResolutionFactor}
          onChange={(val) => updatePreference('playbackResolutionFactor', val)}
          min={0.25}
          max={1.0}
          step={0.25}
          unit="x"
          helpText="Playback resolution scaling (0.5 = half resolution, 1.0 = full)"
        />

        <div className="text-xs text-gray-400">
          <p>Lower resolution factors reduce bandwidth and improve performance on slow connections.</p>
        </div>
      </div>
    </FormSection>
  );
}
